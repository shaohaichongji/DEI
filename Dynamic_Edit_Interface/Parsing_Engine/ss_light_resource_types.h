#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

enum class SS_LIGHT_CONNECT_TYPE
{
    UNKNOWN = 0,
    SERIAL,
    SOCKET
};

enum class SS_LIGHT_PROTOCOL_TYPE
{
    UNKNOWN = 0,
    STRING,
    BYTE
};

enum class SS_LIGHT_PARAM_LOCATION
{
    UNKNOWN = 0,
    GLOBAL,
    CHANNEL
};

enum class SS_LIGHT_VALUE_TYPE
{
    UNKNOWN = 0,
    INT,
    DOUBLE,
    BOOL,
    STRING,
    ENUM,
    BYTES
};

enum class SS_LIGHT_WIDGET_TYPE
{
    UNKNOWN = 0,
    LINE_EDIT,
    COMBO_BOX,
    SPIN_BOX,
    DOUBLE_SPIN_BOX,
    CHECK_BOX,
    SLIDER
};

enum class SS_LIGHT_COMMAND_SCOPE
{
    UNKNOWN = 0,
    GLOBAL,
    CHANNEL
};

enum class SS_LIGHT_COMMAND_WHEN
{
    UNKNOWN = 0,
    ON_CHANGE,
    ON_COMMIT,
    MANUAL
};

enum class SS_LIGHT_COMMAND_COMMIT
{
    UNKNOWN = 0,
    SEND_ONLY,
    SAVE_ONLY,
    SAVE_AND_SEND
};

// Widget参数
struct SS_LightWidgetConfig
{
    SS_LIGHT_WIDGET_TYPE type = SS_LIGHT_WIDGET_TYPE::UNKNOWN;

    // Common behavior
    bool enabled = true;
    bool visible = true;
    bool read_only = false;
    std::string tooltip;

    // 控件默认值
    std::string default_value;

    // 控件值范围 (SPIN_BOX / DOUBLE_SPIN_BOX / SLIDER)
    double min_value = 0.0;
    double max_value = 0.0;
    double step = 1.0;
    int decimals = 0;

    // 文本输入最大长度 (LINE_EDIT)
    int text_max_length = 0;
    std::string placeholder;

    // 控件可选项 (COMBO_BOX / ENUM)
    std::vector<std::string> options;
};

// 协议指令模板中具体某个占位符的生成规则
struct SS_LightPlaceholderRule
{
    // 协议参数来源 param_value / channel_num / ...
    std::string source;

    // 大小端开关，只在protocol_type == BYTE时
    bool endian = false;

    // 协议解析工具名称
    std::string parser_tool;

    // 协议解析工具额外参数
    std::vector<std::string> extra_param;
};

struct SS_LightCommandRule
{
    SS_LIGHT_COMMAND_WHEN when = SS_LIGHT_COMMAND_WHEN::ON_CHANGE;
    SS_LIGHT_COMMAND_SCOPE scope = SS_LIGHT_COMMAND_SCOPE::UNKNOWN;
    SS_LIGHT_COMMAND_COMMIT commit = SS_LIGHT_COMMAND_COMMIT::SAVE_AND_SEND;

    std::string cmd_template; // e.g. "S<channel><value>#"
    std::unordered_map<std::string, SS_LightPlaceholderRule> placeholders;
};

// 具体某条参数的模型
struct SS_LightParamDef
{
    std::string key; // 参数名，如：PulseWidthTime
    SS_LIGHT_PARAM_LOCATION location = SS_LIGHT_PARAM_LOCATION::UNKNOWN;
    std::string display_name;

    SS_LIGHT_VALUE_TYPE value_type = SS_LIGHT_VALUE_TYPE::UNKNOWN;

    // 控件参数中也有默认值，此项作为副本，方便core调用
    std::string default_value;

    SS_LightWidgetConfig widget;
    SS_LightCommandRule command;
};

// 控制器列表下的通道项
struct SS_LightChannelItem
{
    std::string channel_id;   // ch_0
    int index = 0;            // 0..channel_max-1
    std::string display_name; // UI tree label

    bool enabled = true;
    bool deleted = false;
    int order = 0;
};

// 参数设置请求（UI -> core）
struct SS_LightParamSetRequest
{
    std::string instance_id;
    std::string param_key;
    SS_LIGHT_PARAM_LOCATION location = SS_LIGHT_PARAM_LOCATION::UNKNOWN;

    // 通道类型参数时有效
    std::string channel_id;

    // 统一按照sring类型，具体数值类型由value_type区分，core负责转换和校验
    std::string value_str;
};

struct SS_LightParamSetResult
{
    bool ok = false;
    std::string message;

    // 解析后的指令，可保持string字符串、或转为十六进制字节流
    std::string command_out;
};
