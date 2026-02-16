#pragma once
#include "instant/compile_depend/ss_yaml.h"

#include <string>
#include <vector>

// DLL 导出/导入宏（构建 UI DLL 时导出，否则作为使用方导入）
#ifdef SS_API_EXPORT_UI
#  define SS_EXPORT_UI __declspec(dllexport)
#else
#  define SS_EXPORT_UI __declspec(dllimport)
#endif

namespace YAML {
    class Node;
}

namespace ndstore {
    class NDStore;
}

// DEI 解析/构建代理：负责从插件 YAML 配置中解析布局树，并构建 NDStore 存储的 UI 结构树
class SS_EXPORT_UI SS_DEIAgent
{
public:
    // 构造：传入插件名与插件配置文件路径（用于定位对应 YAML）
    SS_DEIAgent(const std::string& plugin_name,
        const std::string& plugin_file_path);
    // 析构：释放内部资源
    ~SS_DEIAgent();

    // 初始化：读取 YAML，构建 NDStore 的 UI 树
    void InitUITree();

private:
    // 从 totality_layout 根开始遍历整棵布局树
    void TraverseTotalityLayout(const YAML::Node& totalityLayout);

    // 递归遍历单个节点及其子节点
    // parentPath: 从根到“当前节点父节点”的路径（字符串数组）
    // name: 当前节点名（例如 father1_widget）
    // nodesMap: 当前层的 nodes 映射（name -> 节点定义）
    void TraverseOneNode(const std::vector<std::string>& parentPath,
        const std::string& name,
        const YAML::Node& nodesMap);

    // 实际“处理一个节点”的逻辑：
    // - 查找 widget_library 中该节点对应的定义
    // - 组装 WidgetFeature/Value
    // - 写入 NDStore（并可能附带保存对应 YAML::Node）
    void ProcessOneNode(const std::vector<std::string>& parentPath,
        const std::string& name,
        const YAML::Node& childNode);

    // 将当前 NDStore 的 UI 树以可读形式输出到文件（用于调试/核对）
    void DumpNDStoreToFile(const ndstore::NDStore* store, const std::string& filepath);

private:
    YAML::Node dei_tree_node_;           // 整棵 YAML 配置树（根节点）
    ndstore::NDStore* dei_tree_ = nullptr; // NDStore 存储的 UI 结构树（由实现侧负责创建/释放）
    std::string dei_file_path_;          // DEI/YAML 文件路径
};
