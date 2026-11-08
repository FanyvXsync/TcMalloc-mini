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

void* ThreadCache::FetachFromCentralCache(size_t index, size_t size)
{
    //慢启动 调节算法 最开始不会向 centralcache 要很多, 用不完
    //
    size_t batchNum = std::min(_freeList[index].MaxSize(), SizeClass::NumMoveSize(size));
    if(batchNum == _freeList[index].MaxSize())
    {
        _freeList[index].MaxSize() += 1;   // 链表增大
    }

    void* start = nullptr;
    void* end = nullptr;

    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
    assert(actualNum >= 1);

    if(actualNum == 1)
    {
        assert(start == end);
        _freeList[index].PushFront(start); // ?
        return start;
    }
    else
    {
        _freeList[index].PushRange(start, end); // ?
        return start;
    }

}