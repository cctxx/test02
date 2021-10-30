using System.Collections.Generic;
using UnityEngine;
using System.Linq;

namespace UnityEditor
{
	[CustomEditor(typeof(QualitySettings))]
	internal class QualitySettingsEditor : Editor
	{
		private static class Styles
		{
			public static readonly GUIStyle kToggle = "OL Toggle";
			public static readonly GUIStyle kDefaultToggle = "OL ToggleWhite";
			public static readonly GUIStyle kButton = "Button";
			public static readonly GUIStyle kSelected = "PR Label";
			
			public static readonly GUIContent kPlatformTooltip = new GUIContent ("", "Allow quality setting on platform");
			public static readonly GUIContent kIconTrash = EditorGUIUtility.IconContent ("TreeEditor.Trash", "Delete Level");
			public static readonly GUIContent kSoftParticlesHint = EditorGUIUtility.TextContent ("QualitySettings.SoftParticlesHint");

			public static readonly GUIStyle kListEvenBg = "ObjectPickerResultsOdd";
			public static readonly GUIStyle kListOddBg = "ObjectPickerResultsEven";
			public static readonly GUIStyle kDefaultDropdown = "QualitySettingsDefault";

			public const int kMinToggleWidth = 15;
			public const int kMaxToggleWidth = 20;
			public const int kHeaderRowHeight = 20;
			public const int kLabelWidth = 80;
		}

		private SerializedObject m_QualitySettings;
		private SerializedProperty m_QualitySettingsProperty;
		private SerializedProperty m_PerPlatformDefaultQualityProperty;
		private List<BuildPlayerWindow.BuildPlatform> m_ValidPlatforms;

		public void OnEnable ()
		{
			m_QualitySettings = new SerializedObject(target);
			m_QualitySettingsProperty = m_QualitySettings.FindProperty("m_QualitySettings");
			m_PerPlatformDefaultQualityProperty = m_QualitySettings.FindProperty ("m_PerPlatformDefaultQuality");
			m_ValidPlatforms = BuildPlayerWindow.GetValidPlatforms();
		}
		
		private struct QualitySetting
		{
			public string m_Name;
			public string m_PropertyPath;
			public List<string> m_ExcludedPlatforms;
		}

		private readonly int m_QualityElementHash = "QualityElementHash".GetHashCode ();
		private class Dragging
		{
			public int m_StartPosition;
			public int m_Position;
		}

		private Dragging m_Dragging;
		private bool m_ShouldAddNewLevel;
		private int m_DeleteLevel = -1;
		private int DoQualityLevelSelection (int currentQualitylevel, IList<QualitySetting> qualitySettings, Dictionary<string, int> platformDefaultQualitySettings)
		{
			GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			GUILayout.BeginVertical ();
			var selectedLevel = currentQualitylevel;

			//Header row
			GUILayout.BeginHorizontal ();

			Rect header = GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.ExpandWidth(false), GUILayout.Width(Styles.kLabelWidth), GUILayout.Height(Styles.kHeaderRowHeight));
			header.x += EditorGUI.indent;
			header.width -= EditorGUI.indent;
			GUI.Label(header, "Levels", EditorStyles.boldLabel);

			//Header row icons
			foreach (var platform in m_ValidPlatforms)
			{
				var iconRect = GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.MinWidth(Styles.kMinToggleWidth), GUILayout.MaxWidth(Styles.kMaxToggleWidth), GUILayout.Height(Styles.kHeaderRowHeight));
				var temp = EditorGUIUtility.TempContent (platform.smallIcon);
				temp.tooltip = platform.name;
				GUI.Label(iconRect, temp);
				temp.tooltip = "";
			}
			
