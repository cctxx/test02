#include "UnityPrefix.h"
#include "PropertyModification.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Editor/Src/Utility/SerializedPropertyPath.h"
#include "Runtime/Serialize/FloatStringConversion.h"

///@TODO: For Align4... Might want to move it!
#include "Runtime/Serialize/CacheWrap.h"


using namespace std;

static void ApplyPropertyModifictionAtLocation (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition, const PropertyModification& modification);


///@TODO: Test float, int, Sint8 etc all combinations. Min/max values, some values in between. Conversion between the different value types.
/// @TODO: Array support 
///	- Resize array in place
/// - 

///@TODO: Test setting PPtr to an invalid object type that the target does not allow (Camera.targetTexture -> Mesh)

static inline int FindPropertySeperator (const char* in, char compareChar = '.')
{
	const char* c = in;
	while (*c != compareChar && *c != '\0')
		c++;
	return c - in;
}

static inline bool CompareData (const UInt8* src, const UInt8* dst, int size)
{
	if (size == 4)
		return *reinterpret_cast<const UInt32*> (src) == *reinterpret_cast<const UInt32*> (dst);
	else
	{
		for (int i=0;i<size;i++)
		{
			if (src[i] != dst[i])
				return false;
		}
		return true;
	}
}

static bool ExtractBasicTypeStringValueFromBytes (const TypeTree& typeTree, const UInt8* data, UnityStr& output)
{
	const string& type = typeTree.m_Type;
	
	if (type == SerializeTraits<SInt8>::GetTypeString ())
		output = IntToString(*reinterpret_cast<const SInt8*> (data));
	else if (type == SerializeTraits<UInt8>::GetTypeString ())
		output = UnsignedIntToString(*reinterpret_cast<const UInt8*> (data));
	else if (type == SerializeTraits<SInt16>::GetTypeString ())
		output = IntToString(*reinterpret_cast<const SInt16*> (data));
	else if (type == SerializeTraits<UInt16>::GetTypeString ())
		output = UnsignedIntToString(*reinterpret_cast<const UInt16*> (data));
	else if (type == SerializeTraits<SInt32>::GetTypeString ())
		output = IntToString(*reinterpret_cast<const SInt32*> (data));
	else if (type == SerializeTraits<UInt32>::GetTypeString ())
		output = UnsignedIntToString(*reinterpret_cast<const UInt32*> (data));
	else if (type == SerializeTraits<SInt64>::GetTypeString ())
		output = Int64ToString(*reinterpret_cast<const SInt32*> (data));
	else if (type == SerializeTraits<UInt64>::GetTypeString ())
		output = UnsignedInt64ToString(*reinterpret_cast<const UInt32*> (data));
	else if (type == SerializeTraits<char>::GetTypeString ())
		output = IntToString(*reinterpret_cast<const char*> (data));
	else if (type == SerializeTraits<bool>::GetTypeString ())
		output = IntToString(*reinterpret_cast<const UInt8*> (data));
	
	// Float types
	else if (type == SerializeTraits<float>::GetTypeString ())
		FloatToStringAccurate(*reinterpret_cast<const float*> (data), output);
	else if (type == SerializeTraits<double>::GetTypeString ())
		DoubleToStringAccurate(*reinterpret_cast<const double*> (data), output);
	
	else if (IsTypeTreeString(typeTree))
	{
		SInt32 size = ExtractArraySize(data);
		data += sizeof(SInt32);
		output.assign(data, data + size);
	}
	
	else
	{
		ErrorString("Unsupported int type " + type);
		return false;
	}
	
	return true;
}

bool AddPropertyModificationValueFromBytes (char* propertyNameBuffer, bool addPropertyName, const TypeTree& typeTree, const UInt8* data, PropertyModifications& outProperties)
{
	if (addPropertyName)
		AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
	
	PropertyModification modification;
	modification.propertyPath = propertyNameBuffer;
	
	if (addPropertyName)
		ClearPropertyNameFromBuffer(propertyNameBuffer);
	
	if (ExtractPropertyModificationValueFromBytes (typeTree, data, modification))
	{
		outProperties.push_back(modification);
		return true;
	}
	else
		return false;
}

bool ExtractPropertyModificationValueFromBytes (const TypeTree& typeTree, const UInt8* data, PropertyModification& outPropertyModification)
{
	if (typeTree.IsBasicDataType() || IsTypeTreeString(typeTree))
	{
		outPropertyModification.objectReference = NULL;
		return ExtractBasicTypeStringValueFromBytes(typeTree, data, outPropertyModification.value);
	}
	else if (IsTypeTreePPtr(typeTree))
	{
		outPropertyModification.value.clear();
		outPropertyModification.objectReference.SetInstanceID(ExtractPPtrInstanceID(data));
		return true;
	}
	else
	{
		ErrorString("Unsupported type " + typeTree.m_Type);
		return false;
	}
}

static SInt32 ExtractSignedInt (const PropertyModification& modification)
{
	return atol(modification.value.c_str());
}

static UInt32 ExtractUnsignedInt (const PropertyModification& modification)
{
	#if UNITY_WIN
	return _atoi64(modification.value.c_str());
	#else
	return atoll(modification.value.c_str());
	#endif
}

static float ExtractFloat (const PropertyModification& modification)
{
	return StringToFloatAccurate(modification.value.c_str());
}
static double ExtractDouble (const PropertyModification& modification)
{
	return StringToDoubleAccurate(modification.value.c_str());
}

