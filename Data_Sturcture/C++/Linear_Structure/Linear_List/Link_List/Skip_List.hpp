#pragma once

#include <math.h>
#include <sstream>
#include "../../List_Node.hpp"
#include <optional>
#include <vector>

// 实现参考 redis zset、《数据结构、算法与应用 C++语言描述 》10.4 跳表表示

/// ============================================================================================================
/// 跳表
/// - 有序元素的链表，与二叉搜索树类似
/// - 跳表的增长由底层向上不断增高，与B树类似
/// - 元素节点的高度由随机几率决定
/// - 通常会限制最大层数，性能最佳。可以不限制，但是实现逻辑更复杂
///
/// 每个节点存一个element，节点指针有多级
///
/// 查找：O(logn)
/// 插入：定位O(logn) x 插入节点O(1) = O(logn)
/// 删除：定位O(logn) x 删除节点O(1) = O(logn)
///
/// 实现细节
/// - 如果使用首元节点，则使用单链节点实现的逻辑简单
/// - 如果要支持动态扩展高度，则双链节点的逻辑简单
/// ============================================================================================================
// template <typename ElementType, size_t max_level=32,float probable=0.5f>
using ElementType = int;
static constexpr size_t max_level = 32;
static constexpr float probable = 0.5;
class Skip_List
{
public:
    using Node = List_Node_Skiplist<ElementType>;
    constexpr size_t Get_Maxsize() const { return max_level * (1 / probable); }

public:
    // 属性
    // const float cutOff{0.5}; // 确定插入层数上移的几率  0 < prob < 1
    // const int max_level{32}; // 允许的最大链表层数
    // ElementType tailKey{max_level * 1 / cutOff}; // 最大关键字
    // 这些限制会使跳表性能更好。Redis也有类似的限制。
private:
    // 数据成员
    size_t level{}; // 当前链表的层数。没有元素为0层，有元素时最少为1层
    size_t size{};  // 当前元素个数
    // header.size() == level   头结点的元素个数等于当前层数
    std::vector<Node *> header; // 头结点指针数组。支持层级扩展
    // Node *header; // 头结点指针
    Node *tail; // 尾结点指针
public:
    // 关键字小于largeKey 且数对个数size最多为maxPairs，
    Skip_List()
    {
        static_assert(probable > 0 && probable < 1, "probable must be in (0, 1)");
        // header = new Node(max_level + 1);//初始化头结点
        header.reserve(32); // 预留32层的头结点
    }
    ~Skip_List()
    {
        // 删除所有结点和数组

        Node *node{header[0]};
        Node *node_delete;
        // 从headerNode开始，延数对链的方向删除
        while (node)
        {
            node_delete = node;
            node = node->next[0];
            delete node_delete;
        }
    }

private:
    // 随机一个元素所在的层级，用于绝对实际的层数
    int _Determine_Level() const
    {
        // 获取一个表示链表级且不大于max_level的随机数
        static const auto threshold = RAND_MAX * probable;
        int lev{}; // 0层作为最底层存放元素

        // lev限制在[0,level]中，防止lev远超过当前level过多，如当前skip_list3层，获取了一个60层的结果，则4-59层的链表都是空的
        // 使用while+随机计算lev可以控制每一层的节点的数量
        while (lev <= level && rand() <= threshold)
            ++lev;
        return lev; // 返回的结果∈[0,level+1] level+1表示向上增加一层
    }
    // 搜索并把在每一级链表搜索时遇到的最后一个结点存储起来
    // 返回插入节点的前一个节点，可能等值
    std::vector<Node *> _Loacte_Previous_Node(const ElementType &element) const
    {
        if (!size) // 没有一个元素的时候直接返回头节点
            return {header};

        std::vector<Node *> last_node_at_level(level + 1, nullptr);

        for (int i = level; i >= 0; --i)
        {
            // 自上而下遍历每一级节点
            Node *previous_node = header[i];
            if (header[i]->element >= element)
                continue; // 没有比element小的元素，则跳过。默认为nullptr

            while (previous_node->next[i] && previous_node->next[i]->element < element) // 同一层中定位到最接近key的上一个元素节点
                previous_node = previous_node->next[i];
            last_node_at_level[i] = previous_node; // 第i级插入位置的前驱节点,如果没有，或者前驱为header[level],则为nullptr
        }
        return last_node_at_level;
    }

public:
    // 判断是否为空
    bool empty() const { return size; }

    // 返回元素个数
    int Size() const { return size; }

