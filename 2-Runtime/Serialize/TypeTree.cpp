#include "UnityPrefix.h"
#include "TypeTree.h"
#include <iostream>
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/Word.h"
#include "SwapEndianBytes.h"
#include "SerializationMetaFlags.h"
#include "SerializeTraits.h"
#include "Configuration/UnityConfigure.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#endif

#if ENABLE_SECURITY
#define TEST_LEN(x) if (iterator + sizeof(x) > end) \
{\
	return false; \
}
#else
#define TEST_LEN(x)
#endif

using namespace std;

static void RecalculateTypeTreeByteSize (TypeTree& typetree, int options);
static void RecalculateTypeTreeByteSize (TypeTree& typetree, int* typePosition, int options);


static void DeprecatedConvertUnity43BetaIntegerTypeNames (TypeTreeString& type)
{
	const char* rawText = type.c_str ();
	if (rawText[0] != 'S' && rawText[0] != 'U')
		return;

	if (strcmp (rawText, "SInt32") == 0)
		type = SerializeTraits<SInt32>::GetTypeString ();
	else if (strcmp (rawText, "UInt32") == 0)
		type = SerializeTraits<UInt32>::GetTypeString ();
}


TypeTree::TypeTree ()
{
	m_Father = NULL;
	m_Index = -1;
	m_ByteOffset = -1;
	m_IsArray = false;
	m_Version = 1;
	m_MetaFlag = kNoTransferFlags;
	m_DirectPtr = NULL;
	m_ByteSize = -1;
}

TypeTree::TypeTree (const std::string& name, const std::string& type, SInt32 size)
{
	m_Father = NULL;
	m_Index = -1;
	m_ByteOffset = -1;
	m_IsArray = false;
	m_Version = 1;
	m_MetaFlag = kNoTransferFlags;
	m_DirectPtr = NULL;

	m_Type = type;
	m_Name = name;
	m_ByteSize = size;
}

void TypeTree::DebugPrint (string& buffer, int level) const
{
	int i;
	for (i=0;i<level;i++)
		buffer += "\t";
	buffer += m_Name.c_str();
	buffer += " Type:";
	buffer += m_Type.c_str();
	buffer += " ByteSize:" + IntToString (m_ByteSize);
	buffer += " MetaFlag:" + IntToString (m_MetaFlag);
	if (m_IsArray)
		buffer += " IsArray";
	buffer += "\n";
	
	TypeTree::TypeTreeList::const_iterator iter;
	for (iter = m_Children.begin ();iter != m_Children.end ();++iter)
		iter->DebugPrint (buffer, level + 1);
}

void GetTypePath (const TypeTree* type, std::string& s)
{
	if (type != NULL)
	{
		if (type->m_Father != NULL)
		{	
			s += " -> ";
			GetTypePath (type->m_Father, s);
		}
		s += type->m_Name.c_str();
		s += '(';
		s += type->m_Type.c_str();
		s += ") ";
	}
}

void TypeTree::operator = (const TypeTree& typeTree)
{
	m_Children = typeTree.m_Children;
	for (iterator i = begin ();i != end ();i++)
		i->m_Father = this;
	m_Type = typeTree.m_Type;
	m_Name = typeTree.m_Name;
	m_ByteSize = typeTree.m_ByteSize;
	m_Index = typeTree.m_Index;
	m_IsArray = typeTree.m_IsArray;
	m_Version = typeTree.m_Version;
	m_MetaFlag = typeTree.m_MetaFlag;
}
/*
bool TypeTree::operator == (const TypeTree& t) const
{
	if (m_ByteSize != t.m_ByteSize || m_Name != t.m_Name || m_Type != t.m_Type || m_Version != t.m_Version)
		return false;
	return m_Children == t.m_Children;
}
*/

bool IsStreamedBinaryCompatbile (const TypeTree& lhs, const TypeTree& rhs)
{
	if (lhs.m_ByteSize != rhs.m_ByteSize || lhs.m_Name != rhs.m_Name || lhs.m_Type != rhs.m_Type || lhs.m_Version != rhs.m_Version)
		return false;
	if ((lhs.m_MetaFlag & kAlignBytesFlag) != (rhs.m_MetaFlag & kAlignBytesFlag))
		return false;
	
	if (lhs.m_Children.size() != rhs.m_Children.size())
		return false;

	TypeTree::const_iterator i, k;
	for (i=lhs.begin(), k=rhs.begin();i != lhs.end();i++,k++)
	{
		if (!IsStreamedBinaryCompatbile(*i, *k))
			return false;
	}
	
	return true;
}

