#pragma once

#include "Runtime/BaseClasses/BaseObject.h"

typedef UInt32 BindingHash;

namespace UnityEngine
{
namespace Animation
{

struct GenericBinding
{
	BindingHash    path;
	BindingHash    attribute;
	PPtr<Object>   script;
	UInt16         classID;
	UInt8          customType;
	UInt8          isPPtrCurve;
	
	GenericBinding () : path (0), attribute(0), classID(0), customType(0), isPPtrCurve(0)
	{
		
	}
	
	
	DECLARE_SERIALIZE (GenericBinding)
};

struct AnimationClipBindingConstant
{
	dynamic_array<GenericBinding>    genericBindings;
	dynamic_array<PPtr<Object> >     pptrCurveMapping;
	
	DECLARE_SERIALIZE (AnimationClipBindingConstant)
};

template<class TransferFunc> inline
void GenericBinding::Transfer (TransferFunc& transfer)
{
	TRANSFER(path);
	TRANSFER(attribute);
	TRANSFER(script);
	TRANSFER(classID);
	TRANSFER(customType);
	TRANSFER(isPPtrCurve);
}

template<class TransferFunc> inline
void AnimationClipBindingConstant::Transfer (TransferFunc& transfer)
{
	TRANSFER(genericBindings);
	TRANSFER(pptrCurveMapping);
}

inline bool operator < (const GenericBinding& lhs, const GenericBinding& rhs)
{
	// Transform components are sorted first by attribute, then by path.
	// This is because when creating the ValueArrayConstant.
	// We want scale curves at the end. This is because scale curves are most likely to not actually be changed
	// Thus more likely to be culled away by the constant clip optimization code.
	if (lhs.classID == ClassID (Transform) && rhs.classID == ClassID (Transform))
	{
		if (lhs.attribute != rhs.attribute)
			return lhs.attribute < rhs.attribute;
			
		return lhs.path < rhs.path;
	}
	 
	// All transform bindings always come first
	int lhsClassID = lhs.classID == ClassID (Transform) ? -1 : lhs.classID;
	int rhsClassID = rhs.classID == ClassID (Transform) ? -1 : rhs.classID;

	if (lhsClassID != rhsClassID)	
		return lhsClassID < rhsClassID;
	else if (lhs.isPPtrCurve != rhs.isPPtrCurve)	
		return lhs.isPPtrCurve < rhs.isPPtrCurve;
	else if (lhs.customType != rhs.customType)	
		return lhs.customType < rhs.customType;
	else if (lhs.path != rhs.path)
		return lhs.path < rhs.path;
	else if (lhs.script != rhs.script)
		return lhs.script < rhs.script;
	else
		return lhs.attribute < rhs.attribute;
}
	
}
}
