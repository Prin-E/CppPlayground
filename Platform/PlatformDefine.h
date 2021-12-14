//
//  PlatformDefine.h
//  CppPlayground
//
//  Created by 이현우 on 2021/12/15.
//

#ifndef PlatformDefine_h
#define PlatformDefine_h

#if __APPLE__

// Apple variant
#include <TargetConditionals.h>

#define PLATFORM_APPLE 1

#if TARGET_CPU_ARM64

#define PLATFORM_APPLE_APPLESILICON 1
#define PLATFORM_APPLE_INTEL 0
// cache line size is 128 in A13 and later (and M chipset)
#define PLATFORM_CACHE_LINE_SIZE 128

#elif TRAGET_CPU_X86 || TARGET_CPU_X86_64

#define PLATFORM_APPLE_APPLESILICON 0
#define PLATFORM_APPLE_INTEL 1
#define PLATFORM_CACHE_LINE_SIZE 64

#else

#error "unrecognized target cpu"

#endif

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

// Windows variant
#include <Windows.h>

#define PLATFORM_WINDOWS 1
// Assumes that the cache line size is 64 in both ARM and AMD64.
#define PLATFORM_CACHE_LINE_SIZE 64

#endif

#ifndef PLATFORM_CACHE_LINE_SIZE
#define PLATFORM_CACHE_LINE_SIZE 64
#endif

#endif /* PlatformDefine_h */
