/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 * outfile-writer.c
 *
 * Copyright (C) 2005 Novell, Inc.
 *
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
#include "UnityPrefix.h"

#include "MonoHeapShotWriter.h"

#if ENABLE_MONO_HEAPSHOT
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Mono/MonoBehaviour.h"

#define MAGIC_NUMBER 0x4eabfdd1
#define FILE_FORMAT_VERSION 6
#define FILE_LABEL "heap-shot logfile"
#define TAG_TYPE	0x01
#define TAG_OBJECT  0x02
#define TAG_UNITY  0x03
#define TAG_EOS	 0xff



extern "C"
{
//	extern char* mono_type_full_name (MonoType *type);
	void mono_free (void* p)
	{
		g_free(p);
	}
}


MonoHeapShotWriter::MonoHeapShotWriter()
{
	m_Data = NULL;
	m_SeenItems = new ClassHash();
}
MonoHeapShotWriter::~MonoHeapShotWriter()
{
	delete m_SeenItems;
}
void MonoHeapShotWriter::Start(HeapShotData* data)
{
	m_Data = data;
	m_Data->clear();
	m_SeenItems->clear();
	WriteUInt32 (MAGIC_NUMBER);
	WriteInt32 (FILE_FORMAT_VERSION);
	WriteString (FILE_LABEL);
	m_SavedOutfileOffset = m_Data->size();
	// we update these after dumping all objects
	WriteInt32 (0);   // total # of types
	WriteInt32 (0);   // total # of objects
	WriteInt32 (0); // total # of references
	WriteInt32 (0); // total # of fields
}

void MonoHeapShotWriter::End()
{
	// Write out the end-of-stream tag.
	WriteByte (TAG_EOS);
	
	memcpy(&(*m_Data)[m_SavedOutfileOffset], &m_TypeCount, sizeof(SInt32));
	memcpy(&(*m_Data)[m_SavedOutfileOffset + sizeof(SInt32)], &m_ObjecCount, sizeof(SInt32));
	memcpy(&(*m_Data)[m_SavedOutfileOffset + sizeof(SInt32) * 2], &m_ReferenceCount, sizeof(SInt32));
	memcpy(&(*m_Data)[m_SavedOutfileOffset + sizeof(SInt32) * 3], &m_FieldCount, sizeof(SInt32)); 
}
void MonoHeapShotWriter::BeginObjectDump(MonoObject* obj, MonoClass* klass)
{
	char *name;
	
	/* First, add this type if we haven't seen it before. */
	//if (g_hash_table_lookup (m_SeenItems, klass) == NULL)
	if (m_SeenItems->find(klass) == m_SeenItems->end())
	{
		MonoClassField *field;
		MonoClass *cur_klass = klass;
		void* iter;

		name = mono_type_full_name (mono_class_get_type (klass));
		WriteByte (TAG_TYPE);
		WritePointer (klass);
		WriteString (name);
		mono_free (name);
		(*m_SeenItems)[klass] = klass;
		//g_hash_table_insert (m_SeenItems, klass, klass);
		++m_TypeCount;

		// Write every field
		do 
		{
			iter = NULL;
			while ((field = mono_class_get_fields (cur_klass, &iter)) != NULL) {
				WritePointer (field);
				WriteString (mono_field_get_name (field));
				m_FieldCount++;
			}
			cur_klass = mono_class_get_parent (cur_klass);
		} 
		while (cur_klass);
		WritePointer (NULL);
	}
	

	WriteByte (TAG_OBJECT);
	if (obj) 
	{
		WritePointer ((void*)obj);	// id of the object
		WritePointer (klass);			// class
		WriteInt32 ((SInt32)mono_object_get_size (obj));
	} 
	else 
	{
		// Used to register references from static class members
		WritePointer ((void*)klass);
		WritePointer (klass);
		WriteInt32 ((SInt32)0);
	}
	m_ObjecCount++;

}

void MonoHeapShotWriter::EndObjectDump()
{
	WritePointer (NULL);	// no more references
}

void MonoHeapShotWriter::DumpReference(void* ref, void* field)
{
	WritePointer (ref);
	WritePointer (field);
	m_ReferenceCount++;
}
void MonoHeapShotWriter::DumpObjectsReferencedByUnity()
{
	FindMode mode = Scripting::kFindAssets;
	MonoClass* compareKlass = GetMonoManager().GetBuiltinMonoClass ("Object", false);
	if (compareKlass == NULL)
	{
		ErrorString ("DumpObjectsReferencedByUnity: Invalid Type");
		return ;
	}
	
	int classID = GetClassIDFromScriptingClass (compareKlass);
	
	if (classID == -1)
	{
		string klassName = mono_class_get_name (compareKlass);
		ErrorString ("DumpObjectsReferencedByUnity: The type has to be derived from UnityEngine.Object. Type is " + klassName + ".");
		return ;
	}

	// Gather the derived objects
	std::vector<SInt32> objects;
	Object::FindAllDerivedObjects (classID, &objects);
	
	sort(objects.begin(), objects.end());
	
	// We might need to ignore some objects which are not derived from the mono class but from MonoBehaviour
	// so we store them in a buffer and then copy them into the mono array
	MonoObject** monoObjects;
	ALLOC_TEMP(monoObjects, MonoObject*, objects.size(), kMemProfiler);
	

	int count = 0;
	for (int i=0;i<objects.size ();i++)
	{
		Object& object = *PPtr<Object> (objects[i]);
		if (object.TestHideFlag (Object::kDontSave) && mode != Scripting::kFindAnything)
			continue;
	
		
		MonoObject* mono = ScriptingWrapperFor (&object);
		if (mono)
		{
			if (object.IsDerivedFrom (ClassID (MonoBehaviour)))
			{	
				MonoClass* klass = mono_object_get_class (mono);
				if (mono_class_is_subclass_of (klass, compareKlass, true))
					monoObjects[count++] = mono;
			}
			else
				monoObjects[count++] = mono;
		}
	}
	WriteByte (TAG_UNITY);
	for (int i = 0; i < count; i++)
	{
		WritePointer ((void*)monoObjects[i]);
	}
	WritePointer(0);
}
void MonoHeapShotWriter::WriteByte (UInt8 x)
{
	m_Data->push_back (x);
}

void MonoHeapShotWriter::WritePointer (void* x)
{
	UInt32 y = (UInt32) (x);
	WriteUInt32 (y);
}

void MonoHeapShotWriter::WriteInt32 (SInt32 x)
{
	SwapEndianBytesLittleToNative(x);
   	int o = m_Data->size();
	m_Data->resize(o + sizeof(UInt32));
	memcpy(&(*m_Data)[o], &x, sizeof(SInt32));
}

void MonoHeapShotWriter::WriteUInt32 (UInt32 x)
{
	SwapEndianBytesLittleToNative(x);
	int o = m_Data->size();
	m_Data->resize(o + sizeof(UInt32));
	memcpy(&(*m_Data)[o], &x, sizeof(UInt32));
}

void MonoHeapShotWriter::WriteVInt (UInt32 x)
{
	UInt8 y;
	do 
	{
		y = (UInt8) (x & 0x7f);
		x = x >> 7;
		if (x != 0) y |= 0x80;
		WriteByte (y);
	} 
	while (x != 0);
}

void MonoHeapShotWriter::WriteString (const char *str)
{
	int len = strlen (str);
	WriteVInt ((UInt32) len);
	if (len > 0)
	{
		int o = m_Data->size();
		m_Data->resize(o + len);
		memcpy(&(*m_Data)[o], str, len);
	}
}

#endif
