#pragma once
#include "Common.hpp"


// 单例 一个 内存池只有一个
// 饿汉
class CentralCache
{
public:
    static CentralCache* GetInstance()
    {
        return &_Inst;
    }
    
    //获取一个非空 Span
    Span* GetOneSpan(SpanList& list, size_t size); // ?

    //给 thread cache 对象
    size_t FetchRangeObj(void*& start, void*&end, size_t batchNum,  size_t size);


    //threadcache 还给 centralcache
    void ReleaseObjToSpan(void* start,  size_t size);

private:
    SpanList _spanList[NFREELIST];

private:
    CentralCache() {}
    CentralCache(const CentralCache& ch) = delete;

    static CentralCache _Inst;
};