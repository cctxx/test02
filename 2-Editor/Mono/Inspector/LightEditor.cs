using UnityEngine;
using UnityEditor;
using UnityEditorInternal;


namespace UnityEditor
{
	[CustomEditor(typeof(Light))]
	[CanEditMultipleObjects]
	class LightEditor : Editor
	{
		SerializedProperty m_Type;
		SerializedProperty m_Range;
		SerializedProperty m_SpotAngle;
		SerializedProperty m_CookieSize;
		SerializedProperty m_Color;
		SerializedProperty m_Intensity;
		SerializedProperty m_Cookie;
		SerializedProperty m_ShadowsType;
		SerializedProperty m_ShadowsStrength;
		SerializedProperty m_ShadowsResolution;
		SerializedProperty m_ShadowsBias;
		SerializedProperty m_ShadowsSoftness;
		SerializedProperty m_ShadowsSoftnessFade;
		SerializedProperty m_Halo;
		SerializedProperty m_Flare;
		SerializedProperty m_RenderMode;
		SerializedProperty m_CullingMask;
		SerializedProperty m_Lightmapping;
		SerializedProperty m_AreaSizeX;
		SerializedProperty m_AreaSizeY;

		AnimValueManager m_Anims = new AnimValueManager ();
		AnimBool m_ShowSpotOptions = new AnimBool();
		AnimBool m_ShowPointOptions = new AnimBool();
		AnimBool m_ShowSoftOptions = new AnimBool();
		AnimBool m_ShowDirOptions = new AnimBool();
		AnimBool m_ShowAreaOptions = new AnimBool();
		AnimBool m_ShowShadowOptions = new AnimBool();
		AnimBool m_ShowShadowWarning = new AnimBool();
		AnimBool m_ShowForwardShadowsWarning = new AnimBool();
		AnimBool m_ShowAreaWarning = new AnimBool();
		
		bool m_UsingDeferred;
	   
		// Should match same colors in GizmoDrawers.cpp!
		internal static Color kGizmoLight = new Color(254/255f, 253/255f, 136/255f, 128/255f);
		internal static Color kGizmoDisabledLight = new Color(135/255f, 116/255f, 50/255f, 128/255f);
 		
 		private bool typeIsSame				{ get { return !m_Type.hasMultipleDifferentValues; } }
 		private bool shadowTypeIsSame		{ get { return !m_ShadowsType.hasMultipleDifferentValues; } }
 		private Light light					{ get { return target as Light; } }
 		
 		private bool spotOptionsValue		{ get { return (typeIsSame && light.type == LightType.Spot); } }
 		private bool pointOptionsValue		{ get { return (typeIsSame && light.type == LightType.Point); } }
 		private bool softOptionsValue		{ get { return (shadowTypeIsSame && typeIsSame && (light.shadows == LightShadows.Soft) && light.type == LightType.Directional); } }
 		private bool dirOptionsValue		{ get { return (typeIsSame && light.type == LightType.Directional); } }
		private bool areaOptionsValue		{ get { return (typeIsSame && light.type == LightType.Area); } }
 		private bool shadowOptionsValue		{ get { return (shadowTypeIsSame && light.shadows != LightShadows.None); } }
 		private bool shadowWarningValue     { get { return (typeIsSame && !InternalEditorUtility.HasPro() && light.type != LightType.Directional); } }
 		private bool forwardWarningValue	{ get { return (typeIsSame && !m_UsingDeferred && light.type != LightType.Directional); } }
		private bool areaWarningValue		{ get { return (typeIsSame && !InternalEditorUtility.HasPro() && light.type == LightType.Area); } }
		
		private void InitShowOptions ()
		{
			m_ShowSpotOptions.value = spotOptionsValue;
			m_ShowPointOptions.value = pointOptionsValue;
			m_ShowSoftOptions.value = softOptionsValue;
			m_ShowDirOptions.value = dirOptionsValue;
			m_ShowAreaOptions.value = areaOptionsValue;
			m_ShowShadowOptions.value = shadowOptionsValue;
			m_ShowShadowWarning.value = shadowWarningValue;
			m_ShowForwardShadowsWarning.value = forwardWarningValue;
			m_ShowAreaWarning.value = areaWarningValue;
			
			m_Anims.Add(m_ShowSpotOptions);
			m_Anims.Add(m_ShowPointOptions);
			m_Anims.Add(m_ShowDirOptions);
			m_Anims.Add(m_ShowAreaOptions);
			m_Anims.Add(m_ShowShadowOptions);
			m_Anims.Add(m_ShowShadowWarning);
			m_Anims.Add(m_ShowForwardShadowsWarning);
			m_Anims.Add(m_ShowSoftOptions);
			m_Anims.Add(m_ShowAreaWarning);
		}
		
