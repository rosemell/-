#pragma once
#include"common.hpp"

class CentralCache
{
public:

	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	// ��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t size);
	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	SpanList _spanLists[NFREELIST];


	//����ģʽ
private:
	CentralCache()
	{}

	CentralCache operator=(CentralCache) = delete;
	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;
};


