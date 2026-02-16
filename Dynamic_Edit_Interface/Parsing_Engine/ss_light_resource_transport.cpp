// ss_light_resource_transport.cpp
#include "ss_light_resource_transport.h"

#include <sstream>
#include <atomic>
#include <mutex>

#include "../../include/Communication_Library/ss_communicate_interface.h"
#include "../../include/Communication_Library/ss_communicate_library.h"

#ifdef _DEBUG
#pragma comment(lib, "../../lib/Debug/Communication_Library.lib")
#else
#pragma comment(lib, "../../lib/Release/Communication_Library.lib")
#endif

static std::string MakeErr_(const std::string& prefix, const std::string& detail)
{
    if (detail.empty()) return prefix;
    return prefix + ": " + detail;
}

static ConnectionInfo ToConnectionInfo_(const SS_LightConnectionConfig& cfg, std::string& out_error)
{
    out_error.clear();
    ConnectionInfo ci;

    if (cfg.connect_type == SS_LIGHT_CONNECT_TYPE::SOCKET)
    {
        ci.ip_ = cfg.socket_parameter.destination_ip_address;
        ci.port_ = cfg.socket_parameter.destination_port;

        if (ci.ip_.empty())
        {
            out_error = "socket destination_ip_address is empty.";
            return ci;
        }
        if (ci.port_ <= 0 || ci.port_ > 65535)
        {
            out_error = "socket destination_port is invalid.";
            return ci;
        }
        return ci;
    }

    if (cfg.connect_type == SS_LIGHT_CONNECT_TYPE::SERIAL)
    {
        // 这里假设你的 ConnectionInfo 已经扩展了这些字段
        ci.com_port_ = cfg.serial_parameter.com_port_num;
        ci.baud_rate_ = cfg.serial_parameter.baud_rate;
        ci.character_size_ = cfg.serial_parameter.character_size;
        ci.stop_bits_ = cfg.serial_parameter.stop_bits;
        ci.parity_ = cfg.serial_parameter.parity;

        if (ci.com_port_.empty())
        {
            out_error = "serial com_port_num is empty.";
            return ci;
        }
        if (ci.baud_rate_ <= 0)
        {
            out_error = "serial baud_rate is invalid.";
            return ci;
        }
        if (ci.character_size_ <= 0) ci.character_size_ = 8;
        if (ci.stop_bits_ <= 0) ci.stop_bits_ = 1;

        return ci;
    }

    out_error = "connect_type is UNKNOWN.";
    return ci;
}

// 继承SS_LightTransport
class SS_LightCommunicateTransport final : public SS_LightTransport
{
public:
    SS_LightCommunicateTransport() = default;

    ~SS_LightCommunicateTransport() override
    {
        Disconnect();
        std::lock_guard<std::mutex> lk(cb_mtx_);
        rx_cb_ = nullptr;
        disc_cb_ = nullptr;
        err_cb_ = nullptr;
    }

    bool Connect(const SS_LightConnectionConfig& cfg, std::string& out_error) override
    {
        out_error.clear();
        last_tx_.clear();

        CommunicateType ct;
        if (cfg.connect_type == SS_LIGHT_CONNECT_TYPE::SOCKET)
            ct = CommunicateType::TCP_CLIENT;
        else if (cfg.connect_type == SS_LIGHT_CONNECT_TYPE::SERIAL)
            ct = CommunicateType::SERIAL;
        else
        {
            out_error = "Connect: unsupported connect_type (UNKNOWN).";
            PublishError_(1001, out_error);
            connected_.store(false);
            return false;
        }

        std::string map_err;
        ConnectionInfo ci = ToConnectionInfo_(cfg, map_err);
        if (!map_err.empty())
        {
            out_error = MakeErr_("Connect: invalid connection config", map_err);
            PublishError_(1002, out_error);
            connected_.store(false);
            return false;
        }

        if (!comm_ || comm_type_ != ct)
        {
            comm_.reset();
            comm_ = CommunicateLibrary::Instance().CreateCommunicateFactory(ct);
            comm_type_ = ct;

            if (!comm_)
            {
                out_error = "Connect: CreateCommunicateFactory failed (nullptr).";
                PublishError_(1003, out_error);
                connected_.store(false);
                return false;
            }

            comm_->Init();
        }

        // 每次 Connect 都重新绑回调，防止底层换对象/重连丢回调
        comm_->SetDataCallback([this](std::string /*ip*/, std::vector<char> data) {
            if (data.empty()) return;

            std::vector<uint8_t> bytes;
            bytes.reserve(data.size());
            for (char c : data) bytes.push_back(static_cast<uint8_t>(c));
            PublishRx_(bytes);
        });

        comm_->SetErrorCallback([this](int code, const std::string& msg) {
            PublishError_(code, msg);

            if (ShouldTreatAsDisconnect_(code, msg))
            {
                const bool was_connected = connected_.exchange(false);
                if (was_connected)
                    PublishDisconnected_(msg.empty() ? "disconnected by error" : msg);
            }
        });

        const bool ok = comm_->Connect(ci);
        if (!ok)
        {
            out_error = "Connect: communicator connect failed.";
            connected_.store(false);
            PublishError_(1004, out_error);
            return false;
        }

        connected_.store(true);
        return true;
    }

