// ss_widget_light_add_controller_dialog.cpp
#include "ss_widget_light_add_controller_dialog.h"
#include "moc/moc_ss_widget_light_add_controller_dialog.cpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QLabel>
#include <QtGui/QIntValidator>

#include "../../include/Parsing_Engine/ss_light_resource_api.h"
#include "../../include/Parsing_Engine/ss_light_resource_models.h"

static void FillCommonBaudRates(QComboBox* baud)
{
    if (!baud) return;
    const int rates[] = { 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 230400, 460800, 921600 };
    for (int r : rates)
        baud->addItem(QString::number(r), r);
    int idx = baud->findData(9600);
    if (idx >= 0) baud->setCurrentIndex(idx);

}

// ------------------------------------------------------------
// 把弹窗选择的连接信息写入 instance.connection
// ------------------------------------------------------------
static void ApplyConnectionToInstance_(
    SS_LightControllerInstance& inst,
    SS_LIGHT_CONNECT_TYPE ct,
    const std::string& com_port,
    int baud_rate,
    const std::string& dest_ip,
    int dest_port)
{
    // connect_type 以弹窗选择为准
    inst.connection.connect_type = ct;

    // 改配置后，连接状态肯定要归零，避免 UI 假装已连接
    inst.connection.connect_state = false;

    if (ct == SS_LIGHT_CONNECT_TYPE::SERIAL)
    {
        inst.connection.serial_parameter.com_port_num = com_port;
        inst.connection.serial_parameter.baud_rate = baud_rate;

        // 模板里没让用户选这些，给个合理默认
        if (inst.connection.serial_parameter.character_size <= 0)
            inst.connection.serial_parameter.character_size = 8;
        if (inst.connection.serial_parameter.stop_bits <= 0)
            inst.connection.serial_parameter.stop_bits = 1;
        // parity: 0 作为默认（None）
    }
    else if (ct == SS_LIGHT_CONNECT_TYPE::SOCKET)
    {
        // 网口只需要目标IP和目标端口
        inst.connection.socket_parameter.destination_ip_address = dest_ip;
        inst.connection.socket_parameter.destination_port = dest_port;

        // 其它字段暂时不用就别碰（保留已有值）
    }
}

SS_WidgetLightAddControllerDialog::SS_WidgetLightAddControllerDialog(QWidget* parent)
    : QDialog(parent)
{
    BuildUi_();
    InitConnections_();
}

void SS_WidgetLightAddControllerDialog::SetSystem(SS_LightResourceSystem* system)
{
    system_ = system;

    QString err;
    if (!InitIndex_(err))
    {
        QMessageBox::warning(this, QStringLiteral("Error"), err);// 错误
        return;
    }

    RefreshFactory_();
}

void SS_WidgetLightAddControllerDialog::SlotFactoryChanged(int) { RefreshType_(); }
void SS_WidgetLightAddControllerDialog::SlotTypeChanged(int) { RefreshModel_(); }
void SS_WidgetLightAddControllerDialog::SlotModelChanged(int) { RefreshChannelMax_(); }
void SS_WidgetLightAddControllerDialog::SlotChannelMaxChanged(int) { RefreshConnectType_(); }
void SS_WidgetLightAddControllerDialog::SlotConnectTypeChanged(int) { RefreshConnectParamPanel_(); }

