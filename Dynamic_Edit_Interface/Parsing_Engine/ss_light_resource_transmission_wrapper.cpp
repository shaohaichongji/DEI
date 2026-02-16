// ss_light_resource_transmission_wrapper.cpp
#include "ss_light_resource_transmission_wrapper.h"

#include <algorithm>
#include <cctype>
#include <atomic>

static inline bool IsSpace(unsigned char c) { return std::isspace(c) != 0; }
static inline bool IsHex(unsigned char c) { return std::isxdigit(c) != 0; }

bool SS_LightTransmissionWrapper::WrapPdu(
    const std::vector<uint8_t>& pdu_bytes,
    const SS_LightByteTransmissionParams& params,
    std::vector<uint8_t>& frame_bytes,
    std::string& out_error) const
{
    out_error.clear();
    frame_bytes.clear();

    if (pdu_bytes.empty())
    {
        out_error = "WrapPdu: pdu_bytes is empty.";
        return false;
    }

    const std::string header_type = ToLowerCopy(params.message_header_type);
    const std::string tail_type = ToLowerCopy(params.tail_check_type);

    // device address / unit id
    uint8_t addr = 0;
    if (!ParseHexByte(params.device_address, addr))
    {
        out_error = "WrapPdu: device_address is invalid or empty.";
        return false;
    }

    // 1) Modbus TCP
    if (header_type == "modbustcp_mbap")
    {
        // TCP 下不加 CRC16_Modbus
        return BuildMbapAdu(pdu_bytes, addr, frame_bytes, out_error);
    }

    // 2) RTU: Addr + PDU + CRC
    if ((header_type.empty() || header_type == "empty") && tail_type == "crc_16_modbus")
    {
        frame_bytes.reserve(1 + pdu_bytes.size() + 2);
        frame_bytes.push_back(addr);
        frame_bytes.insert(frame_bytes.end(), pdu_bytes.begin(), pdu_bytes.end());

        const uint16_t crc = ComputeCrc16Modbus(frame_bytes.data(), frame_bytes.size());
        const uint8_t lo = static_cast<uint8_t>(crc & 0xFF);
        const uint8_t hi = static_cast<uint8_t>((crc >> 8) & 0xFF);

        if (params.crc_endian) { frame_bytes.push_back(hi); frame_bytes.push_back(lo); }
        else { frame_bytes.push_back(lo); frame_bytes.push_back(hi); }
        return true;
    }

    // 3) EMPTY：原样输出（仅允许在非严格模式下）
    if (header_type.empty() || header_type == "empty")
    {
#ifdef SS_LIGHT_WRAP_STRICT
        // 严格模式：EMPTY 必须配合明确的 tail/header 规则，否则视为配置错误
        out_error = "WrapPdu(STRICT): EMPTY passthrough is not allowed. "
            "Please specify a supported header/tail (e.g. ModbusTCP_MBAP or CRC_16_Modbus).";
        return false;
#else
        frame_bytes = pdu_bytes;
        return true;
#endif
    }

    out_error = "WrapPdu: unsupported message_header_type/tail_check_type combination: header=" +
        params.message_header_type + ", tail=" + params.tail_check_type;
    return false;
}

bool SS_LightTransmissionWrapper::ParseHexByte(const std::string& s_in, uint8_t& out_byte)
{
    std::string s = s_in;

    // trim
    while (!s.empty() && IsSpace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && IsSpace((unsigned char)s.back())) s.pop_back();
    if (s.empty()) return false;

    const bool has_0x = (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    if (has_0x) s.erase(0, 2);

    // remove inner spaces?（可选：你现在只 trim，不移除中间空格）
    // 这里我建议允许 "0x 01" 这种写法就太放纵人类了，所以不支持。

    if (s.empty()) return false;

    // 强制 hex：必须全是 hex digit
    for (unsigned char c : s)
        if (!IsHex(c)) return false;

    // 强制规则：不带 0x 时必须是偶数位（拒绝 "1" 这种歧义输入）
    if (!has_0x && (s.size() % 2 != 0))
        return false;

    // 支持 0x0001 / 0001：只取最后两位
    if (s.size() > 2) s = s.substr(s.size() - 2);

    // 如果带 0x 且只写 1 位，比如 0x1，补 0
    if (s.size() == 1) s.insert(s.begin(), '0');

    auto hexval = [](char ch)->uint8_t {
        if (ch >= '0' && ch <= '9') return (uint8_t)(ch - '0');
        if (ch >= 'a' && ch <= 'f') return (uint8_t)(ch - 'a' + 10);
        if (ch >= 'A' && ch <= 'F') return (uint8_t)(ch - 'A' + 10);
        return 0;
    };

    out_byte = (uint8_t)((hexval(s[0]) << 4) | hexval(s[1]));
    return true;
}

uint16_t SS_LightTransmissionWrapper::ComputeCrc16Modbus(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
        {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

std::string SS_LightTransmissionWrapper::ToLowerCopy(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

bool SS_LightTransmissionWrapper::BuildMbapAdu(
    const std::vector<uint8_t>& pdu_bytes,
    uint8_t unit_id,
    std::vector<uint8_t>& out_adu,
    std::string& out_error)
{
    out_error.clear();
    out_adu.clear();

    // MBAP Length = UnitId(1) + PDU length
    const uint32_t length_field = static_cast<uint32_t>(1 + pdu_bytes.size());
    if (length_field > 0xFFFFu)
    {
        out_error = "MBAP: PDU too large.";
        return false;
    }

    // 线程安全
    static std::atomic_uint16_t s_txid{ 0 };
    const uint16_t txid = static_cast<uint16_t>(s_txid.fetch_add(1, std::memory_order_relaxed) + 1);

    out_adu.reserve(7 + pdu_bytes.size());

    // Transaction ID (BE)
    out_adu.push_back(static_cast<uint8_t>((txid >> 8) & 0xFF));
    out_adu.push_back(static_cast<uint8_t>(txid & 0xFF));

    // Protocol ID = 0
    out_adu.push_back(0x00);
    out_adu.push_back(0x00);

    // Length (BE)
    out_adu.push_back(static_cast<uint8_t>((length_field >> 8) & 0xFF));
    out_adu.push_back(static_cast<uint8_t>(length_field & 0xFF));

    // Unit ID
    out_adu.push_back(unit_id);

    // PDU
    out_adu.insert(out_adu.end(), pdu_bytes.begin(), pdu_bytes.end());
    return true;
}

