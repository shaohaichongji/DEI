#include "ss_widget_light_controller_tree.h"
#include "moc/moc_ss_widget_light_controller_tree.cpp"

#include "ss_widget_light_controller_row.h"

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QHBoxLayout>

static const int kRoleType = Qt::UserRole + 1;
static const int kRoleInstanceId = Qt::UserRole + 2;
static const int kRoleChannelId = Qt::UserRole + 3;

enum class SS_TREE_ITEM_TYPE
{
    CONTROLLER = 1,
    CHANNEL = 2
};

SS_WidgetLightControllerTree::SS_WidgetLightControllerTree(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();

}

void SS_WidgetLightControllerTree::AddController(const std::string& instance_id,
    const std::string& display_name,
    bool connected)
{
    const QString qid = QString::fromStdString(instance_id);
    const QString title = QString::fromStdString(display_name);

    QTreeWidgetItem* item = FindControllerItem(qid);
    if (!item)
    {
        item = new QTreeWidgetItem();
        item->setData(0, kRoleType, (int)SS_TREE_ITEM_TYPE::CONTROLLER);
        item->setData(0, kRoleInstanceId, qid);
        item->setSizeHint(0, QSize(0, 26));

        tree_->addTopLevelItem(item);
        item->setExpanded(true);

        SS_WidgetLightControllerRow* row = new SS_WidgetLightControllerRow(tree_);
        row->SetInstanceId(qid);
        row->SetTitle(title);
        row->SetConnected(connected);
        row->SetAllowAddChannel(allow_add_channel_);

        connect(row, &SS_WidgetLightControllerRow::SignalAddChannelClicked,
            this, &SS_WidgetLightControllerTree::SlotAddChannelClicked);

        tree_->setItemWidget(item, 0, row);
    }
    else
    {
        QWidget* w = tree_->itemWidget(item, 0);
        auto* row = qobject_cast<SS_WidgetLightControllerRow*>(w);
        if (row)
        {
            row->SetTitle(title);
            row->SetConnected(connected);
        }
        else
        {
            item->setText(0, title);
        }
    }
}

void SS_WidgetLightControllerTree::AddChannel(const std::string& instance_id,
    const std::string& channel_id,
    const std::string& channel_display_name)
{
    const QString qinst = QString::fromStdString(instance_id);
    const QString qcid = QString::fromStdString(channel_id);
    const QString qname = QString::fromStdString(channel_display_name);

    QTreeWidgetItem* ctrl = FindControllerItem(qinst);
    if (!ctrl)
        return;

    // 防重复
    for (int i = 0; i < ctrl->childCount(); ++i)
    {
        QTreeWidgetItem* ch = ctrl->child(i);
        if (!ch) continue;
        if (ch->data(0, kRoleType).toInt() == (int)SS_TREE_ITEM_TYPE::CHANNEL &&
            ch->data(0, kRoleChannelId).toString() == qcid)
        {
            ch->setText(0, qname);
            return;
        }
    }

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, qname);
    item->setData(0, kRoleType, (int)SS_TREE_ITEM_TYPE::CHANNEL);
    item->setData(0, kRoleInstanceId, qinst);
    item->setData(0, kRoleChannelId, qcid);

    ctrl->addChild(item);
    ctrl->setExpanded(true);
}

void SS_WidgetLightControllerTree::Clear()
{
    tree_->clear();
}

bool SS_WidgetLightControllerTree::SelectControllerById(const QString& instance_id, bool emit_signal)
{
    if (instance_id.isEmpty())
        return false;

    QTreeWidgetItem* ctrl = FindControllerItem(instance_id);
    if (!ctrl)
        return false;

    tree_->setCurrentItem(ctrl);
    ctrl->setExpanded(true);

    if (emit_signal)
        emit SignalControllerSelected(instance_id);

    return true;
}

bool SS_WidgetLightControllerTree::SelectChannelById(
    const QString& instance_id,
    const QString& channel_id,
    bool emit_signal)
{
    if (instance_id.isEmpty() || channel_id.isEmpty())
        return false;

    QTreeWidgetItem* ctrl = FindControllerItem(instance_id);
    if (!ctrl)
        return false;

    for (int i = 0; i < ctrl->childCount(); ++i)
    {
        QTreeWidgetItem* ch = ctrl->child(i);
        if (!ch) continue;

        const int type = ch->data(0, kRoleType).toInt();
        const QString cid = ch->data(0, kRoleChannelId).toString();

        if (type == (int)SS_TREE_ITEM_TYPE::CHANNEL && cid == channel_id)
        {
            tree_->setCurrentItem(ch);
            ctrl->setExpanded(true);

            if (emit_signal)
                emit SignalChannelSelected(instance_id, channel_id);

            return true;
        }
    }

    return false;
}