bool SetStringValue (const TypeTree& typeTree, dynamic_array<UInt8>& dataVector, int bytePosition, const std::string& stringValue)
{
	ResizeArrayGeneric(typeTree.m_Children.back(), dataVector, bytePosition, stringValue.size(), false);
	memcpy(dataVector.begin() + bytePosition + sizeof(SInt32), stringValue.c_str(), stringValue.size());
	return true;
}

/// Counts the size of an array element when all values are set to zero
static void CountZeroElementSize (const TypeTree& typeTree, int* bytePosition)
{
	/// During the 2.1 beta we had a bug that generated kAnyChildUsesAlignBytesFlag incorrectly,
	/// this was only available in development builds thus we require the (|| typeTree.m_Children.empty())
	bool hasBasicTypeSize = typeTree.m_ByteSize != -1 && (typeTree.m_MetaFlag & kAnyChildUsesAlignBytesFlag) == 0 || typeTree.m_Children.empty();
	
	if (hasBasicTypeSize)
	{
		AssertIf (typeTree.m_ByteSize == -1);
		*bytePosition += typeTree.m_ByteSize;
	}
	else if (typeTree.m_IsArray)
	{
		*bytePosition += sizeof(SInt32);
	}
	else
	{
		AssertIf (typeTree.m_Children.empty ());
		
		TypeTree::TypeTreeList::const_iterator i;
		for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
			CountZeroElementSize (*i, bytePosition);
	}
	
	if (typeTree.m_MetaFlag & kAlignBytesFlag)
	{
		*bytePosition = Align4(*bytePosition);
	}
}

///@TODO: Test for setting property in PropertyModification array first, then size, robustness.
///       Test for having two arrays nested in each other and random orders of array size & array

////@TODO: Option for smart resize which fills based on last value vs all zero
////       PREFABS should use dumb zero resize!
static int CalculateByteSize (dynamic_array<UInt8>& dataVector, int bytePosition, const TypeTree& elementType, int arraySize)
{
	int tmp = bytePosition;
	for (int i=0;i<arraySize;i++)
		WalkTypeTree(elementType, &dataVector[0], &tmp);
	return tmp - bytePosition;
}

void FixAlignmentForArrayResize (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int arrayDataByteOffset, int oldArrayByteSize, int newArrayByteSize)
{
	Assert(arrayTypeTree.m_IsArray);
	// Compute aligned data size
	if ((arrayTypeTree.m_MetaFlag & kAlignBytesFlag) != 0)
		return;
	
	int newArrayByteSizeAligned = Align4LeftOver(newArrayByteSize);
	int oldArrayByteSizeAligned = Align4LeftOver(oldArrayByteSize);
	
	int endOfArrayData = arrayDataByteOffset + newArrayByteSize;
	
	if (dataVector.size() >= endOfArrayData + oldArrayByteSizeAligned)
		dataVector.erase (dataVector.begin() + endOfArrayData, dataVector.begin() + endOfArrayData + oldArrayByteSizeAligned);
	dynamic_array<UInt8> tempData(newArrayByteSizeAligned, 0, kMemTempAlloc);
	dataVector.insert (dataVector.begin() + endOfArrayData, tempData.begin(), tempData.end());
}

bool DuplicateArrayElement (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int bytePosition, SInt32 srcIndex, SInt32 dstIndex)
{
	Assert(arrayTypeTree.m_IsArray);
	
	int arrayDataBytePosition = bytePosition + sizeof(SInt32);
	int arraySize = ExtractArraySize(dataVector, bytePosition);
	
	if (srcIndex < 0 || srcIndex >= arraySize)
		return false;
	if (dstIndex < 0 || dstIndex > arraySize)
		return false;
	
	const TypeTree& elementType = arrayTypeTree.m_Children.back();
	int oldArrayByteSize = CalculateByteSize(dataVector, arrayDataBytePosition, elementType, arraySize);
	
	int srcBegin = CalculateByteSize(dataVector, arrayDataBytePosition, elementType, srcIndex) + arrayDataBytePosition;
	int duplicateSize = CalculateByteSize(dataVector, srcBegin, elementType, 1);
	int dstBegin = CalculateByteSize(dataVector, arrayDataBytePosition, elementType, dstIndex) + arrayDataBytePosition;
	
	dynamic_array<UInt8> elementData(kMemTempAlloc);
	elementData.assign(dataVector.begin () + srcBegin, dataVector.begin () + srcBegin + duplicateSize);
	
	// insert data
	dataVector.insert (dataVector.begin () + dstBegin, elementData.begin (), elementData.end ()); 
	
	// Set size variable
	SetArraySize(&dataVector[bytePosition], arraySize + 1);
	
	FixAlignmentForArrayResize(arrayTypeTree, dataVector, arrayDataBytePosition, oldArrayByteSize, oldArrayByteSize + duplicateSize);
	
	return true;
}

