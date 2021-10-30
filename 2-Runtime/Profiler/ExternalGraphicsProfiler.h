#pragma once

#include "Profiler.h"

#if ENABLE_PROFILER

#include "Runtime/GfxDevice/GfxDevice.h"

struct AutoGfxEventMainThread {
	AutoGfxEventMainThread(const char* name) { GetGfxDevice().BeginProfileEvent(name); }
	~AutoGfxEventMainThread() { GetGfxDevice().EndProfileEvent(); }
};
#define PROFILER_AUTO_GFX(INFO, OBJECT_PTR) PROFILER_AUTO(INFO,OBJECT_PTR); AutoGfxEventMainThread _gfx_event(INFO.name);


#else // no profiling compiled in

#	define PROFILER_AUTO_GFX(INFO, OBJECT_PTR)

#endif
