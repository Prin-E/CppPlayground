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
#include "../../Platform/Platform.h"

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
        /*
        uint32_t num_threads_expected = (uint32_t)std::thread::hardware_concurrency();
        if(num_threads_expected < 4)
            num_threads_expected = 4;
        threadlocal_info.reserve(num_threads_expected);
        for(uint32_t i = 0; i < num_threads_expected; i++) {
            threadlocal_info_t info;
            threadlocal_info.push_back(info);
        }
         */
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
            page->free(ptr);
        }
        else {
            page->deferred_free(ptr);
        }
    }
    
    void collect() {
        get_threadlocal_info(threadlocal_get_thread_id()).collect();
    }
    
private:
    class page_t {
    public:
        static constexpr size_t page_header_size = ((2 * PLATFORM_CACHE_LINE_SIZE + (block_size-1)) & (~(block_size-1)));
        static constexpr size_t allocatable_page_size = page_size - page_header_size;
        static constexpr size_t num_blocks_in_page = allocatable_page_size / block_size;
        
        page_t() {
            num_allocated = 0;
            void *buffer = (void*)((uintptr_t)this + page_header_size);
            //std::memset(buffer, 0, allocatable_page_size);
            thread_id = threadlocal_get_thread_id();
            local_free_list = (block_t*)buffer;
        }
        
        inline bool is_block_available() { return num_allocated < num_blocks_in_page; }
        
        void *allocate() {
            // use the free block
            block_t *b = local_free_list;
            if(!is_block_available())
                local_free_list = nullptr;
            else if(local_free_list->next != 0)
                local_free_list = (block_t*)local_free_list->next;
            else
                local_free_list = (block_t*)((uintptr_t)local_free_list + block_size);
            num_allocated++;
            
            // initialize the value of the block
            b->next = 0;
            return b;
        }
        
        void free(void *ptr) {
            block_t *b = (block_t*)ptr;
            b->next = local_free_list;
            local_free_list = b;
            num_allocated--;
        }
        
        void deferred_free(void *ptr) {
            block_t *b = (block_t*)ptr;
            b->next = thread_pending_free_list.load();
            while(!thread_pending_free_list.compare_exchange_strong(b->next, b));
        }
        
        void collect() {
            block_t *free_block = (block_t*)thread_pending_free_list.exchange(0);
            while(free_block != nullptr) {
                block_t *next_block = (block_t*)free_block->next;
                free(free_block);
                free_block = next_block;
            }
        }
        
    private:
        struct block_t {
            struct block_t* next;
        };
        
    private:
        friend class memory_pool;
        
        struct alignas(PLATFORM_CACHE_LINE_SIZE) {
            threadlocal_thread_id thread_id;
            uint32_t num_allocated;
            block_t *local_free_list;
        };
        struct alignas(PLATFORM_CACHE_LINE_SIZE) {
            std::atomic<block_t*> thread_pending_free_list;
        };
    };
    
    class threadlocal_info_t {
    public:
        threadlocal_info_t() : heartbeat(0) {}
        
        ~threadlocal_info_t() {
            collect();
        }
        
        void *allocate() {
            void *ptr = nullptr;
            page_t *available_page = free_pages.size() > 0 ? free_pages.back() : nullptr;
            ++heartbeat;
            
            if(available_page == nullptr) {
                if(heartbeat >= page_t::num_blocks_in_page) {
                    collect();
                    if(free_pages.size() > 0)
                        available_page = free_pages.back();
                    else
                        available_page = create_new_page();
                    heartbeat = 0;
                }
                else
                    available_page = create_new_page();
            }
            ptr = available_page->allocate();
            if(!available_page->is_block_available()) {
                free_pages.pop_back();
                filled_pages.push_back(available_page);
                available_page = nullptr;
            }
            return ptr;
        }
        
        void collect() {
            for(size_t i = 0, cnt = filled_pages.size(); i < cnt; i++) {
                page_t *page = filled_pages.at(i);
                page->collect();
                if(page->is_block_available()) {
                    free_pages.push_back(page);
                    filled_pages.erase(filled_pages.begin() + i);
                    i--;
                    cnt--;
                }
            }
        }
        
        page_t *create_new_page() {
            void *buffer;
            constexpr size_t alloc_size = page_size < PAGE_SIZE_2MB ? PAGE_SIZE_2MB : page_size;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
            buffer = _aligned_malloc(alloc_size, alloc_size);
#else
            posix_memalign(&buffer, alloc_size, alloc_size);
#endif
            std::memset(buffer, 0, alloc_size);
            uint8_t *ptr = (uint8_t*)buffer;
            for(uintptr_t offset = 0; offset < alloc_size; offset += page_size) {
                page_t *page = new (ptr + offset) page_t();
                free_pages.push_back(page);
            }
            return free_pages.back();
        }
        
    private:
        std::vector<page_t*> filled_pages;
        std::vector<page_t*> free_pages;
        uint64_t heartbeat;
    };
    
    spinlock_mutex mutex;
    inline static thread_local threadlocal_info_t threadlocal_info;
    
private:
    threadlocal_info_t& get_threadlocal_info(threadlocal_thread_id thread_id) {
        return threadlocal_info;
        /*
        
        // TODO: remove scoped lock for more performance
        scoped_lock<spinlock_mutex> lock{ &mutex };
        if(threadlocal_info.size() < thread_id) {
            create_new_threadlocal_infos(thread_id - threadlocal_info.size());
        }
        return threadlocal_info.at((size_t)thread_id - 1);
         */
    }
    
    /*
    void create_new_threadlocal_infos(size_t new_info_count) {
        for(size_t i = 0; i < new_info_count; i++) {
            threadlocal_info.push_back(threadlocal_info_t());
        }
    }
     */
    
    static uintptr_t get_base_address(void *ptr) {
        uintptr_t value = (uintptr_t)ptr;
        return value & ~(page_size - 1);
    }
    
private:
    // limitations
    static_assert(block_size >= 16, "The block size must be equal or larger than minimum alignment(16)!");
    static_assert((block_size & (block_size - 1)) == 0, "The block size must be power of 2!");
    static_assert((page_size & (page_size - 1)) == 0, "The page size must be power of 2!");
    static_assert(block_size <= page_size, "The page size must be equal or larger than the block size!");
    static_assert(page_size <= PAGE_SIZE_4MB, "The page size must be equal or smaller than the maximum page size(4MB)!");
};

inline memory_pool<64, PAGE_SIZE_512KB> global_memory_pool;

#endif /* MemoryPool_h */