bool DeleteArrayElement (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int bytePosition, SInt32 index)
{
	Assert(arrayTypeTree.m_IsArray);
	
	int arrayDataBytePosition = bytePosition + sizeof(SInt32);
	int arraySize = ExtractArraySize(dataVector, bytePosition);
	
	if (index < 0 || index >= arraySize)
		return false;
	
	const TypeTree& elementType = arrayTypeTree.m_Children.back();
	
	int oldArrayByteSize = CalculateByteSize(dataVector, arrayDataBytePosition, elementType, arraySize);
	
	int begin = CalculateByteSize(dataVector, arrayDataBytePosition, elementType, index) + arrayDataBytePosition;
	int deleteSize = CalculateByteSize(dataVector, begin, elementType, 1);
	
	// erase data
	dataVector.erase (dataVector.begin () + begin, 
	                  dataVector.begin () + begin + deleteSize);
	
	// Set size variable
	SetArraySize(&dataVector[bytePosition], arraySize - 1);
	
	FixAlignmentForArrayResize(arrayTypeTree, dataVector, arrayDataBytePosition, oldArrayByteSize, oldArrayByteSize - deleteSize);
	
	return true;
}

bool ResizeArrayGeneric (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int bytePosition, SInt32 newArraySize, bool copyLastElement)
{
	Assert(arrayTypeTree.m_IsArray);
	
	// This will come quite unexpected for whoever calls the function!
	// In case this is happening, must move alignment handling of parent into ResizeArrayGeneric.
	if ((arrayTypeTree.m_Father->m_MetaFlag & kAlignBytesFlag) != 0 && (arrayTypeTree.m_MetaFlag & kAlignBytesFlag) == 0)
	{
		AssertString("Unhandled alignment handling");
	}
	
	int arraySize = ExtractArraySize(dataVector, bytePosition);
	if (arraySize == newArraySize)
		return false;
	
	if (newArraySize < 0)
		return false;
	
	int newElements = newArraySize - arraySize;
	
	const TypeTree& elementType = arrayTypeTree.m_Children.back();
	int arrayDataByteOffset = bytePosition + sizeof(SInt32);
	
	// Calculate the complete old array bytesize including aligned empty data
	int oldArrayByteSize = CalculateByteSize(dataVector, arrayDataByteOffset, elementType, arraySize);
	int newArrayByteSize;
	
	int lastElementByteSize = 0;

	// Only allow copying of the last element, if the array resize actually allows for that
	if(newArraySize <= arraySize || arraySize <= 0)
		copyLastElement = false;
	
	if (newArraySize > arraySize)
	{
		if (copyLastElement)
		{
			// Find the last array element byte start (Walk typetree arraySize - 1 times)
			int lastArrayElementByteStart = arrayDataByteOffset;
			for (int i=0;i<arraySize - 1;i++)
				WalkTypeTree(elementType, &dataVector[0], &lastArrayElementByteStart);
			
			// Calculate the bytesize of the last element by walking over it (Just in case it also has arrays inside)
			int tmp = lastArrayElementByteStart;
			WalkTypeTree (elementType, &dataVector[0], &tmp);
			
			lastElementByteSize = tmp - lastArrayElementByteStart;
			newArrayByteSize = oldArrayByteSize + lastElementByteSize * newElements;
			AssertIf(lastElementByteSize < 0);
		}
		// Append empty data element
		else
		{
			CountZeroElementSize(elementType, &lastElementByteSize);
			newArrayByteSize = lastElementByteSize * newElements + oldArrayByteSize;
		}
	}
	else
	{
		newArrayByteSize = CalculateByteSize(dataVector, arrayDataByteOffset, elementType, newArraySize);
	}
	
	// Compute aligned data size
	int newArrayByteSizeAligned = newArrayByteSize;
	int oldArrayByteSizeAligned = oldArrayByteSize;
	if (arrayTypeTree.m_MetaFlag & kAlignBytesFlag)
	{
		newArrayByteSizeAligned = Align4(newArrayByteSize);
		oldArrayByteSizeAligned = Align4(oldArrayByteSize);
	}
	
	// Inject extra data into array
	int alignedByteSizeDifference = newArrayByteSizeAligned - oldArrayByteSizeAligned;
	if (alignedByteSizeDifference > 0)
	{
		dynamic_array<UInt8> tempData(alignedByteSizeDifference, 0, kMemTempAlloc);
		dataVector.insert(dataVector.begin() + arrayDataByteOffset + oldArrayByteSize, tempData.begin(), tempData.end());
	}
	else if (alignedByteSizeDifference < 0)
		dataVector.erase (dataVector.begin() + arrayDataByteOffset + newArrayByteSizeAligned, dataVector.begin() + arrayDataByteOffset + oldArrayByteSizeAligned);
	
	UInt8* arrayData = dataVector.begin() + arrayDataByteOffset;
	
	// Replicate last element
	if (copyLastElement)
	{
		for (int i=0;i<newElements;i++)
			memcpy(arrayData + oldArrayByteSize + i * lastElementByteSize, arrayData + (oldArrayByteSize - lastElementByteSize), lastElementByteSize);
	}
	// Replicate empty element size
	else
	{
		for (int i=0;i<newElements;i++)
			memset(arrayData + oldArrayByteSize + i * lastElementByteSize, 0, lastElementByteSize);
	}
	
	// Clear aligned data
	memset(arrayData + newArrayByteSize, 0, newArrayByteSizeAligned - newArrayByteSize);
	
	SetArraySize(&dataVector[bytePosition], newArraySize);
	return true;
}


