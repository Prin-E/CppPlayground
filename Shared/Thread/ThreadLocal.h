//
//  ThreadLocal.h
//  CppPlayground
//
//  Created by 이현우 on 2021/12/10.
//

#ifndef ThreadLocal_h
#define ThreadLocal_h

#include <atomic>

typedef uint32_t threadlocal_thread_id;

inline thread_local threadlocal_thread_id __tls_thread_id = 0;
inline std::atomic<threadlocal_thread_id> __global_thread_id_counter;

inline threadlocal_thread_id threadlocal_get_thread_id() {
    if(__tls_thread_id == 0) {
        __tls_thread_id = __global_thread_id_counter.fetch_add(1) + 1;
    }
    return __tls_thread_id;
}

#endif /* ThreadLocal_h */
