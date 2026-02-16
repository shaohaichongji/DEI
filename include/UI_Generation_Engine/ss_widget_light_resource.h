#pragma once

#include <QtWidgets/QWidget>

#ifdef SS_LightResourceWidgetExport
#define SS_LIGHT_RESOURCE_UI __declspec(dllexport)
#else
#define SS_LIGHT_RESOURCE_UI __declspec(dllimport)
#endif

namespace SS {
    struct UI_Protocol;
}
class SS_WidgetLightResourceMain;
class SS_WidgetLightResourceViewerMain;
class SS_LIGHT_RESOURCE_UI WidgetLightResource : public QWidget
{
    Q_OBJECT
public:
    WidgetLightResource();
    ~WidgetLightResource();
    void ReRtanslation();
    void ReLoadTheme();

    void InitQSS();
    void BuildUI();
    void InitConnections();
    bool InitCore();

    QWidget* GetWidget(int index);

signals:
    void SignalSendProtocol(SS::UI_Protocol* protocol);

private:

private:
    SS_WidgetLightResourceMain* ss_widget_light_resource_main_ = nullptr;
    SS_WidgetLightResourceViewerMain* ss_widget_light_resource_viewer_main_ = nullptr;
};