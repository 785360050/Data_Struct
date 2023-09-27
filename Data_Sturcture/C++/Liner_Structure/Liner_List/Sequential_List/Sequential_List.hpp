#pragma once

// #include "Object.h"
#include "../Liner_List.hpp"
#include <iostream>
#include <cstring> //memcpy

namespace Storage_Structure
{

	template <typename ElementType>
	class Sequential_List : public Liner_List<ElementType>
	{
	protected:
		size_t capcity{}; /// 当前最大容量

	protected:
		/// @param pos 位序从1开始
		static size_t Index(size_t pos)
		{ /// 位序转元素索引
			if (pos == 0)
				throw std::underflow_error("size_t pos == 0");
			return --pos;
		}

	public: /// 链表操作
		void List_Clear() override
		{
			if (!this->storage)
				throw std::runtime_error("List is not exist");
			memset(this->storage, ElementType{}, capcity * sizeof(capcity));
			this->size = 0;
		}
		// 返回当前最大容量
		size_t Get_Capcity() const { return capcity; }
		// 返回第pos个元素的元素值
		ElementType &operator[](size_t pos) override
		{
			return this->storage[Index(pos)];
		}

	public: /// 元素操作
		// 插入元素
		virtual void Element_Insert(size_t pos, ElementType elem) = 0;
		// 删除元素
		virtual void Element_Delete(size_t pos) = 0;
		// 修改顺序表List第pos个位置上的元素为elem
		void Element_Update(size_t pos, ElementType elem) override
		{
			// operator[](pos) = elem;//有点丑
			this->storage[pos] = elem;
		}

	public:
		// 显示线性表所有信息
		virtual void List_Show(const string &string)
		{
			std::cout << string << std::endl
					  << "[Size/Capcity]:\n"
					  << " [" << this->size << '/' << capcity << ']' << std::endl
					  << "storage->";
			for (int index = 0; index < capcity; index++)
				std::cout << '[' << index << ':' << this->storage[index] << "]-";
			std::cout << "End\n";
		}
	};
};

/// 静态数组
template <typename ElementType, size_t capcity>
class Sequential_List_Static : public Storage_Structure::Sequential_List<ElementType>
{
	ElementType array[capcity]{}; // 栈上申请空间
public:
	Sequential_List_Static()
		// : Storage_Structure::Sequential_List<ElementType>(){};
	{
		this->storage = array;
		this->capcity = capcity;
		// this->storage=new ElementType[capcity]{};
	}
	// Sequential_List_Static(size_t capcity)
	// 	: Storage_Structure::Sequential_List<ElementType>(capcity){};

public:
	void Element_Insert(size_t pos, ElementType elem) override
	{ /// n个元素有n+1个可插入位置,存储空间不足时不扩展并报错，位置pos非法时候抛出异常并终止插入元素
		if (pos < 0 || pos > this->size + 1)
			throw std::out_of_range("List insert failed: Position out of range");
		if (this->size >= this->capcity)
			throw std::runtime_error("List insert failed: List is full");

		if (this->size == 0)
			this->storage[0] = elem;
		else
		{ /// 从后往前，把当前索引向后搬
			for (size_t index = this->Index(this->size); this->Index(pos) <= index; index--)
				this->storage[index + 1] = this->storage[index];
			this->storage[this->Index(pos)] = elem;
		}
		++this->size;
	}
	void Element_Delete(size_t pos) override
	{
		this->Index(pos); // Check pos is valid
		for (size_t i = this->Index(pos); i <= this->Index(this->size) - 1; i++)
			this->storage[i] = this->storage[i + 1];
		this->storage[this->size - 1] = 0; /// 末尾补0
		--this->size;
	}
};

