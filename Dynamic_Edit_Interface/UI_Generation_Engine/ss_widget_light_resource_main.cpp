#include "ss_widget_light_resource_main.h"
#include "moc/moc_ss_widget_light_resource_main.cpp"

#include <set>
#include <variant>
#include <type_traits>

#include <QtCore/QMetaObject>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>

#include "ss_widget_light_controller_tree.h"
#include "ss_widget_light_basic_info_panel.h"
#include "ss_widget_light_param_host.h"
#include "ss_widget_light_add_controller_dialog.h"

// core
#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_api.h"

static std::string ToStdString(const QString& s) { return s.toStdString(); }
static QString ToQString(const std::string& s) { return QString::fromStdString(s); }

SS_WidgetLightResourceMain::SS_WidgetLightResourceMain(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();
    InitConnections();
    ClearUiState();
}

void SS_WidgetLightResourceMain::SetSystem(SS_LightResourceSystem* system)
{
    system_ = system;

    // 关键：先取消旧订阅，防止重复订阅导致 UI 被轰炸
    light_bus_sub_.Reset();

    if (param_host_)
        param_host_->SetSystem(system_);

    if (!system_)
    {
        ClearUiState();
        return;
    }

    QString err;
    if (!RefreshControllers(err))
        QMessageBox::warning(this, tr("Failed to refresh the controller list"), err); //刷新控制器列表失败

    // 用 SubscribeEvents（系统对外 API），不直接 GetEventBus
    light_bus_sub_ = system_->SubscribeEvents([this](const SS_LightEvent& ev) {
        // 注意：任意线程回调，切回 UI 线程
        QMetaObject::invokeMethod(this, [this, ev]() {
            OnLightEventUiThread_(ev);
        }, Qt::QueuedConnection);
    });
}

bool SS_WidgetLightResourceMain::RefreshControllers(QString& out_error)
{
    out_error.clear();

    if (!system_)
    {
        out_error = "RefreshControllers: system_ is null.";
        return false;
    }

    if (controller_tree_) controller_tree_->Clear();

    std::vector<std::string> instance_ids = system_->ListInstanceIds();
    for (const auto& id : instance_ids)
    {
        SS_LightControllerInstance inst;
        if (!system_->GetInstance(id, inst))
            continue;

        controller_tree_->AddController(
            inst.info.instance_id,
            inst.info.display_name,
            inst.connection.connect_state);

        for (const auto& ch : inst.channels)
            controller_tree_->AddChannel(inst.info.instance_id, ch.channel_id, ch.display_name);
    }

    if (basic_info_panel_) basic_info_panel_->Clear();
    if (param_host_) param_host_->Clear();
    return true;
}

void SS_WidgetLightResourceMain::SlotControllerSelected(const QString& instance_id)
{
    QString err;
    if (!ShowController(instance_id, err))
        QMessageBox::warning(this, tr("Display controller parameters failed"), err);//显示控制器参数失败
}

void SS_WidgetLightResourceMain::SlotChannelSelected(const QString& instance_id, const QString& channel_id)
{
    QString err;
    if (!ShowChannel(instance_id, channel_id, err))
        QMessageBox::warning(this, tr("Failed to display channel parameters"), err);// 显示通道参数失败
}

void SS_WidgetLightResourceMain::SlotAddControllerClicked()
{
    if (!system_)
        return;

    SS_WidgetLightAddControllerDialog dlg(this);
    dlg.SetSystem(system_);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // 创建成功后刷新列表
    QString err;
    if (!RefreshControllers(err))
    {
        QMessageBox::warning(this, tr("Failed to refresh the controller list"), err);//刷新控制器列表失败
        return;
    }

    // 自动选中新创建的控制器（并触发右侧切换）
    const std::string new_id = dlg.GetCreatedInstanceId();
    if (!new_id.empty() && controller_tree_)
        controller_tree_->SelectControllerById(QString::fromStdString(new_id), true);
}

