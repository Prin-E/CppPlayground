//
//  MemoryPool.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/07.
//

#ifndef MemoryPool_h
#define MemoryPool_h

#include <cstdlib>
#include "../LockFree/Mutex.h"

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
    memory_pool() {}
    ~memory_pool() {
        scoped_lock<spinlock_mutex> lock{ &mutex };
        for(size_t i = 0; i < pages.size(); i++) {
            page *p = pages.at(i);
            delete p;
        }
        pages.clear();
    }
    
    void *allocate() {
        void *ptr = nullptr;
        page *selected_page = nullptr;
        do
        {
            {
                scoped_lock<spinlock_mutex> lock{ &mutex };
                for(size_t i = 0; i < pages.size(); i++) {
                    page *p = pages.at(i);
                    if(p->is_block_available()) {
                        selected_page = p;
                        break;
                    }
                }
                
                if(selected_page == nullptr) {
                    // create a new page
                    pages.push_back(new page());
                    selected_page = pages.back();
                }
            }
            ptr = selected_page->allocate();
        }
        while(ptr == nullptr);
        return ptr;
    }
    
    void free(void *ptr) {
        page *selected_page = nullptr;
        {
            scoped_lock<spinlock_mutex> lock{ &mutex };
            for(page *p : pages) {
                if(p->is_block_belong_to_page(ptr)) {
                    selected_page = p;
                    break;
                }
            }
        }
        if(selected_page != nullptr) {
            selected_page->free(ptr);
        }
    }
    
private:
    static constexpr size_t num_blocks_in_page = page_size / block_size;
    static constexpr uintptr_t free_flag = 0xDFD;
    static constexpr uintptr_t address_shift = 52;
    static constexpr uintptr_t address_bit = (1ull << address_shift) - 1;
    
    class page {
    public:
        page() {
            num_allocated = 0;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
            buffer = _aligned_malloc(page_size, page_size);
#else
            posix_memalign(&buffer, page_size, page_size);
#endif
            std::memset(buffer, 0, page_size);
            free_block = (block_t*)buffer;
        }
        
        page(page &&other) : num_allocated(other.num_allocated), buffer(other.buffer), free_block(other.free_block) {
            other.num_allocated = 0;
            other.buffer = nullptr;
            other.free_block = nullptr;
        }
        
        ~page() {
            if(buffer != nullptr)
                std::free(buffer);
        }
        
        inline bool is_block_available() { return num_allocated < num_blocks_in_page; }
        
        inline bool is_block_belong_to_page(void *ptr) {
            return get_base_address(ptr) == (uintptr_t)buffer;
        }
        
        void *allocate() {
            scoped_lock<spinlock_mutex> lock{ &mutex };
            if(!is_block_available())
                return nullptr;
            
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
            num_allocated--;
        }
        
    private:
        uintptr_t get_base_address(void *ptr) {
            uintptr_t value = (uintptr_t)ptr;
            return value & ~(page_size - 1);
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
        
        uint32_t num_allocated;
        void *buffer;
        block_t *free_block;
        spinlock_mutex mutex;
    };
    
    std::vector<page*> pages;
    spinlock_mutex mutex;
    
private:
    // limitations
    static_assert((block_size & (block_size - 1)) == 0, "The block size must be power of 2!");
    static_assert((page_size & (page_size - 1)) == 0, "The page size must be power of 2!");
    static_assert(block_size <= page_size, "The page size must be equal or larger than the block size!");
};

inline memory_pool<128, PAGE_SIZE_2MB>  global_memory_pool;

#endif /* MemoryPool_h */
