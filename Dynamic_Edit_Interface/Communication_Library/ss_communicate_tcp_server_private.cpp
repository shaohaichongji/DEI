#include "ss_communicate_tcp_server_private.h"

#include <iostream>
#include <queue>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

class ClientSession : public boost::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::io_context& io_context, int id)
        : socket_(io_context), client_id_(id) {
    }

    tcp::socket& Socket() {
        return socket_;
    }

    int GetID() const {
        return client_id_;
    }

    // 启动会话
    void Start() {
        DoRead();
    }

    // 直接发送数据
    void Send(const std::string& data) {
        size_t wirte_size = boost::asio::write(socket_, boost::asio::buffer(data));
        //boost::asio::async_write(socket_,
        //    boost::asio::buffer(data),
        //    [self = shared_from_this()](boost::system::error_code ec, size_t) {
        //    if (ec) {
        //        if (error_call_back_) {
        //            std::string str_error = ec.message();
        //            error_call_back_(-1, str_error);
        //        }
        //        //std::cout << "发送错误: " << ec.message() << std::endl;
        //    }
        //});
    }
public:
    static DataCallback data_call_back_;
    static ErrorCallback error_call_back_;
private:
    // 读取数据
    void DoRead() {
        auto self = shared_from_this();

        socket_.async_read_some(
            boost::asio::buffer(buffer_, sizeof(buffer_)),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    // 直接处理接收到的数据
                    std::string data(buffer_, length);
                    //handle_data(data);
                    std::cout << "server read: " << client_id_ << "    " << data << "   " << std::endl;
                    if (data_call_back_) {
                        std::vector<char> vec1(data.begin(), data.end());
                        boost::asio::ip::address remote_address =
                            socket_.remote_endpoint().address();
                        data_call_back_(remote_address.to_string(), vec1);
                    }
                    // 继续读取
                    DoRead();
                }
                else {
                    std::cout << "client: " << client_id_ << " disconnect: " << ec.message() << std::endl;
                }
            });
    }

    // 处理数据
    void HnadleData(const std::string& data) {
        //std::cout << "从客户端 " << client_id_ << " 收到: " << data;

        //// 简单回声
        //std::string response = "回声: " + data;
        //Send(response);

        //// 如果是quit命令则关闭连接
        //if (data.find("quit") != std::string::npos) {
        //    Send("再见!\r\n");
        //    socket_.close();
        //}
    }

    tcp::socket socket_;
    int client_id_;
    char buffer_[1024];
};
DataCallback ClientSession::data_call_back_;
ErrorCallback ClientSession::error_call_back_;

class SimpleServer {
public:
    SimpleServer(boost::asio::io_context& io_context, short port)
        : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        next_id_(1) {

        std::cout << "服务器启动在端口 " << port << std::endl;
        DoAccept();
    }

    // 广播消息给所有客户端
    void BroadCast(const std::string& message) {
        for (auto& client : clients_) {
            //if (auto client_ptr = client) {
            client->Send(message);
            //}
        }
    }

private:
    void DoAccept() {
        auto new_client = boost::make_shared<ClientSession>(io_context_, next_id_++);

        acceptor_.async_accept(new_client->Socket(),
            [this, new_client](boost::system::error_code ec) {
                if (!ec) {
                    // 添加到客户端列表
                    clients_.push_back(new_client);

                    // 启动客户端
                    new_client->Start();

                    // 通知其他客户端
                    //BroadCast("new client " + std::to_string(new_client->GetID()) + " join\r\n");

                    // 继续接受新连接
                    DoAccept();
                }
                else {
                    std::cerr << "接受连接错误: " << ec.message() << std::endl;
                }
            });
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::vector<boost::shared_ptr<ClientSession>> clients_;
    int next_id_;

};

CommunicateTcpServerPrivate::CommunicateTcpServerPrivate()
{

}

CommunicateTcpServerPrivate::~CommunicateTcpServerPrivate()
{
    io_context_.stop();

    // 等待线程结束
    if (work_thread_ && work_thread_->joinable()) {
        work_thread_->join();
    }
}

bool CommunicateTcpServerPrivate::Init()
{
    return true;
}

bool CommunicateTcpServerPrivate::Connect(ConnectionInfo connect_info)
{
    server_ = boost::make_shared<SimpleServer>(io_context_, connect_info.port_);
    work_thread_ = std::make_shared<std::thread>([this]() {
        io_context_.run();
        });
    return true;
}

bool CommunicateTcpServerPrivate::IsConnected() const
{
    return true;
}

void CommunicateTcpServerPrivate::Disconnect()
{

}

bool CommunicateTcpServerPrivate::ReConnect()
{
    return true;
}

CommunicateConnectState CommunicateTcpServerPrivate::GetState() const
{
    return  CommunicateConnectState::ConnectedState;
}

int64_t CommunicateTcpServerPrivate::WriteData(const char* data)
{
    std::string message{ data };
    //std::cout << "Sent: " << message << std::endl;
    server_->BroadCast(message);
    return -1;
}

int64_t CommunicateTcpServerPrivate::WriteData(const char* data, int64_t len)
{
    std::string message(data, len);
    server_->BroadCast(message);
    //std::cout << "Sent: " << message << std::endl;
    return -1;
}

void CommunicateTcpServerPrivate::SetDataCallback(DataCallback callback)
{
    ClientSession::data_call_back_ = callback;
}

void CommunicateTcpServerPrivate::SetErrorCallback(ErrorCallback callback)
{
    error_call_back_ = callback;
}
