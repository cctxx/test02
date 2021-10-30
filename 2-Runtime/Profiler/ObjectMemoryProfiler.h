#ifndef _OBJECT_MEMORY_PROFILER
#define _OBJECT_MEMORY_PROFILER

#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/dynamic_array.h"

#if ENABLE_MEM_PROFILER

namespace ObjectMemoryProfiler
{
	void TakeMemorySnapshot (dynamic_array<int>& stream);

#if UNITY_EDITOR	
	void SetDataFromEditor ();
	void DeserializeAndApply (const void* data, size_t size);
#endif
};
#endif
#endif
