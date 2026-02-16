// ss_light_resource_frame_parser.cpp
#include "ss_light_resource_frame_parser.h"

#include <algorithm>
#include <cctype>

void SS_LightFrameParser::Reset()
{
    rx_buffer_.clear();
}

bool SS_LightFrameParser::Feed(
    const uint8_t* data,
    size_t len,
    const SS_LightByteTransmissionParams& params,
    std::vector<std::vector<uint8_t>>& out_frames,
    std::string& out_error)
{
    out_error.clear();
    out_frames.clear();

    if (data == nullptr || len == 0) return true;

    rx_buffer_.insert(rx_buffer_.end(), data, data + len);

    const std::string header_type = ToLowerCopy(params.message_header_type);
    const std::string tail_type = ToLowerCopy(params.tail_check_type);

    if (header_type == "modbustcp_mbap")
    {
        while (true)
        {
            std::vector<uint8_t> frame;
            std::string err;
            bool ok = TryExtractOneMbapFrame(frame, err);
            if (!err.empty()) out_error = err;
            if (!ok) break;
            out_frames.push_back(std::move(frame));
        }
        return true;
    }

    if (tail_type == "crc_16_modbus")
    {
        while (true)
        {
            std::vector<uint8_t> frame;
            bool ok = TryExtractOneCrcFrame(params.crc_endian, frame);
            if (!ok) break;
            out_frames.push_back(std::move(frame));
        }
        return true;
    }

    // EMPTY：无边界能力，先当“当前缓存就是一帧”
    if (!rx_buffer_.empty())
    {
        out_frames.push_back(rx_buffer_);
        rx_buffer_.clear();
    }
    return true;
}

bool SS_LightFrameParser::ExtractPayload(
    const std::vector<uint8_t>& frame,
    const SS_LightByteTransmissionParams& params,
    std::vector<uint8_t>& out_payload,
    std::string& out_error) const
{
    out_error.clear();
    out_payload.clear();

    const std::string header_type = ToLowerCopy(params.message_header_type);
    const std::string tail_type = ToLowerCopy(params.tail_check_type);

    if (header_type == "modbustcp_mbap")
    {
        if (frame.size() < 8)
        {
            out_error = "ExtractPayload(MBAP): frame too short.";
            return false;
        }

        uint16_t len_field = 0;
        ReadMbapLength(frame.data(), len_field);
        const size_t full = 6 + static_cast<size_t>(len_field);
        if (frame.size() != full)
        {
            out_error = "ExtractPayload(MBAP): length mismatch.";
            return false;
        }

        // frame[6]=UnitId, frame[7..]=PDU
        out_payload.assign(frame.begin() + 7, frame.end());
        return true;
    }

    if (tail_type == "crc_16_modbus")
    {
        if (!CheckCrc16Modbus(frame, params.crc_endian))
        {
            out_error = "ExtractPayload(CRC): CRC check failed.";
            return false;
        }
        // 去掉 CRC，保留 Addr+PDU
        out_payload.assign(frame.begin(), frame.end() - 2);
        return true;
    }

    out_payload = frame;
    return true;
}

bool SS_LightFrameParser::CheckCrc16Modbus(const std::vector<uint8_t>& frame, bool crc_endian)
{
    // addr(1)+func(1)+至少2字节数据 + crc(2) => 最少 6
    if (frame.size() < 6) return false;

    const size_t n = frame.size();
    const uint8_t c0 = frame[n - 2];
    const uint8_t c1 = frame[n - 1];

    uint16_t got;
    if (crc_endian) got = static_cast<uint16_t>((c0 << 8) | c1);  // hi-lo
    else got = static_cast<uint16_t>((c1 << 8) | c0);             // lo-hi

    const uint16_t calc = SS_LightTransmissionWrapper::ComputeCrc16Modbus(frame.data(), n - 2);
    return calc == got;
}

std::string SS_LightFrameParser::ToLowerCopy(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

bool SS_LightFrameParser::TryExtractOneMbapFrame(std::vector<uint8_t>& out_frame, std::string& out_error)
{
    out_error.clear();
    out_frame.clear();

    if (rx_buffer_.size() < 7) return false;

    uint16_t len_field = 0;
    ReadMbapLength(rx_buffer_.data(), len_field);

    // full = 6 + len_field （len_field = UnitId + PDU）
    const size_t full = 6 + static_cast<size_t>(len_field);

    // 合法性：至少 UnitId(1)+Func(1) => len_field >=2 => full >= 8
    if (len_field < 2 || full < 8)
    {
        out_error = "MBAP length field invalid (too small).";
        rx_buffer_.erase(rx_buffer_.begin()); // 丢 1 字节容错
        return false;
    }

    if (rx_buffer_.size() < full) return false;

    out_frame.assign(rx_buffer_.begin(), rx_buffer_.begin() + full);
    rx_buffer_.erase(rx_buffer_.begin(), rx_buffer_.begin() + full);
    return true;
}

bool SS_LightFrameParser::TryExtractOneCrcFrame(bool crc_endian, std::vector<uint8_t>& out_frame)
{
    out_frame.clear();

    // 最少 6 字节才可能是合法 RTU 帧（保守一点减少误判）
    if (rx_buffer_.size() < 6) return false;

    // 从 buffer 起点开始，寻找第一个 CRC 成功的片段
    for (size_t len = 6; len <= rx_buffer_.size(); ++len)
    {
        std::vector<uint8_t> cand(rx_buffer_.begin(), rx_buffer_.begin() + len);
        if (CheckCrc16Modbus(cand, crc_endian))
        {
            out_frame = std::move(cand);
            rx_buffer_.erase(rx_buffer_.begin(), rx_buffer_.begin() + len);
            return true;
        }
    }

    // 找不到：保留末尾一段，丢前面 1 字节避免卡死
    rx_buffer_.erase(rx_buffer_.begin());
    return false;
}

bool SS_LightFrameParser::ReadMbapLength(const uint8_t* mbap7, uint16_t& out_len)
{
    out_len = static_cast<uint16_t>((mbap7[4] << 8) | mbap7[5]);
    return true;
}
