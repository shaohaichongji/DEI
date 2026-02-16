#pragma once
#include <QtWidgets/QWidget>
#include <string>

struct SS_LightControllerTemplate;
struct SS_LightControllerInstance;
struct SS_LightParamSetRequest;

class QLabel;
class QWidget;
class QVBoxLayout;

class SS_LightResourceSystem;

class SS_LightParamFormBuilder;
class SS_WidgetLightConnectionPanel;

class SS_WidgetLightControllerParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightControllerParamPage(QWidget* parent = nullptr);
    ~SS_WidgetLightControllerParamPage() override = default;

    void SetSystem(SS_LightResourceSystem* system) { system_ = system; }
    void SetData(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst);

signals:
    // 通知外面（Host/Main）去刷新左侧树、其它展示
    void SignalInstanceUpdated(const QString& instance_id);

private slots:
    void SlotRequestReady(const SS_LightParamSetRequest& req);
    void SlotConnectionUpdated(const std::string& instance_id);

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    void RebuildForm_(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst);
    void ClearParamsArea_();
    void RefreshTitle_(const SS_LightControllerInstance& inst);

private:
    QLabel* title_ = nullptr;

    // 顶部：连接配置面板
    SS_WidgetLightConnectionPanel* conn_panel_ = nullptr;

    // 参数区域（专门放 builder 生成的 scroll）
    QWidget* params_container_ = nullptr;
    QVBoxLayout* params_layout_ = nullptr;
    QWidget* form_holder_ = nullptr;

    SS_LightParamFormBuilder* builder_ = nullptr;

    SS_LightResourceSystem* system_ = nullptr;
    std::string instance_id_;
};
