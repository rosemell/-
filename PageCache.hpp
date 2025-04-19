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

	// 获取一个K页的span
	Span* NewSpan(size_t k);

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
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
	//基数树在写的时候不会影响数据结构
	TCMalloc_PageMap2<32-PAGE_SHIFT> _idSpanMap;
	SpanList _spanLists[NPAGES];
};