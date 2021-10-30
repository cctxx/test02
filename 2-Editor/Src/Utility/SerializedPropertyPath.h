#pragma once

#include "Runtime/Serialize/TypeTree.h"

enum { kMaxPropertyNameBufferSize = 16 * 1024 };

struct SerializedPropertyStack
{
	TypeTree::iterator iterator;
	int arrayIndex;
	int arrayByteOffset;
	int multipleObjectsMinArraySize;
};

///
void AddPropertyArrayIndexToBuffer (char* propertyNameBuffer, int arrayIndex);
void ClearPropertyNameFromBuffer (char* propertyNameBuffer);
void AddPropertyNameToBuffer (char* propertyNameBuffer, const TypeTree& typeTree);

void AddPropertyArrayIndexToBufferSerializedProperty (std::string& propertyNameBuffer, int arrayIndex);
void AddPropertyNameToBufferSerializedProperty (std::string& propertyNameBuffer, const TypeTree& typeTree);
void ClearPropertyNameFromBufferSerializedProperty (std::string& propertyNameBuffer);