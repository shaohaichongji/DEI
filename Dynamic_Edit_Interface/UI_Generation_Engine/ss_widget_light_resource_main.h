#pragma once
#include <QtWidgets/QWidget>
#include <QtCore/QString>

#include "../../include/Parsing_Engine/ss_light_resource_event_bus.h"
#include "../../include/Parsing_Engine/ss_light_resource_events.h"

class QGroupBox;
class QSplitter;

class SS_WidgetLightControllerTree;
class SS_WidgetLightBasicInfoPanel;
class SS_WidgetLightParamHost;

class SS_LightResourceSystem;

class SS_WidgetLightResourceMain : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightResourceMain(QWidget* parent = nullptr);
    ~SS_WidgetLightResourceMain() override = default;

    // 绑定 core 系统入口（UI 不负责 Init/Shutdown，只消费）
    void SetSystem(SS_LightResourceSystem* system);

    // 刷新左侧树（从 core 拉实例列表）
    bool RefreshControllers(QString& out_error);

private slots:
    void SlotControllerSelected(const QString& instance_id);
    void SlotChannelSelected(const QString& instance_id, const QString& channel_id);
    void SlotAddControllerClicked();
    void SlotAddChannelClicked(const QString& instance_id);
    void SlotInstanceUpdated(const QString& instance_id);
    void SlotChannelUpdated(const QString& instance_id, const QString& channel_id);

private:
    void InitQSS();
    void BuildUi();
    void InitConnections();
    void ClearUiState();

    bool ShowController(const QString& instance_id, QString& out_error);
    bool ShowChannel(const QString& instance_id, const QString& channel_id, QString& out_error);

    // event bus -> UI thread dispatcher
    void OnLightEventUiThread_(const SS_LightEvent& ev);

    // variant handlers
    void HandleEvent_(const SS_LightEventBase&) {} // ignore
    void HandleEvent_(const SS_LightEventConnect& e);
    void HandleEvent_(const SS_LightEventError& e);
    void HandleEvent_(const SS_LightEventFrame& e);

private:
    SS_LightResourceSystem* system_ = nullptr;

    QSplitter* main_splitter_ = nullptr;

    QGroupBox* group_list_ = nullptr;
    QGroupBox* group_basic_info_ = nullptr;
    QGroupBox* group_params_ = nullptr;

    SS_WidgetLightControllerTree* controller_tree_ = nullptr;
    SS_WidgetLightBasicInfoPanel* basic_info_panel_ = nullptr;
    SS_WidgetLightParamHost* param_host_ = nullptr;

    QString current_instance_id_;
    QString current_channel_id_;

    SS_LightEventBus::Subscription light_bus_sub_;
};
