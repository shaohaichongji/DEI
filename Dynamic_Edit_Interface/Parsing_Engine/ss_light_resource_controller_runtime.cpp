// ss_light_resource_controller_runtime.cpp
#include "ss_light_resource_controller_runtime.h"

SS_LightControllerRuntime::SS_LightControllerRuntime() = default;

void SS_LightControllerRuntime::BindTemplate(const SS_LightControllerTemplate& tpl)
{
    tpl_ = tpl;
}

void SS_LightControllerRuntime::BindInstance(const SS_LightControllerInstance& inst)
{
    inst_ = inst;
}

bool SS_LightControllerRuntime::Connect(std::string& out_error)
{
    out_error.clear();

    PublishConnectEvent_(SS_LightEventType::INSTANCE_CONNECTING, "connecting...");

    if (!transport_)
    {
        transport_ = CreateDefaultLightTransport();
        if (!transport_)
        {
            out_error = "连接：无法创建传输通道。";
            PublishConnectEvent_(SS_LightEventType::INSTANCE_CONNECT_FAILED, out_error);
            return false;
        }
    }

    BindTransportCallbacksIfNeeded_();

    const bool ok = transport_->Connect(inst_.connection, out_error);
    if (!ok)
    {
        PublishConnectEvent_(SS_LightEventType::INSTANCE_CONNECT_FAILED, out_error);
        return false;
    }

    // 标记 instance 内状态（可选）
    inst_.connection.connect_state = true;

    PublishConnectEvent_(SS_LightEventType::INSTANCE_CONNECTED, "connected");
    return true;
}

void SS_LightControllerRuntime::Disconnect()
{
    if (transport_)
    {
        transport_->Disconnect(); // transport 会回调 disconnected -> PublishConnectEvent_
    }
    else
    {
        // 兜底：从未连接过/transport 不存在时，也让 UI 收到一次“已断开”
        inst_.connection.connect_state = false;
        PublishConnectEvent_(SS_LightEventType::INSTANCE_DISCONNECTED, "断开连接（无传输通道）");
    }
}

bool SS_LightControllerRuntime::IsConnected() const
{
    return transport_ && transport_->IsConnected();
}

bool SS_LightControllerRuntime::SetParamAndBuildCommand(
    const SS_LightParamSetRequest& req,
    SS_LightParamSetResult& out_result)
{
    SS_LightBuiltPayload payload;
    return BuildAndMaybeSave_(req, out_result, payload);
}

bool SS_LightControllerRuntime::SetParamAndSend(
    const SS_LightParamSetRequest& req,
    SS_LightParamSetResult& out_result)
{
    SS_LightBuiltPayload payload;
    if (!BuildAndMaybeSave_(req, out_result, payload))
        return false;

    if (!out_result.ok)
        return false;

    // commit=SAVE_ONLY：不发送
    if (payload.protocol_type == SS_LIGHT_PROTOCOL_TYPE::UNKNOWN)
        return true;

    // 确保连接
    if (!IsConnected())
    {
        std::string err;
        out_result.ok = false;
        out_result.message = "Not connected";
        return false;
    }

    std::string err;

    if (payload.protocol_type == SS_LIGHT_PROTOCOL_TYPE::STRING)
    {
        std::vector<uint8_t> bytes(payload.string_cmd.begin(), payload.string_cmd.end());
        if (!transport_->SendBytes(bytes, err))
        {
            out_result.ok = false;
            out_result.message = "SendBytes failed: " + err;
            return false;
        }

        PublishFrameEvent_(SS_LightEventType::TX_FRAME, payload.string_cmd);

        out_result.message = "OK (sent)";
        return true;
    }

    if (payload.protocol_type == SS_LIGHT_PROTOCOL_TYPE::BYTE)
    {
        if (!transport_->SendBytes(payload.frame_bytes, err))
        {
            out_result.ok = false;
            out_result.message = "SendBytes failed: " + err;
            return false;
        }

        PublishFrameEvent_(SS_LightEventType::TX_FRAME, out_result.command_out); // 你已经是printable

        // out_result.command_out 已经是 printable(hex)
        out_result.message = "OK (sent)";
        return true;
    }

    out_result.ok = false;
    out_result.message = "模板中使用的协议类型不被支持。";
    return false;
}

