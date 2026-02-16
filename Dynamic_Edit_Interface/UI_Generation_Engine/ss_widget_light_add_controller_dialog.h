// ss_widget_light_add_controller_dialog.h
#pragma once
#include <QtWidgets/QDialog>
#include <QtCore/QString>

#include <string>

#include "ss_light_template_leaf_index.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QStackedWidget;
class QLabel;

class SS_LightResourceSystem;

class SS_WidgetLightAddControllerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SS_WidgetLightAddControllerDialog(QWidget* parent = nullptr);
    ~SS_WidgetLightAddControllerDialog() override = default;

    void SetSystem(SS_LightResourceSystem* system);

    // 创建成功后可取到
    std::string GetCreatedInstanceId() const { return created_instance_id_; }

    // 暂存用户选择的连接配置（等补 core 接口后，再把它写回 instance）
    SS_LIGHT_CONNECT_TYPE GetSelectedConnectType() const { return selected_connect_type_; }
    std::string GetSelectedComPort() const { return selected_com_port_; }
    int GetSelectedBaudRate() const { return selected_baud_rate_; }
    std::string GetSelectedDestIp() const { return selected_dest_ip_; }
    int GetSelectedDestPort() const { return selected_dest_port_; }

signals:
    void SignalControllerCreated(const QString& instance_id);

private slots:
    void SlotFactoryChanged(int);
    void SlotTypeChanged(int);
    void SlotModelChanged(int);
    void SlotChannelMaxChanged(int);
    void SlotConnectTypeChanged(int);
    void SlotAddClicked();

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    bool InitIndex_(QString& out_error);

    void RefreshFactory_();
    void RefreshType_();
    void RefreshModel_();
    void RefreshChannelMax_();
    void RefreshConnectType_();
    void RefreshConnectParamPanel_();

    static QString ConnectTypeToDisplay_(SS_LIGHT_CONNECT_TYPE t);
    static SS_LIGHT_CONNECT_TYPE DisplayToConnectType_(const QString& s);

    static std::string ToStdString_(const QString& s) { return s.toStdString(); }
    static QString ToQString_(const std::string& s) { return QString::fromStdString(s); }

private:
    SS_LightResourceSystem* system_ = nullptr;
    SS_LightTemplateLeafIndex index_;

    // UI widgets
    QLineEdit* name_edit_ = nullptr;

    QComboBox* factory_combo_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QComboBox* model_combo_ = nullptr;
    QComboBox* channel_combo_ = nullptr;
    QComboBox* connect_combo_ = nullptr;

    QStackedWidget* connect_stack_ = nullptr;

    // SERIAL page
    QComboBox* com_combo_ = nullptr;
    QComboBox* baud_combo_ = nullptr;

    // SOCKET page
    QLineEdit* ip_edit_ = nullptr;
    QSpinBox* port_spin_ = nullptr;

    //QLabel* hint_label_ = nullptr;
    QPushButton* add_btn_ = nullptr;

    // result
    std::string created_instance_id_;

    // selected connect params cache
    SS_LIGHT_CONNECT_TYPE selected_connect_type_ = SS_LIGHT_CONNECT_TYPE::UNKNOWN;
    std::string selected_com_port_;
    int selected_baud_rate_ = 0;
    std::string selected_dest_ip_;
    int selected_dest_port_ = 0;
};
