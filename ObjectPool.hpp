#pragma once
#include"common.hpp"

template<class T>
class ObjectPool
{
private:
	static const size_t memoryMax = 128 * 1024;
	std::mutex _mtx;
public:

	T* New()
	{
		std::lock_guard<std::mutex> lock(_mtx);
		T* res = nullptr;
		//优先使用被释放的内存
		if (_freelist)
		{
			void* next = *((void**)_freelist);
			res = (T*)_freelist;
			_freelist = next;
		}
		else
		{
			//如果需要的大小小于指针大小，那么保底给指针大小
			//							（为了后续形成链表
			size_t NeedN = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			if (_lastmemory<NeedN)//空间不够
			{
				_lastmemory = memoryMax;
				//_memory = (char*)malloc(memoryMax);
				_memory = (char*)SystemAlloc(memoryMax>>13);

				if (nullptr == _memory)
				{
					throw std::bad_alloc();
				}
			}
			res = (T*)_memory;
			_memory += NeedN;
			_lastmemory -= NeedN;
		}
		//手动调用构造函数
		new(res)T;
		return res;
	}
	void Delete(T*obj)
	{
		obj-> ~T();
		//头插
		*(void**)obj = _freelist;
		_freelist = obj;
	}
private:
	char* _memory=nullptr;//大片内存，需要内存从中取
	size_t _lastmemory=0;//大片内存剩余内存
	void* _freelist=nullptr;//释放的内存存在这个链表
};

//
//
//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//
//void TestObjectPool()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 5;
//
//	// 每轮申请释放多少次
//	const size_t N = 100000;
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	size_t begin1 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i];
//		}
//		v1.clear();
//	}
//
//	size_t end1 = clock();
//
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	cout << "new cost time:" << end1 - begin1 << endl;
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}