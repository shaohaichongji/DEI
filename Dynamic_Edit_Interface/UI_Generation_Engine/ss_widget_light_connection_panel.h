#pragma once
#include <QtWidgets/QWidget>
#include <QtCore/QString>

#include <string>

class QGroupBox;
class QComboBox;
class QStackedWidget;
class QPushButton;
class QLabel;

class QLineEdit;
class QSpinBox;
class QToolButton;

class SS_LightResourceSystem;
struct SS_LightControllerInstance;
struct SS_LightConnectionConfig;

class SS_WidgetLightConnectionPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightConnectionPanel(QWidget* parent = nullptr);
    ~SS_WidgetLightConnectionPanel() override = default;

    void SetSystem(SS_LightResourceSystem* system) { system_ = system; }
    void SetInstance(const SS_LightControllerInstance& inst); // 用 instance 来刷新显示

signals:
    void SignalConnectionUpdated(const std::string& instance_id);

private slots:
    void SlotConnectTypeChanged(int index);
    void SlotApplyClicked();
    void SlotConnectClicked();
    void SlotDisconnectClicked();
    void SlotRenameClicked();// 改名按钮

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();
    void SetUiFromConnection_(const SS_LightConnectionConfig& conn);
    void ReadUiToConnection_(SS_LightConnectionConfig& out_conn) const;

    void BuildBaudRateOptions_();
    void SelectBaudRate_(int baud);

private:
    SS_LightResourceSystem* system_ = nullptr;

    std::string instance_id_;
    std::string display_name_;

    QGroupBox* group_ = nullptr;

    // 顶部一行：显示名称 + 修改按钮
    QLabel* name_label_ = nullptr;
    QToolButton* rename_btn_ = nullptr;

    QComboBox* connect_type_combo_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    QPushButton* disconnect_btn_ = nullptr;

    // SERIAL/SOCKET 区域切换
    QStackedWidget* stack_ = nullptr;

    // serial page
    QWidget* serial_page_ = nullptr;
    QLineEdit* serial_com_ = nullptr;
    QComboBox* serial_baud_combo_ = nullptr;
    QSpinBox* serial_data_bits_ = nullptr;
    QSpinBox* serial_stop_bits_ = nullptr;
    QSpinBox* serial_parity_ = nullptr;

    // socket page (只保留目标IP/端口)
    QWidget* socket_page_ = nullptr;
    QLineEdit* socket_dest_ip_ = nullptr;
    QSpinBox* socket_dest_port_ = nullptr;
};