bool IsStreamedBinaryCompatbileAndIndices (const TypeTree& lhs, const TypeTree& rhs)
{
	if (lhs.m_ByteSize != rhs.m_ByteSize || lhs.m_Name != rhs.m_Name || lhs.m_Type != rhs.m_Type || lhs.m_Version != rhs.m_Version || lhs.m_Index != rhs.m_Index)
		return false;
	if ((lhs.m_MetaFlag & kAlignBytesFlag) != (rhs.m_MetaFlag & kAlignBytesFlag))
		return false;
	
	if (lhs.m_Children.size() != rhs.m_Children.size())
		return false;

	TypeTree::const_iterator i, k;
	for (i=lhs.begin(), k=rhs.begin();i != lhs.end();i++,k++)
	{
		if (!IsStreamedBinaryCompatbileAndIndices(*i, *k))
			return false;
	}
	
	return true;
}

void AppendTypeTree (TypeTree& typeTree, TypeTree::iterator begin, TypeTree::iterator end)
{
	Assert(typeTree.m_Father == NULL);
	typeTree.m_Children.insert (typeTree.end (), begin, end);
	RecalculateTypeTreeByteSize( typeTree, 0);
}

void AppendTypeTree (TypeTree& typeTree, const TypeTree& typeTreeToAdd)
{
	Assert(typeTree.m_Father == NULL);
	typeTree.m_Children.push_back(typeTreeToAdd);
	RecalculateTypeTreeByteSize( typeTree, 0);
}

void RemoveFromTypeTree (TypeTree& typeTree, TypeTree::iterator begin, TypeTree::iterator end)
{
	Assert(typeTree.m_Father == NULL);
	typeTree.m_Children.erase (begin, end);
	RecalculateTypeTreeByteSize( typeTree, 0);
}

static void RecalculateTypeTreeByteSize (TypeTree& typetree, int* typePosition, int options)
{
	DebugAssertIf (typetree.m_ByteSize == 0 && typetree.m_Type == "Generic Mono");

	if ((typetree.m_MetaFlag & kDebugPropertyMask) == 0)
	{
		typetree.m_Index = *typePosition;
		(*typePosition)++;
	}
	else
	{
		if (options & kIgnoreDebugPropertiesForIndex)
			typetree.m_Index = -1;
		else
		{
			typetree.m_Index = *typePosition;
			(*typePosition)++;
		}
	}
	
	if (typetree.m_Children.empty ())
	{
		AssertIf (typetree.m_IsArray);
		return;
	}

	bool cantDetermineSize = false;
	typetree.m_ByteSize = 0;

	TypeTree::TypeTreeList::iterator i;
	for (i = typetree.m_Children.begin ();i != typetree.m_Children.end ();++i)
	{
		RecalculateTypeTreeByteSize (*i, typePosition, options);
		if (i->m_ByteSize == -1)
			cantDetermineSize = true;
		
		if (!cantDetermineSize)
			typetree.m_ByteSize += i->m_ByteSize;
		
		i->m_Father = &typetree;
	}
	
	if (typetree.m_IsArray || cantDetermineSize)
		typetree.m_ByteSize = -1;

	DebugAssertIf (typetree.m_ByteSize == 0 && typetree.m_Type == "Generic Mono");
}

static void RecalculateTypeTreeByteSize (TypeTree& typetree, int options)
{
//	AssertIf (typetree.m_Father != NULL);
//	AssertIf (typetree.m_Name != "Base");
	typetree.m_ByteSize = -1;
	int typePosition = 0;
	RecalculateTypeTreeByteSize (typetree, &typePosition, options);
}

/*
void GetTypePath (const TypeTree* type, string& s)
{
	if (type != NULL)
	{
		GetTypePath (type->m_Father, s);
		s += "::";
		s += type->m_Name;
		s += '(';
		s += type->m_Type;
		s += ") ";
	}
}*/



Type::Type ()
	: m_OldType (NULL),
	  m_NewType (NULL),
	  m_Equals (false)
{}

