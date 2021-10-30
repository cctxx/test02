using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using UnityEditorInternal;
using UnityEngine;
using Object = UnityEngine.Object;


namespace UnityEditor
{
	internal class LightmappingWindow : EditorWindow, IHasCustomMenu
	{
		int[] kModeValues = { (int)LightmapsMode.Single, (int)LightmapsMode.Dual, (int)LightmapsMode.Directional };
		GUIContent[] kModeStrings = { new GUIContent("Single Lightmaps"), new GUIContent("Dual Lightmaps"), new GUIContent("Directional Lightmaps") };
		GUIContent[] kBouncesStrings = { new GUIContent("0"), new GUIContent("1"), new GUIContent("2"), new GUIContent("3"), new GUIContent("4") };
		int[] kBouncesValues = { 0, 1, 2, 3, 4 };
		GUIContent[] kQualityStrings = { new GUIContent("High"), new GUIContent("Low") };
		int[] kQualityValues = { (int)LightmapBakeQuality.High, (int)LightmapBakeQuality.Low };
		GUIContent[] kShadowTypeStrings = { new GUIContent("Off"), new GUIContent("On (Realtime: Hard Shadows)"), new GUIContent("On (Realtime: Soft Shadows)") };
		int[] kShadowTypeValues = { 0, 1, 2 };
		GUIContent[] kTerrainLightmapSizeStrings = { new GUIContent("0"), new GUIContent("32"), new GUIContent("64"), new GUIContent("128"), new GUIContent("256"), 
													 new GUIContent("512"), new GUIContent("1024"), new GUIContent("2048"), new GUIContent("4096") };
		int[] kTerrainLightmapSizeValues = { 0, 32, 64, 128, 256, 512, 1024, 2048, 4096 };

		enum Mode
		{
			ObjectSettings = 0,
			BakeSettings = 1,
			Maps = 2
		}
		
		enum BakeMode
		{
			BakeScene = 0,
			BakeSelected = 1,
			BakeProbes = 2
		}

		static LightmappingWindow s_LightmappingWindow;

		readonly LightProbeGUI m_LightProbeEditor = new LightProbeGUI();
		Vector2 m_ScrollPosition = Vector2.zero;
		Vector2 m_ScrollPositionLightmaps = Vector2.zero;
		Vector2 m_ScrollPositionMaps = Vector2.zero;
		float m_OldResolution = -1.0f;
		Mode m_Mode = Mode.ObjectSettings;
		int m_SelectedLightmap = -1;
		float m_BakeStartTime = -1;
		string m_LastBakeTimeString = "";
		
		const string kBakeModeKey = "LightmapEditor.BakeMode";
		BakeMode bakeMode
		{
			get { return (BakeMode)EditorPrefs.GetInt (kBakeModeKey, 0); }
			set { EditorPrefs.SetInt (kBakeModeKey, (int)value); }
		}

		AnimValueManager m_Anims = new AnimValueManager();
		AnimBool m_ShowDualOptions = new AnimBool ();
		AnimBool m_ShowFinalGather = new AnimBool();
		AnimBool m_ShowShadowOptions = new AnimBool();
		AnimBool m_ShowShadowAngleOrSize = new AnimBool();
		AnimBool m_ShowAO = new AnimBool();
		AnimBool m_ShowClampedSize = new AnimBool();
		AnimBool m_ShowColorSpaceWarning = new AnimBool();
		AnimBool m_ShowAreaLight = new AnimBool();
		bool m_ShowAtlas = false;
		int m_LastAmountOfLights = 0;
		
		PreviewResizer m_PreviewResizer = new PreviewResizer ();
		bool m_ProbePositionsChanged = true;
		static public void ProbePositionsChanged()
		{
			if (s_LightmappingWindow)
				s_LightmappingWindow.m_ProbePositionsChanged = true;
		}
		bool m_IncorrectProbePositions = false;
		
		static Styles s_Styles;
		
		private static bool colorSpaceWarningValue { get { return LightmapSettings.bakedColorSpace != QualitySettings.desiredColorSpace && LightmapSettings.lightmaps.Length > 0; } }

		void OnEnable()
		{
			s_LightmappingWindow = this;
			m_ShowDualOptions.value = (LightmapSettings.lightmapsMode == LightmapsMode.Dual);
			m_ShowFinalGather.value = (LightmapEditorSettings.bounces > 0) && InternalEditorUtility.HasPro();
			m_ShowShadowOptions.value = true;
			m_ShowShadowAngleOrSize.value = true;
			m_ShowAO.value = (LightmapEditorSettings.aoAmount > 0);
			m_ShowClampedSize.value = false;
			m_ShowColorSpaceWarning.value = colorSpaceWarningValue;
			m_ShowAreaLight.value = false;
			m_Anims.Add (m_ShowDualOptions);
			m_Anims.Add(m_ShowFinalGather);
			m_Anims.Add(m_ShowShadowOptions);
			m_Anims.Add(m_ShowShadowAngleOrSize);
			m_Anims.Add(m_ShowAO);
			m_Anims.Add(m_ShowClampedSize);
			m_Anims.Add (m_ShowColorSpaceWarning);
			m_Anims.Add(m_ShowAreaLight);
			autoRepaintOnSceneChange = true;
			m_PreviewResizer.Init ("LightmappingPreview");
			EditorApplication.searchChanged += Repaint;
			Repaint ();
		}

		void OnDisable()
		{
			s_LightmappingWindow = null;
			EditorApplication.searchChanged -= Repaint;
		}

		float m_OldQualitySettingsShadowDistance = -1.0f;
		float m_ShadowDistance = -1.0f;
		static bool s_IsVisible = false;

		void OnBecameVisible()
		{
			if(s_IsVisible == true) return;
			
			s_IsVisible = true;
			LightmapVisualization.enabled = true;
			LightmapVisualization.showLightProbes = true;
			// init to the shadow distance value from the quality settings only if it
			// changed while we were invisible (or closed)
			if (m_OldQualitySettingsShadowDistance != QualitySettings.shadowDistance)
			{
				m_ShadowDistance = QualitySettings.shadowDistance;
				m_OldQualitySettingsShadowDistance = m_ShadowDistance;
			}
			LightmapVisualization.shadowDistance = m_ShadowDistance;
			SceneView.onSceneGUIDelegate += OnSceneViewGUI;
			RepaintSceneAndGameViews();
		}

		void OnBecameInvisible()
		{
			s_IsVisible = false;
			LightmapVisualization.enabled = false;
			LightmapVisualization.showLightProbes = false;
			m_OldQualitySettingsShadowDistance = QualitySettings.shadowDistance;
			SceneView.onSceneGUIDelegate -= OnSceneViewGUI;
			RepaintSceneAndGameViews();
		}

