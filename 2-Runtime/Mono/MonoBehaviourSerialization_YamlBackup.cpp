#include "UnityPrefix.h"

#if ENABLE_SCRIPTING
#include "MonoBehaviour.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "MonoScript.h"
#include "MonoTypeSignatures.h"
#include "MonoManager.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Scripting/ScriptingUtility.h"

using namespace std;

#if SUPPORT_TEXT_SERIALIZATION
struct ArrayAsString
{
	char const* name;
	bool serializeAsString;
};

ArrayAsString g_ArrayAsString[] =
{
#define YAML_DECLARE_WRITE_ARRAY_AS_STRING(x) \
{ #x, YAMLSerializeTraits<x>::ShouldSerializeArrayAsCompactString () }
	
	YAML_DECLARE_WRITE_ARRAY_AS_STRING(int),
	YAML_DECLARE_WRITE_ARRAY_AS_STRING(char),
	YAML_DECLARE_WRITE_ARRAY_AS_STRING(SInt32),
};

static const size_t g_ArrayAsStringCount = sizeof (g_ArrayAsString) / sizeof (g_ArrayAsString[0]);

bool ConvertArrayAsString (std::string const& typeName)
{
	for (int i=0; i<g_ArrayAsStringCount; ++i)
	{
		if (g_ArrayAsString[i].name == typeName)
			return g_ArrayAsString[i].serializeAsString;
	}
	
	return false;
}

struct YAMLConverterContext
{
	dynamic_array<UInt8>& m_Data;
	YAMLWrite& m_Writer;
	
	template<class T>
	T* ExtractAndAdvance (int* bytePosition)
	{
		T* ptr = reinterpret_cast<T*> (&m_Data[*bytePosition]);
		*bytePosition += sizeof (T);
		return ptr;
	}
	
	YAMLConverterContext (dynamic_array<UInt8>& data, YAMLWrite& writer)
	:   m_Data (data), m_Writer (writer)
	{}
	
	void EmitBasicData (TypeTree const& typeTree, int* bytePosition)
	{
#define HANDLE_COMPLEX_TYPE(T) \
if (typeTree.m_Type == #T) { \
T* data = ExtractAndAdvance<T> (bytePosition); \
m_Writer.Transfer (*data, typeTree.m_Name.c_str()); \
} else
		
#define HANDLE_BASIC_TYPE(T) \
if (typeTree.m_Type == #T) { \
T data = *reinterpret_cast<T*> (&m_Data[*bytePosition]); \
m_Writer.Transfer (data, typeTree.m_Name.c_str()); \
} else
		
		HANDLE_BASIC_TYPE (int)
		HANDLE_BASIC_TYPE (SInt32)
		HANDLE_BASIC_TYPE (bool)
		HANDLE_BASIC_TYPE (float)
		HANDLE_BASIC_TYPE (double)
		HANDLE_BASIC_TYPE (UInt8)
		{
			ErrorStringMsg ("Binary to YAML conversion: type %s is unsupported\n", typeTree.m_Type.c_str());
		}
	}
	
	void ConvertDataToYAML (TypeTree const& typeTree)
	{
		int bytePosition = 0;
		
		for (TypeTree::TypeTreeList::const_iterator i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
		{
			ConvertDataToYAML (*i, &bytePosition);
		}
	}
	
	void ConvertDataToYAML (TypeTree const& typeTree, int* bytePosition)
	{
		if (typeTree.IsBasicDataType())
		{
			// TypeTrees can contain $initialized_XXX$ member for Mono structs. We don't want to appear in YAML
			bool skip = !typeTree.m_Name.empty () && typeTree.m_Name[0] == '$' && typeTree.m_Name[typeTree.m_Name.size ()-1] == '$';
			if (!skip)
				EmitBasicData (typeTree, bytePosition);
		}
		else if (IsTypeTreePPtr (typeTree))
		{
			m_Writer.PushMetaFlag (kTransferUsingFlowMappingStyle);
			
			m_Writer.BeginMetaGroup (typeTree.m_Name);
			SInt32 instanceID = *reinterpret_cast<SInt32*> (&m_Data[*bytePosition]);
			m_Writer.Transfer (instanceID, "instanceID");
			*bytePosition += typeTree.m_ByteSize;
			
			m_Writer.EndMetaGroup ();
			m_Writer.PopMetaFlag ();
		}
		else if (typeTree.m_IsArray)
		{
			SInt32 arraySize = *ExtractAndAdvance<SInt32> (bytePosition);
			Assert (typeTree.m_Children.size () == 2);
			TypeTree::const_iterator sizeIt = typeTree.begin ();
			TypeTree::const_iterator dataIt = sizeIt; ++dataIt;
			TypeTree const& elementTypeTree = *dataIt;
			
			if (elementTypeTree.IsBasicDataType () && ConvertArrayAsString (elementTypeTree.m_Type))
			{
				std::string str;
				size_t elementSize = elementTypeTree.m_ByteSize;
				size_t numBytes = arraySize * elementSize;
				str.resize (numBytes*2);
				
				for (size_t i=0; i<arraySize; ++i, *bytePosition += elementSize)
				{
					void* dataPtr = &m_Data[*bytePosition];
					BytesToHexString (dataPtr, elementSize, &str[i*2*elementSize]);
				}
				
				m_Writer.TransferStringData (str);
			}
			else
			{
				m_Writer.StartSequence ();
				
				for (size_t i = 0; i<arraySize; ++i)
					ConvertDataToYAML (elementTypeTree, bytePosition);
			}
		}
		else
		{
			HANDLE_COMPLEX_TYPE (Vector2f)
			HANDLE_COMPLEX_TYPE (Vector3f)
			HANDLE_COMPLEX_TYPE (Vector4f)
			HANDLE_COMPLEX_TYPE (Quaternionf)
			HANDLE_COMPLEX_TYPE (Matrix4x4f)
			{
				m_Writer.BeginMetaGroup (typeTree.m_Name);
				
				for (TypeTree::TypeTreeList::const_iterator i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
					ConvertDataToYAML (*i, bytePosition);
				
				m_Writer.EndMetaGroup ();
			}
		}
		
		if (typeTree.IsBasicDataType ())
			*bytePosition += typeTree.m_ByteSize;
		
		if (typeTree.m_MetaFlag & kAlignBytesFlag)
			*bytePosition = Align4_Iterate (*bytePosition);
	}
};

YAMLNode* ConvertBackupToYAML (BackupState& binary)
{
	YAMLWrite writer (0);
	YAMLConverterContext ctx(binary.state, writer);
	
	ctx.ConvertDataToYAML (binary.typeTree);
	
	yaml_document_t* yaml = writer.GetDocument();
	YAMLNode* node = YAMLDocNodeToNode (yaml, yaml_document_get_root_node(yaml));
	
#if 0
	if (node)
	{
		std::string str = node->EmitYAMLString();
		printf_console ("CONVERTED BINARY TO YAML:\n%s<< END\n", str.c_str());
	}
#endif
	
	return node;
}
#endif // SUPPORT_TEXT_SERIALIZATION

#endif // ENABLE_SCRIPTING
