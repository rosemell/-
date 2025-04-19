#define _CRT_SECURE_NO_WARNINGS 1
#include"PageCache.hpp"
PageCache PageCache::_sInstan;

// 获取一个K页的span
Span* PageCache::NewSpan(size_t k)
{
	if (k > NPAGES - 1)
	{
		//Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)SystemAlloc(k) >> PAGE_SHIFT;
		span->_n = k;
		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);
		return span;
	}
	assert(k > 0 && k < NPAGES);
	if (!_spanLists[k].Empty())
	{
		Span* kspan = _spanLists[k].PopFront();
		// 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
		PAGE_ID i = 0;
		for (i = 0; i < kspan->_n; ++i)
		{
			//_idSpanMap[i + kspan->_pageId] = kspan;
			_idSpanMap.set(i + kspan->_pageId, kspan);

		}
		return kspan;
	}
	size_t i = k;
	for (i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nspan = _spanLists[i].PopFront();
			//Span* kspan = new Span;
			Span* kspan = _spanPool.New();
			// 在nSpan的头部切一个k页下来
			// k页span返回
			// nSpan再挂到对应映射的位置
			kspan->_pageId = nspan->_pageId;
			kspan->_n = k;
			nspan->_pageId += k;

			nspan->_n -= k;
			_spanLists[nspan->_n].PushFront(nspan);
			// 存储nSpan的首位页号跟nSpan映射，方便page cache回收内存时
			// 进行的合并查找
			//_idSpanMap[nspan->_pageId + nspan->_n - 1]=nspan;
			//_idSpanMap[nspan->_pageId]=nspan;
			_idSpanMap.set(nspan->_pageId + nspan->_n - 1, nspan);
			_idSpanMap.set(nspan->_pageId, nspan);

			// 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
			PAGE_ID i = 0;
			for (i = 0; i < kspan->_n; ++i)
			{
				//_idSpanMap[i+kspan->_pageId] = kspan;

				_idSpanMap.set(i + kspan->_pageId, kspan);

			}
			return kspan;

		}
	}
	//没有大小足够的span了，弄个新的
	//Span* nspan = new Span;
	Span* nspan = _spanPool.New();
	void* mem = SystemAlloc(NPAGES - 1);
	nspan->_pageId = (size_t)mem >> PAGE_SHIFT;
	nspan->_n = NPAGES - 1;
	_spanLists[NPAGES - 1].PushFront(nspan);
	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//return _idSpanMap[(PAGE_ID)obj >> PAGE_SHIFT];
	PAGE_ID n = (PAGE_ID)obj >> PAGE_SHIFT;
	//std::unique_lock<std::mutex> lock(_pageMtx);
	//auto ret= _idSpanMap.find(n);
	//if (ret!=_idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	assert(false);
	//	return nullptr;
	//}
	auto ret = _idSpanMap.get(n);
	assert(ret != nullptr);
	return (Span*)ret;
}
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//合并
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}

	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		//auto ret = _idSpanMap.find(prevId);
		//// 前面的页号没有，不合并了
		//if (ret == _idSpanMap.end())
		//{
		//	break;
		//}
		auto ret = _idSpanMap.get(prevId);
		if (ret == nullptr)
		{
			break;
		}
		Span* prev = (Span*)ret;


		// 前面相邻页的span在使用，不合并了
		if (prev->_isUse == true)
		{
			break;
		}
		// 合并出超过128页的span没办法管理，不合并了
		if (prev->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		span->_pageId = prev->_pageId;
		span->_n += prev->_n;

		_spanLists[prev->_n].Erase(prev);
		//delete prev;
		_spanPool.Delete(prev);

	}

	while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		//auto ret = _idSpanMap.find(nextId);
		//if (ret == _idSpanMap.end())
		//{
		//	break;
		//}
		//Span* next = ret->second;

		auto ret = _idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}
		Span* next = (Span*)ret;

		if (true == next->_isUse)
		{
			break;
		}
		if (next->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		span->_n += next->_n;
		_spanLists[next->_n].Erase(next);
		//delete next;
		_spanPool.Delete(next);

	}
	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;
	//_idSpanMap[span->_pageId + span->_n - 1]=span;
	//_idSpanMap[span->_pageId] = span;
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
	_idSpanMap.set(span->_pageId, span);
}