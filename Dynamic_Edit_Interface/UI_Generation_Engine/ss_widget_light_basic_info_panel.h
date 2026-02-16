#pragma once
#include <QtWidgets/QWidget>

class QLabel;

struct SS_LightControllerTemplate;
struct SS_LightControllerInstance;
enum class SS_LIGHT_CONNECT_TYPE;
enum class SS_LIGHT_PROTOCOL_TYPE;

class SS_WidgetLightBasicInfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightBasicInfoPanel(QWidget* parent = nullptr);
    ~SS_WidgetLightBasicInfoPanel() override = default;

    void Clear();
    void SetInfo(const SS_LightControllerTemplate& tpl, const SS_LightControllerInstance& inst);

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    void SetLine(QLabel* value_label, const QString& value);

    QString ConnectTypeToText(SS_LIGHT_CONNECT_TYPE t);
    QString ProtocolTypeToText(SS_LIGHT_PROTOCOL_TYPE t);

private:
    QLabel* factory_value_ = nullptr;
    QLabel* model_value_ = nullptr;
    QLabel* type_value_ = nullptr;
    QLabel* channel_max_value_ = nullptr;
    QLabel* connect_value_ = nullptr;
    QLabel* protocol_value_ = nullptr;
};