static void ApplyPropertyModifictionAtLocation (const TypeTree& typeTree, dynamic_array<UInt8>& dataVector, int bytePosition, const PropertyModification& modification)
{
	const string& type = typeTree.m_Type;
	
	UInt8* data = &dataVector[bytePosition];
	
	// Array size
	if (IsTypeTreeArraySize(typeTree))
		ResizeArrayGeneric(*typeTree.m_Father, dataVector, bytePosition, ExtractSignedInt(modification), false);
	
	// Integer types
	if (type == SerializeTraits<SInt32>::GetTypeString ())
		*reinterpret_cast<int*> (data) = ExtractSignedInt(modification);
	else if (type == SerializeTraits<UInt32>::GetTypeString ())
		*reinterpret_cast<unsigned int*> (data) = ExtractUnsignedInt(modification);
	else if (type == SerializeTraits<SInt8>::GetTypeString ())
		*reinterpret_cast<SInt8*> (data) = ExtractSignedInt(modification);
	else if (type == SerializeTraits<UInt8>::GetTypeString ())
		*reinterpret_cast<UInt8*> (data) = ExtractUnsignedInt(modification);
	else if (type == SerializeTraits<SInt16>::GetTypeString ())
		*reinterpret_cast<SInt16*> (data) = ExtractSignedInt(modification);
	else if (type == SerializeTraits<UInt16>::GetTypeString ())
		*reinterpret_cast<UInt16*> (data) = ExtractUnsignedInt(modification);
	else if (type == SerializeTraits<SInt32>::GetTypeString ())
		*reinterpret_cast<SInt32*> (data) = ExtractSignedInt(modification);
	else if (type == SerializeTraits<UInt32>::GetTypeString ())
		*reinterpret_cast<UInt32*> (data) = ExtractUnsignedInt(modification);
	else if (type == SerializeTraits<char>::GetTypeString ())
		*reinterpret_cast<char*> (data) = ExtractSignedInt(modification);
	else if (type == SerializeTraits<bool>::GetTypeString ())
		*reinterpret_cast<UInt8*> (data) = ExtractSignedInt(modification);
	
	// Float types
	else if (type == SerializeTraits<float>::GetTypeString ())
		*reinterpret_cast<float*> (data) = ExtractFloat(modification);
	else if (type == SerializeTraits<double>::GetTypeString ())
		*reinterpret_cast<double*> (data) = ExtractDouble(modification);
	else if (IsTypeTreePPtr(typeTree))
		SetPPtrInstanceID(modification.objectReference.GetInstanceID(), dataVector, bytePosition);
	
	// String
	else if (IsTypeTreeString(typeTree))
		SetStringValue(typeTree, dataVector, bytePosition, modification.value);
	
	///@TODO: PPtr and consistency check for it
	else
	{	
		// ErrorString("Unsupported int type " + type);
	}
}

static void GeneratePropertyDiff (char* propertyNameBuffer, bool addPropertyName, const TypeTree& typeTree, const UInt8* src, int* srcBytePosition, const UInt8* dst, int* dstBytePosition, RemapPPtrCallback* clonedObjectToParentObjectRemap, vector<PropertyModification>& properties)
{
	if (typeTree.IsBasicDataType ())
	{
		if (!CompareData (src + *srcBytePosition, dst + *dstBytePosition, typeTree.m_ByteSize))
		{
			AddPropertyModificationValueFromBytes(propertyNameBuffer, addPropertyName, typeTree, dst + *dstBytePosition, properties);
		}
		*srcBytePosition += typeTree.m_ByteSize;
		*dstBytePosition += typeTree.m_ByteSize;
	}
	else if (IsTypeTreeString(typeTree))
	{
		SInt32 arraySize = ExtractArraySize(src + *srcBytePosition);
		bool isSame = false;
		if (arraySize == ExtractArraySize(dst + *dstBytePosition))
		{
			if (CompareData (src + *srcBytePosition + sizeof(SInt32), dst + *dstBytePosition + sizeof(SInt32), arraySize))
				isSame = true;
		}
		
		if (!isSame)
		{
			AddPropertyModificationValueFromBytes(propertyNameBuffer, addPropertyName, typeTree, dst + *dstBytePosition, properties);
		}
		
		WalkTypeTree(typeTree, src, srcBytePosition);
		WalkTypeTree(typeTree, dst, dstBytePosition);
		return;
	}
	else if (typeTree.m_IsArray)
	{
		SInt32 srcSize = ExtractArraySize(src + *srcBytePosition);
		SInt32 dstSize = ExtractArraySize(dst + *dstBytePosition);
		
		const TypeTree& sizeTypeTree = typeTree.m_Children.front();
		const TypeTree& elementTypeTree = typeTree.m_Children.back();
		
		// Create diff for array size
		AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
		GeneratePropertyDiff (propertyNameBuffer, true, sizeTypeTree, src, srcBytePosition, dst, dstBytePosition, clonedObjectToParentObjectRemap, properties);
		
		// Iterate over both arrays
		for (int i=0;i<max(dstSize, srcSize);i++)
		{
			AddPropertyArrayIndexToBuffer(propertyNameBuffer, i);
			
			// Doesn't exist in dst size, no need for diffing
			if (i >= dstSize)
			{
				Assert(i < srcSize);
				Assert(i >= dstSize);
				WalkTypeTree(elementTypeTree, src, srcBytePosition);
			}
			// Array element doesn't exist in source -> Use empty array data
			else if (i >= srcSize)
			{
				dynamic_array<UInt8> emptyBuffer(kMemTempAlloc);
				int zeroElementSize = 0;
				CountZeroElementSize(elementTypeTree, &zeroElementSize);
				emptyBuffer.resize_initialized(zeroElementSize);
				int emptyBufferPosition = 0;
				GeneratePropertyDiff (propertyNameBuffer, false, elementTypeTree, &emptyBuffer[0], &emptyBufferPosition, dst, dstBytePosition, clonedObjectToParentObjectRemap, properties);
			}
			// Array element exists in source -> compare
			else
			{
				GeneratePropertyDiff (propertyNameBuffer, false, elementTypeTree, src, srcBytePosition, dst, dstBytePosition, clonedObjectToParentObjectRemap, properties);
			}
			
			ClearPropertyNameFromBuffer(propertyNameBuffer);
		}
		
		ClearPropertyNameFromBuffer(propertyNameBuffer);
	}
	else if (IsTypeTreePPtr(typeTree))
	{
		SInt32 srcInstanceID = ExtractPPtrInstanceID(src + *srcBytePosition);
		SInt32 dstInstanceID = ExtractPPtrInstanceID(dst + *dstBytePosition);
		SInt32 dstInstanceIDRemapped = dstInstanceID;
		
		// Remap the dst PPtr, this is used so that referenced to objects in the cloned island are compared to the same object in the clone.
		// For example the gameObject pptr of a component, should map to the cloned object not to the original prefab
		if (clonedObjectToParentObjectRemap != NULL)
			dstInstanceIDRemapped = clonedObjectToParentObjectRemap->Remap(dstInstanceID);
		
		if (srcInstanceID != dstInstanceIDRemapped)
		{
			if (addPropertyName)
				AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
			
			PropertyModification modification;
			modification.propertyPath = propertyNameBuffer;
			modification.objectReference = PPtr<Object> (dstInstanceID);
			properties.push_back(modification);
			
			if (addPropertyName)
				ClearPropertyNameFromBuffer(propertyNameBuffer);
		}
		
		*srcBytePosition += typeTree.m_ByteSize;
		*dstBytePosition += typeTree.m_ByteSize;
	}
	else
	{
		// Recurse
		TypeTree::TypeTreeList::const_iterator i;
		for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
		{
			if (addPropertyName)
				AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
			GeneratePropertyDiff (propertyNameBuffer, true, *i, src, srcBytePosition, dst, dstBytePosition, clonedObjectToParentObjectRemap, properties);
			
			if (addPropertyName)
				ClearPropertyNameFromBuffer(propertyNameBuffer);
		}
	}
	
	// Apply alignment on both arrays
	if (typeTree.m_MetaFlag & kAlignBytesFlag)
	{
		*srcBytePosition = Align4_Iterate (*srcBytePosition);
		*dstBytePosition = Align4_Iterate (*dstBytePosition);
	}
}