bool SS_LightControllerRuntime::BuildAndMaybeSave_(
    const SS_LightParamSetRequest& req,
    SS_LightParamSetResult& out_result,
    SS_LightBuiltPayload& out_payload)
{
    out_result = SS_LightParamSetResult{};
    out_result.ok = false;
    out_result.command_out.clear();
    out_payload = SS_LightBuiltPayload{};

    // 0) 找参数定义
    auto it = tpl_.params.find(req.param_key);
    if (it == tpl_.params.end())
    {
        out_result.message = "模板中未找到相关参数：" + req.param_key;
        return false;
    }
    const SS_LightParamDef& def = it->second;

    // 1) 校验请求
    {
        std::string err;
        if (!ValidateRequestAgainstTemplate_(req, def, err))
        {
            out_result.message = err;
            return false;
        }
    }

    // 2) effective location：request 优先，否则用 template
    SS_LIGHT_PARAM_LOCATION effective_loc = req.location;
    if (effective_loc == SS_LIGHT_PARAM_LOCATION::UNKNOWN)
        effective_loc = def.location;

    if (effective_loc == SS_LIGHT_PARAM_LOCATION::UNKNOWN)
    {
        out_result.message = "参数位置未知（请求和模板均为未知）。";
        return false;
    }

    // 3) 写入 instance（保存值）
    if (effective_loc == SS_LIGHT_PARAM_LOCATION::GLOBAL)
    {
        inst_.global_param_values[req.param_key] = req.value_str;
    }
    else // CHANNEL
    {
        inst_.channel_param_values[req.param_key][req.channel_id] = req.value_str;
    }

    // 4) commit 策略：SAVE_ONLY 不生成命令
    if (def.command.commit == SS_LIGHT_COMMAND_COMMIT::SAVE_ONLY)
    {
        out_result.ok = true;
        out_result.message = "仅保存（提交选项为“仅保存”）。";
        out_result.command_out.clear();
        // out_payload 留空，外部会据此“不发送”
        return true;
    }

    // 5) 算 channel_index（仅当 CHANNEL）
    int channel_index = 0;
    if (effective_loc == SS_LIGHT_PARAM_LOCATION::CHANNEL)
    {
        if (!FindChannelIndexById_(req.channel_id, channel_index))
        {
            out_result.message = "通道ID未找到：" + req.channel_id;
            return false;
        }
    }

    // 6) build payload（STRING / BYTE）
    out_payload.protocol_type = tpl_.info.protocol_type;

    std::string err;
    if (tpl_.info.protocol_type == SS_LIGHT_PROTOCOL_TYPE::STRING)
    {
        if (!BuildStringCommand_(def.command, req.value_str, channel_index, out_payload.string_cmd, err))
        {
            out_result.message = err;
            return false;
        }
        out_payload.printable = out_payload.string_cmd;
    }
    else if (tpl_.info.protocol_type == SS_LIGHT_PROTOCOL_TYPE::BYTE)
    {
        // byte_transmission_params 来自模板，做个最基本校验
        if (tpl_.info.byte_transmission_params.device_address.empty())
        {
            out_result.message = "模板 byte_transmission_params.device_address 为空（BYTE 协议必须配置）";
            return false;
        }

        if (!BuildByteFrame_(def.command, req.value_str, channel_index, tpl_.info.byte_transmission_params,
            out_payload.frame_bytes, err))
        {
            out_result.message = err;
            return false;
        }
        out_payload.printable = BytesToHexString_(out_payload.frame_bytes, true);
    }
    else
    {
        out_result.message = "模板中使用的协议类型不被支持（未知类型）。";
        return false;
    }

    out_result.ok = true;
    out_result.command_out = out_payload.printable;
    out_result.message = "OK";
    return true;
}

bool SS_LightControllerRuntime::ValidateRequestAgainstTemplate_(
    const SS_LightParamSetRequest& req,
    const SS_LightParamDef& def,
    std::string& out_error) const
{
    out_error.clear();

    if (req.param_key.empty())
    {
        out_error = "param_key is empty.";
        return false;
    }

    // location 校验
    if (def.location != SS_LIGHT_PARAM_LOCATION::UNKNOWN &&
        req.location != SS_LIGHT_PARAM_LOCATION::UNKNOWN &&
        def.location != req.location)
    {
        out_error = "位置不匹配：请求=" + std::to_string((int)req.location) +
            " 模板=" + std::to_string((int)def.location);
        return false;
    }

    // effective location：request 优先，否则 template
    SS_LIGHT_PARAM_LOCATION effective_loc = req.location;
    if (effective_loc == SS_LIGHT_PARAM_LOCATION::UNKNOWN)
        effective_loc = def.location;

    if (effective_loc == SS_LIGHT_PARAM_LOCATION::CHANNEL)
    {
        if (req.channel_id.empty())
        {
            out_error = "当 location 的值为“CHANNEL”时，必须指定 channel_id 。";
            return false;
        }

        int idx = 0;
        if (!FindChannelIndexById_(req.channel_id, idx))
        {
            out_error = "在实例通道中未找到通道编号：" + req.channel_id;
            return false;
        }
    }

    return true;
}

