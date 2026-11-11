#pragma once
#include <iostream>
#include <assert.h>
#include <vector>
#include <mutex>
#include "ObjectPool.hpp"
#include <atomic>

using std::cout;
using std::endl;

const static size_t MAX_BYTES = 256 * 1024;
const static size_t NFREELIST = 208;
const static size_t NPAGES = 129;
const static size_t PAGE_SHIFT = 13;


#ifdef _WIN64
    typedef unsigned long long PAGE_ID;
#elif _WIN32
    typedef size_t PAGE_ID;
#else
//
#endif



static void*& NextObj(void* ptr)  // 加 static 防链接错误
{
    return *((void**)ptr);
}


class FreeList
{
public:
    void PushFront(void* ptr)
    {
        assert(ptr);
        
        NextObj(ptr) = _freeList;
        _freeList = ptr;
        _size++;
    }
    

    void* PopFront()
    {
        assert(_freeList);

        void* ret = _freeList;
        _freeList = NextObj(_freeList);
        _size--;
        return ret;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }

    size_t& MaxSize()
    {
        return _maxSize;
    }

    void PushRange(void* start, void* end, size_t n)
    {
        NextObj(end) = _freeList;
        _freeList = start;
        
        _size += n;
    }

    void PopRange(void*& start, void*& end, size_t size)
    {
        start = _freeList;
        end = _freeList;
        size_t i = 0;
        while(i < size)
        {
            end = NextObj(end);
        }

        _freeList = NextObj(end);
        NextObj(end) = nullptr;
        _size -= size;
    }

    size_t Size()
    {
        return _size;
    }

private:
    void* _freeList = nullptr;
    size_t _maxSize = 1;  // 慢启动的时候去调节的
    size_t _size;
};

// 计算对象大小 对齐
class SizeClass
{
public:
    // 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐	    freelist[0,16)
	// [128+1,1024]				16byte对齐	    freelist[16,72)
	// [1024+1,8*1024]			128byte对齐	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐   freelist[184,208)

    // 怎么算对齐     
    // 算出 最高位   然后    去掉所有的低位
    // 1 + 7   8    
    // 2 + 7   9   
    static inline size_t _RoundUp(size_t bytes,  size_t alignNum) // 子函数
    {
        return (bytes + alignNum - 1) & ~(alignNum - 1);
    }


    static inline size_t RoundUp(size_t size)  // 算对齐到哪个大小的桶
    {
        if(size <= 128)
        {
            return _RoundUp(size, 8);
        }
        else if(size <= 1024)
        {
            return _RoundUp(size, 16);
        }
        else if(size  <= 8 * 1024)
        {
            return _RoundUp(size, 128);
        }
        else if(size <= 64 * 1024)
        {
            return _RoundUp(size, 1024);
        }
        else if(size <= 256 * 1024)
        {
            return _RoundUp(size, 8 * 1024);
        }
        else
        {
            return _RoundUp(size, 1 << PAGE_SHIFT);
            // 大于 256 k  以页为单位对齐
        }

    } 

    // 这里传到是 对齐数是 2 的几次方
    static inline  size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    // 超出一块去 然后右移就算第几个 这是从 0 开始 所以 减一 


    // 算映射到哪个桶的第几号
    static inline size_t Index(size_t bytes)
    {   
        assert(bytes < MAX_BYTES);

        // 每个不同对齐数的区间 有多少个链
        static int group_array[4] = {16, 56, 56, 56};
        if(bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if(bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if(bytes <= 8 * 1024)
        {
            return _Index(bytes  - 1024, 10) + group_array[0] + group_array[1];
        }
        else if (bytes <= 64 * 1024)
        {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024)
        {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else
        {
			assert(false);
		}

        return -1;
    }

    static size_t NumMoveSize(size_t size)
    {
        assert(size > 0);   // [2, 512]

        int num = MAX_BYTES / size;
        // 小对象 批量一次上限高
        // 大对象 批量一次上限低`
        
        
        if(num < 2)
            num = 2;
        //
        if(num > 512)
            num = 512;
        return num;
    }


    static size_t NumMovePage(size_t size)
    {
        size_t num = NumMoveSize(size); // ? 为什么 不是取小了一次吗
        size_t npage = size * num;

        npage >>= PAGE_SHIFT;
        if(npage == 0) 
            npage = 1;
        //
        return npage;
    }   


};

struct Span
{
    PAGE_ID _pageId = 0; // 大块内存起始页的页号
    size_t _n = 0;       // span 管理的页的数量

    Span* _next = nullptr;
    Span* _prev = nullptr;

    size_t _usecount = 0;  // 切好的小块内存 被分配给 threadchace 使用的数量
    void* _freeList = nullptr;
    // 小块内存挂的链表

    bool _isUse = false;

    size_t _objSize = 0;

};

// 带头双向循环链表
class SpanList
{
public:
    SpanList()
    {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    } // 初始化

    void Insert(Span* pos, Span* newSpan)
    {
        assert(pos); // 别忘了断言
        assert(newSpan);

        // prev  newSpan  pos
        Span* prev = pos->_prev;
        prev->_next = newSpan;
        newSpan ->_next = pos; 
        pos->_prev = newSpan;
    }

    void Erase(Span* pos)
    {
        assert(pos);
        assert(pos != _head);

        Span* prev = pos->_prev;
        prev->_next = pos->_next;
        pos->_next->_prev = prev;

    }

    void PushFront(Span* span)
    {
        Span* next = _head->_next;
        span->_next = next;
        
        next->_prev = span;
        span->_prev = _head;
    }

    Span* Begin()
    {
        return _head->_next;
    }

    Span* End()
    {
        return _head->_prev;
    }

    bool Empty()
    {
        return _head->_next == _head;
    }


    Span* PopFront()
    {
        Span* ret = _head->_next;
        ret->_next->_prev = _head;
        _head->_next = ret->_next;

        return ret;
    }

private:
    Span* _head;
public:
    std::mutex _mtx;  // 桶锁 
};

