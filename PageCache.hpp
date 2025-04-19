#pragma once
#include"common.hpp"
#include"ObjectPool.hpp"
#include"PageMap.hpp"

class PageCache
{
public:
	static PageCache*GetInstance()
	{
		return &_sInstan;
	}

	// ��ȡһ��Kҳ��span
	Span* NewSpan(size_t k);

	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);
public:
	std::mutex _pageMtx;
	
private:
	void operator=(PageCache) = delete;
	PageCache()
	{}
	PageCache(const PageCache&) = delete;
	static PageCache _sInstan;
private:
	ObjectPool<Span> _spanPool;
	//std::unordered_map<PAGE_ID, Span*>_idSpanMap;
	//��������д��ʱ�򲻻�Ӱ�����ݽṹ
	TCMalloc_PageMap2<32-PAGE_SHIFT> _idSpanMap;
	SpanList _spanLists[NPAGES];
};