		void OnSelectionChange()
		{
			UpdateLightmapSelection();
			if(m_Mode == Mode.ObjectSettings || m_Mode == Mode.Maps)
				Repaint();
		}

		private static void LightmappingDone()
		{
			if (s_LightmappingWindow)
			{
				s_LightmappingWindow.MarkEndTime();
				//s_LightmappingWindow.m_LightProbeEditor.LightmappingDone();
				s_LightmappingWindow.Repaint();
			}

			Analytics.Track("/LightMapper/Created");
		}

		static void RepaintSceneAndGameViews()
		{
			SceneView.RepaintAll();
			GameView.RepaintAll();
		}

		void OnGUI()
		{
			if (m_Anims.callback == null)
				m_Anims.callback = Repaint;
			if (s_Styles == null)
				s_Styles = new Styles();

			EditorGUIUtility.labelWidth = 130;
			
			EditorGUILayout.Space();
			ModeToggle();
			EditorGUILayout.Space();
			
			//Color Space warning for lightmaps
			m_ShowColorSpaceWarning.target = colorSpaceWarningValue;
			if (EditorGUILayout.BeginFadeGroup(m_ShowColorSpaceWarning.faded))
			{
				GUIContent c = EditorGUIUtility.TextContent("LightEditor.WrongColorSpaceWarning");
				EditorGUILayout.HelpBox (c.text, MessageType.Warning);
			}
			EditorGUILayout.EndFadeGroup();

			m_ScrollPosition = EditorGUILayout.BeginScrollView(m_ScrollPosition);
				switch(m_Mode)
				{
					case Mode.ObjectSettings:
						ObjectSettings();
						break;
					case Mode.BakeSettings:
						BakeSettings();
						break;
					case Mode.Maps:
						Maps();
						break;
				}
			EditorGUILayout.EndScrollView();
			
			//GUILayout.BeginVertical("PopupCurveSwatchBackground");
				
				EditorGUILayout.Space();

				if (m_ProbePositionsChanged && Event.current.type == EventType.Layout)
				{
					int[] indices;
					Vector3[] positions;
					LightProbeGroupEditor.TetrahedralizeSceneProbes(out positions, out indices);
					m_IncorrectProbePositions = (positions.Length > 0 && indices.Length == 0);
					m_ProbePositionsChanged = false;
				}

				if (m_IncorrectProbePositions)
					EditorGUILayout.HelpBox (s_Styles.IncorrectLightProbePositions.text, MessageType.Warning);

				GUI.enabled = !EditorApplication.isPlayingOrWillChangePlaymode;
				Buttons();
				GUI.enabled = true;
				
				EditorGUILayout.Space();
	
				Summary();
				
			//GUILayout.EndVertical();
			
			EditorGUILayout.BeginHorizontal (GUIContent.none, "preToolbar", GUILayout.Height (17));
			{
				GUILayout.FlexibleSpace ();
				GUI.Label (GUILayoutUtility.GetLastRect (), "Preview", "preToolbar2");
			} EditorGUILayout.EndHorizontal();
			
			float previewSize = m_PreviewResizer.ResizeHandle (position, 100, 250, 17);
			
			if (previewSize > 0)
				Lightmaps(new Rect(0, position.height - previewSize, position.width, previewSize));
		}
		
		void ModeToggle()
		{
			m_Mode = (Mode)GUILayout.Toolbar ((int)m_Mode, s_Styles.ModeToggles, "LargeButton");
		}

		public void OnSceneViewGUI(SceneView sceneView)
		{
			if (!s_IsVisible)
				return;

			SceneViewOverlay.Window(new GUIContent("Lightmap Display"), DisplayControls, (int)SceneViewOverlay.Ordering.Lightmapping, SceneViewOverlay.WindowDisplayOption.OneWindowPerTarget);
		}

		void OnDidOpenScene ()
		{
			//s_LightmapEditor.m_LightProbeEditor.DidOpenScene();
		}

		void DisplayControls(Object target, SceneView sceneView)
		{
			EditorGUIUtility.labelWidth = 110;
			bool useLightmaps = LightmapVisualization.useLightmaps;
			if (useLightmaps != EditorGUILayout.Toggle(EditorGUIUtility.TextContent("LightmapEditor.UseLightmaps"), useLightmaps))
			{
				LightmapVisualization.useLightmaps = !useLightmaps;
				RepaintSceneAndGameViews();
			}

			float sd = Mathf.Max(EditorGUILayout.FloatField(EditorGUIUtility.TextContent("LightmapEditor.ShadowDistance"), m_ShadowDistance), 0);
			if (sd != m_ShadowDistance)
			{
				m_ShadowDistance = sd;
				LightmapVisualization.shadowDistance = m_ShadowDistance;
				RepaintSceneAndGameViews();
			}

			if (sceneView)
			{
				DrawCameraMode renderMode = sceneView.renderMode;
				bool visualiseResolution = EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("LightmapEditor.DisplayControls.VisualiseResolution"), renderMode == DrawCameraMode.LightmapResolution);
				if (visualiseResolution && renderMode != DrawCameraMode.LightmapResolution)
				{
					sceneView.renderMode = DrawCameraMode.LightmapResolution;
					sceneView.Repaint();
				}
				else if (!visualiseResolution && renderMode == DrawCameraMode.LightmapResolution)
				{
					sceneView.renderMode = DrawCameraMode.Textured;
					sceneView.Repaint();
				}

				m_LightProbeEditor.DisplayControls(sceneView);
			}
			else
			{
				// We are drawing this in OnSceneViewGUI
				// - how can it make sense to draw controls if the scene view reference is null?
				bool guiEnabledState = GUI.enabled;
				GUI.enabled = false;
				EditorGUILayout.Toggle(EditorGUIUtility.TextContent("LightmapEditor.DisplayControls.VisualiseResolution"), false);
				GUI.enabled = guiEnabledState;
			}
		}
		
		float LightmapScaleGUI (SerializedObject so, Renderer[] renderers)
		{
			// Different LOD levels in the renderer result in different lod lightmap scaling.
			// Find the best fitting lod scale for all selected renderers.
			// If there is not a common LOD scale, use 1.0
			float lodScale = LightmapVisualization.GetLightmapLODLevelScale(renderers[0]);
			for (int i=1; i<renderers.Length; i++)
			{
				if (!Mathf.Approximately(lodScale, LightmapVisualization.GetLightmapLODLevelScale(renderers[i])))
					lodScale = 1.0F;
			}
			
			SerializedProperty lightmapScaleProp = so.FindProperty("m_ScaleInLightmap");
			float lightmapScale = lodScale * lightmapScaleProp.floatValue;
			
			Rect rect = EditorGUILayout.GetControlRect ();
			EditorGUI.BeginProperty (rect, null, lightmapScaleProp);
			EditorGUI.BeginChangeCheck ();
			lightmapScale = EditorGUI.FloatField (rect, s_Styles.ScaleInLightmap, lightmapScale);
			if (EditorGUI.EndChangeCheck ())
				lightmapScaleProp.floatValue = Mathf.Max (lightmapScale / lodScale, 0.0f);
			EditorGUI.EndProperty ();
				
			return LightmapVisualization.GetLightmapLODLevelScale(renderers[0]) * lightmapScale;;
		}

