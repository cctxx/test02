#ifndef REMAPPPTRTRANSFER_H
#define REMAPPPTRTRANSFER_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Allocator/MemoryMacros.h"

#include <stack>

template<class T>
class PPtr;
template<class T>
class ImmediatePtr;
struct StreamingInfo;

class GenerateIDFunctor
{
	public:

	/// Calls GenerateInstanceID for every PPtr that is found while transferring.
	/// oldInstanceID is the instanceID of the PPtr. metaFlag is the ored metaFlag of the the Transfer trace to the currently transferred pptr.
	/// After GenerateInstanceID returns, the PPtr will be set to the returned instanceID
	/// If you dont want to change the PPtr return oldInstanceID
	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag) = 0;
};


////@todo: Think about a smart way to calculate to let the compiler optimize transfer code of non-pptr classes away
//// currently we have  a serializeTraits::MightContainPPtr but this should be somehow automatic!


/// Transfer that scans for PPtrs and optionally allows to replace them in-place.  Is given a GenerateIDFunctor which maps one
/// or more existing instance IDs to new instance IDs and then crawls through an object's transfers looking for PPtr transfers.
/// Does not touch any other data.
class EXPORT_COREMODULE RemapPPtrTransfer : public TransferBase
{
private:

	GenerateIDFunctor*              m_GenerateIDFunctor;
	UNITY_TEMP_VECTOR(TransferMetaFlags) m_MetaMaskStack;
	TransferMetaFlags               m_CachedMetaMaskStackTop;
	bool                            m_ReadPPtrs;

public:

	RemapPPtrTransfer (int flags, bool readPPtrs);

	void SetGenerateIDFunctor (GenerateIDFunctor* functor)	{ m_GenerateIDFunctor = functor; }
	GenerateIDFunctor* GetGenerateIDFunctor () const { return m_GenerateIDFunctor; }

	bool IsReadingPPtr ()       { return m_ReadPPtrs; }
	bool IsWritingPPtr ()       { return true; }
	bool IsRemapPPtrTransfer () { return true; }

	bool DidReadLastPPtrProperty () { return true; }

	void AddMetaFlag (TransferMetaFlags flag);

	void PushMetaFlag (TransferMetaFlags flag);
	void PopMetaFlag ();

	template<class T>
	void Transfer (T& data, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);
	template<class T>
	void TransferWithTypeString (T& data, const char*, const char*, TransferMetaFlags metaFlag = kNoTransferFlags);

	void TransferTypeless (unsigned*, const char*, TransferMetaFlags /*metaFlag*/ = kNoTransferFlags) { }

	void TransferTypelessData (unsigned, void*, TransferMetaFlags /*metaFlag*/ = kNoTransferFlags) { }

	template<class T>
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferSTLStyleMap (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	SInt32 GetNewInstanceIDforOldInstanceID (SInt32 oldInstanceID) { return m_GenerateIDFunctor->GenerateInstanceID (oldInstanceID, m_CachedMetaMaskStackTop);}
};

template<class T>
struct RemapPPtrTraits
{
	static bool IsPPtr () 												{ return false; }
	static int GetInstanceIDFromPPtr (const T& )		       			{ AssertString ("Never"); return 0; }
	static void SetInstanceIDOfPPtr (T& , SInt32)						{ AssertString ("Never");  }
};

template<class T>
struct RemapPPtrTraits<PPtr<T> >
{
	static bool IsPPtr () 												{ return true; }
	static int GetInstanceIDFromPPtr (const PPtr<T>& data)				{ return data.GetInstanceID (); }
	static void SetInstanceIDOfPPtr (PPtr<T>& data, SInt32 instanceID)	{ data.SetInstanceID (instanceID);  }
};

template<class T>
struct RemapPPtrTraits<ImmediatePtr<T> >
{
	static bool IsPPtr () 														{ return true; }
	static int GetInstanceIDFromPPtr (const ImmediatePtr<T>& data)				{ return data.GetInstanceID (); }
	static void SetInstanceIDOfPPtr (ImmediatePtr<T>& data, SInt32 instanceID)	{ data.SetInstanceID (instanceID);  }
};

template<class T>
void RemapPPtrTransfer::Transfer (T& data, const char*, TransferMetaFlags metaFlag)
{
	if (SerializeTraits<T>::MightContainPPtr ())
	{
		if (metaFlag != 0)
			PushMetaFlag(metaFlag);

		if (RemapPPtrTraits<T>::IsPPtr ())
		{
			AssertIf (m_GenerateIDFunctor == NULL);
			ANALYSIS_ASSUME (m_GenerateIDFunctor);
			SInt32 oldInstanceID = RemapPPtrTraits<T>::GetInstanceIDFromPPtr (data);
			SInt32 newInstanceID = GetNewInstanceIDforOldInstanceID(oldInstanceID);

			if (m_ReadPPtrs)
			{
				RemapPPtrTraits<T>::SetInstanceIDOfPPtr (data, newInstanceID);
			}
			else
			{
				AssertIf(oldInstanceID != newInstanceID);
			}
		}
		else
			SerializeTraits<T>::Transfer (data, *this);

		if (metaFlag != 0)
			PopMetaFlag();
	}
}

template<class T>
void RemapPPtrTransfer::TransferWithTypeString (T& data, const char*, const char*, TransferMetaFlags metaFlag)
{
	Transfer(data, NULL, metaFlag);
}


template<class T>
void RemapPPtrTransfer::TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag)
{
	if (SerializeTraits<typename T::value_type>::MightContainPPtr ())
	{
		typename T::iterator i = data.begin ();
		typename T::iterator end = data.end ();
		while (i != end)
		{
			Transfer (*i, "data", metaFlag);
			++i;
		}
	}
}

template<class T>
void RemapPPtrTransfer::TransferSTLStyleMap (T& data, TransferMetaFlags metaFlag)
{
	typedef typename NonConstContainerValueType<T>::value_type NonConstT;
	if (SerializeTraits<NonConstT>::MightContainPPtr ())
	{
		typename T::iterator i = data.begin ();
		typename T::iterator end = data.end ();

		// maps value_type is: pair<const First, Second>
		// So we have to write to maps non-const value type
		while (i != end)
		{
			NonConstT& p = (NonConstT&) (*i);
			Transfer (p, "data", metaFlag);
			i++;
		}
	}
}

#endif
