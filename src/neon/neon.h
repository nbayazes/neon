#pragma once

#include "neon-types.h"
#include "Widechar.h"

namespace neon {
    #ifdef _DEBUG
    #define ASSERT(x) (void)( (!!(x)) || (__debugbreak(), 0))
    #else
    #define ASSERT(x)
    #endif
}
