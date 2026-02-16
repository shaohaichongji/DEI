// ss_light_resource_protocol_factory.h
#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <cstdint>

#include "ss_light_resource_types.h"

class SS_LightProtocolFactory
{
public:
    SS_LightProtocolFactory();

    // STRING 协议：输出字符串命令（<xxx> 替换回字符串）
    bool BuildCommand(
        const SS_LightCommandRule& rule,
        const std::string& param_value_str,
        int channel_index,
        std::string& out_cmd,
        std::string& out_error) const;

    // BYTE 协议：输出字节命令（pipeline：按 <xxx> 顺序把 bytes 追加到 out_bytes）
    // 重要约定：这里输出的是“裸 PDU”（不包含地址/MBAP/CRC），这些由 transmission wrapper 统一加
    bool BuildBytesCommand(
        const SS_LightCommandRule& rule,
        const std::string& param_value_str,
        int channel_index,
        std::vector<uint8_t>& out_bytes,
        std::string& out_error) const;

private:
    // ---- 内部辅助工具函数 ----
    static std::vector<std::string> ExtractPlaceholdersFast(const std::string& s);// 提取所有 <xxx> 中的内容（去重，用于 STRING）
    static void ReplaceAll(std::string& s, const std::string& from, const std::string& to);

    static std::string TrimCopy(std::string_view sv);
    static std::string ToLowerCopy(std::string_view sv);

    // 判断模板里是否包含精确占位符 "<name>"
    static bool ContainsPlaceholder(std::string_view tpl, std::string_view name);

    // 从 source 解析 raw_value：param_value/channel_num/channel_index/empty
    static bool ResolveSourceValue(
        const std::string& source,
        const std::string& param_value_str,
        int channel_index,
        std::string& out_value,
        std::string& out_error);

    bool ApplyPlaceholderRuleString(
        const SS_LightPlaceholderRule& ph_rule,
        const std::string& raw_value,
        std::string& out_value,
        std::string& out_error) const;

    bool ApplyPlaceholderRuleBytes(
        const SS_LightPlaceholderRule& ph_rule,
        const std::string& raw_value,
        std::vector<uint8_t>& out_bytes,
        std::string& out_error) const;

    // BYTE 模板辅助：把模板中“普通段落”解析为 hex 字节追加
    static bool AppendHexBytesFromText(std::string_view text, std::vector<uint8_t>& out_bytes, std::string& out_error);

    // ---- tools (string) ----
    static std::string ToolDoNothing(std::string_view input, const std::vector<std::string>& extra_param);
    static std::string ToolNumberToUpperAlpha(std::string_view input, const std::vector<std::string>& extra_param);
    static std::string ToolNumberToFixedDec(std::string_view input, const std::vector<std::string>& extra_param);
    static std::string ToolGetStringMapValue(std::string_view input, const std::vector<std::string>& extra_param);
    static std::string ToolDigitalCharacterCalculation(std::string_view input, const std::vector<std::string>& extra_param);

    // ---- tools (bytes) ----
    static std::vector<uint8_t> HexStringToBytes(const std::string& hex_str);

    static bool ToolByteConversion(std::string_view input, const std::vector<std::string>& extra_param,
        std::vector<uint8_t>& out_bytes, bool endian);

    static bool ToolGetRawData(std::string_view input, const std::vector<std::string>& extra_param,
        std::vector<uint8_t>& out_bytes, bool endian);

    static bool ToolGetStringMapValueToBytes(std::string_view input, const std::vector<std::string>& extra_param,
        std::vector<uint8_t>& out_bytes, bool endian);

private:
    using StringToolFn = std::function<std::string(std::string_view input, const std::vector<std::string>& extra_param)>;
    using BytesToolFn = std::function<bool(std::string_view input, const std::vector<std::string>& extra_param, std::vector<uint8_t>& out_bytes, bool endian)>;

    std::unordered_map<std::string, StringToolFn> string_tool_map_;
    std::unordered_map<std::string, BytesToolFn>  bytes_tool_map_;
};
