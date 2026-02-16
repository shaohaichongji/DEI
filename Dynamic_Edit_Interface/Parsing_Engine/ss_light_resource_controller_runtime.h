// ss_light_resource_controller_runtime.h
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "ss_light_resource_models.h"
#include "ss_light_resource_protocol_factory.h"
#include "ss_light_resource_transmission_wrapper.h"
#include "ss_light_resource_transport.h"

#include "ss_light_resource_events.h"
#include "ss_light_resource_event_bus.h"

class SS_LightControllerRuntime
{
public:
    SS_LightControllerRuntime();

    void BindTemplate(const SS_LightControllerTemplate& tpl);
    void BindInstance(const SS_LightControllerInstance& inst);

    const SS_LightControllerTemplate& GetTemplate() const { return tpl_; }
    const SS_LightControllerInstance& GetInstance() const { return inst_; }
    SS_LightControllerInstance& GetInstanceMutable() { return inst_; }

    // 连接控制
    bool Connect(std::string& out_error);
    void Disconnect();
    bool IsConnected() const;

    // 核心入口：写入参数值 + 根据模板生成“最终发送内容”
    // - STRING：out_result.command_out = 最终字符串命令
    // - BYTE：out_result.command_out = 最终帧的 hex 字符串（便于日志/调试）
    bool SetParamAndBuildCommand(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result);

    // build + send
    bool SetParamAndSend(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result);

    void SetEventBus(SS_LightEventBus* bus) { event_bus_ = bus; }

private:
    // --- core shared pipeline ---
    struct SS_LightBuiltPayload
    {
        SS_LIGHT_PROTOCOL_TYPE protocol_type = SS_LIGHT_PROTOCOL_TYPE::UNKNOWN;

        // STRING 模式有效
        std::string string_cmd;

        // BYTE 模式有效（最终可发送帧）
        std::vector<uint8_t> frame_bytes;

        // 统一给 UI/日志用（STRING=string_cmd；BYTE=hex(frame_bytes)）
        std::string printable;
    };

    bool BuildAndMaybeSave_(
        const SS_LightParamSetRequest& req,
        SS_LightParamSetResult& out_result,
        SS_LightBuiltPayload& out_payload);

    // --- helpers ---
    bool ValidateRequestAgainstTemplate_(
        const SS_LightParamSetRequest& req,
        const SS_LightParamDef& def,
        std::string& out_error) const;

    bool FindChannelIndexById_(
        const std::string& channel_id,
        int& out_channel_index) const;

    bool BuildStringCommand_(
        const SS_LightCommandRule& cmd_rule,
        const std::string& param_value_str,
        int channel_index,
        std::string& out_cmd,
        std::string& out_error) const;

    // BYTE：build payload -> wrap frame
    bool BuildByteFrame_(
        const SS_LightCommandRule& cmd_rule,
        const std::string& param_value_str,
        int channel_index,
        const SS_LightByteTransmissionParams& tx_params,
        std::vector<uint8_t>& out_frame,
        std::string& out_error) const;

    static std::string BytesToHexString_(const std::vector<uint8_t>& bytes, bool with_prefix);

    void BindTransportCallbacksIfNeeded_();
    void PublishConnectEvent_(SS_LightEventType type, const std::string& msg);
    void PublishErrorEvent_(int code, const std::string& msg);
    void PublishFrameEvent_(SS_LightEventType type, const std::string& printable);

private:
    SS_LightControllerTemplate tpl_;
    SS_LightControllerInstance inst_;

    SS_LightProtocolFactory protocol_factory_;
    SS_LightTransmissionWrapper transmission_wrapper_;

    std::unique_ptr<SS_LightTransport> transport_;

    SS_LightEventBus* event_bus_ = nullptr;
    bool transport_cb_bound_ = false;
};