//
//  Platform.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/14.
//

#ifndef Platform_h
#define Platform_h

#include "PlatformDefine.h"
#include "PlatformCommon.h"

#if PLATFORM_APPLE

#define SUPPORTS_PLATFORM_IMPLEMENTATION 1
#include "Apple/Atomic.h"

#elif PLATFORM_WINDOWS

#define SUPPORTS_PLATFORM_IMPLEMENTATION 1
#include "Windows/Atomic.h"

#else

#define SUPPORTS_PLATFORM_IMPLEMENTATION 0

#endif

#endif /* Platform_h */
