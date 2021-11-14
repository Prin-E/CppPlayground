//
//  Option.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/14.
//

#ifndef Option_h
#define Option_h

#ifndef USE_MEMORY_POOL
#define USE_MEMORY_POOL 0
#endif

#if USE_MEMORY_POOL
#include "../Shared/Memory/MemoryPool.h"
#endif

#endif /* Option_h */
