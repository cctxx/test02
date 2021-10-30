#include "UnityPrefix.h"
#include "SerializedPropertyPath.h"
#include "Runtime/Serialize/IterateTypeTree.h"

using namespace std;

void AddPropertyArrayIndexToBuffer (char* propertyNameBuffer, int arrayIndex)
{
	int size = strlen(propertyNameBuffer);
	sprintf (propertyNameBuffer + size, ".data[%i]", arrayIndex);
}

void AddPropertyNameToBuffer (char* propertyNameBuffer, const TypeTree& typeTree)
{
	const char* name = typeTree.m_Name.c_str();
	int size = strlen(propertyNameBuffer);
	if (size != 0)
	{
		propertyNameBuffer[size] = '.';
		size += 1;
	}
	memcpy(propertyNameBuffer + size, name, strlen(name) + 1);
	////@TODO: Fix out of bounds
}

void ClearPropertyNameFromBuffer (char* propertyNameBuffer)
{
	int size = strlen(propertyNameBuffer);
	for (int i=size-1;i>=0;i--)
	{
		if (propertyNameBuffer[i] == '.')
		{	
			propertyNameBuffer[i] = 0;
			return;
		}
	}
	propertyNameBuffer[0] = 0;
}


void AddPropertyArrayIndexToBufferSerializedProperty (std::string& propertyNameBuffer, int arrayIndex)
{
	char buf[256];
	snprintf (buf, 256, ".data[%i]", arrayIndex);
	propertyNameBuffer.append(buf);
}

void AddPropertyNameToBufferSerializedProperty (std::string& propertyNameBuffer, const TypeTree& typeTree)
{
	if (!propertyNameBuffer.empty())
		propertyNameBuffer.push_back('.');
	propertyNameBuffer.append(typeTree.m_Name);
}

void ClearPropertyNameFromBufferSerializedProperty (std::string& propertyNameBuffer)
{
	Assert(!propertyNameBuffer.empty());
	
	string::size_type pos = propertyNameBuffer.rfind('.');
	if (pos == string::npos)
		pos = 0;
		
	propertyNameBuffer.resize(pos);
}