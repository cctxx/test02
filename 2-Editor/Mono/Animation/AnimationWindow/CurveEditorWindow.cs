using System;
using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;

namespace UnityEditor
{

internal enum WrapModeFixedCurve
{
	Clamp = (int)WrapMode.ClampForever,
	Loop = (int)WrapMode.Loop,
	PingPong = (int)WrapMode.PingPong
}

[System.Serializable]
internal class CurveEditorWindow : EditorWindow
{
	//const int kToolbarHeight = 17;
	const int kPresetsHeight = 46;
	
	static CurveEditorWindow s_SharedCurveEditor;
	
	CurveEditor m_CurveEditor;
	
	AnimationCurve m_Curve;
	Color m_Color;
	
	CurvePresetsContentsForPopupWindow m_CurvePresets;
	GUIContent m_GUIContent = new GUIContent();
	
	[SerializeField]
	GUIView delegateView;
	
	public static CurveEditorWindow instance
	{
		get
		{ 
			if (!s_SharedCurveEditor)
				s_SharedCurveEditor = ScriptableObject.CreateInstance<CurveEditorWindow>();
			return s_SharedCurveEditor;
		}
	}

	public string currentPresetLibrary
	{
		get
		{
			InitCurvePresets();
			return m_CurvePresets.currentPresetLibrary;
		}
		set
		{
			InitCurvePresets();
			m_CurvePresets.currentPresetLibrary = value;
		}
	}

	public static AnimationCurve curve 
	{
		get { return CurveEditorWindow.instance.m_Curve; }
		set {
			if (value == null)
			{
				CurveEditorWindow.instance.m_Curve = null;
			}
			else
			{
				CurveEditorWindow.instance.m_Curve = value;
				CurveEditorWindow.instance.RefreshShownCurves();
			}
		}
	}
	
	public static Color color {
		get { return CurveEditorWindow.instance.m_Color; }
		set {
			CurveEditorWindow.instance.m_Color = value;
			CurveEditorWindow.instance.RefreshShownCurves();
		}
	}
	
	public static bool visible {
		get { return s_SharedCurveEditor != null; }
	}
	
	void OnEnable ()
	{
		s_SharedCurveEditor = this;
		Init(null);
		m_CurveEditor.OnEnable();
	}
	
	// Called by OnEnable to make sure the CurveEditor is not null,
	// and by Show so we get a fresh CurveEditor when the user clicks a new curve.
	void Init (CurveEditorSettings settings)
	{
		m_CurveEditor = new CurveEditor(GetCurveEditorRect(), GetCurveWrapperArray(), true);
		m_CurveEditor.curvesUpdated = UpdateCurve;
		m_CurveEditor.scaleWithWindow = true;
		m_CurveEditor.margin = 40;
		if (settings != null)
			m_CurveEditor.settings = settings;
		m_CurveEditor.settings.hTickLabelOffset = 10;
		
		m_CurveEditor.FrameSelected(true, true);
	}

	CurveLibraryType curveLibraryType 
	{
		get
		{
			if (m_CurveEditor.settings.hasUnboundedRanges)
				return CurveLibraryType.Unbounded;
			return CurveLibraryType.NormalizedZeroToOne;
		}
	}

	// Returns true if a valid normalizationRect is returned (ranges are bounded)
	bool GetNormalizationRect (out Rect normalizationRect)
	{
		normalizationRect = new Rect ();
		if (m_CurveEditor.settings.hasUnboundedRanges)
			return false;

		normalizationRect = new Rect (
			m_CurveEditor.settings.hRangeMin,
			m_CurveEditor.settings.vRangeMin, 
			m_CurveEditor.settings.hRangeMax - m_CurveEditor.settings.hRangeMin, 
			m_CurveEditor.settings.vRangeMax - m_CurveEditor.settings.vRangeMin);
		return true;
	}

