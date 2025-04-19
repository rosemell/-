#pragma once
#include<stdlib.h>
#include<vector>
#include<assert.h>
#include<thread>
#include<mutex>
#include<algorithm>
#include <iostream>
#include<unordered_map>
#include<vector>
using std::cout;
using std::endl;



#ifdef _WIN32
#define NOMINMAX
#include<Windows.h>
#else
// linux��brk mmap��
#endif

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
// linux
#endif
static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELIST = 208; 
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 13;

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
#define NOMINMAX
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif
}

inline void*& NextObj(void* obj)
{
	return *(void**)obj;
}
class SizeClass
{
public:
	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����	    freelist[0,16)
	// [128+1,1024]				16byte����	    freelist[16,72)
	// [1024+1,8*1024]			128byte����	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte����   freelist[184,208)
	static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		return (size + alignNum - 1) & ~(alignNum - 1);
	}
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 126 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}
	static inline size_t _Index(size_t size, size_t  align_shift)
	{
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	static inline size_t Index(size_t size)
	{
		assert(size <= MAX_BYTES);

		// ÿ�������ж��ٸ���
		static int group_array[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{
			return _Index(size, 3);
		}
		else if (size <= 1024)
		{
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024)
		{
			return _Index(size - 1024, 7) + group_array[0] + group_array[1];
		}
		else if (size <= 64 * 1024)
		{
			return _Index(size - 8 * 1024, 10) + group_array[0] + group_array[1] + group_array[2];
		}
		else if (size <= 126 * 1024)
		{
			return _Index(size - 64 * 1024, 13) + group_array[0] + group_array[1] + group_array[2] + group_array[3];
		}
		else
		{
			assert(false);
		}
		return -1;
	}
	// һ��thread cache�����Ļ����ȡ���ٸ�
	static inline size_t NumMoveSize(size_t size)
	{
		size_t n = MAX_BYTES / size;
		if (n > 512)
		{
			n = 512;
		}
		if (n < 2)
		{
			n = 2;
		}
		return n;
	}
	static inline size_t NumMovePage(size_t size)
	{
		size_t npage = size * NumMoveSize(size);
		npage >>= PAGE_SHIFT;
		if (0 == npage)
		{
			npage = 1;
		}
		return npage;
	}

};
class FreeList
{
private:


public:
	void PushRange(void* start, void* end,size_t size)
	{
		assert(start);
		assert(end);
		NextObj(end) = _freelist;
		_freelist = start;
		_size += size;
	}
	void PopRange(void*& start, void*& end, size_t size)
	{
		assert(size<=_size);
		start = _freelist;
		end = _freelist;
		size_t i = 0;
		for (i = 0; i < size-1; ++i)
		{
			end = NextObj(end);
		}
		_freelist = NextObj(end);
		NextObj(end) = nullptr;
		_size -= size;
	}
	size_t Size()
 	{
		return _size;
	}
	void Push(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freelist;
		_freelist = obj;

		++_size;
	}
	void* Pop()
	{
		assert(_freelist);
		void* obj = _freelist;
		_freelist = NextObj(_freelist);
		--_size;
		return obj;
	}
	bool Empty()
	{
		return _freelist == nullptr;
	}
	size_t& MaxSize()
	{
		return _maxSize;
	}
private:
	void* _freelist = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};

struct Span
{
public:
	PAGE_ID _pageId = 0; // ����ڴ���ʼҳ��ҳ��
	size_t  _n = 0;      // ҳ������

	Span* _next = nullptr;	// ˫������Ľṹ
	Span* _prev = nullptr;

	size_t _objsize;			//�кõ��ڴ���С
	size_t _useCount = 0; // �к�С���ڴ棬�������thread cache�ļ���
	void* _freeList = nullptr;  // �кõ�С���ڴ����������

	bool _isUse = false;          // �Ƿ��ڱ�ʹ��
};
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}
	Span* PopFront()
	{
		Span* front = Begin();
		Erase(front);
		return front;
	}
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;

		newSpan->_prev = prev;
		newSpan->_next= pos;

		prev->_next= newSpan;
		pos->_prev= newSpan;

	}
	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);

		Span* next = pos->_next;
		Span* prev = pos->_prev;
		next->_prev = prev;
		prev->_next = next;
	}
	bool Empty()
	{
		return _head->_next == _head;
	}
private:
	Span* _head;
public:
	std::mutex _mtx;
};