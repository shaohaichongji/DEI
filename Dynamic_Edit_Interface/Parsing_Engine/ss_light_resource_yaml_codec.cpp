// ss_light_resource_yaml_codec.cpp
#include "ss_light_resource_yaml_codec.h"

#include <fstream>
#include <sstream>
#include <cctype>

// ----------------- public APIs -----------------

bool SS_LightYamlCodec::LoadTemplate(const std::string& yaml_path, SS_LightControllerTemplate& out_tpl)
{
    out_tpl = SS_LightControllerTemplate{};
    last_error_.clear();

    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    }
    catch (const std::exception& e) {
        SetError(std::string("LoadTemplate failed: ") + e.what());
        return false;
    }

    if (!root || !root.IsMap()) {
        SetError("LoadTemplate failed: root is not a map.");
        return false;
    }

    auto info = root["template_info"];
    if (!info || !info.IsMap()) {
        SetError("LoadTemplate failed: missing template_info.");
        return false;
    }

    // schema.version -> template_version（你的 models 里有这个字段）
    out_tpl.info.template_version = GetSchemaVersionAsString(root);
    if (out_tpl.info.template_version.empty()) out_tpl.info.template_version = "0.1";

    out_tpl.info.template_id = GetString(info, "template_id");
    out_tpl.info.factory = GetString(info, "factory");
    out_tpl.info.controller_model = GetString(info, "controller_model");
    out_tpl.info.controller_type = GetString(info, "controller_type");
    out_tpl.info.controller_type_display_name = GetString(info, "controller_type_display_name");
    out_tpl.info.channel_max = GetInt(info, "channel_max", 0);

    // connect_type
    out_tpl.info.connect_types.clear();
    auto ct = info["connect_type"];
    if (ct && ct.IsSequence()) {
        for (auto it : ct) {
            out_tpl.info.connect_types.push_back(ParseConnectType(AsString(it)));
        }
    }

    // protocol_type
    out_tpl.info.protocol_type = ParseProtocolType(GetString(info, "protocol_type", "STRING"));


    // 放在 template_info 下：template_info.byte_transmission_params
    {
        auto bt = info["byte_transmission_params"];
        if (bt && bt.IsMap())
        {
            out_tpl.info.byte_transmission_params.message_header_type =
                GetString(bt, "message_header_type", "EMPTY");
            out_tpl.info.byte_transmission_params.tail_check_type =
                GetString(bt, "tail_check_type", "EMPTY");
            out_tpl.info.byte_transmission_params.device_address =
                GetString(bt, "device_address", "0x01");
            out_tpl.info.byte_transmission_params.crc_endian =
                GetBool(bt, "crc_endian", false);
        }
        else
        {
            // 没写就保持默认值（EMPTY/EMPTY/0x01/false）
            out_tpl.info.byte_transmission_params = SS_LightByteTransmissionParams{};
        }

        // 你要是想“严格”：BYTE 协议必须指定 device_address
        // 我建议先不严格，避免配置升级期一堆旧模板跑不起来。
        // if (out_tpl.info.protocol_type == SS_LIGHT_PROTOCOL_TYPE::BYTE &&
        //     out_tpl.info.byte_transmission_params.device_address.empty())
        // {
        //     SetError("LoadTemplate failed: BYTE protocol requires template_info.byte_transmission_params.device_address.");
        //     return false;
        // }
    }


    // parameter_info
    auto pinfo = root["parameter_info"];
    if (!pinfo || !pinfo.IsMap()) {
        SetError("LoadTemplate failed: missing parameter_info.");
        return false;
    }

    out_tpl.params.clear();
    for (auto it = pinfo.begin(); it != pinfo.end(); ++it)
    {
        const std::string param_key = it->first.as<std::string>();
        const YAML::Node param_node = it->second;

        SS_LightParamDef def{};
        def.key = param_key;

        def.location = ParseParamLocation(GetString(param_node, "location", "GLOBAL"));
        def.display_name = GetString(param_node, "display_name", param_key);
        def.value_type = ParseValueType(GetString(param_node, "value_type", "STRING"));

        def.default_value = GetString(param_node, "default_value", "");
        def.widget.default_value = def.default_value; // 默认先同步

        // widget_config
        auto w = param_node["widget_config"];
        if (w && w.IsMap())
        {
            def.widget.type = ParseWidgetType(GetString(w, "type", "LINE_EDIT"));
            def.widget.enabled = GetBool(w, "enabled", true);
            def.widget.visible = GetBool(w, "visible", true);
            def.widget.read_only = GetBool(w, "read_only", false);
            def.widget.tooltip = GetString(w, "tooltip", "");
            def.widget.placeholder = GetString(w, "placeholder", "");
            def.widget.text_max_length = GetInt(w, "text_max_length", 0);
            def.widget.decimals = GetInt(w, "decimals", 0);
            def.widget.step = GetDouble(w, "step", 1.0);

            // widget default_value：如果没写则回退到 param default
            def.widget.default_value = GetString(w, "default_value", def.default_value);

            // range: [min, max]
            auto range = w["range"];
            if (range && range.IsSequence())
            {
                if (range.size() >= 2) {
                    try {
                        def.widget.min_value = range[0].as<double>();
                        def.widget.max_value = range[1].as<double>();
                    }
                    catch (...) {
                        // BOOL/ENUM 写了非数值 range 就忽略
                    }
                }
                // range.size()==0：保持默认
            }

            // options
            def.widget.options.clear();
            auto opt = w["options"];
            if (opt && opt.IsSequence()) {
                for (auto o : opt) def.widget.options.push_back(AsString(o));
            }
        }
        else
        {
            // widget_config 不存在：默认 LINE_EDIT，default_value 已同步
            def.widget.type = SS_LIGHT_WIDGET_TYPE::LINE_EDIT;
        }

        // parameter_commands
        auto cmd = param_node["parameter_commands"];
        if (cmd && cmd.IsMap())
        {
            def.command.scope = ParseCommandScope(GetString(cmd, "scope", "GLOBAL"));
            def.command.when = ParseCommandWhen(GetString(cmd, "when", "ON_CHANGE"));
            def.command.commit = ParseCommandCommit(GetString(cmd, "commit", "SAVE_AND_SEND"));
            def.command.cmd_template = GetString(cmd, "cmd_template", "");

            // placeholders
            def.command.placeholders.clear();
            auto ph = cmd["placeholders"];
            if (ph && ph.IsMap())
            {
                for (auto pit = ph.begin(); pit != ph.end(); ++pit)
                {
                    const std::string ph_key = pit->first.as<std::string>();
                    const YAML::Node ph_node = pit->second;

                    SS_LightPlaceholderRule rule{};
                    rule.source = GetString(ph_node, "source", "");
                    rule.endian = GetBool(ph_node, "endian", false);

                    rule.parser_tool = GetString(ph_node, "parser_tool", "");
                    if (rule.parser_tool.empty()) {
                        rule.parser_tool = GetString(ph_node, "paser_tool", ""); // 兼容旧拼写
                    }

                    rule.extra_param.clear();
                    auto extra = ph_node["extra_param"];
                    if (extra && extra.IsSequence()) {
                        for (auto ex : extra) rule.extra_param.push_back(AsString(ex));
                    }

                    def.command.placeholders[ph_key] = rule;
                }
            }
        }

        out_tpl.params[param_key] = def;
    }

    if (out_tpl.info.template_id.empty()) {
        SetError("LoadTemplate failed: template_info.template_id is empty.");
        return false;
    }

    return true;
}

