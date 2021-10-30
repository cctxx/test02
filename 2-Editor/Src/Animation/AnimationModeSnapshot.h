#pragma once

#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Runtime/Animation/EditorCurveBinding.h"

namespace Unity { class GameObject;  }
class AnimationClip;

class AnimationModeSnapshot
{
public:
	
	AnimationModeSnapshot ();
	
	/// Between BeginAnimationSampling & EndAnimationSampling
	/// you can call UpdateModifiedPropertiesForAnimationClipSampling multiple times.
	/// UpdateModifiedPropertiesForAnimationClipSampling adds properties that currently do not have a backup to the backup.
	/// EndAnimationSampling Reverts properties to the stored backup value if it is no longer animated.
	void BeginSampling ();
	void UpdateModifiedPropertiesForAnimationClipSampling (Unity::GameObject& gameObject, AnimationClip& clip);
	void DisableAnimator (Unity::GameObject& root);
	
	void EndSampling ();
	
	/// Reverts all animated properties to their values before animation sampling
	void RevertAnimatedProperties ();
	
	// Allows you to add a modified property 
	void AddPropertyModification (const EditorCurveBinding& binding, const PropertyModification& modification, bool hasPrefabOverride);
	
	/// Returns true if the value is currently being driven by animation
	bool IsPropertyAnimated (Object* targetObject, const char* propertyPath);
	
	void StartAnimationMode ();
	void StopAnimationMode ();
	bool IsInAnimationMode () const			{ return m_AnimationMode; }

	struct RecordedProperty
	{
		bool					isInUse;
		EditorCurveBinding		binding;
		bool					hasPrefabOverride;
		PropertyModification	value;
	};
	
private:
	
	void				AddPropertyModification (Unity::GameObject& root, const EditorCurveBinding& binding);
	RecordedProperty*	AddPropertyModification  (const EditorCurveBinding& binding);
	int					FindProperty (const EditorCurveBinding& binding);
	
	typedef std::vector<RecordedProperty> Properties;
	Properties	m_Properties;
	bool		m_AnimationMode;
};
AnimationModeSnapshot& GetAnimationModeSnapshot();

