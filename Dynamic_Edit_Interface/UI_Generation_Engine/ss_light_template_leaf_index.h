// ss_light_template_leaf_index.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <set>

#include "../../include/Parsing_Engine/ss_light_resource_models.h"
#include "../../include/Parsing_Engine/ss_light_resource_types.h"

// forward declare
class SS_LightResourceSystem;

class SS_LightTemplateLeafIndex
{
public:
    struct Leaf
    {
        std::string template_id;

        std::string factory;
        std::string type_display;   // controller_type_display_name
        std::string model;
        int channel_max = 0;

        SS_LIGHT_CONNECT_TYPE connect_type = SS_LIGHT_CONNECT_TYPE::UNKNOWN;
    };

public:
    bool BuildFromSystem(SS_LightResourceSystem* system, std::string& out_error);

    std::vector<std::string> ListFactories() const;
    std::vector<std::string> ListTypes(const std::string& factory) const;
    std::vector<std::string> ListModels(const std::string& factory, const std::string& type_display) const;
    std::vector<int> ListChannelMax(const std::string& factory, const std::string& type_display, const std::string& model) const;
    std::vector<SS_LIGHT_CONNECT_TYPE> ListConnectTypes(const std::string& factory, const std::string& type_display, const std::string& model, int channel_max) const;

    // 解析唯一叶子：如果匹配 0 或 >1，返回 false，并给出 out_error
    bool ResolveUniqueLeaf(
        const std::string& factory,
        const std::string& type_display,
        const std::string& model,
        int channel_max,
        SS_LIGHT_CONNECT_TYPE connect_type,
        Leaf& out_leaf,
        std::string& out_error) const;

    const std::vector<Leaf>& GetLeaves() const { return leaves_; }

private:
    static bool Match_(
        const Leaf& l,
        const std::string& factory,
        const std::string& type_display,
        const std::string& model,
        int channel_max,
        SS_LIGHT_CONNECT_TYPE connect_type);

private:
    std::vector<Leaf> leaves_;
};
