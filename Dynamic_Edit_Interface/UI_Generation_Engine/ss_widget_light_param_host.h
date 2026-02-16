#pragma once
#include <QtWidgets/QWidget>

#include <unordered_map>
#include <string>

class QStackedWidget;

// forward declare core api
class SS_LightResourceSystem;

struct SS_LightControllerTemplate;
struct SS_LightControllerInstance;

class SS_WidgetLightControllerParamPage;
class SS_WidgetLightChannelParamPage;

class SS_WidgetLightParamHost : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightParamHost(QWidget* parent = nullptr);
    ~SS_WidgetLightParamHost() override = default;

    void SetSystem(SS_LightResourceSystem* system) { system_ = system; }

    void Clear();

    void ShowControllerParams(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst);
    void ShowChannelParams(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst, const std::string& channel_id);

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    static std::string MakeControllerKey(const std::string& instance_id);
    static std::string MakeChannelKey(const std::string& instance_id, const std::string& channel_id);

signals:
    // 控制器信息更新（display_name/连接参数等）
    void SignalInstanceUpdated(const QString& instance_id);

    //void SignalInstanceUpdated(const QString& instance_id);
    void SignalChannelUpdated(const QString& instance_id, const QString& channel_id);

private:
    SS_LightResourceSystem* system_ = nullptr;  // Page 需要它发命令

    QStackedWidget* stacked_ = nullptr;

    // key -> page widget
    std::unordered_map<std::string, QWidget*> pages_;
};