// ss_light_resource_manager.cpp
#include "ss_light_resource_manager.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <regex>
#include <vector>

namespace fs = std::filesystem;

static bool EndsWith(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

static bool StartsWith(const std::string& s, const std::string& prefix)
{
    if (s.size() < prefix.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), s.begin());
}

static bool IsDigits(const std::string& s)
{
    if (s.empty()) return false;
    for (unsigned char c : s)
        if (!std::isdigit(c)) return false;
    return true;
}

static std::string PickDefaultValueForParam(const SS_LightParamDef& def)
{
    if (!def.default_value.empty()) return def.default_value;
    if (!def.widget.default_value.empty()) return def.widget.default_value;
    return std::string{};
}

SS_LightResourceManager::SS_LightResourceManager(std::string template_dir, std::string instance_dir)
    : template_dir_(std::move(template_dir))
    , instance_dir_(std::move(instance_dir))
{
}

bool SS_LightResourceManager::Init(std::string& out_error)
{
    out_error.clear();

    try
    {
        if (template_dir_.empty())
        {
            //out_error = "Init: template_dir_ is empty.";
            out_error = "初始化：模板目录为空。";
            return false;
        }
        if (instance_dir_.empty())
        {
            //out_error = "Init: instance_dir_ is empty.";
            out_error = "初始化：实例目录为空。";
            return false;
        }

        fs::path tdir(template_dir_);
        fs::path idir(instance_dir_);

        if (!fs::exists(tdir)) fs::create_directories(tdir);
        if (!fs::exists(idir)) fs::create_directories(idir);

        if (!fs::is_directory(tdir))
        {
            //out_error = "Init: template_dir_ exists but is not a directory: " + template_dir_;
            out_error = "初始信息：模板目录已存在，但并非一个目录：" + template_dir_;
            return false;
        }
        if (!fs::is_directory(idir))
        {
            //out_error = "Init: instance_dir_ exists but is not a directory: " + instance_dir_;
            out_error = "初始状态：实例目录已存在，但并非为目录：" + instance_dir_;
            return false;
        }
    }
    catch (const std::exception& e)
    {
        out_error = std::string("Init: filesystem error: ") + e.what();
        return false;
    }

    if (!ReloadTemplates(out_error))
        return false;
    if (!ReloadInstances(out_error))
        return false;

    return true;
}

void SS_LightResourceManager::Shutdown()
{
    runtimes_.clear();
    templates_.clear();
    instance_paths_.clear();
}

bool SS_LightResourceManager::LoadTemplateFile(const std::string& template_yaml_path, std::string& out_error)
{
    out_error.clear();

    if (template_yaml_path.empty())
    {
        //out_error = "LoadTemplateFile: template_yaml_path is empty.";
        out_error = "加载模板文件：模板 YAML 路径为空。";
        return false;
    }

    fs::path p(template_yaml_path);
    if (!fs::exists(p))
    {
        //out_error = "LoadTemplateFile: file not exists: " + template_yaml_path;
        out_error = "加载模板文件：该文件不存在：" + template_yaml_path;
        return false;
    }

    if (!fs::is_regular_file(p))
    {
        //out_error = "LoadTemplateFile: not a regular file: " + template_yaml_path;
        out_error = "加载模板文件：非常规文件：" + template_yaml_path;
        return false;
    }

    const std::string fname = p.filename().string();
    if (!EndsWith(fname, "_template.yaml"))
    {
        //out_error = "LoadTemplateFile: filename must end with '_template.yaml': " + fname;
        out_error = "加载模板文件：文件名必须以“_template.yaml”结尾：" + fname;
        return false;
    }

    SS_LightControllerTemplate tpl;
    if (!codec_.LoadTemplate(template_yaml_path, tpl))
    {
        //out_error = "LoadTemplateFile: LoadTemplate failed: " + template_yaml_path +
        " | codec error: " + codec_.GetLastError();
        out_error = "加载模板文件：加载模板失败：" + template_yaml_path +
            " | 编码器错误：" + codec_.GetLastError();
        return false;
    }

    if (tpl.info.template_id.empty())
    {
        //out_error = "LoadTemplateFile: template_id is empty in file: " + template_yaml_path;
        out_error = "加载模板文件：文件中模板 ID 为空，文件路径：" + template_yaml_path;
        return false;
    }

    templates_[tpl.info.template_id] = std::move(tpl);
    return true;
}

