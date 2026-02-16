// ss_light_resource_protocol_factory.cpp
#include "ss_light_resource_protocol_factory.h"

#include <cctype>
#include <charconv>
#include <algorithm>
#include <limits>
#include <stdexcept>

static inline bool IsSpace(unsigned char c) { return std::isspace(c) != 0; }

SS_LightProtocolFactory::SS_LightProtocolFactory()
{
    // -------- string tools --------
    string_tool_map_["DoNothing"] = &SS_LightProtocolFactory::ToolDoNothing;
    string_tool_map_["NumberToUpperAlpha"] = &SS_LightProtocolFactory::ToolNumberToUpperAlpha;
    string_tool_map_["NumberToFixedDec"] = &SS_LightProtocolFactory::ToolNumberToFixedDec;
    string_tool_map_["GetStringMapValue"] = &SS_LightProtocolFactory::ToolGetStringMapValue;
    string_tool_map_["DigitalCharacterCalculation"] = &SS_LightProtocolFactory::ToolDigitalCharacterCalculation;

    // -------- bytes tools --------
    bytes_tool_map_["ByteConversion"] = &SS_LightProtocolFactory::ToolByteConversion;
    bytes_tool_map_["GetRawData"] = &SS_LightProtocolFactory::ToolGetRawData;
    bytes_tool_map_["GetStringMapValueToBytes"] = &SS_LightProtocolFactory::ToolGetStringMapValueToBytes;
}

// ---------------- public APIs ----------------

bool SS_LightProtocolFactory::BuildCommand(
    const SS_LightCommandRule& rule,
    const std::string& param_value_str,
    int channel_index,
    std::string& out_cmd,
    std::string& out_error) const
{
    out_error.clear();
    out_cmd = rule.cmd_template;

    if (out_cmd.empty())
    {
        out_error = "cmd_template is empty.";
        return false;
    }

    const auto placeholders = ExtractPlaceholdersFast(out_cmd);

    for (const auto& ph_key : placeholders)
    {
        auto it = rule.placeholders.find(ph_key);
        if (it == rule.placeholders.end())
        {
            out_error = "Missing placeholder rule for <" + ph_key + ">.";
            return false;
        }

        const SS_LightPlaceholderRule& ph_rule = it->second;

        std::string raw_value;
        std::string err;
        if (!ResolveSourceValue(ph_rule.source, param_value_str, channel_index, raw_value, err))
        {
            out_error = "ResolveSourceValue failed for <" + ph_key + ">: " + err;
            return false;
        }

        std::string final_value;
        if (!ApplyPlaceholderRuleString(ph_rule, raw_value, final_value, err))
        {
            out_error = "ApplyPlaceholderRuleString failed for <" + ph_key + ">: " + err;
            return false;
        }

        ReplaceAll(out_cmd, "<" + ph_key + ">", final_value);
    }

    if (out_cmd.empty())
    {
        out_error = "BuildCommand result is empty.";
        return false;
    }

    return true;
}