		bool HasNormals (Renderer renderer)
		{
			Mesh mesh = null;
			if (renderer is MeshRenderer)
			{
				MeshFilter mf = renderer.GetComponent<MeshFilter> ();
				if (mf != null)
				mesh = mf.sharedMesh;
			}
			else if (renderer is SkinnedMeshRenderer)
			{
				mesh = (renderer as SkinnedMeshRenderer).sharedMesh;
			}
			return InternalMeshUtil.HasNormals (mesh);
		}

		void ObjectSettings()
		{
			bool emptySelection = true;
			GameObject[] gos;
			SceneModeUtility.SearchBar (typeof (Light), typeof (Renderer), typeof (Terrain));
			EditorGUILayout.Space ();
			
			// Renderers
			Renderer[] renderers = SceneModeUtility.GetSelectedObjectsOfType<Renderer> (out gos, typeof (MeshRenderer), typeof (SkinnedMeshRenderer));
			if (gos.Length > 0)
			{
				emptySelection = false;
				EditorGUILayout.MultiSelectionObjectTitleBar (renderers);
				
				var goso = new SerializedObject(gos);
				EditorGUI.BeginDisabledGroup (!SceneModeUtility.StaticFlagField ("Lightmap Static", goso.FindProperty ("m_StaticEditorFlags"), (int)StaticEditorFlags.LightmapStatic));

				var so = new SerializedObject (renderers);
				
				float lightmapScale = LightmapScaleGUI (so, renderers);
				
				// tell the user if the object's size in lightmap has reached the max atlas size
				float cachedSurfaceArea = renderers[0] is MeshRenderer ?
					InternalMeshUtil.GetCachedMeshSurfaceArea(renderers[0] as MeshRenderer) :
					InternalMeshUtil.GetCachedSkinnedMeshSurfaceArea(renderers[0] as SkinnedMeshRenderer);

				float sizeInLightmap = Mathf.Sqrt(cachedSurfaceArea) * LightmapEditorSettings.resolution * lightmapScale;
				float maxAtlasSize = Math.Min(LightmapEditorSettings.maxAtlasWidth, LightmapEditorSettings.maxAtlasHeight);
				m_ShowClampedSize.target = sizeInLightmap > maxAtlasSize;
				if (EditorGUILayout.BeginFadeGroup(m_ShowClampedSize.faded))
					GUILayout.Label(s_Styles.ClampedSize, EditorStyles.helpBox);
				EditorGUILayout.EndFadeGroup();

				m_ShowAtlas = EditorGUILayout.Foldout(m_ShowAtlas, s_Styles.Atlas);
				if (m_ShowAtlas)
				{
					EditorGUI.indentLevel += 1;
					EditorGUILayout.PropertyField(so.FindProperty("m_LightmapIndex"), s_Styles.AtlasIndex);
					EditorGUILayout.PropertyField(so.FindProperty("m_LightmapTilingOffset.x"), s_Styles.AtlasTilingX);
					EditorGUILayout.PropertyField(so.FindProperty("m_LightmapTilingOffset.y"), s_Styles.AtlasTilingY);
					EditorGUILayout.PropertyField(so.FindProperty("m_LightmapTilingOffset.z"), s_Styles.AtlasOffsetX);
					EditorGUILayout.PropertyField(so.FindProperty("m_LightmapTilingOffset.w"), s_Styles.AtlasOffsetY);
					EditorGUI.indentLevel -= 1;
				}

				if (!HasNormals(renderers[0]))
					EditorGUILayout.HelpBox(s_Styles.NoNormalsNoLightmapping.text, MessageType.Warning);

				goso.ApplyModifiedProperties ();
				so.ApplyModifiedProperties ();
				EditorGUI.EndDisabledGroup ();
				GUILayout.Space(10);
			}
			
			// Lights
			Light[] lights = SceneModeUtility.GetSelectedObjectsOfType<Light> (out gos);
			if (gos.Length > 0)
			{
				emptySelection = false;
				EditorGUILayout.MultiSelectionObjectTitleBar (lights.ToArray ());
				SerializedObject so = new SerializedObject (lights.ToArray ());

				SerializedProperty type = so.FindProperty ("m_Type");
				bool showAreaLight = !type.hasMultipleDifferentValues && (lights[0].type == LightType.Area);
				if (m_LastAmountOfLights > 0)
				{
					// We had lights selected before, do the fade
					m_ShowAreaLight.target = showAreaLight;
				}
				else
				{
					// We didn't have any lights selected before, make the value have an immediate effect
					m_ShowAreaLight.value = showAreaLight;
				}

				SerializedProperty lightmapping = so.FindProperty("m_Lightmapping");

				if (EditorGUILayout.BeginFadeGroup (1 - m_ShowAreaLight.faded))
				{
					EditorGUILayout.PropertyField(lightmapping);
				}
				EditorGUILayout.EndFadeGroup ();

				EditorGUI.BeginDisabledGroup (lightmapping.intValue == 0);

				EditorGUILayout.PropertyField(so.FindProperty("m_Color"));
				EditorGUILayout.Slider(so.FindProperty("m_Intensity"), 0f, 8f);

				if (InternalEditorUtility.HasPro())
					EditorGUILayout.PropertyField(so.FindProperty("m_IndirectIntensity"), s_Styles.LightIndirectIntensity);

				EditorGUILayout.IntPopup(so.FindProperty("m_Shadows.m_Type"), kShadowTypeStrings, kShadowTypeValues, s_Styles.LightShadows);
	
				m_ShowShadowOptions.target = (lights[0].shadows != LightShadows.None);
				if (EditorGUILayout.BeginFadeGroup(m_ShowShadowOptions.faded))
				{
					EditorGUI.indentLevel += 1;
					EditorGUILayout.PropertyField(so.FindProperty("m_ShadowSamples"), s_Styles.LightShadowSamples);
					m_ShowShadowAngleOrSize.target = (lights[0].type != LightType.Area);
					if (EditorGUILayout.BeginFadeGroup(m_ShowShadowAngleOrSize.faded))
					{
						if (lights[0].type == LightType.Directional)
							EditorGUILayout.Slider(so.FindProperty("m_ShadowAngle"), 0f, 90f, s_Styles.LightShadowAngle);
						else
							EditorGUILayout.Slider(so.FindProperty("m_ShadowRadius"), 0f, 2f, s_Styles.LightShadowRadius);
					}
					EditorGUILayout.EndFadeGroup();
					EditorGUI.indentLevel -= 1;
				}
				EditorGUILayout.EndFadeGroup();
	
				so.ApplyModifiedProperties ();
				EditorGUI.EndDisabledGroup ();
				GUILayout.Space(10);
			}
			m_LastAmountOfLights = lights.Length;
			
			// Terrains
			Terrain[] terrains = SceneModeUtility.GetSelectedObjectsOfType<Terrain> (out gos);
			if (gos.Length > 0)
			{
				emptySelection = false;
				EditorGUILayout.MultiSelectionObjectTitleBar (terrains);
				
				SerializedObject goso = new SerializedObject(gos);
				EditorGUI.BeginDisabledGroup (!SceneModeUtility.StaticFlagField ("Lightmap Static", goso.FindProperty ("m_StaticEditorFlags"), (int) StaticEditorFlags.LightmapStatic));
	
				SerializedObject so = new SerializedObject(terrains.ToArray());
				SerializedProperty lightmapSizeProp = so.FindProperty("m_LightmapSize");
				bool terrainsNeedFlush = false;
				int oldLightmapSize = lightmapSizeProp.intValue;
				EditorGUILayout.IntPopup(lightmapSizeProp, kTerrainLightmapSizeStrings, kTerrainLightmapSizeValues, s_Styles.TerrainLightmapSize);
				terrainsNeedFlush |= oldLightmapSize != lightmapSizeProp.intValue;

				m_ShowAtlas = EditorGUILayout.Foldout(m_ShowAtlas, s_Styles.Atlas);
				if (m_ShowAtlas)
				{
					EditorGUI.indentLevel += 1;
					SerializedProperty lightmapIndexProp = so.FindProperty("m_LightmapIndex");
					int oldLightmapIndex = lightmapIndexProp.intValue;
					EditorGUILayout.PropertyField(lightmapIndexProp, s_Styles.AtlasIndex);
					terrainsNeedFlush |= oldLightmapIndex != lightmapIndexProp.intValue;
					EditorGUI.indentLevel -= 1;
				}

				goso.ApplyModifiedProperties ();
				so.ApplyModifiedProperties ();
	
				if (terrainsNeedFlush)
				{
					foreach (Terrain t in terrains)
					{
						if (t != null)
							t.Flush();
					}
				}
				EditorGUI.EndDisabledGroup ();
				GUILayout.Space(10);
			}

	
			if (emptySelection)
				GUILayout.Label(s_Styles.EmptySelection, EditorStyles.helpBox);
		}