Type::~Type ()
{
	UNITY_DELETE(m_OldType, kMemTypeTree);
	UNITY_DELETE(m_NewType, kMemTypeTree);
}

void Type::SetOldType (TypeTree* t)
{
	UNITY_DELETE(m_OldType, kMemTypeTree);
	m_OldType = t;
	m_Equals = m_OldType != NULL && m_NewType != NULL && IsStreamedBinaryCompatbile(*m_OldType, *m_NewType);
}

void Type::SetNewType (TypeTree* t)
{
	UNITY_DELETE (m_NewType, kMemTypeTree);
	m_NewType = t;
	m_Equals = m_OldType != NULL && m_NewType != NULL && IsStreamedBinaryCompatbile(*m_OldType, *m_NewType);
}

const TypeTree& GetElementTypeFromContainer (const TypeTree& typeTree)
{
	AssertIf (typeTree.m_Children.size () != 1);
	AssertIf (typeTree.m_Children.back ().m_Children.size () != 2);
	return typeTree.m_Children.back ().m_Children.back ();
}

SInt32 GetContainerArraySize (const TypeTree& typeTree, void* data)
{
	AssertIf (data == NULL);
	AssertIf (typeTree.m_Children.size () != 1);
	AssertIf (typeTree.m_Children.back ().m_Children.size () != 2);
	AssertIf (!typeTree.m_Children.back ().m_IsArray);
	SInt32* casted = reinterpret_cast<SInt32*> (data);
	return *casted;
}

template<bool kSwap>
bool ReadTypeTreeImpl (TypeTree& t, UInt8 const*& iterator, UInt8 const* end, int version)
{
	// Read Type
	if (!ReadString (t.m_Type, iterator, end))
		return false;

	// During the 4.3 beta cycle, we had a couple of versions that were using "SInt32" and "UInt32"
	// instead of "int" and "unsigned int".  Get rid of these type names here.
	//
	// NOTE: For now, we always do this.  Ideally, we only want this for old data.  However, ATM we
	//	don't version TypeTrees independently (they are tied to kCurrentSerializeVersion).  TypeTree
	//  serialization is going to change soonish so we wait with bumping the version until then.
	DeprecatedConvertUnity43BetaIntegerTypeNames (t.m_Type);

	// Read Name
	if (!ReadString (t.m_Name, iterator, end))
		return false;

	// Read bytesize
	TEST_LEN(t.m_ByteSize);
	ReadHeaderCache<kSwap> (t.m_ByteSize, iterator);

	// Read variable count
	if (version == 2)
	{
		SInt32 variableCount;
		TEST_LEN(variableCount);
		ReadHeaderCache<kSwap> (variableCount, iterator);
	}

	// Read Typetree position
	if (version != 3)
	{
		TEST_LEN(t.m_Index);
		ReadHeaderCache<kSwap> (t.m_Index, iterator);
	}
	
	// Read IsArray
	TEST_LEN(t.m_IsArray);
	ReadHeaderCache<kSwap> (t.m_IsArray, iterator);

	// Read version
	TEST_LEN(t.m_Version);
	ReadHeaderCache<kSwap> (t.m_Version, iterator);
	
	// Read metaflag
	if (version != 3)
	{
		TEST_LEN(t.m_MetaFlag);
		ReadHeaderCache<kSwap> ((UInt32&)t.m_MetaFlag, iterator);
	}
	
	// Read Children count
	SInt32 childrenCount;
	TEST_LEN(childrenCount);
	ReadHeaderCache<kSwap> (childrenCount, iterator);
	
	enum { kMaxDepth = 50, kMaxChildrenCount = 5000 };
	static int depth = 0;
	depth++;
	if (depth > kMaxDepth || childrenCount < 0 || childrenCount > kMaxChildrenCount)
	{
		depth--;
		ErrorString ("Fatal error while reading file. Header is invalid!");
		return false;
	}
	// Read children 
	for (int i=0;i<childrenCount;i++)
	{
		TypeTree newType;
		t.m_Children.push_back (newType);
		if (!ReadTypeTree (t.m_Children.back (), iterator, end, version, kSwap))
		{
			depth--;
			return false;
		}
		t.m_Children.back ().m_Father = &t;
	}
	depth--;
	return true;
}

