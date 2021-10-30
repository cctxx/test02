using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;

namespace UnityEditor
{

internal class KeyIdentifier
{
	public CurveRenderer renderer;
	public int curveId;
	public int key;
	public KeyIdentifier (CurveRenderer _renderer, int _curveId, int _keyIndex)
	{
		renderer = _renderer;
		curveId = _curveId;
		key = _keyIndex;
	}
	public AnimationCurve curve { get { return renderer.GetCurve(); } }
	public Keyframe keyframe { get { return curve[key]; } }
}

internal interface CurveUpdater
{
	void UpdateCurves (List<int> curveIds, string undoText);
}

internal class CurveMenuManager
{
	CurveUpdater updater;
	
	public CurveMenuManager (CurveUpdater updater)
	{
		this.updater = updater;
	}
	
	public void AddTangentMenuItems (GenericMenu menu, List<KeyIdentifier> keyList)
	{
		bool anyKeys = (keyList.Count > 0);
		// Find out which qualities apply to all the keys
		bool allAuto = anyKeys;
		bool allFreeSmooth = anyKeys;
		bool allFlat = anyKeys;
		bool allBroken = anyKeys;
		bool allLeftFree = anyKeys;
		bool allLeftLinear = anyKeys;
		bool allLeftConstant = anyKeys;
		bool allRightFree = anyKeys;
		bool allRightLinear = anyKeys;
		bool allRightConstant = anyKeys;
		foreach (KeyIdentifier sel in keyList)
		{
			Keyframe key = sel.keyframe;
			TangentMode leftMode = CurveUtility.GetKeyTangentMode(key, 0);
			TangentMode rightMode = CurveUtility.GetKeyTangentMode(key, 1);
			bool broken = CurveUtility.GetKeyBroken(key);
			if (leftMode != TangentMode.Smooth || rightMode != TangentMode.Smooth) allAuto = false;
			if (broken || leftMode != TangentMode.Editable || rightMode != TangentMode.Editable) allFreeSmooth = false;
			if (broken || leftMode != TangentMode.Editable || key.inTangent != 0 || rightMode != TangentMode.Editable || key.outTangent != 0) allFlat = false;
			if (!broken) allBroken = false;
			if (!broken || leftMode  != TangentMode.Editable) allLeftFree = false;
			if (!broken || leftMode  != TangentMode.Linear  ) allLeftLinear = false;
			if (!broken || leftMode  != TangentMode.Stepped ) allLeftConstant = false;
			if (!broken || rightMode != TangentMode.Editable) allRightFree = false;
			if (!broken || rightMode != TangentMode.Linear  ) allRightLinear = false;
			if (!broken || rightMode != TangentMode.Stepped ) allRightConstant = false;
		}
		if (anyKeys)
		{
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupAuto"),       allAuto, SetSmooth, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupFreeSmooth"), allFreeSmooth, SetEditable, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupFlat"),       allFlat, SetFlat, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupBroken"),     allBroken, SetBroken, keyList);
			menu.AddSeparator("");
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupLeftTangent/Free"),      allLeftFree, SetLeftEditable, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupLeftTangent/Linear"),    allLeftLinear, SetLeftLinear, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupLeftTangent/Constant"),  allLeftConstant, SetLeftConstant, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupRightTangent/Free"),     allRightFree, SetRightEditable, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupRightTangent/Linear"),   allRightLinear, SetRightLinear, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupRightTangent/Constant"), allRightConstant, SetRightConstant, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupBothTangents/Free"),     allRightFree && allLeftFree, SetBothEditable, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupBothTangents/Linear"),   allRightLinear && allLeftLinear, SetBothLinear, keyList);
			menu.AddItem(EditorGUIUtility.TextContent ("CurveKeyPopupBothTangents/Constant"), allRightConstant && allLeftConstant, SetBothConstant, keyList);
		}
		else
		{
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupAuto"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupFreeSmooth"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupFlat"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupBroken"));
			menu.AddSeparator("");
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupLeftTangent/Free"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupLeftTangent/Linear"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupLeftTangent/Constant"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupRightTangent/Free"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupRightTangent/Linear"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupRightTangent/Constant"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupBothTangents/Free"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupBothTangents/Linear"));
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("CurveKeyPopupBothTangents/Constant"));
		}
	}
	
	// Popup menu callbacks for tangents
	public void SetSmooth(object keysToSet) { SetBoth(TangentMode.Smooth, (List<KeyIdentifier>)keysToSet); }
	public void SetEditable(object keysToSet) { SetBoth(TangentMode.Editable, (List<KeyIdentifier>)keysToSet); }
	public void SetFlat(object keysToSet) { SetBoth(TangentMode.Editable, (List<KeyIdentifier>)keysToSet); Flatten((List<KeyIdentifier>)keysToSet); }
	public void SetBoth (TangentMode mode, List<KeyIdentifier> keysToSet)
	{
		List<int> curveIds = new List<int>();
		foreach (KeyIdentifier keyToSet in keysToSet)
		{
			AnimationCurve animationCurve = keyToSet.curve;
			Keyframe key = keyToSet.keyframe;
			CurveUtility.SetKeyBroken(ref key, false);
			CurveUtility.SetKeyTangentMode(ref key, 1, mode);
			CurveUtility.SetKeyTangentMode(ref key, 0, mode);
			
			// Smooth Tangents based on neighboring nodes
			// Note: not needed since the UpdateTangentsFromModeSurrounding call below will handle it
			//if (mode == TangentMode.Smooth) animationCurve.SmoothTangents(keyToSet.key, 0.0F);
			// Smooth tangents based on existing tangents
			if (mode == TangentMode.Editable)
			{
				float slope = CurveUtility.CalculateSmoothTangent(key);
				key.inTangent = slope;
				key.outTangent = slope;
			}
			animationCurve.MoveKey(keyToSet.key, key);
			CurveUtility.UpdateTangentsFromModeSurrounding(animationCurve, keyToSet.key);
			
			curveIds.Add(keyToSet.curveId);
		}
		updater.UpdateCurves(curveIds, "Set Tangents");
	}
	public void Flatten (List<KeyIdentifier> keysToSet)
	{
		List<int> curveIds = new List<int>();
		foreach (KeyIdentifier keyToSet in keysToSet)
		{
			AnimationCurve animationCurve = keyToSet.curve;
			Keyframe key = keyToSet.keyframe;
			key.inTangent = 0;
			key.outTangent = 0;
			animationCurve.MoveKey(keyToSet.key, key);
			CurveUtility.UpdateTangentsFromModeSurrounding(animationCurve, keyToSet.key);
			
			curveIds.Add(keyToSet.curveId);
		}
		updater.UpdateCurves(curveIds, "Set Tangents");
	}
	public void SetBroken(object _keysToSet)
	{
		List<KeyIdentifier> keysToSet = (List<KeyIdentifier>)_keysToSet;
		List<int> curveIds = new List<int>();
		foreach (KeyIdentifier keyToSet in keysToSet)
		{
			AnimationCurve animationCurve = keyToSet.curve;
			Keyframe key = keyToSet.keyframe;
			CurveUtility.SetKeyBroken(ref key, true);
			if (CurveUtility.GetKeyTangentMode(key, 1) == TangentMode.Smooth) CurveUtility.SetKeyTangentMode(ref key, 1, TangentMode.Editable);
			if (CurveUtility.GetKeyTangentMode(key, 0) == TangentMode.Smooth) CurveUtility.SetKeyTangentMode(ref key, 0, TangentMode.Editable);
			
			animationCurve.MoveKey(keyToSet.key, key);
			CurveUtility.UpdateTangentsFromModeSurrounding(animationCurve, keyToSet.key);
			
			curveIds.Add(keyToSet.curveId);
		}
		updater.UpdateCurves(curveIds, "Set Tangents");
	}
	public void SetLeftEditable(object keysToSet) { SetTangent(0, TangentMode.Editable, (List<KeyIdentifier>)keysToSet); }
	public void SetLeftLinear(object keysToSet) { SetTangent(0, TangentMode.Linear, (List<KeyIdentifier>)keysToSet); }
	public void SetLeftConstant(object keysToSet) { SetTangent(0, TangentMode.Stepped, (List<KeyIdentifier>)keysToSet); }
	public void SetRightEditable(object keysToSet) { SetTangent(1, TangentMode.Editable, (List<KeyIdentifier>)keysToSet); }
	public void SetRightLinear(object keysToSet) { SetTangent(1, TangentMode.Linear, (List<KeyIdentifier>)keysToSet); }
	public void SetRightConstant(object keysToSet) { SetTangent(1, TangentMode.Stepped, (List<KeyIdentifier>)keysToSet); }
	public void SetBothEditable(object keysToSet) { SetTangent(2, TangentMode.Editable, (List<KeyIdentifier>)keysToSet); }
	public void SetBothLinear(object keysToSet) { SetTangent(2, TangentMode.Linear, (List<KeyIdentifier>)keysToSet); }
	public void SetBothConstant(object keysToSet) { SetTangent(2, TangentMode.Stepped, (List<KeyIdentifier>)keysToSet); }
	public void SetTangent(int leftRight, TangentMode mode, List<KeyIdentifier> keysToSet)
	{
		List<int> curveIds = new List<int>();
		foreach (KeyIdentifier keyToSet in keysToSet)
		{
			AnimationCurve animationCurve = keyToSet.curve;
			Keyframe key = keyToSet.keyframe;
			CurveUtility.SetKeyBroken(ref key, true);
			if (leftRight == 2)
			{
				CurveUtility.SetKeyTangentMode(ref key, 0, mode);
				CurveUtility.SetKeyTangentMode(ref key, 1, mode);
			}
			else
			{
				CurveUtility.SetKeyTangentMode(ref key, leftRight, mode);
				
				// Make sure other tangent is handled correctly
				if (CurveUtility.GetKeyTangentMode(key, 1-leftRight) == TangentMode.Smooth)
					CurveUtility.SetKeyTangentMode(ref key, 1-leftRight, TangentMode.Editable);
			}
			
			if (mode == TangentMode.Stepped && (leftRight == 0 || leftRight == 2))
				key.inTangent = Mathf.Infinity;
			if (mode == TangentMode.Stepped && (leftRight == 1 || leftRight == 2))
				key.outTangent = Mathf.Infinity;
			
			animationCurve.MoveKey(keyToSet.key, key);
			CurveUtility.UpdateTangentsFromModeSurrounding(animationCurve, keyToSet.key);
			// Debug.Log ("Before " + DebKey (key) + " after: " + DebKey (animationCurve[keyToSet.key]));
			
			curveIds.Add(keyToSet.curveId);
		}
		updater.UpdateCurves(curveIds, "Set Tangents");
	}
/*	
	string DebKey (Keyframe key) {
		return System.String.Format ("time:{0} value:{1} inTangent:{2} outTangent{3}", key.time, key.value, key.inTangent, key.outTangent);
	}
*/	
}



} // namespace