//
//  MemoryPool.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/07.
//

#ifndef MemoryPool_h
#define MemoryPool_h

#include <cstdlib>
#include <cstring>
#include <cassert>
#include "../LockFree/Mutex.h"
#include "../Thread/ThreadLocal.h"

constexpr size_t PAGE_SIZE_16KB = 16 * 1024;
constexpr size_t PAGE_SIZE_32KB = 32 * 1024;
constexpr size_t PAGE_SIZE_64KB = 64 * 1024;
constexpr size_t PAGE_SIZE_128KB = 128 * 1024;
constexpr size_t PAGE_SIZE_256KB = 256 * 1024;
constexpr size_t PAGE_SIZE_512KB = 512 * 1024;
constexpr size_t PAGE_SIZE_1MB = 1 * 1024 * 1024;
constexpr size_t PAGE_SIZE_2MB = 2 * 1024 * 1024;
constexpr size_t PAGE_SIZE_4MB = 4 * 1024 * 1024;

template<size_t block_size, size_t page_size>
class memory_pool {
public:
    memory_pool() {
        uint32_t num_threads_expected = (uint32_t)std::thread::hardware_concurrency();
        if(num_threads_expected < 4)
            num_threads_expected = 4;
        threadlocal_info.reserve(num_threads_expected);
        for(uint32_t i = 0; i < num_threads_expected; i++) {
            threadlocal_info_t info;
            threadlocal_info.push_back(info);
        }
    }
    
    ~memory_pool() {
    }
    
    void *allocate() {
        threadlocal_info_t &threadlocal = get_threadlocal_info(threadlocal_get_thread_id());
        return threadlocal.allocate();
    }
    
    void free(void *ptr) {
        uintptr_t base_address = get_base_address(ptr);
        page_t *page = (page_t*)base_address;
        threadlocal_thread_id thread_id = threadlocal_get_thread_id();
        if(page->thread_id == thread_id) {
            
        }
        else {
            
        }
    }
    
private:
    static constexpr size_t num_blocks_in_page = page_size / block_size;
    static constexpr uintptr_t free_flag = 0xDFD;
    static constexpr uintptr_t address_shift = 52;
    static constexpr uintptr_t address_bit = (1ull << address_shift) - 1;
    
    class page_t {
    public:
        page_t() {
            num_allocated = 0;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
            buffer = _aligned_malloc(page_size, page_size);
#else
            posix_memalign(&buffer, page_size, page_size);
#endif
            std::memset(buffer, 0, page_size);
            thread_id = threadlocal_get_thread_id();
        }
        
        inline bool is_block_available() { return num_allocated < num_blocks_in_page; }
        
        inline bool is_block_belong_to_page(void *ptr) {
            return get_base_address(ptr) == (uintptr_t)buffer;
        }
        
        void *allocate() {
            // use the free block
            block_t *b = free_block;
            if(!is_block_available())
                free_block = nullptr;
            else if(free_block->next != 0)
                free_block = (block_t*)free_block->next;
            else
                free_block = (block_t*)((uintptr_t)free_block + block_size);
            num_allocated++;
            
            // initialize the value of the block
            b->value = 0;
            return b;
        }
        
        void free(void *ptr) {
            if(ptr == nullptr) return;
            
            block_t *b = (block_t*)ptr;
            uintptr_t base_address = get_base_address((void*)b->next);
            
            scoped_lock<spinlock_mutex> lock{ &mutex };
            if(b->flag == free_flag && (uintptr_t)buffer == base_address) return;
            assert(num_allocated > 0);
            
            b->next = (uintptr_t)free_block & address_bit;
            b->flag = free_flag;
            free_block = b;
        }
        
    private:
        struct block_t {
            union {
                uintptr_t value;
                struct {
                    uintptr_t next : 52;
                    uintptr_t flag : 12;
                };
            };
        };
        
        
    private:
        uintptr_t get_base_address(void *ptr) {
            uintptr_t value = (uintptr_t)ptr;
            return value & ~(page_size - 1);
        }
        
        void flush_thread_pending_free_list() {
            block_t *free_list = thread_pending_free_list.exchange(nullptr);
            while(free_list != nullptr) {
                block_t *next_block = (block_t*)free_list->next;
                
            }
        }
        
        void free_block(block_t *ptr) {
            
        }
        
    private:
        struct alignas(64) {
            threadlocal_thread_id thread_id;
            void *buffer;
        };
        uint32_t num_allocated;
        block_t *local_free_list;
        std::atomic<block_t*> thread_pending_free_list;
    };
    
    class threadlocal_info_t {
        std::vector<class page_t*> pages;
        page_t *available_page;
        
        void *allocate() {
            
            if(available_page != nullptr) {
                available_page.allocate();
            }
        }
    };
    
    
    spinlock_mutex mutex;
    std::vector<threadlocal_info_t> threadlocal_info;
    
private:
    threadlocal_info_t& get_threadlocal_info(threadlocal_thread_id thread_id) {
        if(threadlocal_info.size() < thread_id) {
            
        }
        return threadlocal_info.at((size_t)thread_id);
    }
    
    uintptr_t get_base_address(void *ptr) {
        uintptr_t value = (uintptr_t)ptr;
        return value & ~(page_size - 1);
    }
    
    page_t *create_new_page() {
        scoped_lock<spinlock_mutex> lock{ &mutex };
        void *buffer = aligned_alloc(page_size, page_size);
        page_t *page = new (buffer) page_t();
        pages.push_back(page);
        return page;
    }
    
private:
    // limitations
    static_assert((block_size & (block_size - 1)) == 0, "The block size must be power of 2!");
    static_assert((page_size & (page_size - 1)) == 0, "The page size must be power of 2!");
    static_assert(block_size <= page_size, "The page size must be equal or larger than the block size!");
};

inline memory_pool<64, PAGE_SIZE_512KB> global_memory_pool;

#endif /* MemoryPool_h */