	static Keyframe[] CopyAndScaleCurveKeys (Keyframe[] orgKeys, Rect rect, bool realToNormalized)
	{
		if (rect.width == 0f || rect.height == 0f || float.IsInfinity(rect.width) || float.IsInfinity(rect.height))
		{
			Debug.LogError ("CopyAndScaleCurve: Invalid scale: " + rect);
			return orgKeys;
		}

		Keyframe[] scaledKeys = new Keyframe[orgKeys.Length];
		if (realToNormalized)
		{
			for (int i = 0; i < scaledKeys.Length; ++i)
			{
				scaledKeys[i].time = (orgKeys[i].time - rect.xMin) / rect.width;
				scaledKeys[i].value = (orgKeys[i].value - rect.yMin) / rect.height;
			}
		}
		else
		{
			// From normalized to real
			for (int i = 0; i < scaledKeys.Length; ++i)
			{
				scaledKeys[i].time = orgKeys[i].time * rect.width + rect.xMin;
				scaledKeys[i].value = orgKeys[i].value * rect.height + rect.yMin;
			}
		}

		return scaledKeys;
	}

	void InitCurvePresets ()
	{
		if (m_CurvePresets == null)
		{
			// Selection callback for library window
			System.Action<AnimationCurve> presetSelectedCallback = delegate (AnimationCurve presetCurve)
			{
				ValidateCurveLibraryTypeAndScale ();

				// Scale curve up using ranges
				Rect normalizationRect;
				if (GetNormalizationRect (out normalizationRect))
				{
					bool realToNormalized = false;
					m_Curve.keys = CopyAndScaleCurveKeys (presetCurve.keys, normalizationRect, realToNormalized);
				}
				else
				{
					m_Curve.keys = presetCurve.keys;
				}

				m_Curve.postWrapMode = presetCurve.postWrapMode;
				m_Curve.preWrapMode = presetCurve.preWrapMode;

				m_CurveEditor.SelectNone();
				RefreshShownCurves();
				SendEvent ("CurveChanged", true);
			};
			
			// We set the curve to save when showing the popup to ensure to scale the current state of the curve
			AnimationCurve curveToSaveAsPreset = null;
			m_CurvePresets = new CurvePresetsContentsForPopupWindow (curveToSaveAsPreset, curveLibraryType, presetSelectedCallback);
			m_CurvePresets.InitIfNeeded ();
		}
	}

	void OnDestroy ()
	{
		m_CurvePresets.GetPresetLibraryEditor().UnloadUsedLibraries();
	}

	void OnDisable ()
	{
		m_CurveEditor.OnDisable();
		if (s_SharedCurveEditor == this)
			s_SharedCurveEditor = null;
		else
			if (!this.Equals(s_SharedCurveEditor))
				throw new ApplicationException("s_SharedCurveEditor does not equal this");
	}
	
	private void RefreshShownCurves ()
	{
		m_CurveEditor.animationCurves = GetCurveWrapperArray();
	}
	
	public void Show (GUIView viewToUpdate, CurveEditorSettings settings)
	{
		delegateView = viewToUpdate;
		Init(settings);
		ShowAuxWindow();
		title = "Curve";
		
		// deal with window size
		minSize = new Vector2(240, 240+kPresetsHeight);
		maxSize = new Vector2(10000, 10000);
	}
	
	internal class Styles
	{
		public GUIStyle curveEditorBackground = "PopupCurveEditorBackground";
		public GUIStyle miniToolbarPopup = "MiniToolbarPopup";
		public GUIStyle miniToolbarButton = "MiniToolbarButtonLeft";
		public GUIStyle curveSwatch = "PopupCurveEditorSwatch";
		public GUIStyle curveSwatchArea = "PopupCurveSwatchBackground";
		public GUIStyle curveWrapPopup = "PopupCurveDropdown";
	}
	internal static Styles ms_Styles;
	
	CurveWrapper[] GetCurveWrapperArray ()
	{
		if (m_Curve == null)
			return new CurveWrapper[] {};
		CurveWrapper cw = new CurveWrapper();
		cw.id = "Curve".GetHashCode();
		cw.groupId = -1;
		cw.color = m_Color;
		cw.hidden = false;
		cw.readOnly = false;
		cw.renderer = new NormalCurveRenderer(m_Curve);
		cw.renderer.SetWrap(m_Curve.preWrapMode, m_Curve.postWrapMode);
		return new CurveWrapper[] { cw };
	}
	