void SS_WidgetLightAddControllerDialog::SlotAddClicked()
{
    if (!system_)
        return;

    const QString name_q = name_edit_->text().trimmed();
    if (name_q.isEmpty())
    {
        QMessageBox::warning(this, tr("Hint"), tr("Please enter the name of the controller."));//提示 请填写控制器名称。
        return;
    }

    const std::string factory = ToStdString_(factory_combo_->currentText());
    const std::string type = ToStdString_(type_combo_->currentText());
    const std::string model = ToStdString_(model_combo_->currentText());
    const int ch_max = channel_combo_->currentData().toInt();
    const SS_LIGHT_CONNECT_TYPE ct = (SS_LIGHT_CONNECT_TYPE)connect_combo_->currentData().toInt();

    // 缓存用户选择的连接参数
    if (ct == SS_LIGHT_CONNECT_TYPE::SERIAL)
    {
        selected_com_port_ = ToStdString_(com_combo_->currentText());
        selected_baud_rate_ = baud_combo_->currentData().toInt();
        if (selected_com_port_.empty() || selected_baud_rate_ <= 0)
        {
            QMessageBox::warning(this, tr("Hint"), tr("Please select the valid serial port number and baud rate."));//提示 请选择有效的串口号和波特率。
            return;
        }
    }
    else if (ct == SS_LIGHT_CONNECT_TYPE::SOCKET)
    {
        selected_dest_ip_ = ToStdString_(ip_edit_->text().trimmed());
        selected_dest_port_ = port_spin_->value();

        if (selected_dest_ip_.empty())
        {
            QMessageBox::warning(this, tr("Hint"), tr("Please enter the target IP."));// 提示 请输入目标IP。
            return;
        }
        if (selected_dest_port_ <= 0 || selected_dest_port_ > 65535)
        {
            QMessageBox::warning(this, tr("Hint"), tr("The target port number is invalid."));// 提示 请输入目标IP。
            return;
        }
    }
    else
    {
        QMessageBox::warning(this, tr("Hint"), tr("Please select the communication method."));// 提示 请输入目标IP。
        return;
    }

    // 解析唯一模板叶子
    SS_LightTemplateLeafIndex::Leaf leaf;
    std::string resolve_err;
    if (!index_.ResolveUniqueLeaf(factory, type, model, ch_max, ct, leaf, resolve_err))
    {
        QMessageBox::warning(this, tr("Template selection failed"), QString::fromStdString(resolve_err));// 模板选择失败
        return;
    }

    // 1) 创建实例
    std::string instance_id;
    std::string err;
    if (!system_->CreateInstanceFromTemplate(leaf.template_id, ToStdString_(name_q), instance_id, err))
    {
        QMessageBox::warning(this, tr("Creation failed"), QString::fromStdString(err)); // 创建失败
        return;
    }

    // 2) 取回 instance，写 connection 配置
    SS_LightControllerInstance inst;
    if (!system_->GetInstance(instance_id, inst))
    {
        QMessageBox::warning(this, tr("Creation failed"), tr("The instance data cannot be read after creation."));// 创建失败 创建后无法读取实例数据。
        return;
    }

    ApplyConnectionToInstance_(
        inst,
        ct,
        selected_com_port_,
        selected_baud_rate_,
        selected_dest_ip_,
        selected_dest_port_);

    // 3) 写回 runtime（UpdateConnectionConfig）并落盘
    std::string update_err;
    if (!system_->UpdateConnectionConfig(instance_id, inst.connection, update_err))
    {
        QMessageBox::warning(this, tr("Failed to update connection configuration"), QString::fromStdString(update_err));// 更新连接配置失败
        return;
    }

    std::string save_err;
    if (!system_->SaveInstanceById(instance_id, save_err))
    {
        QMessageBox::warning(this, tr("Save to failed"), QString::fromStdString(save_err));// 保存失败
        return;
    }

    // 4) 默认尝试连接（你需求说“默认尝试建立通讯连接”）
    std::string conn_err;
    if (!system_->ConnectInstance(instance_id, conn_err))
    {
        // 连接失败不要阻断创建，提示一下就行
        QMessageBox::information(this, tr("It has been created, but the connection failed."),// 已创建，但连接失败
            tr("The instance has been created and saved, but the connection failed:\n%1").arg(QString::fromStdString(conn_err)));// 实例已创建并保存，但连接未成功：\n%1
    }

    created_instance_id_ = instance_id;
    emit SignalControllerCreated(QString::fromStdString(instance_id));
    accept();
}

void SS_WidgetLightAddControllerDialog::InitQSS_()
{
}

