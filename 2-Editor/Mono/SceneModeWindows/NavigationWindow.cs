using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using Object = UnityEngine.Object;

namespace UnityEditor
{
internal class NavMeshEditorWindow : EditorWindow, IHasCustomMenu
{
	private static NavMeshEditorWindow s_MsNavMeshEditorWindow;

	//Agent settings
	private SerializedObject m_Object;
	private SerializedProperty m_AgentRadius;
	private SerializedProperty m_AgentHeight;
	private SerializedProperty m_AgentSlope;
	private SerializedProperty m_AgentClimb;
	private SerializedProperty m_LedgeDropHeight;
	private SerializedProperty m_MaxJumpAcrossDistance;
	private SerializedProperty m_AccuratePlacement;

	//Advanced Settings
	private SerializedProperty m_MinRegionArea;

	private SerializedProperty m_WidthInaccuracy;
	private SerializedProperty m_HeightInaccuracy;

	private const string kRootPath = "m_BuildSettings.";

	static Styles s_Styles;

	enum Mode
	{
		ObjectSettings = 0,
		BakeSettings = 1,
		LayerSettings = 2
	}
	Mode m_Mode = Mode.ObjectSettings;

	private class Styles
	{
		public readonly GUIContent m_AgentRadiusContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.Radius");
		public readonly GUIContent m_AgentHeightContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.Height");
		public readonly GUIContent m_AgentSlopeContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.MaxSlope");
		public readonly GUIContent m_AgentDropContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.DropHeight");
		public readonly GUIContent m_AgentClimbContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.StepHeight");
		public readonly GUIContent m_AgentJumpContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.JumpDistance");
		public readonly GUIContent m_AgentPlacementContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.HeightMesh");
		public readonly GUIContent m_MinRegionAreaContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.MinRegionArea");
		public readonly GUIContent m_WidthInaccuracyContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.WidthInaccuracy");
		public readonly GUIContent m_HeightInaccuracyContent = EditorGUIUtility.TextContent ("NavMeshEditorWindow.HeightInaccuracy");

		public readonly GUIContent m_GeneralHeader = new GUIContent ("General");
		public readonly GUIContent m_OffmeshHeader = new GUIContent ("Generated Off Mesh Links");
		public readonly GUIContent m_AdvancedHeader = new GUIContent ("Advanced");

		public readonly GUIContent[] m_ModeToggles =
		{
			EditorGUIUtility.TextContent ("NavmeshEditor.ObjectSettings"),
			EditorGUIUtility.TextContent ("NavmeshEditor.BakeSettings"),
			EditorGUIUtility.TextContent ("NavmeshEditor.LayerSettings"),
		};
	};

	[MenuItem ("Window/Navigation", false, 2100)]
	public static void SetupWindow ()
	{
		var window = GetWindow <NavMeshEditorWindow> (typeof (InspectorWindow));
		window.title = EditorGUIUtility.TextContent ("NavmeshEditor.WindowTitle").text;
		window.minSize = new Vector2 (300, 360);
	}

	public void OnEnable ()
	{
		s_MsNavMeshEditorWindow = this;
		s_Styles = new Styles ();
		Init ();
		EditorApplication.searchChanged += Repaint;
		Repaint ();
	}

	private void Init ()
	{
		//Agent settings
		m_Object = new SerializedObject (NavMeshBuilder.navMeshSettingsObject);

		m_AgentRadius = m_Object.FindProperty (kRootPath + "agentRadius");
		m_AgentHeight = m_Object.FindProperty (kRootPath + "agentHeight");
		m_AgentSlope = m_Object.FindProperty (kRootPath + "agentSlope");
		m_LedgeDropHeight = m_Object.FindProperty (kRootPath + "ledgeDropHeight");
		m_AgentClimb = m_Object.FindProperty (kRootPath + "agentClimb");
		m_MaxJumpAcrossDistance = m_Object.FindProperty (kRootPath + "maxJumpAcrossDistance");
		m_AccuratePlacement = m_Object.FindProperty (kRootPath + "accuratePlacement");

		//Advanced Settings
		m_MinRegionArea = m_Object.FindProperty (kRootPath + "minRegionArea");

		m_WidthInaccuracy = m_Object.FindProperty (kRootPath + "widthInaccuracy");
		m_HeightInaccuracy = m_Object.FindProperty (kRootPath + "heightInaccuracy");
	}

	public void OnDisable ()
	{
		s_MsNavMeshEditorWindow = null;
		EditorApplication.searchChanged -= Repaint;
	}

