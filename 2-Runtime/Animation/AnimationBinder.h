#pragma once

#include "BoundCurveDeprecated.h"

class Transform;
class TypeTree;
class MonoScript;
namespace Unity { class GameObject; class Material; }

#include <string>
#include <map>
#include <vector>
#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Mono/MonoScript.h"

struct CurveID
{
	const char*   path;
	int           classID;
	const char*   attribute;
	MonoScriptPtr   script;
	unsigned      hash;

	CurveID () {}
	CurveID (const char* inPath, int inClassID, MonoScriptPtr inScript, const char* inAttribute, unsigned inHash)
	{
		path = inPath;
		attribute = inAttribute;
		classID = inClassID;
		script = inScript;
		hash = inHash;
	}
	
	friend bool operator == (const CurveID& lhs, const CurveID& rhs)
	{
		if (lhs.hash == rhs.hash && lhs.classID == rhs.classID)
		{
			int pathCompare = strcmp(lhs.path, rhs.path);
			if (pathCompare == 0)
			{
				int attributeCompare = strcmp(lhs.attribute, rhs.attribute);
				if (attributeCompare == 0)
					return lhs.script == rhs.script;
				else
					return false;
			}
			else
				return false;
		}
		return false;
	}
	
	void CalculateHash ();
};

struct hash_curve
{ 
	unsigned operator()(const CurveID& curve) const
	{
		return curve.hash;
	}
};


class AnimationBinder
{
	typedef std::map<int, TypeTree*> TypeTreeCache;
	TypeTreeCache m_TypeTreeCache;
	
	
	friend AnimationBinder& GetAnimationBinder();
	static AnimationBinder* s_Instance;

public: 
	typedef std::pair<const CurveID, unsigned> CurveIntPair;
	typedef STL_ALLOCATOR(kMemTempAlloc, CurveIntPair) TempCurveIDAllocator;
	typedef dense_hash_map<CurveID, unsigned, hash_curve, std::equal_to<CurveID>, TempCurveIDAllocator > CurveIDLookup;
	typedef dynamic_array<BoundCurveDeprecated> BoundCurves;
	typedef UNITY_VECTOR(kMemAnimation, Transform*) AffectedRootTransforms;
	
	AnimationBinder () { }
	~AnimationBinder ();

	static void StaticInitialize ();
	static void StaticDestroy ();

	bool CalculateTargetPtr(int classID, Object* targetObject, const char* attribute, void** targetPtr, int* type);

	// Builds outBoundCurves and generates all binding information
	// NOTE: If a curves can not be bound to the target objects, the entry will be removed from the lookup and will also not be in outBoundCurves
	void BindCurves (const CurveIDLookup& lookup, Transform& transform, BoundCurves& outBoundCurves, AffectedRootTransforms& affectedRootTransforms, int& transformChangedMask);
	void BindCurves (const CurveIDLookup& lookup, Unity::GameObject& rootGameObject, BoundCurves& outBoundCurves);
	
	static void RemoveUnboundCurves (CurveIDLookup& lookup, BoundCurves& outBoundCurves);
	
	static void InitCurveIDLookup (CurveIDLookup& lookup);
	static int InsertCurveIDIntoLookup (CurveIDLookup& lookup, const CurveID& curveIDLookup);
	
	// Simplified curve binding. No support for materials
	bool BindCurve (const CurveID& curveID, BoundCurveDeprecated& bound, Transform& transform);
	
	
	// Sets the value on the bound curve.
	// Does not call AwakeFromLoad or SetDirty. You can call SetValueAwakeGeneric or do it yourself.
	static bool SetFloatValue (const BoundCurveDeprecated& bind, float value);

	// Calls AwakeFromLoad or SetDirty on the target
	static void SetValueAwakeGeneric (const BoundCurveDeprecated& bind);

	static bool ShouldAwakeGeneric (const BoundCurveDeprecated& bind) { return bind.targetType == kBindFloat || bind.targetType == kBindFloatToBool; }
	
	static inline bool AnimationFloatToBool (float result)
	{
		return result > 0.001F || result < -0.001F;
	}

	static inline float AnimationBoolToFloat (bool value)
	{
		return value ? 1.0F : 0.0F;
	}
	
	#if UNITY_EDITOR

	static bool IsAnimatablePropertyOrHasAnimatableChild (const TypeTree& variable, bool isScript, Object* targetObject);
	
	#endif
};

AnimationBinder& GetAnimationBinder();
