#include "ss_widget_light_connection_panel.h"
#include "moc/moc_ss_widget_light_connection_panel.cpp"

#include <QtWidgets/QGroupBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QInputDialog>

#include "../../include/Parsing_Engine/ss_light_resource_api.h"
#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

static QString ToQString(const std::string& s) { return QString::fromStdString(s); }
static std::string ToStdString(const QString& s) { return s.toStdString(); }

SS_WidgetLightConnectionPanel::SS_WidgetLightConnectionPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi_();
    InitConnections_();
}

void SS_WidgetLightConnectionPanel::SetInstance(const SS_LightControllerInstance& inst)
{
    instance_id_ = inst.info.instance_id;
    display_name_ = inst.info.display_name;

    // 显示控制器名称，不显示 instance_id
    name_label_->setText(tr("Controller:") + ToQString(display_name_));//控制器：

    SetUiFromConnection_(inst.connection);
}

void SS_WidgetLightConnectionPanel::SlotConnectTypeChanged(int index)
{
    stack_->setCurrentIndex(index);
}

void SS_WidgetLightConnectionPanel::SlotApplyClicked()
{
    if (!system_ || instance_id_.empty())
        return;

    // 从 core 拉一次 instance，保留 UI 没显示的字段（byte_transmission_params 等）
    SS_LightControllerInstance inst;
    if (!system_->GetInstance(instance_id_, inst))
    {
        QMessageBox::warning(this, tr("Error"), tr("GetInstance failed"));//错误 GetInstance 失败
        return;
    }

    SS_LightConnectionConfig new_conn = inst.connection;
    ReadUiToConnection_(new_conn);

    std::string err;
    if (!system_->UpdateConnectionConfig(instance_id_, new_conn, err))
    {
        QMessageBox::warning(this, tr("Application failed"), ToQString(err));//应用失败
        return;
    }

    // 保存落盘
    std::string save_err;
    if (!system_->SaveInstanceById(instance_id_, save_err))
    {
        QMessageBox::warning(this, tr("Save to failed"), ToQString(save_err));//保存失败
        return;
    }

    emit SignalConnectionUpdated(instance_id_);
}

void SS_WidgetLightConnectionPanel::SlotConnectClicked()
{
    if (!system_ || instance_id_.empty())
        return;

    std::string err;
    if (!system_->ConnectInstance(instance_id_, err))
    {
        QMessageBox::warning(this, tr("Connection fail"), ToQString(err));//连接失败
        return;
    }
}

void SS_WidgetLightConnectionPanel::SlotDisconnectClicked()
{
    if (!system_ || instance_id_.empty())
        return;

    std::string err;
    (void)system_->DisconnectInstance(instance_id_, err);
}

void SS_WidgetLightConnectionPanel::SlotRenameClicked()
{
    if (!system_ || instance_id_.empty())
        return;

    bool ok = false;
    const QString new_name = QInputDialog::getText(
        this,
        tr("Change the name of the controller"),//修改控制器名称
        tr("Please enter a new name:"),//请输入新的名称：
        QLineEdit::Normal,
        ToQString(display_name_),
        &ok
    );

    if (!ok)
        return;

    const std::string new_display_name = ToStdString(new_name).empty()
        ? display_name_
        : ToStdString(new_name);

    if (!system_->RenameController(instance_id_, new_display_name))
    {
        QMessageBox::warning(this, tr("Change failed"), tr("RenameController failed"));//修改失败 RenameController 失败
        return;
    }

    std::string save_err;
    if (!system_->SaveInstanceById(instance_id_, save_err))
    {
        QMessageBox::warning(this, tr("Save to failed"), ToQString(save_err));// 保存失败
        return;
    }

    display_name_ = new_display_name;
    name_label_->setText(tr("Controller:") + ToQString(display_name_));// 控制器：

    emit SignalConnectionUpdated(instance_id_);
}

void SS_WidgetLightConnectionPanel::InitQSS_()
{
}

void SS_WidgetLightConnectionPanel::BuildUi_()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    group_ = new QGroupBox(tr("Connection configuration"), this);// 连接配置
    QVBoxLayout* g = new QVBoxLayout(group_);
    g->setContentsMargins(8, 8, 8, 8);
    g->setSpacing(8);

    // top row
    QHBoxLayout* top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(8);

    name_label_ = new QLabel(group_);
    name_label_->setText(tr("Controller: -"));//控制器：-

    rename_btn_ = new QToolButton(group_);
    rename_btn_->setText(tr("Alter"));//修改
    rename_btn_->setToolTip(tr("Change the name of the controller"));//修改控制器名称

    connect_type_combo_ = new QComboBox(group_);
    connect_type_combo_->addItem("SERIAL");
    connect_type_combo_->addItem("SOCKET");

    apply_btn_ = new QPushButton(tr("Apply and save"), group_);//应用并保存
    connect_btn_ = new QPushButton(tr("Connect"), group_);//连接
    disconnect_btn_ = new QPushButton(tr("Disconnect"), group_);//断开

    top->addWidget(name_label_, 1);
    top->addWidget(rename_btn_);
    top->addWidget(new QLabel(tr("Mode:"), group_));//方式：
    top->addWidget(connect_type_combo_);
    top->addStretch();
    top->addWidget(apply_btn_);
    top->addWidget(connect_btn_);
    top->addWidget(disconnect_btn_);

    g->addLayout(top);

    // stacked pages
    stack_ = new QStackedWidget(group_);

    // -------- serial page --------
    serial_page_ = new QWidget(stack_);
    {
        QFormLayout* f = new QFormLayout(serial_page_);
        f->setHorizontalSpacing(12);
        f->setVerticalSpacing(8);

        serial_com_ = new QLineEdit(serial_page_);

        serial_baud_combo_ = new QComboBox(serial_page_);
        BuildBaudRateOptions_();

        serial_data_bits_ = new QSpinBox(serial_page_);
        serial_data_bits_->setRange(5, 8);

        serial_stop_bits_ = new QSpinBox(serial_page_);
        serial_stop_bits_->setRange(1, 2);

        serial_parity_ = new QSpinBox(serial_page_);
        serial_parity_->setRange(0, 4);

        f->addRow(tr("COM port"), serial_com_);//COM口
        f->addRow(tr("Baud rate"), serial_baud_combo_);//波特率
        f->addRow(tr("Data bits"), serial_data_bits_);//数据位
        f->addRow(tr("Stop bit"), serial_stop_bits_);//停止位
        f->addRow(tr("Verify(0-4)"), serial_parity_);//校验(0-4)
    }

    // -------- socket page (只保留目标) --------
    socket_page_ = new QWidget(stack_);
    {
        QFormLayout* f = new QFormLayout(socket_page_);
        f->setHorizontalSpacing(12);
        f->setVerticalSpacing(8);

        socket_dest_ip_ = new QLineEdit(socket_page_);
        socket_dest_port_ = new QSpinBox(socket_page_);
        socket_dest_port_->setRange(0, 65535);

        f->addRow(tr("Target IP"), socket_dest_ip_);//目标IP
        f->addRow(tr("Target port"), socket_dest_port_);//目标端口
    }

    stack_->addWidget(serial_page_); // index 0
    stack_->addWidget(socket_page_); // index 1
    g->addWidget(stack_);

    root->addWidget(group_);


    InitQSS_();

}