		void BakeSettings()
		{
			SerializedObject so = new SerializedObject(LightmapEditorSettings.GetLightmapSettings());
			SerializedProperty lightmapsMode = so.FindProperty("m_LightmapsMode");
			SerializedProperty useDualInForward = so.FindProperty ("m_UseDualLightmapsInForward");
			SerializedProperty skyLightColor = so.FindProperty("m_LightmapEditorSettings.m_SkyLightColor");
			SerializedProperty skyLightIntensity = so.FindProperty("m_LightmapEditorSettings.m_SkyLightIntensity");
			SerializedProperty bounces = so.FindProperty("m_LightmapEditorSettings.m_Bounces");
			SerializedProperty bounceBoost = so.FindProperty("m_LightmapEditorSettings.m_BounceBoost");
			SerializedProperty bounceIntensity = so.FindProperty("m_LightmapEditorSettings.m_BounceIntensity");
			SerializedProperty quality = so.FindProperty("m_LightmapEditorSettings.m_Quality");
			SerializedProperty rays = so.FindProperty("m_LightmapEditorSettings.m_FinalGatherRays");
			SerializedProperty contrastThreshold = so.FindProperty("m_LightmapEditorSettings.m_FinalGatherContrastThreshold");
			SerializedProperty interpolation = so.FindProperty("m_LightmapEditorSettings.m_FinalGatherGradientThreshold");
			SerializedProperty interpolationPoints = so.FindProperty("m_LightmapEditorSettings.m_FinalGatherInterpolationPoints");
			SerializedProperty aoAmount = so.FindProperty("m_LightmapEditorSettings.m_AOAmount");
			SerializedProperty aoMaxDistance = so.FindProperty("m_LightmapEditorSettings.m_AOMaxDistance");
			SerializedProperty aoContrast = so.FindProperty("m_LightmapEditorSettings.m_AOContrast");
			SerializedProperty lockAtlas = so.FindProperty("m_LightmapEditorSettings.m_LockAtlas");
			SerializedProperty resolution = so.FindProperty("m_LightmapEditorSettings.m_Resolution");
			SerializedProperty padding = so.FindProperty("m_LightmapEditorSettings.m_Padding");
			SerializedProperty lodSurfaceDistance = so.FindProperty("m_LightmapEditorSettings.m_LODSurfaceMappingDistance");

			bool beastSettingsOverride = BeastSettingsFileOverride();

			EditorGUILayout.IntPopup(lightmapsMode, kModeStrings, kModeValues, s_Styles.Mode);
			m_ShowDualOptions.target = (LightmapSettings.lightmapsMode == LightmapsMode.Dual);
			if (EditorGUILayout.BeginFadeGroup (m_ShowDualOptions.faded))
			{
				EditorGUILayout.PropertyField (useDualInForward, s_Styles.UseDualInForward);
			}
			EditorGUILayout.EndFadeGroup ();

			GUILayout.Space(5);

			// disable the settings UI, if we're using the settings from the file
			GUI.enabled = !beastSettingsOverride;

			// quality
			int prevQuality = quality.intValue;
			EditorGUILayout.IntPopup(quality, kQualityStrings, kQualityValues, s_Styles.Quality);
			if (quality.intValue != prevQuality)
			{
				if (quality.intValue == (int)LightmapBakeQuality.High)
				{
					// high
					rays.intValue = 1000;
					contrastThreshold.floatValue = 0.05f;
				}
				else
				{
					// low
					rays.intValue = 200;
					contrastThreshold.floatValue = 0.1f;
				}
			}

			GUILayout.Space(5);

			// number of light bounces
			if (InternalEditorUtility.HasPro())
			{
				EditorGUILayout.IntPopup(bounces, kBouncesStrings, kBouncesValues, s_Styles.Bounces);
			}
			else
			{
				bool guiEnabledState = GUI.enabled;
				GUI.enabled = false;
				string[] options = {"0"};
				EditorGUILayout.IntPopup(s_Styles.Bounces.text, 0, options, kBouncesValues);
				GUI.enabled = guiEnabledState;
			}

			// indirect(bounce) intensity
			m_ShowFinalGather.target = (bounces.intValue > 0) && InternalEditorUtility.HasPro();
			if (EditorGUILayout.BeginFadeGroup (m_ShowFinalGather.faded))
			{
				// sky light
				EditorGUILayout.PropertyField(skyLightColor, s_Styles.SkyLightColor);
				EditorGUILayout.PropertyField(skyLightIntensity, s_Styles.SkyLightIntensity);
				if (skyLightIntensity.floatValue < 0.0f)
					skyLightIntensity.floatValue = 0.0f;
				
				// bounce settings
				EditorGUILayout.Slider (bounceBoost, 0.0f, 4.0f, s_Styles.BounceBoost);
				EditorGUILayout.Slider (bounceIntensity, 0.0f, 5.0f, s_Styles.BounceIntensity);

				// final gather settings
				EditorGUILayout.PropertyField (rays, s_Styles.FinalGatherRays);
				if (rays.intValue < 1)
					rays.intValue = 1;
				EditorGUILayout.Slider (contrastThreshold, 0.0f, 0.5f, s_Styles.FinalGatherContrastThreshold);
				EditorGUILayout.Slider (interpolation, 0.0f, 1.0f, s_Styles.FinalGatherGradientThreshold);
				EditorGUILayout.IntSlider (interpolationPoints, 15, 30, s_Styles.FinalGatherInterpolationPoints);
			}
			EditorGUILayout.EndFadeGroup();
			
			// settings file override ends here
			GUI.enabled = true;

			GUILayout.Space(5);

			// AO settings
			EditorGUILayout.Slider(aoAmount, 0.0f, 1.0f, s_Styles.AO);
			m_ShowAO.target = (aoAmount.floatValue > 0);
			if (EditorGUILayout.BeginFadeGroup(m_ShowAO.faded))
			{
				EditorGUI.indentLevel += 1;
				EditorGUILayout.PropertyField(aoMaxDistance, s_Styles.AOMaxDistance);
				if (aoMaxDistance.floatValue < 0.0f)
					aoMaxDistance.floatValue = 0.0f;
				EditorGUILayout.Slider(aoContrast, 0.0f, 2.0f, s_Styles.AOContrast);
				EditorGUI.indentLevel -= 1;
			}
			EditorGUILayout.EndFadeGroup();
			
			GUILayout.Space(5);
			
			EditorGUILayout.PropertyField (lodSurfaceDistance, s_Styles.LODSurfaceDistance);
			

			GUILayout.Space(20);

			// lock atlas
			EditorGUILayout.PropertyField(lockAtlas, s_Styles.LockAtlas);

			GUI.enabled = !lockAtlas.boolValue;

			// resolution
			GUILayout.BeginHorizontal();
			EditorGUILayout.PropertyField(resolution, s_Styles.Resolution);
			if (resolution.floatValue != m_OldResolution)
			{
				resolution.floatValue = resolution.floatValue > 0.0f ? resolution.floatValue : 0.0f;
				SceneView.RepaintAll();
				m_OldResolution = resolution.floatValue;
			}
			GUILayout.Label(" texels per world unit", s_Styles.labelStyle);
			GUILayout.EndHorizontal();

			// padding
			GUILayout.BeginHorizontal();
			EditorGUILayout.PropertyField(padding, s_Styles.Padding);
			GUILayout.Label(" texels", s_Styles.labelStyle);
			GUILayout.EndHorizontal();

			GUI.enabled = true;

			so.ApplyModifiedProperties();
		}

