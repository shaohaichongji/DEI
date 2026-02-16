#include "ss_widget_light_param_host.h"
#include "moc/moc_ss_widget_light_param_host.cpp"

#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include "ss_widget_light_controller_param_page.h"
#include "ss_widget_light_channel_param_page.h"

#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_api.h"

SS_WidgetLightParamHost::SS_WidgetLightParamHost(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();
}

void SS_WidgetLightParamHost::Clear()
{
    for (auto& kv : pages_)
    {
        QWidget* w = kv.second;
        if (w)
        {
            stacked_->removeWidget(w);
            w->deleteLater();
        }
    }
    pages_.clear();
}

void SS_WidgetLightParamHost::ShowControllerParams(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst)
{
    const std::string key = MakeControllerKey(inst.info.instance_id);

    SS_WidgetLightControllerParamPage* page = nullptr;

    auto it = pages_.find(key);
    if (it != pages_.end())
    {
        page = qobject_cast<SS_WidgetLightControllerParamPage*>(it->second);
    }
    else
    {
        page = new SS_WidgetLightControllerParamPage(this);
        page->SetSystem(system_);
        pages_[key] = page;
        stacked_->addWidget(page);

        // 只连一次，转发 instance 更新
        connect(page, &SS_WidgetLightControllerParamPage::SignalInstanceUpdated,
            this, &SS_WidgetLightParamHost::SignalInstanceUpdated,
            Qt::UniqueConnection);
    }

    if (page)
        page->SetData(tpl, inst);

    stacked_->setCurrentWidget(page);
}

void SS_WidgetLightParamHost::ShowChannelParams(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst,
    const std::string& channel_id)
{
    const std::string key = MakeChannelKey(inst.info.instance_id, channel_id);

    SS_WidgetLightChannelParamPage* page = nullptr;

    auto it = pages_.find(key);
    if (it != pages_.end())
    {
        page = qobject_cast<SS_WidgetLightChannelParamPage*>(it->second);
    }
    else
    {
        page = new SS_WidgetLightChannelParamPage(this);
        page->SetSystem(system_);
        pages_[key] = page;
        stacked_->addWidget(page);

        // 只连一次，转发通道更新
        connect(page, &SS_WidgetLightChannelParamPage::SignalChannelUpdated,
            this, &SS_WidgetLightParamHost::SignalChannelUpdated,
            Qt::UniqueConnection);
    }

    if (page)
        page->SetData(tpl, inst, channel_id);

    stacked_->setCurrentWidget(page);
}

void SS_WidgetLightParamHost::InitQSS_()
{
}

void SS_WidgetLightParamHost::BuildUi_()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    stacked_ = new QStackedWidget(this);
    root->addWidget(stacked_);

    InitQSS_();
}

void SS_WidgetLightParamHost::InitConnections_()
{
}

std::string SS_WidgetLightParamHost::MakeControllerKey(const std::string& instance_id)
{
    return "ctrl::" + instance_id;
}

std::string SS_WidgetLightParamHost::MakeChannelKey(const std::string& instance_id, const std::string& channel_id)
{
    return "ch::" + instance_id + "::" + channel_id;
}
