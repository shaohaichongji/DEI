// ss_light_resource_yaml_codec.h
#pragma once
#include <string>
#include "ss_light_resource_models.h"

#include "../include/ss_yaml_depend.h"

class SS_LightYamlCodec
{
public:
    bool LoadTemplate(const std::string& yaml_path, SS_LightControllerTemplate& out_tpl);
    bool LoadInstance(const std::string& yaml_path, SS_LightControllerInstance& out_inst);
    bool SaveInstance(const std::string& yaml_path, const SS_LightControllerInstance& inst);

    std::string GetLastError() const { return last_error_; }

private:
    std::string last_error_;

private:
    void SetError(const std::string& msg);

    // ---- 涉及yaml读取辅助工具 ----
    static bool Has(const YAML::Node& n, const char* key);
    static YAML::Node GetNode(const YAML::Node& n, const char* key);

    static std::string AsString(const YAML::Node& n);
    static std::string GetString(const YAML::Node& n, const char* key, const std::string& def = "");
    static bool GetBool(const YAML::Node& n, const char* key, bool def = false);
    static int GetInt(const YAML::Node& n, const char* key, int def = 0);
    static double GetDouble(const YAML::Node& n, const char* key, double def = 0.0);

    // schema头信息读取工具
    static std::string GetSchemaName(const YAML::Node& root);
    static std::string GetSchemaVersionAsString(const YAML::Node& root);

    // ---- 辅助工具 ----
    static SS_LIGHT_CONNECT_TYPE ParseConnectType(const std::string& s);
    static SS_LIGHT_PROTOCOL_TYPE ParseProtocolType(const std::string& s);
    static SS_LIGHT_PARAM_LOCATION ParseParamLocation(const std::string& s);
    static SS_LIGHT_VALUE_TYPE ParseValueType(const std::string& s);
    static SS_LIGHT_WIDGET_TYPE ParseWidgetType(const std::string& s);

    static SS_LIGHT_COMMAND_SCOPE ParseCommandScope(const std::string& s);
    static SS_LIGHT_COMMAND_WHEN ParseCommandWhen(const std::string& s);
    static SS_LIGHT_COMMAND_COMMIT ParseCommandCommit(const std::string& s);

    static std::string ToString(SS_LIGHT_CONNECT_TYPE v);
    static std::string ToString(SS_LIGHT_PROTOCOL_TYPE v);
    static std::string ToString(SS_LIGHT_COMMAND_WHEN v);
    static std::string ToString(SS_LIGHT_COMMAND_SCOPE v);
    static std::string ToString(SS_LIGHT_COMMAND_COMMIT v);
};