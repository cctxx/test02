using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor
{

internal class RotationCurveInterpolation
{
	public struct State
	{
		public bool allAreNonBaked;
		public bool allAreBaked;
		public bool allAreRotations;		
	}
	
	public static char[] kPostFix = { 'x', 'y', 'z', 'w' };
		
	public enum Mode { Baked, NonBaked, RawQuaternions, Undefined }

	public static Mode GetModeFromCurveData (EditorCurveBinding data)
	{
		if (data.type == typeof(Transform) && data.propertyName.StartsWith("localEulerAngles"))
		{
			if (data.propertyName.StartsWith("localEulerAnglesBaked"))
				return Mode.Baked;
			else
				return Mode.NonBaked;
		}
		else if (data.type == typeof(Transform) && data.propertyName.StartsWith("m_LocalRotation"))
			return Mode.RawQuaternions;

		return Mode.Undefined;
	}

	// Extracts the interpolation state for the selection from the clip
	public static State GetCurveState (AnimationClip clip, EditorCurveBinding[] selection)
	{
		State state;
		state.allAreNonBaked = true;
		state.allAreBaked = false;
		state.allAreRotations = false;

		foreach (EditorCurveBinding data in selection)
		{
			Mode mode = GetModeFromCurveData (data);
			if (mode == Mode.NonBaked)
				state.allAreNonBaked = false;
			else if (mode == Mode.Baked)
				state.allAreBaked = false;
			else
				state.allAreRotations = false;
		}
		
		return state;
	}

	public static int GetCurveIndexFromName (string name)	
	{
		return ExtractComponentCharacter (name) - 'x';
	}
			
	public static char ExtractComponentCharacter (string name)	
	{
		return name[name.Length-1];
	}
	
	public static string GetPrefixForInterpolation (Mode newInterpolationMode)
	{
		if (newInterpolationMode == Mode.Baked)
			return "localEulerAnglesBaked";
		else if (newInterpolationMode == Mode.NonBaked)
			return "localEulerAngles";
		else if (newInterpolationMode == Mode.RawQuaternions)
			return "m_LocalRotation";
		else
			return null;
	}


	internal static EditorCurveBinding[] ConvertRotationPropertiesToDefaultInterpolation (AnimationClip clip, EditorCurveBinding[] selection)
	{
		return ConvertRotationPropertiesToInterpolationType (selection, Mode.Baked);
	}

	internal static EditorCurveBinding[] ConvertRotationPropertiesToInterpolationType (EditorCurveBinding[] selection, Mode newInterpolationMode)
	{
		if (selection.Length != 4)
			return selection;
		
		if (GetModeFromCurveData (selection[0]) == Mode.RawQuaternions)
		{
			EditorCurveBinding[] newCurves = new EditorCurveBinding[3];
			newCurves[0] = selection[0];
			newCurves[1] = selection[1];
			newCurves[2] = selection[2];
			
			string prefix = GetPrefixForInterpolation(newInterpolationMode);
			newCurves[0].propertyName = prefix + ".x";
			newCurves[1].propertyName = prefix + ".y";
			newCurves[2].propertyName = prefix + ".z";
		
			return newCurves;
			
		}
		else
			return selection;
	
	}

	static EditorCurveBinding[] GenerateTransformCurveBindingArray	(string path, string property, int count)
	{
		EditorCurveBinding[] bindings = new EditorCurveBinding[count];
		for (int i=0;i<count;i++)
			bindings[i] = EditorCurveBinding.FloatCurve(path, typeof(Transform), property + kPostFix[i]);
		return bindings;
	}

	static public EditorCurveBinding[] RemapAnimationBindingForAddKey (EditorCurveBinding binding, AnimationClip clip)
	{
		if (binding.type != typeof(Transform))
			return null;
		else if (binding.propertyName.StartsWith("m_LocalPosition."))
			return GenerateTransformCurveBindingArray(binding.path, "m_LocalPosition.", 3);
		else if (binding.propertyName.StartsWith("m_LocalScale."))
			return GenerateTransformCurveBindingArray(binding.path, "m_LocalScale.", 3);
		else if (binding.propertyName.StartsWith ("m_LocalRotation"))
		{
			EditorCurveBinding testBinding = binding;
			testBinding.propertyName = "localEulerAngles.x";
			if (AnimationUtility.GetEditorCurve (clip, testBinding) != null)
				return GenerateTransformCurveBindingArray(binding.path, "localEulerAngles.", 3);
			else
				return GenerateTransformCurveBindingArray(binding.path, "localEulerAnglesBaked.", 3);
		}
		else
			return null;
	}
			
	static public EditorCurveBinding RemapAnimationBindingForRotationCurves (EditorCurveBinding curveBinding, AnimationClip clip)
	{
		if (curveBinding.type != typeof (Transform))
			return curveBinding;

		// When we encounter local rotation quaternion curves
		// We might want to actually display localEulerAngles (euler angles with quaternion interpolation) 
		// or localEulerAnglesBaked (euler angles sampled/baked into quaternion curves)
		if (curveBinding.propertyName.StartsWith ("m_LocalRotation"))
		{
			string suffix = curveBinding.propertyName.Split ('.')[1];

			EditorCurveBinding newBinding = curveBinding;
			newBinding.propertyName = "localEulerAngles." + suffix;
			AnimationCurve curve = AnimationUtility.GetEditorCurve (clip, newBinding);
			if (curve != null)
				return newBinding;
			
			newBinding.propertyName = "localEulerAnglesBaked." + suffix;
			curve = AnimationUtility.GetEditorCurve (clip, newBinding);
			if (curve != null)
				return newBinding;

			return curveBinding;
		}
		else
			return curveBinding;
	}

	internal static void SetInterpolation (AnimationClip clip, EditorCurveBinding[] curveBindings, Mode newInterpolationMode)
	{
		Undo.RegisterCompleteObjectUndo (clip, "Rotation Interpolation");

		List<EditorCurveBinding> newCurvesBindings = new List<EditorCurveBinding> ();
		List<AnimationCurve> newCurveDatas = new List<AnimationCurve> ();
		List<EditorCurveBinding> oldCurvesBindings = new List<EditorCurveBinding> ();

		foreach (EditorCurveBinding curveBinding in curveBindings)
		{
			Mode currentMode = GetModeFromCurveData(curveBinding);
			
			if (currentMode == Mode.Undefined)
				continue;
			
			if (currentMode == Mode.RawQuaternions)
			{
				Debug.LogWarning("Can't convert quaternion curve: " + curveBinding.propertyName);
				continue;
			}
			
			AnimationCurve curve = AnimationUtility.GetEditorCurve (clip, curveBinding);
			
			if (curve == null)
				continue;
			
			string newPropertyPath = GetPrefixForInterpolation (newInterpolationMode) + '.' + ExtractComponentCharacter(curveBinding.propertyName);

			EditorCurveBinding newBinding = new EditorCurveBinding ();
			newBinding.propertyName = newPropertyPath;
			newBinding.type = curveBinding.type;
			newBinding.path = curveBinding.path;
			newCurvesBindings.Add (newBinding);
			newCurveDatas.Add (curve);

			EditorCurveBinding removeCurve = new EditorCurveBinding ();	
			removeCurve.propertyName = curveBinding.propertyName;
			removeCurve.type = curveBinding.type;
			removeCurve.path = curveBinding.path;
			oldCurvesBindings.Add(removeCurve);	
		}
		
		Undo.RegisterCompleteObjectUndo(clip, "Rotation Interpolation");
		
		foreach (EditorCurveBinding binding in oldCurvesBindings)
			AnimationUtility.SetEditorCurve (clip, binding, null);

		foreach (EditorCurveBinding binding in newCurvesBindings)
			AnimationUtility.SetEditorCurve (clip, binding, newCurveDatas[newCurvesBindings.IndexOf (binding)]);
	}
	
}
}
