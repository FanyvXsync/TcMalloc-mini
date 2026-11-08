#include "Threadcache.hpp"

void* ThreadCache::Allocate(size_t size)
{
    assert(size < MAX_BYTES);

    size_t alignsize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(alignsize);

    if(!_freeList[index].Empty()) return _freeList[index].PopFront();
    else
    {
        return FetachFromCentralCache(index, alignsize);  // 注意是 对齐以后的数
    } 
    
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
    assert(ptr);
    assert(size < MAX_BYTES);

    // 释放的话就找到 hash 桶链接进去
    size_t index = SizeClass::Index(size);
    _freeList[index].PushFront(ptr);
}