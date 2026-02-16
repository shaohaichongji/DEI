#include "ss_communicate_tcp_client_private.h"

#include <iostream>

using boost::asio::ip::tcp;

CommunicateTcpClientPrivate::CommunicateTcpClientPrivate()
    : socket_(io_context_)
{
}

CommunicateTcpClientPrivate::~CommunicateTcpClientPrivate()
{
    // 让线程退出 + 关 socket
    Disconnect();

    is_running_.store(false);
    if (read_thread_ && read_thread_->joinable())
        read_thread_->join();
}

bool CommunicateTcpClientPrivate::Init()
{
    return true;
}

void CommunicateTcpClientPrivate::ReportError_(int code, const std::string& msg)
{
    if (error_call_back_)
        error_call_back_(code, msg);
}

bool CommunicateTcpClientPrivate::Connect(ConnectionInfo connect_info)
{
    connect_info_ = connect_info;

    if (connect_info_.ip_.empty())
        connect_info_.ip_ = "127.0.0.1";
    if (connect_info_.port_ <= 0)
        connect_info_.port_ = 0; // 让 resolver 报错更明确

    try
    {
        // 先断开旧连接
        Disconnect();

        tcp::resolver resolver(io_context_);
        boost::system::error_code ec;

        auto endpoints = resolver.resolve(connect_info_.ip_, std::to_string(connect_info_.port_), ec);
        if (ec)
        {
            ReportError_(ec.value(), "TCP resolve failed: " + ec.message());
            connected_.store(false);
            return false;
        }

        boost::asio::connect(socket_, endpoints, ec);
        if (ec)
        {
            ReportError_(ec.value(), "TCP connect failed: " + ec.message());
            connected_.store(false);
            return false;
        }

        connected_.store(true);

        if (!is_running_.load())
        {
            is_running_.store(true);
            read_thread_.reset(new std::thread(&CommunicateTcpClientPrivate::ReceiveLoop_, this));
        }

        std::cout << "TCP Client connected: " << connect_info_.ip_ << ":" << connect_info_.port_ << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        ReportError_(-1, std::string("TCP connect exception: ") + e.what());
        connected_.store(false);
        return false;
    }
}

bool CommunicateTcpClientPrivate::IsConnected() const
{
    // socket_.is_open() 只能说明句柄开着，不代表真的连着
    // 这里用 connected_ 作为我们自己的逻辑状态
    return connected_.load() && socket_.is_open();
}

void CommunicateTcpClientPrivate::Disconnect()
{
    connected_.store(false);

    boost::system::error_code ec;

    if (socket_.is_open())
    {
        // 尽量优雅关闭
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

bool CommunicateTcpClientPrivate::ReConnect()
{
    Disconnect();
    return Connect(connect_info_);
}

CommunicateConnectState CommunicateTcpClientPrivate::GetState() const
{
    return IsConnected()
        ? CommunicateConnectState::ConnectedState
        : CommunicateConnectState::UnconnectedState;
}

int64_t CommunicateTcpClientPrivate::WriteData(const char* data)
{
    if (!data) return -1;
    const std::string s(data);
    return WriteData(s.data(), (int64_t)s.size());
}

int64_t CommunicateTcpClientPrivate::WriteData(const char* data, int64_t len)
{
    if (!data || len <= 0) return -1;
    if (!IsConnected()) return -1;

    try
    {
        boost::system::error_code ec;
        const size_t n = boost::asio::write(socket_, boost::asio::buffer(data, (size_t)len), ec);
        if (ec)
        {
            ReportError_(ec.value(), "TCP write failed: " + ec.message());
            connected_.store(false);
            return -1;
        }
        return (int64_t)n;
    }
    catch (const std::exception& e)
    {
        ReportError_(-1, std::string("TCP write exception: ") + e.what());
        connected_.store(false);
        return -1;
    }
}

ConnectionInfo CommunicateTcpClientPrivate::GetCommunicateInfo()
{
    return connect_info_;
}

void CommunicateTcpClientPrivate::SetDataCallback(DataCallback callback)
{
    data_call_back_ = callback;
}

void CommunicateTcpClientPrivate::SetErrorCallback(ErrorCallback callback)
{
    error_call_back_ = callback;
}

void CommunicateTcpClientPrivate::ReceiveLoop_()
{
    std::array<char, 4096> buffer;

    while (is_running_.load())
    {
        if (!IsConnected())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        try
        {
            boost::system::error_code ec;
            const size_t bytes = socket_.read_some(boost::asio::buffer(buffer), ec);

            if (ec)
            {
                // 连接被对端关闭/网络错误
                ReportError_(ec.value(), "TCP read failed: " + ec.message());
                connected_.store(false);

                // 给一点喘息，避免疯狂刷错误
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            if (bytes > 0 && data_call_back_)
            {
                std::vector<char> vec_tmp(buffer.begin(), buffer.begin() + bytes);
                data_call_back_(connect_info_.ip_, vec_tmp);
            }
        }
        catch (const std::exception& e)
        {
            ReportError_(-1, std::string("TCP receive exception: ") + e.what());
            connected_.store(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}
