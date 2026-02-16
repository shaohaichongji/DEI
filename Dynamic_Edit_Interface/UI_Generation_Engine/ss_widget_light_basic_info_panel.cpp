#include "ss_widget_light_basic_info_panel.h"
#include "moc/moc_ss_widget_light_basic_info_panel.cpp"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>

#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

SS_WidgetLightBasicInfoPanel::SS_WidgetLightBasicInfoPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();
}

void SS_WidgetLightBasicInfoPanel::Clear()
{
    SetLine(factory_value_, "-");
    SetLine(model_value_, "-");
    SetLine(type_value_, "-");
    SetLine(channel_max_value_, "-");
    SetLine(connect_value_, "-");
    SetLine(protocol_value_, "-");
}

void SS_WidgetLightBasicInfoPanel::SetInfo(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst)
{
    SetLine(factory_value_, QString::fromStdString(tpl.info.factory));
    SetLine(model_value_, QString::fromStdString(tpl.info.controller_model));

    // 优先显示 display_name（模板里预留了）
    QString type_text = QString::fromStdString(tpl.info.controller_type_display_name);
    if (type_text.trimmed().isEmpty())
        type_text = QString::fromStdString(tpl.info.controller_type);
    SetLine(type_value_, type_text);

    SetLine(channel_max_value_, QString::number(tpl.info.channel_max));
    SetLine(connect_value_, ConnectTypeToText(inst.connection.connect_type));
    SetLine(protocol_value_, ProtocolTypeToText(tpl.info.protocol_type));
}

void SS_WidgetLightBasicInfoPanel::InitQSS_()
{
}

void SS_WidgetLightBasicInfoPanel::BuildUi_()
{
    QGridLayout* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    int r = 0;

    auto add_row = [&](const QString& name, QLabel*& value_label)
    {
        QLabel* k = new QLabel(name, this);
        value_label = new QLabel("-", this);
        value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

        grid->addWidget(k, r, 0);
        grid->addWidget(value_label, r, 1);
        ++r;
    };

    add_row(tr("Factory"), factory_value_);// 厂家
    add_row(tr("Model"), model_value_);// 型号
    add_row(tr("Controller type"), type_value_);// 控制器类型
    add_row(tr("Max channel number"), channel_max_value_);// 最大通道数
    add_row(tr("Communication mode"), connect_value_);// 通讯方式
    add_row(tr("Protocol type"), protocol_value_);// 协议类型

    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);

    InitQSS_();
}

void SS_WidgetLightBasicInfoPanel::InitConnections_()
{
}

void SS_WidgetLightBasicInfoPanel::SetLine(QLabel* value_label, const QString& value)
{
    if (!value_label) return;
    value_label->setText(value.isEmpty() ? "-" : value);
}

QString SS_WidgetLightBasicInfoPanel::ConnectTypeToText(SS_LIGHT_CONNECT_TYPE t)
{
    switch (t)
    {
    case SS_LIGHT_CONNECT_TYPE::SERIAL: return tr("Serial port");// 串口
    case SS_LIGHT_CONNECT_TYPE::SOCKET: return tr("Network port");// 网口
    default: return tr("Unknown");// 未知
    }
}

QString SS_WidgetLightBasicInfoPanel::ProtocolTypeToText(SS_LIGHT_PROTOCOL_TYPE t)
{
    switch (t)
    {
    case SS_LIGHT_PROTOCOL_TYPE::STRING: return tr("String Protocol");// 字符串协议
    case SS_LIGHT_PROTOCOL_TYPE::BYTE: return tr("Byte Protocol");// 字节协议
    default: return tr("Unknown");// 未知
    }
}