bool SS_LightResourceManager::ReloadTemplates(std::string& out_error)
{
    out_error.clear();

    fs::path dir(template_dir_);
    if (template_dir_.empty())
    {
        //out_error = "ReloadTemplates: template_dir_ is empty.";
        out_error = "重新加载模板：模板目录为空。";
        return false;
    }
    if (!fs::exists(dir))
    {
        //out_error = "ReloadTemplates: template_dir_ not exists: " + template_dir_;
        out_error = "重新加载模板：模板目录“template_dir_”不存在：" + template_dir_;
        return false;
    }
    if (!fs::is_directory(dir))
    {
        //out_error = "ReloadTemplates: template_dir_ is not a directory: " + template_dir_;
        out_error = "重新加载模板：模板目录“template_dir_”不是一个目录：" + template_dir_;
        return false;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        const std::string fname = entry.path().filename().string();
        if (EndsWith(fname, "_template.yaml"))
            files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());

    std::unordered_map<std::string, SS_LightControllerTemplate> new_templates;

    for (const auto& p : files)
    {
        SS_LightControllerTemplate tpl;
        const std::string path_str = p.string();

        if (!codec_.LoadTemplate(path_str, tpl))
        {
            //out_error = "ReloadTemplates: LoadTemplate failed: " + path_str +
            //    " | codec error: " + codec_.GetLastError();
            out_error = "重新加载模板：加载模板失败：" + path_str +
                " | 编码器错误：" + codec_.GetLastError();
            return false;
        }

        if (tpl.info.template_id.empty())
        {
            //out_error = "ReloadTemplates: template_id is empty in file: " + path_str;
            out_error = "重新加载模板：文件中“模板 ID”字段为空：" + path_str;
            return false;
        }

        new_templates[tpl.info.template_id] = std::move(tpl);
    }

    templates_ = std::move(new_templates);
    return true;
}

std::vector<std::string> SS_LightResourceManager::ListTemplateIds() const
{
    std::vector<std::string> ids;
    ids.reserve(templates_.size());
    for (const auto& kv : templates_)
        ids.push_back(kv.first);

    std::sort(ids.begin(), ids.end());
    return ids;
}

bool SS_LightResourceManager::GetTemplate(const std::string& template_id, SS_LightControllerTemplate& out_tpl) const
{
    auto it = templates_.find(template_id);
    if (it == templates_.end())
        return false;
    out_tpl = it->second;
    return true;
}

bool SS_LightResourceManager::LoadInstanceFile(const std::string& instance_yaml_path, std::string& out_error)
{
    out_error.clear();

    if (instance_yaml_path.empty())
    {
        //out_error = "LoadInstanceFile: instance_yaml_path is empty.";
        out_error = "加载实例文件：实例 YAML 文件路径为空。";
        return false;
    }

    fs::path p(instance_yaml_path);
    if (!fs::exists(p))
    {
        //out_error = "LoadInstanceFile: file not exists: " + instance_yaml_path;
        out_error = "加载实例文件：文件不存在： " + instance_yaml_path;
        return false;
    }

    if (!fs::is_regular_file(p))
    {
        //out_error = "LoadInstanceFile: not a regular file: " + instance_yaml_path;
        out_error = "加载实例文件：不是常规文件：" + instance_yaml_path;
        return false;
    }

    // 兼容：这里不再强行限制文件名必须 controller_*
    SS_LightControllerInstance inst;
    if (!codec_.LoadInstance(instance_yaml_path, inst))
    {
        //out_error = "LoadInstanceFile: LoadInstance failed: " + instance_yaml_path +
        //    " | codec error: " + codec_.GetLastError();
        out_error = "加载实例文件：加载实例失败：" + instance_yaml_path +
            " | 编码器错误：" + codec_.GetLastError();
        return false;
    }

    if (!AddOrReplaceInstance(inst, out_error))
        return false;

    // 记录路径：instance_id -> yaml
    instance_paths_[inst.info.instance_id] = fs::absolute(p).string();
    return true;
}

