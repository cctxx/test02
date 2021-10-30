#ifndef PROPERTY_MODIFICATION_H
#define PROPERTY_MODIFICATION_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/dynamic_array.h"

class TypeTree;
class dynamic_bitset;

// A single modification
struct PropertyModification
{
	PPtr<Object> target; // The object the property should be applied to
	UnityStr propertyPath; // The path where the property can be found (m_LocalPosition.x)
	UnityStr value; // The encoded value for all builtin types
	PPtr<Object> objectReference; // the serialized object reference
	
	DECLARE_SERIALIZE(PropertyModification)
	
	static bool CompareValues (const PropertyModification& lhs, const PropertyModification& rhs)
	{
		return lhs.value == rhs.value && lhs.objectReference == rhs.objectReference;
	}
	
	static bool ComparePathAndTarget(const PropertyModification& lhs, const PropertyModification& rhs)
	{
		return lhs.target == rhs.target && lhs.propertyPath == rhs.propertyPath;
	}

	static bool CompareAll (const PropertyModification& lhs, const PropertyModification& rhs)
	{
		return CompareValues (lhs, rhs) && ComparePathAndTarget(lhs, rhs);
	}
};

inline PropertyModification CreatePropertyModification (const std::string& propertyPath, const std::string& value)
{
	PropertyModification mod;
	mod.propertyPath = propertyPath;
	mod.value = value;
	return mod;
}

inline PropertyModification CreatePropertyModification (const std::string& propertyPath, const std::string& value, PPtr<Object> target)
{
	PropertyModification mod;
	mod.propertyPath = propertyPath;
	mod.value = value;
	mod.target = target;
	return mod;
}

inline PropertyModification CreatePropertyModification (const std::string& propertyPath, PPtr<Object> value)
{
	PropertyModification mod;
	mod.propertyPath = propertyPath;
	mod.objectReference = value;
	return mod;
}

typedef std::vector<PropertyModification> PropertyModifications;


template<class TransferFunction>
void PropertyModification::Transfer (TransferFunction& transfer)
{
	TRANSFER(target);
	TRANSFER(propertyPath);
	TRANSFER(value);
	TRANSFER(objectReference);
}

/// Applies a Generic PropertyModification to /data/.
/// Finds the property to modify using PropertyModification.propertyPath and replaces the value with PropertyModification.value / PropertyModification.objectReference
bool ApplyPropertyModification (const TypeTree& typeTree, dynamic_array<UInt8>& data, const PropertyModification& modification);
bool ApplyPropertyModification (const TypeTree& typeTree, dynamic_array<UInt8>& data, const PropertyModification* modifications, int nbModifications);

bool ExtractPropertyModificationValueFromBytes (const TypeTree& typeTree, const UInt8* data, PropertyModification& outPropertyModification);

struct RemapPPtrCallback
{
	virtual SInt32 Remap (SInt32 instanceID) = 0;
};

/// Compares to serialized objects (WriteObjectToVector) and generates a diff of all changes
void GeneratePropertyDiff (const TypeTree& typeTree, const dynamic_array<UInt8>& src, const dynamic_array<UInt8>& dst, RemapPPtrCallback* remapPPtr, PropertyModifications& properties);
void GeneratePropertyDiffBackwardsCompatible (const TypeTree& typeTree, const dynamic_bitset& overrides, const dynamic_array<UInt8>& dst, PropertyModifications& properties);

/// 
bool ExtractCurrentValueOfAllModifications (PPtr<Object> targetObject, const TypeTree& typeTree, dynamic_array<UInt8>& data, PropertyModification* modifications, int modificationCount);

void TestPropertyModification ();

void ApplyOrInsertPropertyModification_ArraysUnsupported (TypeTree& typeTree, dynamic_array<UInt8>& data, const std::string& dataType, const PropertyModification& modification);

bool ResizeArrayGeneric (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int bytePosition, SInt32 newArraySize, bool copyLastElement);

bool DuplicateArrayElement (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int bytePosition, SInt32 srcIndex, SInt32 dstIndex);
bool DeleteArrayElement (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& dataVector, int bytePosition, SInt32 index);

bool SetStringValue (const TypeTree& typeTree, dynamic_array<UInt8>& dataVector, int bytePosition, const std::string& stringValue);

/// Returns true if pathToDisplay is a property path leading up to propertyPath
/// Used to detect if a property in the inspector should be shown as overridden. If any child in the inspector is overridden the parent is bolded as well.
bool IsPropertyPathOverridden (const char* propertyPath, const char* pathToDisplay);

void EnsureSizePropertiesSorting (std::vector<PropertyModification>& properties);


#endif
