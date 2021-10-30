#include "UnityPrefix.h"
#include "ProxyTransfer.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"

using namespace std;

ProxyTransfer::ProxyTransfer (TypeTree& t, int options, void* objectPtr, int objectSize)
	: m_TypeTree (t),
	  m_ActiveFather (NULL)
{
	m_Flags = options;
	
	m_ObjectPtr = (char*)objectPtr;
	m_ObjectSize = objectSize;
	m_Index = 0;
	m_SimulatedByteOffset = 0;
	m_RequireTypelessData = false;
	m_DidErrorAlignment = false;
}

void ProxyTransfer::BeginArrayTransfer (const char* name, const char* typeString, SInt32& size, TransferMetaFlags metaFlag)
{
	AssertIf (m_ActiveFather->m_IsArray);
	BeginTransfer (name, typeString, NULL, metaFlag);
	m_ActiveFather->m_IsArray = true;
	
	// transfer size
	Transfer (size, "size");
}

void ProxyTransfer::BeginTransfer (const char* name, const char* typeString, char* dataPtr, TransferMetaFlags metaFlag)
{
	#if !UNITY_RELEASE
	if (m_RequireTypelessData)
	{
		AssertString ("TransferTypeless needs to be followed by TransferTypelessData with no other variables in between!");
	}
	#endif

	#if DEBUGMODE
	// skip this check for debug mode inspector, as we can have interface names from C# in the debug data.
	if (!(metaFlag & kDebugPropertyMask) && (strstr (name, ".") != NULL || strstr (name, "Array[") != NULL))
	{
		string s = "Illegal serialize property name :";
		GetTypePath (m_ActiveFather, s);
		s += name;
		s += "\n The name may not contain '.' or Array[";
		ErrorString (s);
	}
	#endif
	
	TypeTree* typeTree;
	// Setup a normal typetree child
	if (m_ActiveFather != NULL)
	{
		// Check for multiple occurences of same name
		#if DEBUGMODE
		TypeTree::TypeTreeList::iterator i;
		for (i=m_ActiveFather->m_Children.begin ();i != m_ActiveFather->m_Children.end ();++i)
		{
			if (i->m_Name == name)
			{
				string s = "The same field name is serialized multiple names in the class or it's parent class. This is not supported: ";
				GetTypePath (m_ActiveFather, s);
				s += name;
				ErrorString (s);
			}
		}
		#endif

		m_ActiveFather->m_Children.push_back (TypeTree ());
		// Setup new type
		typeTree = &m_ActiveFather->m_Children.back ();
		typeTree->m_Father = m_ActiveFather;
		typeTree->m_Type = typeString;
		typeTree->m_Name = name;
		typeTree->m_MetaFlag = metaFlag | m_ActiveFather->m_MetaFlag;
		AssertIf(typeTree->m_MetaFlag & kAlignBytesFlag);
		typeTree->m_MetaFlag &= ~(kAnyChildUsesAlignBytesFlag);
		typeTree->m_ByteSize = 0;
	}
	// Setup root TypeTree
	else
	{
		m_TypeTree.m_Father = NULL;
		m_TypeTree.m_Type = typeString;
		m_TypeTree.m_Name = name;
		m_TypeTree.m_MetaFlag = metaFlag;
		m_TypeTree.m_ByteSize = 0;
		typeTree = &m_TypeTree;
	}
	
	// Calculate typetree index	
	if ((typeTree->m_MetaFlag & kDebugPropertyMask) == 0)
		typeTree->m_Index = m_Index++;
	else
	{
		if (m_Flags & kIgnoreDebugPropertiesForIndex)
			typeTree->m_Index = -1;
		else
			typeTree->m_Index = m_Index++;
	}
	
	m_ActiveFather = typeTree;
		
	int offset = dataPtr - m_ObjectPtr;
	if (m_ObjectPtr && dataPtr && offset >= 0 && offset < m_ObjectSize)
		m_ActiveFather->m_ByteOffset = offset;

	m_ActiveFather->m_DirectPtr = dataPtr;
}

void ProxyTransfer::AssertContainsNoPPtr (const TypeTree* typeTree)
{
	AssertIf(typeTree->m_Type.find("PPtr<") == 0);
	for (TypeTree::const_iterator i=typeTree->begin();i != typeTree->end();i++)
		AssertContainsNoPPtr(&*i);
}

