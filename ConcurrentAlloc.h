#pragma once

#include "Threadcache.hpp"
#include <thread>

// 测试 TLS

static void* ConcurrentAlloc(size_t size)
{
    if(pTLSThreadCache == nullptr)
    {
        pTLSThreadCache = new ThreadCache;
    }

    cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl; 
    return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr, size_t size)
{
    assert(pTLSThreadCache);

    pTLSThreadCache->Deallocate(ptr, size);
}