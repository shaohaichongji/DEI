#include <QtWidgets/QApplication>
#include "../../include/UI_Generation_Engine/ss_widget_light_resource.h"

# ifdef _DEBUG
#pragma comment(lib, "../../lib/Debug/UI_Generation_Engine.lib")
# else
#pragma comment(lib, "../../lib/Release/UI_Generation_Engine.lib")
# endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    WidgetLightResource w;
    //w.show();

    if (w.InitCore())
    {

    }

    return app.exec();
}
