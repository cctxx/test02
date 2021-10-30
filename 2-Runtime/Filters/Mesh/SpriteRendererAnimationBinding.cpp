#include "UnityPrefix.h"
#include "Runtime/Animation/GenericAnimationBindingCache.h"
#include "Runtime/Animation/AnimationClipBindings.h"
#include "SpriteRenderer.h"
#include "Runtime/Interfaces/IAnimationBinding.h"

#if ENABLE_SPRITES

static const char* kSpriteFrame = "m_Sprite";

class SpriteRendererAnimationBinding : public IAnimationBinding
{
public:
	
#if UNITY_EDITOR
	virtual void GetAllAnimatableProperties (Object& targetObject, std::vector<EditorCurveBinding>& outProperties) const
	{
		AddPPtrBinding (outProperties, ClassID(SpriteRenderer), kSpriteFrame);
	}
#endif
	
	virtual float GetFloatValue (const UnityEngine::Animation::BoundCurve& bind) const { return 0.0F; }
	virtual void SetFloatValue (const UnityEngine::Animation::BoundCurve& bind, float value) const { }
	
	virtual void SetPPtrValue (const UnityEngine::Animation::BoundCurve& bound, SInt32 value) const
	{
		SpriteRenderer* renderer = reinterpret_cast<SpriteRenderer*>(bound.targetObject);
		renderer->SetSprite(PPtr<Sprite> (value));
	}
	
	virtual SInt32 GetPPtrValue (const UnityEngine::Animation::BoundCurve& bound) const
	{
		SpriteRenderer* renderer = reinterpret_cast<SpriteRenderer*>(bound.targetObject);
		return renderer->GetSprite().GetInstanceID();
	}
	
	virtual bool GenerateBinding (const UnityStr& attribute, bool pptrCurve, UnityEngine::Animation::GenericBinding& outputBinding) const
	{
		if (attribute == kSpriteFrame && pptrCurve)
		{
			outputBinding.attribute = 0;
			return true;
		}
		
		return false;
	}
	
	virtual ClassIDType BindValue (Object& target, const UnityEngine::Animation::GenericBinding& inputBinding, UnityEngine::Animation::BoundCurve& bound) const
	{
		return ClassID(Sprite);
	}
};

static SpriteRendererAnimationBinding* gSpriteRendererBinding = NULL;

void InitializeSpriteRendererAnimationBindingInterface ()
{
	Assert(gSpriteRendererBinding == NULL);
	gSpriteRendererBinding = UNITY_NEW (SpriteRendererAnimationBinding, kMemAnimation);
	UnityEngine::Animation::GetGenericAnimationBindingCache ().RegisterIAnimationBinding (ClassID(SpriteRenderer), UnityEngine::Animation::kSpriteRendererPPtrBinding, gSpriteRendererBinding);
}

void CleanupSpriteRendererAnimationBindingInterface ()
{
	UNITY_DELETE (gSpriteRendererBinding, kMemAnimation);
}

#endif