    void Disconnect() override
    {
        const bool was_connected = connected_.exchange(false);

        if (comm_)
            comm_->Disconnect();

        if (was_connected)
            PublishDisconnected_("manual disconnect");
    }

    bool IsConnected() const override
    {
        return connected_.load();
    }

    bool SendBytes(const std::vector<uint8_t>& bytes, std::string& out_error) override
    {
        out_error.clear();

        if (!comm_)
        {
            out_error = "SendBytes: communicator is null (not connected).";
            PublishError_(2001, out_error);
            return false;
        }
        if (!connected_.load())
        {
            out_error = "SendBytes: not connected.";
            PublishError_(2002, out_error);
            return false;
        }
        if (bytes.empty())
        {
            out_error = "SendBytes: bytes is empty.";
            PublishError_(2003, out_error);
            return false;
        }

        last_tx_ = bytes;

        const int64_t n = comm_->WriteData(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<int64_t>(bytes.size()));

        if (n < 0 || n != static_cast<int64_t>(bytes.size()))
        {
            std::ostringstream oss;
            oss << "SendBytes: write failed, write_size=" << n
                << ", expect=" << bytes.size();
            out_error = oss.str();
            PublishError_(2004, out_error);
            return false;
        }

        return true;
    }

    std::vector<uint8_t> GetLastTxBytes() const override
    {
        return last_tx_;
    }

    void SetRxCallback(RxCallback cb) override
    {
        std::lock_guard<std::mutex> lk(cb_mtx_);
        rx_cb_ = std::move(cb);
    }

    void SetDisconnectedCallback(DisconnectCallback cb) override
    {
        std::lock_guard<std::mutex> lk(cb_mtx_);
        disc_cb_ = std::move(cb);
    }

    void SetErrorCallback(ErrorCallback cb) override
    {
        std::lock_guard<std::mutex> lk(cb_mtx_);
        err_cb_ = std::move(cb);
    }

private:
    static bool ShouldTreatAsDisconnect_(int code, const std::string& msg)
    {
        std::string m = msg;
        for (auto& c : m) c = (char)std::tolower((unsigned char)c);

        if (m.find("eof") != std::string::npos) return true;
        if (m.find("connection reset") != std::string::npos) return true;
        if (m.find("broken pipe") != std::string::npos) return true;
        if (m.find("disconnect") != std::string::npos) return true;
        if (m.find("closed") != std::string::npos) return true;

        (void)code;
        return false;
    }

    void PublishError_(int code, const std::string& msg)
    {
        ErrorCallback cb;
        {
            std::lock_guard<std::mutex> lk(cb_mtx_);
            cb = err_cb_;
        }
        if (cb) cb(code, msg);
    }

    void PublishDisconnected_(const std::string& reason)
    {
        DisconnectCallback cb;
        {
            std::lock_guard<std::mutex> lk(cb_mtx_);
            cb = disc_cb_;
        }
        if (cb) cb(reason);
    }

    void PublishRx_(const std::vector<uint8_t>& bytes)
    {
        RxCallback cb;
        {
            std::lock_guard<std::mutex> lk(cb_mtx_);
            cb = rx_cb_;
        }
        if (cb) cb(bytes);
    }

private:
    std::shared_ptr<CommunicateInterface> comm_;
    CommunicateType comm_type_{ CommunicateType::TCP_CLIENT };

    std::atomic_bool connected_{ false };

    std::vector<uint8_t> last_tx_;

    mutable std::mutex cb_mtx_;
    RxCallback rx_cb_;
    DisconnectCallback disc_cb_;
    ErrorCallback err_cb_;
};

std::unique_ptr<SS_LightTransport> CreateDefaultLightTransport()
{
    return std::make_unique<SS_LightCommunicateTransport>();
}
