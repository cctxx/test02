#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Threads/Mutex.h"

#include "MonoHeapShot.h"

#if ENABLE_MONO_HEAPSHOT

#if defined(SN_TARGET_PS3)
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#endif

#include "MonoHeapShotWriter.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * heap-shot.c
 *
 * Copyright (C) 2005 Novell, Inc.
 *
 * This profiler is unsafe: it does in signal context almost everything
 * that must not be done there, like taking locks, running a GC etc.
 * It takes a lock in heap_shot_gc_func(), which can cause a deadlock.
 * It doesn't deal with moving objects.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

/*
#include <string.h>
#include <glib.h>
#include <pthread.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/object.h>
#include <mono/metadata/profiler.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/debug-helpers.h>
#include <unistd.h>
#include <time.h>

*/


/*
typedef pthread_mutex_t mono_mutex_t;
#define mono_mutex_init(mutex,attr) pthread_mutex_init (mutex, attr)
#define mono_mutex_lock(mutex) pthread_mutex_lock (mutex)
#define mono_mutex_unlock(mutex) pthread_mutex_unlock (mutex)
*/
//#define MONO_CONSOLES (UNITY_XENON || UNITY_PS3)

//#if MONO_CONSOLES
//#define MONOPROFILER MonoProfiler
//#define MONOGCEVENT MonoGCEvent
//#else
#define MONOPROFILER void
#define MONOGCEVENT int
//#endif

#if UNITY_WII
typedef OSMutex mono_mutex_t;
#define mono_mutex_init(mutex,attr) OSInitMutex(mutex);
#define mono_mutex_lock(mutex) OSLockMutex(mutex)
#define mono_mutex_unlock(mutex) OSUnlockMutex(mutex);
#else
typedef Mutex* mono_mutex_t;
#define mono_mutex_init(mutex,attr) *mutex = new Mutex()
#define mono_mutex_lock(mutex) (*mutex)->Lock()
#define mono_mutex_unlock(mutex) (*mutex)->Unlock()
#endif


struct _MonoProfiler 
{
	mono_mutex_t   lock;
	mono_mutex_t   dump_lock;
	ObjectsHash    *objects_hash;
	ObjectsHash    *work_objects_hash;

	ClassHash    *class_hash;
	ClassHash    *exclude_class_hash;
	ClassHash    *work_class_hash;
	MonoHeapShotWriter dumpfile_writer;
	const char    *out_file_name;
	int           dump_count;
};
typedef _MonoProfiler MonoProfiler;

static MonoProfiler* s_MonoProfiler = NULL;

MonoProfiler* HeapShotCreateMonoProfiler (const char *outfilename)
{
	//struct sigaction sa;
	MonoProfiler* p = new MonoProfiler();
	s_MonoProfiler = p;

	mono_mutex_init (&p->lock, NULL);
	mono_mutex_init (&p->dump_lock, NULL);

	p->objects_hash     = new ObjectsHash();
	p->class_hash       = new ClassHash();
	p->exclude_class_hash = new ClassHash();
	p->out_file_name    = outfilename;
	p->dump_count       = 0;

	// Sets the PROF signal
	//sa.sa_handler = profiler_signal_handler;
	//sigemptyset (&sa.sa_mask);
	//sa.sa_flags = 0;
	//g_assert (sigaction (SIGPROF, &sa, NULL) != -1);

	return p;
}

