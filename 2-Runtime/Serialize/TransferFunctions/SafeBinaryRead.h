#ifndef SAFEBINARYREAD_H
#define SAFEBINARYREAD_H

#include "Configuration/UnityConfigure.h"

#if SUPPORT_SERIALIZED_TYPETREES

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Serialize/SwapEndianBytes.h"

#include <stack>
class dynamic_bitset;
class SafeBinaryRead;

#define LOG_CONVERTING_VARIBALES 0
#define LOG_MISSING_VARIBALES 0

typedef bool ConversionFunction (void* inData, SafeBinaryRead& transfer);

class EXPORT_COREMODULE SafeBinaryRead : public TransferBase
{
	CachedReader               m_Cache;
	SInt32                  m_BaseBytePosition;
	SInt32                  m_BaseByteSize;

	const TypeTree*         m_OldBaseType;
	#if UNITY_EDITOR
	bool                    m_TypeTreeHasChanged;
	#endif

	enum { kNotFound = 0, kMatchesType = 1, kFastPathMatchesType = 2, kNeedConversion = -1 };

	struct StackedInfo
	{
		const TypeTree* type; /// The type tree of the old type we are reading data from
		const char*     currentTypeName; /// The name of the type we are currently reading (This is the new type name and not from the stored data)
		int             bytePosition;/// byte position of that element
		int             version; /// current version (This is the new version and not from the stored data)

		int             cachedBytePosition; /// The cached byte position of the last visited child
		TypeTree::const_iterator cachedIterator; /// The cached iterator of the last visited child
		#if UNITY_EDITOR
		int             lookupCount; // counts number of looks, used to determine if the typetree matches
		#endif

		#if !UNITY_RELEASE
		std::string     currentTypeNameCheck;/// For debugging purposes in case someone changes the typename string while still reading!
		#endif

	};

	StackedInfo*                   m_CurrentStackInfo;
	SInt32*                           m_CurrentPositionInArray;
	std::stack<StackedInfo> m_StackInfo;

	struct ArrayPositionInfo
	{
		SInt32 arrayPosition;
		SInt32 cachedBytePosition;
		SInt32 cachedArrayPosition;
	};

	std::stack<ArrayPositionInfo>  m_PositionInArray;// position in an array

	bool m_DidReadLastProperty;
	
	friend class MonoBehaviour;

public:

	CachedReader& Init (const TypeTree& oldBase, int bytePosition, int byteSize, int flags);
	CachedReader& Init (SafeBinaryRead& transfer);
	~SafeBinaryRead ();

	void SetVersion (int version);
	bool IsCurrentVersion ();
	bool IsOldVersion (int version);
	bool IsVersionSmallerOrEqual (int version);

	bool IsReading ()                          { return true; }
	bool IsReadingPPtr ()                      { return true; }
	bool IsReadingBackwardsCompatible ()       { return true; }
	bool NeedsInstanceIDRemapping ()                   { return m_Flags & kNeedsInstanceIDRemapping; }
	bool ConvertEndianess ()                   { return m_Flags & kSwapEndianess; }

	bool DidReadLastProperty ()                { return m_DidReadLastProperty; }
	bool DidReadLastPPtrProperty ()            { return m_DidReadLastProperty; }

	template<class T>
	void Transfer (T& data, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags metaFlag = kNoTransferFlags);

	/// In order to transfer typeless data (Read: transfer data real fast)
	/// Call TransferTypeless. You have to always do this. Even for a proxytransfer. Then when you want to access the datablock.
	/// Call TransferTypelessData
	/// On return:
	/// When reading bytesize will contain the size of the data block that should be read,
	/// when writing bytesize has to contain the size of the datablock.
	/// MarkerID will contain an marker which you have to give TransferTypelessData when you want to start the actual transfer.
	/// optional: A serializedFile will be seperated into two chunks. One is the normal object data. (It is assumed that they are all relatively small)
	/// So caching them makes a lot of sense. Big datachunks will be stored in another part of the file.
	/// They will not be cached but usually read directly into the allocated memory, probably reading them asynchronously
	void TransferTypeless (unsigned* byteSize, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);
	// markerID is the id that was given by TransferTypeless.
	// byteStart is the bytestart relative to the beginning of the typeless data
	// copyData is a pointer to where the data will be written or read from
	/// optional: if metaFlag is kTransferBigData the data will be optimized into seperate blocks,
	void TransferTypelessData (unsigned byteSize, void* copyData, int metaData = 0);

	template<class T>
	void TransferBasicData (T& data);

	template<class T>
	void TransferPtr (bool, ReduceCopyData*){}

