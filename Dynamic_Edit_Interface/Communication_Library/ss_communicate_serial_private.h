#pragma once

#include "ss_communicate_interface.h"

// boost
#include <boost/asio.hpp>
#include <atomic>
#include <thread>
#include <memory>

class CommunicateSerialPrivate
{
public:
    CommunicateSerialPrivate();
    ~CommunicateSerialPrivate();

    bool Init();
    bool Connect(ConnectionInfo connect_info);
    bool IsConnected() const;
    void Disconnect();
    bool ReConnect();
    CommunicateConnectState GetState() const;

    int64_t WriteData(const char* data);
    int64_t WriteData(const char* data, int64_t len);

    ConnectionInfo GetCommunicateInfo();

    void SetDataCallback(DataCallback callback);
    void SetErrorCallback(ErrorCallback callback);

public:
    DataCallback data_call_back_;
    ErrorCallback error_call_back_;
    ConnectionInfo connect_info_;

private:
    void ReceiveLoop_();
    bool ApplySerialOptions_(const ConnectionInfo& info, std::string& out_error);

private:
    boost::asio::io_context io_context_;
    boost::asio::serial_port serial_;

    std::shared_ptr<std::thread> read_thread_;
    std::atomic_bool is_running_{ false };
    std::atomic_bool connected_{ false };
};