		void UpdateLightmapSelection()
		{
			MeshRenderer renderer;
			Terrain terrain = null;
			// if the active object in the selection is a renderer or a terrain, we're interested in it's lightmapIndex
			if (Selection.activeGameObject == null ||
				((renderer = Selection.activeGameObject.GetComponent<MeshRenderer>()) == null && 
				(terrain = Selection.activeGameObject.GetComponent<Terrain>()) == null))
			{
				m_SelectedLightmap = -1;
				return;
			}
			m_SelectedLightmap = renderer != null ? renderer.lightmapIndex : terrain.lightmapIndex;
		}

		Texture2D LightmapField(Texture2D lightmap, int index)
		{
			Rect rect = GUILayoutUtility.GetRect(100, 100, EditorStyles.objectField);
			MenuSelectLightmapUsers(rect, index);
			Texture2D retval = EditorGUI.ObjectField(rect, lightmap, typeof (Texture2D), false) as Texture2D;
			if (index == m_SelectedLightmap && Event.current.type == EventType.Repaint)
				s_Styles.selectedLightmapHighlight.Draw (rect, false, false, false, false);
			
			return retval;
		}

		void Maps()
		{
			GUI.changed = false;

			SerializedObject so = new SerializedObject(LightmapEditorSettings.GetLightmapSettings());
			SerializedProperty lightProbes = so.FindProperty("m_LightProbes");
			EditorGUILayout.PropertyField(lightProbes, s_Styles.LightProbes);
			so.ApplyModifiedProperties();

			GUILayout.Space(10);

			LightmapData[] lightmaps = LightmapSettings.lightmaps;
			Rect arraySizeFieldRect = GUILayoutUtility.GetRect (100, 100, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.numberField);
			int newLength = Mathf.Clamp(EditorGUI.ArraySizeField(arraySizeFieldRect, s_Styles.MapsArraySize, LightmapSettings.lightmaps.Length, EditorStyles.numberField), 0, 254);

			Compress();

 			m_ScrollPositionMaps = GUILayout.BeginScrollView(m_ScrollPositionMaps);
			for (int i = 0; i < lightmaps.Length; i++ )
			{
				GUILayout.BeginHorizontal();
				GUILayout.FlexibleSpace();
				GUILayout.Label(i.ToString());
				GUILayout.Space (5);
				lightmaps[i].lightmapFar = LightmapField(lightmaps[i].lightmapFar, i);
				GUILayout.Space (10);
				lightmaps[i].lightmapNear = LightmapField(lightmaps[i].lightmapNear, i);
				GUILayout.FlexibleSpace();
				GUILayout.EndHorizontal();
			}
 			GUILayout.EndScrollView();

			if (GUI.changed)
			{
				if( newLength != lightmaps.Length)
				{
					LightmapData[] newLightmaps = System.Array.CreateInstance(typeof(LightmapData), newLength) as LightmapData[];
					System.Array.Copy(lightmaps, newLightmaps, Mathf.Min(lightmaps.Length, newLength));
					for (int i = lightmaps.Length; i < newLength; i++)
						newLightmaps[i] = new LightmapData();
					LightmapSettings.lightmaps = newLightmaps;
				}
				else
				{
					LightmapSettings.lightmaps = lightmaps;	
				}
				RepaintSceneAndGameViews();
			}
		}