bool SS_LightProtocolFactory::BuildBytesCommand(
    const SS_LightCommandRule& rule,
    const std::string& param_value_str,
    int channel_index,
    std::vector<uint8_t>& out_bytes,
    std::string& out_error) const
{
    out_error.clear();
    out_bytes.clear();

    const std::string& tpl = rule.cmd_template;
    if (tpl.empty())
    {
        out_error = "cmd_template is empty.";
        return false;
    }

    // 关键约定：BYTE 模式模板输出的是“裸 PDU”
    // 地址/MBAP/CRC 都由 transmission wrapper 统一处理
    if (ContainsPlaceholder(tpl, "DeviceAddress"))
    {
        out_error = "BYTE cmd_template must NOT contain <DeviceAddress>. "
            "Use SS_LightByteTransmissionParams.device_address in wrapper.";
        return false;
    }

    // pipeline：按模板顺序解析
    size_t pos = 0;
    while (pos < tpl.size())
    {
        size_t lt = tpl.find('<', pos);
        if (lt == std::string::npos)
        {
            // 剩余普通段落
            std::string err;
            if (!AppendHexBytesFromText(std::string_view(tpl).substr(pos), out_bytes, err))
            {
                out_error = err;
                return false;
            }
            break;
        }

        // placeholder 前的普通段
        if (lt > pos)
        {
            std::string err;
            if (!AppendHexBytesFromText(std::string_view(tpl).substr(pos, lt - pos), out_bytes, err))
            {
                out_error = err;
                return false;
            }
        }

        size_t gt = tpl.find('>', lt + 1);
        if (gt == std::string::npos)
        {
            out_error = "BuildBytesCommand: missing '>' for placeholder.";
            return false;
        }

        std::string ph_key = TrimCopy(std::string_view(tpl).substr(lt + 1, gt - lt - 1));
        if (ph_key.empty())
        {
            out_error = "BuildBytesCommand: empty placeholder name.";
            return false;
        }

        auto it = rule.placeholders.find(ph_key);
        if (it == rule.placeholders.end())
        {
            out_error = "Missing placeholder rule for <" + ph_key + ">.";
            return false;
        }

        const SS_LightPlaceholderRule& ph_rule = it->second;

        std::string raw_value;
        std::string err;
        if (!ResolveSourceValue(ph_rule.source, param_value_str, channel_index, raw_value, err))
        {
            out_error = "ResolveSourceValue failed for <" + ph_key + ">: " + err;
            return false;
        }

        if (!ApplyPlaceholderRuleBytes(ph_rule, raw_value, out_bytes, err))
        {
            out_error = "ApplyPlaceholderRuleBytes failed for <" + ph_key + ">: " + err;
            return false;
        }

        pos = gt + 1;
    }

    if (out_bytes.empty())
    {
        out_error = "BuildBytesCommand result is empty.";
        return false;
    }
    return true;
}