bool SS_LightYamlCodec::LoadInstance(const std::string& yaml_path, SS_LightControllerInstance& out_inst)
{
    out_inst = SS_LightControllerInstance{};
    last_error_.clear();

    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    }
    catch (const std::exception& e) {
        SetError(std::string("LoadInstance failed: ") + e.what());
        return false;
    }

    if (!root || !root.IsMap()) {
        SetError("LoadInstance failed: root is not a map.");
        return false;
    }

    auto info = root["instance_info"];
    if (!info || !info.IsMap()) {
        SetError("LoadInstance failed: missing instance_info.");
        return false;
    }

    out_inst.info.instance_id = GetString(info, "instance_id");
    out_inst.info.template_id = GetString(info, "template_id");
    out_inst.info.display_name = GetString(info, "display_name");

    out_inst.info.enabled = GetBool(info, "enabled", true);
    out_inst.info.deleted = GetBool(info, "deleted", false);
    out_inst.info.order = GetInt(info, "order", 0);

    // connection
    auto conn = root["connection"];
    if (conn && conn.IsMap())
    {
        out_inst.connection.connect_type = ParseConnectType(GetString(conn, "connect_type", "SERIAL"));
        out_inst.connection.connect_state = GetBool(conn, "connect_state", false);

        // serial_parameter
        auto sp = conn["serial_parameter"];
        if (sp && sp.IsMap()) {
            out_inst.connection.serial_parameter.com_port_num = GetString(sp, "com_port_num", "");
            out_inst.connection.serial_parameter.baud_rate = GetInt(sp, "baud_rate", 0);
            out_inst.connection.serial_parameter.character_size = GetInt(sp, "character_size", 8);
            out_inst.connection.serial_parameter.stop_bits = GetInt(sp, "stop_bits", 1);
            out_inst.connection.serial_parameter.parity = GetInt(sp, "parity", 0);
        }

        // socket_parameter
        auto sock = conn["socket_parameter"];
        if (sock && sock.IsMap()) {
            out_inst.connection.socket_parameter.ip_address = GetString(sock, "ip_address", "");
            out_inst.connection.socket_parameter.subnet_mask = GetString(sock, "subnet_mask", "");
            out_inst.connection.socket_parameter.gateway = GetString(sock, "gateway", "");
            out_inst.connection.socket_parameter.port = GetInt(sock, "port", 0);

            out_inst.connection.socket_parameter.destination_ip_address =
                GetString(sock, "destination_ip_address", GetString(sock, "destination_ip", ""));
            out_inst.connection.socket_parameter.destination_port =
                GetInt(sock, "destination_port", 0);
        }
    }

    // channel_map
    out_inst.channels.clear();
    auto ch = root["channel_map"];
    if (ch && ch.IsSequence())
    {
        for (auto item : ch)
        {
            if (!item.IsMap()) continue;
            SS_LightChannelItem ci{};
            ci.channel_id = GetString(item, "channel_id", "");
            ci.index = GetInt(item, "index", 0);
            ci.display_name = GetString(item, "display_name", ci.channel_id);
            ci.enabled = GetBool(item, "enabled", true);
            ci.deleted = GetBool(item, "deleted", false);
            ci.order = GetInt(item, "order", 0);

            if (!ci.channel_id.empty())
                out_inst.channels.push_back(ci);
        }
    }

    // parameters_value
    out_inst.global_param_values.clear();
    out_inst.channel_param_values.clear();

    auto pv = root["parameters_value"];
    if (pv && pv.IsMap())
    {
        auto gp = pv["global_parameter"];
        if (gp && gp.IsMap()) {
            for (auto it = gp.begin(); it != gp.end(); ++it) {
                const std::string k = it->first.as<std::string>();
                out_inst.global_param_values[k] = AsString(it->second);
            }
        }

        auto cp = pv["channel_parameter"];
        if (cp && cp.IsMap())
        {
            for (auto it = cp.begin(); it != cp.end(); ++it)
            {
                const std::string param_key = it->first.as<std::string>();
                const YAML::Node channel_values = it->second;

                if (!channel_values.IsMap()) continue;

                std::unordered_map<std::string, std::string> values;
                for (auto cit = channel_values.begin(); cit != channel_values.end(); ++cit) {
                    const std::string channel_id = cit->first.as<std::string>();
                    values[channel_id] = AsString(cit->second);
                }

                out_inst.channel_param_values[param_key] = std::move(values);
            }
        }
    }

    if (out_inst.info.instance_id.empty()) {
        SetError("LoadInstance failed: instance_info.instance_id is empty.");
        return false;
    }
    if (out_inst.info.template_id.empty()) {
        SetError("LoadInstance failed: instance_info.template_id is empty.");
        return false;
    }

    return true;
}