static void GeneratePropertyDiffBackwardsCompatible (char* propertyNameBuffer, bool addPropertyName, const TypeTree& typeTree, const dynamic_bitset& overrides, const UInt8* dst, int* dstBytePosition, vector<PropertyModification>& properties)
{
	if (typeTree.IsBasicDataType ())
	{
		if (overrides[typeTree.m_Index])
			AddPropertyModificationValueFromBytes(propertyNameBuffer, addPropertyName, typeTree, dst + *dstBytePosition, properties);
		
		*dstBytePosition += typeTree.m_ByteSize;
	}
	else if (IsTypeTreeString(typeTree))
	{
		if (overrides[typeTree.m_Index])
			AddPropertyModificationValueFromBytes(propertyNameBuffer, addPropertyName, typeTree, dst + *dstBytePosition, properties);
		
		WalkTypeTree(typeTree, dst, dstBytePosition);
		return;
	}
	else if (typeTree.m_IsArray)
	{
		SInt32 dstSize = ExtractArraySize(dst + *dstBytePosition);
		
		const TypeTree& sizeTypeTree = typeTree.m_Children.front();
		const TypeTree& elementTypeTree = typeTree.m_Children.back();
		
		// Create diff for array size
		AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
		GeneratePropertyDiffBackwardsCompatible (propertyNameBuffer, true, sizeTypeTree, overrides, dst, dstBytePosition, properties);
		
		// Iterate over both arrays
		for (int i=0;i<dstSize;i++)
		{
			AddPropertyArrayIndexToBuffer(propertyNameBuffer, i);
			
			GeneratePropertyDiffBackwardsCompatible (propertyNameBuffer, false, elementTypeTree, overrides, dst, dstBytePosition, properties);
			
			ClearPropertyNameFromBuffer(propertyNameBuffer);
		}
		ClearPropertyNameFromBuffer(propertyNameBuffer);
	}
	else if (IsTypeTreePPtr(typeTree))
	{
		SInt32 dstInstanceID = ExtractPPtrInstanceID(dst + * dstBytePosition);

		if (overrides[typeTree.m_Index])
		{
			if (addPropertyName)
				AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
			
			PropertyModification modification;
			modification.propertyPath = propertyNameBuffer;
			modification.objectReference = PPtr<Object> (dstInstanceID);
			properties.push_back(modification);
			
			if (addPropertyName)
				ClearPropertyNameFromBuffer(propertyNameBuffer);
		}
		
		*dstBytePosition += typeTree.m_ByteSize;
	}
	else
	{
		// Recurse
		TypeTree::TypeTreeList::const_iterator i;
		for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
		{
			if (addPropertyName)
				AddPropertyNameToBuffer(propertyNameBuffer, typeTree);
			GeneratePropertyDiffBackwardsCompatible (propertyNameBuffer, true, *i, overrides, dst, dstBytePosition, properties);
			
			if (addPropertyName)
				ClearPropertyNameFromBuffer(propertyNameBuffer);
		}
	}
	
	// Apply alignment on both arrays
	if (typeTree.m_MetaFlag & kAlignBytesFlag)
	{
		*dstBytePosition = Align4_Iterate (*dstBytePosition);
	}
}


