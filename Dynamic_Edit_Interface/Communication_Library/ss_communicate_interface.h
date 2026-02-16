#pragma once

#include <string>
#include <functional>
#include <vector>

enum class CommunicateType
{
    TCP_CLIENT,
    TCP_SERVER,
    UDP,
    SERIAL,
    PROFINET
};

enum class CommunicateConnectState
{
    UnconnectedState,
    HostLookupState,
    ConnectingState,
    ConnectedState,
    BoundState,
    ListeningState,
    ClosingState
};

struct ConnectionInfo
{
    std::string ip_{ "127.0.0.1" };
    int port_ = -1;

    std::string com_port_{ "COM1" };
    int baud_rate_ = 9600;
    int character_size_ = 8;   // 5/6/7/8
    int stop_bits_ = 1;        // 1/2
    int parity_ = 0;           // 0=None,1=Odd,2=Even

    ConnectionInfo& operator = (const ConnectionInfo& info)
    {
        if (this != &info) {
            ip_ = info.ip_;
            port_ = info.port_;
            com_port_ = info.com_port_;
            baud_rate_ = info.baud_rate_;
            character_size_ = info.character_size_;
            stop_bits_ = info.stop_bits_;
            parity_ = info.parity_;
        }
        return *this;
    }
};

using DataCallback = std::function<void(std::string ip, std::vector<char>)>;
using ErrorCallback = std::function<void(int, const std::string&)>;

class CommunicateInterface
{
public:
    CommunicateInterface() {}
    virtual ~CommunicateInterface() {}

    /**
     * @brief return init is succeed
     * @param  null
     * @return bool true is succeed, false is init failed
     */
    virtual bool Init() { return false; }

    /**
     * @brief return connect is succeed
     * @param  ConnectionInfo connect information
     * @return bool true Connect succeed, false is Connect failed
     */
    virtual bool Connect(ConnectionInfo connect_info) const = 0;

    /**
     * @brief return connect is connect
     * @return bool true Connect succeed, false is Connect failed
     */
    virtual bool IsConnected() const = 0;

    /**
     * @brief disconnect
     */
    virtual void Disconnect() = 0;

    /**
     * @brief reconnect
     */
    virtual bool ReConnect() = 0;

    virtual CommunicateConnectState GetState() const = 0;

    /**
     * @brief write data
     * @param  data is send data, len is write data length
     * @return  write size
     */
    virtual int64_t WriteData(const char* data, int64_t len) = 0;

    /**
     * @brief write data
     * @param  data is send data
     * @return  write size
     */
    virtual int64_t WriteData(const char* data) = 0;

    /**
     * @brief write data by IP,only server side use, write to specified IP
     * @param  str_ip is ipaddress, data is send data
     * @return  write size
     */
    virtual int64_t WriteData(std::string str_ip, const char* data) { return -1; }

    virtual ConnectionInfo GetCommunicateInfo() = 0;

    /**
     * @brief call back data
     * @param  std::function<void(std::vector<char>)> , std::vector<char> is call back data
     * @return
     */
    virtual void SetDataCallback(DataCallback callback) = 0;

    /**
     * @brief call back erroe information
     * @param  std::function<void(int, const std::string&)>;, int is error index, string is error description
     * @return
     */
    virtual void SetErrorCallback(ErrorCallback callback) = 0;
};