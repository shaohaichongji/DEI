#pragma once

#include <memory>
#include "ss_communicate_interface.h"

class CommunicateTcpServerPrivate;
class CommunicateTcpServer : public CommunicateInterface
{
public:
    CommunicateTcpServer();
    ~CommunicateTcpServer();

    virtual bool Init() override;
    virtual bool Connect(ConnectionInfo connect_info) const override;
    virtual bool IsConnected() const override;
    virtual void Disconnect() override;
    virtual bool ReConnect() override;
    virtual CommunicateConnectState GetState() const override;
    virtual int64_t WriteData(const char* data) override;
    virtual int64_t WriteData(const char* data, int64_t len) override;
    virtual ConnectionInfo GetCommunicateInfo() override;
    virtual void SetDataCallback(DataCallback callback) override;
    virtual void SetErrorCallback(ErrorCallback callback) override;
private:
    std::shared_ptr<CommunicateTcpServerPrivate> communicate_tcp_server_impl_;
};
