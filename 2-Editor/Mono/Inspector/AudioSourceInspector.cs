using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditor
{
	[CustomEditor (typeof (AudioSource))]
	[CanEditMultipleObjects]
	class AudioSourceInspector : Editor
	{
		SerializedProperty m_AudioClip;
		SerializedProperty m_PlayOnAwake;
		SerializedProperty m_Volume;
		SerializedProperty m_Pitch;
		SerializedProperty m_Loop;
		SerializedProperty m_Mute;
		SerializedProperty m_Priority;
		SerializedProperty m_PanLevel;
		SerializedProperty m_DopplerLevel;
		SerializedProperty m_MinDistance;
		SerializedProperty m_MaxDistance;
		SerializedProperty m_Pan2D;
		SerializedProperty m_RolloffMode;
		SerializedProperty m_BypassEffects;
		SerializedProperty m_BypassListenerEffects;
		SerializedProperty m_BypassReverbZones;
		
		SerializedObject m_LowpassObject;
		SerializedProperty m_CutoffFrequency;
		
		class AudioCurveWrapper
		{
			public AudioCurveType type;
			public GUIContent legend;
			public int id;
			public Color color;
			public SerializedProperty curveProp;
			public int rangeMin;
			public int rangeMax;
			public AudioCurveWrapper (AudioCurveType type, string legend, int id, Color color, SerializedProperty curveProp, int rangeMin, int rangeMax)
			{
				this.type = type;
				this.legend = new GUIContent (legend);
				this.id = id;
				this.color = color;
				this.curveProp = curveProp;
				this.rangeMin = rangeMin;
				this.rangeMax = rangeMax;
			}
		}
		private AudioCurveWrapper[] m_AudioCurves;
		
		CurveEditor m_CurveEditor = null;
		Vector3 m_LastListenerPosition;
		
		const int kRolloffCurveID = 0;
		const int kPanLevelCurveID = 1;
		const int kSpreadCurveID = 2;
		const int kLowPassCurveID = 3;
		internal const float kMaxCutoffFrequency = 22000.0f;
		const float EPSILON = 0.0001f;
		
		
		static CurveEditorSettings m_CurveEditorSettings = new CurveEditorSettings ();
		internal static Color kRolloffCurveColor  = new Color (0.90f, 0.30f, 0.20f, 1.0f);
		internal static Color kPanLevelCurveColor = new Color (0.25f, 0.70f, 0.20f, 1.0f);
		internal static Color kSpreadCurveColor   = new Color (0.25f, 0.55f, 0.95f, 1.0f);
		internal static Color kLowPassCurveColor  = new Color (0.80f, 0.25f, 0.90f, 1.0f);
		
		internal bool[] m_SelectedCurves = new bool[0];
		
		private enum AudioCurveType { Volume, Pan, Lowpass, Spread }
		
		private bool m_Expanded3D = false;
		private bool m_Expanded2D = false;
		
		internal class Styles
		{
			public GUIStyle labelStyle = "ProfilerBadge";
			public GUIContent rolloffLabel =  new GUIContent("Volume Rolloff", "Which type of rolloff curve to use");
			public string controlledByCurveLabel = "Controlled by curve";
			public GUIContent panLevelLabel = new GUIContent ("Pan Level", "Sets how much the 3d position affects the pan and attenuation of the sound. If PanLevel is 0, 3D panning and attenuation is ignored.");
			public GUIContent spreadLabel = new GUIContent ("Spread", "Sets the spread of a 3d sound in speaker space.");
			
		}
		static Styles ms_Styles;
		
		
		void OnEnable () {
			m_AudioClip = serializedObject.FindProperty ("m_audioClip");
			m_PlayOnAwake = serializedObject.FindProperty ("m_PlayOnAwake");
			m_Volume = serializedObject.FindProperty ("m_Volume");
			m_Pitch = serializedObject.FindProperty ("m_Pitch");
			m_Loop = serializedObject.FindProperty ("Loop");
			m_Mute = serializedObject.FindProperty ("Mute");
			m_Priority = serializedObject.FindProperty ("Priority");
			m_DopplerLevel = serializedObject.FindProperty ("DopplerLevel");
			m_MinDistance = serializedObject.FindProperty ("MinDistance");
			m_MaxDistance = serializedObject.FindProperty ("MaxDistance");
			m_Pan2D = serializedObject.FindProperty ("Pan2D");
			m_RolloffMode = serializedObject.FindProperty ("rolloffMode");
			m_BypassEffects = serializedObject.FindProperty ("BypassEffects");
			m_BypassListenerEffects = serializedObject.FindProperty("BypassListenerEffects");
			m_BypassReverbZones = serializedObject.FindProperty("BypassReverbZones");
			
			m_AudioCurves = new AudioCurveWrapper[]
			{
				new AudioCurveWrapper (AudioCurveType.Volume, "Volume", kRolloffCurveID, kRolloffCurveColor, serializedObject.FindProperty ("rolloffCustomCurve"), 0, 1),
				new AudioCurveWrapper (AudioCurveType.Pan, "Pan", kPanLevelCurveID, kPanLevelCurveColor, serializedObject.FindProperty ("panLevelCustomCurve"), 0, 1),
				new AudioCurveWrapper (AudioCurveType.Spread, "Spread", kSpreadCurveID, kSpreadCurveColor, serializedObject.FindProperty ("spreadCustomCurve"), 0, 1),
				new AudioCurveWrapper (AudioCurveType.Lowpass, "Low-Pass", kLowPassCurveID, kLowPassCurveColor, null, 0, 1)
			};
			
			m_CurveEditorSettings.hRangeMin = 0.0f;
			m_CurveEditorSettings.vRangeMin = 0.0f;
			m_CurveEditorSettings.vRangeMax = 1.0f;
			m_CurveEditorSettings.hRangeMax = 1.0f;
			m_CurveEditorSettings.vSlider = false;
			m_CurveEditorSettings.hSlider = false;
			
			TickStyle hTS = new TickStyle ();
			hTS.color = new Color (0.0f, 0.0f, 0.0f, 0.15f);
			hTS.distLabel = 30;
			m_CurveEditorSettings.hTickStyle = hTS;
			TickStyle vTS = new TickStyle ();
			vTS.color = new Color (0.0f, 0.0f, 0.0f, 0.15f);
			vTS.distLabel = 20;
			m_CurveEditorSettings.vTickStyle = vTS;
				
			m_CurveEditor = new CurveEditor (new Rect (0, 0, 1000, 100), new CurveWrapper[0], false);
			m_CurveEditor.settings = m_CurveEditorSettings;
			m_CurveEditor.margin = 25;
			m_CurveEditor.SetShownHRangeInsideMargins (0.0f, 1.0f);
			m_CurveEditor.SetShownVRangeInsideMargins (0.0f, 1.0f);	
			m_CurveEditor.ignoreScrollWheelUntilClicked = true;
						
			m_LastListenerPosition = AudioUtil.GetListenerPos ();
			EditorApplication.update += Update;
			Undo.undoRedoPerformed += UndoRedoPerformed;			
			
			m_Expanded2D = EditorPrefs.GetBool ("AudioSourceExpanded2D", m_Expanded2D);
			m_Expanded3D = EditorPrefs.GetBool ("AudioSourceExpanded3D", m_Expanded3D);
		}
		
		void OnDisable ()
		{
			EditorApplication.update -= Update;	
			Undo.undoRedoPerformed -= UndoRedoPerformed;
			EditorPrefs.SetBool ("AudioSourceExpanded2D", m_Expanded2D);
			EditorPrefs.SetBool ("AudioSourceExpanded3D", m_Expanded3D);
		}
		
		CurveWrapper[] GetCurveWrapperArray ()
		{
			List<CurveWrapper> wrappers = new List<CurveWrapper> ();
			
			foreach (AudioCurveWrapper audioCurve in m_AudioCurves)
			{
				if (audioCurve.curveProp == null)
					continue;
				
				bool includeCurve = false;
				AnimationCurve curve = audioCurve.curveProp.animationCurveValue;
				
				// Special handling of volume rolloff curve
				if (audioCurve.type == AudioCurveType.Volume)
				{
					AudioRolloffMode mode = (AudioRolloffMode)m_RolloffMode.enumValueIndex;
					if (m_RolloffMode.hasMultipleDifferentValues)
					{
						includeCurve = false;
					}
					else if (mode == AudioRolloffMode.Custom)
					{
						includeCurve = !audioCurve.curveProp.hasMultipleDifferentValues;
					}
					else
					{
						includeCurve = !m_MinDistance.hasMultipleDifferentValues && !m_MaxDistance.hasMultipleDifferentValues;
						if (mode == AudioRolloffMode.Linear)
							curve = AnimationCurve.Linear (m_MinDistance.floatValue / m_MaxDistance.floatValue, 1.0f, 1.0f, 0.0f);
						else if (mode == AudioRolloffMode.Logarithmic)
							curve = Logarithmic (m_MinDistance.floatValue / m_MaxDistance.floatValue, 1.0f, 1.0f);
					}
				}
				// All other curves
				else
				{
					includeCurve = !audioCurve.curveProp.hasMultipleDifferentValues;
				}
				
				if (includeCurve)
				{
					if (curve.length == 0)
						Debug.LogError (audioCurve.legend.text+" curve has no keys!");
					else
						wrappers.Add (GetCurveWrapper (curve, audioCurve));
				}
			}
			
			return wrappers.ToArray ();
		}
		
		private CurveWrapper GetCurveWrapper (AnimationCurve curve, AudioCurveWrapper audioCurve)
		{
			float colorMultiplier = !EditorGUIUtility.isProSkin ? 0.9f : 1.0f;
			Color colorMult = new Color (colorMultiplier, colorMultiplier, colorMultiplier, 1);
			
			CurveWrapper wrapper = new CurveWrapper ();
			wrapper.id = audioCurve.id;
			wrapper.groupId = -1;
			wrapper.color = audioCurve.color * colorMult;
			wrapper.hidden = false;
			wrapper.readOnly = false;
			wrapper.renderer = new NormalCurveRenderer (curve);
			wrapper.renderer.SetCustomRange (0.0f, 1.0f);
			wrapper.getAxisUiScalarsCallback = GetAxisScalars;
			return wrapper;
		}
		
		// Callback for Curve Editor to get axis labels
		public Vector2 GetAxisScalars ()
		{
			return new Vector2 (m_MaxDistance.floatValue, 1);
		}
		
		private static float LogarithmicValue (float distance, float minDistance, float rolloffScale)
		{
			if ((distance > minDistance) && (rolloffScale != 1.0f))
			{
				distance -= minDistance;
				distance *= rolloffScale;
				distance += minDistance;
			}			
			if (distance < .000001f)
				distance = .000001f;
			return minDistance / distance;
		}
		
		/// A logarithmic curve starting at /timeStart/, /valueStart/ and ending at /timeEnd/, /valueEnd/
		private static AnimationCurve Logarithmic (float timeStart, float timeEnd, float logBase)
		{
			float value, slope, s;
			List<Keyframe> keys = new List<Keyframe> ();
			// Just plain set the step to 2 always. It can't really be any less,
			// or the curvature will end up being imprecise in certain edge cases.
			float step = 2;
			timeStart = Mathf.Max (timeStart, 0.0001f);
			for (float d=timeStart; d<timeEnd; d*=step)
			{
				// Add key w. sensible tangents	
				value = LogarithmicValue (d, timeStart, logBase);
				s = d/50.0f;	
				slope = (LogarithmicValue (d+s, timeStart, logBase) - LogarithmicValue (d-s, timeStart, logBase)) / (s * 2);
				keys.Add (new Keyframe (d, value, slope, slope));
			}
			
			// last key
			value = LogarithmicValue (timeEnd, timeStart, logBase);
			s = timeEnd/50.0f;
			slope = (LogarithmicValue (timeEnd+s, timeStart, logBase) - LogarithmicValue (timeEnd-s, timeStart, logBase)) / (s * 2);
			keys.Add (new Keyframe (timeEnd, value, slope, slope));
			
			return new AnimationCurve (keys.ToArray ());
		}
		
		internal void InitStyles ()
		{
			if (ms_Styles == null)
				ms_Styles = new Styles ();
		}
		
		private void Update ()
		{
			// listener moved?
			Vector3 listenerPos = AudioUtil.GetListenerPos ();			
			if ((m_LastListenerPosition - listenerPos).sqrMagnitude > EPSILON)
			{
				m_LastListenerPosition = listenerPos;
				Repaint (); 
			}			
		}
		
		private void UndoRedoPerformed ()
		{
			m_CurveEditor.animationCurves = GetCurveWrapperArray ();
		}
		
		private void HandleLowPassFilter ()
		{
			AudioCurveWrapper audioCurve = m_AudioCurves[kLowPassCurveID];
			
			// Low pass filter present for all targets?
			AudioLowPassFilter[] filterArray = new AudioLowPassFilter[targets.Length];
			for (int i=0; i<targets.Length; i++)
			{
				filterArray[i] = ((AudioSource)targets[i]).GetComponent<AudioLowPassFilter> ();
				if (filterArray[i] == null)
				{
					m_LowpassObject = null;
					audioCurve.curveProp = null;
					m_CutoffFrequency = null;
					// Return if any of the GameObjects don't have an AudioLowPassFilter
					return;
				}
			}
			
			// All the GameObjects have an AudioLowPassFilter.
			// If we don't have the corresponding SerializedObject and SerializedProperties, create them.
			if (audioCurve.curveProp == null)
			{
				m_LowpassObject = new SerializedObject (filterArray);
				m_CutoffFrequency = m_LowpassObject.FindProperty ("m_CutoffFrequency");
				audioCurve.curveProp = m_LowpassObject.FindProperty ("lowpassLevelCustomCurve");
			}
		}
		
		public override void OnInspectorGUI ()
		{
			bool updateWrappers = false;
			
			InitStyles ();
			
			serializedObject.Update ();
			if (m_LowpassObject != null)
				m_LowpassObject.Update ();
			
			HandleLowPassFilter ();
			
			// Check if curves changed outside of the control of this editor
			foreach (AudioCurveWrapper audioCurve in m_AudioCurves)
			{
				CurveWrapper cw = m_CurveEditor.getCurveWrapperById (audioCurve.id);
				if (audioCurve.curveProp != null)
				{
					AnimationCurve propCurve = audioCurve.curveProp.animationCurveValue;
					if ((cw == null) != audioCurve.curveProp.hasMultipleDifferentValues)
					{
						updateWrappers = true;
					}
					else if (cw != null)
					{
						if (cw.curve.length == 0)
							updateWrappers = true;
						else if (propCurve.length >= 1 && propCurve.keys[0].value != cw.curve.keys[0].value)
							updateWrappers = true;
					}
				}
				else if (cw != null)
					updateWrappers = true;
			}
			UpdateWrappersAndLegend (ref updateWrappers);
			
			EditorGUILayout.PropertyField (m_AudioClip);
			if (!m_AudioClip.hasMultipleDifferentValues && m_AudioClip.objectReferenceValue != null)
			{
				string info;
				if (AudioUtil.Is3D (m_AudioClip.objectReferenceValue as AudioClip))
					info = "This is a 3D Sound.";
				else
					info = "This is a 2D Sound.";
				EditorGUILayout.LabelField (EditorGUIUtility.blankContent, EditorGUIUtility.TempContent (info), EditorStyles.helpBox);
			}
			EditorGUILayout.Space ();
			EditorGUILayout.PropertyField (m_Mute);
			EditorGUILayout.PropertyField (m_BypassEffects);
			EditorGUILayout.PropertyField (m_BypassListenerEffects);
			EditorGUILayout.PropertyField (m_BypassReverbZones);
			
			EditorGUILayout.PropertyField (m_PlayOnAwake);
			EditorGUILayout.PropertyField(m_Loop);
				
			EditorGUILayout.Space ();
			EditorGUILayout.IntSlider (m_Priority, 0, 256);
			EditorGUILayout.Space ();
			EditorGUILayout.Slider (m_Volume, 0f, 1.0f);
			EditorGUILayout.Slider (m_Pitch, -3.0f, 3.0f);
			EditorGUILayout.Space ();
			
			m_Expanded3D = EditorGUILayout.Foldout (m_Expanded3D, "3D Sound Settings");
			if (m_Expanded3D)
			{
				EditorGUI.indentLevel++;
				Audio3DGUI (updateWrappers);
				EditorGUI.indentLevel--;
			}
			
			m_Expanded2D = EditorGUILayout.Foldout (m_Expanded2D, "2D Sound Settings");
			if (m_Expanded2D)
			{
				EditorGUI.indentLevel++;
				Audio2DGUI ();
				EditorGUI.indentLevel--;
			}
			
			serializedObject.ApplyModifiedProperties ();
			if (m_LowpassObject != null)
				m_LowpassObject.ApplyModifiedProperties ();
		}
		
		private static void SetRolloffToTarget (SerializedProperty property, Object target)
		{
			property.SetToValueOfTarget (target);
			property.serializedObject.FindProperty ("rolloffMode").SetToValueOfTarget (target);
			property.serializedObject.ApplyModifiedProperties ();
			EditorUtility.ForceReloadInspectors ();
		}
			
		private void Audio3DGUI (bool updateWrappers)
		{
			EditorGUILayout.Slider (m_DopplerLevel, 0.0f, 5.0f);
			EditorGUILayout.Space ();
			
			// If anything inside this block changes, we need to update the wrappers
			EditorGUI.BeginChangeCheck ();
			
			// Rolloff mode
			if (m_RolloffMode.hasMultipleDifferentValues ||
				 (m_RolloffMode.enumValueIndex == (int)AudioRolloffMode.Custom && m_AudioCurves[kRolloffCurveID].curveProp.hasMultipleDifferentValues)
			)
			{
				EditorGUILayout.TargetChoiceField (m_AudioCurves[kRolloffCurveID].curveProp, ms_Styles.rolloffLabel , SetRolloffToTarget);
			}			
			else
			{
				EditorGUILayout.PropertyField (m_RolloffMode, ms_Styles.rolloffLabel);
				
				// Rolloff min distance
				EditorGUI.indentLevel++;
				if ((AudioRolloffMode)m_RolloffMode.enumValueIndex != AudioRolloffMode.Custom)
				{
					EditorGUI.BeginChangeCheck ();
					EditorGUILayout.PropertyField (m_MinDistance);
					if (EditorGUI.EndChangeCheck ())
					{
						m_MinDistance.floatValue = Mathf.Clamp (m_MinDistance.floatValue, 0, m_MaxDistance.floatValue / 1.01f);
					}
				}
				else
				{
					EditorGUI.BeginDisabledGroup (true);
					EditorGUILayout.LabelField (m_MinDistance.displayName, ms_Styles.controlledByCurveLabel);
					EditorGUI.EndDisabledGroup ();
				}
				EditorGUI.indentLevel--;
			}
				
			// Pan Level control
			AnimProp (ms_Styles.panLevelLabel, m_AudioCurves[kPanLevelCurveID].curveProp, 0.0f, 1.0f);
			
			// Spread control
			AnimProp (ms_Styles.spreadLabel, m_AudioCurves[kSpreadCurveID].curveProp, 0.0f, 360.0f);
			
			// Max distance control
			EditorGUI.BeginChangeCheck ();
			EditorGUILayout.PropertyField (m_MaxDistance);
			if (EditorGUI.EndChangeCheck ())
				m_MaxDistance.floatValue = Mathf.Min(Mathf.Max (Mathf.Max (m_MaxDistance.floatValue, 0.01f), m_MinDistance.floatValue * 1.01f), 1000000.0f);
			
			// If anything changed, update the wrappers
			if (EditorGUI.EndChangeCheck ())
				updateWrappers = true;
			
			Rect r = GUILayoutUtility.GetAspectRect (1.333f, GUI.skin.textField);
			r.xMin += EditorGUI.indent;
			if (Event.current.type != EventType.Layout && Event.current.type != EventType.Used)
			{	
				m_CurveEditor.rect = new Rect (r.x,r.y,r.width,r.height);
			}
							
			
			// Draw Curve Editor
			UpdateWrappersAndLegend (ref updateWrappers);
			GUI.Label (m_CurveEditor.drawRect, GUIContent.none, "TextField");
			
			m_CurveEditor.hRangeLocked = Event.current.shift;
			m_CurveEditor.vRangeLocked = EditorGUI.actionKey;
			
			m_CurveEditor.OnGUI ();
			
			
			// Draw current listener position
			if (targets.Length == 1)
			{
				AudioSource t = (AudioSource)target;
				AudioListener audioListener = (AudioListener)FindObjectOfType (typeof (AudioListener));
				if (audioListener != null)
				{
					float distToListener = (AudioUtil.GetListenerPos () - t.transform.position).magnitude;
					DrawLabel ("Listener", distToListener, r);
				}
			}
			
			
			// Draw legend
			DrawLegend ();
			
			
			// Check if any of the curves changed
			foreach (AudioCurveWrapper audioCurve in m_AudioCurves)
			{
				if ((m_CurveEditor.getCurveWrapperById (audioCurve.id) != null) && (m_CurveEditor.getCurveWrapperById (audioCurve.id).changed))
				{
					AnimationCurve changedCurve = m_CurveEditor.getCurveWrapperById (audioCurve.id).curve;
					
					// Never save a curve with no keys
					if (changedCurve.length > 0)
					{
						audioCurve.curveProp.animationCurveValue = changedCurve;
						m_CurveEditor.getCurveWrapperById (audioCurve.id).changed = false;
						
						// Volume curve special handling
						if (audioCurve.type == AudioCurveType.Volume)
							m_RolloffMode.enumValueIndex = (int)AudioRolloffMode.Custom;
						
						// Low pass curve special handling
						if (audioCurve.type == AudioCurveType.Lowpass)
						{
							if (audioCurve.curveProp.animationCurveValue.length == 1)
							{
								Keyframe kf = audioCurve.curveProp.animationCurveValue.keys[0];
								m_CutoffFrequency.floatValue = (1.0f - kf.value) * kMaxCutoffFrequency;
							}
						}
					}
				}
			}
		}
			
		void UpdateWrappersAndLegend (ref bool updateWrappers)
		{
			if (updateWrappers)
			{
				m_CurveEditor.animationCurves = GetCurveWrapperArray ();
				SyncShownCurvesToLegend (GetShownAudioCurves ());
				updateWrappers = false;
			}
		}
			
		void DrawLegend ()
		{
			List<Rect> legendRects = new List<Rect> ();
			List<AudioCurveWrapper> curves = GetShownAudioCurves ();
			
			Rect legendRect = GUILayoutUtility.GetRect (10, 20);
			legendRect.x += 4 + EditorGUI.indent;
			legendRect.width -= 8 + EditorGUI.indent;
			int width = Mathf.Min (75, Mathf.FloorToInt (legendRect.width / curves.Count));
			for (int i=0; i<curves.Count; i++)
			{
				legendRects.Add (new Rect (legendRect.x + width*i, legendRect.y, width, legendRect.height));
			}
			
			bool resetSelections = false;
			if (curves.Count != m_SelectedCurves.Length)
			{
				m_SelectedCurves = new bool[curves.Count];
				resetSelections = true;
			}
			
			if (EditorGUIExt.DragSelection (legendRects.ToArray (), ref m_SelectedCurves, GUIStyle.none) || resetSelections)
			{
				// If none are selected, select all
				bool someSelected = false;
				for (int i=0; i<curves.Count; i++)
				{
					if (m_SelectedCurves[i])
						someSelected = true;
				}
				if (!someSelected)
				{
					for (int i=0; i<curves.Count; i++)
					{
						m_SelectedCurves[i] = true;
					}
				}
				
				SyncShownCurvesToLegend (curves);
			}
			
			for (int i=0; i<curves.Count; i++)
				{
				EditorGUI.DrawLegend (legendRects[i], curves[i].color, curves[i].legend.text, m_SelectedCurves[i]);
				if (curves[i].curveProp.hasMultipleDifferentValues)
				{
					GUI.Button (new Rect (legendRects[i].x, legendRects[i].y+20, legendRects[i].width, 20), "Different");
				}
			}
		}
		
		private void Audio2DGUI ()
		{
			EditorGUILayout.Slider (m_Pan2D,-1f,1f);
		}
		
		private List<AudioCurveWrapper> GetShownAudioCurves ()
		{
			return m_AudioCurves.Where (f => m_CurveEditor.getCurveWrapperById (f.id) != null).ToList ();
		}
		
		private void SyncShownCurvesToLegend (List<AudioCurveWrapper> curves)
		{
			if (curves.Count != m_SelectedCurves.Length)
				return; // Selected curves in sync'ed later in this frame
			
			for (int i=0; i<curves.Count; i++)
				m_CurveEditor.getCurveWrapperById (curves[i].id).hidden = !m_SelectedCurves[i];
			
			// Need to apply animation curves again to synch selections
			m_CurveEditor.animationCurves = m_CurveEditor.animationCurves;
		}
		
		void DrawLabel (string label, float value, Rect r)
		{
			Vector2 size = ms_Styles.labelStyle.CalcSize (new GUIContent (label));
			size.x += 2;
			Vector2 posA = m_CurveEditor.DrawingToViewTransformPoint (new Vector2 (value / m_MaxDistance.floatValue, 0));
			Vector2 posB = m_CurveEditor.DrawingToViewTransformPoint (new Vector2 (value / m_MaxDistance.floatValue, 1));
			GUI.BeginGroup (r);
				Color temp = Handles.color;
				Handles.color = new Color (1,0,0, 0.3f);
				Handles.DrawLine (new Vector3 (posA.x  , posA.y, 0), new Vector3 (posB.x  , posB.y, 0));
				Handles.DrawLine (new Vector3 (posA.x+1, posA.y, 0), new Vector3 (posB.x+1, posB.y, 0));
				Handles.color = temp;
				GUI.Label (new Rect (Mathf.Floor (posB.x - size.x/2), 2, size.x, 15), label, ms_Styles.labelStyle);
			GUI.EndGroup ();
		}
		
		internal static void AnimProp (GUIContent label, SerializedProperty prop, float min, float max)
		{
			if (prop.hasMultipleDifferentValues)
			{
				EditorGUILayout.TargetChoiceField (prop, label);
				return;
			}
			
			AnimationCurve curve = prop.animationCurveValue;
			if (curve == null)
			{
				Debug.LogError (label.text+" curve is null!");
				return;
			}
			else if (curve.length == 0)
			{
				Debug.LogError (label.text+" curve has no keys!");
				return;
			}
			
			if (curve.length != 1)
			{
				EditorGUI.BeginDisabledGroup (true);
				EditorGUILayout.LabelField (label.text, "Controlled by Curve");
				EditorGUI.EndDisabledGroup ();
			}
			else
			{
				float f = Mathf.Lerp (min, max, curve.keys[0].value);
				f = MathUtils.DiscardLeastSignificantDecimal (f);
				EditorGUI.BeginChangeCheck ();
				if (max > min)
				f = EditorGUILayout.Slider (label, f, min, max);
				else
					f = EditorGUILayout.Slider (label, f, max, min);
				if (EditorGUI.EndChangeCheck ())
				{
					Keyframe kf = curve.keys[0];
					kf.time = 0.0f;
					kf.value = Mathf.InverseLerp (min, max, f);
					curve.MoveKey (0,kf);
				}
			}
			
			prop.animationCurveValue = curve;
		}
		
		void OnSceneGUI ()
		{
			AudioSource source = (AudioSource)target;
			
			Color tempColor = Handles.color;
			if (source.enabled)
				Handles.color = new Color(0.50f, 0.70f, 1.00f, 0.5f);
			else
				Handles.color = new Color(0.30f, 0.40f, 0.60f, 0.5f);
			
			Vector3 position = source.transform.position;
			
			EditorGUI.BeginChangeCheck();
			float minDistance = Handles.RadiusHandle(Quaternion.identity, position, source.minDistance, true);
			float maxDistance = Handles.RadiusHandle(Quaternion.identity, position, source.maxDistance, true);
			if (EditorGUI.EndChangeCheck())
			{
				Undo.RecordObject(source, "AudioSource Distance");
				source.minDistance = minDistance;
				source.maxDistance = maxDistance;
			}
				
			Handles.color = tempColor;
		}
	}
}

