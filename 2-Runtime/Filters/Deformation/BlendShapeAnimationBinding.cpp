#include "UnityPrefix.h"
#include "Runtime/Animation/GenericAnimationBindingCache.h"
#include "Runtime/Animation/BoundCurve.h"
#include "SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/MeshBlendShape.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Interfaces/IAnimationBinding.h"
#include "Runtime/mecanim/generic/crc32.h"

const char* kBlendShapePrefix = "blendShape.";
const char* kBlendShapeSerializedPropertyPath = "m_BlendShapeWeights.Array.data[";

static int BlendShapeNameToIndex (SkinnedMeshRenderer& renderer, BindingHash hash)
{
	const Mesh* mesh = renderer.GetMesh();
	if (mesh == NULL)
		return -1;
	
	const BlendShapeData& blendShapes = mesh->GetBlendShapeData();
	int index = GetChannelIndex (blendShapes, hash);
	
	return index;
}

static std::string BlendShapeIndexToName (Object& targetObject, int index)
{
	SkinnedMeshRenderer* renderer = dynamic_pptr_cast<SkinnedMeshRenderer*> (&targetObject);
	
	if (renderer == NULL)
		return std::string();
	const Mesh* mesh = renderer->GetMesh();
	if (mesh == NULL)
		return std::string();
	
	const BlendShapeData& blendShapeData = mesh->GetBlendShapeData();
	if (index < mesh->GetBlendShapeChannelCount ())
		return std::string(kBlendShapePrefix) + GetChannelName (blendShapeData, index);
	else
		return std::string();
}

static bool CalculateBlendShapeHash (const char* name, BindingHash& hash)
{
	// Special case support for blendshape weights. We have to call a function.
	const char* shapeName = ParsePrefixedName (name, kBlendShapePrefix);
	if (shapeName == NULL)
		return false;
	
	hash = mecanim::processCRC32(shapeName);
	return true;
}

class BlendshapePropertyBinding : public IAnimationBinding
{
#if UNITY_EDITOR
	virtual void GetAllAnimatableProperties (Object& targetObject, std::vector<EditorCurveBinding>& outProperties) const
	{
		SkinnedMeshRenderer& renderer = static_cast<SkinnedMeshRenderer&> (targetObject);
		Mesh* mesh = renderer.GetMesh();
		if (mesh == NULL)
			return;
		
		for (int i = 0; i < mesh->GetBlendShapeChannelCount (); ++i)
			AddBinding (outProperties, ClassID(SkinnedMeshRenderer), BlendShapeIndexToName (targetObject, i));
	}
#endif
	
	virtual float GetFloatValue (const UnityEngine::Animation::BoundCurve& bind) const
	{
		SkinnedMeshRenderer* renderer = reinterpret_cast<SkinnedMeshRenderer*>(bind.targetObject);
		const int shapeIndex = reinterpret_cast<int> (bind.targetPtr);
		return renderer->GetBlendShapeWeight(shapeIndex);
	}
	
	virtual void SetFloatValue (const UnityEngine::Animation::BoundCurve& bind, float value) const
	{
		SkinnedMeshRenderer* renderer = reinterpret_cast<SkinnedMeshRenderer*>(bind.targetObject);
		
		const int shapeIndex = reinterpret_cast<int> (bind.targetPtr);
		renderer->SetBlendShapeWeight(shapeIndex, value);
	}
	
	virtual void SetPPtrValue (const UnityEngine::Animation::BoundCurve& bound, SInt32 value) const { }
	
	virtual SInt32 GetPPtrValue (const UnityEngine::Animation::BoundCurve& bound) const	{ return 0;}
	
	virtual bool GenerateBinding (const UnityStr& attribute, bool pptrCurve, UnityEngine::Animation::GenericBinding& outputBinding) const
	{
		BindingHash hash;
		if (pptrCurve || !CalculateBlendShapeHash (attribute.c_str(), hash))
			return false;
		
		outputBinding.attribute = hash;
		return true;
	}
	
	virtual ClassIDType BindValue (Object& target, const UnityEngine::Animation::GenericBinding& inputBinding, UnityEngine::Animation::BoundCurve& bound) const
	{
		int index = BlendShapeNameToIndex (static_cast<SkinnedMeshRenderer&> (target), inputBinding.attribute);
		if (index == -1)
			return ClassID(Undefined);
		
		bound.targetPtr = (void*)index;
		return ClassID(float);
	}
	
	virtual std::string SerializedPropertyPathToCurveAttribute (Object& target, const char* propertyPath) const
	{
		Assert (target.GetClassID () == ClassID(SkinnedMeshRenderer));

		if (BeginsWith (propertyPath, kBlendShapeSerializedPropertyPath))
		{
			int index = StringToInt (propertyPath + strlen(kBlendShapeSerializedPropertyPath));
			return BlendShapeIndexToName (target, index);
		}
		else
			return string();
	}
	
	virtual std::string CurveAttributeToSerializedPath (const UnityEngine::Animation::BoundCurve& bound) const
	{
		const int shapeIndex = reinterpret_cast<int> (bound.targetPtr);
		Assert (shapeIndex != -1);
		return kBlendShapeSerializedPropertyPath + IntToString(shapeIndex) + ']';
	}
	
};

static BlendshapePropertyBinding* gBinding = NULL;

void InitializeBlendShapeAnimationBindingInterface ()
{
	gBinding = UNITY_NEW (BlendshapePropertyBinding, kMemAnimation);
	UnityEngine::Animation::GetGenericAnimationBindingCache ().RegisterIAnimationBinding (ClassID(SkinnedMeshRenderer), UnityEngine::Animation::kBlendShapeWeightBinding, gBinding);
}

void CleanupBlendShapeAnimationBindingInterface ()
{
	UNITY_DELETE (gBinding, kMemAnimation);
}

