#include "ss_widget_light_controller_param_page.h"
#include "moc/moc_ss_widget_light_controller_param_page.cpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>

#include "ss_light_param_form_builder.h"
#include "ss_widget_light_connection_panel.h"

#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_api.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

static QString ToQString(const std::string& s) { return QString::fromStdString(s); }

SS_WidgetLightControllerParamPage::SS_WidgetLightControllerParamPage(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();

}

void SS_WidgetLightControllerParamPage::SetData(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst)
{
    instance_id_ = inst.info.instance_id;

    RefreshTitle_(inst);

    // 连接配置面板刷新
    conn_panel_->SetSystem(system_);
    conn_panel_->SetInstance(inst);

    // 参数表单刷新
    RebuildForm_(tpl, inst);
}

void SS_WidgetLightControllerParamPage::SlotRequestReady(const SS_LightParamSetRequest& req)
{
    if (!system_)
        return;

    SS_LightParamSetResult result;
    if (!system_->SetParameterAndSend(req, result))
    {
        QMessageBox::warning(this, tr("Fail to send"),//发送失败
            QStringLiteral("param=%1\n%2")
            .arg(ToQString(req.param_key))
            .arg(ToQString(result.message)));
        return;
    }

    std::string save_err;
    (void)system_->SaveInstanceById(req.instance_id, save_err);

    SS_LightControllerInstance inst;
    if (system_->GetInstance(req.instance_id, inst))
        builder_->RefreshValues(inst);
}

void SS_WidgetLightControllerParamPage::SlotConnectionUpdated(const std::string& instance_id)
{
    if (!system_)
        return;

    SS_LightControllerInstance inst;
    if (!system_->GetInstance(instance_id, inst))
        return;

    // 侧标题立即刷新
    RefreshTitle_(inst);

    // 顶部连接面板刷新（你原本就有）
    conn_panel_->SetInstance(inst);

    // 关键：告诉外面“这个控制器的信息（含 display_name）变了”
    emit SignalInstanceUpdated(ToQString(instance_id));
}

void SS_WidgetLightControllerParamPage::InitQSS_()
{
}

void SS_WidgetLightControllerParamPage::BuildUi_()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    title_ = new QLabel(this);
    title_->setStyleSheet("font-weight:700; font-size:16px;");
    root->addWidget(title_);

    // 顶部连接配置面板
    conn_panel_ = new SS_WidgetLightConnectionPanel(this);
    root->addWidget(conn_panel_);
    connect(conn_panel_, &SS_WidgetLightConnectionPanel::SignalConnectionUpdated, this, &SS_WidgetLightControllerParamPage::SlotConnectionUpdated);

    builder_ = new SS_LightParamFormBuilder(this);
    connect(builder_, &SS_LightParamFormBuilder::SignalRequestReady, this, &SS_WidgetLightControllerParamPage::SlotRequestReady);

    // 参数区容器
    params_container_ = new QWidget(this);
    params_layout_ = new QVBoxLayout(params_container_);
    params_layout_->setContentsMargins(0, 0, 0, 0);
    params_layout_->setSpacing(0);
    root->addWidget(params_container_, 1);

    InitQSS_();
}

void SS_WidgetLightControllerParamPage::InitConnections_()
{
}

void SS_WidgetLightControllerParamPage::RebuildForm_(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst)
{
    ClearParamsArea_();

    SS_LightParamFormBuilder::BuildArgs args;
    args.tpl = &tpl;
    args.inst = &inst;
    args.location = SS_LIGHT_PARAM_LOCATION::GLOBAL;

    form_holder_ = builder_->Build(args, params_container_);
    params_layout_->addWidget(form_holder_);
}

void SS_WidgetLightControllerParamPage::ClearParamsArea_()
{
    if (!params_layout_) return;

    while (QLayoutItem* it = params_layout_->takeAt(0))
    {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }

    form_holder_ = nullptr;
}

void SS_WidgetLightControllerParamPage::RefreshTitle_(const SS_LightControllerInstance& inst)
{
    title_->setText(tr("Controller parameter:") + ToQString(inst.info.display_name));//控制器参数：
}

