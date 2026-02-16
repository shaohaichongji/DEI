#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ss_light_resource_types.h"

// BYTE模式下数据包封装参数，如：ModbusTCP的MBAP头，CRC/LRC校验等
struct SS_LightByteTransmissionParams
{
    std::string message_header_type; // EMPTY / ModbusTCP_MBAP ...
    std::string tail_check_type;     // EMPTY / CRC_16_Modbus / LRC ...
    std::string device_address;      // 格式如下："0x01"
    bool crc_endian = false;
};

// 模板模型信息，包含控制器层面的信息
struct SS_LightTemplateInfo
{
    std::string template_id;
    std::string template_version;

    std::string factory;
    std::string controller_model;

    std::string controller_type;
    std::string controller_type_display_name;

    int channel_max = 0;

    std::vector<SS_LIGHT_CONNECT_TYPE> connect_types;
    SS_LIGHT_PROTOCOL_TYPE protocol_type = SS_LIGHT_PROTOCOL_TYPE::UNKNOWN;

    SS_LightByteTransmissionParams byte_transmission_params;
};

struct SS_LightControllerTemplate
{
    SS_LightTemplateInfo info;

    // key -> param definition
    std::unordered_map<std::string, SS_LightParamDef> params;

    // optional UI hints (keep minimal now)
    std::vector<std::string> global_order;
    std::vector<std::string> channel_order;
};

// 实例模型信息
struct SS_LightInstanceInfo
{
    std::string instance_id;
    std::string template_id;
    std::string display_name;

    bool enabled = true;
    bool deleted = false;
    int order = 0;
};

struct SS_LightSerialConfig
{
    std::string com_port_num;   // COM1
    int baud_rate = 0;
    int character_size = 8;
    int stop_bits = 1;
    int parity = 0;
};

struct SS_LightSocketConfig
{
    std::string ip_address;
    std::string subnet_mask;
    std::string gateway;
    int port = 0;

    std::string destination_ip_address;
    int destination_port = 0;
};

// 控制器通讯连接模型
struct SS_LightConnectionConfig
{
    SS_LIGHT_CONNECT_TYPE connect_type = SS_LIGHT_CONNECT_TYPE::UNKNOWN;
    bool connect_state = false;

    SS_LightSerialConfig serial_parameter;
    SS_LightSocketConfig socket_parameter;
};

struct SS_LightControllerInstance
{
    SS_LightInstanceInfo info;
    SS_LightConnectionConfig connection;

    // 控制器列表中已经添加的通道（光源）子条目
    std::vector<SS_LightChannelItem> channels;

    // 全局通道的值（key -> value_str）
    std::unordered_map<std::string, std::string> global_param_values;

    // 通道级参数值(channel_id -> value_str)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> channel_param_values;
};
