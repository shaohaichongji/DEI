// ss_light_resource_api.h
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "ss_light_resource_models.h"
#include "ss_light_resource_types.h"
#include "ss_light_resource_event_bus.h"

#ifdef SS_LightResourceExport
#define SS_LIGHT_RESOURCE_API __declspec(dllexport)
#else
#define SS_LIGHT_RESOURCE_API __declspec(dllimport)
#endif

class SS_LightResourceManager;
class SS_LightEventBus;

// - 所有错误通过 out_error 返回
class SS_LIGHT_RESOURCE_API SS_LightResourceSystem
{
public:
    static SS_LightResourceSystem* GetInstance();

    bool Init(const std::string& template_dir, const std::string& instance_dir, std::string& out_error);
    void Shutdown();

    // -------- Templates --------
    bool ReloadTemplates(std::string& out_error);
    std::vector<std::string> ListTemplateIds() const;
    bool GetTemplate(const std::string& template_id, SS_LightControllerTemplate& out_tpl) const;

    // -------- Instances --------
    bool ReloadInstances(std::string& out_error);
    bool LoadInstanceFile(const std::string& instance_yaml_path, std::string& out_error);
    std::vector<std::string> ListInstanceIds() const;
    bool GetInstance(const std::string& instance_id, SS_LightControllerInstance& out_inst) const;

    bool CreateInstanceFromTemplate(const std::string& template_id, const std::string& display_name, std::string& out_instance_id, std::string& out_error);

    // -------- Persist ops --------
    bool ResolveInstancePath(const std::string& instance_id, std::string& out_path, std::string& out_error) const;
    bool SaveInstanceById(const std::string& instance_id, std::string& out_error);
    bool DeleteInstanceById(const std::string& instance_id, std::string& out_error);

    // -------- UI-ish ops (metadata / channels) --------
    bool RenameController(const std::string& instance_id, const std::string& new_display_name);
    bool SetControllerEnabled(const std::string& instance_id, bool enabled);
    bool SetControllerOrder(const std::string& instance_id, int order);

    bool AddDefaultChannel(const std::string& instance_id, std::string& out_channel_id, std::string& out_error);
    bool AddDefaultChannelAndSave(const std::string& instance_id, std::string& out_channel_id, std::string& out_error);
    bool RenameChannel(const std::string& instance_id, const std::string& channel_id, const std::string& new_display_name);
    bool RemoveChannel(const std::string& instance_id, const std::string& channel_id);

    bool SetChannelIndex(const std::string& instance_id, const std::string& channel_id, int new_channel_index, std::string& out_error);

    // -------- Minimal send loop --------
    bool ConnectInstance(const std::string& instance_id, std::string& out_error);
    bool DisconnectInstance(const std::string& instance_id, std::string& out_error);
    bool SetParameterValue(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result);      // build only
    bool SetParameterAndSend(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result);   // build + send

    // 更新连接配置（给 UI 用）
    bool UpdateConnectionConfig(const std::string& instance_id, const SS_LightConnectionConfig& conn, std::string& out_error);

    SS_LightEventBus::Subscription SubscribeEvents(SS_LightEventBus::Handler cb);
    SS_LightEventBus& GetEventBus() { return event_bus_; }
    const SS_LightEventBus& GetEventBus() const { return event_bus_; }

private:
    SS_LightResourceSystem();
    ~SS_LightResourceSystem();
    SS_LightResourceSystem(const SS_LightResourceSystem&) = delete;
    SS_LightResourceSystem& operator=(const SS_LightResourceSystem&) = delete;

private:
    static SS_LightResourceSystem* instance_;

    std::string template_dir_;
    std::string instance_dir_;
    std::unique_ptr<SS_LightResourceManager> manager_;

    SS_LightEventBus event_bus_;
};