// STRING 模式用：去重解析所有 <xxx>
std::vector<std::string> SS_LightProtocolFactory::ExtractPlaceholdersFast(const std::string& s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size())
    {
        size_t lt = s.find('<', i);
        if (lt == std::string::npos) break;
        size_t gt = s.find('>', lt + 1);
        if (gt == std::string::npos) break;

        if (gt > lt + 1)
        {
            std::string key = s.substr(lt + 1, gt - lt - 1);
            key = TrimCopy(key);
            if (!key.empty()) out.push_back(key);
        }
        i = gt + 1;
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

void SS_LightProtocolFactory::ReplaceAll(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::string SS_LightProtocolFactory::TrimCopy(std::string_view sv)
{
    size_t i = 0, j = sv.size();
    while (i < j && IsSpace((unsigned char)sv[i])) ++i;
    while (j > i && IsSpace((unsigned char)sv[j - 1])) --j;
    return std::string(sv.substr(i, j - i));
}

std::string SS_LightProtocolFactory::ToLowerCopy(std::string_view sv)
{
    std::string s = TrimCopy(sv);
    for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

bool SS_LightProtocolFactory::ContainsPlaceholder(std::string_view tpl, std::string_view name)
{
    std::string n = TrimCopy(name);
    if (n.empty()) return false;
    std::string needle;
    needle.reserve(n.size() + 2);
    needle.push_back('<');
    needle.append(n);
    needle.push_back('>');
    return tpl.find(needle) != std::string_view::npos;
}

bool SS_LightProtocolFactory::ResolveSourceValue(
    const std::string& source,
    const std::string& param_value_str,
    int channel_index,
    std::string& out_value,
    std::string& out_error)
{
    out_error.clear();
    out_value.clear();

    const std::string src = ToLowerCopy(source);

    if (src == "param_value")
    {
        out_value = param_value_str;
        return true;
    }

    // 建议：channel_num = 1-based（兼容旧 NumberToUpperAlpha 1..26）
    if (src == "channel_num")
    {
        out_value = std::to_string(channel_index + 1);
        return true;
    }

    if (src == "channel_index")
    {
        out_value = std::to_string(channel_index);
        return true;
    }

    // 允许 source 为空：返回空串（给 GetRawData / 固定值工具用）
    if (src.empty() || src == "empty")
    {
        out_value.clear();
        return true;
    }

    out_error = "Unsupported placeholder source: " + source;
    return false;
}

bool SS_LightProtocolFactory::ApplyPlaceholderRuleString(
    const SS_LightPlaceholderRule& ph_rule,
    const std::string& raw_value,
    std::string& out_value,
    std::string& out_error) const
{
    out_error.clear();
    out_value.clear();

    std::string tool = TrimCopy(ph_rule.parser_tool);
    if (tool.empty()) tool = "DoNothing";

    auto it = string_tool_map_.find(tool);
    if (it == string_tool_map_.end())
    {
        out_error = "Unknown parser_tool (string): " + tool;
        return false;
    }

    try
    {
        out_value = it->second(raw_value, ph_rule.extra_param);
        return true;
    }
    catch (const std::exception& e)
    {
        out_error = "parser_tool '" + tool + "' failed: " + std::string(e.what());
        return false;
    }
}

bool SS_LightProtocolFactory::ApplyPlaceholderRuleBytes(
    const SS_LightPlaceholderRule& ph_rule,
    const std::string& raw_value,
    std::vector<uint8_t>& out_bytes,
    std::string& out_error) const
{
    out_error.clear();

    std::string tool = TrimCopy(ph_rule.parser_tool);
    if (tool.empty())
    {
        // BYTE 模式：强制要求写 parser_tool，避免 raw_value 被误当 hex
        out_error = "BYTE mode requires parser_tool for placeholder (empty parser_tool is not allowed).";
        return false;
    }

    // 1) bytes tool
    auto itb = bytes_tool_map_.find(tool);
    if (itb != bytes_tool_map_.end())
    {
        try
        {
            return itb->second(raw_value, ph_rule.extra_param, out_bytes, ph_rule.endian);
        }
        catch (const std::exception& e)
        {
            out_error = "bytes parser_tool '" + tool + "' failed: " + std::string(e.what());
            return false;
        }
    }

    // 2) 兼容：误用 string tool 时，将输出作为 hex 解析追加
    auto its = string_tool_map_.find(tool);
    if (its != string_tool_map_.end())
    {
        try
        {
            std::string s = its->second(raw_value, ph_rule.extra_param);
            return AppendHexBytesFromText(s, out_bytes, out_error);
        }
        catch (const std::exception& e)
        {
            out_error = "string parser_tool '" + tool + "' failed: " + std::string(e.what());
            return false;
        }
    }

    out_error = "Unknown parser_tool (bytes): " + tool;
    return false;
}

// 普通文本段解析为 bytes：提取其中所有 hex digit，按两位一字节追加
bool SS_LightProtocolFactory::AppendHexBytesFromText(std::string_view text, std::vector<uint8_t>& out_bytes, std::string& out_error)
{
    out_error.clear();
    std::string s = TrimCopy(text);
    if (s.empty()) return true;

    std::string hex;
    hex.reserve(s.size());
    for (char c : s)
    {
        if (std::isxdigit((unsigned char)c)) hex.push_back(c);
    }

    if (hex.empty())
    {
        out_error = "AppendHexBytesFromText: no hex digits in text: '" + s + "'";
        return false;
    }

    if (hex.size() & 1) hex.insert(hex.begin(), '0');

    auto hexval = [](char ch)->uint8_t {
        if (ch >= '0' && ch <= '9') return (uint8_t)(ch - '0');
        if (ch >= 'a' && ch <= 'f') return (uint8_t)(ch - 'a' + 10);
        if (ch >= 'A' && ch <= 'F') return (uint8_t)(ch - 'A' + 10);
        return 0;
    };

    const size_t nbytes = hex.size() / 2;
    out_bytes.reserve(out_bytes.size() + nbytes);
    for (size_t i = 0; i < nbytes; ++i)
    {
        uint8_t b = (uint8_t)((hexval(hex[2 * i]) << 4) | hexval(hex[2 * i + 1]));
        out_bytes.push_back(b);
    }
    return true;
}

// ---------------- string tools ----------------

std::string SS_LightProtocolFactory::ToolDoNothing(std::string_view input, const std::vector<std::string>&)
{
    return TrimCopy(input);
}

std::string SS_LightProtocolFactory::ToolNumberToUpperAlpha(std::string_view input, const std::vector<std::string>& extra_param)
{
    const std::string v = TrimCopy(input);
    if (v.empty()) throw std::runtime_error("NumberToUpperAlpha: empty input");

    int n = 0;
    {
        auto* f = v.data();
        auto* l = v.data() + v.size();
        auto res = std::from_chars(f, l, n, 10);
        if (res.ec != std::errc{} || res.ptr != l) throw std::runtime_error("NumberToUpperAlpha: invalid decimal");
    }
    if (n < 1 || n > 26) throw std::runtime_error("NumberToUpperAlpha: out of range [1..26]");

    if (extra_param.empty()) throw std::runtime_error("NumberToUpperAlpha: missing extra_param[0]=true/false");

    const std::string flag = ToLowerCopy(extra_param[0]);
    bool upper;
    if (flag == "true") upper = true;
    else if (flag == "false") upper = false;
    else throw std::runtime_error("NumberToUpperAlpha: extra_param[0] must be true/false");

    const char base = upper ? 'A' : 'a';
    return std::string(1, (char)(base + (n - 1)));
}

std::string SS_LightProtocolFactory::ToolNumberToFixedDec(std::string_view input, const std::vector<std::string>& extra_param)
{
    const std::string sv = TrimCopy(input);
    if (sv.empty()) throw std::runtime_error("NumberToFixedDec: empty input");
    if (extra_param.empty()) throw std::runtime_error("NumberToFixedDec: missing extra_param[0]=width");

    long long width_ll = 0;
    {
        const std::string w = TrimCopy(extra_param[0]);
        auto* f = w.data();
        auto* l = w.data() + w.size();
        auto res = std::from_chars(f, l, width_ll, 10);
        if (res.ec != std::errc{} || res.ptr != l) throw std::runtime_error("NumberToFixedDec: width invalid");
        if (width_ll < 0) throw std::runtime_error("NumberToFixedDec: width must be >= 0");
        if (width_ll > 100000) throw std::runtime_error("NumberToFixedDec: width too large");
    }
    const size_t target = (size_t)width_ll;

    std::string_view v(sv);
    std::string_view sign, digits;
    if (!v.empty() && (v.front() == '+' || v.front() == '-'))
    {
        sign = v.substr(0, 1);
        digits = v.substr(1);
        if (digits.empty()) throw std::runtime_error("NumberToFixedDec: invalid signed decimal");
    }
    else digits = v;

    for (unsigned char c : digits)
        if (std::isdigit(c) == 0) throw std::runtime_error("NumberToFixedDec: invalid decimal string");

    if (digits.size() >= target) return sv;

    std::string out;
    out.reserve(sign.size() + target);
    out.append(sign.data(), sign.size());
    out.append(target - digits.size(), '0');
    out.append(digits.data(), digits.size());
    return out;
}

std::string SS_LightProtocolFactory::ToolGetStringMapValue(std::string_view input, const std::vector<std::string>& extra_param)
{
    const std::string key = TrimCopy(input);
    if (key.empty()) throw std::runtime_error("GetStringMapValue: empty input");
    if (extra_param.empty()) throw std::runtime_error("GetStringMapValue: extra_param empty");

    auto trim_sv = [](std::string_view sv)->std::string_view {
        size_t i = 0, j = sv.size();
        while (i < j && IsSpace((unsigned char)sv[i])) ++i;
        while (j > i && IsSpace((unsigned char)sv[j - 1])) --j;
        return sv.substr(i, j - i);
    };

    const std::string key_l = SS_LightProtocolFactory::ToLowerCopy(key);

    for (const auto& item : extra_param)
    {
        std::string_view sv(item);
        sv = trim_sv(sv);
        if (sv.empty()) continue;

        size_t lt = sv.rfind('<');
        if (lt == std::string_view::npos) continue;
        size_t gt = sv.find('>', lt + 1);
        if (gt == std::string_view::npos || gt <= lt + 1) continue;

        std::string_view label = trim_sv(sv.substr(0, lt));
        std::string_view val = trim_sv(sv.substr(lt + 1, gt - lt - 1));

        if (SS_LightProtocolFactory::ToLowerCopy(std::string(label)) == key_l)
            return std::string(val);
    }

    throw std::runtime_error("GetStringMapValue: no mapping for '" + key + "'");
}

std::string SS_LightProtocolFactory::ToolDigitalCharacterCalculation(std::string_view input, const std::vector<std::string>& extra_param)
{
    const std::string lhs = TrimCopy(input);
    if (lhs.empty()) throw std::runtime_error("DigitalCharacterCalculation: empty lhs");
    if (extra_param.size() < 2) throw std::runtime_error("DigitalCharacterCalculation: need extra_param[0]=operand, [1]=operator");

    auto parse_i64 = [](const std::string& s)->std::int64_t {
        std::int64_t v{};
        auto* f = s.data();
        auto* l = s.data() + s.size();
        auto res = std::from_chars(f, l, v, 10);
        if (res.ec != std::errc{} || res.ptr != l) throw std::runtime_error("invalid integer: " + s);
        return v;
    };

    const std::int64_t a = parse_i64(lhs);
    const std::int64_t b = parse_i64(TrimCopy(extra_param[0]));
    const std::string op = ToLowerCopy(extra_param[1]);

    std::int64_t r = 0;
    if (op == "add") r = a + b;
    else if (op == "subtract") r = a - b;
    else if (op == "multiply") r = a * b;
    else if (op == "divide")
    {
        if (b == 0) throw std::runtime_error("division by zero");
        r = a / b;
    }
    else if (op == "modulo")
    {
        if (b == 0) throw std::runtime_error("modulo by zero");
        r = a % b;
    }
    else throw std::runtime_error("operator must be add/subtract/multiply/divide/modulo");

    return std::to_string(r);
}

// ---------------- bytes tools ----------------

std::vector<uint8_t> SS_LightProtocolFactory::HexStringToBytes(const std::string& hex_str)
{
    std::string s = hex_str;
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; }), s.end());

    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.erase(0, 2);

    std::string hex;
    hex.reserve(s.size());
    for (char c : s)
        if (std::isxdigit((unsigned char)c)) hex.push_back(c);

    if (hex.empty()) throw std::runtime_error("HexStringToBytes: empty string");
    if (hex.size() & 1) hex.insert(hex.begin(), '0');

    auto hexval = [](char ch)->uint8_t {
        if (ch >= '0' && ch <= '9') return (uint8_t)(ch - '0');
        if (ch >= 'a' && ch <= 'f') return (uint8_t)(ch - 'a' + 10);
        if (ch >= 'A' && ch <= 'F') return (uint8_t)(ch - 'A' + 10);
        return 0;
    };

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        uint8_t b = (uint8_t)((hexval(hex[i]) << 4) | hexval(hex[i + 1]));
        bytes.push_back(b);
    }
    return bytes;
}