bool SS_LightResourceManager::ReloadInstances(std::string& out_error)
{
    out_error.clear();

    fs::path dir(instance_dir_);
    if (instance_dir_.empty())
    {
        //out_error = "ReloadInstances: instance_dir_ is empty.";
        out_error = "重新加载实例：实例目录为空。";
        return false;
    }
    if (!fs::exists(dir))
    {
        //out_error = "ReloadInstances: instance_dir_ not exists: " + instance_dir_;
        out_error = "重新加载实例：实例目录不存在：" + instance_dir_;
        return false;
    }
    if (!fs::is_directory(dir))
    {
        //out_error = "ReloadInstances: instance_dir_ is not a directory: " + instance_dir_;
        out_error = "重新加载实例：实例目录不是一个目录：" + instance_dir_;
        return false;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;

        // 兼容：只要是 *_instance.yaml 都加载（如果只想加载 controller_*，这里再收窄）
        const std::string fname = entry.path().filename().string();
        if (EndsWith(fname, "_instance.yaml"))
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    std::unordered_map<std::string, std::unique_ptr<SS_LightControllerRuntime>> new_runtimes;
    std::unordered_map<std::string, std::string> new_paths;

    for (const auto& p : files)
    {
        const std::string path_str = p.string();

        SS_LightControllerInstance inst;
        if (!codec_.LoadInstance(path_str, inst))
        {
            /*out_error = "ReloadInstances: LoadInstance failed: " + path_str +
                " | codec error: " + codec_.GetLastError();*/
            out_error = "重新加载实例：加载实例失败：" + path_str +
                " | 编码器错误：" + codec_.GetLastError();
            return false;
        }

        if (inst.info.instance_id.empty())
        {
            //out_error = "ReloadInstances: instance_id is empty in file: " + path_str;
            out_error = "重新加载实例：实例 ID 在文件中为空：" + path_str;
            return false;
        }
        if (inst.info.template_id.empty())
        {
            /*out_error = "ReloadInstances: template_id is empty in file: " + path_str;*/
            out_error = "重新加载实例：模板 ID 在文件中为空：" + path_str;
            return false;
        }

        const SS_LightControllerTemplate* tpl = FindTemplate(inst.info.template_id);
        if (!tpl)
        {
            /*out_error = "ReloadInstances: template not found for instance_id=" + inst.info.instance_id +
                " template_id=" + inst.info.template_id + " file=" + path_str;*/
            out_error = "重新加载实例：未找到模板，实例 ID=" + inst.info.instance_id +
                " 模板 ID=" + inst.info.template_id + " 文件=" + path_str;
            return false;
        }

        auto rt = std::make_unique<SS_LightControllerRuntime>();
        rt->BindTemplate(*tpl);
        rt->BindInstance(inst);
        rt->SetEventBus(event_bus_);

        if (new_runtimes.find(inst.info.instance_id) != new_runtimes.end())
        {
            /*out_error = "ReloadInstances: duplicate instance_id: " + inst.info.instance_id +
                " (file=" + path_str + ")";*/
            out_error = "重新加载实例：重复的实例 ID：" + inst.info.instance_id +
                " (file=" + path_str + ")";
            return false;
        }

        new_runtimes[inst.info.instance_id] = std::move(rt);
        new_paths[inst.info.instance_id] = fs::absolute(p).string();
    }

    runtimes_ = std::move(new_runtimes);
    instance_paths_ = std::move(new_paths);
    return true;
}

bool SS_LightResourceManager::AddOrReplaceInstance(const SS_LightControllerInstance& inst, std::string& out_error)
{
    out_error.clear();

    if (inst.info.instance_id.empty())
    {
        //out_error = "Instance instance_id is empty.";
        out_error = "实例 ID 为空。";
        return false;
    }
    if (inst.info.template_id.empty())
    {
        /*out_error = "Instance template_id is empty.";*/
        out_error = "实例模板 ID 为空。";
        return false;
    }

    const SS_LightControllerTemplate* tpl = FindTemplate(inst.info.template_id);
    if (!tpl)
    {
        /*out_error = "Template not found: " + inst.info.template_id;*/
        out_error = "未找到模板：" + inst.info.template_id;
        return false;
    }

    auto rt = std::make_unique<SS_LightControllerRuntime>();
    rt->BindTemplate(*tpl);
    rt->BindInstance(inst);
    rt->SetEventBus(event_bus_);

    runtimes_[inst.info.instance_id] = std::move(rt);
    return true;
}

bool SS_LightResourceManager::RemoveInstance(const std::string& instance_id)
{
    instance_paths_.erase(instance_id);
    return runtimes_.erase(instance_id) > 0;
}

std::vector<std::string> SS_LightResourceManager::ListInstanceIds() const
{
    std::vector<std::string> ids;
    ids.reserve(runtimes_.size());
    for (const auto& kv : runtimes_)
        ids.push_back(kv.first);

    std::sort(ids.begin(), ids.end());
    return ids;
}

bool SS_LightResourceManager::GetInstance(const std::string& instance_id, SS_LightControllerInstance& out_inst) const
{
    const SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;
    out_inst = rt->GetInstance();
    return true;
}

bool SS_LightResourceManager::RenameController(const std::string& instance_id, const std::string& new_display_name)
{
    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;
    rt->GetInstanceMutable().info.display_name = new_display_name;
    return true;
}

bool SS_LightResourceManager::SetControllerEnabled(const std::string& instance_id, bool enabled)
{
    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;
    rt->GetInstanceMutable().info.enabled = enabled;
    return true;
}

bool SS_LightResourceManager::SetControllerOrder(const std::string& instance_id, int order)
{
    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;
    rt->GetInstanceMutable().info.order = order;
    return true;
}

bool SS_LightResourceManager::AddChannel(
    const std::string& instance_id,
    int channel_index,
    const std::string& channel_display_name,
    std::string& out_channel_id)
{
    out_channel_id.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;

    auto& inst = rt->GetInstanceMutable();

    const SS_LightControllerTemplate* tpl = FindTemplate(inst.info.template_id);
    if (!tpl) return false;

    const int channel_max = tpl->info.channel_max;
    if (channel_max <= 0) return false;

    if (channel_index < 0 || channel_index >= channel_max)
        return false;

    if (static_cast<int>(inst.channels.size()) >= channel_max)
        return false;

    for (const auto& ch : inst.channels)
    {
        if (ch.index == channel_index)
            return false;
    }

    std::string base = "ch_" + std::to_string(channel_index);
    std::string cid = base;

    auto exists_id = [&](const std::string& id) {
        for (const auto& ch : inst.channels)
            if (ch.channel_id == id) return true;
        return false;
    };

    if (exists_id(cid))
    {
        int suffix = 1;
        while (true)
        {
            std::string trial = base + "_" + std::to_string(suffix++);
            if (!exists_id(trial)) { cid = std::move(trial); break; }
        }
    }

    SS_LightChannelItem item;
    item.channel_id = cid;
    item.index = channel_index;
    item.display_name = channel_display_name.empty()
        ? ("CH" + std::to_string(channel_index + 1))
        : channel_display_name;

    item.enabled = true;
    item.deleted = false;
    item.order = channel_index;

    inst.channels.push_back(item);

    for (const auto& kv : tpl->params)
    {
        const SS_LightParamDef& def = kv.second;
        if (def.location != SS_LIGHT_PARAM_LOCATION::CHANNEL) continue;

        const std::string key = def.key.empty() ? kv.first : def.key;
        auto& per_channel = inst.channel_param_values[key];
        if (per_channel.find(cid) == per_channel.end())
            per_channel[cid] = PickDefaultValueForParam(def);
    }

    out_channel_id = cid;
    return true;
}

bool SS_LightResourceManager::RenameChannel(
    const std::string& instance_id,
    const std::string& channel_id,
    const std::string& new_display_name)
{
    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;

    auto& inst = rt->GetInstanceMutable();
    for (auto& ch : inst.channels)
    {
        if (ch.channel_id == channel_id)
        {
            ch.display_name = new_display_name;
            return true;
        }
    }
    return false;
}

bool SS_LightResourceManager::RemoveChannel(const std::string& instance_id, const std::string& channel_id)
{
    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;

    auto& inst = rt->GetInstanceMutable();

    auto old_size = inst.channels.size();
    inst.channels.erase(
        std::remove_if(inst.channels.begin(), inst.channels.end(),
            [&](const SS_LightChannelItem& c) { return c.channel_id == channel_id; }),
        inst.channels.end());

    if (inst.channels.size() == old_size)
        return false;

    CleanupChannelParamValues(inst, channel_id);
    return true;
}

bool SS_LightResourceManager::SetChannelIndex(const std::string& instance_id,
    const std::string& channel_id,
    int new_channel_index,
    std::string& out_error)
{
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "SetChannelIndex: instance not found: " + instance_id;*/
        out_error = "设置通道索引：实例未找到：" + instance_id;
        return false;
    }

    auto& inst = rt->GetInstanceMutable();

    const SS_LightControllerTemplate* tpl = FindTemplate(inst.info.template_id);
    if (!tpl)
    {
        /*out_error = "SetChannelIndex: template not found: " + inst.info.template_id;*/
        out_error = "设置通道索引：未找到模板：" + inst.info.template_id;
        return false;
    }

    const int channel_max = tpl->info.channel_max;
    if (channel_max <= 0)
    {
        /*out_error = "SetChannelIndex: template channel_max <= 0.";*/
        out_error = "设置通道索引：模板 channel_max <= 0.";
        return false;
    }

    if (new_channel_index < 0 || new_channel_index >= channel_max)
    {
        /*out_error = "SetChannelIndex: new_channel_index out of range: " + std::to_string(new_channel_index);*/
        out_error = "设置通道索引：new_channel_index 超出范围：" + std::to_string(new_channel_index);
        return false;
    }

    // 找到要改的通道
    SS_LightChannelItem* target = nullptr;
    for (auto& ch : inst.channels)
    {
        if (ch.channel_id == channel_id)
        {
            target = &ch;
            break;
        }
    }
    if (!target)
    {
        /*out_error = "SetChannelIndex: channel_id not found: " + channel_id;*/
        out_error = "设置通道索引：未找到通道 ID：" + channel_id;
        return false;
    }

    // 若没变，直接成功
    if (target->index == new_channel_index)
        return true;

    // 冲突检查：是否被其它通道占用
    for (const auto& ch : inst.channels)
    {
        if (ch.channel_id != channel_id && ch.index == new_channel_index)
        {
            /*out_error = "SetChannelIndex: new index already used by channel_id=" + ch.channel_id;*/
            out_error = "设置通道索引：新索引已被通道 ID=" + ch.channel_id + " 占用";
            return false;
        }
    }

    // 更新 index
    target->index = new_channel_index;
    target->order = new_channel_index; // 可选：保持排序一致

    return true;
}

