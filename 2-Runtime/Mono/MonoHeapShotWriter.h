#ifndef MONOHEAPSHOTWRITER_H
#define MONOHEAPSHOTWRITER_H

#include "Runtime/Mono/MonoTypes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "MonoHeapShot.h"

#if ENABLE_MONO_HEAPSHOT


class MonoHeapShotWriter
{
public:
	MonoHeapShotWriter();
	~MonoHeapShotWriter();

	void Start(HeapShotData* data);

	void End();

	void DumpObjectsReferencedByUnity();

	void BeginObjectDump(MonoObject* obj, MonoClass* klass);
	
	void EndObjectDump();
	
	void DumpReference(gpointer ref, gpointer field);
	
	inline const UInt8* GetData() const 
	{
		return &(*m_Data)[0];
	}
	inline UInt32 GetDataSize() const
	{
		return m_Data->size();
	}

private:
	void WriteByte (UInt8 x);

	void WritePointer (void* x);

	void WriteInt32 (SInt32 x);

	void WriteUInt32 (UInt32 x);

	void WriteVInt (UInt32 x);

	void WriteString (const char *str);

	typedef std::vector<UInt8> Data;
	HeapShotData* m_Data;
	ClassHash* m_SeenItems;
	int		m_TypeCount;
    int		m_ObjecCount;
    int		m_ReferenceCount;
    int		m_FieldCount;
    long	m_SavedOutfileOffset;
};

#endif

#endif

