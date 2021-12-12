//
//  Platform.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/14.
//

#ifndef Platform_h
#define Platform_h

#if __APPLE__

// Apple variant
#include <TargetConditionals.h>
#include "Apple/Atomic.h"

#define SUPPORTS_PLATFORM_IMPLEMENTATION 1

#if TARGET_CPU_ARM64
// cache line size is 128 in A13 or later (&M chipset)
#define PLATFORM_CACHE_LINE_SIZE 128
#else
#define PLATFORM_CACHE_LINE_SIZE 64
#endif

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

// Windows variant
#include "Windows/Atomic.h"

#define SUPPORTS_PLATFORM_IMPLEMENTATION 1
#define PLATFORM_CACHE_LINE_SIZE 64

#else

//#error "not supported on this platform"

#endif

#ifndef SUPPORTS_PLATFORM_IMPLEMENTATION
#define SUPPORTS_PLATFORM_IMPLEMENTATION 0
#endif

#ifndef PLATFORM_CACHE_LINE_SIZE
#define PLATFORM_CACHE_LINE_SIZE 64
#endif

#endif /* Platform_h */
