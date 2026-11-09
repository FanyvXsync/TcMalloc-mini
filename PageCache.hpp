#pragma once

#include "Common.hpp"


class PageCache
{
public:
    static PageCache* GetInstance()
    {
        return &_sInst;
    }

    // 获取一个 k 页的 span
    Span* NewSpan(size_t k);

    std::mutex _pageMtx;  // 大锁
private:
    SpanList _spanLists[NPAGES];


private:
    PageCache(){}

    PageCache(const PageCache&) = delete;

    static PageCache _sInst;
};