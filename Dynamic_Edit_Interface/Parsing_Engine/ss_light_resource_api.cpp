// ss_light_resource_api.cpp
#include "ss_light_resource_api.h"
#include "ss_light_resource_manager.h"

SS_LightResourceSystem* SS_LightResourceSystem::instance_ = nullptr;

SS_LightResourceSystem* SS_LightResourceSystem::GetInstance()
{
    if (!instance_)
        instance_ = new SS_LightResourceSystem();
    return instance_;
}

SS_LightResourceSystem::SS_LightResourceSystem() = default;

SS_LightResourceSystem::~SS_LightResourceSystem()
{
    Shutdown();
}

bool SS_LightResourceSystem::Init(const std::string& template_dir, const std::string& instance_dir, std::string& out_error)
{
    out_error.clear();

    template_dir_ = template_dir;
    instance_dir_ = instance_dir;

    manager_ = std::make_unique<SS_LightResourceManager>(template_dir_, instance_dir_);
    manager_->SetEventBus(&event_bus_);
    return manager_->Init(out_error);
}

void SS_LightResourceSystem::Shutdown()
{
    if (manager_)
    {
        manager_->Shutdown();
        manager_.reset();
    }
}

bool SS_LightResourceSystem::ReloadTemplates(std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "ReloadTemplates: system not initialized.";
        out_error = "重新加载模板：系统尚未初始化。";
        return false;
    }
    return manager_->ReloadTemplates(out_error);
}

std::vector<std::string> SS_LightResourceSystem::ListTemplateIds() const
{
    if (!manager_) return {};
    return manager_->ListTemplateIds();
}

bool SS_LightResourceSystem::GetTemplate(const std::string& template_id, SS_LightControllerTemplate& out_tpl) const
{
    if (!manager_) return false;
    return manager_->GetTemplate(template_id, out_tpl);
}

bool SS_LightResourceSystem::ReloadInstances(std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "ReloadInstances: system not initialized.";
        out_error = "重新加载实例：系统尚未初始化。";
        return false;
    }
    return manager_->ReloadInstances(out_error);
}

bool SS_LightResourceSystem::LoadInstanceFile(const std::string& instance_yaml_path, std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "LoadInstanceFile: system not initialized.";
        out_error = "加载实例文件：系统未初始化。";
        return false;
    }
    return manager_->LoadInstanceFile(instance_yaml_path, out_error);
}

std::vector<std::string> SS_LightResourceSystem::ListInstanceIds() const
{
    if (!manager_) return {};
    return manager_->ListInstanceIds();
}

bool SS_LightResourceSystem::GetInstance(const std::string& instance_id, SS_LightControllerInstance& out_inst) const
{
    if (!manager_) return false;
    return manager_->GetInstance(instance_id, out_inst);
}

bool SS_LightResourceSystem::CreateInstanceFromTemplate(
    const std::string& template_id,
    const std::string& display_name,
    std::string& out_instance_id,
    std::string& out_error)
{
    out_error.clear();
    out_instance_id.clear();

    if (!manager_)
    {
        //out_error = "CreateInstanceFromTemplate: system not initialized.";
        out_error = "从模板创建实例”：系统尚未初始化。";
        return false;
    }
    return manager_->CreateInstanceFromTemplate(template_id, display_name, out_instance_id, out_error);
}

bool SS_LightResourceSystem::ResolveInstancePath(const std::string& instance_id, std::string& out_path, std::string& out_error) const
{
    out_error.clear();
    out_path.clear();

    if (!manager_)
    {
        //out_error = "ResolveInstancePath: system not initialized.";
        out_error = "ResolveInstancePath：系统未初始化。";
        return false;
    }
    return manager_->ResolveInstancePath(instance_id, out_path, out_error);
}

bool SS_LightResourceSystem::SaveInstanceById(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "SaveInstanceById: system not initialized.";
        out_error = "保存实例（按 ID 保存）：系统尚未初始化。";
        return false;
    }
    return manager_->SaveInstanceById(instance_id, out_error);
}

