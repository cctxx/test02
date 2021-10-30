#include "UnityPrefix.h"
#include "Runtime/Animation/GenericAnimationBindingCache.h"
#include "Runtime/Animation/AnimationClipBindings.h"
#include "Runtime/Animation/BoundCurve.h"
#include "MonoBehaviour.h"
#include "Runtime/Interfaces/IAnimationBinding.h"
#include "MonoScript.h"

static const char* kEnabledStr = "m_Enabled";

class MonoBehaviourPropertyBinding : public IAnimationBinding
{
public:
	
#if UNITY_EDITOR
	virtual void GetAllAnimatableProperties (Object& targetObject, std::vector<EditorCurveBinding>& outProperties) const
	{
		MonoBehaviour* beh = reinterpret_cast<MonoBehaviour *> (&targetObject);
		MonoScript& script = *beh->GetScript ();

		outProperties.push_back(EditorCurveBinding ("", ClassID(MonoBehaviour), &script, kEnabledStr, false));
	}
#endif
	
	virtual float GetFloatValue (const UnityEngine::Animation::BoundCurve& bind) const
	{
		MonoBehaviour* mono = reinterpret_cast<MonoBehaviour*>(bind.targetObject);

		return UnityEngine::Animation::AnimationBoolToFloat(mono->GetEnabled());
	}
	
	virtual void SetFloatValue (const UnityEngine::Animation::BoundCurve& bind, float value) const
	{
		MonoBehaviour* mono = reinterpret_cast<MonoBehaviour*>(bind.targetObject);

		mono->SetEnabled(UnityEngine::Animation::AnimationFloatToBool(value));
	}
	
	virtual void SetPPtrValue (const UnityEngine::Animation::BoundCurve& bound, SInt32 value) const { }
	
	virtual SInt32 GetPPtrValue (const UnityEngine::Animation::BoundCurve& bound) const	{ return 0;}
	
	virtual bool GenerateBinding (const UnityStr& attribute, bool pptrCurve, UnityEngine::Animation::GenericBinding& outputBinding) const
	{
		return attribute == kEnabledStr && !pptrCurve;
	}
	
	virtual ClassIDType BindValue (Object& target, const UnityEngine::Animation::GenericBinding& inputBinding, UnityEngine::Animation::BoundCurve& bound) const
	{
		return ClassID(bool);
	}
};

static MonoBehaviourPropertyBinding* gBinding = NULL;

void InitializeMonoBehaviourAnimationBindingInterface ()
{
	gBinding = UNITY_NEW (MonoBehaviourPropertyBinding, kMemAnimation);
	UnityEngine::Animation::GetGenericAnimationBindingCache ().RegisterIAnimationBinding (ClassID(MonoBehaviour), UnityEngine::Animation::kMonoBehaviourPropertyBinding, gBinding);
}

void CleanupMonoBehaviourAnimationBindingInterface ()
{
	UNITY_DELETE (gBinding, kMemAnimation);
}