		private void UpdateShowOptions ()
		{
			m_ShowSpotOptions.target = spotOptionsValue;
			m_ShowPointOptions.target = pointOptionsValue;
			m_ShowSoftOptions.target = softOptionsValue;
			m_ShowDirOptions.target = dirOptionsValue;
			m_ShowAreaOptions.target = areaOptionsValue;
			m_ShowShadowOptions.target = shadowOptionsValue;
			m_ShowShadowWarning.target = shadowWarningValue;
			m_ShowForwardShadowsWarning.target = forwardWarningValue;
			m_ShowAreaWarning.target = areaWarningValue;
		}
 		
		void OnEnable () {
			m_Type = serializedObject.FindProperty ("m_Type");
			m_Range = serializedObject.FindProperty("m_Range");
			m_SpotAngle = serializedObject.FindProperty("m_SpotAngle");
			m_CookieSize = serializedObject.FindProperty("m_CookieSize");
			m_Color = serializedObject.FindProperty("m_Color");
			m_Intensity = serializedObject.FindProperty("m_Intensity");
			m_Cookie = serializedObject.FindProperty("m_Cookie");
			m_ShadowsType = serializedObject.FindProperty("m_Shadows.m_Type");
			m_ShadowsStrength = serializedObject.FindProperty("m_Shadows.m_Strength");
			m_ShadowsResolution = serializedObject.FindProperty("m_Shadows.m_Resolution");
			m_ShadowsBias = serializedObject.FindProperty ("m_Shadows.m_Bias");
			m_ShadowsSoftness = serializedObject.FindProperty ("m_Shadows.m_Softness");
			m_ShadowsSoftnessFade = serializedObject.FindProperty ("m_Shadows.m_SoftnessFade");
			m_Halo = serializedObject.FindProperty ("m_DrawHalo");
			m_Flare = serializedObject.FindProperty("m_Flare");
			m_RenderMode = serializedObject.FindProperty("m_RenderMode");
			m_CullingMask = serializedObject.FindProperty("m_CullingMask");
			m_Lightmapping = serializedObject.FindProperty("m_Lightmapping");
			m_AreaSizeX = serializedObject.FindProperty("m_AreaSize.x");
			m_AreaSizeY = serializedObject.FindProperty("m_AreaSize.y");
		
			InitShowOptions ();
			
			m_UsingDeferred = CameraUtility.DoesAnyCameraUseDeferred();
		}
		