    // 搜索元素，
    // 返回所在的节点
    std::optional<ElementType> Search(const ElementType &element) const
    {
        // 从最高级链表开始查找，在每一级链表中，从左边尽可能逼近要查找的记录

        for (size_t i{}; i <= level; ++i)
        {
            size_t current_level{level - i};//从上到下遍历每一层
            if (header[current_level]->element > element)
                continue;
            auto node = header[current_level];
            while (node->next[current_level] && node->element < element)
                node = node->next[current_level];
            if (node->element== element)
                return node->element;
            else
                return std::nullopt;
        }
        return std::nullopt;
    }

    // 删除元素
    void Element_Delete(const ElementType &element)
    {

        // 查看是否存在关键字匹配的数对
        const std::vector<Node *> previous_node = _Loacte_Previous_Node(element);
        Node *node_delete = previous_node[0] ? previous_node[0]->next[0] : header[0];
        if (!node_delete || node_delete->element != element)
            // 不存在
            throw std::runtime_error("No such element " + std::to_string(element));
        // return;

        // 如果删除最高层级唯一的节点，则晚点降低一层跳表高度
        unsigned short decrease_level{};
        if (node_delete->level == this->level)
        {
            for (auto level = node_delete->level; level > 0; --level)//最底层0层不用减
            {
                if (header[level] == node_delete && node_delete->next[level] == nullptr)
                {
                    ++decrease_level;
                    continue;
                }
                break;
            }
        }

        // 从跳表中删除结点
        for (int i = 0; i <= node_delete->level ; ++i)
        {
            if (previous_node[i] && previous_node[i]->next[i] == node_delete)
                previous_node[i]->next[i] = node_delete->next[i];
            else
                header[i] = node_delete->next[i];
        }

        if (node_delete->next[0] == nullptr) // 删除尾节点，更新tail
            tail = previous_node[0];

        delete node_delete;
        --size;

        if (decrease_level && size) // size用于防止删除最后一个元素时，level减为-1
            level -= decrease_level;
    }

    // 插入元素
    void Element_Insert(const ElementType &element)
    {

        // 查看和插入数对相同关键字的数对是否已经存在
        std::vector<Node *> previous_node = _Loacte_Previous_Node(element); // 定位到插入的节点位置
        // Node *theNode = _Loacte_Previous_Node(key);
        if (!previous_node.empty() && previous_node[0] && previous_node[0] != header[0] && previous_node[0] != tail && previous_node[0]->next[0]->element == element)
        {
            // // 已经存在，则更新数对的值
            // node_insert->element = key;
            // return;
            throw std::runtime_error("Key already exists");
        }

        // 如果不存在，则确定新结点所在的级链表
        int level_target = _Determine_Level(); // 新结点的级
        // 保证级theLevel <= levels + 1
        if (level_target > level) // 如果计算的层级结果大于当前的最大层级，则跳表增加一个层级
        {
            ++level; // 插入元素最多+1层，但是删除时可以一次减少多层
            // previous_node.push_back(header); // 添加了一层，则最顶层的前驱必定为header。用于后续的顶层节点插入
            if (header.size() < level + 1) // 头结点层数个数 < 需要的层数 ： size从1起始，level从0起始
                header.emplace_back(nullptr);
            // if(previous_node.size() < level_target + 1)
            //     previous_node.emplace_back(nullptr);
        }

        // 在结点theNode之后插入新结点
        Node *node = new Node(element, level_target);
        for (int i = 0; i <= level_target; ++i)
        {
            // 自下而上，插入i级链表
            // if (previous_node.size()>=level_target-1 && previous_node[i])
            if (i<previous_node.size() && previous_node[i])
            { // 当前层非空
                node->next[i] = previous_node[i]->next[i];
                previous_node[i]->next[i] = node;
            }
            else
            {
                if (header.empty() || header.size() < i) // 在新层插入时先添加一个元素
                    header.emplace_back(nullptr);
                node->next[i] = header[i]; // header[i]就是刚刚emplace_back(nullptr)
                header[i] = node;
            }
        }
        if (!node->next[0])
            tail = node;

        ++size;
        return;
    }

public:
    void Show(const std::string_view &info = "", bool only_elements = false)
    {

        if (only_elements)
        {
            // 遍历0级链表
            for (Node *node = header[0]; node; node = node->next[0])
                std::cout << "[" << node->element << "] ";
            std::cout << "\tSize : " << size << std::endl;
        }
        else
        {
            std::cout << info << " ============= Level " << level << " =========== Size " << size << " =============== " << std::endl;
            for (size_t i{}; i <= level; ++i)
            {
                size_t current_level{level - i};//从上到下遍历每一层
                std::cout << "Level " << current_level << " : ";
                for (Node *node = header[current_level]; node; node = node->next[current_level])
                    std::cout << "[" << node->element << "] ";
                std::cout << std::endl;
            }
            std::cout << "============================================================= " << std::endl;
        }
    }
};
