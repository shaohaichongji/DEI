#pragma once
#include <QtWidgets/QWidget>
#include <QtCore/QString>

#include <string>

struct SS_LightControllerTemplate;
struct SS_LightControllerInstance;

class QLabel;
class QWidget;
class QComboBox;
class QToolButton;

class SS_LightResourceSystem;
struct SS_LightParamSetRequest;

class SS_LightParamFormBuilder;

class SS_WidgetLightChannelParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightChannelParamPage(QWidget* parent = nullptr);
    ~SS_WidgetLightChannelParamPage() override = default;

    void SetData(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst, const std::string& channel_id);
    void SetSystem(SS_LightResourceSystem* system) { system_ = system; }

signals:
    // 通知主界面刷新左侧树：更新某个通道的显示名/信息
    void SignalChannelUpdated(const QString& instance_id, const QString& channel_id);

private slots:
    void SlotRequestReady(const SS_LightParamSetRequest& req);
    void SlotRenameChannelClicked();
    void SlotChannelIndexChanged(int index);

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    void RebuildForm_(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst);
    void RefreshTopPanel_(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst);
    int GetCurrentChannelIndex0_(const SS_LightControllerInstance& inst) const;
    std::string GetCurrentChannelDisplayName_(const SS_LightControllerInstance& inst) const;

private:
    // ---- top panel widgets ----
    QLabel* title_ = nullptr;

    QLabel* channel_name_value_ = nullptr;
    QToolButton* rename_channel_btn_ = nullptr;

    QComboBox* channel_combo_ = nullptr;
    int last_channel_index0_ = 0; // 0-based

    // ---- form ----
    QWidget* form_holder_ = nullptr;
    SS_LightParamFormBuilder* builder_ = nullptr;

    // ---- state ----
    SS_LightResourceSystem* system_ = nullptr;
    std::string instance_id_;
    std::string channel_id_;
    int channel_max_ = 0;
};
