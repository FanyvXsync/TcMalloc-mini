#include "Threadcache.hpp"
#include "Centralcache.hpp"

void* ThreadCache::Allocate(size_t size)
{
    assert(size <= MAX_BYTES);

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

    // 当链表长度 大于一次批量申请的内存的时候 就可以还了

    if(_freeList[index].Size() >= _freeList[index].MaxSize())
    {
        ListTooLong(_freeList[index], size);
    }


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
    assert(actualNum > 0);

    if(actualNum == 1)
    {
        assert(start == end);
         // ?
        return start;
    }
    else
    {
        _freeList[index].PushRange(NextObj(start), end, actualNum - 1); 
        return start;
    }

}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
    void* start = nullptr;
    void* end = nullptr;

    list.PopRange(start, end, list.MaxSize());

    CentralCache::GetInstance()->ReleaseObjToSpan(start, size);
}