template<bool kSwap>
void WriteTypeTreeImpl (TypeTree& t, SerializationCache& cache)
{
	// Write type
	WriteString (t.m_Type, cache);

	// Write name
	WriteString (t.m_Name, cache);

	// Write bytesize
	WriteHeaderCache<kSwap> (t.m_ByteSize, cache);

	// Write typetree position
	WriteHeaderCache<kSwap> (t.m_Index, cache);

	// Write IsArray
	WriteHeaderCache<kSwap> (t.m_IsArray, cache);

	// Write version
	WriteHeaderCache<kSwap> (t.m_Version, cache);

	// Write metaflag
	WriteHeaderCache<kSwap> ((UInt32)t.m_MetaFlag, cache);
	
	// Write Children Count
	SInt32 childrenCount = t.m_Children.size ();
	WriteHeaderCache<kSwap> (childrenCount, cache);
	
	// Write children 
	TypeTree::TypeTreeList::iterator i;
	for (i = t.m_Children.begin ();i != t.m_Children.end ();i++)
		WriteTypeTreeImpl<kSwap> (*i, cache);
}

void WriteTypeTree (TypeTree& t, SerializationCache& cache, bool swapEndian)
{
	if (swapEndian)
		WriteTypeTreeImpl<true> (t, cache);
	else
		WriteTypeTreeImpl<false> (t, cache);
}

bool ReadTypeTree (TypeTree& t, UInt8 const*& iterator, UInt8 const* end, int version, bool swapEndian)
{
	if (swapEndian)
		return ReadTypeTreeImpl<true> (t, iterator, end, version);
	else
		return ReadTypeTreeImpl<false> (t, iterator, end, version);
}

bool ReadVersionedTypeTreeFromVector (TypeTree* typeTree, UInt8 const*& iterator, UInt8 const* end, bool swapEndian)
{
	if (iterator == end)
	{
		*typeTree = TypeTree ();
		return false;
	}
	SInt32 version;
	
	if (swapEndian)
	{
		ReadHeaderCache<true> (version, iterator);
		ReadTypeTreeImpl<true> (*typeTree, iterator, end, version);
	}
	else
	{
		ReadHeaderCache<false> (version, iterator);
		ReadTypeTreeImpl<false> (*typeTree, iterator, end, version);
	}
	return true;
}

void WriteString (UnityStr const& s, SerializationCache& cache)
{
	int size = cache.size ();
	cache.resize (size + s.size () + 1);
	memcpy (&cache[size], s.data (), s.size ());
	cache.back () = '\0';
}

void WriteString (TypeTreeString const& s, SerializationCache& cache)
{
	int size = cache.size ();
	cache.resize (size + s.size () + 1);
	memcpy (&cache[size], s.data (), s.size ());
	cache.back () = '\0';
}


bool ReadString (TypeTreeString& s, UInt8 const*& iterator, UInt8 const* end)
{
	UInt8 const* base = iterator;
	while (*iterator != 0 && iterator != end)
		iterator++;
	
#if ENABLE_SECURITY
	if (iterator == end)
		return false;
#endif
	
	s.assign(base, iterator);
	iterator++;
	return true;
}

bool ReadString (UnityStr& s, UInt8 const*& iterator, UInt8 const* end)
{
	UInt8 const* base = iterator;
	while (*iterator != 0 && iterator != end)
		iterator++;
	
#if ENABLE_SECURITY
	if (iterator == end)
		return false;
#endif
	
	s.assign(base, iterator);
	iterator++;
	return true;
}


#if UNITY_EDITOR
void HashTypeTree (MdFourGenerator& gen, TypeTree& typeTree)
{
	gen.Feed (typeTree.m_Type);
	gen.Feed (typeTree.m_Name);
	gen.Feed (typeTree.m_ByteSize);
	gen.Feed (typeTree.m_IsArray);
	gen.Feed (typeTree.m_Version);

	for (TypeTree::iterator it = typeTree.begin (), end = typeTree.end (); it != end; ++it)
	{
		HashTypeTree (gen, *it);
	}
}

UInt32 HashTypeTree (TypeTree& typeTree)
{
	MdFourGenerator gen;
	HashTypeTree (gen, typeTree);
	MdFour hash = gen.Finish ();
	return hash.PackToUInt32 ();
}
#endif
