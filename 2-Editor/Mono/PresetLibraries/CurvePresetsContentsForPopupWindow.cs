using System;
using System.Collections.Generic;
using UnityEditorInternal;
using UnityEngine;

namespace UnityEditor
{

internal enum CurveLibraryType
{
	Unbounded,
	NormalizedZeroToOne
}


internal class CurvePresetsContentsForPopupWindow : IPopupWindowContent
{
	PresetLibraryEditor<CurvePresetLibrary> m_CurveLibraryEditor;
	PresetLibraryEditorState m_CurveLibraryEditorState;
	AnimationCurve m_Curve;
	CurveLibraryType m_CurveLibraryType;
	bool m_WantsToClose = false;
	System.Action<AnimationCurve> m_PresetSelectedCallback;

	public AnimationCurve curveToSaveAsPreset {get{return m_Curve;} set{m_Curve = value;}}

	public CurvePresetsContentsForPopupWindow (AnimationCurve animCurve, CurveLibraryType curveLibraryType, System.Action<AnimationCurve> presetSelectedCallback)
	{
		m_CurveLibraryType = curveLibraryType;
		m_Curve = animCurve;
		m_PresetSelectedCallback = presetSelectedCallback;
	}

	public static string GetBasePrefText (CurveLibraryType curveLibraryType)
	{
		return GetExtension(curveLibraryType);
	}

	public string currentPresetLibrary
	{
		get
		{
			InitIfNeeded();
			return m_CurveLibraryEditor.currentLibraryWithoutExtension;
		}
		set
		{
			InitIfNeeded();
			m_CurveLibraryEditor.currentLibraryWithoutExtension = value;
		}
	}

	static string GetExtension(CurveLibraryType curveLibraryType)
	{
		switch (curveLibraryType)
		{
			case CurveLibraryType.NormalizedZeroToOne: return PresetLibraryLocations.GetCurveLibraryExtension(true);
			case CurveLibraryType.Unbounded: return PresetLibraryLocations.GetCurveLibraryExtension(false);
			default:
				Debug.LogError("Enum not handled!");
				return "curves";
		}
	}
	
	public void OnDisable ()
	{
		m_CurveLibraryEditorState.TransferEditorPrefsState (false);
	}

	public PresetLibraryEditor<CurvePresetLibrary> GetPresetLibraryEditor ()
	{
		return m_CurveLibraryEditor;
	}

	public void InitIfNeeded ()
	{
		if (m_CurveLibraryEditorState == null)
		{
			m_CurveLibraryEditorState = new PresetLibraryEditorState  (GetBasePrefText(m_CurveLibraryType));
			m_CurveLibraryEditorState.TransferEditorPrefsState (true);
		}
		
		if (m_CurveLibraryEditor == null)
		{
			var saveLoadHelper = new ScriptableObjectSaveLoadHelper<CurvePresetLibrary> (GetExtension (m_CurveLibraryType), SaveType.Text);
			m_CurveLibraryEditor = new PresetLibraryEditor <CurvePresetLibrary> (saveLoadHelper, m_CurveLibraryEditorState, ItemClickedCallback);
			m_CurveLibraryEditor.defaultLibraryWasCreated += OnDefaultLibraryWasCreated;
			m_CurveLibraryEditor.presetsWasReordered += OnPresetsWasReordered;
			m_CurveLibraryEditor.previewAspect = 4f;
			m_CurveLibraryEditor.minMaxPreviewHeight = new Vector2(24f, 24f);
			m_CurveLibraryEditor.showHeader = true;
		}
	}

	void OnPresetsWasReordered()
	{
		InternalEditorUtility.RepaintAllViews();
	}

	public void OnGUI (EditorWindow caller, Rect rect)
	{
		InitIfNeeded ();

		m_CurveLibraryEditor.OnGUI (rect, m_Curve);

		if (m_WantsToClose)
			caller.Close ();
	}


	void ItemClickedCallback(int clickCount, object presetObject)
	{
		AnimationCurve curve = presetObject as AnimationCurve;
		if (curve == null)
			Debug.LogError("Incorrect object passed " + presetObject);

		m_PresetSelectedCallback (curve);
	}

	public Vector2 GetWindowSize()
	{
		return new Vector2(240,330);
	}

	void OnDefaultLibraryWasCreated(PresetLibrary defaultPresetLibrary)
	{
		CurvePresetLibrary curveDefaultLib = defaultPresetLibrary as CurvePresetLibrary;
		if (curveDefaultLib == null)
		{
			Debug.Log("Incorrect preset library, should be a CurvePresetLibrary but was a " + defaultPresetLibrary.GetType());
			return;
		}

		List<AnimationCurve> defaults = new List<AnimationCurve>();
		defaults.Add(new AnimationCurve(CurveEditorWindow.GetConstantKeys(1f)));
		defaults.Add(new AnimationCurve(CurveEditorWindow.GetLinearKeys()));
		defaults.Add(new AnimationCurve(CurveEditorWindow.GetEaseInKeys()));
		defaults.Add(new AnimationCurve(CurveEditorWindow.GetEaseOutKeys()));
		defaults.Add(new AnimationCurve(CurveEditorWindow.GetEaseInOutKeys()));

		foreach (AnimationCurve preset in defaults)
			curveDefaultLib.Add(preset, "");
	}
}

}