void GeneratePropertyDiff (const TypeTree& typeTree, const dynamic_array<UInt8>& src, const dynamic_array<UInt8>& dst, RemapPPtrCallback* clonedObjectToParentObjectRemap, PropertyModifications& properties)
{
	Assert(properties.empty());
	
	int srcBytePosition = 0;
	int dstBytePosition = 0;
	
	char propertyNameBuffer[kMaxPropertyNameBufferSize] = { 0 };
	
	GeneratePropertyDiff (propertyNameBuffer, false, typeTree, &src[0], &srcBytePosition, &dst[0], &dstBytePosition, clonedObjectToParentObjectRemap, properties);
	Assert(propertyNameBuffer[0] == 0);
}

void GeneratePropertyDiffBackwardsCompatible (const TypeTree& typeTree, const dynamic_bitset& overrides, const dynamic_array<UInt8>& dst, PropertyModifications& properties)
{
	Assert(properties.empty());
	
	int dstBytePosition = 0;
	
	char propertyNameBuffer[kMaxPropertyNameBufferSize] = { 0 };
	
	GeneratePropertyDiffBackwardsCompatible (propertyNameBuffer, false, typeTree, overrides, dst.begin(), &dstBytePosition, properties);
	Assert(propertyNameBuffer[0] == 0);
}


// Returns the index of the seperator if the next name up to the seperator is the same as name
// If it's not the same, returns -1;
inline int CompareWithSeperator (const char* path, const std::string& name, char seperatorName = '.')
{
	int seperator = FindPropertySeperator (path, seperatorName);
	if (seperator != name.size())
		return -1;
	
	// continue if the name isn't the same
	string::const_iterator n = name.begin ();
	int j;
	for (j=0;j<seperator;j++,n++)
	{
		if (path[j] != *n)
			break;
	}
	
	if (j == seperator)
		return seperator;
	else
		return -1;
}		

template<class Functor>
bool ApplyPropertyRecurse  (Functor& functor, const TypeTree& typeTree, dynamic_array<UInt8>& data, int* bytePosition, const char* path, PropertyModification& property)
{
	if (typeTree.m_IsArray)
	{
		SInt32 arraySize = ExtractArraySize(data, *bytePosition);
		
		// Apply size
		if (strcmp(path, "size") == 0)
		{	
			functor(typeTree.m_Children.front(), data, *bytePosition, property);
			return true;
		}
		*bytePosition += sizeof(SInt32);
		
		// Make sure we have a path of the format: "Array.data[5]"
		if (!BeginsWith(path, "data["))
			return false;
		path += 5;
		
		int seperator = FindPropertySeperator (path, '.');
		if (path[seperator-1] != ']')
			return false;
		
		// Make sure the array index is in a sensible range
		int index = atoi (path);
		if (index < 0 || index >= arraySize)
			return false;
		
		// Skip to the index that we want to modify
		const TypeTree& elementTypeTree = typeTree.m_Children.back();
		for (int i=0;i<index;i++)
			WalkTypeTree(elementTypeTree, data.begin(), bytePosition);
		
		// Leaf property -> Call functor
		if (path[seperator] == '\0')
		{
			functor(elementTypeTree, data, *bytePosition, property);
			return true;
		}
		// Recurse
		else
		{
			return ApplyPropertyRecurse(functor, elementTypeTree, data, bytePosition, path + seperator + 1, property);
		}
	}
	else
	{
		TypeTree::TypeTreeList::const_iterator i;
		for (i = typeTree.m_Children.begin (); i !=  typeTree.m_Children.end ();++i)
		{
			int seperator = CompareWithSeperator (path, i->m_Name);
			
			// Found a path we should follow..
			if (seperator != -1)
			{
				// Leaf property -> call functor
				if (path[seperator] == '\0')
				{
					functor(*i, data, *bytePosition, property);
					return true;
				}
				// Path matches -> keep recursing
				else
				{
					return ApplyPropertyRecurse(functor, *i, data, bytePosition, path + seperator + 1, property);
				}
			}
			// Keep going through the data, since we don't have something matching yet
			else
			{
				WalkTypeTree(*i, data.begin(), bytePosition);
			}
		}
	}
	
	return false;
}

struct UpdatePrefabModificationToCurrentState
{
	bool m_ModifiedAnything;
	
	UpdatePrefabModificationToCurrentState ()
	{
		m_ModifiedAnything = false;
	}
	
	void operator () (const TypeTree& typeTree, dynamic_array<UInt8>& dataVector, int bytePosition, PropertyModification& modification)
	{
		PropertyModification tempModification;
		ExtractPropertyModificationValueFromBytes(typeTree, &dataVector[bytePosition], tempModification);
		if (!PropertyModification::CompareValues (tempModification, modification))
		{
			modification.value = tempModification.value;
			modification.objectReference = tempModification.objectReference;
			m_ModifiedAnything = true;
		}
	}
};

bool ExtractCurrentValueOfAllModifications (PPtr<Object> targetObject, const TypeTree& typeTree, dynamic_array<UInt8>& data, PropertyModification* modifications, int modificationCount)
{
	UpdatePrefabModificationToCurrentState updateModification;
	
	for (int i=0;i<modificationCount;i++)
	{
		if (modifications[i].target == targetObject)
		{
			int bytePosition = 0;
			ApplyPropertyRecurse(updateModification, typeTree, data, &bytePosition, modifications[i].propertyPath.c_str(), modifications[i]);
		}
	}
	
	return updateModification.m_ModifiedAnything;
}


