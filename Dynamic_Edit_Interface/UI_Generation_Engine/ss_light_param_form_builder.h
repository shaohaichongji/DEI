#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <vector>
#include <string>

#include "ss_light_param_widget_factory.h"

// core forward
struct SS_LightControllerTemplate;
struct SS_LightControllerInstance;
struct SS_LightParamDef;

enum class SS_LIGHT_PARAM_LOCATION;
struct SS_LightParamSetRequest;

class QScrollArea;
class QWidget;
class QFormLayout;

class SS_LightParamFormBuilder : public QObject
{
    Q_OBJECT
public:
    struct BuildArgs
    {
        const SS_LightControllerTemplate* tpl = nullptr;
        const SS_LightControllerInstance* inst = nullptr;
        SS_LIGHT_PARAM_LOCATION location;     // GLOBAL / CHANNEL
        std::string channel_id;               // 仅 CHANNEL 用
    };

    explicit SS_LightParamFormBuilder(QObject* parent = nullptr);

    QWidget* Build(const BuildArgs& args, QWidget* parent);
    void RefreshValues(const SS_LightControllerInstance& inst);

signals:
    void SignalRequestReady(const SS_LightParamSetRequest& req);

private:
    struct Item
    {
        std::string instance_id;
        std::string param_key;
        SS_LIGHT_PARAM_LOCATION location;
        std::string channel_id; // 可空
        SS_LightWidgetHandle handle;
    };

    static std::string GetInitialValue_(
        const SS_LightControllerInstance& inst,
        const SS_LightParamDef& def,
        SS_LIGHT_PARAM_LOCATION loc,
        const std::string& channel_id);

private:
    std::vector<Item> items_;
    SS_LightParamWidgetFactory factory_;
};
