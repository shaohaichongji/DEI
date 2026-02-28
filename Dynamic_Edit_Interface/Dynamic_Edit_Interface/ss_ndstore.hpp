#pragma once
/*
 * ndstore.hpp - Header-only N-Dimensional Hierarchical Store (C++17)
 *
 * 设计目标：
 *  - 动态层级树表达任意维度（与 YAML/JSON DOM 同构）
 *  - 扁平反向索引（Path -> Node*）近 O(1) 直达定位
 *  - 字符串驻留（SymbolId）降低内存 & 加速比较/哈希
 *  - 可 O(depth) 回溯路径（父指针）
 *
 * 适用规模：千～万级节点
 */

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <stdexcept>

#include "../../include/ss_yaml_depend.h"

namespace ndstore {

    // 控件布局/特性描述（与业务侧 widget 配置对应）
    struct WidgetFeature
    {
        std::string location; // 布局位置/区域标识
        int rows;             // 行数
        int cols;             // 列数
        int row_span;         // 行跨度
        int column_span;      // 列跨度
        int type;             // 控件类型（业务枚举/编号）
        int combine_mode;     // 组合模式（业务定义）
        bool is_scroll;       // 是否滚动
    };

    // ============================
    // 1) 字符串驻留（Symbol 表）
    // ============================

    // 字符串驻留后的 ID（用于降低内存与加速比较/哈希）
    using SymbolId = std::uint32_t;

