#include "ss_dei_agent.h"
#include "ss_ndstore.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>


SS_DEIAgent::SS_DEIAgent(const std::string& plugin_name,
    const std::string& plugin_file_path)
{
    // 当前实现：DEI 文件路径写死到固定相对位置（后续可改为使用入参 plugin_file_path）
    dei_file_path_ = "\\DefaultModel\\dei-new.yaml";

    // 创建 NDStore（用于承载解析出的 UI 层级结构树）
    dei_tree_ = new ndstore::NDStore();

    // 初始化：读取 YAML 并构建整棵 UI 树
    InitUITree();
}

SS_DEIAgent::~SS_DEIAgent()
{
    // 释放 NDStore
    delete dei_tree_;
    dei_tree_ = nullptr;
}

void SS_DEIAgent::InitUITree()
{
    // 1) 拼出 DEI YAML 文件的绝对路径：current_path + 相对路径 dei_file_path_
    std::filesystem::path dei_file_path = std::filesystem::current_path().concat(dei_file_path_);
    if (!std::filesystem::exists(dei_file_path))
        return;

    // 2) 读取 YAML 文件，得到整棵配置树（根节点）
    dei_tree_node_ = YAML::LoadFile(dei_file_path.string());

    // 3) 获取 totality_layout 根节点（布局树入口）
    YAML::Node totality_layout = dei_tree_node_["totality_layout"];
    if (!totality_layout || !totality_layout.IsMap()) {
        return;
    }

    // 4) 从 totality_layout 开始递归遍历布局树，同时写入 NDStore
    TraverseTotalityLayout(totality_layout);

    // 5) dump 一份 NDStore 树到文件，便于验证节点是否都成功加载
    std::string output_path = ".\\DefaultModel\\ndstore_dump.txt";
    DumpNDStoreToFile(dei_tree_, output_path);
}

// ---------------------------
// 从 totality_layout 根开始的总入口
// totalityLayout = dei_tree_node_["totality_layout"]
// ---------------------------
void SS_DEIAgent::TraverseTotalityLayout(const YAML::Node& totalityLayout)
{
    if (!totalityLayout || !totalityLayout.IsMap()) return;

    // totality_layout 下的每个 key 都是一个“顶层祖宗节点”（根层 widget）
    // 顶层节点的父路径为空
    std::vector<std::string> emptyPath;

    // 遍历所有顶层节点名，并从每个顶层节点开始递归
    for (auto it = totalityLayout.begin(); it != totalityLayout.end(); ++it) {
        const std::string rootName = it->first.as<std::string>();
        // 这一层的 nodes 映射就是 totality_layout 本身（name -> 节点定义）
        TraverseOneNode(emptyPath, rootName, totalityLayout);
    }
}

// ---------------------------
// 递归遍历某一层上的节点
// parentPath: 从祖宗到“当前节点父节点”的名字序列
// name      : 当前节点名
// nodesMap  : 当前层级的节点映射（name -> node）
// ---------------------------
void SS_DEIAgent::TraverseOneNode(const std::vector<std::string>& parentPath, const std::string& name, const YAML::Node& nodesMap)
{
    if (!nodesMap || !nodesMap.IsMap()) return;

    // 取出当前节点定义
    YAML::Node childNode = nodesMap[name];
    if (!childNode || !childNode.IsMap())  return;

    // 1) 先处理当前节点（在 NDStore 中确保节点存在并写入业务数据）
    ProcessOneNode(parentPath, name, childNode);

    // 2) 获取子节点信息：
    // - childrens: 子节点名称列表（Sequence）
    // - nodes    : 子节点 name -> 节点定义（Map）
    YAML::Node childrens = childNode["childrens"];
    YAML::Node sub_nodes = childNode["nodes"];

    // 没有子节点则结束递归（叶子节点）
    if (!childrens || !childrens.IsSequence() || !sub_nodes || !sub_nodes.IsMap()) return;

    // 当前节点作为下一层的父节点：构造 newParentPath = parentPath + name
    std::vector<std::string> newParentPath = parentPath;
    newParentPath.push_back(name);

    // childrens 是字符串列表：[ "father1_widget", "father2_widget", ... ]
    for (std::size_t i = 0; i < childrens.size(); ++i)
    {
        YAML::Node childNameNode = childrens[i];
        if (!childNameNode || !childNameNode.IsScalar()) continue;

        std::string childName = childNameNode.as<std::string>();

        // sub_nodes 中必须存在对应定义，否则跳过
        if (!sub_nodes[childName] || !sub_nodes[childName].IsMap()) continue;

        // 递归到下一层
        TraverseOneNode(newParentPath, childName, sub_nodes);
    }
}

