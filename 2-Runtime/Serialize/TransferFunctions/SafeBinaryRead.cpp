#include "UnityPrefix.h"
#include "SafeBinaryRead.h"
#include "TransferNameConversions.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"

#define LOG_MISSING_VARIBALES 0

#if SUPPORT_SERIALIZED_TYPETREES

using namespace std;

typedef map<pair<char*, char*>, ConversionFunction*,  smaller_cstring_pair> ConverterFunctions;
static ConverterFunctions* gConverterFunctions;

namespace SafeBinaryReadManager
{
	void StaticInitialize()
	{
		gConverterFunctions = UNITY_NEW(ConverterFunctions,kMemSerialization);
	}
	void StaticDestroy()
	{
		UNITY_DELETE(gConverterFunctions,kMemSerialization);
	}
}
static RegisterRuntimeInitializeAndCleanup s_SafeBinaryReadManagerCallbacks(SafeBinaryReadManager::StaticInitialize, SafeBinaryReadManager::StaticDestroy);

ConversionFunction* FindConverter (const char* oldType, const char* newTypeName)
{
	pair<char*, char*> arg = make_pair(const_cast<char*> (oldType), const_cast<char*> (newTypeName));
	
	ConverterFunctions::iterator found = gConverterFunctions->find (arg);
	if (found == gConverterFunctions->end())
		return NULL;
	
	return found->second;
}

void SafeBinaryRead::RegisterConverter (const char* oldType, const char* newType, ConversionFunction* converter)
{
	
	pair<char*, char*> arg = make_pair(const_cast<char*> (oldType), const_cast<char*> (newType));
	AssertMsg (!gConverterFunctions->count (arg), "Duplicate conversion registered");
	(*gConverterFunctions)[arg] = converter;
}

void SafeBinaryRead::CleanupConverterTable ()
{
	(*gConverterFunctions).clear();
}

static void Walk (const TypeTree& typeTree, CachedReader& cache, SInt32* bytePosition, bool endianSwap);

CachedReader& SafeBinaryRead::Init (const TypeTree& oldBase, int bytePosition, int byteSize, int flags)
{
	AssertIf (!m_StackInfo.empty ());
	m_OldBaseType = &oldBase;
	m_BaseBytePosition = bytePosition;
	AssertIf (m_BaseBytePosition < 0);
	m_BaseByteSize = byteSize;
	m_Flags = flags;
	m_UserData = NULL;
	m_DidReadLastProperty = false;
	#if UNITY_EDITOR
	m_TypeTreeHasChanged = false;
	#endif	
	return m_Cache;
}

CachedReader& SafeBinaryRead::Init (SafeBinaryRead& transfer)
{
	int newBasePosition = transfer.m_StackInfo.top ().bytePosition;
	int size = transfer.m_BaseByteSize - (newBasePosition - transfer.m_BaseBytePosition);
	Init (*transfer.m_StackInfo.top ().type, newBasePosition, size, transfer.m_Flags);
	m_Cache.InitRead (*transfer.m_Cache.GetCacher(), transfer.m_StackInfo.top ().bytePosition, size);
	m_UserData = NULL;
	m_DidReadLastProperty = false;
	#if UNITY_EDITOR
	m_TypeTreeHasChanged = false;
	#endif	
	
	return m_Cache;
}

SafeBinaryRead::~SafeBinaryRead ()
{
	AssertIf (!m_StackInfo.empty ());
	AssertIf (!m_PositionInArray.empty ());
}

