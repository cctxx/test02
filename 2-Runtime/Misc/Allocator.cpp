#include "UnityPrefix.h"
#if defined(UNIT_TEST)
#include <cassert>
#define AssertIf(x) assert(!(x))
#else
#include "Configuration/UnityConfigure.h"
#endif

#include <map>

#if UNITY_WIN || UNITY_ANDROID
#include <stdlib.h>
#endif

#include "Allocator.h"
#if UNITY_WII
#include <rvlaux/clib.h>
#endif
