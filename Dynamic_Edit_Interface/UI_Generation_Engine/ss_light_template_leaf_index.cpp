// ss_light_template_leaf_index.cpp
#include "ss_light_template_leaf_index.h"

#include <algorithm>

#include "../../include/Parsing_Engine/ss_light_resource_api.h"

static std::string ConnectTypeToString(SS_LIGHT_CONNECT_TYPE t)
{
    switch (t)
    {
    case SS_LIGHT_CONNECT_TYPE::SERIAL: return "SERIAL";
    case SS_LIGHT_CONNECT_TYPE::SOCKET: return "SOCKET";
    default: return "UNKNOWN";
    }
}

bool SS_LightTemplateLeafIndex::BuildFromSystem(SS_LightResourceSystem* system, std::string& out_error)
{
    out_error.clear();
    leaves_.clear();

    if (!system)
    {
        out_error = "BuildFromSystem: system is null.";
        return false;
    }

    const std::vector<std::string> ids = system->ListTemplateIds();
    for (const auto& id : ids)
    {
        SS_LightControllerTemplate tpl;
        if (!system->GetTemplate(id, tpl))
            continue;

        // 一个模板可能支持多个 connect_types，把它“展开成多个叶子”
        if (tpl.info.connect_types.empty())
        {
            Leaf l;
            l.template_id = tpl.info.template_id;
            l.factory = tpl.info.factory;
            l.type_display = tpl.info.controller_type_display_name;
            l.model = tpl.info.controller_model;
            l.channel_max = tpl.info.channel_max;
            l.connect_type = SS_LIGHT_CONNECT_TYPE::UNKNOWN;
            leaves_.push_back(std::move(l));
            continue;
        }

        for (auto ct : tpl.info.connect_types)
        {
            Leaf l;
            l.template_id = tpl.info.template_id;
            l.factory = tpl.info.factory;
            l.type_display = tpl.info.controller_type_display_name;
            l.model = tpl.info.controller_model;
            l.channel_max = tpl.info.channel_max;
            l.connect_type = ct;
            leaves_.push_back(std::move(l));
        }
    }

    if (leaves_.empty())
    {
        out_error = "BuildFromSystem: no templates loaded. Check templates dir / YAML codec.";
        return false;
    }

    return true;
}

std::vector<std::string> SS_LightTemplateLeafIndex::ListFactories() const
{
    std::set<std::string> s;
    for (const auto& l : leaves_) s.insert(l.factory);

    return std::vector<std::string>(s.begin(), s.end());
}

std::vector<std::string> SS_LightTemplateLeafIndex::ListTypes(const std::string& factory) const
{
    std::set<std::string> s;
    for (const auto& l : leaves_)
        if (l.factory == factory)
            s.insert(l.type_display);

    return std::vector<std::string>(s.begin(), s.end());
}

std::vector<std::string> SS_LightTemplateLeafIndex::ListModels(const std::string& factory, const std::string& type_display) const
{
    std::set<std::string> s;
    for (const auto& l : leaves_)
        if (l.factory == factory && l.type_display == type_display)
            s.insert(l.model);

    return std::vector<std::string>(s.begin(), s.end());
}

std::vector<int> SS_LightTemplateLeafIndex::ListChannelMax(const std::string& factory, const std::string& type_display, const std::string& model) const
{
    std::set<int> s;
    for (const auto& l : leaves_)
        if (l.factory == factory && l.type_display == type_display && l.model == model)
            s.insert(l.channel_max);

    return std::vector<int>(s.begin(), s.end());
}

std::vector<SS_LIGHT_CONNECT_TYPE> SS_LightTemplateLeafIndex::ListConnectTypes(
    const std::string& factory, const std::string& type_display, const std::string& model, int channel_max) const
{
    std::set<int> s;
    for (const auto& l : leaves_)
    {
        if (l.factory == factory && l.type_display == type_display && l.model == model && l.channel_max == channel_max)
            s.insert((int)l.connect_type);
    }

    std::vector<SS_LIGHT_CONNECT_TYPE> out;
    out.reserve(s.size());
    for (int v : s) out.push_back((SS_LIGHT_CONNECT_TYPE)v);

    // 固定顺序：SERIAL -> SOCKET -> UNKNOWN
    std::stable_sort(out.begin(), out.end(), [](SS_LIGHT_CONNECT_TYPE a, SS_LIGHT_CONNECT_TYPE b)
    {
        auto rank = [](SS_LIGHT_CONNECT_TYPE t)
        {
            if (t == SS_LIGHT_CONNECT_TYPE::SERIAL) return 0;
            if (t == SS_LIGHT_CONNECT_TYPE::SOCKET) return 1;
            return 2;
        };
        return rank(a) < rank(b);
    });

    return out;
}

bool SS_LightTemplateLeafIndex::Match_(
    const Leaf& l,
    const std::string& factory,
    const std::string& type_display,
    const std::string& model,
    int channel_max,
    SS_LIGHT_CONNECT_TYPE connect_type)
{
    return l.factory == factory &&
        l.type_display == type_display &&
        l.model == model &&
        l.channel_max == channel_max &&
        l.connect_type == connect_type;
}

bool SS_LightTemplateLeafIndex::ResolveUniqueLeaf(
    const std::string& factory,
    const std::string& type_display,
    const std::string& model,
    int channel_max,
    SS_LIGHT_CONNECT_TYPE connect_type,
    Leaf& out_leaf,
    std::string& out_error) const
{
    out_error.clear();

    std::vector<const Leaf*> hits;
    for (const auto& l : leaves_)
    {
        if (Match_(l, factory, type_display, model, channel_max, connect_type))
            hits.push_back(&l);
    }

    if (hits.empty())
    {
        out_error = "ResolveUniqueLeaf: no template matches selection.";
        return false;
    }

    if (hits.size() > 1)
    {
        out_error = "ResolveUniqueLeaf: ambiguous selection, matched " + std::to_string(hits.size()) +
            " templates. Please make template_info unique (factory/type/model/channel_max/connect).";
        return false;
    }

    out_leaf = *hits.front();
    return true;
}
