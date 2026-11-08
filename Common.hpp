#pragma once
#include <iostream>
#include <assert.h>
#include <vector>

using std::cout;
using std::endl;

const static size_t MAX_BYTES = 256 * 1024;
const static size_t NFREELIST = 208;

static void*& NextObj(void* ptr)  // 加 static 防链接错误
{
    return *((void**)ptr);
}


class FreeList
{
public:
    void PushFront(void* ptr)
    {
        assert(ptr);
        
        NextObj(ptr) = _freeList;
        _freeList = ptr;
    }
    

    void* PopFront()
    {
        assert(_freeList);

        void* ret = _freeList;
        _freeList = NextObj(_freeList);
        return ret;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }


private:
    void* _freeList = nullptr;
};

// 计算对象大小 对齐
class SizeClass
{
public:
    // 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐	    freelist[0,16)
	// [128+1,1024]				16byte对齐	    freelist[16,72)
	// [1024+1,8*1024]			128byte对齐	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐   freelist[184,208)

    // 怎么算对齐     
    // 算出 最高位   然后    去掉所有的低位
    // 1 + 7   8    
    // 2 + 7   9   
    static inline size_t _RoundUp(size_t bytes,  size_t alignNum) // 子函数
    {
        return (bytes + alignNum - 1) & ~(alignNum - 1);
    }


    static inline size_t RoundUp(size_t size)  // 算对齐到哪个大小的桶
    {
        if(size <= 128)
        {
            return _RoundUp(size, 8);
        }
        else if(size <= 1024)
        {
            return _RoundUp(size, 16);
        }
        else if(size  <= 8 * 1024)
        {
            return _RoundUp(size, 128);
        }
        else if(size <= 64 * 1024)
        {
            return _RoundUp(size, 1024);
        }
        else if(size <= 256 * 1024)
        {
            return _RoundUp(size, 8 * 1024);
        }
        else
        {
            assert(false);
            return -1;
        }

    } 

    // 这里传到是 对齐数是 2 的几次方
    static inline  size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    // 超出一块去 然后右移就算第几个 这是从 0 开始 所以 减一 


    // 算映射到哪个桶的第几号
    static inline size_t Index(size_t bytes)
    {   
        assert(bytes < MAX_BYTES);

        // 每个不同对齐数的区间 有多少个链
        static int group_array[4] = {16, 56, 56, 56};
        if(bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if(bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if(bytes <= 8 * 1024)
        {
            return _Index(bytes  - 1024, 10) + group_array[0] + group_array[1];
        }
        else if (bytes <= 64 * 1024)
        {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024)
        {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else
        {
			assert(false);
		}

        return -1;
    }

};