    // C++17：透明哈希/相等器不启用异构查找，但保留无害
    // 用于 unordered_map 的哈希：支持 std::string / std::string_view
    struct SvHash {
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        std::size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string>{}(s);
        }
    };
    // 用于 unordered_map 的相等比较：支持 std::string / std::string_view
    struct SvEq {
        bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
        bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
        bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
        bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
    };

    // 字符串驻留表：维护“字符串 <-> SymbolId”的双向映射
    struct SymbolTable {
        using map_type = std::unordered_map<std::string, SymbolId, SvHash, SvEq>;
        using iterator = map_type::iterator;
        using const_iterator = map_type::const_iterator;

        map_type to_id;                   // 字符串 -> id
        std::vector<std::string> to_str; // id -> 字符串

        // C++17：统一 find，回退到构造临时 std::string（避免异构 find 依赖）
        iterator       umap_find(std::string_view sv) { return to_id.find(std::string(sv)); }
        const_iterator umap_find(std::string_view sv) const { return to_id.find(std::string(sv)); }
        iterator       umap_find(const std::string& s) { return to_id.find(s); }
        const_iterator umap_find(const std::string& s) const { return to_id.find(s); }
        iterator       umap_find(const char* s) { return to_id.find(std::string(s)); }
        const_iterator umap_find(const char* s) const { return to_id.find(std::string(s)); }

        // 将字符串驻留并返回对应 id；若已存在则直接返回已有 id
        SymbolId intern(std::string_view sv) {
            auto it = umap_find(sv);
            if (it != to_id.end()) return it->second;
            SymbolId id = static_cast<SymbolId>(to_str.size());
            to_str.emplace_back(sv);            // 稳定存储
            to_id.emplace(to_str.back(), id);   // key 使用稳定存储
            return id;
        }
        // 判断字符串是否已驻留
        bool has(std::string_view sv) const { return umap_find(sv) != to_id.end(); }
        // 获取字符串对应 id（若未驻留则抛出异常）
        SymbolId id_of(std::string_view sv) const {
            auto it = umap_find(sv);
            if (it == to_id.end()) throw std::out_of_range("Symbol not interned: " + std::string(sv));
            return it->second;
        }
        // 通过 id 获取字符串（越界会抛异常）
        const std::string& str(SymbolId id) const { return to_str.at(id); }
        // 清空符号表
        void clear() { to_id.clear(); to_str.clear(); }

        // 批量构建时可预留容量，减少 rehash/扩容
        void reserve(std::size_t n) { to_id.reserve(n); to_str.reserve(n); }
        // 已驻留字符串数量
        std::size_t size() const noexcept { return to_str.size(); }
    };

    // ============================
    // 2) 节点值（Value）定义
    // ============================

    // 节点承载的值类型：
    // - monostate：表示“无值”（仅结构节点）
    // - bool/int64/double/string：基础标量
    // - WidgetFeature：业务侧控件特性
    using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string, WidgetFeature>;

    // ============================
    // 3) 树节点（Node）
    // ============================

    // 树节点：同时保存值、子节点、父指针与插入顺序
    struct Node {
        Value value{}; // 当前节点的值
        std::unordered_map<SymbolId, std::unique_ptr<Node>> children; // 子节点（key 为驻留后的 SymbolId）
        std::vector<SymbolId> order; // 子节点插入顺序（用于稳定遍历/序列化）

        Node* parent = nullptr; // 父节点指针（用于回溯路径）
        SymbolId key_from_parent = static_cast<SymbolId>(-1); // 本节点在父节点 children 中的 key

        // 指向对应 YAML::Node（由 Node 拥有其深拷贝副本）
        std::unique_ptr<YAML::Node> widget_node;

        // 判断是否存在指定子节点
        bool has_child(SymbolId k) const { return children.find(k) != children.end(); }
        // 获取子节点指针（不存在返回 nullptr）
        Node* child(SymbolId k) const {
            auto it = children.find(k);
            return (it == children.end()) ? nullptr : it->second.get();
        }
        // 是否存在任意子节点
        bool has_children() const { return !children.empty(); }
        // 是否包含值（非 monostate）
        bool has_value()    const { return value.index() != 0; } // 非 monostate
    };

    // ============================
    // 4) 路径类型与哈希
    // ============================

    // 路径：由多层 SymbolId 组成，表示从根到节点的绝对路径
    using Path = std::vector<SymbolId>;

    // Path 的哈希（用于 unordered_map）
    struct PathHash {
        std::size_t operator()(const Path& p) const {
            std::size_t h = 0;
            for (auto id : p) {
                h ^= std::hash<SymbolId>{}(id)+0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    };

    namespace detail {
        // 检测类型是否有 size() 成员（用于容量预估）
        template<class T, class = void>
        struct has_size : std::false_type {};
        template<class T>
        struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>> : std::true_type {};

        // 尝试估算 Range 的 size（没有 size() 则返回 0）
        template<class Range>
        std::size_t guess_size(const Range& r) {
            if constexpr (has_size<Range>::value) return r.size();
            else return 0;
        }
    }

    // ============================
    // 5) 主存储 NDStore
    // ============================

    // N 维层级存储：
    // - 以树表示层级结构
    // - 以 index_ 提供 Path -> Node* 的 O(1) 定位
    class NDStore {
    public:
        // 构造：创建根节点，默认开启“前缀索引”（索引所有中间路径）
        NDStore()
            : root_(std::make_unique<Node>())
            , index_all_prefixes_(true)
        {
        }

        // ---------- 写入：Upsert（创建或覆盖值，同时维护索引） ----------

        // Range 版本（多层路径）——旧接口（不带 YAML）
        template<class KeyRange>
        Node* upsert(const KeyRange& keys, const Value& v) {
            return upsert_impl_(keys, v, nullptr);
        }

        // 便捷：initializer_list —— 旧接口
        Node* upsert(std::initializer_list<std::string_view> keys, const Value& v) {
            return upsert_impl_<std::initializer_list<std::string_view>>(keys, v, nullptr);
        }

        // 单键重载（根下第一层）—— 旧接口
        Node* upsert(std::string_view key, const Value& v) {
            Path p{ sym_.intern(key) };
            Node* n = descend_create_with_index_(p);
            n->value = v;
            n->widget_node = nullptr;
            index_[p] = n;
            return n;
        }
        Node* upsert(const std::string& key, const Value& v) { return upsert(std::string_view{ key }, v); }
        Node* upsert(const char* key, const Value& v) { return upsert(std::string_view{ key }, v); }

        // 带 YAML::Node* 的 upsert 重载（Range）
        // - widget_node 不为空：深拷贝保存到节点中
        // - widget_node 为空：清空节点中已保存的 YAML 信息
        template<class KeyRange>
        Node* upsert(const KeyRange& keys, const Value& v, YAML::Node* widget_node) {
            return upsert_impl_(keys, v, widget_node);
        }

        // 带 YAML::Node* 的 upsert（initializer_list）
        Node* upsert(std::initializer_list<std::string_view> keys,
            const Value& v,
            YAML::Node* widget_node) {
            return upsert_impl_<std::initializer_list<std::string_view>>(keys, v, widget_node);
        }

        // 带 YAML::Node* 的 upsert（单键）
        Node* upsert(std::string_view key, const Value& v, YAML::Node* widget_node) {
            Path p{ sym_.intern(key) };
            Node* n = descend_create_with_index_(p);
            n->value = v;

            if (widget_node) {
                // 深拷贝一份到 unique_ptr 里，由 Node 负责生命周期
                n->widget_node = std::make_unique<YAML::Node>(*widget_node);
            }
            else {
                // 如果传进来是空指针，清空之前可能存在的 widget_node
                n->widget_node.reset();
            }

            index_[p] = n;
            return n;
        }
        Node* upsert(const std::string& key, const Value& v, YAML::Node* widget_node) {
            return upsert(std::string_view{ key }, v, widget_node);
        }
        Node* upsert(const char* key, const Value& v, YAML::Node* widget_node) {
            return upsert(std::string_view{ key }, v, widget_node);
        }

        // ---------- 写结构：Ensure（仅保证节点存在，不改 value） ----------

        // 旧接口（不带 YAML）—— Range
        template<class KeyRange>
        Node* ensure_node(const KeyRange& keys) {
            return ensure_node_impl_(keys, nullptr);
        }

        // 旧接口：initializer_list
        Node* ensure_node(std::initializer_list<std::string_view> keys) {
            return ensure_node_impl_<std::initializer_list<std::string_view>>(keys, nullptr);
        }

        // 旧接口：单键
        Node* ensure_node(std::string_view key) {
            Path p{ sym_.intern(key) };
            Node* n = descend_create_with_index_(p);
            index_[p] = n;
            return n;
        }
        Node* ensure_node(const std::string& key) { return ensure_node(std::string_view{ key }); }
        Node* ensure_node(const char* key) { return ensure_node(std::string_view{ key }); }

        // 带 YAML::Node* 的 ensure_node（Range）：仅保证路径节点存在，可附带保存 YAML
        template<class KeyRange>
        Node* ensure_node(const KeyRange& keys, YAML::Node* widget_node) {
            return ensure_node_impl_(keys, widget_node);
        }

        // 带 YAML::Node* 的 ensure_node（initializer_list）
        Node* ensure_node(std::initializer_list<std::string_view> keys, YAML::Node* widget_node) {
            return ensure_node_impl_<std::initializer_list<std::string_view>>(keys, widget_node);
        }

        // 带 YAML::Node* 的 ensure_node（单键）
        Node* ensure_node(std::string_view key, YAML::Node* widget_node) {
            Path p{ sym_.intern(key) };
            Node* n = descend_create_with_index_(p);

            if (widget_node) {
                n->widget_node = std::make_unique<YAML::Node>(*widget_node);
            }
            else {
                n->widget_node.reset();
            }

            index_[p] = n;
            return n;
        }
        Node* ensure_node(const std::string& key, YAML::Node* widget_node) {
            return ensure_node(std::string_view{ key }, widget_node);
        }
        Node* ensure_node(const char* key, YAML::Node* widget_node) {
            return ensure_node(std::string_view{ key }, widget_node);
        }

        // ---------- 读取：Find（O(1) 查） ----------

        // 基于 index_ 的查找：传入多层路径 keys，返回对应节点指针（不存在则 nullptr）
        template<class KeyRange>
        Node* find(const KeyRange& keys) const {
            Path p = to_ids(keys);
            auto it = index_.find(p);
            return (it == index_.end()) ? nullptr : it->second;
        }
        // initializer_list 版本：如果出现未驻留的 key，会返回 nullptr
        Node* find(std::initializer_list<std::string_view> keys) const {
            Path p = to_ids_safe(keys);
            if (p.empty() && !std::empty(keys)) return nullptr;
            auto it = index_.find(p);
            return (it == index_.end()) ? nullptr : it->second;
        }
        // 单键重载
        Node* find(std::string_view key) const {
            if (!sym_.has(key)) return nullptr;
            Path p{ sym_.id_of(key) };
            auto it = index_.find(p);
            return (it == index_.end()) ? nullptr : it->second;
        }
        Node* find(const std::string& key) const { return find(std::string_view{ key }); }
        Node* find(const char* key)        const { return find(std::string_view{ key }); }

        // 判断路径是否存在
        template<class KeyRange>
        bool exists(const KeyRange& keys) const { return find(keys) != nullptr; }
        bool exists(std::initializer_list<std::string_view> keys) const { return find(keys) != nullptr; }
        bool exists(std::string_view key) const { return find(key) != nullptr; }

        // ---------- 不依赖索引的逐层下行查找（绝对路径） ----------

        // 不使用 index_，直接从 root_ 逐层寻找（适用于 index_all_prefixes_=false 或容错场景）
        template<class KeyRange>
        Node* descend_find(const KeyRange& keys) const {
            const Node* cur = root_.get();
            using std::begin; using std::end;
            if (begin(keys) == end(keys)) return const_cast<Node*>(cur);
            for (auto&& k : keys) {
                if (!sym_.has(k)) return nullptr;
                auto id = sym_.id_of(k);
                auto it = cur->children.find(id);
                if (it == cur->children.end()) return nullptr;
                cur = it->second.get();
            }
            return const_cast<Node*>(cur);
        }
        Node* descend_find(std::initializer_list<std::string_view> keys) const {
            return descend_find<std::initializer_list<std::string_view>>(keys);
        }
        // 单键重载：仅查 root_ 第一层
        Node* descend_find(std::string_view key) const {
            if (!sym_.has(key)) return nullptr;
            auto id = sym_.id_of(key);
            auto it = root_->children.find(id);
            return (it == root_->children.end()) ? nullptr : it->second.get();
        }

        // ---------- 相对下行（从任意起点） ----------

        // 从任意起点 start 开始逐层查找相对路径 keys
        template<class KeyRange>
        Node* descend_find_from(Node* start, const KeyRange& keys) const {
            if (!start) return nullptr;
            const Node* cur = start;
            using std::begin; using std::end;
            if (begin(keys) == end(keys)) return const_cast<Node*>(cur);
            for (auto&& k : keys) {
                if (!sym_.has(k)) return nullptr;
                auto id = sym_.id_of(k);
                auto it = cur->children.find(id);
                if (it == cur->children.end()) return nullptr;
                cur = it->second.get();
            }
            return const_cast<Node*>(cur);
        }
        Node* descend_find_from(Node* start, std::initializer_list<std::string_view> keys) const {
            return descend_find_from<std::initializer_list<std::string_view>>(start, keys);
        }
        // 单键重载
        Node* descend_find_from(Node* start, std::string_view key) const {
            if (!start || !sym_.has(key)) return nullptr;
            auto id = sym_.id_of(key);
            auto it = start->children.find(id);
            return (it == start->children.end()) ? nullptr : it->second.get();
        }

        // ---------- 删除：Erase 子树 ----------

        // 删除指定路径对应的子树（同时清理索引）
        template<class KeyRange>
        bool erase_subtree(const KeyRange& keys) {
            Path p = to_ids_safe(keys);
            if (p.empty() && !keys_empty_(keys)) return false;
            auto it = index_.find(p);
            if (it == index_.end()) return false;

            Node* n = it->second;
            if (n->parent) {
                // 从父节点摘除该子树
                n->parent->children.erase(n->key_from_parent);
                auto& ord = n->parent->order;
                ord.erase(std::remove(ord.begin(), ord.end(), n->key_from_parent), ord.end());
            }
            else {
                // 删除根：等价于清空
                root_ = std::make_unique<Node>();
                index_.clear();
                return true;
            }
            cleanup_index_under_(p);
            return true;
        }
        bool erase_subtree(std::initializer_list<std::string_view> keys) {
            return erase_subtree<std::initializer_list<std::string_view>>(keys);
        }
        // 单键重载
        bool erase_subtree(std::string_view key) {
            if (!sym_.has(key)) return false;
            return erase_subtree(std::initializer_list<std::string_view>{key});
        }

        // ---------- Typed Set / Get ----------

        // 类型安全 set：将 v 包装为 Value 写入
        template<class T, class KeyRange>
        bool set(const KeyRange& keys, T&& v) {
            return set_impl_(keys, Value(std::forward<T>(v)));
        }
        // initializer_list 版本
        template<class T>
        bool set(std::initializer_list<std::string_view> keys, T&& v) {
            return set<std::decay_t<T>, std::initializer_list<std::string_view>>(keys, std::forward<T>(v));
        }
        // 单键版本
        template<class T>
        bool set(std::string_view key, T&& v) {
            return set(std::initializer_list<std::string_view>{key}, std::forward<T>(v));
        }

        // 类型安全 get：取出指定类型值（类型不匹配/不存在返回 std::nullopt）
        template<class T, class KeyRange>
        std::optional<T> get(const KeyRange& keys) const {
            if (auto* n = find(keys)) {
                if (auto pv = std::get_if<T>(&n->value)) return *pv;
            }
            return std::nullopt;
        }
        template<class T>
        std::optional<T> get(std::initializer_list<std::string_view> keys) const {
            return get<T, std::initializer_list<std::string_view>>(keys);
        }
        template<class T>
        std::optional<T> get(std::string_view key) const {
            return get<T, std::initializer_list<std::string_view>>({ key });
        }

        // ---------- 反查路径 ----------

        // 从节点指针反推出路径（字符串形式）；若为根节点则返回空数组
        std::vector<std::string> path_of(const Node* n) const {
            std::vector<SymbolId> rev;
            while (n && n->parent) { rev.push_back(n->key_from_parent); n = n->parent; }
            std::vector<std::string> out; out.reserve(rev.size());
            for (auto it = rev.rbegin(); it != rev.rend(); ++it) out.push_back(sym_.str(*it));
            return out;
        }

        // ---------- 遍历：前序 DFS ----------

        // 前序深度优先遍历：fn(pathStrings, value, node*)
        template<class Fn>
        void traverse(Fn&& fn) const {
            std::vector<std::string> cur; cur.reserve(16);
            dfs_(root_.get(), cur, std::forward<Fn>(fn));
        }

        // ---------- 列子键（字符串 / SymbolId） ----------

        // 列出指定路径下的子节点 key（字符串形式，按插入顺序）
        template<class KeyRange>
        std::vector<std::string> list_children(const KeyRange& keys) const {
            std::vector<std::string> names;
            const Node* n = find(keys);
            if (!n) n = descend_find(keys);
            if (n) {
                names.reserve(n->order.size());
                for (auto id : n->order) names.push_back(sym_.str(id));
            }
            return names;
        }
        std::vector<std::string> list_children(std::initializer_list<std::string_view> keys) const {
            return list_children<std::initializer_list<std::string_view>>(keys);
        }
        std::vector<std::string> list_children(std::string_view key) const {
            if (!sym_.has(key)) return {};
            return list_children(std::initializer_list<std::string_view>{key});
        }

        // 列出指定路径下的子节点 key（SymbolId 形式）
        template<class KeyRange>
        std::vector<SymbolId> list_children_ids(const KeyRange& keys) const {
            std::vector<SymbolId> ids;
            const Node* n = find(keys);
            if (!n) n = descend_find(keys);
            if (n) ids = n->order;
            return ids;
        }
        std::vector<SymbolId> list_children_ids(std::initializer_list<std::string_view> keys) const {
            return list_children_ids<std::initializer_list<std::string_view>>(keys);
        }
        std::vector<SymbolId> list_children_ids(std::string_view key) const {
            if (!sym_.has(key)) return {};
            return list_children_ids(std::initializer_list<std::string_view>{key});
        }

        // ---------- 重命名：将 path 最后一层键改为 new_name ----------

        // 重命名某节点（仅改最后一层 key），不覆盖已有同名兄弟节点
        bool rename_node(std::initializer_list<std::string_view> keys, std::string_view new_name) {
            Path p = to_ids_safe(keys);
            if (p.empty() && !std::empty(keys)) return false;
            if (p.empty()) return false; // 不允许重命名根

            Path parent_p(p.begin(), p.end() - 1);
            Node* parent = nullptr;
            if (index_all_prefixes_) {
                auto it = index_.find(parent_p);
                if (it == index_.end()) return false;
                parent = it->second;
            }
            else {
                std::vector<std::string_view> parent_keys; parent_keys.reserve(parent_p.size());
                for (auto id : parent_p) parent_keys.push_back(sym_.str(id));
                parent = descend_find(parent_keys);
                if (!parent) return false;
            }

            SymbolId old_id = p.back();
            auto it_old = parent->children.find(old_id);
            if (it_old == parent->children.end()) return false;

            SymbolId new_id = sym_.intern(new_name);
            if (parent->children.find(new_id) != parent->children.end()) return false; // 不覆盖

            auto mover = std::move(it_old->second);
            parent->children.erase(it_old);
            mover->key_from_parent = new_id;
            parent->children.emplace(new_id, std::move(mover));

            for (auto& kid : parent->order) if (kid == old_id) { kid = new_id; break; }

            rebuild_index_(); // 重命名会影响整棵子树的 Path -> Node* 映射，重建最稳妥
            return true;
        }

        // ---------- 访问器 / 清空 / 行为开关 ----------

        // 获取符号表（可用于提前驻留字符串、取 id 等）
        SymbolTable& symbols() { return sym_; }
        const SymbolTable& symbols() const { return sym_; }
        // 获取根节点
        Node* root() { return root_.get(); }
        const Node* root() const { return root_.get(); }

        // 清空所有节点；可选是否同时清空符号表
        void clear(bool clear_symbols = false) {
            root_ = std::make_unique<Node>();
            index_.clear();
            if (clear_symbols) sym_.clear();
        }
        // 是否索引所有前缀路径：
        // - true：index_ 中包含每一级中间路径（find 速度快，索引占用更大）
        // - false：index_ 主要用于叶子/写入路径（部分操作会回退 descend_find）
        void set_index_all_prefixes(bool on) { index_all_prefixes_ = on; }

    private:
        // ---------- 路径辅助（intern/to-ids） ----------
        // 加护栏：防止把“单个字符串”误传给“Range 版”

        // 将 keys（字符串序列）驻留为 SymbolId 路径（不存在则创建 id）
        template<class KeyRange>
        Path intern_path(const KeyRange& keys) {
            using K = std::decay_t<KeyRange>;
            static_assert(!std::is_convertible<K, std::string_view>::value,
                "NDStore: This overload expects a RANGE of keys. For a single key, "
                "call foo(\"key\") or foo({\"key\"}).");
            Path p; p.reserve(detail::guess_size(keys));
            for (auto&& k : keys) p.push_back(sym_.intern(k));
            return p;
        }
        // 将 keys（字符串序列）转换为 SymbolId 路径（要求已驻留，否则抛异常）
        template<class KeyRange>
        Path to_ids(const KeyRange& keys) const {
            using K = std::decay_t<KeyRange>;
            static_assert(!std::is_convertible<K, std::string_view>::value,
                "NDStore: This overload expects a RANGE of keys. For a single key, "
                "call foo(\"key\") or foo({\"key\"}).");
            Path p; p.reserve(detail::guess_size(keys));
            for (auto&& k : keys) p.push_back(sym_.id_of(k)); // 未 intern 会抛异常
            return p;
        }
        // 将 keys 转换为 SymbolId 路径：若遇到未驻留 key，则返回空 Path
        template<class KeyRange>
        Path to_ids_safe(const KeyRange& keys) const {
            using K = std::decay_t<KeyRange>;
            static_assert(!std::is_convertible<K, std::string_view>::value,
                "NDStore: This overload expects a RANGE of keys. For a single key, "
                "call foo(\"key\") or foo({\"key\"}).");
            Path p; p.reserve(detail::guess_size(keys));
            using std::begin; using std::end;
            if (begin(keys) == end(keys)) return p;
            for (auto&& k : keys) {
                if (!sym_.has(k)) return Path{};
                p.push_back(sym_.id_of(k));
            }
            return p;
        }
        // 判断 Range 是否为空
        template<class KeyRange>
        static bool keys_empty_(const KeyRange& keys) {
            using std::begin; using std::end;
            return begin(keys) == end(keys);
        }

        // ---------- 节点创建 / 索引维护 ----------

        // 沿路径创建节点（不维护 index_）
        Node* descend_create_(const Path& p) {
            Node* cur = root_.get();
            for (auto id : p) {
                auto it = cur->children.find(id);
                if (it == cur->children.end()) {
                    auto child = std::make_unique<Node>();
                    child->parent = cur;
                    child->key_from_parent = id;
                    cur->order.push_back(id);
                    Node* raw = child.get();
                    cur->children.emplace(id, std::move(child));
                    cur = raw;
                }
                else {
                    cur = it->second.get();
                }
            }
            return cur;
        }

        // 沿路径创建节点并维护 index_（可选索引所有前缀）
        Node* descend_create_with_index_(const Path& p) {
            Node* cur = root_.get();
            Path curpath; curpath.reserve(p.size());
            if (index_all_prefixes_) index_[curpath] = cur; // 空路径 -> 根

            for (auto id : p) {
                auto it = cur->children.find(id);
                if (it == cur->children.end()) {
                    auto child = std::make_unique<Node>();
                    child->parent = cur;
                    child->key_from_parent = id;
                    cur->order.push_back(id);
                    Node* raw = child.get();
                    cur->children.emplace(id, std::move(child));
                    cur = raw;
                }
                else {
                    cur = it->second.get();
                }
                curpath.push_back(id);
                if (index_all_prefixes_) index_[curpath] = cur;
            }
            return cur;
        }

        // 判断 pre 是否为 p 的前缀
        static bool is_prefix_(const Path& pre, const Path& p) {
            if (pre.size() > p.size()) return false;
            for (std::size_t i = 0; i < pre.size(); ++i) if (pre[i] != p[i]) return false;
            return true;
        }

        // 清理 index_ 中以 prefix 开头的所有条目（用于删除子树）
        void cleanup_index_under_(const Path& prefix) {
            std::vector<Path> to_del;
            to_del.reserve(index_.size() / 4 + 1);
            for (auto& kv : index_) {
                if (is_prefix_(prefix, kv.first)) to_del.push_back(kv.first);
            }
            for (auto& p : to_del) index_.erase(p);
        }

        // 重建整个索引（用于重命名等会影响大量路径的操作）
        void rebuild_index_() {
            index_.clear();
            Path cur;
            rebuild_dfs_(root_.get(), cur);
        }
        // DFS 重建：cur 为当前路径
        void rebuild_dfs_(Node* n, Path& cur) {
            index_[cur] = n;
            for (auto id : n->order) {
                auto it = n->children.find(id);
                if (it == n->children.end()) continue;
                cur.push_back(id);
                rebuild_dfs_(it->second.get(), cur);
                cur.pop_back();
            }
        }

        // ---------- upsert 实现（带可选 YAML::Node*） ----------
        // 内部实现：keys 写入/创建，赋值 v，并按需保存 YAML
        template<class KeyRange>
        Node* upsert_impl_(const KeyRange& keys, const Value& v, YAML::Node* widget_node) {
            Path p = intern_path(keys);
            Node* n = descend_create_with_index_(p);

            n->value = v;

            if (widget_node) {
                // 深拷贝到 unique_ptr
                n->widget_node = std::make_unique<YAML::Node>(*widget_node);
            }
            else {
                n->widget_node.reset();
            }

            index_[p] = n;
            return n;
        }

        // ---------- ensure_node 实现（带可选 YAML::Node*） ----------
        // 内部实现：仅确保节点存在，并按需保存 YAML（不修改 value）
        template<class KeyRange>
        Node* ensure_node_impl_(const KeyRange& keys, YAML::Node* widget_node) {
            Path p = intern_path(keys);
            Node* n = descend_create_with_index_(p);

            if (widget_node) {
                n->widget_node = std::make_unique<YAML::Node>(*widget_node);
            }
            else {
                n->widget_node.reset();
            }

            index_[p] = n;
            return n;
        }

        // ---------- set/get 实现 ----------

        // set 的内部实现（不处理 YAML）
        template<class KeyRange>
        bool set_impl_(const KeyRange& keys, const Value& v) {
            Path p = intern_path(keys);
            Node* n = descend_create_with_index_(p);
            n->value = v;
            index_[p] = n;
            return true;
        }

        // ---------- 遍历 ----------

        // DFS 前序访问：cur 为当前路径字符串
        template<class Fn>
        void dfs_(const Node* n, std::vector<std::string>& cur, Fn&& fn) const {
            fn(cur, n->value, n);
            for (auto id : n->order) {
                auto it = n->children.find(id);
                if (it == n->children.end()) continue;
                cur.push_back(sym_.str(id));
                dfs_(it->second.get(), cur, fn);
                cur.pop_back();
            }
        }

    private:
        SymbolTable sym_; // 字符串驻留表
        std::unique_ptr<Node> root_; // 根节点（空路径）
        std::unordered_map<Path, Node*, PathHash> index_; // 反向索引：Path -> Node*
        bool index_all_prefixes_; // 是否索引所有前缀路径
    };

} // namespace ndstore