void SS_WidgetLightAddControllerDialog::BuildUi_()
{
    setWindowTitle(tr("Add a light source controller"));// 添加光源控制器
    setModal(true);
    resize(520, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText(tr("Please enter the name of the controller"));//请输入控制器名称
    form->addRow(tr("Controller name"), name_edit_); // 控制器名称

    factory_combo_ = new QComboBox(this);
    type_combo_ = new QComboBox(this);
    model_combo_ = new QComboBox(this);
    channel_combo_ = new QComboBox(this);
    connect_combo_ = new QComboBox(this);

    form->addRow(tr("Factory"), factory_combo_);// 厂家
    form->addRow(tr("Controller type"), type_combo_);// 控制器类型
    form->addRow(tr("Controller model"), model_combo_);// 控制器型号
    form->addRow(tr("Channel number"), channel_combo_);// 通道数
    form->addRow(tr("Communication method"), connect_combo_);// 通讯方式

    root->addLayout(form);

    // connect param stack
    connect_stack_ = new QStackedWidget(this);

    // SERIAL page
    {
        auto* page = new QWidget(connect_stack_);
        auto* f = new QFormLayout(page);
        f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        f->setHorizontalSpacing(12);
        f->setVerticalSpacing(8);

        com_combo_ = new QComboBox(page);
        // 如果后续要扫真实串口，这里换成枚举逻辑即可
        for (int i = 1; i <= 32; ++i)
            com_combo_->addItem(QString("COM%1").arg(i));

        baud_combo_ = new QComboBox(page);
        FillCommonBaudRates(baud_combo_);

        f->addRow(tr("Serial number"), com_combo_);// 串口号
        f->addRow(tr("Baud rate"), baud_combo_);// 波特率

        connect_stack_->addWidget(page);
    }

    // SOCKET page
    {
        auto* page = new QWidget(connect_stack_);
        auto* f = new QFormLayout(page);
        f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        f->setHorizontalSpacing(12);
        f->setVerticalSpacing(8);

        ip_edit_ = new QLineEdit(page);
        ip_edit_->setPlaceholderText(tr("For example: 192.168.1.252"));// 例如 192.168.1.252

        port_spin_ = new QSpinBox(page);
        port_spin_->setRange(1, 65535);
        port_spin_->setValue(8234);

        f->addRow(tr("Target IP"), ip_edit_);// 目标IP
        f->addRow(tr("Target port"), port_spin_);// 目标端口

        connect_stack_->addWidget(page);
    }

    root->addWidget(connect_stack_);

    //hint_label_ = new QLabel(this);
    //hint_label_->setWordWrap(true);
    //hint_label_->setText(QStringLiteral("说明：当前版本会创建实例并尝试连接。\n"
    //    "连接参数写回实例文件的接口后续补齐（不会影响现在先把 UI 跑通）。"));
    //root->addWidget(hint_label_);

    // bottom buttons
    auto* btn_row = new QHBoxLayout();
    btn_row->addStretch();

    add_btn_ = new QPushButton(tr("Add"), this);// 添加
    btn_row->addWidget(add_btn_);

    root->addLayout(btn_row);

    InitQSS_();

}

void SS_WidgetLightAddControllerDialog::InitConnections_()
{
    // signals
    connect(factory_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SS_WidgetLightAddControllerDialog::SlotFactoryChanged);
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SS_WidgetLightAddControllerDialog::SlotTypeChanged);
    connect(model_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SS_WidgetLightAddControllerDialog::SlotModelChanged);
    connect(channel_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SS_WidgetLightAddControllerDialog::SlotChannelMaxChanged);
    connect(connect_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SS_WidgetLightAddControllerDialog::SlotConnectTypeChanged);
    connect(add_btn_, &QPushButton::clicked,
        this, &SS_WidgetLightAddControllerDialog::SlotAddClicked);
}

bool SS_WidgetLightAddControllerDialog::InitIndex_(QString& out_error)
{
    out_error.clear();
    if (!system_)
    {
        out_error = tr("SetSystem: system is null.");// SetSystem：系统为空。
        return false;
    }

    std::string err;
    if (!index_.BuildFromSystem(system_, err))
    {
        out_error = QString::fromStdString(err);
        return false;
    }
    return true;
}

