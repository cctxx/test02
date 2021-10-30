#ifndef TRANSFERUTILITY_H
#define TRANSFERUTILITY_H

#include "Runtime/Utilities/dynamic_array.h"

class TypeTree;
class Object;
class dynamic_bitset;

#if !UNITY_EXTERNAL_TOOL

/// Generates a TypeTree by serializing object using the ProxyTransfer
void GenerateTypeTree (Object& object, TypeTree* typeTree, int options = 0);

const TypeTree* FindAttributeInTypeTreeNoArrays (const TypeTree& typeTree, const char* propertyPath);

/// Serializes an object using to a memory buffer as binary data.
///
/// @note No type information will be written to the buffer.  To read the data,
///		a matching version of the serialization code must be used.
void WriteObjectToVector (Object& object, dynamic_array<UInt8>* data, int options = 0);

/// Unserializes an object from a memory buffer using binary serialization without type trees
/// (meaning the structure of the serialized data must match exactly with what the runtime
/// classes currently look like).
///
/// @note Dont forget calling CheckConsistency (), AwakeFromLoad (), SetDirty () after calling this method.
void ReadObjectFromVector (Object* object, const dynamic_array<UInt8>& data, int options = 0);

#if SUPPORT_TEXT_SERIALIZATION

/// Serialize the given object as text and return the resulting string.
std::string WriteObjectToString (Object& object, int options = 0);

/// 
void ReadObjectFromString (Object* object, std::string& string, int options = 0);

#endif // SUPPORT_TEXT_SERIALIZATION

/// Dont forget calling CheckConsistency (), AwakeFromLoad (), SetDirty () after calling ReadObjectFromVector

/// Call this after using CompletedReadObjectFromVector
void CompletedReadObjectFromVector (Object& object);

/// Collects the island of ptrs starting with object.
/// the collectedPtrs set should in most cases be empty on calling since when 
/// a ptr is already inserted it will not insert his ptrs to the set
void CollectPPtrs (Object& object, std::set<SInt32>* collectedPtrs);
void CollectPPtrs (Object& object, std::vector<Object*>& collectedObjects);

/// Copies all values from one object to another!
/// - does call set dirty and awake
/// - no pptr remapping or deep copying is done here!
void CopySerialized(Object& src, Object& dst);

int FindTypeTreeSeperator (const char* in);

#endif // !UNITY_EXTERNAL_TOOL


#if UNITY_EDITOR

/// Unserializes raw data and a typeTree of the data.
/// The serialized raw data and typeTree can be serialized in a different format.
/// The serializion code will try to match variables and convert data if possible
/// override can be NULL. If it is defined, only variables that are set to be overridden will be written to the object.
/// Every variables override is determined using override[variableTypeTree.m_Index] 
/// where variableTypeTree is some child of a variable in typeTree
void ReadObjectFromVector (Object* object, const dynamic_array<UInt8>& data, const TypeTree& typeTree, int options = 0);

void WriteObjectToVectorSwapEndian (Object& object, dynamic_array<UInt8>* data, int options = 0);

// Counts the variables that are contained in a typeTree
int CountTypeTreeVariables (const TypeTree& typeTree);

/// Walks over a typeTree, data pair moving the byteposition while going along.
/// The start position is data + *bytePosition
/// Takes into account arrays.
void WalkTypeTree (const TypeTree& typeTree, const UInt8* data, int* bytePosition);

/// CalculateByteSize from type and data
SInt32 CalculateByteSize(const TypeTree& type, const UInt8* data);

/// Byteswaps a data array with typetree.
/// * This will not work correctly if the data contains typeless data transfer!
void ByteSwapGeneric (const TypeTree& typeTree, dynamic_array<UInt8>& data);

/// Returns the class name part of the PPtr reference.
/// eg. returns "Transform" for "PPtr<Transform>"
/// For Mono classes it returns ""
std::string ExtractPPtrClassName (const TypeTree& typeTree);
std::string ExtractPPtrClassName (const std::string& typeTree);

/// Returns the class name of the pptr.
std::string ExtractMonoPPtrClassName (const TypeTree& typeTree);
std::string ExtractMonoPPtrClassName (const std::string& typeName);

#endif // UNITY_EDITOR

#endif