bool SS_WidgetLightControllerTree::SetControllerConnected(const QString& instance_id, bool connected)
{
    if (instance_id.isEmpty())
        return false;

    QTreeWidgetItem* item = FindControllerItem(instance_id);
    if (!item)
        return false;

    QWidget* w = tree_->itemWidget(item, 0);
    auto* row = qobject_cast<SS_WidgetLightControllerRow*>(w);
    if (row)
    {
        row->SetConnected(connected);
        return true;
    }

    return false;
}

bool SS_WidgetLightControllerTree::UpdateControllerTitle(const QString& instance_id, const QString& display_name)
{
    QTreeWidgetItem* item = FindControllerItem(instance_id);
    if (!item) return false;

    QWidget* w = tree_->itemWidget(item, 0);
    auto* row = qobject_cast<SS_WidgetLightControllerRow*>(w);
    if (row)
    {
        row->SetTitle(display_name);
        return true;
    }

    item->setText(0, display_name);
    return true;
}

void SS_WidgetLightControllerTree::SetAllowAddController(bool allowed)
{
    allow_add_controller_ = allowed;
    if (add_btn_) add_btn_->setVisible(allowed);
}

void SS_WidgetLightControllerTree::SetAllowAddChannel(bool allowed)
{
    allow_add_channel_ = allowed;
}

void SS_WidgetLightControllerTree::SlotItemClicked(QTreeWidgetItem* item, int)
{
    if (!item) return;

    const int type = item->data(0, kRoleType).toInt();
    const QString instance_id = item->data(0, kRoleInstanceId).toString();

    if (type == (int)SS_TREE_ITEM_TYPE::CONTROLLER)
    {
        emit SignalControllerSelected(instance_id);
        return;
    }

    if (type == (int)SS_TREE_ITEM_TYPE::CHANNEL)
    {
        const QString channel_id = item->data(0, kRoleChannelId).toString();
        emit SignalChannelSelected(instance_id, channel_id);
        return;
    }
}

void SS_WidgetLightControllerTree::SlotAddClicked()
{
    emit SignalAddControllerClicked();
}

void SS_WidgetLightControllerTree::SlotAddChannelClicked(const QString& instance_id)
{
    if (instance_id.isEmpty())
        return;

    emit SignalAddChannelClicked(instance_id);
}

void SS_WidgetLightControllerTree::InitQSS_()
{
}

void SS_WidgetLightControllerTree::BuildUi_()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    QHBoxLayout* top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);

    add_btn_ = new QToolButton(this);
    add_btn_->setText("+");
    add_btn_->setToolTip(tr("Add a light source controller"));//添加光源控制器
    add_btn_->setFixedSize(24, 24);

    top->addStretch();
    top->addWidget(add_btn_);
    root->addLayout(top);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setIndentation(18);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setExpandsOnDoubleClick(true);
    tree_->header()->setStretchLastSection(true);

    root->addWidget(tree_);

    InitQSS_();
}

void SS_WidgetLightControllerTree::InitConnections_()
{
    connect(tree_, &QTreeWidget::itemClicked, this, &SS_WidgetLightControllerTree::SlotItemClicked);
    connect(add_btn_, &QToolButton::clicked, this, &SS_WidgetLightControllerTree::SlotAddClicked);
}

QTreeWidgetItem* SS_WidgetLightControllerTree::FindControllerItem(const QString& instance_id) const
{
    const int top_count = tree_->topLevelItemCount();
    for (int i = 0; i < top_count; ++i)
    {
        QTreeWidgetItem* it = tree_->topLevelItem(i);
        if (!it) continue;

        const int type = it->data(0, kRoleType).toInt();
        const QString id = it->data(0, kRoleInstanceId).toString();
        if (type == (int)SS_TREE_ITEM_TYPE::CONTROLLER && id == instance_id)
            return it;
    }
    return nullptr;
}