void SS_WidgetLightAddControllerDialog::RefreshFactory_()
{
    factory_combo_->blockSignals(true);
    factory_combo_->clear();

    const auto factories = index_.ListFactories();
    for (const auto& f : factories)
        factory_combo_->addItem(ToQString_(f));

    factory_combo_->blockSignals(false);

    RefreshType_();
}

void SS_WidgetLightAddControllerDialog::RefreshType_()
{
    type_combo_->blockSignals(true);
    type_combo_->clear();

    const std::string factory = ToStdString_(factory_combo_->currentText());
    const auto types = index_.ListTypes(factory);
    for (const auto& t : types)
        type_combo_->addItem(ToQString_(t));

    type_combo_->blockSignals(false);

    RefreshModel_();
}

void SS_WidgetLightAddControllerDialog::RefreshModel_()
{
    model_combo_->blockSignals(true);
    model_combo_->clear();

    const std::string factory = ToStdString_(factory_combo_->currentText());
    const std::string type = ToStdString_(type_combo_->currentText());

    const auto models = index_.ListModels(factory, type);
    for (const auto& m : models)
        model_combo_->addItem(ToQString_(m));

    model_combo_->blockSignals(false);

    RefreshChannelMax_();
}

void SS_WidgetLightAddControllerDialog::RefreshChannelMax_()
{
    channel_combo_->blockSignals(true);
    channel_combo_->clear();

    const std::string factory = ToStdString_(factory_combo_->currentText());
    const std::string type = ToStdString_(type_combo_->currentText());
    const std::string model = ToStdString_(model_combo_->currentText());

    const auto cms = index_.ListChannelMax(factory, type, model);
    for (int c : cms)
        channel_combo_->addItem(QString::number(c), c);

    channel_combo_->blockSignals(false);

    RefreshConnectType_();
}

void SS_WidgetLightAddControllerDialog::RefreshConnectType_()
{
    connect_combo_->blockSignals(true);
    connect_combo_->clear();

    const std::string factory = ToStdString_(factory_combo_->currentText());
    const std::string type = ToStdString_(type_combo_->currentText());
    const std::string model = ToStdString_(model_combo_->currentText());
    const int ch_max = channel_combo_->currentData().toInt();

    const auto cts = index_.ListConnectTypes(factory, type, model, ch_max);
    for (auto ct : cts)
        connect_combo_->addItem(ConnectTypeToDisplay_(ct), (int)ct);

    connect_combo_->blockSignals(false);

    RefreshConnectParamPanel_();
}

void SS_WidgetLightAddControllerDialog::RefreshConnectParamPanel_()
{
    const SS_LIGHT_CONNECT_TYPE ct = (SS_LIGHT_CONNECT_TYPE)connect_combo_->currentData().toInt();
    selected_connect_type_ = ct;

    if (ct == SS_LIGHT_CONNECT_TYPE::SERIAL)
        connect_stack_->setCurrentIndex(0);
    else if (ct == SS_LIGHT_CONNECT_TYPE::SOCKET)
        connect_stack_->setCurrentIndex(1);
    else
        connect_stack_->setCurrentIndex(0);
}

QString SS_WidgetLightAddControllerDialog::ConnectTypeToDisplay_(SS_LIGHT_CONNECT_TYPE t)
{
    switch (t)
    {
    case SS_LIGHT_CONNECT_TYPE::SERIAL: return tr("Serial port");// 串口
    case SS_LIGHT_CONNECT_TYPE::SOCKET: return tr("Network port");// 网口
    default: return tr("Unknown");// 未知
    }
}

SS_LIGHT_CONNECT_TYPE SS_WidgetLightAddControllerDialog::DisplayToConnectType_(const QString& s)
{
    if (s == tr("Serial port")) return SS_LIGHT_CONNECT_TYPE::SERIAL;// 串口
    if (s == tr("Network port")) return SS_LIGHT_CONNECT_TYPE::SOCKET;// 网口
    return SS_LIGHT_CONNECT_TYPE::UNKNOWN;
}