bool SS_LightResourceManager::AddDefaultChannel(
    const std::string& instance_id,
    std::string& out_channel_id,
    std::string& out_error)
{
    out_channel_id.clear();
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "AddDefaultChannel: instance not found: " + instance_id;*/
        out_error = "添加默认通道：未找到实例：" + instance_id;
        return false;
    }

    auto& inst = rt->GetInstanceMutable();

    const SS_LightControllerTemplate* tpl = FindTemplate(inst.info.template_id);
    if (!tpl)
    {
        /*out_error = "AddDefaultChannel: template not found: " + inst.info.template_id;*/
        out_error = "添加默认通道：未找到模板：" + inst.info.template_id;
        return false;
    }

    const int channel_max = tpl->info.channel_max;
    if (channel_max <= 0)
    {
        /*out_error = "AddDefaultChannel: template channel_max <= 0.";*/
        out_error = "添加默认通道：模板 channel_max <= 0.";
        return false;
    }

    if (static_cast<int>(inst.channels.size()) >= channel_max)
    {
        /*out_error = "AddDefaultChannel: channel count reached max: " + std::to_string(channel_max);*/
        out_error = "添加默认通道：通道数量已达到最大值：" + std::to_string(channel_max);
        return false;
    }

    std::vector<bool> used(static_cast<size_t>(channel_max), false);
    for (const auto& ch : inst.channels)
    {
        if (ch.index >= 0 && ch.index < channel_max)
            used[static_cast<size_t>(ch.index)] = true;
    }

    int pick_index = -1;
    for (int i = 0; i < channel_max; ++i)
    {
        if (!used[static_cast<size_t>(i)])
        {
            pick_index = i;
            break;
        }
    }

    if (pick_index < 0)
    {
        /*out_error = "AddDefaultChannel: no available channel_index.";*/
        out_error = "添加默认通道：没有可用的通道索引。";
        return false;
    }

    const std::string display_name = "CH" + std::to_string(pick_index + 1);

    std::string new_channel_id;
    if (!AddChannel(instance_id, pick_index, display_name, new_channel_id))
    {
        /*out_error = "AddDefaultChannel: AddChannel failed.";*/
        out_error = "添加默认通道：AddChannel 失败。";
        return false;
    }

    // 补齐这个 channel 的“通道参数默认值”
    for (const auto& kv : tpl->params)
    {
        const SS_LightParamDef& def = kv.second;
        if (def.location != SS_LIGHT_PARAM_LOCATION::CHANNEL)
            continue;

        const std::string key = def.key.empty() ? kv.first : def.key;
        auto& per_channel = inst.channel_param_values[key];

        if (per_channel.find(new_channel_id) == per_channel.end())
            per_channel[new_channel_id] = PickDefaultValueForParam(def);
    }

    out_channel_id = new_channel_id;
    return true;
}