bool SS_LightYamlCodec::SaveInstance(const std::string& yaml_path, const SS_LightControllerInstance& inst)
{
    last_error_.clear();

    YAML::Node root;
    root["schema"]["name"] = "ss_light_controller_instance";
    root["schema"]["version"] = "0.1"; // 你要是想严格跟 inst 走，也可以未来加字段

    // instance_info
    YAML::Node info;
    info["instance_id"] = inst.info.instance_id;
    info["template_id"] = inst.info.template_id;
    info["display_name"] = inst.info.display_name;
    info["enabled"] = inst.info.enabled;
    info["deleted"] = inst.info.deleted;
    info["order"] = inst.info.order;
    root["instance_info"] = info;

    // connection
    YAML::Node conn;
    conn["connect_type"] = ToString(inst.connection.connect_type);
    conn["connect_state"] = inst.connection.connect_state;

    YAML::Node sp;
    sp["com_port_num"] = inst.connection.serial_parameter.com_port_num;
    sp["baud_rate"] = inst.connection.serial_parameter.baud_rate;
    sp["character_size"] = inst.connection.serial_parameter.character_size;
    sp["stop_bits"] = inst.connection.serial_parameter.stop_bits;
    sp["parity"] = inst.connection.serial_parameter.parity;
    conn["serial_parameter"] = sp;

    YAML::Node sock;
    sock["ip_address"] = inst.connection.socket_parameter.ip_address;
    sock["subnet_mask"] = inst.connection.socket_parameter.subnet_mask;
    sock["gateway"] = inst.connection.socket_parameter.gateway;
    sock["port"] = inst.connection.socket_parameter.port;
    sock["destination_ip_address"] = inst.connection.socket_parameter.destination_ip_address;
    sock["destination_port"] = inst.connection.socket_parameter.destination_port;
    conn["socket_parameter"] = sock;

    root["connection"] = conn;

    // channel_map
    YAML::Node ch_seq(YAML::NodeType::Sequence);
    for (const auto& c : inst.channels)
    {
        YAML::Node item;
        item["channel_id"] = c.channel_id;
        item["index"] = c.index;
        item["display_name"] = c.display_name;
        item["enabled"] = c.enabled;
        item["deleted"] = c.deleted;
        item["order"] = c.order;
        ch_seq.push_back(item);
    }
    root["channel_map"] = ch_seq;

    // parameters_value
    YAML::Node pv;

    YAML::Node gp;
    for (const auto& kv : inst.global_param_values) {
        gp[kv.first] = kv.second; // 目前 value 都是 string
    }
    pv["global_parameter"] = gp;

    YAML::Node cp;
    for (const auto& pk : inst.channel_param_values)
    {
        YAML::Node m;
        for (const auto& ck : pk.second) {
            m[ck.first] = ck.second; // 目前 value 都是 string
        }
        cp[pk.first] = m;
    }
    pv["channel_parameter"] = cp;

    root["parameters_value"] = pv;

    // dump to file
    try {
        std::ofstream ofs(yaml_path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            SetError("SaveInstance failed: cannot open file: " + yaml_path);
            return false;
        }
        ofs << root;
        ofs.close();
    }
    catch (const std::exception& e) {
        SetError(std::string("SaveInstance failed: ") + e.what());
        return false;
    }

    return true;
}


