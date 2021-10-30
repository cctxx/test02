using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	static internal class AnimationWindowUtility
	{
		public static void CreateDefaultCurves (AnimationWindowState state, EditorCurveBinding[] properties)
		{
			AnimationClip clip = state.m_ActiveAnimationClip;
			GameObject root = state.m_RootGameObject;
			
			properties = RotationCurveInterpolation.ConvertRotationPropertiesToDefaultInterpolation (state.m_ActiveAnimationClip, properties);
			foreach (EditorCurveBinding prop in properties)
				state.SaveCurve(CreateDefaultCurve(clip, root, prop));
		}

		public static AnimationWindowCurve CreateDefaultCurve (AnimationClip clip, GameObject rootGameObject, EditorCurveBinding binding)
		{
			Type type = AnimationUtility.GetEditorCurveValueType(rootGameObject, binding);
			AnimationWindowCurve curve = new AnimationWindowCurve(clip, binding, type);
			
			object currentValue = GetCurrentValue (rootGameObject, binding);
			if (clip.length == 0.0F)
			{
				AddKeyframeToCurve(curve, currentValue, type, AnimationKeyTime.Time(0.0F, clip.frameRate));
			}
			else
			{
				AddKeyframeToCurve(curve, currentValue, type, AnimationKeyTime.Time(0.0F, clip.frameRate));
				AddKeyframeToCurve(curve, currentValue, type, AnimationKeyTime.Time(clip.length, clip.frameRate));
			}
			
			return curve;
		}
				
		public static bool ShouldShowAnimationWindowCurve (EditorCurveBinding curveBinding)
		{
			// We don't want to convert the w component of rotation curves to be shown in animation window
			if (curveBinding.type == typeof(Transform))
				return !curveBinding.propertyName.EndsWith (".w");
				
			return true;
		}
		
		public static bool IsNodeLeftOverCurve(AnimationWindowHierarchyNode node, GameObject rootGameObject)
		{
			if (rootGameObject == null)
				return false;

			if (node.binding != null)
				return AnimationUtility.GetEditorCurveValueType(rootGameObject, (EditorCurveBinding)node.binding) == null;

			// Go through all child nodes recursively
			foreach (var child in node.children)
				return IsNodeLeftOverCurve(child as AnimationWindowHierarchyNode, rootGameObject);

			return false;
		}

		public static void AddSelectedKeyframes (AnimationWindowState state, AnimationKeyTime time)
		{
			foreach (AnimationWindowCurve curve in state.activeCurves)
			{
				AddKeyframeToCurve (state, curve, time);
			}
		}

		public static AnimationWindowKeyframe AddKeyframeToCurve (AnimationWindowState state, AnimationWindowCurve curve, AnimationKeyTime time)
		{
			object value = AnimationWindowUtility.GetCurrentValue (state.m_RootGameObject, curve.binding);
			System.Type type = AnimationUtility.GetEditorCurveValueType (state.m_RootGameObject, curve.binding);
			AnimationWindowKeyframe keyframe = AnimationWindowUtility.AddKeyframeToCurve (curve, value, type, time);
			state.SaveCurve (curve);
			return keyframe;
		}

		public static AnimationWindowKeyframe AddKeyframeToCurve (AnimationWindowCurve curve, object value, Type type, AnimationKeyTime time)
		{
			// When there is already a key a this time
			// Make sure that only value is updated but tangents are maintained.
			AnimationWindowKeyframe previousKey = curve.FindKeyAtTime (time);
			if (previousKey != null)
			{
				previousKey.value = value;
				return previousKey;
			}

			AnimationWindowKeyframe keyframe = new AnimationWindowKeyframe ();
			keyframe.time = time.time;

			if (curve.isPPtrCurve)
			{
				keyframe.value = value;
				
				keyframe.curve = curve;
				curve.AddKeyframe (keyframe, time);
			}
			else if (type == typeof(bool) || type == typeof(float))
			{
				// Create temporary curve for getting proper tangents
				AnimationCurve animationCurve = curve.ToAnimationCurve ();
				
				Keyframe tempKey = new Keyframe (time.time, (float)value);
				if (type == typeof(bool))
				{
					CurveUtility.SetKeyTangentMode (ref tempKey , 0, TangentMode.Stepped);
					CurveUtility.SetKeyTangentMode (ref tempKey , 1, TangentMode.Stepped);
					CurveUtility.SetKeyBroken (ref tempKey, true);
					keyframe.m_TangentMode = tempKey.tangentMode;
					keyframe.m_InTangent = Mathf.Infinity;
					keyframe.m_OutTangent = Mathf.Infinity;
				}
				else
				{
					int keyIndex = animationCurve.AddKey (tempKey);
					if (keyIndex != -1)
					{
						CurveUtility.SetKeyModeFromContext (animationCurve, keyIndex);
						keyframe.m_TangentMode = animationCurve[keyIndex].tangentMode;
					}
				}
				
				keyframe.value = value;
				keyframe.curve = curve;
				curve.AddKeyframe (keyframe, time);
			}

			return keyframe;
		}

		public static AnimationWindowCurve[] FilterCurves (AnimationWindowCurve[] curves, string path, bool entireHierarchy)
		{
			List<AnimationWindowCurve> results = new List<AnimationWindowCurve> ();

			foreach (AnimationWindowCurve curve in curves)
				if (curve.path.Equals (path) || (entireHierarchy && curve.path.Contains (path)))
					results.Add (curve);

			return results.ToArray ();
		}

		public static AnimationWindowCurve[] FilterCurves (AnimationWindowCurve[] curves, string path, Type animatableObjectType)
		{
			List<AnimationWindowCurve> results = new List<AnimationWindowCurve> ();

			foreach (AnimationWindowCurve curve in curves)
				if (curve.path.Equals (path) && curve.type == animatableObjectType)
					results.Add (curve);

			return results.ToArray ();
		}
		
		public static bool IsCurveCreated (AnimationClip clip, EditorCurveBinding binding)
		{
			if (binding.isPPtrCurve)
				return AnimationUtility.GetObjectReferenceCurve (clip, binding) != null;
			else
				return AnimationUtility.GetEditorCurve (clip, binding) != null;
		}

		public static bool ContainsFloatKeyframes (List<AnimationWindowKeyframe> keyframes)
		{
			if (keyframes == null || keyframes.Count == 0)
				return false;

			foreach (var key in keyframes)
			{
				if (!key.isPPtrCurve)
					return true;
			}

			return false;
		}

		// Get curves for property or propertygroup (example: x,y,z)
		public static AnimationWindowCurve[] FilterCurves (AnimationWindowCurve[] curves, string path, Type animatableObjectType, string propertyName)
		{
			List<AnimationWindowCurve> results = new List<AnimationWindowCurve> ();

			foreach (AnimationWindowCurve curve in curves)
			{
				if (curve.path.Equals (path) && curve.type == animatableObjectType && (curve.propertyName.Equals (propertyName) || curve.propertyName.Contains (propertyName)))
					results.Add (curve);
			}
			return results.ToArray ();
		}

		// Current value of the property that rootGO + curveBinding is pointing to
		public static object GetCurrentValue (GameObject rootGameObject, EditorCurveBinding curveBinding)
		{
			if (curveBinding.isPPtrCurve)
			{
				Object value;
				AnimationUtility.GetObjectReferenceValue (rootGameObject, curveBinding, out value);
				return value;
			}
			else
			{
				float value;
				AnimationUtility.GetFloatValue (rootGameObject, curveBinding, out value);
				return value;
			}
		}
		
		// Takes raw animation curve propertyname and makes it pretty
		public static string GetPropertyDisplayName (string propertyName)
		{
			propertyName = propertyName.Replace ("m_LocalPosition", "Position");
			propertyName = propertyName.Replace ("m_LocalScale", "Scale");
			propertyName = propertyName.Replace ("m_LocalRotation", "Rotation");
			propertyName = propertyName.Replace ("localEulerAnglesBaked", "Rotation");
			propertyName = propertyName.Replace ("localEulerAngles", "Rotation");
			propertyName = propertyName.Replace ("m_Materials.Array.data", "Material Reference");

			propertyName = ObjectNames.NicifyVariableName (propertyName);

			return propertyName;
		}

		// Transform and Sprite: just show Position / Rotation / Scale / Sprite
		public static bool ShouldPrefixWithTypeName (Type animatableObjectType, string propertyName)
		{
			if (animatableObjectType == typeof(Transform))
				return false;
				
			if (animatableObjectType == typeof(SpriteRenderer) && propertyName == "m_Sprite")
				return false;
				
			return true;
		}

		public static string GetNicePropertyDisplayName(Type animatableObjectType, string propertyName)
		{
			if (ShouldPrefixWithTypeName (animatableObjectType, propertyName))
				return ObjectNames.NicifyVariableName (animatableObjectType.Name) + "." + GetPropertyDisplayName (propertyName);
			else
				return GetPropertyDisplayName (propertyName);
		}

		public static string GetNicePropertyGroupDisplayName(Type animatableObjectType, string propertyGroupName)
		{
			if (ShouldPrefixWithTypeName (animatableObjectType, propertyGroupName))
				return ObjectNames.NicifyVariableName (animatableObjectType.Name) + "." + NicifyPropertyGroupName (propertyGroupName);
			else
				return NicifyPropertyGroupName (propertyGroupName);
		}

		// Takes raw animation curve propertyname and returns a pretty groupname
		public static string NicifyPropertyGroupName (string propertyGroupName)
		{
			return GetPropertyGroupName (GetPropertyDisplayName (propertyGroupName));
		}

		// We automatically group Vector4, Vector3 and Color
		static public int GetComponentIndex (string name)
		{
			if (name.Length < 3 || name[name.Length-2] != '.')
				return -1;
			char lastCharacter = name[name.Length-1];
			switch (lastCharacter)
			{
				case 'r':
					return 0;
				case 'g':
					return 1;
				case 'b':
					return 2;
				case 'a':
					return 3;
				case 'x':
					return 0;
				case 'y':
					return 1;
				case 'z':
					return 2;
				case 'w':
					return 3;
				default:
					return -1;
			}
		}
		
		// If Vector4, Vector3 or Color, return group name instead of full name
		public static string GetPropertyGroupName(string propertyName)
		{
			if (GetComponentIndex (propertyName) != -1)
				return propertyName.Substring(0, propertyName.Length-2);
			
			return propertyName;
		}

		public static float GetNextKeyframeTime (AnimationWindowCurve[] curves, float currentTime)
		{
			float candidate = float.MaxValue;
			bool found = false;

			foreach (AnimationWindowCurve curve in curves)
			{
				foreach (AnimationWindowKeyframe keyframe in curve.m_Keyframes)
				{
					if (keyframe.time < candidate && keyframe.time > currentTime)
					{
						candidate = keyframe.time;
						found = true;
					}
				}
			}

			return found ? candidate : currentTime;
		}

		public static float GetPreviousKeyframeTime (AnimationWindowCurve[] curves, float currentTime)
		{
			float candidate = float.MinValue;
			bool found = false;

			foreach (AnimationWindowCurve curve in curves)
			{
				foreach (AnimationWindowKeyframe keyframe in curve.m_Keyframes)
				{
					if (keyframe.time > candidate && keyframe.time < currentTime)
					{
						candidate = keyframe.time;
						found = true;
					}
				}
			}

			return found ? candidate : currentTime;
		}

		public static bool GameObjectIsAnimatable (GameObject gameObject, AnimationClip animationClip)
		{
			if (gameObject == null)
				return false;
			// Object is not editable
			if ((gameObject.hideFlags & HideFlags.NotEditable) != 0)
				return false;
			// Object is a prefab - shouldn't be edited	
			if (EditorUtility.IsPersistent (gameObject))
				return false;
			// Clip is imported and shouldn't be edited
			if (animationClip != null && ((animationClip.hideFlags & HideFlags.NotEditable) != 0 || !AssetDatabase.IsOpenForEdit(animationClip)))
				return false;

			return true;
		}

		public static int GetPropertyNodeID (string path, System.Type type, string propertyName)
		{
			return (path + type.Name + propertyName).GetHashCode();
		}

		// What is the first animation component when recursing parent tree toward root
		public static Transform GetClosestAnimationComponentInParents (Transform tr)
		{
			while (true)
			{
				if (tr.animation || tr.GetComponent<Animator> ()) return tr;
				if (tr == tr.root) break;
				tr = tr.parent;
			}
			return null;
		}
	}
}