	Rect GetCurveEditorRect ()
	{
		//return new Rect(0, kToolbarHeight, position.width, position.height-kToolbarHeight);
		return new Rect(0, 0, position.width, position.height-kPresetsHeight);
	}
	
	static internal Keyframe[] GetLinearKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0,0,1,1);
		keys[1] = new Keyframe(1,1,1,1);
		
		for (int i=0; i<2; i++)
		{
			CurveUtility.SetKeyBroken(ref keys[i], false);
			CurveUtility.SetKeyTangentMode(ref keys[i], 0, TangentMode.Smooth);
			CurveUtility.SetKeyTangentMode(ref keys[i], 1, TangentMode.Smooth);
		}
		
		return keys;
	}

	static internal Keyframe[] GetLinearMirrorKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0,1,-1,-1);
		keys[1] = new Keyframe(1,0,-1,-1);
		
		for (int i=0; i<2; i++)
		{
			CurveUtility.SetKeyBroken(ref keys[i], false);
			CurveUtility.SetKeyTangentMode(ref keys[i], 0, TangentMode.Smooth);
			CurveUtility.SetKeyTangentMode(ref keys[i], 1, TangentMode.Smooth);
		}
		
		return keys;
	}


	static internal Keyframe[] GetEaseInKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0,0,0,0);
		keys[1] = new Keyframe(1,1,2,2);
		SetSmoothEditable(ref keys);
		return keys;
	}

	static internal Keyframe[] GetEaseInMirrorKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0, 1, -2, -2);
		keys[1] = new Keyframe(1, 0, 0, 0);
		SetSmoothEditable(ref keys);
		return keys;
	}

	static internal Keyframe[] GetEaseOutKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0,0,2,2);
		keys[1] = new Keyframe(1,1,0,0);
		SetSmoothEditable(ref keys);		
		return keys;
	}

	static internal Keyframe[] GetEaseOutMirrorKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0,1,0,0);
		keys[1] = new Keyframe(1,0,-2,-2);
		SetSmoothEditable(ref keys);		
		return keys;
	}

	static internal Keyframe[] GetEaseInOutKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0,0,0,0);
		keys[1] = new Keyframe(1,1,0,0);
		SetSmoothEditable(ref keys);		
		return keys;
	}

	static internal Keyframe[] GetEaseInOutMirrorKeys()
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0, 1, 0, 0);
		keys[1] = new Keyframe(1, 0, 0, 0);
		SetSmoothEditable(ref keys);
		return keys;
	}

	static internal Keyframe[] GetConstantKeys(float value)
	{
		Keyframe[] keys = new Keyframe[2];
		keys[0] = new Keyframe(0, value, 0, 0);
		keys[1] = new Keyframe(1, value, 0, 0);
		SetSmoothEditable(ref keys);
		return keys;
	}

	static internal void SetSmoothEditable(ref Keyframe[] keys)
	{
		for (int i=0; i<keys.Length; i++)
		{
			CurveUtility.SetKeyBroken(ref keys[i], false);
			CurveUtility.SetKeyTangentMode(ref keys[i], 0, TangentMode.Editable);
			CurveUtility.SetKeyTangentMode(ref keys[i], 1, TangentMode.Editable);
		}
	}

	void OnGUI ()
	{
		bool gotMouseUp = (Event.current.type == EventType.mouseUp);

		if (delegateView == null)
			m_Curve = null;
		
		if (ms_Styles == null)
			ms_Styles = new Styles();
				
		// Curve Editor
		m_CurveEditor.rect = GetCurveEditorRect();
		m_CurveEditor.hRangeLocked = Event.current.shift;
		m_CurveEditor.vRangeLocked = EditorGUI.actionKey;
		
		GUI.changed = false;
		
		GUI.Label (m_CurveEditor.drawRect, GUIContent.none, ms_Styles.curveEditorBackground);
		m_CurveEditor.BeginViewGUI();
		m_CurveEditor.GridGUI();
		m_CurveEditor.CurveGUI();
		DoWrapperPopups();
		m_CurveEditor.EndViewGUI();
		
		// Preset swatch area
		GUI.Box(new Rect(0, position.height-kPresetsHeight, position.width, kPresetsHeight), "", ms_Styles.curveSwatchArea);
		Color curveColor = m_Color;
		curveColor.a *= 0.6f;
		const float margin = 45f;
		const float width = 40f;
		const float height = 25f;
		float yPos = position.height- kPresetsHeight + (kPresetsHeight-height)*0.5f;
		InitCurvePresets ();
		CurvePresetLibrary curveLibrary = m_CurvePresets.GetPresetLibraryEditor ().GetCurrentLib();
		if (curveLibrary != null)
		{
			for (int i = 0; i < curveLibrary.Count(); i++)
			{
				Rect swatchRect = new Rect(margin + (width + 5f) * i, yPos, width, height);
				m_GUIContent.tooltip = curveLibrary.GetName(i);
				if (GUI.Button(swatchRect, m_GUIContent, ms_Styles.curveSwatch))
				{
					AnimationCurve animCurve = curveLibrary.GetPreset(i) as AnimationCurve;
					m_Curve.keys = animCurve.keys;
					m_Curve.postWrapMode = animCurve.postWrapMode;
					m_Curve.preWrapMode = animCurve.preWrapMode;
					m_CurveEditor.SelectNone();
					SendEvent("CurveChanged", true);
				}
				if (Event.current.type == EventType.repaint)
					curveLibrary.Draw(swatchRect, i);

				if (swatchRect.xMax > position.width - 2 * margin)
					break;
			}
		}

		Rect presetDropDownButtonRect = new Rect(margin - 20f, yPos + 5f, 20, 20);
		PresetDropDown (presetDropDownButtonRect);

		// For adding default preset curves
		//if (EditorGUI.ButtonMouseDown(new Rect (position.width -26, yPos, 20, 20), GUIContent.none, FocusType.Passive, "OL Plus"))
		//	AddDefaultPresetsToCurrentLib ();

		if (Event.current.type == EventType.used && gotMouseUp)
		{
			DoUpdateCurve(false);
			SendEvent("CurveChangeCompleted", true);
		}
		else if (Event.current.type != EventType.Layout && Event.current.type != EventType.Repaint)
		{
			DoUpdateCurve(true);
		}
	}

	void PresetDropDown(Rect rect)
	{
		if (EditorGUI.ButtonMouseDown (rect, EditorGUI.s_TitleSettingsIcon, FocusType.Native, EditorStyles.inspectorTitlebarText))
		{
			if (m_Curve != null)
			{
				if (m_CurvePresets == null)
				{
					Debug.LogError ("Curve presets error");
					return;
				}

				ValidateCurveLibraryTypeAndScale ();

				AnimationCurve copy = new AnimationCurve();
				Rect normalizationRect;
				if (GetNormalizationRect (out normalizationRect))
				{
					bool realToNormalized = true;
					copy.keys = CopyAndScaleCurveKeys (m_Curve.keys, normalizationRect, realToNormalized);
				}
				else
				{
					copy = new AnimationCurve(m_Curve.keys);
				}
				copy.postWrapMode = m_Curve.postWrapMode;
				copy.preWrapMode = m_Curve.preWrapMode;

				m_CurvePresets.curveToSaveAsPreset = copy;
				PopupWindow.Show(rect, m_CurvePresets);
			}
		}
	}

	void ValidateCurveLibraryTypeAndScale ()
	{
		Rect normalizationRect;
		if (GetNormalizationRect (out normalizationRect))
		{
			if (curveLibraryType != CurveLibraryType.NormalizedZeroToOne)
				Debug.LogError("When having a normalize rect we should be using curve library type: NormalizedZeroToOne (normalizationRect: " + normalizationRect + ")");
		}
		else
		{
			if (curveLibraryType != CurveLibraryType.Unbounded)
				Debug.LogError("When NOT having a normalize rect we should be using library type: Unbounded");
		}		
	}

	// Polynomial curves have limitations on how they have to be authored.
	// Since we don't enforce the layout, we have a button that enforces the curve layout instead.
	/*
	void OptimizePolynomialCurve (Rect rect)	
	{
		///@TODO: only show this when editing shuriken curves....
		
		bool wasEnabled = GUI.enabled;
		
		bool isValidPolynomialCurve = true;
		for (int i=0;i<m_CurveEditor.animationCurves.Length;i++)
			isValidPolynomialCurve &= AnimationUtility.IsValidPolynomialCurve(m_CurveEditor.animationCurves[i].curve);
		
		GUI.enabled = !isValidPolynomialCurve;
		if (GUI.Button (rect, "Optimize Polynomial Curve"))
		{
			for (int i=0;i<m_CurveEditor.animationCurves.Length;i++)
				AnimationUtility.ConstrainToPolynomialCurve(m_CurveEditor.animationCurves[i].curve);

			m_CurveEditor.SelectNone();
			SendEvent ("CurveChanged", true);
		}
			
		GUI.enabled = wasEnabled;
	}*/
	
	public void UpdateCurve ()
	{
		DoUpdateCurve(false);
	}
	
	private void DoUpdateCurve (bool exitGUI)
	{
		if (m_CurveEditor.animationCurves.Length > 0
			&& m_CurveEditor.animationCurves[0] != null
			&& m_CurveEditor.animationCurves[0].changed)
		{
			m_CurveEditor.animationCurves[0].changed = false;
			SendEvent ("CurveChanged", exitGUI);
		}
	}
	
	void DoWrapperPopups ()
	{
		if (m_Curve != null && m_Curve.length >= 2)
		{
			Color oldText = GUI.contentColor;
			GUI.contentColor = Color.white;
			float buttonWidth = 60;
			
			WrapMode newWrap;
			Keyframe key = m_Curve.keys[0];
			Vector3 pos = new Vector3(key.time, key.value);
			pos = m_CurveEditor.DrawingToViewTransformPoint(pos);
			Rect r = new Rect(pos.x-buttonWidth-6, pos.y, buttonWidth, 17);
			newWrap = (m_Curve != null ? m_Curve.preWrapMode : WrapMode.Default);
			newWrap = (WrapMode)EditorGUI.EnumPopup(r, (WrapModeFixedCurve)newWrap, ms_Styles.curveWrapPopup);
			if (m_Curve != null && newWrap != m_Curve.preWrapMode)
			{
				m_Curve.preWrapMode = newWrap;
				RefreshShownCurves();
				SendEvent ("CurveChanged", true);
			}
			key = m_Curve.keys[m_Curve.length-1];
			pos = new Vector3(key.time, key.value);
			pos = m_CurveEditor.DrawingToViewTransformPoint(pos);
			r = new Rect(pos.x+6, pos.y, buttonWidth, 17);
			newWrap = (m_Curve != null ? m_Curve.postWrapMode : WrapMode.Default);
			newWrap = (WrapMode)EditorGUI.EnumPopup(r, (WrapModeFixedCurve)newWrap, ms_Styles.curveWrapPopup);
			if (m_Curve != null && newWrap != m_Curve.postWrapMode)
			{
				m_Curve.postWrapMode = newWrap;
				RefreshShownCurves();
				SendEvent ("CurveChanged", true);
			}
			
			GUI.contentColor = oldText;
		}
	}
	
	void SendEvent (string eventName, bool exitGUI) {
		if (delegateView) {
			Event e = EditorGUIUtility.CommandEvent (eventName);
			Repaint();
			delegateView.SendEvent (e);
			if (exitGUI)
				GUIUtility.ExitGUI ();
		}
		GUI.changed = true;
	}
}

}