// ----------------- helpers -----------------

void SS_LightYamlCodec::SetError(const std::string& msg)
{
    last_error_ = msg;
}

bool SS_LightYamlCodec::Has(const YAML::Node& n, const char* key)
{
    if (!n || !n.IsMap()) return false;
    return n[key].IsDefined();
}

YAML::Node SS_LightYamlCodec::GetNode(const YAML::Node& n, const char* key)
{
    if (!n || !n.IsMap()) return YAML::Node();
    return n[key];
}

std::string SS_LightYamlCodec::AsString(const YAML::Node& n)
{
    if (!n || !n.IsDefined()) return "";
    if (n.IsScalar()) {
        try { return n.as<std::string>(); }
        catch (...) {}
    }

    // 对 bool/int/double 也做兼容
    try {
        return n.as<std::string>();
    }
    catch (...) {
        std::stringstream ss;
        ss << n;
        return ss.str();
    }
}

std::string SS_LightYamlCodec::GetString(const YAML::Node& n, const char* key, const std::string& def)
{
    auto v = GetNode(n, key);
    if (!v || !v.IsDefined()) return def;
    return AsString(v);
}

bool SS_LightYamlCodec::GetBool(const YAML::Node& n, const char* key, bool def)
{
    auto v = GetNode(n, key);
    if (!v || !v.IsDefined()) return def;
    try { return v.as<bool>(); }
    catch (...) {
        auto s = AsString(v);
        for (auto& c : s) c = (char)std::tolower((unsigned char)c);
        if (s == "true" || s == "1") return true;
        if (s == "false" || s == "0") return false;
        return def;
    }
}

