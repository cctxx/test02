#include "UnityPrefix.h"
#include "DumpSerializedDataToText.h"
#include "TypeTree.h"
#include "CacheWrap.h"
#include "Runtime/Utilities/GUID.h"

using namespace std;

#if (UNITY_EDITOR || UNITY_INCLUDE_SERIALIZATION_DUMP || DEBUGMODE)

void DumpSerializedDataToText (const TypeTree& typeTree, dynamic_array<UInt8>& data)
{
	int offset = 0;
	RecursiveOutput(typeTree, data.begin(), &offset, 0, cout, kDumpNormal, 0, false, -1);
}
#define TAB for (int t=0;t<tab;t++) os << '\t';

template<class T>
void DoSwap (T& t, bool swapBytes)
{
	if (swapBytes)
		SwapEndianBytes(t);
}


SInt32 CalculateByteSize(const TypeTree& type) {
	if(type.m_ByteSize != -1) 
		return type.m_ByteSize;
	
	SInt32 r=0;
	for (TypeTree::const_iterator i=type.begin ();i != type.end ();i++)
		r+=CalculateByteSize(*i);
	return r;
}

void OutputType (const TypeTree& type, ostream& os)
{
	os <<  "Name: "               << type.m_Name;
	os << " Type: "               << type.m_Type;
	os << " ByteSize: "           << type.m_ByteSize;
	os << " TypeTreePosition: "   << type.m_Index;
	os << " IsArray: "            << type.m_IsArray;
	os << " Version: "            << type.m_Version;
	os << " MetaFlag: "           << type.m_MetaFlag;
	os << " IsArray: "            << type.m_IsArray;
}

string ExtractString (const TypeTree& type, const UInt8* data, int* offset, bool doSwap)
{
	string value;
	SInt32 size = *reinterpret_cast<const SInt32*> (data + *offset);
	DoSwap(size, doSwap);
	value.reserve (size);
	for (int i=0;i<size;i++)
	{
		value += data[*offset + i + sizeof(SInt32)];
	}
	
	*offset += sizeof(SInt32) + size;
	
	if (type.m_MetaFlag & (kAnyChildUsesAlignBytesFlag | kAlignBytesFlag))
		*offset = Align4(*offset);
 	
	return value;
}

string ExtractMdFour (const TypeTree& type, const UInt8* data, int* offset, bool doSwap)
{
	string value;
	SInt32 size = *reinterpret_cast<const SInt32*> (data + *offset);
	DoSwap(size, doSwap);
	value.reserve (size*2);
	for (int i=0;i<size;i++)
	{
		value += Format("%02x", data[*offset + i + sizeof(SInt32)]);
	}
	
	*offset += sizeof(SInt32) + size;
	
	if (type.m_MetaFlag & (kAnyChildUsesAlignBytesFlag | kAlignBytesFlag))
		*offset = Align4(*offset);
 	
	return value;
}

string ExtractVector (const TypeTree& type, const UInt8* data, int* offset, bool doSwap, int dimension)
{
	AssertIf(type.m_Father == NULL);
	AssertIf(type.m_Children.size() != dimension);
	AssertIf(CalculateByteSize(type) != dimension*4);
	
	string val = "(";
	for (int i = 0; i < dimension; ++i)
	{
		float v = *reinterpret_cast<const float*> (data + *offset);
		DoSwap(v, doSwap);
		if (i != 0)
			val += ' ';
		val += Format("%g", v);
		*offset += 4;
	}
	val += ')';
	
	if (type.m_MetaFlag & kAlignBytesFlag)
	{
		*offset = Align4(*offset);
	}
	
	return val;
}

