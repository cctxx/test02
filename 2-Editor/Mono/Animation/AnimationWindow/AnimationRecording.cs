using System;
using System.Reflection;
using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	internal class AnimationRecording
	{
		static bool HasAnyRecordableModifications(GameObject root, UndoPropertyModification[] modifications)
		{
			for (int i=0;i<modifications.Length;i++)
			{
				EditorCurveBinding tempBinding;
				if (AnimationUtility.PropertyModificationToEditorCurveBinding (modifications[i].propertyModification, root, out tempBinding) != null)
					return true;
			}
			return false;
		}

		static PropertyModification FindPropertyModification (GameObject root, UndoPropertyModification[] modifications, EditorCurveBinding binding)
		{
			for (int i=0;i<modifications.Length;i++)
			{
				EditorCurveBinding tempBinding;
				AnimationUtility.PropertyModificationToEditorCurveBinding (modifications[i].propertyModification, root, out tempBinding);
				if (tempBinding == binding)
					return modifications[i].propertyModification;
			}
		
			return null;
		}

		static public UndoPropertyModification[] Process(AnimationWindowState state, UndoPropertyModification[] modifications)
		{
			GameObject root = state.m_RootGameObject;
			AnimationClip clip = state.m_ActiveAnimationClip;
            Animator animator = root.GetComponent<Animator>();
			
			// Fast path detect that nothing recordable has changed.
			if (!HasAnyRecordableModifications (root, modifications))
				return modifications;
			
			// Record modified properties
			List<UndoPropertyModification> nonAnimationModifications = new List<UndoPropertyModification>();
			for (int i=0;i<modifications.Length;i++)
			{
				EditorCurveBinding binding = new EditorCurveBinding ();
				
				PropertyModification prop = modifications[i].propertyModification;
				Type type = AnimationUtility.PropertyModificationToEditorCurveBinding (prop, root, out binding);
				if (type != null)
				{
					// TODO@mecanim keyframing of transform driven by an animator is disabled until we can figure out
					// how to rebind partially a controller at each sampling.
					if (animator != null && animator.isHuman && binding.type == typeof(Transform) && animator.IsBoneTransform(prop.target as Transform))
					{
						Debug.LogWarning("Keyframing for humanoid rig is not supported!", prop.target as Transform);
						continue;
					}

					AnimationMode.AddPropertyModification (binding, prop, modifications[i].keepPrefabOverride);

					EditorCurveBinding[] additionalBindings = RotationCurveInterpolation.RemapAnimationBindingForAddKey (binding, clip);
					if (additionalBindings != null)
					{
						//@TOOD: Somehow remap so that we can have a good zero key for rotation too...
						
						for (int a=0;a<additionalBindings.Length;a++)
							AddKey (state, additionalBindings[a], type, FindPropertyModification(root, modifications, additionalBindings[a]));
					}
					else
					{
						AddKey (state, binding, type, prop);
					}
				}
				else
				{
					// All other modifications will be allowed on the undo stack.
					nonAnimationModifications.Add(modifications[i]);
				}
			}
			
			return nonAnimationModifications.ToArray();
		}
		
		static bool ValueFromPropertyModification (PropertyModification modification, EditorCurveBinding binding, out object outObject)
		{
			if (modification == null)
			{
				outObject = null;
				return false;
			}
			else if (binding.isPPtrCurve)
			{
				outObject = modification.objectReference;
				return true;
			}
			else
			{
				float temp;
				if (float.TryParse(modification.value, out temp))
				{
					outObject = temp;
					return true;
				}
				else
				{
					outObject = null;
					return false;
				}
			}	
		}
		
		static void AddKey (AnimationWindowState state, EditorCurveBinding binding, Type type, PropertyModification modification)
		{
			GameObject root = state.m_RootGameObject;
			AnimationClip clip = state.m_ActiveAnimationClip;

			AnimationWindowCurve curve = new AnimationWindowCurve(clip, binding, type);

			// Add key at current frame
			object currentValue = AnimationWindowUtility.GetCurrentValue(root, binding);
			
			// When creating a new curve, Add zero key at value before it was edited.
			object oldValue = null;
			if (curve.length == 0 && state.m_Frame != 0)
			{
				if (!ValueFromPropertyModification (modification, binding, out oldValue))
					oldValue = currentValue;
				
				AnimationWindowUtility.AddKeyframeToCurve (curve, oldValue, type, AnimationKeyTime.Frame (0, clip.frameRate));
			}
			
			AnimationWindowUtility.AddKeyframeToCurve (curve, currentValue, type, AnimationKeyTime.Frame (state.m_Frame, clip.frameRate));
			
			state.SaveCurve (curve);
		}
	}
}