	template<class T>
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferSTLStyleMap (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	bool GetTransferFileInfo(unsigned* position, const char** filePath) const;

	const TypeTree& GetActiveOldTypeTree () 	{ return *m_CurrentStackInfo->type; }

	static void RegisterConverter (const char* oldType, const char* newType, ConversionFunction* converter);
	static void CleanupConverterTable ();

	#if UNITY_EDITOR
	/// Returns if the typetree is different from what was loaded in.
	/// Currently this is incomplete. Arrays will always return true.
	bool HasDifferentTypeTree ()
	{
		return m_TypeTreeHasChanged;
	}
	#endif

private:

	// BeginTransfer / EndTransfer
	int BeginTransfer (const char* name, const char* typeString, ConversionFunction** converter);
	bool BeginArrayTransfer (const char* name, const char* typeString, SInt32& size);

	// Override the root type name, this is used by scripts that can only determine the class type name after the mono class has actually been loaded
	void OverrideRootTypeName (const char* typeString);

	void EndTransfer ();
	void EndArrayTransfer ();

	void Walk (const TypeTree& typeTree, SInt32* bytePosition);
};

template<class T>inline
void SafeBinaryRead::TransferBasicData (T& data)
{
	m_Cache.Read (data, m_CurrentStackInfo->bytePosition);
	if (ConvertEndianess())
	{
		SwapEndianBytes (data);
	}
}

template<class T> inline
void SafeBinaryRead::TransferSTLStyleArray (T& data, TransferMetaFlags)
{
	SInt32 size = data.size ();
	if (!BeginArrayTransfer ("Array", "Array", size))
		return;

	SerializeTraits<T>::ResizeSTLStyleArray (data, size);

	typename T::iterator i;
	typename T::iterator end = data.end ();
	if (size != 0)
	{
		int conversion = BeginTransfer ("data", SerializeTraits<typename T::value_type>::GetTypeString(&*data.begin()), NULL);
		int elementSize = m_CurrentStackInfo->type->m_ByteSize;
		*m_CurrentPositionInArray = 0;
		// If the data types are matching and element size can be determined
		// then we fast path the whole thing and skip all the duplicate stack walking
		if (conversion == kFastPathMatchesType)
		{
			int basePosition = m_CurrentStackInfo->bytePosition;

			for (i = data.begin ();i != end;++i)
			{
				int currentBytePosition = basePosition + (*m_CurrentPositionInArray) * elementSize;
				m_CurrentStackInfo->cachedBytePosition = currentBytePosition;
				m_CurrentStackInfo->bytePosition = currentBytePosition;
				m_CurrentStackInfo->cachedIterator = m_CurrentStackInfo->type->begin();
				(*m_CurrentPositionInArray)++;
				SerializeTraits<typename T::value_type>::Transfer (*i, *this);
			}
			EndTransfer();
		}
		// Fall back to converting variables
		else
		{
			EndTransfer();
			for (i = data.begin ();i != end;++i)
				Transfer (*i, "data");
		}
	}

	EndArrayTransfer ();
}

template<class T> inline
void SafeBinaryRead::TransferSTLStyleMap (T& data, TransferMetaFlags)
{
	SInt32 size = data.size ();
	if (!BeginArrayTransfer ("Array", "Array", size))
		return;

	// maps value_type is: pair<const First, Second>
	// So we have to write to maps non-const value type
	typename NonConstContainerValueType<T>::value_type p;

	data.clear ();
	for (int i=0;i<size;i++)
	{
		Transfer (p, "data");
		data.insert (p);
	}
	EndArrayTransfer ();
}

template<class T> inline
void SafeBinaryRead::TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags)
{
	ConversionFunction* converter;
	int conversion = BeginTransfer (name, typeName, &converter);
	if (conversion == kNotFound)
		return;

	if (conversion >= kMatchesType)
		SerializeTraits<T>::Transfer (data, *this);
	// Try conversion
	else
	{
		bool success = false;
		if (converter != NULL)
			success = converter (&data, *this);

		#if LOG_CONVERTING_VARIBALES
		{
			string s ("Converting variable ");
			if (success)
				s += " succeeded ";
			else
				s += " failed ";

			GetTypePath (m_OldType.top (), s);
			s = s + " new type: ";
			s = s + " new type: (" + SerializeTraits<T>::GetTypeString () + ")\n";
			m_OldBaseType->DebugPrint (s);
			AssertStringQuiet (s);
		}
		#endif
	}
	EndTransfer ();
}

template<class T> inline
void SafeBinaryRead::Transfer (T& data, const char* name, TransferMetaFlags)
{
	TransferWithTypeString(data, name, SerializeTraits<T>::GetTypeString(&data), kNoTransferFlags);
}

#else

namespace SafeBinaryReadManager
{
	inline void StaticInitialize(){};
	inline void StaticDestroy(){};
}

class SafeBinaryRead
{
public:
	static void RegisterAllowTypeNameConversion (const char* oldTypeName, const char* newTypeName) { }
	static void RegisterAllowNameConversion (const char* type, const char* oldName, const char* newName) { }
	static void CleanupConverterTable() { }
};

#endif
#endif