bool ApplyPropertyModification (const TypeTree& typeTree, dynamic_array<UInt8>& data, const PropertyModification* modifications, int nbModifications)
{
	bool allSuccessful = true;
	for (int i=0;i<nbModifications;i++)
	{
		int bytePosition = 0;
		allSuccessful &= ApplyPropertyRecurse(ApplyPropertyModifictionAtLocation, typeTree, data, &bytePosition, modifications[i].propertyPath.c_str(), const_cast<PropertyModification&> (modifications[i]));
	}
	return allSuccessful;
}


bool ComparePropertyPath (const PropertyModification& lhs, const PropertyModification& rhs)
{
	bool lhsArray = EndsWith(lhs.propertyPath, "Array.size");
	bool rhsArray = EndsWith(rhs.propertyPath, "Array.size");
	
	if (lhsArray && rhsArray)
		return lhs.propertyPath.size() < rhs.propertyPath.size();
	else if (lhsArray)
		return true;
	else
		return false;
}

void EnsureSizePropertiesSorting (std::vector<PropertyModification>& properties)
{
	stable_sort(properties.begin(), properties.end(), ComparePropertyPath);
}

bool ApplyPropertyModification (const TypeTree& typeTree, dynamic_array<UInt8>& data, const PropertyModification& modification)
{
	return ApplyPropertyModification (typeTree, data, &modification, 1);
}

bool IsPropertyPathOverridden (const char* propertyPath, const char* pathToDisplay)
{
	int propertyPathLength = strlen(propertyPath);
	int pathToDisplayLength = strlen(pathToDisplay);
	
	if (propertyPathLength < pathToDisplayLength)
		return false;
	
	if (strncmp(propertyPath, pathToDisplay, pathToDisplayLength) != 0)
		return false;
	
	return propertyPath[pathToDisplayLength] == '.' || propertyPath[pathToDisplayLength] == 0;
}

#include "Runtime/Camera/Camera.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Editor/Src/Utility/SerializedProperty.h"

void TestPropertyModificationCamera ()
{
	RenderTexture* tex = CreateObjectFromCode<RenderTexture>(kDefaultAwakeFromLoad);
	
	Camera* camera = CreateObjectFromCode<Camera>(kDefaultAwakeFromLoad);
	camera->SetOrthographic(true);
	
	dynamic_array<UInt8> originalData(kMemTempAlloc);
	WriteObjectToVector(*camera, &originalData);
	
	// Test applying a value
	vector<PropertyModification> modification;
	modification.push_back(CreatePropertyModification("m_NormalizedViewPortRect.y", "5.0"));
	modification.push_back(CreatePropertyModification("m_NormalizedViewPortRect.x", "2.0"));
	modification.push_back(CreatePropertyModification("orthographic", "0"));
	modification.push_back(CreatePropertyModification("m_TargetTexture", tex));
	
	TypeTree type;
	dynamic_array<UInt8> data(kMemTempAlloc);
	GenerateTypeTree(*camera, &type);
	WriteObjectToVector(*camera, &data);
	
	ApplyPropertyModification(type, data, &modification[0], modification.size());
	ReadObjectFromVector(camera, data);
	camera->AwakeFromLoad(kDefaultAwakeFromLoad);
	
	Assert (camera->GetNormalizedViewportRect().y == 5.0F);
	Assert (camera->GetNormalizedViewportRect().x == 2.0F);
	Assert (!camera->GetOrthographic());
	Assert (camera->GetTargetTexture() == tex);
	
	// Test Generating a diff
	vector<PropertyModification> outModifications;
	GeneratePropertyDiff(type, originalData, data, NULL, outModifications);
	
	Assert(outModifications.size() == 4);
	PropertyModification& mod0 = outModifications[0];
	PropertyModification& mod1 = outModifications[1];
	PropertyModification& mod2 = outModifications[2];
	PropertyModification& mod3 = outModifications[3];
	Assert(mod0.propertyPath == "m_NormalizedViewPortRect.x");
	Assert(mod0.value == "2");
	Assert(mod1.propertyPath == "m_NormalizedViewPortRect.y");
	Assert(mod1.value == "5");
	Assert(mod2.propertyPath == "orthographic");
	Assert(mod2.value == "0");
	Assert(mod3.propertyPath == "m_TargetTexture");
	Assert(mod3.value == "");
	Assert(mod3.objectReference == PPtr<Object> (tex));
	
	DestroyObjectHighLevel(camera);
	DestroyObjectHighLevel(tex);
}


void AssertModifications (const PropertyModifications& lhs, const PropertyModifications& rhs)
{
	Assert(lhs.size() == rhs.size());
	for (int i=0;i<lhs.size();i++)
	{
		Assert(PropertyModification::CompareAll(lhs[i], rhs[i]));
	}
}