// ============================
// persist ops: Resolve / Save / Delete
// ============================

bool SS_LightResourceManager::ResolveInstancePath(
    const std::string& instance_id,
    std::string& out_path,
    std::string& out_error) const
{
    out_error.clear();
    out_path.clear();

    if (instance_dir_.empty())
    {
        /*out_error = "ResolveInstancePath: instance_dir_ is empty.";*/
        out_error = "解析实例路径：实例目录“instance_dir_”为空。";
        return false;
    }
    if (instance_id.empty())
    {
        /*out_error = "ResolveInstancePath: instance_id is empty.";*/
        out_error = "解析实例路径：实例 ID 为空。";
        return false;
    }

    // 1) 优先查映射表（支持 UUID / 任意 id）
    auto it = instance_paths_.find(instance_id);
    if (it != instance_paths_.end() && !it->second.empty())
    {
        out_path = it->second;
        return true;
    }

    // 2) fallback：兼容旧规则 controller_<digits> -> instance_dir_/controller_<digits>_instance.yaml
    if (StartsWith(instance_id, "controller_"))
    {
        const std::string num_part = instance_id.substr(std::string("controller_").size());
        if (IsDigits(num_part))
        {
            fs::path p = fs::path(instance_dir_) / (instance_id + "_instance.yaml");
            out_path = p.string();
            return true;
        }
    }

    // 3) generic fallback: <instance_dir>/<instance_id>_instance.yaml
    fs::path p = fs::path(instance_dir_) / (instance_id + "_instance.yaml");
    out_path = p.string();
    return true;

    /*out_error = "ResolveInstancePath: cannot resolve path for instance_id: " + instance_id;*/
    out_error = "解析实例路径：无法解析实例 ID 的路径：" + instance_id;
    return false;
}

