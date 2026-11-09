#include "Centralcache.hpp"
#include "PageCache.hpp"

CentralCache CentralCache::_Inst;


// 从 centralcache 拿 对象 给 threadcache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
    size_t index = SizeClass::Index(size); // 给 size 定 下标的
    _spanList[index]._mtx.lock();

    Span* span = GetOneSpan(_spanList[index], size);
    assert(span);
    assert(span->_freeList);

    // 从 span 中获取 batchNum 个对象
    // 有多少拿多少

    start = span->_freeList;
    end = start;

    size_t i = 0;
    size_t actualNum = 0;
    while(i < batchNum - 1 && NextObj(end) != nullptr)
    {
        end = NextObj(end);
        actualNum++;
        ++i;
    }

    span->_freeList = NextObj(end); // 拿完了以后更新一个 freeList
    NextObj(end) = nullptr;

    _spanList[index]._mtx.unlock();

    return actualNum;
}


Span* CentralCache::GetOneSpan(SpanList& spanlist, size_t size)
{
    // 先检查当前 spanlist 中有没有 没用完的 span
    Span* it = spanlist.Begin();
    while(it != spanlist.End())
    {
        if(it->_freeList != nullptr)
        {
            return it;
        }
        else
        {
            it = it->_next;
        }
    }

    // 没有 空 span 了
    // 解桶锁  防止其他线程还对象回来阻塞
    spanlist._mtx.unlock();

    PageCache::GetInstance()->_pageMtx.lock();
    Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMoveSize(size));
    PageCache::GetInstance()->_pageMtx.unlock();

    // 切分 span 不用加锁
    // 别的线程不会访问到这个 span 不是 临界资源
    
    // 切分 span ,  先确定大块内存起始地址 和 大块内存的大小(字节)
    char* start = (char*)(span->_pageId << PAGE_SHIFT);  // ?
    size_t bytes = span->_n << PAGE_SHIFT;
    char* end = start + bytes;

    //把大块内存切成自由链表
    //先切一块做头 方便尾插

    span->_freeList = start;
    start += size;

    void* tail = span->_freeList;
    int i = 1;
    while(start < end)
    {
        ++i;
        NextObj(tail) = start;
        tail = NextObj(tail);
        start += size;
    }

    // 切好 span 以后 挂到同里面的时候 再加锁
    spanlist._mtx.lock();
    spanlist.PushFront(span);

    return span;
}