string ExtractRectOffset (const TypeTree& type, const UInt8* data, int* offset, bool doSwap)
{
	AssertIf(type.m_Father == NULL);
	AssertIf(type.m_Children.size() != 4);
	
	string val = "(";
	TypeTree::TypeTreeList::const_iterator it = type.m_Children.begin();
	for (int i = 0; i < 4; ++i, ++it)
	{
		int v = *reinterpret_cast<const int*> (data + *offset);
		DoSwap(v, doSwap);
		if (i != 0)
			val += ' ';
		val += Format("%s %i", it->m_Name.c_str(), v);
		*offset += 4;
	}
	val += ')';
	
	if (type.m_MetaFlag & kAlignBytesFlag)
	{
		*offset = Align4(*offset);
	}
	
	return val;
}


string ExtractPPtr (const TypeTree& type, const UInt8* data, int* offset, bool doSwap)
{
	SInt32 fileID = *reinterpret_cast<const SInt32*>(data + *offset);
	SInt32 pathID = *reinterpret_cast<const SInt32*>(data + *offset + 4);
	DoSwap(fileID, doSwap);
	DoSwap(pathID, doSwap);
	
	if (type.m_MetaFlag & kAlignBytesFlag)
	{
		*offset = Align4(*offset);
	}
	
	*offset += 8;
	
	return Format ("(file %i path %i)", (int)fileID, (int)pathID);
}

string ExtractGUID (const TypeTree& type, const UInt8* data, int* offset, bool doSwap)
{
	AssertIf(type.m_Father == NULL);
	AssertIf(type.m_Children.size() != 4);
	AssertIf(CalculateByteSize(type) != 4*4);
	
	UnityGUID val;
	for (int i = 0; i < 4; ++i) {
		UInt32 v = *reinterpret_cast<const UInt32*> (data + *offset);
		val.data[i]=v;
		*offset += 4;
	}
	
	if (type.m_MetaFlag & kAlignBytesFlag)
	{
		*offset = Align4(*offset);
	}
	
	return GUIDToString(val);
}