bool SS_LightResourceManager::SaveInstanceById(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "SaveInstanceById: instance not found: " + instance_id;*/
        out_error = "保存实例（按 ID 查找）：未找到实例：" + instance_id;
        return false;
    }

    std::string path;
    if (!ResolveInstancePath(instance_id, path, out_error))
        return false;

    try
    {
        fs::path dir(instance_dir_);
        if (!fs::exists(dir))
            fs::create_directories(dir);
    }
    catch (const std::exception& e)
    {
        out_error = std::string("保存实例（按 ID 保存）：文件系统错误： ") + e.what();
        return false;
    }

    const SS_LightControllerInstance& inst = rt->GetInstance();
    if (!codec_.SaveInstance(path, inst))
    {
        /*out_error = "SaveInstanceById: SaveInstance failed: " + path +
            " | codec error: " + codec_.GetLastError();*/
        out_error = "保存实例（按实例 ID 保存）：保存实例操作失败：" + path +
            " | 编码器错误：" + codec_.GetLastError();
        return false;
    }

    // 保存成功后确保映射存在（防止外部手动 Remove 了 map）
    instance_paths_[instance_id] = fs::absolute(fs::path(path)).string();
    return true;
}

bool SS_LightResourceManager::DeleteInstanceById(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "DeleteInstanceById: instance not found: " + instance_id;*/
        out_error = "删除实例操作：未找到该实例：" + instance_id;
        return false;
    }

    std::string path;
    if (!ResolveInstancePath(instance_id, path, out_error))
        return false;

    try
    {
        fs::path p(path);
        if (fs::exists(p))
        {
            if (!fs::is_regular_file(p))
            {
                /*out_error = "DeleteInstanceById: not a regular file: " + path;*/
                out_error = "删除实例（按 ID 删除）：并非常规文件：" + path;
                return false;
            }
            fs::remove(p);
        }
    }
    catch (const std::exception& e)
    {
        out_error = std::string("删除实例（按 ID 删除）：文件系统错误：") + e.what();
        return false;
    }

    runtimes_.erase(instance_id);
    instance_paths_.erase(instance_id);
    return true;
}

