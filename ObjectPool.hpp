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
		//����ʹ�ñ��ͷŵ��ڴ�
		if (_freelist)
		{
			void* next = *((void**)_freelist);
			res = (T*)_freelist;
			_freelist = next;
		}
		else
		{
			//�����Ҫ�Ĵ�СС��ָ���С����ô���׸�ָ���С
			//							��Ϊ�˺����γ�����
			size_t NeedN = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			if (_lastmemory<NeedN)//�ռ䲻��
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
		//�ֶ����ù��캯��
		new(res)T;
		return res;
	}
	void Delete(T*obj)
	{
		obj-> ~T();
		//ͷ��
		*(void**)obj = _freelist;
		_freelist = obj;
	}
private:
	char* _memory=nullptr;//��Ƭ�ڴ棬��Ҫ�ڴ����ȡ
	size_t _lastmemory=0;//��Ƭ�ڴ�ʣ���ڴ�
	void* _freelist=nullptr;//�ͷŵ��ڴ�����������
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
//	// �����ͷŵ��ִ�
//	const size_t Rounds = 5;
//
//	// ÿ�������ͷŶ��ٴ�
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