#include "ss_widget_light_channel_param_page.h"
#include "moc/moc_ss_widget_light_channel_param_page.cpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QInputDialog>

#include "ss_light_param_form_builder.h"

#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_api.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

static QString ToQString(const std::string& s) { return QString::fromStdString(s); }
static std::string ToStdString(const QString& s) { return s.toStdString(); }

SS_WidgetLightChannelParamPage::SS_WidgetLightChannelParamPage(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();
}

void SS_WidgetLightChannelParamPage::SetData(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst,
    const std::string& channel_id)
{
    instance_id_ = inst.info.instance_id;
    channel_id_ = channel_id;

    // title: 控制器名 / 通道名（更符合直觉）
    title_->setText(tr("Channel parameters:")//通道参数：
        + ToQString(inst.info.display_name)
        + QStringLiteral(" / ")
        + ToQString(GetCurrentChannelDisplayName_(inst)));

    RefreshTopPanel_(tpl, inst);
    RebuildForm_(tpl, inst);
}

void SS_WidgetLightChannelParamPage::SlotRequestReady(const SS_LightParamSetRequest& req)
{
    if (!system_)
        return;

    SS_LightParamSetResult result;
    if (!system_->SetParameterAndSend(req, result))
    {
        QMessageBox::warning(this, tr("Fail to send"),// 发送失败
            QStringLiteral("param=%1\nchannel=%2\n%3")
            .arg(ToQString(req.param_key))
            .arg(ToQString(req.channel_id))
            .arg(ToQString(result.message)));
        return;
    }

    std::string save_err;
    (void)system_->SaveInstanceById(req.instance_id, save_err);

    SS_LightControllerInstance inst;
    if (system_->GetInstance(req.instance_id, inst))
        builder_->RefreshValues(inst);
}

void SS_WidgetLightChannelParamPage::SlotRenameChannelClicked()
{
    if (!system_) return;

    SS_LightControllerInstance inst;
    if (!system_->GetInstance(instance_id_, inst))
        return;

    const QString cur = ToQString(GetCurrentChannelDisplayName_(inst));

    bool ok = false;
    QString text = QInputDialog::getText(this,
        tr("Change controller name"),//修改光源名称
        tr("Please enter the name of the new light source:"),// 请输入新的光源名称：
        QLineEdit::Normal,
        cur,
        &ok);

    if (!ok) return;

    const std::string new_name = ToStdString(text.trimmed());
    if (new_name.empty())
        return;

    // 注意：这里改的是通道名，不是控制器名
    if (!system_->RenameChannel(instance_id_, channel_id_, new_name))
    {
        QMessageBox::warning(this, tr("Change failed"),//修改失败
            tr("RenameChannel failed"));//RenameChannel 失败
        return;
    }

    std::string save_err;
    if (!system_->SaveInstanceById(instance_id_, save_err))
    {
        QMessageBox::warning(this, tr("Save to failed"),//保存失败
            QString::fromStdString(save_err));
        return;
    }

    // 刷新本页显示
    if (system_->GetInstance(instance_id_, inst))
    {
        channel_name_value_->setText(ToQString(GetCurrentChannelDisplayName_(inst)));
        title_->setText(tr("Channel parameters:")//通道参数：
            + ToQString(inst.info.display_name)
            + QStringLiteral(" / ")
            + ToQString(GetCurrentChannelDisplayName_(inst)));
    }

    // 通知主界面刷新左侧树该通道名称
    emit SignalChannelUpdated(ToQString(instance_id_), ToQString(channel_id_));
}

void SS_WidgetLightChannelParamPage::SlotChannelIndexChanged(int /*index*/)
{
    if (!system_) return;
    if (channel_max_ <= 0) return;
    if (instance_id_.empty() || channel_id_.empty()) return;

    const int selected_1based = channel_combo_->currentData().toInt();
    const int new_index0 = selected_1based - 1;

    if (new_index0 == last_channel_index0_)
        return;

    std::string err;
    if (!system_->SetChannelIndex(instance_id_, channel_id_, new_index0, err))
    {
        // 回滚 UI
        channel_combo_->blockSignals(true);
        const int rollback_1based = last_channel_index0_ + 1;
        int ui_idx = channel_combo_->findData(rollback_1based);
        if (ui_idx >= 0) channel_combo_->setCurrentIndex(ui_idx);
        channel_combo_->blockSignals(false);

        QMessageBox::warning(this, tr("Modification of the channel failed"),//修改通道失败
            QString::fromStdString(err));
        return;
    }

    // 保存
    std::string save_err;
    if (!system_->SaveInstanceById(instance_id_, save_err))
    {
        // 保存失败：回滚 index + 回滚 UI
        std::string err2;
        (void)system_->SetChannelIndex(instance_id_, channel_id_, last_channel_index0_, err2);

        channel_combo_->blockSignals(true);
        const int rollback_1based = last_channel_index0_ + 1;
        int ui_idx = channel_combo_->findData(rollback_1based);
        if (ui_idx >= 0) channel_combo_->setCurrentIndex(ui_idx);
        channel_combo_->blockSignals(false);

        QMessageBox::warning(this, tr("Save to failed"),//保存失败
            QString::fromStdString(save_err));
        return;
    }

    last_channel_index0_ = new_index0;

    // 这里也通知一下主界面（未来想在树上显示 index 的话就用得上）
    emit SignalChannelUpdated(ToQString(instance_id_), ToQString(channel_id_));

    SS_LightControllerInstance inst;
    if (system_->GetInstance(instance_id_, inst))
        builder_->RefreshValues(inst);
}