		void Compress()
		{
			bool compressed = true;
			LightmapData[] lightmaps = LightmapSettings.lightmaps;
			Texture2D firstLightmap = null;
			foreach (LightmapData ld in lightmaps)
			{
				if(ld.lightmapFar != null)
				{
					firstLightmap = ld.lightmapFar;
					break;
				}
				if (ld.lightmapNear != null)
				{
					firstLightmap = ld.lightmapNear;
					break;
				}
			}
			if (firstLightmap != null)
			{
				string path = AssetDatabase.GetAssetPath(firstLightmap);
				TextureImporter textureImporter = AssetImporter.GetAtPath(path) as TextureImporter;
				if (textureImporter != null)
					compressed = textureImporter.textureFormat == TextureImporterFormat.AutomaticCompressed;
			}

			bool newCompressed = EditorGUILayout.Toggle(s_Styles.TextureCompression, compressed);
			if (newCompressed != compressed)
			{
				// clear the selection - otherwise the Apply Import Settings
				// dialog will pop up if a lightmap is selected in project view
				Object[] objects = new Object[0];
				Selection.objects = objects;
				foreach (LightmapData ld in lightmaps)
				{
					string pathFar = AssetDatabase.GetAssetPath(ld.lightmapFar);
					TextureImporter textureImporterFar = AssetImporter.GetAtPath(pathFar) as TextureImporter;
					if (textureImporterFar != null)
					{
						textureImporterFar.textureFormat = newCompressed ? TextureImporterFormat.AutomaticCompressed : TextureImporterFormat.AutomaticTruecolor;
						AssetDatabase.ImportAsset(pathFar);
					}
					string pathNear = AssetDatabase.GetAssetPath(ld.lightmapNear);
					TextureImporter textureImporterNear = AssetImporter.GetAtPath(pathNear) as TextureImporter;
					if (textureImporterNear != null)
					{
						textureImporterNear.textureFormat = newCompressed ? TextureImporterFormat.AutomaticCompressed : TextureImporterFormat.AutomaticTruecolor;
						AssetDatabase.ImportAsset(pathNear);
					}
				}
			}

		}

		void MarkStartTime()
		{
			m_BakeStartTime = Time.realtimeSinceStartup;
		}

		void MarkEndTime()
		{
			if (m_BakeStartTime < 0 || Time.realtimeSinceStartup - m_BakeStartTime < 0)
			{
				// If the window was closed and open during baking, m_BakeStartTime will be incorrect,
				// just don't display the bake time, it's not super important.
				m_LastBakeTimeString = "";
				return;
			}

			try
			{
				System.TimeSpan time = TimeSpan.FromSeconds(Time.realtimeSinceStartup - m_BakeStartTime);
				m_LastBakeTimeString = "Last bake took " + (time.Days > 0 ? time.Days + "." : "") +
									(time.Hours > 0 || time.Days > 0 ? time.Hours.ToString("00") + ":" : "") +
									time.Minutes.ToString("00") + ":" + time.Seconds.ToString("00");
			}
			catch (Exception)
			{
				m_LastBakeTimeString = "";
			}
		}

		void Buttons()
		{
			float buttonWidth = 120;

			bool disableDirectionalLightmaps = (LightmapSettings.lightmapsMode == LightmapsMode.Directional) && !InternalEditorUtility.HasPro();
			if (disableDirectionalLightmaps)
				EditorGUILayout.HelpBox (s_Styles.DirectionalLightmapsProOnly.text, MessageType.Warning);

			EditorGUI.BeginDisabledGroup (disableDirectionalLightmaps);

			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();
			
			if (GUILayout.Button("Clear", GUILayout.Width(buttonWidth)))
			{
				Lightmapping.Clear();
				Analytics.Track("/LightMapper/Clear");
			}
			
			if (!Lightmapping.isRunning)
			{
				if (BakeButton (GUILayout.Width (buttonWidth)))
				{
					DoBake ();

					// DoBake could've spawned a save scene dialog. This breaks GUI on mac (Case 490388).
					// We work around this with an ExitGUI here.
					GUIUtility.ExitGUI ();
				}
			}
			else if (GUILayout.Button("Cancel", GUILayout.Width(buttonWidth)))
			{
				Lightmapping.Cancel();
				m_BakeStartTime = -1;
				Analytics.Track("/LightMapper/Cancel");
			}
			
			GUILayout.EndHorizontal();

			EditorGUI.EndDisabledGroup (); // disableDirectionalLightmaps
		}
		
		private void DoBake ()
		{
			LightmapsMode lightmapsMode = LightmapSettings.lightmapsMode;
			Analytics.Track("/LightMapper/Start");
			Analytics.Event("LightMapper", "Mode", lightmapsMode.ToString(), 1);
			MarkStartTime();
			switch (bakeMode)
			{
			case BakeMode.BakeScene:
				Analytics.Event("LightMapper", "Button", "BakeScene", 1);
				Lightmapping.BakeAsync ();
				break;
			case BakeMode.BakeProbes:
				Analytics.Event("LightMapper", "Button", "BakeProbes", 1);
				Lightmapping.BakeLightProbesOnlyAsync ();
				break;
			case BakeMode.BakeSelected:
				Analytics.Event("LightMapper", "Button", "BakeSelected", 1);
				Lightmapping.BakeSelectedAsync ();
				break;
			}
		}
		
		private bool BakeButton (params GUILayoutOption[] options)
		{
			var content = EditorGUIUtility.TempContent (ObjectNames.NicifyVariableName (bakeMode.ToString ()));
			var rect = GUILayoutUtility.GetRect (content, s_Styles.dropDownButton, options);
			
			var dropDownRect = rect;
			dropDownRect.xMin = dropDownRect.xMax - 20f;
			
			if (Event.current.type == EventType.MouseDown && dropDownRect.Contains (Event.current.mousePosition))
			{
				var menu = new GenericMenu ();
				string[] names = System.Enum.GetNames (typeof(BakeMode));
				int selectedIndex = System.Array.IndexOf (names, System.Enum.GetName (typeof(BakeMode), bakeMode));
				int index = 0;
				foreach (var name in names.Select (x => ObjectNames.NicifyVariableName (x)))
					menu.AddItem (new GUIContent (name), index == selectedIndex, BakeDropDownCallback, index++);
				menu.DropDown (rect);
				
				Event.current.Use ();
				return false;
			}
			
			return GUI.Button (rect, content, s_Styles.dropDownButton);
		}
		
		private void BakeDropDownCallback (object data)
		{
			bakeMode = (BakeMode)data;
			DoBake ();
		}
		
