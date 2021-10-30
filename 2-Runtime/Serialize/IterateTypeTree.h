#ifndef ITERATETYPETREE_H
#define ITERATETYPETREE_H

#include "TypeTree.h"
#include "SerializeTraits.h"
#include "TransferUtility.h"


/* Iterate typetree is used to process serialized data in arbitrary ways.

struct IterateTypeTreeFunctor
{
	// return true if you want to recurse into the function
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
	
	}
}

TypeTree typeTree;
dynamic_array<UInt8> data
// Create typetree and data
GenerateTypeTree(object);
WriteObjectToVector(object, &data);

// Modify data 
IterateTypeTreeFunctor func;
IterateTypeTree (typeTree, data, func);

ReadObjectFromVector(&object, data, typeTree);
object.CheckConsistency ();
object.AwakeFromLoad (false);
object.SetDirty ();

*/

inline SInt32 ExtractPPtrInstanceID (const UInt8* data)
{
	return *reinterpret_cast<const SInt32*> (data);
}

inline SInt32 ExtractPPtrInstanceID (const dynamic_array<UInt8>& data, int bytePosition)
{
	return ExtractPPtrInstanceID(&data[bytePosition]);
}

inline void SetPPtrInstanceID (SInt32 instanceID, dynamic_array<UInt8>& data, int bytePosition)
{
	*reinterpret_cast<SInt32*> (&data[bytePosition]) = instanceID;
}

inline bool IsTypeTreePPtr (const TypeTree& typeTree)
{
	return typeTree.m_Type.find ("PPtr<") == 0;
}

inline bool IsTypeTreeString (const TypeTree& typeTree)
{
	return typeTree.m_Type == "string" && typeTree.m_Children.size() == 1 && typeTree.m_Children.back().m_IsArray;
}

inline bool IsTypeTreePPtrArray (const TypeTree& typeTree)
{
	return typeTree.m_IsArray && typeTree.m_Children.back().m_Type.find ("PPtr<") == 0;
}

inline bool IsTypeTreeArraySize (const TypeTree& typeTree)
{
	return typeTree.m_Father != NULL && typeTree.m_Father->m_IsArray && &typeTree.m_Father->m_Children.front() == &typeTree;
}

inline bool IsTypeTreeArrayElement (const TypeTree& typeTree)
{
	return typeTree.m_Father != NULL && typeTree.m_Father->m_IsArray && &typeTree.m_Father->m_Children.back() == &typeTree;
}

inline bool IsTypeTreeArrayOrArrayContainer (const TypeTree& typeTree)
{
	return typeTree.m_IsArray || (typeTree.m_Children.size() == 1 && typeTree.m_Children.back().m_IsArray);
}

inline bool IsTypeTreeArray (const TypeTree& typeTree)
{
	return typeTree.m_IsArray;
}

inline int ExtractArraySize (const UInt8* data)
{
	return *reinterpret_cast<const SInt32*> (data);
}

inline void SetArraySize (UInt8* data, SInt32 size)
{
	*reinterpret_cast<SInt32*> (data) = size;
}

inline int ExtractArraySize (const dynamic_array<UInt8>& data, int bytePosition)
{
	return *reinterpret_cast<const SInt32*> (&data[bytePosition]);
}


inline UInt32 Align4_Iterate (UInt32 size)
{
	UInt32 value = ((size + 3) >> 2) << 2;
	return value;
}
#if UNITY_EDITOR

template<class Functor>
void IterateTypeTree (const TypeTree& typeTree, dynamic_array<UInt8>& data, Functor& functor)
{
	int bytePosition = 0;
	IterateTypeTree (typeTree, data, &bytePosition, functor);
}
template<class Functor>
void IterateTypeTree (const TypeTree& typeTree, dynamic_array<UInt8>& data, int* bytePosition, Functor& functor)
{
	if (functor (typeTree, data, *bytePosition))
	{
		if (typeTree.m_IsArray)
		{
			// First child in an array is the size
			// Second child is the homogenous type of the array
			AssertIf (typeTree.m_Children.front ().m_Type != SerializeTraits<SInt32>::GetTypeString (NULL) || typeTree.m_Children.front ().m_Name != "size" || typeTree.m_Children.size () != 2);
			
			functor (typeTree.m_Children.front (), data, *bytePosition);
			
			SInt32 arraySize, i;
			arraySize = *reinterpret_cast<SInt32*> (&data[*bytePosition]);
			*bytePosition += sizeof (arraySize);
			
			for (i=0;i<arraySize;i++)
				IterateTypeTree (typeTree.m_Children.back (), data, bytePosition, functor);
		}
		else
		{
			TypeTree::TypeTreeList::const_iterator i;
			for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
				IterateTypeTree (*i, data, bytePosition, functor);
		}
		
		if (typeTree.IsBasicDataType ())
			*bytePosition += typeTree.m_ByteSize;

		if (typeTree.m_MetaFlag & kAlignBytesFlag)
			*bytePosition = Align4_Iterate (*bytePosition);
	}
	else
	{
		WalkTypeTree(typeTree, data.begin (), bytePosition);
	}
}
#endif
#endif
