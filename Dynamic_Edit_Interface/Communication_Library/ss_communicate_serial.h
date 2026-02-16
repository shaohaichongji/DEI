#pragma once

#include <memory>
#include "ss_communicate_interface.h"

class CommunicateSerialPrivate;

class CommunicateSerial : public CommunicateInterface
{
public:
    CommunicateSerial();
    ~CommunicateSerial();

    bool Init() override;
    bool Connect(ConnectionInfo connect_info) const override;
    bool IsConnected() const override;
    void Disconnect() override;
    bool ReConnect() override;
    CommunicateConnectState GetState() const override;

    int64_t WriteData(const char* data) override;
    int64_t WriteData(const char* data, int64_t len) override;

    ConnectionInfo GetCommunicateInfo() override;

    void SetDataCallback(DataCallback callback) override;
    void SetErrorCallback(ErrorCallback callback) override;

private:
    std::shared_ptr<CommunicateSerialPrivate> impl_;
};
