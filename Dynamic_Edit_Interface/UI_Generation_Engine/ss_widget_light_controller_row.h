#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QString>

class QLabel;
class QToolButton;

class SS_WidgetLightControllerRow : public QWidget
{
    Q_OBJECT
public:
    explicit SS_WidgetLightControllerRow(QWidget* parent = nullptr);

    void SetTitle(const QString& title);
    void SetConnected(bool connected);
    void SetInstanceId(const QString& instance_id);

    QString GetInstanceId() const { return instance_id_; }

    void SetAllowAddChannel(bool allowed);

signals:
    void SignalAddChannelClicked(const QString& instance_id);

private slots:
    void SlotAddClicked();

private:
    void InitQSS_();
    void BuildUi_();
    void InitConnections_();

private:
    QString instance_id_;

    QLabel* status_label_ = nullptr;
    QLabel* title_label_ = nullptr;
    QToolButton* add_btn_ = nullptr;
};