void SS_WidgetLightChannelParamPage::InitQSS_()
{
}

void SS_WidgetLightChannelParamPage::BuildUi_()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // title
    title_ = new QLabel(this);
    title_->setStyleSheet("font-weight:700; font-size:16px;");
    root->addWidget(title_);

    // ---------- row1: channel name ----------
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);

        QLabel* lab = new QLabel(tr("Light source name:"), this); // 光源名称：
        channel_name_value_ = new QLabel(this);
        channel_name_value_->setTextInteractionFlags(Qt::TextSelectableByMouse);

        rename_channel_btn_ = new QToolButton(this);
        rename_channel_btn_->setText(tr("Modify"));//修改
        rename_channel_btn_->setToolTip(tr("Change the name of the current channel"));//修改当前通道的名称

        row->addWidget(lab);
        row->addWidget(channel_name_value_, 1);
        row->addWidget(rename_channel_btn_);

        root->addLayout(row);

        connect(rename_channel_btn_, &QToolButton::clicked,
            this, &SS_WidgetLightChannelParamPage::SlotRenameChannelClicked);
    }

    // ---------- row2: channel index combo ----------
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);

        QLabel* lab = new QLabel(tr("Current channel:"), this);// 当前通道：

        channel_combo_ = new QComboBox(this);
        channel_combo_->setToolTip(tr("Modify the index corresponding to the current channel (1..maximum number of channels)"));// 修改当前通道对应的index（1..最大通道数）

        row->addWidget(lab);
        row->addWidget(channel_combo_, 1);

        root->addLayout(row);

        connect(channel_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SS_WidgetLightChannelParamPage::SlotChannelIndexChanged);
    }

    // builder
    builder_ = new SS_LightParamFormBuilder(this);
    connect(builder_, &SS_LightParamFormBuilder::SignalRequestReady, this, &SS_WidgetLightChannelParamPage::SlotRequestReady);

    // placeholder
    form_holder_ = new QWidget(this);
    root->addWidget(form_holder_, 1);

    InitQSS_();
}

void SS_WidgetLightChannelParamPage::InitConnections_()
{
}

void SS_WidgetLightChannelParamPage::RebuildForm_(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst)
{
    if (form_holder_)
    {
        if (auto* lay = qobject_cast<QVBoxLayout*>(layout()))
            lay->removeWidget(form_holder_);
        form_holder_->deleteLater();
        form_holder_ = nullptr;
    }

    SS_LightParamFormBuilder::BuildArgs args;
    args.tpl = &tpl;
    args.inst = &inst;
    args.location = SS_LIGHT_PARAM_LOCATION::CHANNEL;
    args.channel_id = channel_id_;

    form_holder_ = builder_->Build(args, this);
    if (auto* lay = qobject_cast<QVBoxLayout*>(layout()))
        lay->addWidget(form_holder_, 1);
}

void SS_WidgetLightChannelParamPage::RefreshTopPanel_(const SS_LightControllerTemplate& tpl,
    const SS_LightControllerInstance& inst)
{
    // channel name (display_name)
    channel_name_value_->setText(ToQString(GetCurrentChannelDisplayName_(inst)));

    // channel_max
    channel_max_ = tpl.info.channel_max;
    if (channel_max_ <= 0) channel_max_ = 0;

    // rebuild combo 1..channel_max
    channel_combo_->blockSignals(true);
    channel_combo_->clear();
    for (int i = 1; i <= channel_max_; ++i)
        channel_combo_->addItem(QString::number(i), i); // data = 1-based
    channel_combo_->blockSignals(false);

    // select current channel index
    last_channel_index0_ = GetCurrentChannelIndex0_(inst);
    const int want_1based = last_channel_index0_ + 1;

    int ui_index = channel_combo_->findData(want_1based);
    if (ui_index < 0) ui_index = (want_1based > 0 ? want_1based - 1 : 0);

    channel_combo_->blockSignals(true);
    if (ui_index >= 0 && ui_index < channel_combo_->count())
        channel_combo_->setCurrentIndex(ui_index);
    channel_combo_->blockSignals(false);
}

int SS_WidgetLightChannelParamPage::GetCurrentChannelIndex0_(const SS_LightControllerInstance& inst) const
{
    for (const auto& ch : inst.channels)
    {
        if (ch.channel_id == channel_id_)
            return ch.index;
    }
    return 0;
}

std::string SS_WidgetLightChannelParamPage::GetCurrentChannelDisplayName_(const SS_LightControllerInstance& inst) const
{
    for (const auto& ch : inst.channels)
    {
        if (ch.channel_id == channel_id_)
            return ch.display_name;
    }
    return channel_id_; // fallback
}

