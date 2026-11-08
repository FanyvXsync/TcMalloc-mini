#pragma once
#include "Common.hpp"

class ThreadCache
{
public:
    // 申请 释放
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);

    // 从 cnetral cache 获取
    void* FetachFromCentralCache(size_t index, int size);

private:
    FreeList _freeList[NFREELIST];
};

// windows 下 全局的 tls 指针
static __declspec(thread) ThreadCache* pTLSThreadCache = nullptr;