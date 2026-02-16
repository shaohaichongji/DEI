#pragma once
#include <string>
#include <variant>
#include <cstdint>

enum class SS_LightEventType
{
    INSTANCE_CONNECTING,
    INSTANCE_CONNECTED,
    INSTANCE_DISCONNECTED,
    INSTANCE_CONNECT_FAILED,
    INSTANCE_ERROR,
    TX_FRAME,     // 可选：发送了什么
    RX_FRAME      // 可选：收到了什么
};

struct SS_LightEventBase
{
    SS_LightEventType type{};
    std::string instance_id;
};

// 连接状态类事件
struct SS_LightEventConnect
{
    SS_LightEventType type{};
    std::string instance_id;
    std::string message; // error 或提示
};

// 错误事件
struct SS_LightEventError
{
    SS_LightEventType type{ SS_LightEventType::INSTANCE_ERROR };
    std::string instance_id;
    int code = 0;
    std::string message;
};

// 发送/接收帧事件（可选）
struct SS_LightEventFrame
{
    SS_LightEventType type{};
    std::string instance_id;
    std::string printable; // hex/string
};

using SS_LightEvent = std::variant<
    SS_LightEventBase,
    SS_LightEventConnect,
    SS_LightEventError,
    SS_LightEventFrame
>;