static gboolean heap_scan_object (MonoProfiler *p, MonoObject *obj, MonoClass *klass, MonoClassField *parent_field)
{
	gpointer iter;
	MonoClassField *field;
	gboolean has_refs = FALSE;
	MonoClass *cur_klass = klass;

	do {
		iter = NULL;
		while ((field = mono_class_get_fields (cur_klass, &iter)) != NULL) {
			MonoType* field_type = mono_field_get_type (field);
			// Skip static fields
			if (mono_field_get_flags (field) & 0x0010 /*FIELD_ATTRIBUTE_STATIC*/)
				continue;

			if (mono_type_is_reference (field_type)) {
				// Dump the object reference
				MonoObject* ref;
				has_refs = true;
				mono_field_get_value (obj, field, &ref);
				if (ref && p->work_objects_hash->find(ref) != p->work_objects_hash->end())
					p->dumpfile_writer.DumpReference (ref, parent_field ? parent_field : field);
			}
			else {
				MonoClass *fclass = mono_class_from_mono_type (field_type);
				if (fclass && mono_class_is_valuetype (fclass)) {
					if (p->exclude_class_hash->find(fclass) != p->exclude_class_hash->end())
						continue;
					// It's a value type. Check if the class is big enough to hold references
					int size = mono_class_value_size (fclass, NULL);
					if (size >= sizeof(gpointer) && fclass != cur_klass) {
						// Get the object value and scan it
						char* vop = (char*)malloc (size);
						mono_field_get_value (obj, field, vop);
						// Recursively scan the object
						if (heap_scan_object (p, (MonoObject*)(vop - sizeof(MonoObject)), fclass, parent_field))
							has_refs = true;
						free (vop);
					}
				}
			}
		}
		cur_klass = mono_class_get_parent (cur_klass);
	} while (cur_klass);
	
	// If the class doesn't contain references, register in the exclude_class_hash table,
	// so it won't be scanned again.
	if (!has_refs && p->exclude_class_hash->find(klass) == p->exclude_class_hash->end())
	{
		(*p->exclude_class_hash)[klass] = klass;
		//g_hash_table_insert (p->exclude_class_hash, klass, klass);
	}
	return has_refs;
}

static void heap_scan_array (MonoProfiler *p, MonoObject *obj, MonoClass *klass)
{
	MonoArray *array = (MonoArray *) obj;
	MonoClass *eklass = mono_class_get_element_class (klass);
	gboolean has_refs = FALSE;
	
	if (!mono_class_is_valuetype (eklass)) 
	{
		// It's an array of object references, write all of them in the output file
		int n;
		for (n=0; n<mono_array_length (array); n++) {
			MonoObject *ref = mono_array_get (array, MonoObject*, n);
			if (ref && p->work_objects_hash->find(ref) != p->work_objects_hash->end())
				p->dumpfile_writer.DumpReference  (ref, NULL);
		}
		has_refs = true;
	}
	//else if (!g_hash_table_lookup (p->exclude_class_hash, eklass))
	else if (p->exclude_class_hash->find(eklass) == p->exclude_class_hash->end()) 
	{
		// It's an array of value type objects. Each object will be scanned
		// by recursively calling heap_scan_object for each member
		int n;
		gint32 esize = mono_array_element_size (klass);
		if (esize >= sizeof(gpointer)) {
			// The type is big enough to contain references.
			// Scan the array.
			for (n=0; n<mono_array_length (array); n++) {
				char *ref = (char *) mono_array_addr_with_size (array, esize, n);
				ref -= sizeof (MonoObject);
				if (heap_scan_object (p, (MonoObject *) ref, eklass, NULL))
					has_refs = true;
				else
					// The class has no fields, it makes no sense to continue
					break;
			}
		}
	}
	// If the class doesn't contain references, register in the exclude_class_hash table,
	// so it won't be scanned again.
	if (!has_refs && p->exclude_class_hash->find(klass) == p->exclude_class_hash->end())
	{
		(*p->exclude_class_hash)[klass] = klass;
		//g_hash_table_insert (p->exclude_class_hash, klass, klass);
	}
}

static void heap_scan_fn (gpointer key, gpointer value, gpointer user_data)
{
	MonoProfiler *p = (MonoProfiler*)user_data;
	MonoObject *obj = (MonoObject*)key;
	MonoClass  *klass = (MonoClass*)value;
	
	// Write the object header
	p->dumpfile_writer.BeginObjectDump (obj, klass);
	
	// If the type is registered as not having reference fields, just return
	//if (g_hash_table_lookup (p->exclude_class_hash, klass)) 
	if (p->exclude_class_hash->find(klass) != p->exclude_class_hash->end())
	{
		p->dumpfile_writer.EndObjectDump ();
		return;
	}

	if (mono_class_get_rank (klass)) {
		// It's an array
		heap_scan_array (p, obj, klass);
	}
	else {
		heap_scan_object (p, obj, klass, NULL);
	}

	// Write the object end marker	
	p->dumpfile_writer.EndObjectDump ();
}