bool SS_LightProtocolFactory::ToolByteConversion(std::string_view input, const std::vector<std::string>& extra_param,
    std::vector<uint8_t>& out_bytes, bool endian)
{
    if (extra_param.size() < 3) throw std::runtime_error("ByteConversion: need extra_param[0]=bytes,[1]=base_hex,[2]=inc_dec");

    auto trim = [](std::string s)->std::string {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) == 0; }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c) == 0; }).base(), s.end());
        return s;
    };

    auto parse_dec_u64 = [&](const std::string& sin)->uint64_t {
        std::string s = trim(sin);
        if (s.empty()) throw std::runtime_error("ByteConversion: empty decimal");
        uint64_t v = 0;
        for (unsigned char c : s)
        {
            if (!std::isdigit(c)) throw std::runtime_error("ByteConversion: invalid decimal: " + s);
            uint8_t d = (uint8_t)(c - '0');
            if (v > (UINT64_MAX - d) / 10ULL) throw std::runtime_error("ByteConversion: decimal too large");
            v = v * 10ULL + d;
        }
        return v;
    };

    auto parse_hex_u64 = [&](std::string s)->uint64_t {
        s = trim(s);
        if (s.empty()) return 0ULL;
        if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.erase(0, 2);

        std::string hex;
        hex.reserve(s.size());
        for (char c : s) if (std::isxdigit((unsigned char)c)) hex.push_back(c);
        if (hex.empty()) return 0ULL;

        uint64_t v = 0;
        for (char ch : hex)
        {
            uint8_t n;
            if (ch >= '0' && ch <= '9') n = (uint8_t)(ch - '0');
            else if (ch >= 'a' && ch <= 'f') n = (uint8_t)(ch - 'a' + 10);
            else n = (uint8_t)(ch - 'A' + 10);

            if (v > (UINT64_MAX >> 4)) throw std::runtime_error("ByteConversion: hex too large");
            v = (v << 4) | n;
        }
        return v;
    };

    const size_t nbytes = (size_t)parse_dec_u64(extra_param[0]);
    if (nbytes < 1 || nbytes > 8) throw std::runtime_error("ByteConversion: bytes must be in [1..8]");

    const uint64_t base = parse_hex_u64(extra_param[1]);

    std::string inc_src = TrimCopy(input);
    if (inc_src.empty()) inc_src = extra_param[2];
    const uint64_t inc = parse_dec_u64(inc_src);

    const uint64_t maxv = (nbytes == 8) ? UINT64_MAX : ((1ULL << (8 * nbytes)) - 1ULL);
    if (base > maxv) throw std::runtime_error("ByteConversion: base exceeds width");
    if (inc > maxv) throw std::runtime_error("ByteConversion: inc exceeds width");
    if (inc > maxv - base) throw std::runtime_error("ByteConversion: addition overflow");

    const uint64_t sum = base + inc;

    out_bytes.reserve(out_bytes.size() + nbytes);
    if (endian)
    {
        for (size_t i = 0; i < nbytes; ++i)
        {
            size_t shift = 8 * (nbytes - 1 - i);
            out_bytes.push_back((uint8_t)((sum >> shift) & 0xFF));
        }
    }
    else
    {
        for (size_t i = 0; i < nbytes; ++i)
            out_bytes.push_back((uint8_t)((sum >> (8 * i)) & 0xFF));
    }
    return true;
}