void ProxyTransfer::AssertOptimizeTransfer (int sizeofSize)
{
	if (m_ActiveFather->IsBasicDataType())
	{
		AssertIf(sizeofSize != m_ActiveFather->m_ByteSize);
		return;
	}

	int size = 0;
	int baseOffset = m_ActiveFather->m_ByteOffset;
	for (TypeTree::const_iterator i=m_ActiveFather->begin();i != m_ActiveFather->end();i++)
	{
		AssertOptimizeTransferImpl(*i, baseOffset, &size);
	}
	
	// Assert if serialized size is different from sizeof size.
	// - Ignore when serializing for game release. We might be serializing differently in that case. (AnimationCurves)
	AssertIf(sizeofSize != size && (m_Flags & kSerializeGameRelease) == 0);
}

void ProxyTransfer::AssertOptimizeTransferImpl (const TypeTree& typetree, int baseOffset, int* totalSize)
{
	if (typetree.m_ByteOffset != -1)
		AssertIf (typetree.m_ByteOffset - baseOffset != *totalSize);
	
	AssertIf (typetree.m_MetaFlag & kAlignBytesFlag);

	if (typetree.IsBasicDataType())
	{
		*totalSize += typetree.m_ByteSize;
		return;
	}

	for (TypeTree::const_iterator i=typetree.begin();i != typetree.end();i++)
		AssertOptimizeTransferImpl(*i, baseOffset, totalSize);
}

void ProxyTransfer::EndTransfer ()
{
	TypeTree* current = m_ActiveFather;
	// Add bytesize to parent!
	m_ActiveFather = m_ActiveFather->m_Father;
	if (m_ActiveFather)
	{
		if (current->m_ByteSize != -1 && m_ActiveFather->m_ByteSize != -1)
			m_ActiveFather->m_ByteSize += current->m_ByteSize;
		else
			m_ActiveFather->m_ByteSize = -1;

		// Propagate if any child uses alignment up to parents
		if (current->m_MetaFlag & kAnyChildUsesAlignBytesFlag)
		{
			m_ActiveFather->m_MetaFlag |= kAnyChildUsesAlignBytesFlag;
		}

		DebugAssertIf (m_ActiveFather->m_ByteSize == 0 && m_ActiveFather->m_Type == "Generic Mono");
	}
}

void ProxyTransfer::EndArrayTransfer ()
{
	m_ActiveFather->m_ByteSize = -1;
	EndTransfer ();
}

void ProxyTransfer::SetVersion (int version)
{
	// You can not set the version twice on the same type.
	// Probably an inherited class already calls SetVersion
	AssertIf (m_ActiveFather->m_Version != 1);

	m_ActiveFather->m_Version = version;
}

void ProxyTransfer::TransferTypeless (unsigned* byteSize, const char* name, TransferMetaFlags metaFlag)
{
	SInt32 size;
	BeginArrayTransfer (name, "TypelessData", size, metaFlag);
	
	UInt8 temp;
	Transfer (temp, "data", metaFlag);
	
	m_RequireTypelessData = true;
	
	EndArrayTransfer ();
	
	Align ();
}

void ProxyTransfer::TransferTypelessData (unsigned byteSize, void* copyData, int metaData)
{
	m_RequireTypelessData = false;
}

void ProxyTransfer::Align ()
{
	m_SimulatedByteOffset = Align4(m_SimulatedByteOffset);

	if (m_ActiveFather && !m_ActiveFather->m_Children.empty())
	{
		m_ActiveFather->m_Children.back().m_MetaFlag |= kAlignBytesFlag;
		m_ActiveFather->m_MetaFlag |= kAnyChildUsesAlignBytesFlag;
	}
	else
	{
		AssertString("Trying to align type data before anything has been serialized!");
	}
}

#if UNITY_EDITOR
void ProxyTransfer::LogUnalignedTransfer ()
{
	if (m_DidErrorAlignment)
		return;

	// For now we only support 4 byte alignment
	int size = m_ActiveFather->m_ByteSize;
	if (size == 8)
		size = 4;
	if (m_SimulatedByteOffset % size == 0)
		return;
	
	m_DidErrorAlignment = true;
		
	string path;
	GetTypePath(m_ActiveFather, path);
	LogString(Format("Unaligned transfer in '%s' at variable '%s'.\nNext unaligned data path: %s", m_TypeTree.m_Type.c_str(), m_ActiveFather->m_Name.c_str(), path.c_str()));
}
#endif
