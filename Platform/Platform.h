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
#include "Apple/Atomic.h"

#define SUPPORTS_PLATFORM_IMPLEMENTATION 1

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

// Windows variant
#include "Windows/Atomic.h"

#define SUPPORTS_PLATFORM_IMPLEMENTATION 1

#else

//#error "not supported on this platform"

#endif

#ifndef SUPPORTS_PLATFORM_IMPLEMENTATION
#define SUPPORTS_PLATFORM_IMPLEMENTATION 0
#endif

#endif /* Platform_h */