bool SS_LightProtocolFactory::ToolGetRawData(std::string_view, const std::vector<std::string>& extra_param,
    std::vector<uint8_t>& out_bytes, bool)
{
    if (extra_param.empty()) throw std::runtime_error("GetRawData: extra_param[0] required");
    auto bytes = HexStringToBytes(extra_param[0]);
    out_bytes.insert(out_bytes.end(), bytes.begin(), bytes.end());
    return true;
}

// endian=true => BE endian=false => LE
bool SS_LightProtocolFactory::ToolGetStringMapValueToBytes(std::string_view input, const std::vector<std::string>& extra_param,
    std::vector<uint8_t>& out_bytes, bool endian)
{
    const std::string key = ToLowerCopy(input);
    if (key.empty()) throw std::runtime_error("GetStringMapValueToBytes: empty input");
    if (extra_param.empty()) throw std::runtime_error("GetStringMapValueToBytes: extra_param empty");

    auto trim_sv = [](std::string_view sv)->std::string_view {
        size_t i = 0, j = sv.size();
        while (i < j && IsSpace((unsigned char)sv[i])) ++i;
        while (j > i && IsSpace((unsigned char)sv[j - 1])) --j;
        return sv.substr(i, j - i);
    };

    for (const auto& item : extra_param)
    {
        std::string_view sv(item);
        sv = trim_sv(sv);
        if (sv.empty()) continue;

        size_t lt = sv.rfind('<');
        if (lt == std::string_view::npos) continue;
        size_t gt = sv.find('>', lt + 1);
        if (gt == std::string_view::npos || gt <= lt + 1) continue;

        std::string label = ToLowerCopy(std::string(trim_sv(sv.substr(0, lt))));
        std::string hexsv = std::string(trim_sv(sv.substr(lt + 1, gt - lt - 1)));

        if (label == key)
        {
            auto bytes = HexStringToBytes(hexsv);
            if (!endian) std::reverse(bytes.begin(), bytes.end());
            out_bytes.insert(out_bytes.end(), bytes.begin(), bytes.end());
            return true;
        }
    }

    throw std::runtime_error("GetStringMapValueToBytes: no mapping for '" + std::string(input) + "'");
}