bool SS_LightResourceSystem::DeleteInstanceById(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "DeleteInstanceById: system not initialized.";
        out_error = "删除实例（按 ID）：系统未初始化。";
        return false;
    }
    return manager_->DeleteInstanceById(instance_id, out_error);
}

bool SS_LightResourceSystem::RenameController(const std::string& instance_id, const std::string& new_display_name)
{
    if (!manager_) return false;
    return manager_->RenameController(instance_id, new_display_name);
}

bool SS_LightResourceSystem::SetControllerEnabled(const std::string& instance_id, bool enabled)
{
    if (!manager_) return false;
    return manager_->SetControllerEnabled(instance_id, enabled);
}

bool SS_LightResourceSystem::SetControllerOrder(const std::string& instance_id, int order)
{
    if (!manager_) return false;
    return manager_->SetControllerOrder(instance_id, order);
}

bool SS_LightResourceSystem::AddDefaultChannel(
    const std::string& instance_id,
    std::string& out_channel_id,
    std::string& out_error)
{
    out_channel_id.clear();
    out_error.clear();

    if (!manager_)
    {
        //out_error = "AddDefaultChannel: system not initialized.";
        out_error = "AddDefaultChannel：系统尚未初始化。";
        return false;
    }

    return manager_->AddDefaultChannel(instance_id, out_channel_id, out_error);
}

bool SS_LightResourceSystem::AddDefaultChannelAndSave(const std::string& instance_id, std::string& out_channel_id, std::string& out_error)
{
    out_channel_id.clear();
    out_error.clear();

    if (!manager_)
    {
        //out_error = "AddDefaultChannelAndSave: system not initialized.";
        out_error = "添加默认通道并保存：系统未初始化。";
        return false;
    }

    if (!manager_->AddDefaultChannel(instance_id, out_channel_id, out_error))
        return false;

    if (!manager_->SaveInstanceById(instance_id, out_error))
        return false;

    return true;
}

bool SS_LightResourceSystem::RenameChannel(const std::string& instance_id, const std::string& channel_id, const std::string& new_display_name)
{
    if (!manager_) return false;
    return manager_->RenameChannel(instance_id, channel_id, new_display_name);
}

bool SS_LightResourceSystem::RemoveChannel(const std::string& instance_id, const std::string& channel_id)
{
    if (!manager_) return false;
    return manager_->RemoveChannel(instance_id, channel_id);
}

bool SS_LightResourceSystem::SetChannelIndex(const std::string& instance_id,
    const std::string& channel_id,
    int new_channel_index,
    std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "SetChannelIndex: system not initialized.";
        out_error = "设置通道索引：系统未初始化。";
        return false;
    }
    return manager_->SetChannelIndex(instance_id, channel_id, new_channel_index, out_error);
}


bool SS_LightResourceSystem::ConnectInstance(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "ConnectInstance: system not initialized.";
        out_error = "连接实例：系统尚未初始化。";
        return false;
    }
    return manager_->ConnectInstance(instance_id, out_error);
}

bool SS_LightResourceSystem::DisconnectInstance(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "DisconnectInstance: system not initialized.";
        out_error = "断开实例连接：系统尚未初始化。";
        return false;
    }
    return manager_->DisconnectInstance(instance_id, out_error);
}

bool SS_LightResourceSystem::SetParameterValue(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result)
{
    if (!manager_) return false;
    return manager_->SetParameterValue(req, out_result);
}

bool SS_LightResourceSystem::SetParameterAndSend(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result)
{
    if (!manager_) return false;
    return manager_->SetParameterAndSend(req, out_result);
}

bool SS_LightResourceSystem::UpdateConnectionConfig(
    const std::string& instance_id,
    const SS_LightConnectionConfig& conn,
    std::string& out_error)
{
    out_error.clear();
    if (!manager_)
    {
        //out_error = "UpdateConnectionConfig: system not initialized.";
        out_error = "更新连接配置：系统尚未初始化。";
        return false;
    }
    return manager_->UpdateConnectionConfig(instance_id, conn, out_error);
}

SS_LightEventBus::Subscription SS_LightResourceSystem::SubscribeEvents(SS_LightEventBus::Handler cb)
{
    return event_bus_.Subscribe(std::move(cb));
}

