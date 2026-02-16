#pragma once

#include "ss_communicate_interface.h"
#include <boost/asio.hpp>

using namespace boost::asio;
using ip::tcp;

class SimpleServer;
class CommunicateTcpServerPrivate
{
public:
    CommunicateTcpServerPrivate();
    ~CommunicateTcpServerPrivate();

    bool Init();
    bool Connect(ConnectionInfo connect_info);
    bool IsConnected() const;
    void Disconnect();
    bool ReConnect();
    CommunicateConnectState GetState() const;
    int64_t WriteData(const char* data);
    int64_t WriteData(const char* data, int64_t len);
    void SetDataCallback(DataCallback callback);
    void SetErrorCallback(ErrorCallback callback);

    //DataCallback data_call_back_;
    ErrorCallback error_call_back_;
    ConnectionInfo connect_info_;
private:
    //void StartAccept();
private:
    boost::asio::io_context io_context_;
    boost::shared_ptr<SimpleServer> server_;
    std::shared_ptr<std::thread> work_thread_;
    std::atomic_bool work_thread_is_running_{ false };
    //std::shared_ptr<io_context> io_context_;
    //std::shared_ptr<tcp::acceptor> acceptor_;
    //std::shared_ptr<std::thread> connect_thread_;
    //std::atomic_bool connect_thread_is_running_{ false };
};