			//Extra column for deleting setting button
			GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.MinWidth(Styles.kMinToggleWidth), GUILayout.MaxWidth(Styles.kMaxToggleWidth), GUILayout.Height(Styles.kHeaderRowHeight));
			
			GUILayout.EndHorizontal ();

			//Draw the row for each quality setting
			var currentEvent = Event.current;
			for (var i = 0; i < qualitySettings.Count; i++ )
			{
				GUILayout.BeginHorizontal ();
				var bgStyle = i % 2 == 0? Styles.kListEvenBg : Styles.kListOddBg;
				bool selected = (selectedLevel == i);

				//Draw the selected icon if required
				Rect r = GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.ExpandWidth(false), GUILayout.Width(Styles.kLabelWidth));

				switch (currentEvent.type)
				{
					case EventType.Repaint:
						bgStyle.Draw (r, GUIContent.none, false, false, selected, false);
						GUI.Label (r, EditorGUIUtility.TempContent (qualitySettings[i].m_Name));
						break;
					case EventType.MouseDown:
						if (r.Contains (currentEvent.mousePosition))
						{
							selectedLevel = i;
							GUIUtility.keyboardControl = 0;
							GUIUtility.hotControl = m_QualityElementHash;
							m_Dragging = new Dragging {m_StartPosition = i, m_Position = i};
							currentEvent.Use();
						}
						break;
					case EventType.MouseDrag:
						if (GUIUtility.hotControl == m_QualityElementHash)
						{
							if (r.Contains (currentEvent.mousePosition))
							{
								m_Dragging.m_Position = i;
								currentEvent.Use();
							}
						}
						break;
					case EventType.MouseUp:
						if (GUIUtility.hotControl == m_QualityElementHash)
						{
							GUIUtility.hotControl = 0;
							currentEvent.Use ();
						}
						break;
					case EventType.KeyDown:
						if (currentEvent.keyCode == KeyCode.UpArrow || currentEvent.keyCode == KeyCode.DownArrow)
						{
							selectedLevel += currentEvent.keyCode == KeyCode.UpArrow ? -1 : 1;
							selectedLevel = Mathf.Clamp (selectedLevel, 0, qualitySettings.Count-1);
							GUIUtility.keyboardControl = 0;
							currentEvent.Use();
						}
						break;
				}

				//Build a list of the current platform selection and draw it.
				foreach (var platform in m_ValidPlatforms)
				{
					bool isDefaultQuality = false;
					if (platformDefaultQualitySettings.ContainsKey (platform.name) &&  platformDefaultQualitySettings [platform.name] == i)
						isDefaultQuality = true;
					
					var toggleRect = GUILayoutUtility.GetRect(Styles.kPlatformTooltip, Styles.kToggle, GUILayout.MinWidth(Styles.kMinToggleWidth), GUILayout.MaxWidth(Styles.kMaxToggleWidth));
					if (Event.current.type == EventType.repaint)
					{
						bgStyle.Draw (toggleRect, GUIContent.none, false, false, selected, false);
					}
					
					var color = GUI.backgroundColor;
					if (isDefaultQuality && !EditorApplication.isPlayingOrWillChangePlaymode)
						GUI.backgroundColor = Color.green;
					
					var supported = !qualitySettings[i].m_ExcludedPlatforms.Contains(platform.name);
					var newSupported = GUI.Toggle (toggleRect, supported, Styles.kPlatformTooltip, isDefaultQuality ? Styles.kDefaultToggle : Styles.kToggle);
					if (supported != newSupported)
					{
						if (newSupported)
							qualitySettings[i].m_ExcludedPlatforms.Remove(platform.name);
						else
							qualitySettings[i].m_ExcludedPlatforms.Add(platform.name);
					}
					
					GUI.backgroundColor = color;
				}
				
				//Extra column for deleting quality button
				var deleteButton = GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.MinWidth(Styles.kMinToggleWidth), GUILayout.MaxWidth(Styles.kMaxToggleWidth));
				if (Event.current.type == EventType.repaint)
				{
					bgStyle.Draw(deleteButton, GUIContent.none, false, false, selected, false);
				}
				if (GUI.Button (deleteButton, Styles.kIconTrash, GUIStyle.none))
					m_DeleteLevel = i;
				GUILayout.EndHorizontal ();
			}
			
			//Add a spacer line to seperate the levels from the defaults
			GUILayout.BeginHorizontal ();
			GUILayoutUtility.GetRect(	GUIContent.none, Styles.kToggle,
										GUILayout.MinWidth(Styles.kMinToggleWidth),
										GUILayout.MaxWidth(Styles.kMaxToggleWidth),
										GUILayout.Height (1));

			DrawHorizontalDivider ();
			GUILayout.EndHorizontal ();

			//Default platform selection dropdowns
			GUILayout.BeginHorizontal ();

			var defaultQualityTitle = GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.ExpandWidth(false), GUILayout.Width(Styles.kLabelWidth), GUILayout.Height(Styles.kHeaderRowHeight));
			defaultQualityTitle.x += EditorGUI.indent;
			defaultQualityTitle.width -= EditorGUI.indent;
			GUI.Label (defaultQualityTitle, "Default", EditorStyles.boldLabel);

			// Draw default dropdown arrows
			foreach (var platform in m_ValidPlatforms)
			{
				var iconRect = GUILayoutUtility.GetRect (	GUIContent.none, Styles.kToggle,
															GUILayout.MinWidth (Styles.kMinToggleWidth),
															GUILayout.MaxWidth (Styles.kMaxToggleWidth),
															GUILayout.Height (Styles.kHeaderRowHeight));

				int position;
				if (!platformDefaultQualitySettings.TryGetValue(platform.name, out position))
					platformDefaultQualitySettings.Add (platform.name, 0);

				position = EditorGUI.Popup (iconRect, position, qualitySettings.Select (x => x.m_Name).ToArray (), Styles.kDefaultDropdown);
				platformDefaultQualitySettings[platform.name] = position;
			}
			GUILayout.EndHorizontal ();
			
			GUILayout.Space (10);
			
			//Add an extra row for 'Add' button
			GUILayout.BeginHorizontal ();
			
			GUILayoutUtility.GetRect(	GUIContent.none, Styles.kToggle,
										GUILayout.MinWidth(Styles.kMinToggleWidth),
										GUILayout.MaxWidth(Styles.kMaxToggleWidth),
										GUILayout.Height (Styles.kHeaderRowHeight));

			var addButtonRect = GUILayoutUtility.GetRect(GUIContent.none, Styles.kToggle, GUILayout.ExpandWidth(true));

			if (GUI.Button (addButtonRect, EditorGUIUtility.TempContent ("Add Quality Level")))
				m_ShouldAddNewLevel = true;
			
			GUILayout.EndHorizontal ();

			GUILayout.EndVertical ();

			GUILayout.FlexibleSpace();
			GUILayout.EndHorizontal ();
			return selectedLevel;
		}
		
		private List<QualitySetting> GetQualitySettings ()
		{
			// Pull the quality settings from the runtime.
			var qualitySettings = new List<QualitySetting>();

			foreach (SerializedProperty prop in m_QualitySettingsProperty)
			{
				var qs = new QualitySetting
				{
					m_Name = prop.FindPropertyRelative ("name").stringValue,
					m_PropertyPath = prop.propertyPath
				};

				qs.m_PropertyPath = prop.propertyPath;

				var platforms = new List<string>();
				var platformsProp = prop.FindPropertyRelative("excludedTargetPlatforms");
				foreach (SerializedProperty platformProp in platformsProp)
					platforms.Add(platformProp.stringValue);

				qs.m_ExcludedPlatforms = platforms;
				qualitySettings.Add(qs);
			}
			return qualitySettings;
		}

		private void SetQualitySettings(IEnumerable<QualitySetting> settings)
		{
			foreach (var setting in settings)
			{
				var property = m_QualitySettings.FindProperty (setting.m_PropertyPath);
				if (property == null)
					continue;

				var platformsProp = property.FindPropertyRelative("excludedTargetPlatforms");
				if (platformsProp.arraySize != setting.m_ExcludedPlatforms.Count)
					platformsProp.arraySize = setting.m_ExcludedPlatforms.Count;

				var count = 0;
				foreach (SerializedProperty platform in platformsProp)
				{
					if (platform.stringValue != setting.m_ExcludedPlatforms[count])
						platform.stringValue = setting.m_ExcludedPlatforms[count];
					count++;
				}
			}
		}
		
		private void HandleAddRemoveQualitySetting (ref int selectedLevel)
		{
			if (m_DeleteLevel >= 0)
			{
				if (m_DeleteLevel < selectedLevel || m_DeleteLevel == m_QualitySettingsProperty.arraySize - 1)
					selectedLevel--;
				
				//Always ensure there is one quality setting
				if (m_QualitySettingsProperty.arraySize > 1 && m_DeleteLevel >= 0 && m_DeleteLevel < m_QualitySettingsProperty.arraySize)
					m_QualitySettingsProperty.DeleteArrayElementAtIndex (m_DeleteLevel);
				m_DeleteLevel = -1;
			}
			
			if (m_ShouldAddNewLevel)
			{
				m_QualitySettingsProperty.arraySize++;
				var addedSetting = m_QualitySettingsProperty.GetArrayElementAtIndex (m_QualitySettingsProperty.arraySize - 1);
				var nameProperty = addedSetting.FindPropertyRelative("name");
				nameProperty.stringValue = "Level " + (m_QualitySettingsProperty.arraySize - 1);
		
				m_ShouldAddNewLevel = false;
			}
		}

		private Dictionary<string, int> GetDefaultQualityForPlatforms()
		{
			var defaultPlatformQualities = new Dictionary<string, int>();

			foreach (SerializedProperty prop in m_PerPlatformDefaultQualityProperty)
			{
				defaultPlatformQualities.Add(prop.FindPropertyRelative("first").stringValue, prop.FindPropertyRelative("second").intValue);
			}
			return defaultPlatformQualities;
		}

		private void SetDefaultQualityForPlatforms(Dictionary<string, int> platformDefaults)
		{
			if (m_PerPlatformDefaultQualityProperty.arraySize != platformDefaults.Count)
				m_PerPlatformDefaultQualityProperty.arraySize = platformDefaults.Count;
			
			var count = 0;
			foreach (var def in platformDefaults)
			{
				var element = m_PerPlatformDefaultQualityProperty.GetArrayElementAtIndex (count);
				var firstProperty = element.FindPropertyRelative("first");
				var secondProperty = element.FindPropertyRelative("second");
				
				if (firstProperty.stringValue != def.Key || secondProperty.intValue != def.Value)
				{
					firstProperty.stringValue = def.Key;
					secondProperty.intValue = def.Value;
				}
				count++;
			}
		}
		
		private static void DrawHorizontalDivider ()
		{
			var spacerLine = GUILayoutUtility.GetRect (	GUIContent.none,
														GUIStyle.none,
														GUILayout.ExpandWidth (true),
														GUILayout.Height (1));
			var oldBgColor = GUI.backgroundColor;
			if (EditorGUIUtility.isProSkin)
				GUI.backgroundColor = oldBgColor*0.7058f;
			else
				GUI.backgroundColor = Color.black;
			
			if (Event.current.type == EventType.Repaint)
				EditorGUIUtility.whiteTextureStyle.Draw(spacerLine, GUIContent.none, false, false, false, false);

			GUI.backgroundColor = oldBgColor;
		}

		void SoftParticlesHintGUI()
		{
			var mainCamera = Camera.main;
			if (mainCamera == null)
				return;

			RenderingPath renderPath = mainCamera.actualRenderingPath;
			if (renderPath == RenderingPath.DeferredLighting)
				return; // using deferred, all is good

			if ((mainCamera.depthTextureMode & DepthTextureMode.Depth) != 0)
				return; // already produces depth texture, all is good

			EditorGUILayout.HelpBox (Styles.kSoftParticlesHint.text, MessageType.Warning, false);
		}
		
		public override void OnInspectorGUI()
		{
			if (EditorApplication.isPlayingOrWillChangePlaymode)
			{
				EditorGUILayout.HelpBox("Changes made in play mode will not be saved.", MessageType.Warning, true);
			}

			m_QualitySettings.Update ();

			var settings = GetQualitySettings ();
			var defaults = GetDefaultQualityForPlatforms ();
			var selectedLevel = QualitySettings.GetQualityLevel();

			selectedLevel = DoQualityLevelSelection (selectedLevel, settings, defaults);
			
			SetQualitySettings (settings);
			HandleAddRemoveQualitySetting (ref selectedLevel);
			SetDefaultQualityForPlatforms (defaults);
			GUILayout.Space (10.0f);
			DrawHorizontalDivider ();
			GUILayout.Space (10.0f);
			
			var currentSettings = m_QualitySettingsProperty.GetArrayElementAtIndex (selectedLevel);
			var nameProperty = currentSettings.FindPropertyRelative("name");
			var pixelLightCountProperty = currentSettings.FindPropertyRelative("pixelLightCount");
			var shadowsProperty = currentSettings.FindPropertyRelative("shadows");
			var shadowResolutionProperty = currentSettings.FindPropertyRelative("shadowResolution");
			var shadowProjectionProperty = currentSettings.FindPropertyRelative("shadowProjection");
			var shadowCascadesProperty = currentSettings.FindPropertyRelative("shadowCascades");
			var shadowDistanceProperty = currentSettings.FindPropertyRelative("shadowDistance");
			var blendWeightsProperty = currentSettings.FindPropertyRelative("blendWeights");
			var textureQualityProperty = currentSettings.FindPropertyRelative("textureQuality");
			var anisotropicTexturesProperty = currentSettings.FindPropertyRelative("anisotropicTextures");
			var antiAliasingProperty = currentSettings.FindPropertyRelative("antiAliasing");
			var softParticlesProperty = currentSettings.FindPropertyRelative("softParticles");
			var vSyncCountProperty = currentSettings.FindPropertyRelative("vSyncCount");
			var lodBiasProperty = currentSettings.FindPropertyRelative("lodBias");
			var maximumLODLevelProperty = currentSettings.FindPropertyRelative("maximumLODLevel");
			var particleRaycastBudgetProperty = currentSettings.FindPropertyRelative("particleRaycastBudget");

			if (string.IsNullOrEmpty(nameProperty.stringValue))
				nameProperty.stringValue = "Level " + selectedLevel;

			EditorGUILayout.PropertyField(nameProperty);
			GUILayout.Space(10);

			GUILayout.Label (EditorGUIUtility.TempContent ("Rendering"), EditorStyles.boldLabel);
			EditorGUILayout.PropertyField (pixelLightCountProperty);
			EditorGUILayout.PropertyField (textureQualityProperty);
			EditorGUILayout.PropertyField (anisotropicTexturesProperty);
			EditorGUILayout.PropertyField (antiAliasingProperty);
			EditorGUILayout.PropertyField (softParticlesProperty);
			if (softParticlesProperty.boolValue)
				SoftParticlesHintGUI();
			GUILayout.Space(10);
			
			GUILayout.Label (EditorGUIUtility.TempContent ("Shadows"), EditorStyles.boldLabel);
			EditorGUILayout.PropertyField (shadowsProperty);
			EditorGUILayout.PropertyField (shadowResolutionProperty);
			EditorGUILayout.PropertyField (shadowProjectionProperty);
			EditorGUILayout.PropertyField (shadowCascadesProperty);
			EditorGUILayout.PropertyField (shadowDistanceProperty);
			GUILayout.Space (10);

			GUILayout.Label (EditorGUIUtility.TempContent ("Other"), EditorStyles.boldLabel);
			EditorGUILayout.PropertyField (blendWeightsProperty);
			EditorGUILayout.PropertyField (vSyncCountProperty);
			EditorGUILayout.PropertyField (lodBiasProperty);
			EditorGUILayout.PropertyField (maximumLODLevelProperty);
			EditorGUILayout.PropertyField (particleRaycastBudgetProperty);
			
			if (m_Dragging != null 
				&& m_Dragging.m_Position != m_Dragging.m_StartPosition)
				{
					m_QualitySettingsProperty.MoveArrayElement (m_Dragging.m_StartPosition, m_Dragging.m_Position);
					m_Dragging.m_StartPosition = m_Dragging.m_Position;
					selectedLevel = m_Dragging.m_Position;
				}
			m_QualitySettings.ApplyModifiedProperties();
			QualitySettings.SetQualityLevel (Mathf.Clamp( selectedLevel, 0, m_QualitySettingsProperty.arraySize -1));
		}
	}
}