int SS_LightYamlCodec::GetInt(const YAML::Node& n, const char* key, int def)
{
    auto v = GetNode(n, key);
    if (!v || !v.IsDefined()) return def;
    try { return v.as<int>(); }
    catch (...) {
        auto s = AsString(v);
        try { return std::stoi(s); }
        catch (...) { return def; }
    }
}

double SS_LightYamlCodec::GetDouble(const YAML::Node& n, const char* key, double def)
{
    auto v = GetNode(n, key);
    if (!v || !v.IsDefined()) return def;
    try { return v.as<double>(); }
    catch (...) {
        auto s = AsString(v);
        try { return std::stod(s); }
        catch (...) { return def; }
    }
}

static std::string Upper(std::string s)
{
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

std::string SS_LightYamlCodec::GetSchemaName(const YAML::Node& root)
{
    auto schema = root["schema"];
    if (!schema || !schema.IsMap()) return "";
    return AsString(schema["name"]);
}

std::string SS_LightYamlCodec::GetSchemaVersionAsString(const YAML::Node& root)
{
    auto schema = root["schema"];
    if (!schema || !schema.IsMap()) return "";
    auto v = schema["version"];
    return AsString(v);
}

// ----------------- enum parse/toString -----------------

SS_LIGHT_CONNECT_TYPE SS_LightYamlCodec::ParseConnectType(const std::string& s)
{
    auto u = Upper(s);
    if (u == "SERIAL") return SS_LIGHT_CONNECT_TYPE::SERIAL;
    if (u == "SOCKET") return SS_LIGHT_CONNECT_TYPE::SOCKET;
    return SS_LIGHT_CONNECT_TYPE::UNKNOWN;
}

SS_LIGHT_PROTOCOL_TYPE SS_LightYamlCodec::ParseProtocolType(const std::string& s)
{
    auto u = Upper(s);
    if (u == "STRING") return SS_LIGHT_PROTOCOL_TYPE::STRING;
    if (u == "BYTE")   return SS_LIGHT_PROTOCOL_TYPE::BYTE;
    return SS_LIGHT_PROTOCOL_TYPE::UNKNOWN;
}

SS_LIGHT_PARAM_LOCATION SS_LightYamlCodec::ParseParamLocation(const std::string& s)
{
    auto u = Upper(s);
    if (u == "GLOBAL")  return SS_LIGHT_PARAM_LOCATION::GLOBAL;
    if (u == "CHANNEL") return SS_LIGHT_PARAM_LOCATION::CHANNEL;
    return SS_LIGHT_PARAM_LOCATION::UNKNOWN;
}

SS_LIGHT_VALUE_TYPE SS_LightYamlCodec::ParseValueType(const std::string& s)
{
    auto u = Upper(s);
    if (u == "INT")    return SS_LIGHT_VALUE_TYPE::INT;
    if (u == "DOUBLE") return SS_LIGHT_VALUE_TYPE::DOUBLE;
    if (u == "BOOL")   return SS_LIGHT_VALUE_TYPE::BOOL;
    if (u == "STRING") return SS_LIGHT_VALUE_TYPE::STRING;
    if (u == "ENUM")   return SS_LIGHT_VALUE_TYPE::ENUM;
    if (u == "BYTES")  return SS_LIGHT_VALUE_TYPE::BYTES;
    return SS_LIGHT_VALUE_TYPE::UNKNOWN;
}

SS_LIGHT_WIDGET_TYPE SS_LightYamlCodec::ParseWidgetType(const std::string& s)
{
    auto u = Upper(s);
    if (u == "LINE_EDIT")       return SS_LIGHT_WIDGET_TYPE::LINE_EDIT;
    if (u == "COMBO_BOX")       return SS_LIGHT_WIDGET_TYPE::COMBO_BOX;
    if (u == "SPIN_BOX")        return SS_LIGHT_WIDGET_TYPE::SPIN_BOX;
    if (u == "DOUBLE_SPIN_BOX") return SS_LIGHT_WIDGET_TYPE::DOUBLE_SPIN_BOX;
    if (u == "CHECK_BOX")       return SS_LIGHT_WIDGET_TYPE::CHECK_BOX;
    if (u == "SLIDER")          return SS_LIGHT_WIDGET_TYPE::SLIDER;
    return SS_LIGHT_WIDGET_TYPE::UNKNOWN;
}

SS_LIGHT_COMMAND_SCOPE SS_LightYamlCodec::ParseCommandScope(const std::string& s)
{
    auto u = Upper(s);
    if (u == "GLOBAL")  return SS_LIGHT_COMMAND_SCOPE::GLOBAL;
    if (u == "CHANNEL") return SS_LIGHT_COMMAND_SCOPE::CHANNEL;
    return SS_LIGHT_COMMAND_SCOPE::UNKNOWN;
}

SS_LIGHT_COMMAND_WHEN SS_LightYamlCodec::ParseCommandWhen(const std::string& s)
{
    auto u = Upper(s);
    if (u == "ON_CHANGE")  return SS_LIGHT_COMMAND_WHEN::ON_CHANGE;
    if (u == "ON_COMMIT")  return SS_LIGHT_COMMAND_WHEN::ON_COMMIT;
    if (u == "MANUAL")     return SS_LIGHT_COMMAND_WHEN::MANUAL;
    return SS_LIGHT_COMMAND_WHEN::UNKNOWN;
}

SS_LIGHT_COMMAND_COMMIT SS_LightYamlCodec::ParseCommandCommit(const std::string& s)
{
    auto u = Upper(s);
    if (u == "SEND_ONLY")     return SS_LIGHT_COMMAND_COMMIT::SEND_ONLY;
    if (u == "SAVE_ONLY")     return SS_LIGHT_COMMAND_COMMIT::SAVE_ONLY;
    if (u == "SAVE_AND_SEND") return SS_LIGHT_COMMAND_COMMIT::SAVE_AND_SEND;
    return SS_LIGHT_COMMAND_COMMIT::UNKNOWN;
}

std::string SS_LightYamlCodec::ToString(SS_LIGHT_CONNECT_TYPE v)
{
    switch (v)
    {
    case SS_LIGHT_CONNECT_TYPE::SERIAL:  return "SERIAL";
    case SS_LIGHT_CONNECT_TYPE::SOCKET:  return "SOCKET";
    default:                             return "UNKNOWN";
    }
}

std::string SS_LightYamlCodec::ToString(SS_LIGHT_PROTOCOL_TYPE v)
{
    switch (v)
    {
    case SS_LIGHT_PROTOCOL_TYPE::STRING: return "STRING";
    case SS_LIGHT_PROTOCOL_TYPE::BYTE:   return "BYTE";
    default:                             return "UNKNOWN";
    }
}

std::string SS_LightYamlCodec::ToString(SS_LIGHT_COMMAND_WHEN v)
{
    switch (v)
    {
    case SS_LIGHT_COMMAND_WHEN::ON_CHANGE: return "ON_CHANGE";
    case SS_LIGHT_COMMAND_WHEN::ON_COMMIT: return "ON_COMMIT";
    case SS_LIGHT_COMMAND_WHEN::MANUAL:    return "MANUAL";
    default:                               return "UNKNOWN";
    }
}

std::string SS_LightYamlCodec::ToString(SS_LIGHT_COMMAND_SCOPE v)
{
    switch (v)
    {
    case SS_LIGHT_COMMAND_SCOPE::GLOBAL:  return "GLOBAL";
    case SS_LIGHT_COMMAND_SCOPE::CHANNEL: return "CHANNEL";
    default:                              return "UNKNOWN";
    }
}

std::string SS_LightYamlCodec::ToString(SS_LIGHT_COMMAND_COMMIT v)
{
    switch (v)
    {
    case SS_LIGHT_COMMAND_COMMIT::SEND_ONLY:     return "SEND_ONLY";
    case SS_LIGHT_COMMAND_COMMIT::SAVE_ONLY:     return "SAVE_ONLY";
    case SS_LIGHT_COMMAND_COMMIT::SAVE_AND_SEND: return "SAVE_AND_SEND";
    default:                                     return "UNKNOWN";
    }
}
