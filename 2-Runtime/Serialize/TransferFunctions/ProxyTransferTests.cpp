#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Serialize/TransferFunctions/ProxyTransfer.h"

struct TypeTreeTestItem
{
public:
	DECLARE_SERIALIZE (TypeTreeTestItem);

	float m_float;
	double m_double;
	int m_int; // Same as SInt32 to serialization system.
	unsigned int m_unsignedint; // Same as UInt32 to serialization system.
	SInt64 m_SInt64;
	UInt64 m_UInt64;
	SInt32 m_SInt32;
	UInt32 m_UInt32;
	SInt16 m_SInt16;
	UInt16 m_UInt16;
	SInt8 m_SInt8;
	UInt8 m_UInt8;
	char m_char;
	bool m_bool;
	UnityStr m_UnityStr;
};

template<class TransferFunction>
void TypeTreeTestItem::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_float);
	TRANSFER (m_double);
	TRANSFER (m_int);
	TRANSFER (m_unsignedint);
	TRANSFER (m_SInt64);
	TRANSFER (m_UInt64);
	TRANSFER (m_SInt32);
	TRANSFER (m_UInt32);
	TRANSFER (m_SInt16);
	TRANSFER (m_UInt16);
	TRANSFER (m_SInt8);
	TRANSFER (m_UInt8);
	TRANSFER (m_char);
	TRANSFER (m_bool);
	TRANSFER (m_UnityStr);
}

SUITE (ProxyTransferTests)
{
	TEST (TypeTree_GenerateBaseTypes)
	{
		// Create test item.
		TypeTreeTestItem item;

		// Populate test item.
		item.m_float = 123.0f;
		item.m_double = 456.0;
		item.m_int = 2^31-1;
		item.m_unsignedint = 2^32-1;
		item.m_SInt64 = 2^63-1;
		item.m_UInt64 = 2^64-1;
		item.m_SInt32 = 2^31-1;
		item.m_UInt32 = 2^32-1;
		item.m_SInt16 = 2^15-1;
		item.m_UInt16 = 2^16-1;
		item.m_SInt8 = 2^7-1;
		item.m_UInt8 = 2^8-1;
		item.m_char = '@';
		item.m_bool = true;
		item.m_UnityStr = "ProxyTransferTests";		 

		// Generate type tree.
		TypeTree tree;
		ProxyTransfer proxy (tree, 0, &item, sizeof(TypeTreeTestItem));
		proxy.Transfer( item, "Base" );

		// Validate the following expected output:
		//
		// TypeTree: Base Type:TypeTreeTestItem ByteSize:-1 MetaFlag:32768
		//		m_float Type:float ByteSize:4 MetaFlag:0
		//  	m_double Type:double ByteSize:8 MetaFlag:0
		//  	m_int Type:int ByteSize:4 MetaFlag:0
		//  	m_unsignedint Type:unsigned int ByteSize:4 MetaFlag:0
		//  	m_SInt64 Type:SInt64 ByteSize:8 MetaFlag:0
		//  	m_UInt64 Type:UInt64 ByteSize:8 MetaFlag:0
		//  	m_SInt32 Type:SInt32 ByteSize:4 MetaFlag:0
		//  	m_UInt32 Type:UInt32 ByteSize:4 MetaFlag:0
		//  	m_SInt16 Type:SInt16 ByteSize:2 MetaFlag:0
		//  	m_UInt16 Type:UInt16 ByteSize:2 MetaFlag:0
		//  	m_SInt8 Type:SInt8 ByteSize:1 MetaFlag:0
		//  	m_UInt8 Type:UInt8 ByteSize:1 MetaFlag:0
		//  	m_char Type:char ByteSize:1 MetaFlag:0
		//  	m_bool Type:bool ByteSize:1 MetaFlag:0
		//  	m_UnityStr Type:string ByteSize:-1 MetaFlag:32768
		//  		Array Type:Array ByteSize:-1 MetaFlag:16385 IsArray
		//  			size Type:SInt32 ByteSize:4 MetaFlag:1
		//  			data Type:char ByteSize:1 MetaFlag:1

		// Check correct number of children.
		const int expectedChildrenCount = 15;
		CHECK_EQUAL (expectedChildrenCount, tree.m_Children.size());

		// Transfer children for explicit checking.
		TypeTree childTree[expectedChildrenCount];
		int childIndex = 0;
		for( TypeTree::iterator treeItr = tree.begin(); treeItr != tree.end(); ++treeItr )
			childTree[childIndex++] = *treeItr;

		// Check children types...

		TypeTree* child = childTree;
		CHECK_EQUAL (SerializeTraits<float>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_float", child->m_Name);
		CHECK_EQUAL (4, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<double>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_double", child->m_Name);
		CHECK_EQUAL (8, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<SInt32>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_int", child->m_Name);
		CHECK_EQUAL (4, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<UInt32>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_unsignedint", child->m_Name);
		CHECK_EQUAL (4, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<SInt64>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_SInt64", child->m_Name);
		CHECK_EQUAL (8, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<UInt64>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_UInt64", child->m_Name);
		CHECK_EQUAL (8, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<SInt32>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_SInt32", child->m_Name);
		CHECK_EQUAL (4, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<UInt32>::GetTypeString(), child->m_Type);
		CHECK_EQUAL ("m_UInt32", child->m_Name);
		CHECK_EQUAL (4, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<SInt16>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_SInt16", child->m_Name);
		CHECK_EQUAL (2, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<UInt16>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_UInt16", child->m_Name);
		CHECK_EQUAL (2, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<SInt8>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_SInt8", child->m_Name);
		CHECK_EQUAL (1, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<UInt8>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_UInt8", child->m_Name);
		CHECK_EQUAL (1, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL (SerializeTraits<char>::GetTypeString (), child->m_Type);
		CHECK_EQUAL ("m_char", child->m_Name);
		CHECK_EQUAL (1, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL ("m_bool", child->m_Name);
		CHECK_EQUAL (SerializeTraits<bool>::GetTypeString (), child->m_Type);
		CHECK_EQUAL (1, child->m_ByteSize);
		CHECK_EQUAL (0, child->m_Children.size());

		++child;
		CHECK_EQUAL ("m_UnityStr", child->m_Name); // String.
		CHECK_EQUAL (SerializeTraits<UnityStr>::GetTypeString (), child->m_Type);
		CHECK_EQUAL (-1, child->m_ByteSize);
		CHECK_EQUAL (1, child->m_Children.size());
		CHECK_EQUAL ("Array", (*child->begin()).m_Name); // String is an array.
		CHECK_EQUAL ("Array", (*child->begin()).m_Type);
		CHECK_EQUAL (-1, (*child->begin()).m_ByteSize);
		CHECK ( (*child->begin()).m_IsArray != 0);
		CHECK_EQUAL ("size", (*(*child->begin()).begin()).m_Name); // Array size.
		CHECK_EQUAL (SerializeTraits<SInt32>::GetTypeString (), (*(*child->begin()).begin()).m_Type);
		CHECK_EQUAL (4, (*(*child->begin()).begin()).m_ByteSize);
		CHECK_EQUAL ("data", (*++(*child->begin()).begin()).m_Name); // Array data.
		CHECK_EQUAL (SerializeTraits<char>::GetTypeString (), (*++(*child->begin()).begin()).m_Type);
		CHECK_EQUAL (1, (*++(*child->begin()).begin()).m_ByteSize);		
	}
}

#endif //ENABLE_UNIT_TESTS