bool SS_LightControllerRuntime::FindChannelIndexById_(const std::string& channel_id, int& out_channel_index) const
{
    out_channel_index = 0;
    for (const auto& ch : inst_.channels)
    {
        if (ch.channel_id == channel_id)
        {
            out_channel_index = ch.index;
            return true;
        }
    }
    return false;
}

bool SS_LightControllerRuntime::BuildStringCommand_(
    const SS_LightCommandRule& cmd_rule,
    const std::string& param_value_str,
    int channel_index,
    std::string& out_cmd,
    std::string& out_error) const
{
    out_error.clear();
    out_cmd.clear();

    return protocol_factory_.BuildCommand(cmd_rule, param_value_str, channel_index, out_cmd, out_error);
}

bool SS_LightControllerRuntime::BuildByteFrame_(
    const SS_LightCommandRule& cmd_rule,
    const std::string& param_value_str,
    int channel_index,
    const SS_LightByteTransmissionParams& tx_params,
    std::vector<uint8_t>& out_frame,
    std::string& out_error) const
{
    out_error.clear();
    out_frame.clear();

    // 1) 工厂生成 payload（PDU/主体）
    std::vector<uint8_t> payload;
    if (!protocol_factory_.BuildBytesCommand(cmd_rule, param_value_str, channel_index, payload, out_error))
        return false;

    // 2) Wrapper 封装成最终发送帧（RTU: 加 addr/CRC; TCP: MBAP; EMPTY: passthrough）
    if (!transmission_wrapper_.WrapPdu(payload, tx_params, out_frame, out_error))
        return false;

    return true;
}

std::string SS_LightControllerRuntime::BytesToHexString_(const std::vector<uint8_t>& bytes, bool with_prefix)
{
    static const char* kHex = "0123456789ABCDEF";
    std::string s;
    s.reserve(bytes.size() * 3 + (with_prefix ? 2 : 0));

    if (with_prefix) s += "0x";

    for (size_t i = 0; i < bytes.size(); ++i)
    {
        const uint8_t b = bytes[i];
        s.push_back(kHex[(b >> 4) & 0x0F]);
        s.push_back(kHex[b & 0x0F]);
        if (i + 1 < bytes.size()) s.push_back(' ');
    }
    return s;
}

void SS_LightControllerRuntime::BindTransportCallbacksIfNeeded_()
{
    if (!transport_ || transport_cb_bound_)
        return;

    // 收包
    transport_->SetRxCallback([this](const std::vector<uint8_t>& bytes) {
        // 注意：这里是 transport 线程回调，event_bus 的 handler 也会在该线程触发
        // UI 侧要用 Qt::QueuedConnection/InvokeMethod 自己切线程
        const std::string hex = BytesToHexString_(bytes, true);
        PublishFrameEvent_(SS_LightEventType::RX_FRAME, hex);
    });

    // 断线
    transport_->SetDisconnectedCallback([this](const std::string& reason) {
        inst_.connection.connect_state = false;
        PublishConnectEvent_(SS_LightEventType::INSTANCE_DISCONNECTED, reason);
    });

    // 错误
    transport_->SetErrorCallback([this](int code, const std::string& msg) {
        PublishErrorEvent_(code, msg);
    });

    transport_cb_bound_ = true;
}

void SS_LightControllerRuntime::PublishConnectEvent_(SS_LightEventType type, const std::string& msg)
{
    if (!event_bus_)
        return;

    SS_LightEventConnect ev;
    ev.type = type;
    ev.instance_id = inst_.info.instance_id;
    ev.message = msg;
    event_bus_->Publish(ev);
}

void SS_LightControllerRuntime::PublishErrorEvent_(int code, const std::string& msg)
{
    if (!event_bus_)
        return;

    SS_LightEventError ev;
    ev.type = SS_LightEventType::INSTANCE_ERROR;
    ev.instance_id = inst_.info.instance_id;
    ev.code = code;
    ev.message = msg;
    event_bus_->Publish(ev);
}

void SS_LightControllerRuntime::PublishFrameEvent_(SS_LightEventType type, const std::string& printable)
{
    if (!event_bus_)
        return;

    SS_LightEventFrame ev;
    ev.type = type;
    ev.instance_id = inst_.info.instance_id;
    ev.printable = printable;
    event_bus_->Publish(ev);
}
