#include "ss_communicate_serial_private.h"
#include <iostream>
#include <chrono>

using namespace boost::asio;

CommunicateSerialPrivate::CommunicateSerialPrivate()
    : serial_(io_context_)
{
}

CommunicateSerialPrivate::~CommunicateSerialPrivate()
{
    Disconnect();
    is_running_.store(false);
    if (read_thread_ && read_thread_->joinable())
        read_thread_->join();
}

bool CommunicateSerialPrivate::Init()
{
    return true;
}

bool CommunicateSerialPrivate::ApplySerialOptions_(const ConnectionInfo& info, std::string& out_error)
{
    out_error.clear();

    try
    {
        serial_.set_option(serial_port_base::baud_rate(info.baud_rate_));
        serial_.set_option(serial_port_base::character_size(info.character_size_));

        // stop bits
        if (info.stop_bits_ == 2)
            serial_.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::two));
        else
            serial_.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));

        // parity: 0=None, 1=Odd, 2=Even
        if (info.parity_ == 1)
            serial_.set_option(serial_port_base::parity(serial_port_base::parity::odd));
        else if (info.parity_ == 2)
            serial_.set_option(serial_port_base::parity(serial_port_base::parity::even));
        else
            serial_.set_option(serial_port_base::parity(serial_port_base::parity::none));

        // flow control: 先关掉
        serial_.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none));
    }
    catch (const std::exception& e)
    {
        out_error = std::string("ApplySerialOptions failed: ") + e.what();
        return false;
    }
    return true;
}

bool CommunicateSerialPrivate::Connect(ConnectionInfo connect_info)
{
    connect_info_ = connect_info;

    if (connect_info_.com_port_.empty())
        connect_info_.com_port_ = "COM1";
    if (connect_info_.baud_rate_ <= 0)
        connect_info_.baud_rate_ = 9600;

    try
    {
        boost::system::error_code ec;
        if (serial_.is_open())
            serial_.close(ec);

        serial_.open(connect_info_.com_port_, ec);
        if (ec)
        {
            if (error_call_back_)
                error_call_back_(ec.value(), "Serial open failed: " + ec.message());
            return false;
        }

        std::string err;
        if (!ApplySerialOptions_(connect_info_, err))
        {
            if (error_call_back_)
                error_call_back_(-1, err);
            boost::system::error_code ec2;
            serial_.close(ec2);
            return false;
        }

        connected_.store(true);

        if (!is_running_.load())
        {
            is_running_.store(true);
            read_thread_.reset(new std::thread(&CommunicateSerialPrivate::ReceiveLoop_, this));
        }

        std::cout << "Serial connected: " << connect_info_.com_port_
            << " baud=" << connect_info_.baud_rate_ << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        if (error_call_back_)
            error_call_back_(-1, std::string("Serial connect exception: ") + e.what());
        connected_.store(false);
        return false;
    }
}

bool CommunicateSerialPrivate::IsConnected() const
{
    return connected_.load() && serial_.is_open();
}

void CommunicateSerialPrivate::Disconnect()
{
    connected_.store(false);

    boost::system::error_code ec;
    if (serial_.is_open())
        serial_.close(ec);
}

bool CommunicateSerialPrivate::ReConnect()
{
    Disconnect();
    return Connect(connect_info_);
}

CommunicateConnectState CommunicateSerialPrivate::GetState() const
{
    return IsConnected() ? CommunicateConnectState::ConnectedState
        : CommunicateConnectState::UnconnectedState;
}

int64_t CommunicateSerialPrivate::WriteData(const char* data)
{
    if (!data) return -1;
    const std::string s(data);
    return WriteData(s.data(), (int64_t)s.size());
}

int64_t CommunicateSerialPrivate::WriteData(const char* data, int64_t len)
{
    if (!data || len <= 0) return -1;
    if (!IsConnected()) return -1;

    try
    {
        boost::system::error_code ec;
        const size_t n = write(serial_, buffer(data, (size_t)len), ec);
        if (ec)
        {
            if (error_call_back_)
                error_call_back_(ec.value(), "Serial write failed: " + ec.message());
            return -1;
        }
        return (int64_t)n;
    }
    catch (const std::exception& e)
    {
        if (error_call_back_)
            error_call_back_(-1, std::string("Serial write exception: ") + e.what());
        return -1;
    }
}

ConnectionInfo CommunicateSerialPrivate::GetCommunicateInfo()
{
    return connect_info_;
}

void CommunicateSerialPrivate::SetDataCallback(DataCallback callback)
{
    data_call_back_ = callback;
}

void CommunicateSerialPrivate::SetErrorCallback(ErrorCallback callback)
{
    error_call_back_ = callback;
}

void CommunicateSerialPrivate::ReceiveLoop_()
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
            const size_t bytes = serial_.read_some(boost::asio::buffer(buffer), ec);

            if (ec)
            {
                if (error_call_back_)
                    error_call_back_(ec.value(), "Serial read failed: " + ec.message());

                connected_.store(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            if (bytes > 0 && data_call_back_)
            {
                std::vector<char> vec_tmp(buffer.begin(), buffer.begin() + bytes);
                // 串口没 ip，给个固定标识
                data_call_back_(connect_info_.com_port_, vec_tmp);
            }
        }
        catch (const std::exception& e)
        {
            if (error_call_back_)
                error_call_back_(-1, std::string("Serial receive exception: ") + e.what());
            connected_.store(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}