	void OnSelectionChange ()
	{
		m_ScrollPos = Vector2.zero;
		if (m_Mode == Mode.ObjectSettings)
			Repaint ();
	}

	void ModeToggle ()
	{
		m_Mode = (Mode)GUILayout.Toolbar ((int)m_Mode, s_Styles.m_ModeToggles, "LargeButton");
	}

	private Vector2 m_ScrollPos = Vector2.zero;
	public void OnGUI()
	{
		if (m_Object.targetObject == null)
			Init ();

		m_Object.Update ();

		EditorGUIUtility.labelWidth = 130;

		EditorGUILayout.Space ();
		ModeToggle ();
		EditorGUILayout.Space ();

		m_ScrollPos = EditorGUILayout.BeginScrollView (m_ScrollPos);
		switch (m_Mode)
		{
			case Mode.ObjectSettings:
				ObjectSettings ();
				break;
			case Mode.BakeSettings:
				BakeSettings ();
				break;
			case Mode.LayerSettings:
				LayerSettings ();
				break;
		}
		EditorGUILayout.EndScrollView ();
		BakeButtons ();

		m_Object.ApplyModifiedProperties ();
	}

	public void OnBecameVisible ()
	{
		if (NavMeshVisualizationSettings.showNavigation)
			return;

		NavMeshVisualizationSettings.showNavigation = true;

		SceneView.onSceneGUIDelegate += OnSceneViewGUI;
		RepaintSceneAndGameViews ();
	}

	public void OnBecameInvisible ()
	{
		NavMeshVisualizationSettings.showNavigation = false;
		SceneView.onSceneGUIDelegate -= OnSceneViewGUI;
		RepaintSceneAndGameViews ();
	}

	static void RepaintSceneAndGameViews ()
	{
		SceneView.RepaintAll ();
		foreach (GameView gv in Resources.FindObjectsOfTypeAll (typeof (GameView)))
			gv.Repaint ();
	}

	public void OnSceneViewGUI(SceneView sceneView)
	{
		if (!NavMeshVisualizationSettings.showNavigation)
			return;

		SceneViewOverlay.Window (new GUIContent ("Navmesh Display"), DisplayControls, (int)SceneViewOverlay.Ordering.NavMesh, SceneViewOverlay.WindowDisplayOption.OneWindowPerTarget);
	}

	static void DisplayControls (Object target, SceneView sceneView)
	{
		EditorGUIUtility.labelWidth = 110;
		var bRepaint = false;
		var showNavMesh = NavMeshVisualizationSettings.showNavMesh;
		if (showNavMesh != EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("NavMeshEditorWindow.ShowNavMesh"), showNavMesh))
		{
			NavMeshVisualizationSettings.showNavMesh = !showNavMesh;
			bRepaint = true;
		}

		EditorGUI.BeginDisabledGroup (!NavMeshVisualizationSettings.hasHeightMesh);
		bool showHeightMesh = NavMeshVisualizationSettings.showHeightMesh;
		if (showHeightMesh != EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("NavMeshEditorWindow.ShowHeightMesh"), showHeightMesh))
		{
			NavMeshVisualizationSettings.showHeightMesh = !showHeightMesh;
			bRepaint = true;
		}
		EditorGUI.EndDisabledGroup ();