void SS_WidgetLightResourceMain::SlotAddChannelClicked(const QString& instance_id)
{
    if (!system_)
        return;

    const std::string inst_id = instance_id.toStdString();
    if (inst_id.empty())
        return;

    // 1) core 添加通道
    std::string new_channel_id;
    std::string core_err;
    if (!system_->AddDefaultChannel(inst_id, new_channel_id, core_err))
    {
        QMessageBox::warning(this, tr("Failed to add channel"), QString::fromStdString(core_err));//添加通道失败
        return;
    }

    // 2) 持久化
    std::string save_err;
    if (!system_->SaveInstanceById(inst_id, save_err))
    {
        const bool rollback_ok = system_->RemoveChannel(inst_id, new_channel_id);

        QString msg = tr("Save to failed:\n%1").arg(QString::fromStdString(save_err));// 保存失败：\n%1
        if (!rollback_ok)
            msg += tr("\n\n Rollback failed: The RemoveChannel operation also failed.");// \n\n 回滚失败：RemoveChannel 也失败了（可能导致内存态与UI态不一致）。

        QMessageBox::warning(this, tr("Save to failed"), msg);//保存失败
        return;
    }

    // 3) 重新拉 instance 拿 display_name
    SS_LightControllerInstance inst;
    if (!system_->GetInstance(inst_id, inst))
    {
        QMessageBox::warning(this, tr("Error"),// 错误
            tr("Adding the channel was successful, but refreshing the instance failed: GetInstance() failed."));//添加通道成功但刷新实例失败：GetInstance() 失败。
        return;
    }

    std::string channel_display_name = new_channel_id;
    for (const auto& ch : inst.channels)
    {
        if (ch.channel_id == new_channel_id)
        {
            channel_display_name = ch.display_name;
            break;
        }
    }

    // 4) UI tree 更新
    if (controller_tree_)
        controller_tree_->AddChannel(inst_id, new_channel_id, channel_display_name);

    controller_tree_->SelectChannelById(instance_id, QString::fromStdString(new_channel_id), false);
    // 5) 自动切换到新通道参数页
    QString ui_err;
    if (!ShowChannel(instance_id, QString::fromStdString(new_channel_id), ui_err))
    {
        QMessageBox::warning(this, tr("Failed to switch pages"), ui_err);//切换页面失败
        return;
    }
}

void SS_WidgetLightResourceMain::SlotInstanceUpdated(const QString& instance_id)
{
    if (!system_ || !controller_tree_)
        return;

    SS_LightControllerInstance inst;
    if (!system_->GetInstance(instance_id.toStdString(), inst))
        return;

    controller_tree_->UpdateControllerTitle(instance_id, ToQString(inst.info.display_name));


    // 如果当前右侧正显示这个控制器页，顺手刷新
    if (current_instance_id_ == instance_id && current_channel_id_.isEmpty())
    {
        QString err;
        (void)ShowController(instance_id, err);
    }
}

void SS_WidgetLightResourceMain::SlotChannelUpdated(const QString& instance_id, const QString& channel_id)
{
    if (!system_ || !controller_tree_)
        return;

    SS_LightControllerInstance inst;
    if (!system_->GetInstance(instance_id.toStdString(), inst))
        return;

    std::string name = channel_id.toStdString();
    for (const auto& ch : inst.channels)
    {
        if (ch.channel_id == channel_id.toStdString())
        {
            name = ch.display_name;
            break;
        }
    }

    controller_tree_->AddChannel(inst.info.instance_id, channel_id.toStdString(), name);
}

void SS_WidgetLightResourceMain::InitQSS()
{
}

void SS_WidgetLightResourceMain::BuildUi()
{
    main_splitter_ = new QSplitter(this);
    main_splitter_->setOrientation(Qt::Horizontal);

    // -------- Left panel (list + basic info) --------
    QWidget* left_panel = new QWidget(main_splitter_);
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(8, 8, 8, 8);
    left_layout->setSpacing(8);

    group_list_ = new QGroupBox(tr("List of Light Sources"), left_panel);//光源列表
    QVBoxLayout* list_layout = new QVBoxLayout(group_list_);
    list_layout->setContentsMargins(8, 8, 8, 8);

    controller_tree_ = new SS_WidgetLightControllerTree(group_list_);
    list_layout->addWidget(controller_tree_);

    group_basic_info_ = new QGroupBox(tr("Basic information"), left_panel);//基本信息
    QVBoxLayout* info_layout = new QVBoxLayout(group_basic_info_);
    info_layout->setContentsMargins(8, 8, 8, 8);

    basic_info_panel_ = new SS_WidgetLightBasicInfoPanel(group_basic_info_);
    info_layout->addWidget(basic_info_panel_);

    left_layout->addWidget(group_list_, 7);
    left_layout->addWidget(group_basic_info_, 3);

    // -------- Right panel (params host) --------
    QWidget* right_panel = new QWidget(main_splitter_);
    QVBoxLayout* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(8, 8, 8, 8);
    right_layout->setSpacing(8);

    group_params_ = new QGroupBox(tr("parameter list"), right_panel);//参数列表
    QVBoxLayout* params_layout = new QVBoxLayout(group_params_);
    params_layout->setContentsMargins(8, 8, 8, 8);

    param_host_ = new SS_WidgetLightParamHost(group_params_);
    params_layout->addWidget(param_host_);

    right_layout->addWidget(group_params_);

    // splitter sizing
    main_splitter_->addWidget(left_panel);
    main_splitter_->addWidget(right_panel);
    main_splitter_->setStretchFactor(0, 0);
    main_splitter_->setStretchFactor(1, 1);
    main_splitter_->setCollapsible(0, false);
    main_splitter_->setCollapsible(1, false);

    // root layout
    QHBoxLayout* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(main_splitter_);

    InitQSS();
}

