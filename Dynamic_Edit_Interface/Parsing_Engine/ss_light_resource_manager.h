// ss_light_resource_manager.h
#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include "ss_light_resource_controller_runtime.h"
#include "ss_light_resource_yaml_codec.h"

class SS_LightResourceManager
{
public:
    SS_LightResourceManager(std::string template_dir, std::string instance_dir);

    // 载入所有模板、可选载入实例
    bool Init(std::string& out_error);
    void Shutdown();

    // templates
    bool LoadTemplateFile(const std::string& template_yaml_path, std::string& out_error);
    bool ReloadTemplates(std::string& out_error);
    std::vector<std::string> ListTemplateIds() const;
    bool GetTemplate(const std::string& template_id, SS_LightControllerTemplate& out_tpl) const;

    // instances (runtime)
    bool LoadInstanceFile(const std::string& instance_yaml_path, std::string& out_error);
    bool ReloadInstances(std::string& out_error);
    bool AddOrReplaceInstance(const SS_LightControllerInstance& inst, std::string& out_error);
    bool RemoveInstance(const std::string& instance_id);

    std::vector<std::string> ListInstanceIds() const;
    bool GetInstance(const std::string& instance_id, SS_LightControllerInstance& out_inst) const;

    // instance metadata ops
    bool RenameController(const std::string& instance_id, const std::string& new_display_name);
    bool SetControllerEnabled(const std::string& instance_id, bool enabled);
    bool SetControllerOrder(const std::string& instance_id, int order);

    // channels ops
    bool AddChannel(const std::string& instance_id, int channel_index, const std::string& channel_display_name, std::string& out_channel_id);
    bool RenameChannel(const std::string& instance_id, const std::string& channel_id, const std::string& new_display_name);
    bool RemoveChannel(const std::string& instance_id, const std::string& channel_id);

    // change channel index (with conflict check)
    bool SetChannelIndex(const std::string& instance_id, const std::string& channel_id, int new_channel_index, std::string& out_error);

    // 默认加通道（UI 点 “+” 用这个）
    // - 自动选择最小可用 channel_index
    // - 自动命名
    // - 自动补齐 channel 参数默认值
    bool AddDefaultChannel(const std::string& instance_id, std::string& out_channel_id, std::string& out_error);

    // persist ops (闭环关键)
    bool ResolveInstancePath(const std::string& instance_id, std::string& out_path, std::string& out_error) const;
    bool SaveInstanceById(const std::string& instance_id, std::string& out_error);
    bool DeleteInstanceById(const std::string& instance_id, std::string& out_error);

    // core entry
    bool SetParameterValue(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result);
    // build + send（对应 runtime::SetParamAndSend）
    bool SetParameterAndSend(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result);

    // instances creation
    bool CreateInstanceFromTemplate(
        const std::string& template_id,
        const std::string& display_name,
        std::string& out_instance_id,
        std::string& out_error);

    // 连接控制（最小闭环）
    bool ConnectInstance(const std::string& instance_id, std::string& out_error);
    bool DisconnectInstance(const std::string& instance_id, std::string& out_error);
    bool IsInstanceConnected(const std::string& instance_id) const;

    // 更新连接配置（UI 编辑串口/网口参数需要）
    bool UpdateConnectionConfig(const std::string& instance_id, const SS_LightConnectionConfig& conn, std::string& out_error);

    void SetEventBus(SS_LightEventBus* bus) { event_bus_ = bus; }

private:
    const SS_LightControllerTemplate* FindTemplate(const std::string& template_id) const;
    SS_LightControllerRuntime* FindRuntime(const std::string& instance_id);
    const SS_LightControllerRuntime* FindRuntime(const std::string& instance_id) const;

    static void CleanupChannelParamValues(SS_LightControllerInstance& inst, const std::string& channel_id);

private:
    std::string template_dir_;
    std::string instance_dir_;

    SS_LightYamlCodec codec_;

    // template_id -> template
    std::unordered_map<std::string, SS_LightControllerTemplate> templates_;

    // instance_id -> runtime
    std::unordered_map<std::string, std::unique_ptr<SS_LightControllerRuntime>> runtimes_;

    // instance_id -> yaml path（支持 UUID / controller_123 任意格式 id）
    std::unordered_map<std::string, std::string> instance_paths_;

    SS_LightEventBus* event_bus_ = nullptr;
};