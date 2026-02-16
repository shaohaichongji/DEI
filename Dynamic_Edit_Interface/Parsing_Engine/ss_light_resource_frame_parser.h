// ss_light_resource_frame_parser.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ss_light_resource_transmission_wrapper.h" // SS_LightByteTransmissionParams

class SS_LightFrameParser
{
public:
    SS_LightFrameParser() = default;

    void Reset();

    // 输入：新收到的一段 bytes（来自串口/TCP）
    // 输出：解析出的完整 frame 列表
    //
    // 说明：
    // - ModbusTCP_MBAP：严格按 MBAP Length 切包（推荐）
    // - CRC_16_Modbus：无长度字段，只能 CRC 扫描切包（保底）
    // - EMPTY：无法判断边界，则把当前缓存视作一个 frame（仅用于冒烟）
    bool Feed(
        const uint8_t* data,
        size_t len,
        const SS_LightByteTransmissionParams& params,
        std::vector<std::vector<uint8_t>>& out_frames,
        std::string& out_error);

    // 从 frame 中提取“payload”
    // - ModbusTCP_MBAP：返回 PDU（FunctionCode+Data）
    // - RTU+CRC：返回 Addr+PDU（去掉 CRC）
    // - EMPTY：payload 就是 frame
    bool ExtractPayload(
        const std::vector<uint8_t>& frame,
        const SS_LightByteTransmissionParams& params,
        std::vector<uint8_t>& out_payload,
        std::string& out_error) const;

    static bool CheckCrc16Modbus(const std::vector<uint8_t>& frame, bool crc_endian);

private:
    static std::string ToLowerCopy(const std::string& s);

    bool TryExtractOneMbapFrame(
        std::vector<uint8_t>& out_frame,
        std::string& out_error);

    bool TryExtractOneCrcFrame(
        bool crc_endian,
        std::vector<uint8_t>& out_frame);

    static bool ReadMbapLength(const uint8_t* mbap7, uint16_t& out_len);

private:
    std::vector<uint8_t> rx_buffer_;
};