// ---------------------------
// 处理一个节点：
// - 从布局节点读取 location/layout
// - 在 widget_library 中定位 widget_key = name<location>
// - 组装 WidgetFeature
// - 写入 NDStore（路径 = parentPath + name），并保存 widget_node 深拷贝
// ---------------------------
void SS_DEIAgent::ProcessOneNode(const std::vector<std::string>& parentPath, const std::string& name, const YAML::Node& childNode)
{
    if (!dei_tree_) return;

    // 1) 布局节点必须有 location + layout，否则无法构造 WidgetFeature
    if (!childNode["location"] || !childNode["layout"]) return;

    const YAML::Node layout = childNode["layout"];
    std::string location_ptr = childNode["location"].as<std::string>();

    // 2) 在 widget_library 中找到对应控件定义（用于后续保留原始 widget YAML 信息）
    YAML::Node widget_library = dei_tree_node_["widget_library"];
    if (!widget_library || !widget_library.IsMap()) return;

    // widget_library 的 key 是：name<location>
    // 例如：grandfather_widget<0>、father1_widget<0/0>...
    std::string widget_key = name + "<" + location_ptr + ">";
    YAML::Node widget_node = widget_library[widget_key];
    if (!widget_node || widget_node.IsNull()) {
        // 找不到 widget 定义则跳过该节点
        return;
    }

    // 3) 将 layout 信息转为 WidgetFeature（挂到 NDStore::Node::value 上）
    ndstore::WidgetFeature widget_feature;
    widget_feature.location = location_ptr;
    widget_feature.rows = layout["rows"].as<int>(0);
    widget_feature.cols = layout["cols"].as<int>(0);
    widget_feature.row_span = layout["row_span"].as<int>(1);
    widget_feature.column_span = layout["column_span"].as<int>(1);
    widget_feature.type = layout["type"].as<int>(0);
    widget_feature.combine_mode = layout["combine_mode"].as<int>(0);
    widget_feature.is_scroll = layout["is_scroll"].as<bool>(false);

    // --- 4) 拼出 NDStore 路径：父路径 + 当前 name ---
    std::vector<std::string> fullPath = parentPath;
    fullPath.push_back(name);

    // --- 5) 写入 NDStore ---
    // 这里用你的 NDStore::ensure_node(Range) 接口
    ndstore::Node* n = dei_tree_->ensure_node(fullPath);
    if (!n) return;

    n->value = widget_feature;

    // 存 YAML 节点：这里用 new 做拷贝，避免受 YAML 树结构引用语义影响
    // 这会有一点点内存泄漏，建议后续你可以把 Node 里的 widget_node 改成 std::unique_ptr<YAML::Node>
        // 保存 YAML 节点（深拷贝），便于后续根据节点反查其原始 widget 配置
    n->widget_node = std::make_unique<YAML::Node>(widget_node);
}

void SS_DEIAgent::DumpNDStoreToFile(const ndstore::NDStore* store, const std::string& filepath)
{
    if (!store)
    {
        std::cerr << "DumpNDStoreToFile: store is null.\n";
        return;
    }

    // 打开输出文件（覆盖写）：用于输出整棵 NDStore 树的可读信息
    std::ofstream ofs(filepath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "DumpNDStoreToFile: cannot open file: " << filepath << "\n";
        return;
    }

    ofs << "=========== NDStore Tree Dump ===========" << std::endl;

    // 遍历 NDStore：前序 DFS
    // 输出内容包括：节点路径、是否保存 widget_node、以及 WidgetFeature 的字段值
    store->traverse([&ofs](const std::vector<std::string>& path,
        const ndstore::Value& value,
        const ndstore::Node* node)
    {
        // ---- 1) 打印路径 ----
        ofs << "[Node] ";
        if (path.empty())
            ofs << "<root>";
        else {
            for (size_t i = 0; i < path.size(); ++i)
            {
                ofs << path[i];
                if (i + 1 < path.size()) ofs << "/";
            }
        }
        ofs << "\n";

        // ---- 2) 打印 widget_node 是否存在（用于确认是否成功关联 widget_library 配置） ----
        ofs << "    widget_node: " << (node->widget_node ? "YES" : "NO") << "\n";

        // ---- 3) 打印 value 类型与内容（当前主要关注 WidgetFeature） ----
        if (auto wf = std::get_if<ndstore::WidgetFeature>(&value)) {
            ofs << "    value: WidgetFeature\n";
            ofs << "        location     = " << wf->location << "\n";
            ofs << "        rows         = " << wf->rows << "\n";
            ofs << "        cols         = " << wf->cols << "\n";
            ofs << "        row_span     = " << wf->row_span << "\n";
            ofs << "        column_span  = " << wf->column_span << "\n";
            ofs << "        type         = " << wf->type << "\n";
            ofs << "        combine_mode = " << wf->combine_mode << "\n";
            ofs << "        is_scroll    = " << (wf->is_scroll ? "true" : "false") << "\n";
        }
        else {
            ofs << "    value: index=" << value.index() << "\n";
        }

        ofs << "\n";
    });

    ofs << "==========================================" << std::endl;
    ofs.close();
}
