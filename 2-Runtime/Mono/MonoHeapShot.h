#ifndef MONO_HEAPSHOT_H
#define MONO_HEAPSHOT_H


#if ENABLE_MONO_HEAPSHOT

#include "Runtime/Mono/MonoTypes.h"
#include "Runtime/Scripting/ScriptingUtility.h"

extern "C"
{
	typedef enum 
	{
		MONO_GC_EVENT_START,
		MONO_GC_EVENT_MARK_START,
		MONO_GC_EVENT_MARK_END,
		MONO_GC_EVENT_RECLAIM_START,
		MONO_GC_EVENT_RECLAIM_END,
		MONO_GC_EVENT_END,
		MONO_GC_EVENT_PRE_STOP_WORLD,
		MONO_GC_EVENT_POST_STOP_WORLD,
		MONO_GC_EVENT_PRE_START_WORLD,
		MONO_GC_EVENT_POST_START_WORLD
	} MonoGCEvent;

	typedef struct _MonoProfiler MonoProfiler;
	/*
	typedef gint32 mono_bool;

	extern mono_bool mono_type_is_reference (MonoType *type);
	extern gint32 mono_class_value_size (MonoClass *klass, guint32 *align);
	extern MonoClass* mono_class_get_element_class (MonoClass *klass);

	extern char* mono_array_addr_with_size (MonoArray *array, int size, uintptr_t idx);
	#define mono_array_addr(array,type,index) ((type*)(void*) mono_array_addr_with_size (array, sizeof (type), index))
	#define mono_array_get(array,type,index) ( *(type*)mono_array_addr ((array), type, (index)) ) 

	extern gint32 mono_array_element_size (MonoClass *ac);
	extern int mono_class_get_rank (MonoClass *klass);
	extern char* mono_type_full_name (MonoType *type);
	typedef void (*MonoDomainFunc) (MonoDomain *domain, void* user_data);
	extern void mono_domain_foreach (MonoDomainFunc func, void* user_data);
	extern mono_bool mono_object_is_alive (MonoObject* obj);

	// Note: theser are different than the ones defined in MonoFunctions.h
	typedef void (*MonoProfileFunc) (MonoProfiler *prof);
	typedef void (*MonoProfileGCFunc) (MonoProfiler *prof, MonoGCEvent event, int generation);
	typedef void (*MonoProfileGCResizeFunc) (MonoProfiler *prof, gint64 new_size);
	typedef void (*MonoProfileClassFunc) (MonoProfiler *prof, MonoClass    *klass);
	typedef void (*MonoProfileAllocFunc) (MonoProfiler *prof, MonoObject *obj, MonoClass *klass);
	typedef void (*MonoProfileClassResult) (MonoProfiler *prof, MonoClass    *klass,    int result);

	extern void mono_profiler_install (MonoProfiler *prof, MonoProfileFunc shutdown_callback);
	extern void mono_profiler_install_allocation (MonoProfileAllocFunc callback);
	extern void mono_profiler_install_class (MonoProfileClassFunc start_load, MonoProfileClassResult end_load, MonoProfileClassFunc start_unload, MonoProfileClassFunc end_unload);
	extern void mono_profiler_install_gc (MonoProfileGCFunc callback, MonoProfileGCResizeFunc heap_resize_callback);
	extern void mono_profiler_set_events (MonoProfileFlags events);
	*/

	struct MonoThreadsSync;
	struct MonoObject
	{
		MonoVTable *vtable;
		MonoThreadsSync *synchronisation;
	} ;
}

typedef std::map<MonoObject*, MonoClass*> ObjectsHash;
typedef std::map<MonoClass*, MonoClass*> ClassHash;
typedef std::vector<UInt8> HeapShotData;

MonoProfiler* HeapShotCreateMonoProfiler (const char *outfilename);
void HeapShotAllocFunc (MonoProfiler *p, MonoObject *obj, MonoClass *klass);
void HeapShotGCFunc (MonoProfiler *p, MonoGCEvent e, int gen);
void HeapShotLoadClassFunc (MonoProfiler *p, MonoClass *klass, int result);
void HeapShotUnloadClassFunc (MonoProfiler *p, MonoClass *klass);
void HeapShotDumpObjectMap (HeapShotData& data);
void InstallMonoHeapshot();
#endif

#endif
