#include "UnityPrefix.h"
#include "AnimationModeSnapshot.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Editor/Src/Prefabs/BatchApplyPropertyModification.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Animation/GenericAnimationBindingCache.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/Animation/PropertyModificationToEditorCurveBinding.h"
#include "Runtime/Animation/Animator.h"

AnimationModeSnapshot::AnimationModeSnapshot ()
{
	m_AnimationMode = false;
	REGISTER_GLOBAL_CALLBACK (willSaveScene, GetAnimationModeSnapshot().StopAnimationMode ())
}

void AnimationModeSnapshot::StartAnimationMode ()
{
	m_AnimationMode = true;
}

void AnimationModeSnapshot::StopAnimationMode ()
{
	if (!m_AnimationMode)
		return;
	
	m_AnimationMode = false;
	RevertAnimatedProperties();
	
	GetApplication().RequestRepaintAllViews ();
}


void AnimationModeSnapshot::RevertAnimatedProperties ()
{
	BatchApplyPropertyModification batch;
	for (int i=0;i<m_Properties.size();i++)
		batch.Apply (m_Properties[i].value, m_Properties[i].hasPrefabOverride);
	batch.Complete();
	
	m_Properties.clear();
}

void GetAllCurveBindings (AnimationClip& clip, std::vector<EditorCurveBinding>& bindings)
{
	const AnimationClip::FloatCurves& editorCurves = clip.GetEditorCurvesSync();
	const AnimationClip::PPtrCurves& pptrCurves = clip.GetEditorPPtrCurves();
	
	bindings.resize (editorCurves.size() + pptrCurves.size());
	int b = 0;
	for (int i=0;i<editorCurves.size();i++,b++)
	{
		const AnimationClip::FloatCurve& curve = editorCurves[i];
		bindings[b] = EditorCurveBinding (curve.path, curve.classID, curve.script, curve.attribute, false);
	}

	for (int i=0;i<pptrCurves.size();i++,b++)
	{
		const AnimationClip::PPtrCurve& curve = pptrCurves[i];
		bindings[b] = EditorCurveBinding (curve.path, curve.classID, curve.script, curve.attribute, true);
	}
	
}

static bool ExtractPropertyModification (Unity::GameObject& root, const EditorCurveBinding& binding, AnimationModeSnapshot::RecordedProperty& outputProperty)
{
	EditorCurveBindingToPropertyModification (root, binding, outputProperty.value);
	Object* targetObject = outputProperty.value.target;
	if (targetObject == NULL)
		return false;
	
	outputProperty.hasPrefabOverride = HasPrefabOverride (targetObject, outputProperty.value.propertyPath);
	const TypeTree& typeTree = GenerateCachedTypeTree (*targetObject, kSerializeForPrefabSystem);

	dynamic_array<UInt8> buffer(kMemTempAlloc);
	WriteObjectToVector(*targetObject, &buffer, kSerializeForPrefabSystem);

	return ExtractCurrentValueOfAllModifications (targetObject, typeTree, buffer, &outputProperty.value, 1);
}

void AnimationModeSnapshot::BeginSampling ()
{
	for (int i=0;i<m_Properties.size();i++)
		m_Properties[i].isInUse = false;
}

void AnimationModeSnapshot::DisableAnimator (Unity::GameObject& root)
{
	Animator* animator = root.QueryComponent(Animator);
	if (animator == NULL)
		return;
	
	const char* kAnimatorEnabledProperty = "m_Enabled";
	EditorCurveBinding binding ("", ClassID(Animator), NULL, kAnimatorEnabledProperty, false);
	AddPropertyModification (root, binding);

	animator->SetEnabled(false);
}

void AnimationModeSnapshot::AddPropertyModification (Unity::GameObject& root, const EditorCurveBinding& binding)
{
	RecordedProperty* propertyValue = AnimationModeSnapshot::AddPropertyModification (binding);
	if (propertyValue != NULL)
		ExtractPropertyModification (root, binding, *propertyValue);
}

void AnimationModeSnapshot::UpdateModifiedPropertiesForAnimationClipSampling (Unity::GameObject& root, AnimationClip& clip)
{
	Assert(m_AnimationMode);

	// In Animation mode we don't want the animator in playmode to override the animation window scrubbing.
	// So the user can scrub in playmode and when he goes out of animation mode, the animator will continue
	DisableAnimator (root);
	
	// Get all bindings we will sample for clip
	std::vector<EditorCurveBinding> bindings;
	GetAllCurveBindings (clip, bindings);
	
	// Back up all of the bindings in the clip
	for (int i=0;i<bindings.size();i++)
		AddPropertyModification (root, bindings[i]);
}

void AnimationModeSnapshot::EndSampling ()
{
	BatchApplyPropertyModification batch;

	// Revert all properties that are no longer being sampled
	for (int i=0;i<m_Properties.size(); )
	{
		if (!m_Properties[i].isInUse)
		{
			batch.Apply (m_Properties[i].value, m_Properties[i].hasPrefabOverride);
			m_Properties.erase(m_Properties.begin() + i);
		}
		else
			i++;
	}
	batch.Complete();
}

bool AnimationModeSnapshot::IsPropertyAnimated (Object* targetObject, const char* propertyPath)
{
	if (targetObject == NULL)
		return false;
	
	PPtr<Object> targetPPtr = targetObject;
	for (int i=0;i<m_Properties.size();i++)
	{
		if (targetPPtr == m_Properties[i].value.target &&
			IsPropertyPathOverridden(m_Properties[i].value.propertyPath.c_str(), propertyPath))
			return true;
	}
	
	return false;
}

void AnimationModeSnapshot::AddPropertyModification (const EditorCurveBinding& binding, const PropertyModification& modification, bool hasPrefabOverride)
{
	if (!IsInAnimationMode ())
	{
		ErrorString("AnimationModeSnapshot.AddPropertyModification may only be called in animation mode.");
		return;
	}
	
	AnimationModeSnapshot::RecordedProperty* prop = AddPropertyModification (binding);
	if (prop != NULL)
	{
		prop->hasPrefabOverride = hasPrefabOverride;
		prop->value = modification;
	}	
}


AnimationModeSnapshot::RecordedProperty* AnimationModeSnapshot::AddPropertyModification (const EditorCurveBinding& binding)
{
	// Value has already been recorded. Mark as used
	int foundIndex = FindProperty (binding);
	if (foundIndex != -1)
	{
		m_Properties[foundIndex].isInUse = true;
		return NULL;
	}
	
	RecordedProperty property;
	property.binding = binding;
	property.isInUse = true;
	m_Properties.push_back(property);
	
	return &m_Properties.back();
}

int AnimationModeSnapshot::FindProperty (const EditorCurveBinding& binding)
{
	for (int i=0;i<m_Properties.size();i++)
	{
		if (m_Properties[i].binding == binding)
			return i;
	}
	
	return -1;
}

AnimationModeSnapshot* gAnimationModeSnapshot = NULL;

AnimationModeSnapshot& GetAnimationModeSnapshot()
{
	return *gAnimationModeSnapshot;
}

static void InitializeAnimationModeSnapshot ()
{
	gAnimationModeSnapshot = UNITY_NEW_AS_ROOT(AnimationModeSnapshot, kMemAnimation, "AnimationModeSnapshot", "");
}

static void CleanupAnimationModeSnapshot ()
{
	UNITY_DELETE(gAnimationModeSnapshot, kMemAnimation);
}

static RegisterRuntimeInitializeAndCleanup s_InitializeAnimationSnapshot (InitializeAnimationModeSnapshot, CleanupAnimationModeSnapshot);