static void dump_static_fields_fn (gpointer key, gpointer value, gpointer user_data)
{
	MonoClassField *field;
	gpointer iter = NULL;
	gboolean added = FALSE;
	MonoClass *klass = (MonoClass*)key;
	MonoProfiler *p = (MonoProfiler*)((gpointer*)user_data)[0];
	MonoDomain *domain = (MonoDomain*)((gpointer*)user_data)[1];
	MonoVTable *vtable = NULL;
	gpointer field_value;
	
	if (strstr (mono_type_full_name (mono_class_get_type (klass)), "`"))
		return;

	while ((field = mono_class_get_fields (klass, &iter)) != NULL) {
		if (mono_field_get_flags (field) & 0x0010 /*FIELD_ATTRIBUTE_STATIC*/) {
			// Dump the class only if it has static fields
			if (!added) {
				p->dumpfile_writer.BeginObjectDump (NULL, klass);
				vtable = mono_class_vtable (domain, klass);
				added = true;
			}
			MonoType* field_type = mono_field_get_type (field);
			
			if (mono_type_is_reference (field_type)) {
				mono_field_static_get_value (vtable, field, &field_value);
				if (field_value) {
					p->dumpfile_writer.DumpReference (field_value, field);
				}
			} else {
				MonoClass *fclass = mono_class_from_mono_type (field_type);
				if (fclass && mono_class_is_valuetype (fclass)) {
					//if (g_hash_table_lookup (p->exclude_class_hash, fclass))
					if (p->exclude_class_hash->find(fclass) != p->exclude_class_hash->end())
						continue;
					int size = mono_class_value_size (fclass, NULL);
					if (size >= sizeof(gpointer) && fclass != klass) {
						// Get the object value and scan it
						char* vop = (char*)malloc (size);
						mono_field_static_get_value (vtable, field, vop);
						// Recursively scan the object
						heap_scan_object (p, (MonoObject*)(vop - sizeof(MonoObject)), fclass, field);
						free (vop);
					}
				}
			}
		}
	}
	if (added)
		p->dumpfile_writer.EndObjectDump ();
}

static void dump_domain_static_fields_fn (MonoDomain *domain, gpointer user_data)
{
	MonoProfiler *p = (MonoProfiler*)user_data;
	gpointer data [2];
	data [0] = p;
	data [1] = domain;
	//g_hash_table_foreach (p->work_class_hash, dump_static_fields_fn, &data);
	for (ClassHash::iterator item = p->work_class_hash->begin();
		 item != p->work_class_hash->end();
		 item++)
	{
		dump_static_fields_fn(item->first, item->second, &data);
	}
}

void HeapShotDumpObjectMap (HeapShotData& data)
{
	if (s_MonoProfiler == NULL)
	{
		printf_console("Mono HeapShot is disabled? Do you have Mono Debugger enabled?");
		return;
	}
	MonoProfiler *p = s_MonoProfiler;

	mono_gc_collect (0);
	mono_mutex_lock (&p->dump_lock);
	
	// Make a copy of the hashtables which collect object and type data,
	// to avoid deadlocks while inspecting the data
	mono_mutex_lock (&p->lock);
	
	p->work_objects_hash = new ObjectsHash();
	p->work_class_hash = new ClassHash();
	*p->work_objects_hash = *p->objects_hash;
	//g_hash_table_foreach (p->objects_hash, clone_hash_table_fn, p->work_objects_hash);
	//g_hash_table_foreach (p->class_hash, clone_hash_table_fn, p->work_class_hash);
	*p->work_class_hash = *p->class_hash;
	
	mono_mutex_unlock (&p->lock);
	
	char fileName[255];
	sprintf(fileName, "%s_%d.omap", p->out_file_name, p->dump_count);
	printf_console ("Dumping object map to file '%s'\n", fileName);
	p->dumpfile_writer.Start(&data);
	
	// Dump object information
	//g_hash_table_foreach (p->work_objects_hash, heap_scan_fn, p);
	for (ObjectsHash::iterator item = p->work_objects_hash->begin();
		 item != p->work_objects_hash->end();
		 item++)
	{
		heap_scan_fn(item->first, item->second, p);
	}

	p->dump_count++;

	// Dump static field references for each domain
	// This can cause new object allocations
	mono_domain_foreach (dump_domain_static_fields_fn, p);
	p->dumpfile_writer.DumpObjectsReferencedByUnity();
	p->dumpfile_writer.End();

	//g_hash_table_destroy (p->work_objects_hash);
	delete p->work_objects_hash;
	delete p->work_class_hash;
	
	printf_console ("done\n");
	
	mono_mutex_unlock (&p->dump_lock);
}

void HeapShotAllocFunc (MonoProfiler *p, MonoObject *obj, MonoClass *klass)
{
	mono_mutex_lock (&p->lock);
	//g_hash_table_insert (p->objects_hash, obj, klass);
	(*p->objects_hash)[obj] = klass;
	mono_mutex_unlock (&p->lock);
}

