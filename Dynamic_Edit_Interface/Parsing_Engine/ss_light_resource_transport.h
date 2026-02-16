// ss_light_resource_transport.h
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

#include "ss_light_resource_models.h"

// 传输层抽象
class SS_LightTransport
{
public:
    virtual ~SS_LightTransport() = default;

    using RxCallback = std::function<void(const std::vector<uint8_t>& bytes)>;
    using DisconnectCallback = std::function<void(const std::string& reason)>;
    using ErrorCallback = std::function<void(int code, const std::string& msg)>;

    virtual bool Connect(const SS_LightConnectionConfig& cfg, std::string& out_error) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual bool SendBytes(const std::vector<uint8_t>& bytes, std::string& out_error) = 0;// 发送原始 bytes（runtime 已经 wrap 好了）
    virtual std::vector<uint8_t> GetLastTxBytes() const = 0;// Debug：拿到最近一次发送的数据

    virtual void SetRxCallback(RxCallback cb) = 0;// 新增：runtime 用来接收“收包/断线/错误”
    virtual void SetDisconnectedCallback(DisconnectCallback cb) = 0;
    virtual void SetErrorCallback(ErrorCallback cb) = 0;
};

std::unique_ptr<SS_LightTransport> CreateDefaultLightTransport();