		void Summary()
		{
			GUILayout.BeginVertical(EditorStyles.helpBox);
			if (m_LastBakeTimeString != "")
				GUILayout.Label(m_LastBakeTimeString, s_Styles.labelStyle);
			
			int totalMemorySize = 0;
			int lightmapCount = 0;
			Dictionary<Vector2, int> sizes = new Dictionary<Vector2, int>();
			bool dualLightmapsMode = false;
			foreach (LightmapData ld in LightmapSettings.lightmaps)
			{
				if (ld.lightmapFar == null)
					continue;
				lightmapCount++;

				Vector2 texSize = new Vector2(ld.lightmapFar.width, ld.lightmapFar.height);
				if (sizes.ContainsKey(texSize))
					sizes[texSize]++;
				else
					sizes.Add(texSize, 1);

				totalMemorySize += TextureUtil.GetRuntimeMemorySize(ld.lightmapFar);
				if (ld.lightmapNear)
				{
					totalMemorySize += TextureUtil.GetRuntimeMemorySize(ld.lightmapNear);
					dualLightmapsMode = true;
				}
			}
			string sizesString = lightmapCount + (dualLightmapsMode ? " dual" : " single") + " lightmap" + (lightmapCount == 1 ? "" : "s");
			bool first = true;
			foreach (var s in sizes)
			{
				sizesString += first ? ": " : ", ";
				first = false;
				if (s.Value > 1)
					sizesString += s.Value + "x";
				sizesString += s.Key.x + "x" + s.Key.y + "px";
			}
			
			GUILayout.BeginHorizontal();
			
			GUILayout.BeginVertical();
			GUILayout.Label(sizesString + " ", s_Styles.labelStyle);
			GUILayout.Label("Color space ", s_Styles.labelStyle);
			GUILayout.EndVertical();
			
			GUILayout.BeginVertical();
			GUILayout.Label(EditorUtility.FormatBytes (totalMemorySize), s_Styles.labelStyle);
			GUILayout.Label((lightmapCount == 0 ? "No Lightmaps" : "" + LightmapSettings.bakedColorSpace), s_Styles.labelStyle);
			GUILayout.EndVertical();
			
			GUILayout.EndHorizontal();
			
			GUILayout.EndVertical();
		}

		static void Header (ref Rect rect, float headerHeight, float headerLeftMargin, float maxLightmaps, LightmapsMode lightmapsMode)
		{
			// we first needed to get the amount of space that the first texture would get
			// as that's done now, let's request the rect for the header
			Rect rectHeader = GUILayoutUtility.GetRect(rect.width, headerHeight);
			rectHeader.width = rect.width / maxLightmaps;
			// swap the first texture row with the header
			rectHeader.y -= rect.height;
			rect.y += headerHeight;
			// display the header
			rectHeader.x += headerLeftMargin;
			if (lightmapsMode == LightmapsMode.Directional)
			{
				EditorGUI.DropShadowLabel(rectHeader, "color");
				rectHeader.x += rectHeader.width;
				EditorGUI.DropShadowLabel(rectHeader, "scale");
			}
			else
			{
				EditorGUI.DropShadowLabel(rectHeader, "far");
				rectHeader.x += rectHeader.width;
				EditorGUI.DropShadowLabel(rectHeader, "near");
			}
		}

		void Lightmaps (Rect r)
		{
			const float headerHeight = 20;
			const float headerLeftMargin = 6;
			bool firstRow = true;
			GUI.Box(r, "", "PreBackground");
			m_ScrollPositionLightmaps = EditorGUILayout.BeginScrollView(m_ScrollPositionLightmaps, GUILayout.Height(r.height));
			int lightmapIndex = 0;
			LightmapsMode lightmapsMode = LightmapSettings.lightmapsMode;
			float maxLightmaps = 2.0f;

			foreach (LightmapData li in LightmapSettings.lightmaps)
			{
				if (li.lightmapFar == null)
				{
					lightmapIndex++;
					continue;
				}

				// get rect for the two textures in this row
				GUILayoutOption[] layout = { GUILayout.MaxWidth(li.lightmapFar.width * maxLightmaps), GUILayout.MaxHeight(li.lightmapFar.height) };
				Rect rect = GUILayoutUtility.GetAspectRect(li.lightmapFar.width * maxLightmaps / li.lightmapFar.height, layout);

				// display the header
				if (firstRow)
				{
					Header(ref rect, headerHeight, headerLeftMargin, maxLightmaps, lightmapsMode);

					firstRow = false;
				}

				// display the textures
				rect.width /= maxLightmaps;
				EditorGUI.DrawPreviewTexture(rect, li.lightmapFar);
				MenuSelectLightmapUsers(rect, lightmapIndex);
				
				if (li.lightmapNear)
				{
					rect.x += rect.width;
					EditorGUI.DrawPreviewTexture(rect, li.lightmapNear);
					MenuSelectLightmapUsers(rect, lightmapIndex);
				}
				
				lightmapIndex++;
			}
			
			EditorGUILayout.EndScrollView();
		}

		void MenuSelectLightmapUsers(Rect rect, int lightmapIndex)
		{
			if (Event.current.type == EventType.ContextClick && rect.Contains(Event.current.mousePosition))
			{
				string[] menuText = { "Select Lightmap Users" };
				Rect r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
				EditorUtility.DisplayCustomMenu(r, EditorGUIUtility.TempContent(menuText), -1, SelectLightmapUsers, lightmapIndex);
				Event.current.Use();
			}
		}

		void SelectLightmapUsers(object userData, string[] options, int selected)
		{
			int lightmapIndex = (int) userData;
			ArrayList newSelection = new ArrayList();
			MeshRenderer[] renderers = FindObjectsOfType(typeof (MeshRenderer)) as MeshRenderer[];
			foreach (MeshRenderer renderer in renderers)
			{
				if (renderer != null && renderer.lightmapIndex == lightmapIndex)
					newSelection.Add(renderer.gameObject);
			}
			Terrain[] terrains = FindObjectsOfType(typeof(Terrain)) as Terrain[];
			foreach (Terrain terrain in terrains)
			{
				if (terrain != null && terrain.lightmapIndex == lightmapIndex)
					newSelection.Add(terrain.gameObject);
			}
			Selection.objects = newSelection.ToArray(typeof(Object)) as Object[];
		}

		public virtual void AddItemsToMenu(GenericMenu menu)
		{
			menu.AddItem(new GUIContent("Generate Beast settings file"), false, GenerateBeastSettingsFile);
		}

		const string kBeastSettingsFileName = "BeastSettings.xml";

		void GenerateBeastSettingsFile()
		{
			string folderPath = GetLightmapAssetsPath();
			string filePath = folderPath + "/" + kBeastSettingsFileName;
			if (folderPath.Length == 0)
			{
				Debug.LogWarning("Scene hasn't been saved yet, can't generate settings file.");
				return;
			}
			if (System.IO.File.Exists(filePath))
			{
				Debug.LogWarning("Beast settings file already exists for this scene.");
				return;
			}
			System.IO.Directory.CreateDirectory(folderPath);
			AssetDatabase.ImportAsset(folderPath);
			FileUtil.CopyFileOrDirectory(EditorApplication.applicationContentsPath + "/Resources/BeastSettings.xml", filePath);
			AssetDatabase.ImportAsset(filePath);
		}