void TestPropertyModificationGameObject ()
{
	GameObject* go = CreateObjectFromCode<GameObject>();
	
	TypeTree type;
	dynamic_array<UInt8> data(kMemTempAlloc);
	GenerateTypeTree(*go, &type);
	
	
	dynamic_array<UInt8> originalData(kMemTempAlloc);
	WriteObjectToVector(*go, &originalData);
	
	vector<PropertyModification> modification;
	modification.push_back(CreatePropertyModification("m_Name", "Hello"));
	modification.push_back(CreatePropertyModification("m_TagString", "Player"));
	vector<PropertyModification> outModifications;
	
	
#define TEST_APPLY(x) \
modification[0].value = x; \
WriteObjectToVector(*go, &data);  \
ApplyPropertyModification(type, data, &modification[0], modification.size()); \
ReadObjectFromVector(go, data); \
go->AwakeFromLoad(kDefaultAwakeFromLoad); \
Assert (go->GetName() == string(x)); \
Assert (go->GetTag() == kPlayerTag); \
outModifications.clear(); \
GeneratePropertyDiff(type, originalData, data, NULL, outModifications); \
AssertModifications(outModifications, modification);
	
	TEST_APPLY("Hello")
	TEST_APPLY("Hel")
	TEST_APPLY("Helga")
	TEST_APPLY("HelgHelg")
	
	DestroyObjectHighLevel(go);
}




void TestPropertyModificationPPtrArrayRemove ()
{
	TypeTree type;

	MeshRenderer* meshRenderer = CreateObjectFromCode<MeshRenderer>();
	Material* mat = CreateObjectFromCode<Material>();
	meshRenderer->SetMaterialCount(2);
	meshRenderer->SetMaterial(mat, 0);
	meshRenderer->SetMaterial(mat, 1);
	
	dynamic_array<UInt8> originalData(kMemTempAlloc);
	WriteObjectToVector(*meshRenderer, &originalData);
	GenerateTypeTree(*meshRenderer, &type);

	dynamic_array<UInt8> newData = originalData;

	PropertyModification modification;
	modification = CreatePropertyModification("m_Materials.Array.size", "1");
	ApplyPropertyModification(type, newData, &modification, 1);
	ReadObjectFromVector(meshRenderer, newData);
	meshRenderer->AwakeFromLoad(kDefaultAwakeFromLoad);

	Assert(meshRenderer->GetMaterialCount() == 1);
	Assert(meshRenderer->GetMaterial(0) == PPtr<Material> (mat));
	
	vector<PropertyModification> outModifications;
	GeneratePropertyDiff(type, originalData, newData, NULL, outModifications);

	Assert(outModifications.size() == 1);
	Assert(outModifications[0].propertyPath == "m_Materials.Array.size");
	Assert(outModifications[0].value == "1");
	
	DestroyObjectHighLevel(meshRenderer);
	DestroyObjectHighLevel(mat);
}


void TestPropertyModificationPPtrArray ()
{
	MeshRenderer* meshRenderer = CreateObjectFromCode<MeshRenderer>();
	Material* mat = CreateObjectFromCode<Material>();
	
	dynamic_array<UInt8> originalData(kMemTempAlloc);
	WriteObjectToVector(*meshRenderer, &originalData);
	
	// Test applying a value
	vector<PropertyModification> modification;
	modification.push_back(CreatePropertyModification("m_Materials.Array.size", "2"));
	modification.push_back(CreatePropertyModification("m_Materials.Array.data[1]", mat));
	
	TypeTree type;
	dynamic_array<UInt8> data(kMemTempAlloc);
	GenerateTypeTree(*meshRenderer, &type);
	
	WriteObjectToVector(*meshRenderer, &data);
	
	ApplyPropertyModification(type, data, &modification[0], modification.size());
	ReadObjectFromVector(meshRenderer, data);
	meshRenderer->AwakeFromLoad(kDefaultAwakeFromLoad);
	
	Assert (meshRenderer->GetMaterialArray().size() == 2);
	Assert (meshRenderer->GetMaterialArray()[0].GetInstanceID() == 0);
	Assert (meshRenderer->GetMaterialArray()[1] == PPtr<Material> (mat));
	
	// Test Generating a diff
	vector<PropertyModification> outModifications;
	GeneratePropertyDiff(type, originalData, data, NULL, outModifications);
	
	Assert(outModifications.size() == 2);
	PropertyModification& mod0 = outModifications[0];
	PropertyModification& mod1 = outModifications[1];
	Assert(mod0.propertyPath == "m_Materials.Array.size");
	Assert(mod0.value == "2");
	Assert(mod1.propertyPath == "m_Materials.Array.data[1]");
	Assert(mod1.objectReference == PPtr<Object> (mat));
	
	
	// Verify that SerializedProperty property path
	SerializedObject serialized;
	serialized.Init (*meshRenderer);
	
	SerializedProperty property;
	serialized.GetIterator(property);
	property.FindProperty("m_Materials.Array.data[1]");
	Assert(property.GetPPtrValue() == PPtr<Object> (mat));
	
	
	DestroyObjectHighLevel(meshRenderer);
	DestroyObjectHighLevel(mat);
}

static void IsPropertyPathOverriddenTest ()
{
	Assert(IsPropertyPathOverridden ("test.hallo", "test"));
	Assert(IsPropertyPathOverridden ("test.hallo", "test.hallo"));
	Assert(!IsPropertyPathOverridden ("test.hallo", "test."));
	Assert(!IsPropertyPathOverridden ("test.hallo", "tes"));
	Assert(!IsPropertyPathOverridden ("test.hallo", "test.hall"));
}

void TestPropertyModification ()
{
	TestPropertyModificationGameObject();
	TestPropertyModificationCamera();
	TestPropertyModificationPPtrArray ();
	TestPropertyModificationPPtrArrayRemove();
	IsPropertyPathOverriddenTest ();
}

 