static gboolean post_gc_clean_objects_fn (gpointer key, gpointer value, gpointer user_data)
{
	MonoObject *obj = (MonoObject*)key;
	return !mono_object_is_alive (obj);
}

void HeapShotGCFunc (MonoProfiler *p, MonoGCEvent e, int gen)
{
	if (e != MONO_GC_EVENT_MARK_END)
		return;

	mono_mutex_lock (&p->lock);
	//g_hash_table_foreach_remove (p->objects_hash, post_gc_clean_objects_fn, NULL);
	for (ObjectsHash::iterator item = p->objects_hash->begin();
		 item != p->objects_hash->end();
		 )
	{
		if (post_gc_clean_objects_fn(item->first, item->second, NULL))
		{
			ObjectsHash::iterator toDelete = item;
			item++;
			p->objects_hash->erase(toDelete);
		}
		else
		{
			item++;
		}
	}
	mono_mutex_unlock (&p->lock);
}

void HeapShotLoadClassFunc (MonoProfiler *p, MonoClass *klass, int result)
{
	mono_mutex_lock (&p->lock);
	//g_hash_table_insert (p->class_hash, klass, klass);
	(*p->class_hash)[klass] = klass;
	mono_mutex_unlock (&p->lock);
}

 void HeapShotUnloadClassFunc (MonoProfiler *p, MonoClass *klass)
{
	mono_mutex_lock (&p->lock);
	//g_hash_table_remove (p->class_hash, klass);
	p->class_hash->erase(klass);
	mono_mutex_unlock (&p->lock);
}

static void GCMonoCallback (MONOPROFILER* prof, MONOGCEVENT event, int generation)
{
	HeapShotGCFunc((MonoProfiler*)prof, (MonoGCEvent)event, generation);
}

static void GCMonoResizeCallback (MONOPROFILER* prof, SInt64 new_size)
{
	//printf("GCMonoResizeCallback\n");
}

static void GCMonoAllocationCallback (MONOPROFILER* prof, MonoObject *obj, MonoClass *klass)
{
	//printf("GCMonoAllocationCallback: Allocating %d bytes for %s\n", mono_object_get_size(obj), mono_class_get_name(klass));
	HeapShotAllocFunc((MonoProfiler*)prof, obj, klass);
}
static void MonoLoadClassFunc (MONOPROFILER *p, MonoClass *klass, int result)
{
	//printf("MonoLoadClassFunc: %s %d\n", mono_class_get_name(klass), result);
	HeapShotLoadClassFunc((MonoProfiler*)p, klass, result);
}
static void MonoUnloadClassFunc (MONOPROFILER *p, MonoClass *klass)
{
	HeapShotUnloadClassFunc((MonoProfiler*)p, klass);
}
void InstallMonoHeapshot()
{
#if UNITY_WII
	if (GetPlayerSettingsPtr() && GetPlayerSettings().wiiHio2Usage != 1) return;
#endif

#if ENABLE_PLAYERCONNECTION
	if (PlayerConnection::ms_Instance && PlayerConnection::Get().AllowDebugging())
	{
		printf_console("\nCan not initialize mono heapshot when script debugging is on.\n");
		return;
	}
#endif

	printf_console("\nInitializing mono heapshot\n");
	s_MonoProfiler = HeapShotCreateMonoProfiler ("test.txt");

	mono_profiler_install (s_MonoProfiler, NULL);
	mono_profiler_install_gc(GCMonoCallback, GCMonoResizeCallback);
	mono_profiler_install_allocation(GCMonoAllocationCallback);
	//mono_profiler_install_enter_leave (EnterMonoMethod, LeaveMonoMethod);
	//mono_profiler_install_jit_compile (EnterJITCompileMonoMethod, LeaveJITCompileMonoMethod);
	//mono_profiler_install_gc_roots (GCHandleCallback, GCRootCallback);
	mono_profiler_install_class (NULL, MonoLoadClassFunc, MonoUnloadClassFunc, NULL);

	int flags = 
		MONO_PROFILE_GC | 
		MONO_PROFILE_ALLOCATIONS | 
		//MONO_PROFILE_ENTER_LEAVE | 
		//MONO_PROFILE_JIT_COMPILATION |
		//MONO_PROFILE_GC_ROOTS | 
		MONO_PROFILE_CLASS_EVENTS;

	mono_profiler_set_events ((MonoProfileFlags)flags);
}


#endif
