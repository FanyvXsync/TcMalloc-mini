#include <iostream>
#include <vector>
#include <time.h>
using std::cout;
using std::endl;


#ifdef _WIN32
    #include <windows.h>
#endif

// 直接去堆上申请空间
static inline void* SystemAlloc(size_t kpage)
{
    #ifdef  _WIN32
        void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    #else
        // brk();
    #endif

    if(ptr == nullptr)
    {
        throw std::bad_alloc();
    }

    return ptr;

}


static inline void SystemFree(void* ptr)
{
#ifdef _WIN32
    VirtualFree(ptr, 0 , MEM_RELEASE);
#else

#endif 

}


template<class T>
class ObjectPool
{

public:

    T* New()
    {
        
        T* obj = nullptr;

        if(_freelist)
        {
            void* next = _freelist;
            _freelist = *(void**)next;;
            obj = (T*) next;
        }
        else 
        {

            size_t tsize = sizeof(T);
            tsize = std::max(tsize, sizeof(void*));
            if(_remainBytes < tsize)
            {
                _remainBytes = 8 * 1024;
                _memory = (char*)SystemAlloc(_remainBytes >> 13);
            
                if(_memory == nullptr)
                {
                    throw std::bad_alloc();
                }
            
            }
            
            obj = (T*)_memory;
            _memory += tsize;
            _remainBytes -= tsize;

        }

      
        new(obj) T;
        
        return obj;
    }


    void Delete(T* obj)
    {
        obj-> ~T();

       * (void**)obj = _freelist;
        _freelist = obj;
    }

    

private:
    char* _memory = nullptr;
    size_t _remainBytes = 0;

    void* _freelist = nullptr;
};