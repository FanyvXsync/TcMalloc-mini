#pragma once

#include "Common.hpp"
#include <unordered_map>


class PageCache
{
public:
    static PageCache* GetInstance()
    {
        return &_sInst;
    }

    // 获取一个 k 页的 span
    Span* NewSpan(size_t k);


    void ReleaseSpanToPageCache(Span* span);
    Span* MapObjToSpan(void* obj);


    std::mutex _pageMtx;  // 大锁
private:
    SpanList _spanLists[NPAGES];
    std::unordered_map<PAGE_ID, Span*> _idSpanMap; 

    ObjectPool<Span> _spanPool;

private:
    PageCache(){}

    PageCache(const PageCache&) = delete;

    static PageCache _sInst;
};