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
            
            // 放首尾进去
            _idSpanMap[nSpan->_pageId] = nSpan;
            _idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;

            for(PAGE_ID i = 0; i < kSpan->_n; ++i)
            {
                _idSpanMap[kSpan->_pageId + i] = kSpan;
            }

            return kSpan;
        }
    }

    // 没有 更大的 Span 了 找堆要一个 最大的
    Span* bigSpan = new Span;
    void* ptr = malloc(NPAGES - 1 * 8 * 1024);

    bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
    bigSpan->_n = NPAGES - 1;

    _spanLists[bigSpan->_n].PushFront(bigSpan);
    _idSpanMap[bigSpan->_pageId] = bigSpan;

    return NewSpan(k);   // 递归调用一下自己  锁 在 centralcache的时候 不会死锁
}   


Span* PageCache::MapObjToSpan(void* obj)
{
    PAGE_ID pageId =  PAGE_ID(obj) >> PAGE_SHIFT;

    auto ret = _idSpanMap.find(pageId);
    if(ret != _idSpanMap.end())
    {
        return ret->second;
    }
    else
    {
        assert(false);
        return nullptr;
    }

}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
    //对 span 前后的页进行合并 缓解内存碎片
    while(1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        auto ret = _idSpanMap.find(prevId);
        
        if(ret == _idSpanMap.end())
        {
            break;
        }

        // 正在使用  不合并
        Span* prevSpan = ret->second;
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
        delete prevSpan; // ?
    }

    while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = ret->second;
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
		delete nextSpan; // ?
	}

    _spanLists[span->_n].PushFront(span);
    span->_isUse = false;
    _idSpanMap[span->_pageId] = span;
    _idSpanMap[span->_pageId + span->_n - 1] = span;
}