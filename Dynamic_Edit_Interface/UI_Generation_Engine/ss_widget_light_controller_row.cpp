#include "ss_widget_light_controller_row.h"
#include "moc/moc_ss_widget_light_controller_row.cpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>

SS_WidgetLightControllerRow::SS_WidgetLightControllerRow(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();


}

void SS_WidgetLightControllerRow::SetTitle(const QString& title)
{
    title_label_->setText(title);
}

void SS_WidgetLightControllerRow::SetConnected(bool connected)
{
    status_label_->setText(connected ? QStringLiteral("●") : QStringLiteral("○"));
}

void SS_WidgetLightControllerRow::SetInstanceId(const QString& instance_id)
{
    instance_id_ = instance_id;
}

void SS_WidgetLightControllerRow::SetAllowAddChannel(bool allowed)
{
    if (add_btn_)
        add_btn_->setVisible(allowed);
}

void SS_WidgetLightControllerRow::InitQSS_()
{
}

void SS_WidgetLightControllerRow::BuildUi_()
{
    QHBoxLayout* lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 0, 4, 0);
    lay->setSpacing(6);

    status_label_ = new QLabel(this);
    status_label_->setFixedWidth(14);
    status_label_->setText(QStringLiteral("○"));

    title_label_ = new QLabel(this);
    title_label_->setText(QStringLiteral(""));

    add_btn_ = new QToolButton(this);
    add_btn_->setText("+");
    add_btn_->setToolTip(tr("Add channel"));//添加通道
    add_btn_->setFixedSize(18, 18);

    lay->addWidget(status_label_);
    lay->addWidget(title_label_, 1);
    lay->addWidget(add_btn_);

    InitQSS_();
}

void SS_WidgetLightControllerRow::InitConnections_()
{
    connect(add_btn_, &QToolButton::clicked, this, &SS_WidgetLightControllerRow::SlotAddClicked);
}

void SS_WidgetLightControllerRow::SlotAddClicked()
{
    emit SignalAddChannelClicked(instance_id_);
}
