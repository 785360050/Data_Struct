#pragma once

#include <iostream>
#include "ADT.hpp"

/// @brief 逻辑结构。作为基类使用
namespace Logic
{

	template <typename ElementType>
	class Linear_List
	{
	protected:
		// ElementType *storage{}; /// 指向存储结构.
		// 由于该类是通用的线性表抽象类，所以取消了NodeType的模板参数类型
		// 链表和数组的存储元素类型不同，所以不能提取到父类
		size_t size{}; /// 当前元素个数

	public:
		Linear_List() = default;
		Linear_List(size_t size) : size{size} {};
		virtual ~Linear_List() = default;

	public: /// 表操作
		// 清空所有元素
		virtual void List_Clear() = 0;
		// 判断是否表空
		bool Is_Empty() const { return size == 0; }
		// 返回当前表长(元素个数)
		size_t Get_Size() const { return size; }

		/// @brief 返回第pos个元素的引用
		/// @param pos 位序[1,...]，不是从0开始的数组下标
		virtual ElementType &operator[](size_t pos) = 0;

	public: /// 元素操作
		// 插入元素
		virtual void Element_Insert(size_t pos,const ElementType& elem) = 0;
		virtual void Element_Insert(size_t pos, ElementType&& elem) = 0;
		// 删除元素
		virtual void Element_Delete(size_t pos) = 0;
		// 更新元素
		virtual void Element_Update(size_t pos, ElementType elem) = 0;

	public:
		// 显示线性表所有信息
		virtual void List_Show(const string &string = "") = 0;
	};

};
static_assert(ADT::Linear_List<Logic::Linear_List<int>, int>, "Linear_List must meet the Linear_List concept");