		string GetLightmapAssetsPath()
		{
			string filePath = EditorApplication.currentScene;
			// cut 6 last chars (.unity)
			return filePath.Substring(0, Mathf.Max(0, filePath.Length - 6));
		}

		bool BeastSettingsFileOverride()
		{		
			// If <SceneName>/BeastSettings.xml exists, it will be used to override our settings
			string filePath = GetLightmapAssetsPath() + "/" + kBeastSettingsFileName;
			if (!System.IO.File.Exists (filePath))
				return false;

			GUILayout.Space(5);
			GUILayout.BeginVertical(GUI.skin.box);
			GUILayout.Label("Bake settings will be overridden by " + kBeastSettingsFileName, s_Styles.labelStyle);
			if (GUILayout.Button("Open", GUILayout.ExpandWidth(false)))
				AssetDatabase.OpenAsset(AssetDatabase.LoadMainAssetAtPath(filePath));
			GUILayout.EndVertical();
			GUILayout.Space(5);
			return true;
		}

		[MenuItem("Window/Lightmapping", false, 2098)]
		static void CreateLightmapEditor()
		{
			LightmappingWindow window = EditorWindow.GetWindow<LightmappingWindow>();
			window.title = EditorGUIUtility.TextContent("LightmapEditor.WindowTitle").text;
			window.minSize = new Vector2 (300, 360);
			window.Show();
		}

		class Styles
		{
			public GUIContent[] ModeToggles = {
				EditorGUIUtility.TextContent("LightmapEditor.ObjectSettings"),
				EditorGUIUtility.TextContent("LightmapEditor.BakeSettings"),
				EditorGUIUtility.TextContent("LightmapEditor.Maps")
			};

			public GUIContent UseDualInForward = EditorGUIUtility.TextContent ("LightmapEditor.UseDualInForward");
			public GUIContent SkyLightColor = EditorGUIUtility.TextContent ("LightmapEditor.SkyLightColor");
			public GUIContent LODSurfaceDistance = EditorGUIUtility.TextContent ("LightmapEditor.LODSurfaceDistance");
			public GUIContent SkyLightIntensity = EditorGUIUtility.TextContent("LightmapEditor.SkyLightIntensity");
			public GUIContent Bounces = EditorGUIUtility.TextContent("LightmapEditor.Bounces");
			public GUIContent BounceBoost = EditorGUIUtility.TextContent("LightmapEditor.BounceBoost");
			public GUIContent BounceIntensity = EditorGUIUtility.TextContent("LightmapEditor.BounceIntensity");
			public GUIContent Quality = EditorGUIUtility.TextContent("LightmapEditor.Quality");
			public GUIContent FinalGatherRays = EditorGUIUtility.TextContent("LightmapEditor.FinalGather.Rays");
			public GUIContent FinalGatherContrastThreshold = EditorGUIUtility.TextContent("LightmapEditor.FinalGather.ContrastThreshold");
			public GUIContent FinalGatherGradientThreshold = EditorGUIUtility.TextContent("LightmapEditor.FinalGather.GradientThreshold");
			public GUIContent FinalGatherInterpolationPoints = EditorGUIUtility.TextContent("LightmapEditor.FinalGather.InterpolationPoints");
			public GUIContent Resolution = EditorGUIUtility.TextContent("LightmapEditor.Resolution");
			public GUIContent EmptySelection = EditorGUIUtility.TextContent("LightmapEditor.EmptySelection");
			public GUIContent ScaleInLightmap = EditorGUIUtility.TextContent("LightmapEditor.ScaleInLightmap");
			public GUIContent Static = EditorGUIUtility.TextContent("LightmapEditor.Static");
			public GUIContent LightShadows = EditorGUIUtility.TextContent("LightmapEditor.Light.Shadows");
			public GUIContent LightIndirectIntensity = EditorGUIUtility.TextContent("LightmapEditor.Light.IndirectIntensity");
			public GUIContent LightShadowSamples = EditorGUIUtility.TextContent("LightmapEditor.Light.ShadowSamples");
			public GUIContent LightShadowRadius = EditorGUIUtility.TextContent("LightmapEditor.Light.ShadowRadius");
			public GUIContent LightShadowAngle = EditorGUIUtility.TextContent("LightmapEditor.Light.ShadowAngle");
			public GUIContent TerrainLightmapSize = EditorGUIUtility.TextContent("LightmapEditor.Terrain.LightmapSize");
			public GUIContent AO = EditorGUIUtility.TextContent("LightmapEditor.AO");
			public GUIContent AOMaxDistance = EditorGUIUtility.TextContent("LightmapEditor.AOMaxDistance");
			public GUIContent AOContrast= EditorGUIUtility.TextContent("LightmapEditor.AOContrast");
			public GUIContent MapsArraySize = EditorGUIUtility.TextContent("LightmapEditor.MapsArraySize");
			public GUIContent Mode = EditorGUIUtility.TextContent("LightmapEditor.Mode");
			public GUIContent LockAtlas = EditorGUIUtility.TextContent("LightmapEditor.LockAtlas");
			public GUIContent Atlas = EditorGUIUtility.TextContent("LightmapEditor.Atlas");
			public GUIContent AtlasIndex = EditorGUIUtility.TextContent("LightmapEditor.AtlasIndex");
			public GUIContent AtlasTilingX = EditorGUIUtility.TextContent("LightmapEditor.AtlasTilingX");
			public GUIContent AtlasTilingY = EditorGUIUtility.TextContent("LightmapEditor.AtlasTilingY");
			public GUIContent AtlasOffsetX = EditorGUIUtility.TextContent("LightmapEditor.AtlasOffsetX");
			public GUIContent AtlasOffsetY = EditorGUIUtility.TextContent("LightmapEditor.AtlasOffsetY");
			public GUIContent TextureCompression = EditorGUIUtility.TextContent("LightmapEditor.TextureCompression");
			public GUIContent ClampedSize = EditorGUIUtility.TextContent("LightmapEditor.ClampedSize");
			public GUIContent NoNormalsNoLightmapping = EditorGUIUtility.TextContent("LightmapEditor.NoNormalsNoLightmapping");
			public GUIContent DirectionalLightmapsProOnly = EditorGUIUtility.TextContent("LightmapEditor.DirectionalLightmapsProOnly");
			public GUIContent IncorrectLightProbePositions = EditorGUIUtility.TextContent("LightmapEditor.IncorrectLightProbePositions");
			public GUIContent Padding = EditorGUIUtility.TextContent("LightmapEditor.Padding");
			public GUIContent LightProbes = EditorGUIUtility.TextContent("LightmapEditor.LightProbes");
			
			public GUIStyle selectedLightmapHighlight = "LightmapEditorSelectedHighlight";
			
			public GUIStyle labelStyle = EditorStyles.wordWrappedMiniLabel;
			public GUIStyle dropDownButton = "DropDownButton";
		}
		
	}

} // namespace
