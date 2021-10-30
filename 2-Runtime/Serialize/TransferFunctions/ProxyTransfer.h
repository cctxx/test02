#ifndef PROXYTRANSFER_H
#define PROXYTRANSFER_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"

class YAMLNode;
struct StreamingInfo;
struct ReduceCopyData;

class EXPORT_COREMODULE ProxyTransfer : public TransferBase
{
	TypeTree&   m_TypeTree;
	TypeTree*   m_ActiveFather;
	char*       m_ObjectPtr;
	int         m_ObjectSize;
	int			m_Index;
	int			m_SimulatedByteOffset;
	bool        m_DidErrorAlignment;
	bool        m_RequireTypelessData;
	friend class MonoBehaviour;

public:

	ProxyTransfer (TypeTree& t, int flags, void* objectPtr, int objectSize);

	void SetVersion (int version);
	bool IsVersionSmallerOrEqual (int version) { Assert(m_ActiveFather->m_Version > version); return false; }
	bool IsOldVersion (int version) { Assert(m_ActiveFather->m_Version > version); return false; }
	bool NeedNonCriticalMetaFlags () { return (m_Flags & kDontRequireAllMetaFlags) == 0; }

	void AddMetaFlag (TransferMetaFlags flag) { m_ActiveFather->m_MetaFlag = m_ActiveFather->m_MetaFlag | flag; }

	template<class T>
	void Transfer (T& data, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);
	template<class T>
	void TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags metaFlag = kNoTransferFlags);

	void TransferTypeless (unsigned* byteSize, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);
	void TransferTypelessData (unsigned, void*, int metaData = 0);

	template<class T>
	void TransferBasicData (T& data);

	template<class T>
	void TransferPtr (bool, ReduceCopyData*){}

	template<class T>
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferSTLStyleMap (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T> inline
	void TransferSTLStyleArrayWithElement (T& elementType, TransferMetaFlags metaFlag);

	void Align ();

	void BeginTransfer (const char* name, const char* typeString, char* data, TransferMetaFlags metaFlag);
	void EndTransfer ();

	bool GetTransferFileInfo(unsigned* /*position*/, const char** /*filePath*/) const { return false; }

private:

	void LogUnalignedTransfer ();
	void AssertContainsNoPPtr (const TypeTree* typeTree);
	void AssertOptimizeTransfer (int sizeofSize);
	void AssertOptimizeTransferImpl (const TypeTree& typetree, int baseOffset, int* totalSize);
	void CheckAlignment ();


	void BeginArrayTransfer (const char* name, const char* typeString, SInt32& size, TransferMetaFlags metaFlag);
	void EndArrayTransfer ();
};


template<class T> inline
void ProxyTransfer::TransferSTLStyleArray (T& /*data*/, TransferMetaFlags metaFlag)
{
	SInt32 size;
	BeginArrayTransfer ("Array", "Array", size, metaFlag);

	typename T::value_type p;
	Transfer (p, "data");

	// Make sure MightContainPPtr and AllowTransferOptimization is setup correctly
	DebugAssertIf(SerializeTraits<T>::AllowTransferOptimization());

	EndArrayTransfer ();
}

template<class T> inline
void ProxyTransfer::TransferSTLStyleArrayWithElement (T& elementType, TransferMetaFlags metaFlag)
{
	SInt32 size;
	BeginArrayTransfer ("Array", "Array", size, metaFlag);

	Transfer (elementType, "data");

	EndArrayTransfer ();
}


template<class T> inline
void ProxyTransfer::TransferSTLStyleMap (T&, TransferMetaFlags metaFlag)
{
	SInt32 size;
	BeginArrayTransfer ("Array", "Array", size, metaFlag);

	typename NonConstContainerValueType<T>::value_type p;
	Transfer (p, "data");

	#if !UNITY_RELEASE
	DebugAssertIf(SerializeTraits<T>::AllowTransferOptimization());
	#endif

	EndArrayTransfer ();
}

template<class T> inline
void ProxyTransfer::Transfer (T& data, const char* name, TransferMetaFlags metaFlag)
{
	BeginTransfer (name, SerializeTraits<T>::GetTypeString (&data), (char*)&data, metaFlag);
	SerializeTraits<T>::Transfer (data, *this);

	// Make sure MightContainPPtr and AllowTransferOptimization is setup correctly
	#if !UNITY_RELEASE
	if (!SerializeTraits<T>::MightContainPPtr())
		AssertContainsNoPPtr(m_ActiveFather);
	if (SerializeTraits<T>::AllowTransferOptimization())
		AssertOptimizeTransfer(SerializeTraits<T>::GetByteSize());
	#endif

	EndTransfer ();
}

template<class T> inline
void ProxyTransfer::TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags metaFlag)
{
	BeginTransfer (name, typeName, (char*)&data, metaFlag);
		SerializeTraits<T>::Transfer (data, *this);
	EndTransfer ();
}


template<class T>
inline void ProxyTransfer::TransferBasicData (T&)
{
	m_ActiveFather->m_ByteSize = SerializeTraits<T>::GetByteSize ();
	#if UNITY_EDITOR
	if (m_SimulatedByteOffset % m_ActiveFather->m_ByteSize != 0)
	{
		LogUnalignedTransfer();
	}
	m_SimulatedByteOffset += m_ActiveFather->m_ByteSize;
	#endif
}

#endif