		if (bRepaint)
			RepaintSceneAndGameViews ();
	}

	public virtual void AddItemsToMenu (GenericMenu menu)
	{
		menu.AddItem (new GUIContent ("Reset Bake Settings"), false, ResetBakeSettings);
	}

	void ResetBakeSettings ()
	{
		Unsupported.SmartReset (NavMeshBuilder.navMeshSettingsObject);
	}

	public static void BackgroundTaskStatusChanged ()
	{
		if (s_MsNavMeshEditorWindow != null)
			s_MsNavMeshEditorWindow.Repaint ();
	}


	static IEnumerable<GameObject> GetObjectsRecurse (GameObject root)
	{
		var objects = new List<GameObject> {root};
		foreach (Transform t in root.transform)
			objects.AddRange (GetObjectsRecurse (t.gameObject));
		return objects;
	}


	static List<GameObject> GetObjects (bool includeChildren)
	{
		if (includeChildren)
		{
			var objects = new List<GameObject> ();
			foreach (var selected in Selection.gameObjects)
			{
				objects.AddRange (GetObjectsRecurse (selected));
			}
			return objects;
		}
		return new List<GameObject> (Selection.gameObjects);
	}

	static bool SelectionHasChildren ()
	{
		return Selection.gameObjects.Any (obj => obj.transform.childCount > 0);
	}

	static void SetNavMeshLayer (int layer, bool includeChildren)
	{
		var objects = GetObjects (includeChildren);
		if (objects.Count <= 0) return;

		Undo.RecordObjects (objects.ToArray (), "Change NavMesh layer");
		foreach (var go in objects)
			GameObjectUtility.SetNavMeshLayer (go, layer);
	}

	private static void ObjectSettings ()
	{
		bool emptySelection = true;
		GameObject[] gos;
		SceneModeUtility.SearchBar (typeof (MeshRenderer), typeof (Terrain));
		EditorGUILayout.Space ();

		MeshRenderer[] renderers = SceneModeUtility.GetSelectedObjectsOfType<MeshRenderer> (out gos);
		if (gos.Length > 0)
		{
			emptySelection = false;
			ObjectSettings (renderers, gos);
		}

		Terrain[] terrains = SceneModeUtility.GetSelectedObjectsOfType<Terrain> (out gos);
		if (gos.Length > 0)
		{
			emptySelection = false;
			ObjectSettings (terrains, gos);
		}

		if (emptySelection)
			GUILayout.Label ("Select a MeshRenderer or a Terrain from the scene.", EditorStyles.helpBox);
	}

	private static void ObjectSettings (Object[] components, GameObject[] gos)
	{
		EditorGUILayout.MultiSelectionObjectTitleBar (components);

		var so = new SerializedObject (gos);

		EditorGUI.BeginDisabledGroup (!SceneModeUtility.StaticFlagField ("Navigation Static", so.FindProperty ("m_StaticEditorFlags"), (int) StaticEditorFlags.NavigationStatic));

		SceneModeUtility.StaticFlagField ("OffMeshLink Generation", so.FindProperty ("m_StaticEditorFlags"), (int)StaticEditorFlags.OffMeshLinkGeneration);

		var property = so.FindProperty ("m_NavMeshLayer");

		EditorGUI.BeginChangeCheck ();
		EditorGUI.showMixedValue = property.hasMultipleDifferentValues;
		var navLayerNames = GameObjectUtility.GetNavMeshLayerNames ();
		var currentAbsoluteIndex = GameObjectUtility.GetNavMeshLayer (gos[0]);

		var navLayerindex = -1;

		//Need to find the index as the list of names will compress out empty layers
		for (var i = 0; i < navLayerNames.Length; i++)
		{
			if (GameObjectUtility.GetNavMeshLayerFromName (navLayerNames[i]) == currentAbsoluteIndex)
			{
				navLayerindex = i;
				break;
			}
		}

		var navMeshLayer = EditorGUILayout.Popup ("Navigation Layer", navLayerindex, navLayerNames);
		EditorGUI.showMixedValue = false;

		if (EditorGUI.EndChangeCheck ())
		{
			//Convert the selected index into absolute index...
			var newNavLayerIndex = GameObjectUtility.GetNavMeshLayerFromName (navLayerNames[navMeshLayer]);

			GameObjectUtility.ShouldIncludeChildren includeChildren = GameObjectUtility.DisplayUpdateChildrenDialogIfNeeded (Selection.gameObjects,
				"Change Navigation Layer", "Do you want change the navigation layer to "+navLayerNames[navMeshLayer]+" for all the child objects as well?");
			if (includeChildren != GameObjectUtility.ShouldIncludeChildren.Cancel)
			{
				property.intValue = newNavLayerIndex;
				SetNavMeshLayer (newNavLayerIndex, includeChildren == 0);
			}
		}

		EditorGUI.EndDisabledGroup ();

		so.ApplyModifiedProperties ();
	}

	private bool m_Advanced;
	private void BakeSettings ()
	{
		EditorGUILayout.LabelField (s_Styles.m_GeneralHeader, EditorStyles.boldLabel);
		//Agent Settings
		var radius = EditorGUILayout.FloatField (s_Styles.m_AgentRadiusContent, m_AgentRadius.floatValue);
		if (radius >= 0.001f && !Mathf.Approximately (radius - m_AgentRadius.floatValue, 0.0f))
			m_AgentRadius.floatValue = radius;

		var height = EditorGUILayout.FloatField (s_Styles.m_AgentHeightContent, m_AgentHeight.floatValue);
		if (height >= 0.001f && !Mathf.Approximately (height - m_AgentHeight.floatValue, 0.0f))
			m_AgentHeight.floatValue = height;

		EditorGUILayout.Slider (m_AgentSlope, 0.0f, 90.0f, s_Styles.m_AgentSlopeContent);

		//Step height
		var newClimb = EditorGUILayout.FloatField (s_Styles.m_AgentClimbContent, m_AgentClimb.floatValue);
		if (newClimb >= 0.0f && !Mathf.Approximately (m_AgentClimb.floatValue - newClimb, 0.0f))
			m_AgentClimb.floatValue = newClimb;

		if (m_AgentClimb.floatValue >= m_AgentHeight.floatValue)
		{
			// Actual clamping happens in NavMeshBuilder.cpp ConfigureConfig()
			EditorGUILayout.HelpBox ("Step height should be less than agent height.\nClamping step height to " + m_AgentHeight.floatValue + ".", MessageType.Warning);
		}

		EditorGUILayout.Space ();
		EditorGUILayout.LabelField (s_Styles.m_OffmeshHeader, EditorStyles.boldLabel);
		bool disableGeneratedOffMeshLinks = !Application.HasProLicense ();
		if (disableGeneratedOffMeshLinks)
		{
			EditorGUILayout.HelpBox ("This is only available in the Pro version of Unity.", MessageType.Warning);

			if (m_LedgeDropHeight.floatValue != 0.0f)
				m_LedgeDropHeight.floatValue = 0.0f;

			if (m_MaxJumpAcrossDistance.floatValue != 0.0f)
				m_MaxJumpAcrossDistance.floatValue = 0.0f;

			GUI.enabled = false;
		}

		//Drop height
		var newDropHeight =  EditorGUILayout.FloatField (s_Styles.m_AgentDropContent, m_LedgeDropHeight.floatValue);
		if (newDropHeight >= 0.0f && !Mathf.Approximately (newDropHeight - m_LedgeDropHeight.floatValue, 0.0f))
			m_LedgeDropHeight.floatValue = newDropHeight;

		//Jump distance
		var newJumpDistance = EditorGUILayout.FloatField (s_Styles.m_AgentJumpContent, m_MaxJumpAcrossDistance.floatValue);
		if (newJumpDistance >= 0.0f && !Mathf.Approximately (newJumpDistance - m_MaxJumpAcrossDistance.floatValue, 0.0f))
			m_MaxJumpAcrossDistance.floatValue = newJumpDistance;

		if (disableGeneratedOffMeshLinks)
			GUI.enabled = true;

		EditorGUILayout.Space ();

		//Advanced Settings
		m_Advanced = GUILayout.Toggle (m_Advanced, s_Styles.m_AdvancedHeader, EditorStyles.foldout);
		if (m_Advanced)
		{
			var minRegionArea = EditorGUILayout.FloatField (s_Styles.m_MinRegionAreaContent, m_MinRegionArea.floatValue);
			if (minRegionArea >= 0.0f && minRegionArea != m_MinRegionArea.floatValue)
				m_MinRegionArea.floatValue = minRegionArea;

			EditorGUILayout.Slider (m_WidthInaccuracy, 1.0f, 100.0f, s_Styles.m_WidthInaccuracyContent);

			EditorGUILayout.Slider (m_HeightInaccuracy, 1.0f, 100.0f, s_Styles.m_HeightInaccuracyContent);

			//Height mesh
			var accurate = EditorGUILayout.Toggle (s_Styles.m_AgentPlacementContent, m_AccuratePlacement.boolValue);
			if (accurate != m_AccuratePlacement.boolValue) m_AccuratePlacement.boolValue = accurate;
		}
	}

	private void LayerSettings ()
	{
		Object obj = Unsupported.GetSerializedAssetInterfaceSingleton ("NavMeshLayers");
		SerializedObject serializedObject = new SerializedObject (obj);

		Editor.DoDrawDefaultInspector (serializedObject);
	}

	static void BakeButtons ()
	{
		const float kButtonWidth = 95;

		GUILayout.BeginHorizontal ();
		GUILayout.FlexibleSpace ();

		bool wasEnabled = GUI.enabled;
		GUI.enabled &= !Application.isPlaying;
		if (GUILayout.Button ("Clear", GUILayout.Width (kButtonWidth)))
			NavMeshBuilder.ClearAllNavMeshes ();
		GUI.enabled = wasEnabled;

		if (NavMeshBuilder.isRunning)
		{
			if (GUILayout.Button ("Cancel", GUILayout.Width (kButtonWidth)))
				NavMeshBuilder.Cancel ();
		}
		else
		{
			wasEnabled = GUI.enabled;
			GUI.enabled &= !Application.isPlaying;
			if (GUILayout.Button ("Bake", GUILayout.Width (kButtonWidth)))
				NavMeshBuilder.BuildNavMeshAsync ();
			GUI.enabled = wasEnabled;
		}

		GUILayout.EndHorizontal ();

		EditorGUILayout.Space ();
	}
	}
}
