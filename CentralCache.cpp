#include"CentralCache.hpp"
#include"PageCache.hpp"

CentralCache CentralCache::_sInst;


void CentralCache::ReleaseListToSpans(void* start, size_t byte_size)
{
	size_t index = SizeClass::Index(byte_size);
	_spanLists[index]._mtx.lock();
	while(start)
	{
		void*next = NextObj(start);
		Span*span=PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList; 
		span->_freeList = start;
		--span->_useCount;
		if (0 == span->_useCount)
		{
			_spanLists->Erase(span);
			span->_freeList = nullptr;
			span->_prev= nullptr;
			span->_next= nullptr;
			
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}
		start = next;
	}

	_spanLists[index]._mtx.unlock();
}
// 获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	Span*it=list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}
	// 先把central cache的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞
	// 等结束了再锁回去
	list._mtx.unlock();
	
	PageCache::GetInstance()->_pageMtx.lock();
	Span*span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objsize = size;
	PageCache::GetInstance()->_pageMtx.unlock();
	// 对获取span进行切分，不需要加锁，因为这会其他线程访问不到这个span
	// 计算span的大块内存的起始地址和大块内存的大小(字节数)
	char* start = (char*)(span->_pageId<<PAGE_SHIFT);
	size_t n = span->_n << PAGE_SHIFT;
	char* end = n + start;

	span->_freeList = start;
	void* prev = span->_freeList;
	start += size;
	
	while (start<end)
	{
		NextObj(prev)= start;
		//prev = start;			//tail = NextObj(tail);
		prev = NextObj(prev);	//prev = start;	
		start += size;
	}
	NextObj(prev) = nullptr;
	list._mtx.lock();
	list.PushFront(span);
	return span;
}
// 从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);
	// 从span中获取batchNum个对象
	// 如果不够batchNum个，有多少拿多少
	size_t actualNum = 1;
	size_t i = 0;
	end = span->_freeList;
	start = span->_freeList;
	while (i<batchNum-1&&NextObj(end))
	{
		++i;
		++actualNum;
		end = NextObj(end);
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;
	_spanLists[index]._mtx.unlock();
	return actualNum;
}