bool SS_LightResourceManager::SetParameterValue(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result)
{
    SS_LightControllerRuntime* rt = FindRuntime(req.instance_id);
    if (!rt)
    {
        out_result = SS_LightParamSetResult{};
        out_result.ok = false;
        out_result.message = "未找到实例：" + req.instance_id;
        return false;
    }

    return rt->SetParamAndBuildCommand(req, out_result);
}

bool SS_LightResourceManager::SetParameterAndSend(const SS_LightParamSetRequest& req, SS_LightParamSetResult& out_result)
{
    SS_LightControllerRuntime* rt = FindRuntime(req.instance_id);
    if (!rt)
    {
        out_result = SS_LightParamSetResult{};
        out_result.ok = false;
        out_result.message = "实例未找到： " + req.instance_id;
        return false;
    }

    return rt->SetParamAndSend(req, out_result);
}

bool SS_LightResourceManager::CreateInstanceFromTemplate(
    const std::string& template_id,
    const std::string& display_name,
    std::string& out_instance_id,
    std::string& out_error)
{
    out_error.clear();
    out_instance_id.clear();

    if (template_id.empty())
    {
        /*out_error = "CreateInstanceFromTemplate: template_id is empty.";*/
        out_error = "从模板创建实例：模板 ID 为空。";
        return false;
    }

    const SS_LightControllerTemplate* tpl = FindTemplate(template_id);
    if (!tpl)
    {
        /*out_error = "CreateInstanceFromTemplate: template not found: " + template_id;*/
        out_error = "从模板创建实例：未找到模板：" + template_id;
        return false;
    }

    if (instance_dir_.empty())
    {
        /*out_error = "CreateInstanceFromTemplate: instance_dir_ is empty.";*/
        out_error = "从模板创建实例：实例目录为空。";
        return false;
    }

    fs::path dir(instance_dir_);
    std::error_code ec;
    if (!fs::exists(dir, ec))
    {
        if (!fs::create_directories(dir, ec))
        {
            /*out_error = "CreateInstanceFromTemplate: create instance_dir failed: " + instance_dir_ +
                " | ec=" + std::to_string(ec.value());*/
            out_error = "从模板创建实例：创建实例目录失败：" + instance_dir_ +
                " | ec=" + std::to_string(ec.value());
            return false;
        }
    }
    else if (!fs::is_directory(dir, ec))
    {
        /*out_error = "CreateInstanceFromTemplate: instance_dir_ is not a directory: " + instance_dir_;*/
        out_error = "从模板创建实例：instance_dir_ 不是目录：" + instance_dir_;
        return false;
    }

    // 仍保留原先的 “controller_<num>” 文件策略（不影响 UUID 实例的保存）
    int max_index = 0;
    bool has_any = false;
    const std::regex kRe(R"(controller_(\d+)_instance\.yaml$)", std::regex::icase);

    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        const std::string fname = entry.path().filename().string();
        std::smatch m;
        if (!std::regex_match(fname, m, kRe)) continue;

        if (m.size() >= 2)
        {
            try
            {
                int idx = std::stoi(m[1].str());
                if (!has_any) { max_index = idx; has_any = true; }
                else max_index = std::max(max_index, idx);
            }
            catch (...)
            {
            }
        }
    }

    int new_index = has_any ? (max_index + 1) : 1;

    fs::path yaml_path;
    std::string instance_id;
    while (true)
    {
        instance_id = "controller_" + std::to_string(new_index);
        yaml_path = dir / (instance_id + "_instance.yaml");
        if (!fs::exists(yaml_path, ec)) break;
        ++new_index;
    }

    SS_LightControllerInstance inst;
    inst.info.instance_id = instance_id;
    inst.info.template_id = template_id;
    inst.info.display_name = display_name.empty() ? instance_id : display_name;
    inst.info.enabled = true;
    inst.info.deleted = false;
    inst.info.order = new_index;

    inst.connection.connect_type = SS_LIGHT_CONNECT_TYPE::UNKNOWN;
    inst.connection.connect_state = false;

    inst.channels.clear();
    if (tpl->info.channel_max > 0)
    {
        inst.channels.reserve(static_cast<size_t>(tpl->info.channel_max));
        inst.channels.clear(); // keep empty
    }

    inst.global_param_values.clear();
    inst.channel_param_values.clear();

    for (const auto& kv : tpl->params)
    {
        const SS_LightParamDef& def = kv.second;
        const std::string key = def.key.empty() ? kv.first : def.key;
        const std::string dv = PickDefaultValueForParam(def);

        if (def.location == SS_LIGHT_PARAM_LOCATION::GLOBAL)
        {
            inst.global_param_values[key] = dv;
        }
        else if (def.location == SS_LIGHT_PARAM_LOCATION::CHANNEL)
        {
            auto& per_channel = inst.channel_param_values[key];
            for (const auto& ch : inst.channels)
                per_channel[ch.channel_id] = dv;
        }
    }

    if (!codec_.SaveInstance(yaml_path.string(), inst))
    {
        /*out_error = "CreateInstanceFromTemplate: SaveInstance failed: " + yaml_path.string() +
            " | codec error: " + codec_.GetLastError();*/
        out_error = "从模板创建实例：保存实例操作失败：" + yaml_path.string() +
            " | 编码器错误：" + codec_.GetLastError();
        return false;
    }

    if (!AddOrReplaceInstance(inst, out_error))
    {
        /*out_error = "CreateInstanceFromTemplate: AddOrReplaceInstance failed after saving file: " +
            yaml_path.string() + " | " + out_error;*/
        out_error = "从模板创建实例：保存文件后添加或替换实例操作失败：" +
            yaml_path.string() + " | " + out_error;
        return false;
    }

    // 关键：记录路径映射（这样 SaveInstanceById 就不再依赖 instance_id 的格式）
    instance_paths_[inst.info.instance_id] = fs::absolute(yaml_path).string();

    out_instance_id = instance_id;
    return true;
}

