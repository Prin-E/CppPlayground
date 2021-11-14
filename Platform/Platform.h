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

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

// Windows variant
#include "Windows/Atomic.h"

#else

#error "not supported on this platform"

#endif

#endif /* Platform_h */


#ifndef NAMESPACE_PLATFORM_BEGIN
#define namespace platform {
#endif

#ifndef NAMESPACE_PLATFORM_END
#define NAMESPACE_PLATFORM_END }
#endif


