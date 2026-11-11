#pragma once

#include "Threadcache.hpp"
#include "PageCache.hpp"
#include "CentralCache.hpp"
#include <thread>

// 测试 TLS

static void* ConcurrentAlloc(size_t size)
{

    if(size > MAX_BYTES)
    {
        // 两种情况
        // 都让 NewSpan 给我们办了
        size_t alignSize = SizeClass::RoundUp(size);
        size_t kpage = alignSize >> PAGE_SHIFT;

        PageCache::GetInstance()->_pageMtx.lock();
        Span* span = PageCache::GetInstance()->NewSpan(kpage);
        PageCache::GetInstance()->_pageMtx.unlock();
        void* ptr = (void*) (span->_pageId << PAGE_SHIFT);

        return ptr;
    }
    else
    {
        if(pTLSThreadCache == nullptr)
        {
            static ObjectPool<ThreadCache> tcPool;
            pTLSThreadCache = tcPool.New();
        }

    }

    

    //cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl; 
    return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr)
{
    Span* span = PageCache::GetInstance()->MapObjToSpan(ptr);
    size_t size = span->_objSize;

    if(size > MAX_BYTES)
    {
        PageCache::GetInstance()->_pageMtx.lock();
        
        PageCache::GetInstance()->ReleaseSpanToPageCache(span);
        PageCache::GetInstance()->_pageMtx.unlock();
    }
    else
    {
        assert(pTLSThreadCache);

        pTLSThreadCache->Deallocate(ptr, size);
    }
  
}