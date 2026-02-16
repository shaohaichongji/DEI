#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QString>

#include <string>

class QTreeWidget;
class QToolButton;
class QTreeWidgetItem;

class SS_WidgetLightControllerTree : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightControllerTree(QWidget* parent = nullptr);
    ~SS_WidgetLightControllerTree() override = default;

    void AddController(const std::string& instance_id, const std::string& display_name, bool connected);
    void AddChannel(const std::string& instance_id, const std::string& channel_id, const std::string& channel_display_name);
    void Clear();

    // 外部按 instance_id 选中控制器（用于新增后自动定位、重命名后保持选中等）
    bool SelectControllerById(const QString& instance_id, bool emit_signal = true);

    // 外部按 instance_id + channel_id 选中通道（可选，用起来会很爽）
    bool SelectChannelById(const QString& instance_id, const QString& channel_id, bool emit_signal = true);

    // 仅更新控制器连接状态灯（不改标题、不重建 item）
    bool SetControllerConnected(const QString& instance_id, bool connected);

    bool UpdateControllerTitle(const QString& instance_id, const QString& display_name);

    void SetAllowAddController(bool allowed);
    void SetAllowAddChannel(bool allowed);

signals:
    void SignalControllerSelected(const QString& instance_id);
    void SignalChannelSelected(const QString& instance_id, const QString& channel_id);
    void SignalAddControllerClicked();
    void SignalAddChannelClicked(const QString& instance_id);

private slots:
    void SlotItemClicked(QTreeWidgetItem* item, int column);
    void SlotAddClicked();
    void SlotAddChannelClicked(const QString& instance_id); // 由每行 widget 转发过来

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    QTreeWidgetItem* FindControllerItem(const QString& instance_id) const;

private:
    QToolButton* add_btn_ = nullptr;
    QTreeWidget* tree_ = nullptr;

    bool allow_add_controller_ = true;
    bool allow_add_channel_ = true;
};