static void Walk (const TypeTree& typeTree, CachedReader& cache, SInt32* bytePosition, bool endianSwap)
{
	AssertIf (bytePosition == NULL);

	AssertIf((typeTree.m_ByteSize != -1 && ((typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0 || typeTree.m_Children.empty())) != (typeTree.m_ByteSize != -1 && (typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0));

	if (typeTree.m_ByteSize != -1 && (typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0)
	{
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
		cache.Read (arraySize, *bytePosition);
		if (endianSwap)
			SwapEndianBytes(arraySize);
			
		*bytePosition += sizeof (arraySize);

		const TypeTree& elementTypeTree = typeTree.m_Children.back ();

		// If the bytesize is known we can simply skip the recursive loop
		if (elementTypeTree.m_ByteSize != -1 && (elementTypeTree.m_MetaFlag & (kAnyChildUsesAlignBytesFlag | kAlignBytesFlag)) == 0)
			*bytePosition += arraySize * elementTypeTree.m_ByteSize;			
		// Otherwise recursively Walk element typetree
		else
		{
			for (i=0;i<arraySize;i++)
				Walk (typeTree.m_Children.back (), cache, bytePosition, endianSwap);	
		}
	}
	else
	{
		TypeTree::TypeTreeList::const_iterator i;
		for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
			Walk (*i, cache, bytePosition, endianSwap);
	}
	
	if (typeTree.m_MetaFlag & kAlignBytesFlag)
	{
		#if UNITY_EDITOR
//		const TypeTree* root = &typeTree;
//		while (root->m_Father != NULL)
//			root = root->m_Father;
//		if (root->m_Type == "MonoBehaviour")
//			ErrorString("Alignment in monobehaviour???");
		#endif
		*bytePosition = Align4(*bytePosition);
	}
}

// Walk through typetree and data to find the bytePosition
void SafeBinaryRead::Walk (const TypeTree& typeTree, SInt32* bytePosition)
{
	::Walk (typeTree, m_Cache, bytePosition, ConvertEndianess());
}

void SafeBinaryRead::OverrideRootTypeName (const char* typeString)
{
	Assert(m_StackInfo.size() == 1);
	m_StackInfo.top().currentTypeName = typeString;
	#if !UNITY_RELEASE
	m_StackInfo.top().currentTypeNameCheck = typeString;
	#endif
}


int SafeBinaryRead::BeginTransfer (const char* name, const char* typeString, ConversionFunction** converter)
{
	if (converter != NULL)
		*converter = NULL;
	
	m_DidReadLastProperty = false;
	
	// For the first Transfer only setup the stack to the base parameters
	if (m_StackInfo.empty ())
	{
		ErrorIf (name != m_OldBaseType->m_Name);
		
		StackedInfo newInfo;
		newInfo.type = m_OldBaseType;
		newInfo.bytePosition = m_BaseBytePosition;
		newInfo.version = 1;
		#if UNITY_EDITOR
		newInfo.lookupCount = 0;
		#endif
		newInfo.currentTypeName = typeString;
		#if !UNITY_RELEASE
		newInfo.currentTypeNameCheck = typeString;
		#endif
		newInfo.cachedIterator = newInfo.type->begin();
		newInfo.cachedBytePosition = m_BaseBytePosition;
		m_StackInfo.push(newInfo);
		m_CurrentStackInfo = &m_StackInfo.top();
		
		return kMatchesType;
	}
	
	TypeTree::TypeTreeList::const_iterator c;
	
	StackedInfo& info = *m_CurrentStackInfo;
	
	const TypeTree::TypeTreeList& children = info.type->m_Children;

	// Start searching at the cached position
	SInt32 newBytePosition = info.cachedBytePosition;
	int count = 0;
	for (c=info.cachedIterator;c != children.end ();++c)
	{
		if (c->m_Name == name)
			break;
		
		// Walk through old typetree, updating position
		Walk (*c, &newBytePosition);
		count++;
	}
	
	#if UNITY_EDITOR
	if (count > 1)
		m_TypeTreeHasChanged = true;
	#endif
		
	// Didn't find it, try again starting at the first child
	if (c == children.end ())
	{
		#if UNITY_EDITOR
		m_TypeTreeHasChanged = true;
		#endif
		
		// Find name conversion lookup for this type
	#if !UNITY_RELEASE
		DebugAssertIf(info.currentTypeName != info.currentTypeNameCheck);
	#endif
		const AllowNameConversion::mapped_type* nameConversion = GetAllowedNameConversions(info.currentTypeName, name);

		newBytePosition = info.bytePosition;
		for (c=children.begin();c != children.end();++c)
		{
			if (c->m_Name == name)
				break;
			if (nameConversion && nameConversion->count(const_cast<char*>(c->m_Name.c_str())))
				break;
			
			// Walk through old typetree, updating position
			Walk (*c, &newBytePosition);
		}
		
		// No child with name was found?
		if (c == children.end ())
		{
			#if LOG_MISSING_VARIBALES
			string s ("Variable not found in old file ");
			GetTypePath (m_StackInfo.top ().type, s);
			s = s + " new name and type: " + name;
			s = s + '(' + typeString + ")\n";
			m_OldBaseType->DebugPrint (s);
			LogString (s);
			#endif

			return kNotFound;
		}
	}
	
	#if UNITY_EDITOR
	m_CurrentStackInfo->lookupCount++;
	#endif
	
	info.cachedIterator = c;
	info.cachedBytePosition = newBytePosition;
	
	/*Unoptimized version:

	// Find name in children typeTree, updating position
	SInt32 newBytePosition = info.bytePosition;
	
	// Find name conversion lookup for this type
	const AllowNameConversion::mapped_type* nameConversion = NULL;
	DebugAssertIf(info.currentTypeName != info.currentTypeNameCheck);
	AllowNameConversion::iterator foundNameConversion = gAllowNameConversion.find(make_pair(const_cast<char*>(info.currentTypeName), const_cast<char*>(name)));
	if (foundNameConversion == gAllowNameConversion.end())
		nameConversion = &foundNameConversion->second;
	
	for (c=children.begin ();c != children.end ();++c)
	{
		if (c->m_Name == name)
			break;
		if (nameConversion && nameConversion->count(const_cast<char*>(c->m_Name.c_str())))
			break;

		// Walk through old typetree, updating position
		Walk (*c, &newBytePosition);
	}

	// No child with name was found?
	if (c == children.end ())
	{
		#if LOG_MISSING_VARIBALES
		string s ("Variable not found in old file ");
		GetTypePath (m_OldType.top (), s);
		s = s + " new name and type: " + name;
		s = s + '(' + typeString + ")\n";
		m_OldBaseType->DebugPrint (s);
		AssertStringQuiet (s);
		#endif

		return kNotFound;
	}
	*/

	// Walk trough the already iterated array elements
	if (info.type->m_IsArray && c != children.begin ())
	{
		SInt32 arrayPosition = *m_CurrentPositionInArray;
		
		// There are no arrays in the subtree so
		// we can simply use the cached bytesize
		// Alignment cuts across this so use the slow path in that case
		if (c->m_ByteSize != -1 && (c->m_MetaFlag & (kAnyChildUsesAlignBytesFlag | kAlignBytesFlag)) == 0)
		{
			newBytePosition += c->m_ByteSize * arrayPosition;
		}
		// Walk through old typetree, updating position			
		else
		{
			ArrayPositionInfo& arrayInfo = m_PositionInArray.top();
			SInt32 cachedArrayPosition = 0;	
			if (arrayInfo.cachedArrayPosition <= arrayPosition)
			{
				newBytePosition = arrayInfo.cachedBytePosition;
				cachedArrayPosition = arrayInfo.cachedArrayPosition;
			}

			for (SInt32 i = cachedArrayPosition;i < arrayPosition;i++)
				Walk (*c, &newBytePosition);

			arrayInfo.cachedArrayPosition = arrayPosition;
			arrayInfo.cachedBytePosition = newBytePosition;
		}
		
		(*m_CurrentPositionInArray)++;
	}			
	
	StackedInfo newInfo;
	newInfo.type = &*c;
	newInfo.bytePosition = newBytePosition;
	newInfo.version = 1;
	#if UNITY_EDITOR
	newInfo.lookupCount = 0;
	#endif
	newInfo.cachedIterator = newInfo.type->begin();
	newInfo.cachedBytePosition = newBytePosition;
	newInfo.currentTypeName = typeString;
	#if !UNITY_RELEASE
	newInfo.currentTypeNameCheck = typeString;
	#endif

	m_StackInfo.push(newInfo);
	m_CurrentStackInfo = &m_StackInfo.top();

	int conversion = kNeedConversion;
	// Does the type match (compare type string)
	// The root type should get a transfer in any case because the type might change
	// Eg. TransformComponent renamed to Transform (Typename mismatch but we still want to serialize)
	if (c->m_Type == typeString || m_StackInfo.size () == 1)
	{
		conversion = kMatchesType;
		if (c->m_ByteSize != -1 && (c->m_MetaFlag & (kAnyChildUsesAlignBytesFlag | kAlignBytesFlag)) == 0)
			conversion = kFastPathMatchesType;
	}
	else if (AllowTypeNameConversion (c->m_Type, typeString))
	{
		conversion = kMatchesType;
		if (c->m_ByteSize != -1 && (c->m_MetaFlag & (kAnyChildUsesAlignBytesFlag | kAlignBytesFlag)) == 0)
			conversion = kFastPathMatchesType;
		#if UNITY_EDITOR
		m_TypeTreeHasChanged = true;
		#endif
	}
	else
	{
		#if UNITY_EDITOR
		m_TypeTreeHasChanged = true;
		#endif
	}
	
	if (conversion == kNeedConversion && converter != NULL)
		*converter = FindConverter(c->m_Type.c_str(), typeString);
	
	return conversion;
}

void SafeBinaryRead::SetVersion (int version)
{
	m_CurrentStackInfo->version = version;
}


void SafeBinaryRead::EndTransfer ()
{
	#if UNITY_EDITOR
	if (m_CurrentStackInfo && m_CurrentStackInfo->lookupCount != m_CurrentStackInfo->type->m_Children.size())
	{
		m_TypeTreeHasChanged = true;
	}
	#endif
	
	m_StackInfo.pop();
	if (!m_StackInfo.empty())
	{
		m_CurrentStackInfo = &m_StackInfo.top();
	}
	else
		m_CurrentStackInfo = NULL;

	m_DidReadLastProperty = true;
}

bool SafeBinaryRead::BeginArrayTransfer (const char* name, const char* typeString, SInt32& size)
{
	if (BeginTransfer (name, typeString, NULL) == kNotFound)
		return false;
		
	Transfer (size, "size");
	ArrayPositionInfo info;
	info.arrayPosition = 0;
	info.cachedBytePosition = -1;
	info.cachedArrayPosition = std::numeric_limits<SInt32>::max();
	m_PositionInArray.push (info);
	m_CurrentPositionInArray = &m_PositionInArray.top().arrayPosition;

	Assert (GetActiveOldTypeTree ().m_Children.front ().m_Name == "size");
	
	return true;
}

void SafeBinaryRead::EndArrayTransfer ()
{
	m_PositionInArray.pop ();
	if (!m_PositionInArray.empty())
		m_CurrentPositionInArray = &m_PositionInArray.top().arrayPosition;
	else
		m_CurrentPositionInArray = NULL;

	#if UNITY_EDITOR
	m_TypeTreeHasChanged = true;
	#endif
	
	EndTransfer ();
}

bool SafeBinaryRead::IsCurrentVersion ()
{
	return m_CurrentStackInfo->version == m_CurrentStackInfo->type->m_Version;
}

bool SafeBinaryRead::IsOldVersion (int version)
{
	return m_CurrentStackInfo->type->m_Version == version;
}

bool SafeBinaryRead::IsVersionSmallerOrEqual (int version)
{
	return m_CurrentStackInfo->type->m_Version <= version;
}

void SafeBinaryRead::TransferTypeless (unsigned* byteSize, const char* name, TransferMetaFlags metaflag)
{
	SInt32 size;
	if (!BeginArrayTransfer (name, "TypelessData", size))
	{
		*byteSize = 0;
		return;
	}
	// We can only transfer the array if the size was transferred as well 
	AssertIf (GetActiveOldTypeTree ().m_Children.front ().m_Name != "size");

	*byteSize = size;
	
	EndArrayTransfer ();
}

void SafeBinaryRead::TransferTypelessData (unsigned byteSize, void* copyData, int metaData)
{
	if (copyData == NULL || byteSize == 0) return;

	m_Cache.Read (copyData, byteSize);
}

#endif // SUPPORT_SERIALIZED_TYPETREES
