#include "UnityPrefix.h"
#include "TransferUtility.h"
#include "TypeTree.h"
#include "IterateTypeTree.h"
#include "SwapEndianBytes.h"
#include "FileCache.h"
#include "CacheWrap.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/BaseObject.h"

#define DEBUG_PRINT_WALK 0

using namespace std;

#if UNITY_EDITOR

void CountVariables (const TypeTree& typeTree, UInt8** data, int* variableCount);

std::string ExtractPPtrClassName (const TypeTree& typeTree)
{
	return ExtractPPtrClassName(typeTree.m_Type);
}

std::string ExtractMonoPPtrClassName (const TypeTree& typeTree)
{
	return ExtractMonoPPtrClassName(typeTree.m_Type);
}

std::string ExtractPPtrClassName (const std::string& typeName)
{
	if (typeName.size () >= 6 && typeName.find ("PPtr<") == 0)
	{
		if (typeName[5] == '$')
			return std::string ();
		else
		{
			string className (typeName.begin() + 5, typeName.end() - 1);
			return className;
		}
	}
	else
		return std::string ();
}

std::string ExtractMonoPPtrClassName (const std::string& typeName)
{
	if (typeName.size () >= 7 && typeName.find ("PPtr<$") == 0)
	{
		string className (typeName.begin() + 6, typeName.end() - 1);
		return className;
	}
	else
		return std::string ();
}