void SS_WidgetLightResourceMain::InitConnections()
{
    connect(controller_tree_, &SS_WidgetLightControllerTree::SignalControllerSelected,
        this, &SS_WidgetLightResourceMain::SlotControllerSelected);
    connect(controller_tree_, &SS_WidgetLightControllerTree::SignalChannelSelected,
        this, &SS_WidgetLightResourceMain::SlotChannelSelected);
    connect(controller_tree_, &SS_WidgetLightControllerTree::SignalAddControllerClicked,
        this, &SS_WidgetLightResourceMain::SlotAddControllerClicked);
    connect(controller_tree_, &SS_WidgetLightControllerTree::SignalAddChannelClicked,
        this, &SS_WidgetLightResourceMain::SlotAddChannelClicked);

    connect(param_host_, &SS_WidgetLightParamHost::SignalChannelUpdated,
        this, &SS_WidgetLightResourceMain::SlotChannelUpdated);
    connect(param_host_, &SS_WidgetLightParamHost::SignalInstanceUpdated,
        this, &SS_WidgetLightResourceMain::SlotInstanceUpdated);
}

void SS_WidgetLightResourceMain::ClearUiState()
{
    current_instance_id_.clear();
    current_channel_id_.clear();

    if (controller_tree_) controller_tree_->Clear();
    if (basic_info_panel_) basic_info_panel_->Clear();
    if (param_host_) param_host_->Clear();
}

bool SS_WidgetLightResourceMain::ShowController(const QString& instance_id, QString& out_error)
{
    out_error.clear();

    if (!system_)
    {
        out_error = "ShowController: system_ is null.";
        return false;
    }

    SS_LightControllerInstance inst;
    if (!system_->GetInstance(ToStdString(instance_id), inst))
    {
        out_error = "ShowController: GetInstance failed: " + instance_id;
        return false;
    }

    SS_LightControllerTemplate tpl;
    if (!system_->GetTemplate(inst.info.template_id, tpl))
    {
        out_error = "ShowController: GetTemplate failed: " + ToQString(inst.info.template_id);
        return false;
    }

    if (basic_info_panel_) basic_info_panel_->SetInfo(tpl, inst);
    if (param_host_) param_host_->ShowControllerParams(tpl, inst);

    current_instance_id_ = instance_id;
    current_channel_id_.clear();
    return true;
}

bool SS_WidgetLightResourceMain::ShowChannel(const QString& instance_id, const QString& channel_id, QString& out_error)
{
    out_error.clear();

    if (!system_)
    {
        out_error = "ShowChannel: system_ is null.";
        return false;
    }

    SS_LightControllerInstance inst;
    if (!system_->GetInstance(ToStdString(instance_id), inst))
    {
        out_error = "ShowChannel: GetInstance failed: " + instance_id;
        return false;
    }

    SS_LightControllerTemplate tpl;
    if (!system_->GetTemplate(inst.info.template_id, tpl))
    {
        out_error = "ShowChannel: GetTemplate failed: " + ToQString(inst.info.template_id);
        return false;
    }

    if (basic_info_panel_) basic_info_panel_->SetInfo(tpl, inst);
    if (param_host_) param_host_->ShowChannelParams(tpl, inst, ToStdString(channel_id));

    current_instance_id_ = instance_id;
    current_channel_id_ = channel_id;
    return true;
}

void SS_WidgetLightResourceMain::OnLightEventUiThread_(const SS_LightEvent& ev)
{
    std::visit([this](auto&& e) { HandleEvent_(e); }, ev);
}

void SS_WidgetLightResourceMain::HandleEvent_(const SS_LightEventConnect& e)
{
    if (!controller_tree_) return;

    const QString id = QString::fromStdString(e.instance_id);

    if (e.type == SS_LightEventType::INSTANCE_CONNECTED)
        controller_tree_->SetControllerConnected(id, true);
    else if (e.type == SS_LightEventType::INSTANCE_DISCONNECTED ||
        e.type == SS_LightEventType::INSTANCE_CONNECT_FAILED)
        controller_tree_->SetControllerConnected(id, false);

    // 需要的话，把 e.message 打到日志面板
}

void SS_WidgetLightResourceMain::HandleEvent_(const SS_LightEventError& e)
{
    // TODO: 打日志面板，不弹窗
    (void)e;
}

void SS_WidgetLightResourceMain::HandleEvent_(const SS_LightEventFrame& e)
{
    // TODO: TX/RX 打日志面板
    (void)e;
}
