#include "PageCache.hpp"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k)
{
    assert(k > 0 && k < NPAGES);

    if(!_spanLists[k].Empty())
    {
        return _spanLists[k].PopFront();
    }

    // 这个桶是空的 用大块 span 来切

    for(size_t i = k + 1; i < NPAGES; i++)
    {
        if(!_spanLists[i].Empty())
        {
            Span* nSpan = _spanLists[i].PopFront();
            Span* kSpan = new Span;

            // 从 nSpan 头上切 k 页下来
            kSpan->_pageId = nSpan->_pageId;
            kSpan->_n = k;

            nSpan->_n -= k;
            nSpan->_pageId += k;
            
            _spanLists[nSpan->_n].PushFront(nSpan);

            return kSpan;
        }
    }

    // 没有 更大的 Span 了 找堆要一个 最大的
    Span* bigSpan = new Span;
    void* ptr = malloc(NPAGES - 1 * 8 * 1024);

    bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
    bigSpan->_n = NPAGES - 1;

    _spanLists[bigSpan->_n].PushFront(bigSpan);

    return NewSpan(k);   // 递归调用一下自己  锁 在 centralcache的时候 不会死锁
}   