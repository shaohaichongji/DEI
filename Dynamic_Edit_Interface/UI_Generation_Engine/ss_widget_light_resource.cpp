#include "ss_widget_light_resource.h"
#include "moc/moc_ss_widget_light_resource.cpp"

#include "../../include/Parsing_Engine/ss_light_resource_api.h"

# ifdef _DEBUG
#pragma comment(lib, "../../lib/Debug/Parsing_Engine.lib")
# else
#pragma comment(lib, "../../lib/Release/Parsing_Engine.lib")
# endif

#include "ss_widget_light_resource_main.h"

#include <QtWidgets/QApplication>
#include <QtCore/QDir>

WidgetLightResource::WidgetLightResource()
{
    BuildUI();
    InitConnections();

    ss_widget_light_resource_main_ = new SS_WidgetLightResourceMain();

    //QString err;
    ss_widget_light_resource_main_->SetSystem(SS_LightResourceSystem::GetInstance());
    //ss_widget_light_resource_main_->RefreshControllers(err);
}

WidgetLightResource::~WidgetLightResource()
{

}

void WidgetLightResource::ReRtanslation()
{

}

void WidgetLightResource::ReLoadTheme()
{

}

void WidgetLightResource::InitQSS()
{
}

void WidgetLightResource::BuildUI()
{
    InitQSS();
}

void WidgetLightResource::InitConnections()
{
}

bool WidgetLightResource::InitCore()
{
    // 初始化光源控制器模块core
    SS_LightResourceSystem* ss = SS_LightResourceSystem::GetInstance();
    std::string out_error;
    QString config_path = QDir(QApplication::applicationDirPath()).filePath("Config/Templates_Dir");
    QString instance_path = QDir(QApplication::applicationDirPath()).filePath("Config/Instances_Dir");

    QDir dir;
    if (!dir.exists(config_path))
    {
        if (!dir.mkpath(config_path))
        {
            return false;
        }
    }

    QDir dir_instance;
    if (!dir_instance.exists(instance_path))
    {
        if (!dir_instance.mkpath(instance_path))
        {
            return false;
        }
    }

    std::string template_dir = config_path.toStdString();
    std::string instance_dir = instance_path.toStdString();
    bool res = ss->Init(template_dir, instance_dir, out_error);
    if (!res)
    {
        // TODO: 通知或弹窗
    }
    QString err;
    ss_widget_light_resource_main_->RefreshControllers(err);
    ss_widget_light_resource_main_->show();
    return res;
}


QWidget* WidgetLightResource::GetWidget(int index)
{
    return ss_widget_light_resource_main_;
}


