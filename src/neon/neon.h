#pragma once

#include "neon-types.h"
#include "scoped-enum.h"
#include "Widechar.h"

#ifdef _DEBUG
#define ASSERT(x) (void)( (!!(x)) || (__debugbreak(), 0))
#else
#define ASSERT(x)
#endif