/// 动态数组
template <typename ElementType>
class Sequential_List_Dynamic : public Storage_Structure::Sequential_List<ElementType>
{
public:
	Sequential_List_Dynamic()
		: Storage_Structure::Sequential_List<ElementType>(){};
	Sequential_List_Dynamic(size_t capcity)
	{ // capcity必须>0
		if (capcity < 1)
			throw std::invalid_argument("List Init Failed: capcity must be greater than 1");
		this->storage = new ElementType[capcity]{}; /// 每个元素初始化为ElementType默认初始值
		this->capcity = capcity;
		if (!this->storage)
			throw std::bad_alloc();
	}

	Sequential_List_Dynamic(const Sequential_List_Dynamic &other)
	{
		if(this==other)
			throw std::invalid_argument("Self copy is invalid");
		this->size = other.size;
		this->capcity = other.capcity;
		this->storage = new ElementType[this->capcity]{};
		for(size_t i=0; i<other.size; i++)
			this->storage[i]=other.storage[i];
	}
	Sequential_List_Dynamic operator=(const Sequential_List_Dynamic &other)
	{
		if (this == other)
			throw std::invalid_argument("Self copy is invalid");
		this->size = other.size;
		this->capcity = other.capcity;
		this->storage = new ElementType[this->capcity]{};
		for (size_t i = 0; i < other.size; i++)
			this->storage[i] = other.storage[i];
	}
	
	Sequential_List_Dynamic(Sequential_List_Dynamic &&other)
	{
		if (this == other)
			throw std::invalid_argument("Self move Detected");
		this->size    = other.size;
		this->capcity = other.capcity;
		this->storage = other.storage;
		other.storage = nullptr;
		other.capcity = 0;
		other.size    = 0;
	}
	Sequential_List_Dynamic operator=(Sequential_List_Dynamic &&other)
	{
		if (this == other)
			throw std::invalid_argument("Self move Detected");
		this->size    = other.size;
		this->capcity = other.capcity;
		this->storage = other.storage;
		other.storage = nullptr;
		other.capcity = 0;
		other.size    = 0;
	}

	~Sequential_List_Dynamic()
	{
		if (this->storage)
		{
			delete[] this->storage;
			this->storage = nullptr;
			this->size = 0;
		}
	}

protected:
	// 以2倍为单位扩展收缩空间
	void Expand()
	{ /// 重新申请2倍的空间，移动原有数据至该空间
		if (this->storage)
		{
			auto expand = new ElementType[this->capcity * 2]{};
			memcpy(expand, this->storage, sizeof(ElementType) * this->capcity);
			delete[] this->storage;
			this->storage = expand;
			this->capcity *= 2;
		}
	}
	void Shrink()
	{
		if (this->storage)
		{
			auto shrink = new ElementType[this->capcity / 2]{};
			memcpy(shrink, this->storage, sizeof(ElementType) * this->size);
			delete[] this->storage;
			this->storage = shrink;
			this->capcity /= 2;
		}
	}

public: /// 元素操作
	void Element_Insert(size_t pos, ElementType elem)
	{ /// n个元素有n+1个可插入位置,存储空间不足时扩展为两倍，位置pos非法时候抛出异常并终止插入元素
		try
		{
			if (pos < 0 || pos > this->size + 1)
				throw 2;
		}
		catch (...)
		{
			std::cout << "List insert failed: Position out of range" << std::endl;
			return;
		}
		if (this->size >= this->capcity)
			Expand(); /// 空间扩展为2倍
		// this->List_Show(" ");
		if (this->size == 0)
			this->storage[0] = elem;
		else
		{ /// 从后往前，把当前索引向后搬
			for (size_t index = this->Index(this->size); this->Index(pos) <= index; index--)
				this->storage[index + 1] = this->storage[index];
			this->storage[this->Index(pos)] = elem;
		}
		++this->size;
	}
	void Element_Delete(size_t pos)
	{
		this->Index(pos);//check pos valid
		for (size_t i = this->Index(pos); i <= this->Index(this->size) - 1; i++)
			this->storage[i] = this->storage[i + 1];
		this->storage[this->size - 1] = 0;
		--this->size;
		/// 元素个数<=1/2最大容量 时收缩空间
		if (this->size <= this->capcity / 2)
			Shrink();
	}
};