void SS_WidgetLightConnectionPanel::InitConnections_()
{
    connect(connect_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SS_WidgetLightConnectionPanel::SlotConnectTypeChanged);
    connect(apply_btn_, &QPushButton::clicked, this, &SS_WidgetLightConnectionPanel::SlotApplyClicked);
    connect(connect_btn_, &QPushButton::clicked, this, &SS_WidgetLightConnectionPanel::SlotConnectClicked);
    connect(disconnect_btn_, &QPushButton::clicked, this, &SS_WidgetLightConnectionPanel::SlotDisconnectClicked);
    connect(rename_btn_, &QToolButton::clicked, this, &SS_WidgetLightConnectionPanel::SlotRenameClicked);
}

void SS_WidgetLightConnectionPanel::SetUiFromConnection_(const SS_LightConnectionConfig& conn)
{
    // connect type
    if (conn.connect_type == SS_LIGHT_CONNECT_TYPE::SOCKET)
    {
        connect_type_combo_->setCurrentIndex(1);
        stack_->setCurrentIndex(1);
    }
    else
    {
        connect_type_combo_->setCurrentIndex(0);
        stack_->setCurrentIndex(0);
    }

    // serial
    serial_com_->setText(ToQString(conn.serial_parameter.com_port_num));
    SelectBaudRate_(conn.serial_parameter.baud_rate);
    serial_data_bits_->setValue(conn.serial_parameter.character_size);
    serial_stop_bits_->setValue(conn.serial_parameter.stop_bits);
    serial_parity_->setValue(conn.serial_parameter.parity);

    // socket (只保留目标)
    socket_dest_ip_->setText(ToQString(conn.socket_parameter.destination_ip_address));
    socket_dest_port_->setValue(conn.socket_parameter.destination_port);
}

void SS_WidgetLightConnectionPanel::ReadUiToConnection_(SS_LightConnectionConfig& out_conn) const
{
    out_conn = SS_LightConnectionConfig{};

    const int idx = connect_type_combo_->currentIndex();
    out_conn.connect_type = (idx == 1) ? SS_LIGHT_CONNECT_TYPE::SOCKET : SS_LIGHT_CONNECT_TYPE::SERIAL;

    // serial
    out_conn.serial_parameter.com_port_num = ToStdString(serial_com_->text());
    out_conn.serial_parameter.baud_rate = serial_baud_combo_->currentText().toInt();
    out_conn.serial_parameter.character_size = serial_data_bits_->value();
    out_conn.serial_parameter.stop_bits = serial_stop_bits_->value();
    out_conn.serial_parameter.parity = serial_parity_->value();

    // socket (只保留目标)
    out_conn.socket_parameter.destination_ip_address = ToStdString(socket_dest_ip_->text());
    out_conn.socket_parameter.destination_port = socket_dest_port_->value();

    // 本机IP/端口暂时不用：保持默认
}

void SS_WidgetLightConnectionPanel::BuildBaudRateOptions_()
{
    if (!serial_baud_combo_) return;

    serial_baud_combo_->clear();

    // 市面常见/主流波特率（覆盖串口设备常用范围）
    const int baud_list[] = {
        1200, 2400, 4800, 9600,
        14400, 19200, 38400,
        57600, 115200,
        230400, 460800, 921600
    };

    for (int b : baud_list)
        serial_baud_combo_->addItem(QString::number(b));

    // 可编辑：方便遇到奇葩设备（比如 250000 这种）
    serial_baud_combo_->setEditable(true);
}

void SS_WidgetLightConnectionPanel::SelectBaudRate_(int baud)
{
    if (!serial_baud_combo_) return;

    const QString s = QString::number(baud);
    int idx = serial_baud_combo_->findText(s);
    if (idx >= 0)
    {
        serial_baud_combo_->setCurrentIndex(idx);
        return;
    }

    // 不在列表里就塞进去（设备厂商总爱搞点“特色”）
    serial_baud_combo_->addItem(s);
    serial_baud_combo_->setCurrentText(s);
}