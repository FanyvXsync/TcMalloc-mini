#include "PageCache.hpp"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k)
{
    assert(k > 0 && k < NPAGES);

    if(k > NPAGES - 1)
    {
        void* ptr = SystemAlloc(k);
        //
        Span* span = _spanPool.New();
        span->_pageId = (PAGE_ID) ptr >> PAGE_SHIFT;
        span->_n = k;

        _idSpanMap.set(span->_pageId, span);
        return span;
    }



    if(!_spanLists[k].Empty())
    {
        Span* kspan = _spanLists[k].PopFront();
        // id 和 span 的映射

        for(PAGE_ID i = 0; i < kspan->_n; i++)
        {
            _idSpanMap.set(kspan->_pageId + 1, kspan);
        }

        return kspan;
    }

    // 这个桶是空的 用大块 span 来切

    for(size_t i = k + 1; i < NPAGES; i++)
    {
        if(!_spanLists[i].Empty())
        {
            Span* nSpan = _spanLists[i].PopFront();
            Span* kSpan = _spanPool.New();

            // 从 nSpan 头上切 k 页下来
            kSpan->_pageId = nSpan->_pageId;
            kSpan->_n = k;

            nSpan->_n -= k;
            nSpan->_pageId += k;
            
            _spanLists[nSpan->_n].PushFront(nSpan);
            
            // 放首尾进去
            _idSpanMap.set(nSpan->_pageId, nSpan);
            _idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

            for(PAGE_ID i = 0; i < kSpan->_n; ++i)
            {
                _idSpanMap.set(kSpan->_pageId + i, kSpan);
            }

            return kSpan;
        }
    }

    // 没有 更大的 Span 了 找堆要一个 最大的
    Span* bigSpan = _spanPool.New();
    void* ptr = malloc(NPAGES - 1 * 8 * 1024);

    bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
    bigSpan->_n = NPAGES - 1;

    _spanLists[bigSpan->_n].PushFront(bigSpan);
    _idSpanMap.set(bigSpan->_pageId, bigSpan);

    return NewSpan(k);   // 递归调用一下自己  锁 在 centralcache的时候 不会死锁
}   


Span* PageCache::MapObjToSpan(void* obj)
{
    PAGE_ID pageId =  PAGE_ID(obj) >> PAGE_SHIFT;

    // std::unique_lock<std::mutex> lock(_pageMtx);
    // // auto ret = _idSpanMap.find(pageId);
    // if(ret != _idSpanMap.end())
    // {
    //     return ret->second;
    // }
    // else
    // {
    //     assert(false);
    //     return nullptr;
    // }

    auto ret = (Span*)_idSpanMap.get(pageId);
    assert(ret != nullptr);
    return ret;
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
    // 大于 128 页的 直接还给堆
    if(span->_n > NPAGES - 1)
    {
        void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
        SystemFree(ptr);
    
        _spanPool.Delete(span);
        return ;
    }
    
    
    
    //对 span 前后的页进行合并 缓解内存碎片
    while(1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        // auto ret = _idSpanMap.find(prevId);
        auto ret = (Span*)_idSpanMap.get(prevId); 
        
        if(ret == nullptr)
        {
            break;
        }

        // 正在使用  不合并
        Span* prevSpan = ret;
        if(prevSpan->_isUse)
        {
            break;
        }

        // 合并超了 128 的 也不合并
        if(prevSpan->_n + span->_n > NPAGES - 1)
        {
            break;
        }
        
        span->_pageId = prevSpan->_pageId;
        span->_n += prevSpan->_n;
        
        _spanLists[prevSpan->_n].Erase(prevSpan);
        _spanPool.Delete(prevSpan); // ?
    }

    while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}

		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > NPAGES-1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		_spanPool.Delete(nextSpan); // ?
	}

    _spanLists[span->_n].PushFront(span);
    span->_isUse = false;
    _idSpanMap.set(span->_pageId, span);
    _idSpanMap.set(span->_pageId + span->_n - 1, span);
}