		public override void OnInspectorGUI()
		{
			if (m_Anims.callback == null)
				m_Anims.callback = Repaint;
			
			serializedObject.Update();

			UpdateShowOptions ();
			
			EditorGUILayout.PropertyField (m_Type);		

			// When we are switching between two light types that don't show the range (directional and area lights)
			// we want the fade group to stay hidden.
			bool keepRangeHidden = (m_ShowDirOptions.wantsUpdate && m_ShowAreaOptions.wantsUpdate) &&
				(m_ShowDirOptions.target || m_ShowAreaOptions.target);
			float fadeRange = keepRangeHidden ? 0.0f : 1.0f - Mathf.Max (m_ShowDirOptions.faded, m_ShowAreaOptions.faded);

			if (EditorGUILayout.BeginFadeGroup (m_ShowAreaWarning.faded))
			{
				GUIContent c = EditorGUIUtility.TextContent ("LightEditor.AreaLightsProOnly");
				EditorGUILayout.HelpBox (c.text, MessageType.Warning, false);
			}
			EditorGUILayout.EndFadeGroup ();

			if (EditorGUILayout.BeginFadeGroup (fadeRange))
				EditorGUILayout.PropertyField (m_Range);
			EditorGUILayout.EndFadeGroup();
				
			if (EditorGUILayout.BeginFadeGroup (m_ShowSpotOptions.faded))
				EditorGUILayout.Slider(m_SpotAngle, 1f, 179f);
			EditorGUILayout.EndFadeGroup();
			
			EditorGUILayout.Space();
			EditorGUILayout.PropertyField (m_Color);
			EditorGUILayout.Slider (m_Intensity, 0f, 8f);

			// Don't show the real-time only options for area lights
			if (EditorGUILayout.BeginFadeGroup (1 - m_ShowAreaOptions.faded))
			{
			EditorGUILayout.PropertyField (m_Cookie);
			if (EditorGUILayout.BeginFadeGroup (m_ShowDirOptions.faded))
				EditorGUILayout.PropertyField (m_CookieSize);
			EditorGUILayout.EndFadeGroup();
			EditorGUILayout.Space();
			
			EditorGUILayout.PropertyField (m_ShadowsType, new GUIContent("Shadow Type", "Shadow cast options"));

			if (EditorGUILayout.BeginFadeGroup (m_ShowShadowOptions.faded))
			{
				EditorGUI.indentLevel += 1;

				if (EditorGUILayout.BeginFadeGroup (m_ShowForwardShadowsWarning.faded))
				{
					GUIContent c = EditorGUIUtility.TextContent ("LightEditor.ForwardRenderingShadowsWarning");
					EditorGUILayout.HelpBox (c.text, MessageType.Warning, false);
				}
				EditorGUILayout.EndFadeGroup ();

				if (EditorGUILayout.BeginFadeGroup (m_ShowShadowWarning.faded))
				{
					GUIContent c = EditorGUIUtility.TextContent ("LightEditor.NoShadowsWarning");
					EditorGUILayout.HelpBox (c.text, MessageType.Warning, false);
				}
				EditorGUILayout.EndFadeGroup ();

				if (EditorGUILayout.BeginFadeGroup (1 - m_ShowShadowWarning.faded))
				{
					EditorGUILayout.Slider (m_ShadowsStrength, 0f, 1f);
					EditorGUILayout.PropertyField (m_ShadowsResolution);
					EditorGUILayout.Slider (m_ShadowsBias, 0.0f, 2.0f);
					if (EditorGUILayout.BeginFadeGroup (m_ShowSoftOptions.faded))
					{
						EditorGUILayout.Slider (m_ShadowsSoftness, 1.0f, 8.0f);
						EditorGUILayout.Slider (m_ShadowsSoftnessFade, 0.1f, 5.0f);
					}
					EditorGUILayout.EndFadeGroup ();
				}
				EditorGUILayout.EndFadeGroup();
								
				EditorGUI.indentLevel -= 1;
			} 
			EditorGUILayout.EndFadeGroup();
			EditorGUILayout.Space();
			
			EditorGUILayout.PropertyField (m_Halo);
			EditorGUILayout.PropertyField (m_Flare);
			EditorGUILayout.PropertyField (m_RenderMode);
			EditorGUILayout.PropertyField (m_CullingMask);
			EditorGUILayout.PropertyField (m_Lightmapping);
			}
			EditorGUILayout.EndFadeGroup ();

			if (EditorGUILayout.BeginFadeGroup(m_ShowAreaOptions.faded))
			{
				EditorGUILayout.PropertyField (m_AreaSizeX, new GUIContent("Width"));
				EditorGUILayout.PropertyField (m_AreaSizeY, new GUIContent("Height"));
			}
			EditorGUILayout.EndFadeGroup();

			EditorGUILayout.Space();
			
			serializedObject.ApplyModifiedProperties ();
		}
		
		void OnSceneGUI()
		{
			Light t = (Light)target;
			
			Color temp = Handles.color;
			if (t.enabled)
				Handles.color = kGizmoLight;
			else
				Handles.color = kGizmoDisabledLight;
			
			float thisRange = t.range;
			switch(t.type)
			{
				case LightType.Point:	
					thisRange = Handles.RadiusHandle(Quaternion.identity, t.transform.position, thisRange, true);
					
					if (GUI.changed)
					{
						Undo.RecordObject(t, "Adjust Point Light");
						t.range = thisRange;
					}
					
					break;
					
				case LightType.Spot:
					// Give handles twice the alpha of the lines
					Color col = Handles.color;
					col.a = Mathf.Clamp01(temp.a * 2);
					Handles.color = col;
					
					Vector2 angleAndRange = new Vector2(t.spotAngle, t.range);
					angleAndRange = Handles.ConeHandle(t.transform.rotation, t.transform.position, angleAndRange, 1.0f, 1.0f, true);
					if (GUI.changed)
					{
						Undo.RecordObject (t, "Adjust Spot Light");
						t.spotAngle = angleAndRange.x;
						t.range = Mathf.Max(angleAndRange.y, 0.01F);
					}
					break;
				case LightType.Area:
					EditorGUI.BeginChangeCheck ();
					Vector2 size = Handles.DoRectHandles (t.transform.rotation, t.transform.position, t.areaSize);
					if (EditorGUI.EndChangeCheck ())
					{
						Undo.RecordObject (t, "Adjust Area Light");
						t.areaSize = size;
					}
					break;
				default:
					break;			
			}
			Handles.color = temp;
		}
	}	
}