void OutputValue (const TypeTree& type, const UInt8* data, int* offset, ostream& os, bool doSwap)
{
#define OUTPUT(x) \
else if (type.m_Type == #x) \
{ \
x value = *reinterpret_cast<const x*> (data + *offset); \
DoSwap(value, doSwap);\
os << value; \
}
	
#define OUTPUT_INT(x) \
else if (type.m_Type == #x) \
{ \
x value = *reinterpret_cast<const x*> (data + *offset); \
DoSwap(value, doSwap);\
int intValue = value; \
os << intValue; \
}
	
	
	if (false) { }
	OUTPUT (float)
	OUTPUT (double)
	OUTPUT (int)
	OUTPUT (unsigned int)
	OUTPUT (SInt32)
	OUTPUT (UInt32)
	OUTPUT (SInt16)
	OUTPUT (UInt16)
	OUTPUT (SInt64)
	OUTPUT (UInt64)
	OUTPUT_INT (SInt8)
	OUTPUT_INT (UInt8)
	OUTPUT (char)
	OUTPUT (bool)
	else
	{
		AssertString ("Unsupported type! " + type.m_Type);
	}
	*offset = *offset + type.m_ByteSize;
}

const int kArrayMemberColumns = 25;

void RecursiveOutput (const TypeTree& type, const UInt8* data, int* offset, int tab, ostream& os, DumpOutputMode mode, int pathID, bool doSwap, int arrayIndex)
{
	if (type.m_Type == "Vector3f" && type.m_ByteSize != 12)
	{
		AssertString ("Unsupported type! " + type.m_Type);
	}
	
	if (!type.m_IsArray && arrayIndex == -1 && mode != kDumpClean)
		os << Format("% 5d: ", (int)type.m_Index);
	
	if (type.IsBasicDataType ())
	{
		// basic data type
		if (arrayIndex == -1)
		{
			TAB os << type.m_Name  << " ";
			OutputValue (type, data, offset, os, doSwap);
			os << " (" << type.m_Type << ")";
			os << endl;
		}
		else
		{
			// array members: multiple members per line
			if (arrayIndex % kArrayMemberColumns == 0)
			{
				if (arrayIndex != 0)
					os << endl;
				TAB os << type.m_Name  << " (" << type.m_Type << ") #" << arrayIndex << ": ";
				OutputValue (type, data, offset, os, doSwap);
			}
			else
			{
				os << ' ';
				OutputValue (type, data, offset, os, doSwap);
			}
		}
	}
	else if (type.m_IsArray)
	{
		// Extract and Print size
		int size = *reinterpret_cast<const SInt32*> (data + *offset);
		DoSwap(size, doSwap);
		
		RecursiveOutput (type.m_Children.front (), data, offset, tab, os, mode, 0, doSwap, -1);
		// Print children
		for (int i=0;i<size;i++)
		{
			//			char buffy[64]; sprintf (buffy, "%s[%d]", type.m_Name.c_str (), i);
			RecursiveOutput (type.m_Children.back (), data, offset, tab, os, mode, 0, doSwap, i);
		}
		os << endl;
	}
	else if (type.m_Type == "string")
	{
		TAB os << type.m_Name  << " ";
		os << "\""<< ExtractString (type, data, offset, doSwap) << "\"";
		
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "Vector4f" && type.m_ByteSize == 16)
	{
		TAB os << type.m_Name  << " ";
		os << ExtractVector (type, data, offset, doSwap, 4);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "Vector3f" && type.m_ByteSize == 12)
	{
		TAB os << type.m_Name  << " ";
		os << ExtractVector (type, data, offset, doSwap, 3);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "Vector2f" && type.m_ByteSize == 8)
	{
		TAB os << type.m_Name  << " ";
		os << ExtractVector (type, data, offset, doSwap, 2);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "ColorRGBA" && type.m_ByteSize == 16)
	{
		TAB os << type.m_Name  << " ";
		os << ExtractVector (type, data, offset, doSwap, 4);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "FastPropertyName" && type.m_Children.size()==1 && type.m_Children.front().m_Type=="string")
	{
		TAB os << type.m_Name  << " ";
		os << "\""<< ExtractString (type, data, offset, doSwap) << "\"";
		os << " (" << type.m_Type << ")" << endl;
		if (type.m_MetaFlag & kAlignBytesFlag)
		{
			*offset = Align4(*offset);
		}
	}
	else if (type.m_Type == "RectOffset" && type.m_ByteSize == 16)
	{
		TAB os << type.m_Name  << " ";
		os << ExtractRectOffset (type, data, offset, doSwap);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (BeginsWith(type.m_Type, "PPtr<") && type.m_ByteSize == 8)
	{
		TAB os << type.m_Name  << " ";
		os << ExtractPPtr (type, data, offset, doSwap);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "GUID")
	{
		TAB os << type.m_Name  << " ";
		os << ExtractGUID (type, data, offset, doSwap);
		os << " (" << type.m_Type << ")" << endl;
	}
	else if (type.m_Type == "MdFour")
	{
		TAB os << type.m_Name  << " ";
		os << ExtractMdFour (type, data, offset, doSwap);
		os << " (" << type.m_Type << ")" << endl;
	}
	else
	{
		TAB
		if (type.m_Father != NULL)
		{
			os << type.m_Name  << " ";
			os << " (" << type.m_Type << ")" ;
		}
		else
		{
			os << type.m_Type ;
		}
		if (mode != kDumpClean)
		{
			if (pathID == 0)
				os << Format(" [size: %d, children: %d]", (int)CalculateByteSize(type), (int)type.m_Children.size());
			else
				os << Format(" [size: %d, children: %d pathID: %d]", (int)CalculateByteSize(type), (int)type.m_Children.size(), pathID);
		}
		os << endl;
		
		tab++;
		for (TypeTree::const_iterator i=type.begin ();i != type.end ();i++)
			RecursiveOutput (*i, data, offset, tab, os, mode, 0, doSwap, -1);
		tab--;
	}
	
	if (type.m_MetaFlag & kAlignBytesFlag)
	{
		*offset = Align4(*offset);
	}
}

#endif // UNITY_EDITOR || UNITY_INCLUDE_SERIALIZATION_DUMP
