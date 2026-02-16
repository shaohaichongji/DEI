#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <functional>
#include <string>

struct SS_LightParamDef;

class QWidget;

class SS_LightWidgetChangeEmitter : public QObject
{
    Q_OBJECT
public:
    explicit SS_LightWidgetChangeEmitter(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void SignalChanged();
};

struct SS_LightWidgetHandle
{
    QWidget* widget = nullptr;

    std::function<std::string()> get_value;
    std::function<void(const std::string&)> set_value;

    SS_LightWidgetChangeEmitter* emitter = nullptr;
};

class SS_LightParamWidgetFactory
{
public:
    SS_LightWidgetHandle Create(const SS_LightParamDef& def, QWidget* parent);
};