// Walk through typetree and data to find the bytePosition and variablePosition.
void WalkTypeTree (const TypeTree& typeTree, const UInt8* data, int* bytePosition)
{
	AssertIf (bytePosition == NULL);

#if DEBUG_PRINT_WALK
	const TypeTree* parent = &typeTree;
	while (parent->m_Father)
	{
		parent = parent->m_Father;
		printf_console("\t");
	}

	printf_console("%s (%s) position: %d\n", typeTree.m_Type.c_str(), typeTree.m_Name.c_str(), *bytePosition);
#endif

	AssertIf((typeTree.m_ByteSize != -1 && ((typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0 || typeTree.m_Children.empty())) != (typeTree.m_ByteSize != -1 && (typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0));

	/// During the 2.1 beta we had a bug that generated kAnyChildUsesAlignBytesFlag incorrectly,
	/// this was only available in development builds thus we require the (|| typeTree.m_Children.empty())
	bool hasBasicTypeSize = typeTree.m_ByteSize != -1 && (typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0 || typeTree.m_Children.empty();

	if (hasBasicTypeSize)
	{
		AssertIf (typeTree.m_ByteSize == -1);
		*bytePosition += typeTree.m_ByteSize;
	}
	else if (typeTree.m_IsArray)
	{
		// First child in an array is the size
		// Second child is the homogenous type of the array
		AssertIf (typeTree.m_Children.front ().m_Type != SerializeTraits<SInt32>::GetTypeString (NULL));
		AssertIf (typeTree.m_Children.front ().m_Name != "size");
		AssertIf (typeTree.m_Children.size () != 2);

		SInt32 arraySize, i;

		arraySize = *reinterpret_cast<const SInt32*> (&data[*bytePosition]);

#if DEBUG_PRINT_WALK
		printf_console("Array Size %d position: %d\n", arraySize, *bytePosition);
#endif

		*bytePosition += sizeof (arraySize);

		const TypeTree& elementTypeTree = typeTree.m_Children.back ();

		// If the bytesize is known we can simply skip the recursive loop
		if (elementTypeTree.m_ByteSize != -1 && (elementTypeTree.m_MetaFlag & (kAlignBytesFlag | kAnyChildUsesAlignBytesFlag)) == 0)
			*bytePosition += arraySize * elementTypeTree.m_ByteSize;			
		// Otherwise recursively Walk element typetree
		else
		{
			for (i=0;i<arraySize;i++)
				WalkTypeTree (elementTypeTree, data, bytePosition);
		}
	}
	else
	{
		AssertIf (typeTree.m_Children.empty ());

		TypeTree::TypeTreeList::const_iterator i;
		for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
			WalkTypeTree (*i, data, bytePosition);
	}

	if (typeTree.m_MetaFlag & kAlignBytesFlag)
	{
#if DEBUG_PRINT_WALK
		printf_console("Align %d %d", *bytePosition, Align4(*bytePosition));
#endif
		*bytePosition = Align4(*bytePosition);
	}
}

struct ByteSwapGenericIterator
{
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (typeTree.IsBasicDataType ())
		{
			if (typeTree.m_ByteSize == 4)
			{
				// Color does not get byteswapped
				if (typeTree.m_Father->m_Type != "ColorRGBA")
					SwapEndianBytes(*reinterpret_cast<UInt32*> (&data[bytePosition]));
			}
			else if (typeTree.m_ByteSize == 2)
				SwapEndianBytes(*reinterpret_cast<UInt16*> (&data[bytePosition]));
			else if (typeTree.m_ByteSize == 8)
				SwapEndianBytes(*reinterpret_cast<double*> (&data[bytePosition]));
			else if (typeTree.m_ByteSize != 1)
			{
				AssertString (Format("Unsupported data type when byteswapping %s", typeTree.m_Type.c_str()));
			}

			if (typeTree.m_Type == "TypelessData")
			{
				AssertString ("It is not possible to use Generic byteswap for typeless arrays!");
			}
		}
		return true;
	}
};

void ByteSwapGeneric (const TypeTree& typeTree, dynamic_array<UInt8>& data)
{
	ByteSwapGenericIterator functor;
	IterateTypeTree (typeTree, data, functor);
}

SInt32 CalculateByteSize(const TypeTree& type, const UInt8* data)
{
	int position = 0;
	WalkTypeTree (type, data, &position);
	return position;
}

#endif // UNITY_EDITOR

#if !UNITY_EXTERNAL_TOOL

int FindTypeTreeSeperator (const char* in)
{
	const char* c = in;
	while (*c != '.' && *c != '\0')
		c++;
	return c - in;
}

const TypeTree* FindAttributeInTypeTreeNoArrays (const TypeTree& typeTree, const char* path)
{
	int seperator = FindTypeTreeSeperator (path);
	// Search all typetree children for a name that is the same as the string path with length seperator
	for (TypeTree::const_iterator i=typeTree.begin ();i != typeTree.end ();++i)
	{
		// Early out if size is not the same
		if (i->m_Name.size () != seperator)
			continue;

		// continue if the name isn't the same
		TypeTreeString::const_iterator n = i->m_Name.begin ();
		int j;
		for (j=0;j<seperator;j++,n++)
		{
			if (path[j] != *n)
				break;
		}
		if (j != seperator)
			continue;

		// We found the attribute we were searching for
		if (path[seperator] == '\0')
			return &*i;
		// Recursively find in the children
		else
			return FindAttributeInTypeTreeNoArrays (*i, path + seperator + 1);
	}
	return NULL;
}

void GenerateTypeTree (Object& object, TypeTree* typeTree, int options)
{
	AssertIf (typeTree == NULL);
	*typeTree = TypeTree ();
	ProxyTransfer proxy (*typeTree, options, &object, Object::ClassIDToRTTI (object.GetClassID ())->size);
	object.VirtualRedirectTransfer (proxy);
}

template<bool swapEndian>
static inline void WriteObjectToVector (Object& object, dynamic_array<UInt8>* data, int options)
{
	Assert (data != NULL);
	data->clear ();

	MemoryCacheWriter memoryCache (*data);
	StreamedBinaryWrite<swapEndian> writeStream;
	CachedWriter& writeCache = writeStream.Init (options, BuildTargetSelection::NoTarget());

	writeCache.InitWrite (memoryCache);
	object.VirtualRedirectTransfer (writeStream);

	if (!writeCache.CompleteWriting () || writeCache.GetPosition() != data->size ())
		ErrorString ("Error while writing serialized data.");
}

void WriteObjectToVector (Object& object, dynamic_array<UInt8>* data, int options)
{
	WriteObjectToVector<false> (object, data, options);
}

template<bool swapEndian>
void ReadObjectFromVector (Object* object, const dynamic_array<UInt8>& data, int options)
{
	Assert (object != NULL);

	MemoryCacheReader memoryCache (const_cast<dynamic_array<UInt8>&> (data));
	StreamedBinaryRead<swapEndian> readStream;
	CachedReader& readCache = readStream.Init (options);
	readCache.InitRead (memoryCache, 0, data.size ());

	object->VirtualRedirectTransfer (readStream);
	unsigned position = readCache.End ();

	// we read up that object - no need to call Reset as we constructed it fully
	object->HackSetResetWasCalled();

	if (position > (int) data.size ())
		ErrorString ("Error while reading serialized data.");
}

void ReadObjectFromVector (Object* object, const dynamic_array<UInt8>& data, int options)
{
	ReadObjectFromVector<false> (object, data, options);
}

#if SUPPORT_TEXT_SERIALIZATION

string WriteObjectToString (Object& object, int options)
{
	YAMLWrite write (options);
	object.VirtualRedirectTransfer (write);

	string result;
	write.OutputToString(result);

	return result;
}

void ReadObjectFromString (Object* object, string& string, int options)
{
	Assert (object != NULL);
	YAMLRead read (string.c_str (), string.length (), options);
	object->VirtualRedirectTransfer (read);
}

#endif // SUPPORT_TEXT_SERIALIZATION

#endif // !UNITY_EXTERNAL_TOOL

#if UNITY_EDITOR

void ReadObjectFromVector (Object* object, const dynamic_array<UInt8>& data, const TypeTree& typeTree, int options)
{
	Assert (object != NULL);

	MemoryCacheReader memoryCache (const_cast<dynamic_array<UInt8>&> (data));
	SafeBinaryRead readStream;
	CachedReader& readCache = readStream.Init (typeTree, 0, data.size (), options);
	readCache.InitRead (memoryCache, 0, data.size ());

	object->VirtualRedirectTransfer (readStream);
	readCache.End ();

	// we will read up that object - no need to call Reset as we will construct it fully
	object->HackSetResetWasCalled();
}

void WriteObjectToVectorSwapEndian (Object& object, dynamic_array<UInt8>* data, int options)
{
	WriteObjectToVector<true> (object, data, options);
}

void CountVariables (const TypeTree& typeTree, UInt8** data, int* variableCount)
{
	if (typeTree.m_IsArray)
	{
		AssertIf (typeTree.m_Children.size () != 2);
		AssertIf (typeTree.m_Children.front ().m_Type != "SInt32");

		SInt32 arraySize = *reinterpret_cast<SInt32*> (*data);
		*data += sizeof (SInt32);
		*variableCount += 1;

		for (int i=0;i<arraySize;i++)
			CountVariables (typeTree.m_Children.back (), data, variableCount);
	}
	else if (!typeTree.IsBasicDataType ())
	{
		for (TypeTree::const_iterator i=typeTree.m_Children.begin ();i != typeTree.m_Children.end ();i++)
			CountVariables (*i, data, variableCount);
	}
	else
	{
		*variableCount += 1;
		*data += typeTree.m_ByteSize;
	}
}

int CountTypeTreeVariables (const TypeTree& typeTree)
{
	if (typeTree.m_Children.empty ())
		return typeTree.m_Index + 1;
	else
		return CountTypeTreeVariables (*typeTree.m_Children.rbegin ());
}

#endif // UNITY_EDITOR

#if !UNITY_EXTERNAL_TOOL
class IDCollectorFunctor : public GenerateIDFunctor
{
	set<SInt32>*	m_IDs;

public:	

	IDCollectorFunctor (set<SInt32>* ptrs)
	{
		m_IDs = ptrs;
	}
	virtual ~IDCollectorFunctor () {}


	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag = kNoTransferFlags)
	{
		// Only strong pptrs can insert new ids
		if ((metaFlag & kStrongPPtrMask) == 0)
			return oldInstanceID;

		Object* object = PPtr<Object> (oldInstanceID);
		if (object == NULL)
			return oldInstanceID;

		// Already Inserted?
		if (!m_IDs->insert (oldInstanceID).second)
			return oldInstanceID;

		RemapPPtrTransfer transferFunction (0, false);
		transferFunction.SetGenerateIDFunctor (this);
		object->VirtualRedirectTransfer (transferFunction);

		return oldInstanceID;
	}
};

void CollectPPtrs (Object& object, set<SInt32>* collectedPtrs)
{
	IDCollectorFunctor collectFunctor (collectedPtrs);
	collectFunctor.GenerateInstanceID (object.GetInstanceID (), kStrongPPtrMask);
}

void CollectPPtrs (Object& object, vector<Object*>& collectedObjects)
{
	Assert(collectedObjects.empty());
	set<SInt32> output;
	CollectPPtrs (object, &output);

	for (set<SInt32>::iterator i=output.begin();i != output.end();i++)
	{
		Object* obj = dynamic_instanceID_cast<Object*> (*i);
		if (obj)
			collectedObjects.push_back(obj);
	}

}

/// We should use a serialize remapper which clears strong pptrs instead.
void CopySerialized(Object& src, Object& dst)
{
	dynamic_array<UInt8> data(kMemTempAlloc);

	if (src.GetClassID() != dst.GetClassID())
	{
		ErrorString("Source and Destination Types do not match");
		return;
	}

	if (src.GetNeedsPerObjectTypeTree())
	{
		// Verify that the typetree matches, otherwise when the script pointer changes comparing the data will read out of bounds.
		TypeTree srcTypeTree;
		TypeTree dstTypeTree;
		GenerateTypeTree (src, &srcTypeTree, kSerializeForPrefabSystem);
		GenerateTypeTree (dst, &dstTypeTree, kSerializeForPrefabSystem);

		if (!IsStreamedBinaryCompatbile(srcTypeTree, dstTypeTree))
		{
			ErrorString("Source and Destination Types do not match");
			return;
		}
	}


	WriteObjectToVector(src, &data, kSerializeForPrefabSystem);
	ReadObjectFromVector(&dst, data, kSerializeForPrefabSystem);

#if UNITY_EDITOR
	dst.CloneAdditionalEditorProperties(src);
#endif

	// we copied that object - no need to call Reset as we constructed it fully
	dst.HackSetResetWasCalled();

	dst.AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

	dst.SetDirty();
}

void CompletedReadObjectFromVector (Object& object)
{
	object.CheckConsistency ();
	object.AwakeFromLoad (kDefaultAwakeFromLoad);
	object.SetDirty ();
}

#endif // !UNITY_EXTERNAL_TOOL
