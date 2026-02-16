// ss_light_resource_transmission_wrapper.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "ss_light_resource_models.h"

class SS_LightTransmissionWrapper
{
public:
    SS_LightTransmissionWrapper() = default;

    // 输入：pdu_bytes（强约定：Factory 输出的“裸 PDU”：FunctionCode + Data）
    // 输出：frame_bytes（最终可发送帧：RTU=Addr+PDU+CRC / TCP=MBAP+UnitId+PDU）
    //
    // 约定：
    // - header_type == "ModbusTCP_MBAP"：使用 device_address 作为 UnitId，自动加 MBAP
    // - header_type == "EMPTY" + tail_type == "CRC_16_Modbus"：自动前插 device_address 再算 CRC（即 RTU）
    // - 其它组合：当前仅支持 EMPTY（原样输出），未来你自己扩展
    bool WrapPdu(
        const std::vector<uint8_t>& pdu_bytes,
        const SS_LightByteTransmissionParams& params,
        std::vector<uint8_t>& frame_bytes,
        std::string& out_error) const;

    // 解析 device_address（支持 "0x01" / "01" / "1" / "0x0001"），失败返回 false
    static bool ParseHexByte(const std::string& s, uint8_t& out_byte);

    // Modbus RTU CRC16（poly 0xA001, init 0xFFFF）
    static uint16_t ComputeCrc16Modbus(const uint8_t* data, size_t len);

private:
    static std::string ToLowerCopy(const std::string& s);

    // Modbus/TCP 生成 MBAP(7) + PDU
    // TransactionId 用静态自增（非线程安全；你真多线程再加锁）
    static bool BuildMbapAdu(
        const std::vector<uint8_t>& pdu_bytes,
        uint8_t unit_id,
        std::vector<uint8_t>& out_adu,
        std::string& out_error);
};