bool SS_LightResourceManager::ConnectInstance(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "ConnectInstance: instance not found: " + instance_id;*/
        out_error = "连接实例：实例未找到：" + instance_id;
        return false;
    }

    return rt->Connect(out_error);
}

bool SS_LightResourceManager::DisconnectInstance(const std::string& instance_id, std::string& out_error)
{
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "DisconnectInstance: instance not found: " + instance_id;*/
        out_error = "断开实例连接：未找到该实例：" + instance_id;
        return false;
    }

    rt->Disconnect();
    return true;
}

bool SS_LightResourceManager::IsInstanceConnected(const std::string& instance_id) const
{
    const SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt) return false;
    return rt->IsConnected();
}

bool SS_LightResourceManager::UpdateConnectionConfig(
    const std::string& instance_id,
    const SS_LightConnectionConfig& conn,
    std::string& out_error)
{
    out_error.clear();

    SS_LightControllerRuntime* rt = FindRuntime(instance_id);
    if (!rt)
    {
        /*out_error = "UpdateConnectionConfig: instance not found: " + instance_id;*/
        out_error = "更新连接配置：实例未找到：" + instance_id;
        return false;
    }

    // 关键：如果已连接，先断开，让配置切换具备确定性
    if (rt->IsConnected())
    {
        rt->Disconnect(); // 这里会触发 INSTANCE_DISCONNECTED 事件（或兜底事件）
    }

    rt->GetInstanceMutable().connection = conn;
    rt->GetInstanceMutable().connection.connect_state = false; // 存档状态也清掉
    return true;
}

const SS_LightControllerTemplate* SS_LightResourceManager::FindTemplate(const std::string& template_id) const
{
    auto it = templates_.find(template_id);
    if (it == templates_.end()) return nullptr;
    return &it->second;
}

SS_LightControllerRuntime* SS_LightResourceManager::FindRuntime(const std::string& instance_id)
{
    auto it = runtimes_.find(instance_id);
    if (it == runtimes_.end()) return nullptr;
    return it->second.get();
}

const SS_LightControllerRuntime* SS_LightResourceManager::FindRuntime(const std::string& instance_id) const
{
    auto it = runtimes_.find(instance_id);
    if (it == runtimes_.end()) return nullptr;
    return it->second.get();
}

void SS_LightResourceManager::CleanupChannelParamValues(SS_LightControllerInstance& inst, const std::string& channel_id)
{
    for (auto& kv : inst.channel_param_values)
    {
        auto& channel_map = kv.second;
        channel_map.erase(channel_id);
    }
}
