using System;
using UnityEngine;
using UnityEditorInternal;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditor
{

[CustomEditor(typeof(LODGroup))]
internal class LODGroupEditor : Editor
{
	// Default colors for each LOD group.... 
	static readonly Color[] kLODColors =
	{
		new Color (0.4831376f, 0.6211768f, 0.0219608f, 1.0f), 
		new Color (0.2792160f, 0.4078432f, 0.5835296f, 1.0f),
		new Color (0.2070592f, 0.5333336f, 0.6556864f, 1.0f), 
		new Color (0.5333336f, 0.1600000f, 0.0282352f, 1.0f),
		new Color (0.3827448f, 0.2886272f, 0.5239216f, 1.0f),
		new Color (0.8000000f, 0.4423528f, 0.0000000f, 1.0f),
		new Color (0.4486272f, 0.4078432f, 0.0501960f, 1.0f),
		new Color (0.7749016f, 0.6368624f, 0.0250984f, 1.0f)
	};

	static readonly Color kCulledLODColor = new Color (.4f, 0f, 0f, 1f);

	private class Styles
	{
		public readonly GUIStyle m_LODSliderBG = "LODSliderBG";
		public readonly GUIStyle m_LODSliderRange = "LODSliderRange";
		public readonly GUIStyle m_LODSliderRangeSelected = "LODSliderRangeSelected";
		public readonly GUIStyle m_LODSliderText = "LODSliderText";
		public readonly GUIStyle m_LODSliderTextSelected = "LODSliderTextSelected";
		public readonly GUIStyle m_LODStandardButton = "Button";
		public readonly GUIStyle m_LODRendererButton = "LODRendererButton";
		public readonly GUIStyle m_LODRendererAddButton = "LODRendererAddButton";
		public readonly GUIStyle m_LODRendererRemove = "LODRendererRemove";
		public readonly GUIStyle m_LODBlackBox = "LODBlackBox";
		public readonly GUIStyle m_LODCameraLine = "LODCameraLine";

		public readonly GUIStyle m_LODSceneText = "LODSceneText";
		public readonly GUIStyle m_LODRenderersText = "LODRenderersText";
		public readonly GUIStyle m_LODLevelNotifyText = "LODLevelNotifyText";

		public readonly GUIContent m_IconRendererPlus = EditorGUIUtility.IconContent ("Toolbar Plus", "Add New Renderers");
		public readonly GUIContent m_IconRendererMinus = EditorGUIUtility.IconContent ("Toolbar Minus", "Remove Renderer");
		public readonly GUIContent m_CameraIcon = EditorGUIUtility.IconContent("Camera Icon");

		public readonly GUIContent m_UploadToImporter = new GUIContent("Upload to Importer", "Upload the modified screen percentages to the model importer.");
		public readonly GUIContent m_UploadToImporterDisabled = new GUIContent("Upload to Importer", "Number of LOD's in the scene instance differ from the number of LOD's in the imported model.");
		public readonly GUIContent m_RecalculateBounds = new GUIContent("Bounds", "Recalculate bounds for the current LOD group.");
		public readonly GUIContent m_LightmapScale = new GUIContent("Lightmap Scale", "Set the lightmap scale to match the LOD percentages");
		public readonly GUIContent m_RendersTitle = new GUIContent("Renderers:");

		public const int kSceneLabelHalfWidth = 100;
		public const int kSceneLabelHeight = 45;
		public const int kSceneHeaderOffset = 40;

		public const int kSliderBarHeight = 30;

		public const int kRenderersButtonHeight = 60;
		public const int kButtonPadding = 2;
		public const int kDeleteButtonSize = 20;

		public const int kSelectedLODRangePadding = 3;

		public const int kRenderAreaForegroundPadding = 3;
	}

	private static Styles s_Styles;
	
	private SerializedObject m_Object;
	private int m_SelectedLODSlider = -1;
	private int m_SelectedLOD = -1;
	private int m_NumberOfLODs;
	
	private bool m_IsPrefab;

	void OnEnable ()
	{
		m_Object = new SerializedObject (target);
		EditorApplication.update += Update;
		
		// Calculate if the newly selected LOD group is a prefab... they require special handling
		var type = PrefabUtility.GetPrefabType(((LODGroup)target).gameObject);
		if (type == PrefabType.Prefab || type == PrefabType.ModelPrefab)
			m_IsPrefab = true;
		else
			m_IsPrefab = false;

		Repaint();
	}
	
	void OnDisable()
	{
		EditorApplication.update -= Update;	
	}

	// Find the given sceen space recangular bounds from a list of vector 3 points.
	private static Rect CalculateScreenRect(IEnumerable<Vector3> points)
	{
		var points2 = points.Select (p => HandleUtility.WorldToGUIPoint (p)).ToList ();

		var min = new Vector2(float.MaxValue, float.MaxValue);
		var max = new Vector2(float.MinValue, float.MinValue);

		foreach (var point in points2)
		{
			min.x = (point.x < min.x) ? point.x : min.x;
			max.x = (point.x > max.x) ? point.x : max.x;

			min.y = (point.y < min.y) ? point.y : min.y;
			max.y = (point.y > max.y) ? point.y : max.y;
		}

		return new Rect(min.x, min.y, max.x - min.x, max.y - min.y);
	}

	public void OnSceneGUI ()
	{
        if (!Application.HasAdvancedLicense()
			|| Event.current.type != EventType.repaint 
			|| Camera.current == null
			|| SceneView.lastActiveSceneView != SceneView.currentDrawingSceneView
			|| Vector3.Dot(		Camera.current.transform.forward, 
								(Camera.current.transform.position - ((LODGroup)target).transform.position).normalized) > 0)
			return;

		if (s_Styles == null)
			s_Styles = new Styles();

		Camera camera = SceneView.lastActiveSceneView.camera;
	
		var group = target as LODGroup;
		var worldReferencePoint = group.transform.TransformPoint(group.localReferencePoint);

		var info = LODUtility.CalculateVisualizationData(camera, group, -1);
		float size = info.worldSpaceSize;
		
		// Draw cap around LOD to visualize it's size
		Handles.color = info.activeLODLevel != -1 ? kLODColors[info.activeLODLevel] : kCulledLODColor;

		Handles.SelectionFrame(0, worldReferencePoint, camera.transform.rotation, size / 2);

		// Calculate a screen rect for the on scene title
		Vector3 sideways = camera.transform.right * size / 2.0f;
		Vector3 up = camera.transform.up * size / 2.0f;
		var rect = CalculateScreenRect ( 
			new[] 
				{
					worldReferencePoint - sideways + up,
					worldReferencePoint - sideways - up,
					worldReferencePoint + sideways + up,
					worldReferencePoint + sideways - up 
				});

		// Place the screen space lable directaly under the 
		var midPoint = rect.x + rect.width / 2.0f;
		rect = new Rect(midPoint - Styles.kSceneLabelHalfWidth, rect.yMax, Styles.kSceneLabelHalfWidth * 2, Styles.kSceneLabelHeight);

		if (rect.yMax > Screen.height - Styles.kSceneLabelHeight)
			rect.y = Screen.height - Styles.kSceneLabelHeight - Styles.kSceneHeaderOffset;

		Handles.BeginGUI();
		GUI.Label(rect, GUIContent.none, EditorStyles.notificationBackground);
		EditorGUI.DoDropShadowLabel (rect, GUIContent.Temp (info.activeLODLevel >= 0 ? "LOD " + info.activeLODLevel : "Culled"), s_Styles.m_LODLevelNotifyText, 0.3f);
		Handles.EndGUI ();
	}
	
	private Vector3 m_LastCameraPos = Vector3.zero;
	public void Update ()
	{
		if (SceneView.lastActiveSceneView == null || SceneView.lastActiveSceneView.camera == null)
		{
			return;
		}
	
		// Update the last camera positon and repaint if the camera has moved
		if (!Mathf.Approximately (0.0f, Vector3.Distance (SceneView.lastActiveSceneView.camera.transform.position, m_LastCameraPos)))
		{
			m_LastCameraPos = SceneView.lastActiveSceneView.camera.transform.position;
			Repaint();
		}
	}

	private const string kLODRootPath = "m_LODs";
	private const string kLODDataPath = "m_LODs.Array.data[{0}]";
	private const string kPixelHeightDataPath = "m_LODs.Array.data[{0}].screenRelativeHeight";
	private const string kRenderRootPath = "m_LODs.Array.data[{0}].renderers";

	private static float DelinearizeScreenPercentage(float percentage)
	{
		if (Mathf.Approximately(0.0f, percentage))
			return 0.0f;

		return Mathf.Sqrt(percentage);
	}

	private static float LinearizeScreenPercentage(float percentage)
	{
		return percentage * percentage;
	}

	private class LODInfo
	{
		private float m_ScreenPercentage;
		public readonly int m_LODLevel;
		public Rect m_ButtonPosition;
		public Rect m_RangePosition;

		public LODInfo (int lodLevel, float screenPercentage)
		{
			m_LODLevel = lodLevel;
			m_ScreenPercentage = screenPercentage;
		}

		public float ScreenPercent
		{
			get { return DelinearizeScreenPercentage(m_ScreenPercentage); }
			set { m_ScreenPercentage = LinearizeScreenPercentage (value); }
		}

		public float RawScreenPercent
		{
			get { return m_ScreenPercentage; }
		}
	}
	
	private int activeLOD
	{
		get {return m_SelectedLOD;}
	}

	private ModelImporter GetImporter()
	{
		return AssetImporter.GetAtPath(AssetDatabase.GetAssetPath(PrefabUtility.GetPrefabParent(target))) as ModelImporter;
	}

	public override void OnInspectorGUI ()
	{
		var initiallyEnabled = GUI.enabled;
        if (!Application.HasAdvancedLicense())
		{
			EditorGUILayout.HelpBox("LOD only available in Unity Pro", MessageType.Warning);
			GUI.enabled = false;
		}

		if (s_Styles == null)
			s_Styles = new Styles ();
		
		// Grab the latest data from the object
		m_Object.Update ();
		m_NumberOfLODs = m_Object.FindProperty (kLODRootPath).arraySize;
		
		// Prepass to remove all empty renderers
		if (m_NumberOfLODs > 0 && activeLOD >= 0)
		{
			var renderersProperty = m_Object.FindProperty (string.Format (kRenderRootPath, activeLOD));
			for (var i = renderersProperty.arraySize-1; i >= 0; i--)
			{
				var rendererRef = renderersProperty.GetArrayElementAtIndex (i).FindPropertyRelative ("renderer");
				var renderer = rendererRef.objectReferenceValue as Renderer;
			
				if (renderer == null)
					renderersProperty.DeleteArrayElementAtIndex (i);
			}
		}

		// Add some space at the top..
		GUILayout.Space (17);
		
		// Precalculate and cache the slider bar position for this update
		var sliderBarPosition = GUILayoutUtility.GetRect (0, Styles.kSliderBarHeight, GUILayout.ExpandWidth(true));
		
		// Precalculate the lod info (button locations / ranges ect)
		var lods = new List<LODInfo> ();
		
		var lastScreenPercentage = -1.0f;
		for (var i = 0; i < m_NumberOfLODs; i++) 
		{
			var pixelHeight = m_Object.FindProperty (string.Format (kPixelHeightDataPath, i));

			var lodInfo = new LODInfo (i, pixelHeight.floatValue);
			lodInfo.m_ButtonPosition = CalcLODButton(sliderBarPosition, lodInfo.ScreenPercent);
			
			var previousPercentage = (i - 1 < 0) ? 1.0f : lastScreenPercentage;
			lodInfo.m_RangePosition = CalcLODRange(sliderBarPosition, previousPercentage, lodInfo.ScreenPercent);
			lastScreenPercentage = lodInfo.ScreenPercent;
			
			lods.Add (lodInfo);
		}

		// Add some space
		GUILayout.Space (8);
		DrawLODLevelSlider (sliderBarPosition, lods);
		GUILayout.Space(8);

		GUILayout.Label(string.Format("LODBias of {0:0.00} active", QualitySettings.lodBias), EditorStyles.boldLabel);

		// Draw the info for the selected LOD
		if (m_NumberOfLODs > 0 && activeLOD >= 0 && activeLOD < m_NumberOfLODs)
		{
			DrawRenderersInfo (Screen.width / 60);
		}

		GUILayout.Space(8);

		GUILayout.BeginHorizontal ();
		GUILayout.Label ("Recalculate:", EditorStyles.boldLabel);
		if (GUILayout.Button (s_Styles.m_RecalculateBounds))
			LODUtility.CalculateLODGroupBoundingBox (target as LODGroup);

		if (GUILayout.Button (s_Styles.m_LightmapScale))
			SendPercentagesToLightmapScale ();
		GUILayout.EndHorizontal ();

		GUILayout.Space (5);
		
		var isImportedModelPrefab = PrefabUtility.GetPrefabType(target) == PrefabType.ModelPrefabInstance;
		if (isImportedModelPrefab)
		{
			var importer = GetImporter ();
			var importerRef = new SerializedObject (importer);
			var importerLODLevels = importerRef.FindProperty ("m_LODScreenPercentages");
			var lodNumberOnImporterMatches = importerLODLevels.isArray && importerLODLevels.arraySize == lods.Count;

			var guiState = GUI.enabled;
			if (!lodNumberOnImporterMatches)
				GUI.enabled = false;

			if ( importer != null 
				&& GUILayout.Button(lodNumberOnImporterMatches ? s_Styles.m_UploadToImporter : s_Styles.m_UploadToImporterDisabled ))
			{
				// Number of imported LOD's is the same as in the imported model
				for (var i = 0; i < importerLODLevels.arraySize; i++)
					importerLODLevels.GetArrayElementAtIndex (i).floatValue = lods[i].RawScreenPercent;

				importerRef.ApplyModifiedProperties ();

				AssetDatabase.ImportAsset (importer.assetPath);
			}
			GUI.enabled = guiState;
		}

		// Apply the property, handle undo
		m_Object.ApplyModifiedProperties ();

		GUI.enabled = initiallyEnabled;
	}
	
	// Draw the renderers for the current LOD group
	// Arrange in a grid 
	private void DrawRenderersInfo (int horizontalNumber)
	{
		var titleArea = GUILayoutUtility.GetRect (s_Styles.m_RendersTitle, s_Styles.m_LODSliderTextSelected);
		if (Event.current.type == EventType.Repaint)
			EditorStyles.label.Draw (titleArea, s_Styles.m_RendersTitle, false, false, false, false);
		
		// Draw renderer info
		var renderersProperty = m_Object.FindProperty (string.Format (kRenderRootPath, activeLOD));
		
		var numberOfButtons = renderersProperty.arraySize + 1;
		var numberOfRows = Mathf.CeilToInt ( numberOfButtons / (float)horizontalNumber);
		
		var drawArea = GUILayoutUtility.GetRect (0, numberOfRows * Styles.kRenderersButtonHeight, GUILayout.ExpandWidth(true));
		var rendererArea = drawArea;
		GUI.Box(drawArea, GUIContent.none);
		rendererArea.width -= 2 * Styles.kRenderAreaForegroundPadding;
		rendererArea.x += Styles.kRenderAreaForegroundPadding;

		var buttonWidth = rendererArea.width / horizontalNumber;

		var buttons = new List<Rect> ();

		for (int i = 0; i < numberOfRows; i++)
		{
			for (int k = 0; k < horizontalNumber && (i * horizontalNumber + k) < renderersProperty.arraySize; k++)
			{
				var drawPos = new Rect (
					Styles.kButtonPadding + rendererArea.x + k * buttonWidth,
					Styles.kButtonPadding + rendererArea.y + i * Styles.kRenderersButtonHeight,
					buttonWidth - Styles.kButtonPadding * 2,
					Styles.kRenderersButtonHeight - Styles.kButtonPadding * 2);
				buttons.Add (drawPos);
				DrawRendererButton (drawPos, i * horizontalNumber + k);
			}
		}

		if (m_IsPrefab)
			return;

		//+ button
		int horizontalPos = (numberOfButtons - 1) % horizontalNumber;
		int verticalPos = numberOfRows - 1;
		HandleAddRenderer (new Rect (
					Styles.kButtonPadding + rendererArea.x + horizontalPos * buttonWidth,
					Styles.kButtonPadding + rendererArea.y + verticalPos * Styles.kRenderersButtonHeight,
					buttonWidth - Styles.kButtonPadding * 2,
					Styles.kRenderersButtonHeight - Styles.kButtonPadding * 2), buttons, drawArea);
	}

	private void HandleAddRenderer (Rect position, IEnumerable<Rect> alreadyDrawn, Rect drawArea )
	{
		Event evt = Event.current;
		switch (evt.type)
		{
			case EventType.Repaint:
			{
				s_Styles.m_LODStandardButton.Draw (position, GUIContent.none, false, false, false, false);
				s_Styles.m_LODRendererAddButton.Draw(new Rect(position.x - Styles.kButtonPadding, position.y, position.width, position.height), "Add", false, false, false, false);
				break;
			}
			case EventType.DragUpdated:
			case EventType.DragPerform:
			{
				bool dragArea = false;
				if (drawArea.Contains (evt.mousePosition))
				{
					if (alreadyDrawn.All (x => !x.Contains (evt.mousePosition)))
						dragArea = true;
				}

				if (!dragArea)
					break;

				// If we are over a valid range, make sure we have a game object...
				if (DragAndDrop.objectReferences.Count() > 0)
				{
					DragAndDrop.visualMode = m_IsPrefab ? DragAndDropVisualMode.None : DragAndDropVisualMode.Copy;
					
					if (evt.type == EventType.DragPerform)
					{
						// First try gameobjects...
						var selectedGameObjects = 
							from go in DragAndDrop.objectReferences
							where go as GameObject != null
							select go as GameObject;

						var renderers = GetRenderers (selectedGameObjects, true);
						AddGameObjectRenderers (renderers, true);
						DragAndDrop.AcceptDrag();
						
						evt.Use ();
						break;
					}
				}
				evt.Use ();
				break;
			}
			case EventType.MouseDown:
			{
				if (position.Contains (evt.mousePosition))
				{
					evt.Use ();
					int id = "LODGroupSelector".GetHashCode ();
					ObjectSelector.get.Show (null, typeof (Renderer), null, true);
					ObjectSelector.get.objectSelectorID = id;
					GUIUtility.ExitGUI ();
				}
				break;
			}
			case EventType.ExecuteCommand:
			{
				string commandName = evt.commandName;
				if (commandName == "ObjectSelectorClosed" && ObjectSelector.get.objectSelectorID == "LODGroupSelector".GetHashCode())
				{
					AddGameObjectRenderers (GetRenderers (new List<GameObject> { ObjectSelector.GetCurrentObject () as GameObject }, true), true);
					evt.Use();
					GUIUtility.ExitGUI();
				}
				break;
			}
		}
	}
	
	private void DrawRendererButton (Rect position, int rendererIndex)
	{
		var renderersProperty = m_Object.FindProperty (string.Format (kRenderRootPath, activeLOD));
		var rendererRef = renderersProperty.GetArrayElementAtIndex (rendererIndex).FindPropertyRelative ("renderer");
		var renderer = rendererRef.objectReferenceValue as Renderer;

		var deleteButton = new Rect(position.xMax - Styles.kDeleteButtonSize, position.yMax - Styles.kDeleteButtonSize, Styles.kDeleteButtonSize, Styles.kDeleteButtonSize);

		Event evt = Event.current;
		switch (evt.type)
		{
			case EventType.Repaint:
			{
				if (renderer != null)
				{
					GUIContent content;
					
					var filter = renderer.GetComponent<MeshFilter>();
					if (filter != null && filter.sharedMesh != null)
						content = new GUIContent(AssetPreview.GetAssetPreview(filter.sharedMesh), renderer.gameObject.name);
					else if (renderer is SkinnedMeshRenderer)
						content = new GUIContent(AssetPreview.GetAssetPreview((renderer as SkinnedMeshRenderer).sharedMesh), renderer.gameObject.name);
					else
						content = new GUIContent (ObjectNames.NicifyVariableName (renderer.GetType ().Name), renderer.gameObject.name);
					
					s_Styles.m_LODBlackBox.Draw (position, GUIContent.none, false, false, false, false);

					GUIStyle s = "LODRendererButton";

					s.Draw(
						new Rect(
							position.x + Styles.kButtonPadding, 
							position.y + Styles.kButtonPadding,
							position.width - 2 * Styles.kButtonPadding, position.height - 2 * Styles.kButtonPadding), 
						content, false, false, false, false);
				}
				else
				{
					s_Styles.m_LODBlackBox.Draw (position, GUIContent.none, false, false, false, false);
					s_Styles.m_LODRendererButton.Draw (position, "<Empty>", false, false, false, false);
				}

				if (!m_IsPrefab)
				{
					s_Styles.m_LODBlackBox.Draw (deleteButton, GUIContent.none, false, false, false, false);
					s_Styles.m_LODRendererRemove.Draw (deleteButton, s_Styles.m_IconRendererMinus, false, false, false, false);
				}	
				break;
			}
			case EventType.MouseDown:
			{
				if (!m_IsPrefab && deleteButton.Contains (evt.mousePosition))
				{
					renderersProperty.DeleteArrayElementAtIndex (rendererIndex);
					evt.Use ();
					m_Object.ApplyModifiedProperties();
					LODUtility.CalculateLODGroupBoundingBox(target as LODGroup);
				}
				else if (position.Contains (evt.mousePosition))
				{
					EditorGUIUtility.PingObject (renderer);
					evt.Use ();
				}
				break;
			}
		}
	}
	
	// Get all the renderers that are attached to this game object
	private IEnumerable<Renderer> GetRenderers (IEnumerable<GameObject> selectedGameObjects, bool searchChildren)
	{
		// Only allow renderers that are parented to this LODGroup
		var lodGroup = target as LODGroup;

		if (lodGroup == null || EditorUtility.IsPersistent (lodGroup))
			return new List<Renderer> ();

		var validSearchObjects = from go in selectedGameObjects
			where go.transform.IsChildOf (lodGroup.transform)
			select go;

		var nonChildObjects = from go in selectedGameObjects
			where !go.transform.IsChildOf(lodGroup.transform)
			select go;

		// Handle reparenting
		var validChildren = new List<GameObject> ();
		if (nonChildObjects.Count() > 0)
		{
			const string kReparent = "Some objects are not children of the LODGroup GameObject. Do you want to reparent them and add them to the LODGroup?";
			if (EditorUtility.DisplayDialog(
				"Reparent GameObjects",
				kReparent,
				"Yes, Reparent",
				"No, Use Only Existing Children"))
			{
				foreach( var go in nonChildObjects )
				{
					if (EditorUtility.IsPersistent(go))
					{
						var newGo = Instantiate(go) as GameObject;
						if (newGo != null)
						{
							newGo.transform.parent = lodGroup.transform;
							newGo.transform.localPosition = Vector3.zero;
							newGo.transform.localRotation = Quaternion.identity;
							validChildren.Add (newGo);
						}
					}
					else
					{
						go.transform.parent = lodGroup.transform;
						validChildren.Add (go);
					}
				}
				validSearchObjects = validSearchObjects.Union(validChildren);
			}
		}

		//Get all the renderers
		var renderers = new List <Renderer> ();
		foreach (var go in validSearchObjects)
		{
			if (searchChildren)
				renderers.AddRange (go.GetComponentsInChildren <Renderer> ());
			else
				renderers.Add (go.GetComponent <Renderer> ());
		}
			
		// Then try renderers
		var selectedRenderers = from go in DragAndDrop.objectReferences
				where go as Renderer != null
				select go as Renderer;

		renderers.AddRange(selectedRenderers);
		return renderers;
	}
	
	// Add the given renderers to the current LOD group
	private void AddGameObjectRenderers (IEnumerable<Renderer> toAdd, bool add)
	{
		var renderersProperty = m_Object.FindProperty (string.Format (kRenderRootPath, activeLOD));
		
		if (!add)
			renderersProperty.ClearArray ();
		
		// On add make a list of the old renderers (to check for dupes)
		var oldRenderers = new List<Renderer>();
		for (var i = 0; i < renderersProperty.arraySize; i++)
		{
			var lodRenderRef = renderersProperty.GetArrayElementAtIndex (i).FindPropertyRelative ("renderer");
			var renderer = lodRenderRef.objectReferenceValue as Renderer;
			
			if (renderer == null)
				continue;
			
			oldRenderers.Add (renderer);
		}
		
		foreach (var renderer in toAdd)
		{
			// Ensure that we don't add the renderer if it already exists
			if (oldRenderers.Contains (renderer) )
				continue;
			
			renderersProperty.arraySize += 1;
			renderersProperty.
				GetArrayElementAtIndex (renderersProperty.arraySize - 1).
				FindPropertyRelative ("renderer").objectReferenceValue = renderer;
			
			// Stop readd
			oldRenderers.Add (renderer);
		}
		m_Object.ApplyModifiedProperties();
		LODUtility.CalculateLODGroupBoundingBox (target as LODGroup);
	}
	
	// Callabck action for mouse context clicks on the LOD slider(right click ect)
	private class LODAction
	{
		private readonly float m_Percentage;
		private readonly List<LODInfo> m_LODs;
		private readonly Vector2 m_ClickedPosition;
		private readonly SerializedObject m_ObjectRef;
		
		public delegate void Callback();
		private readonly Callback m_Callback;
		
		public LODAction (List<LODInfo> loDs, float percentage, Vector2 clickedPosition, SerializedObject objectRef, Callback callback)
		{
			m_LODs = loDs;
			m_Percentage = percentage;
			m_ClickedPosition = clickedPosition;
			m_ObjectRef = objectRef;
			m_Callback = callback;
		}
		
		public void InsertLOD ()
		{
			var lodArray = m_ObjectRef.FindProperty (kLODRootPath);
			
			if (!lodArray.isArray)
				return;
			
			// Find where to insert
			int insertIndex = -1;
			foreach (var lod in m_LODs) 
			{
				if (m_Percentage > lod.RawScreenPercent)
				{
					insertIndex = lod.m_LODLevel;
					break;
				}
			}
			
			// Clicked in the culled area... duplicate last
			if (insertIndex < 0)
			{
				lodArray.InsertArrayElementAtIndex (m_LODs.Count);
				insertIndex = m_LODs.Count;
			}
			else
			{
				lodArray.InsertArrayElementAtIndex (insertIndex);
			}

			// Null out the copied renderers (we want the list to be empty)
			var renderers = m_ObjectRef.FindProperty(string.Format(kRenderRootPath, insertIndex));
			renderers.arraySize = 0;
			
			var newLOD = lodArray.GetArrayElementAtIndex (insertIndex);
			newLOD.FindPropertyRelative("screenRelativeHeight").floatValue = m_Percentage;
			if (m_Callback != null) 
				m_Callback();

			m_ObjectRef.ApplyModifiedProperties ();
		}
		
		public void DeleteLOD ()
		{
			if (m_LODs.Count <= 0 )
				return;
			
			// Check for range click
			foreach (var lod in m_LODs) 
			{
				var numberOfRenderers = m_ObjectRef.FindProperty(string.Format(kRenderRootPath, lod.m_LODLevel)).arraySize;
				if (lod.m_RangePosition.Contains(m_ClickedPosition) && (numberOfRenderers == 0 
																		|| EditorUtility.DisplayDialog("Delete LOD",
																									"Are you sure you wish to delete this LOD?",
																									"Yes",
																									"No")))
				{
					var lodData = m_ObjectRef.FindProperty (string.Format (kLODDataPath, lod.m_LODLevel));
					lodData.DeleteCommand ();
					
					m_ObjectRef.ApplyModifiedProperties ();
					if (m_Callback != null) 
						m_Callback();
					break;
				}
			}
		}
	}
	
	private void DeletedLOD()
	{
		m_SelectedLOD--;
	}

	// Set the camera distance so that the current LOD group covers the desired percentage of the screen
	private static void UpdateCamera (float desiredPercentage, LODGroup group)
	{
		var worldReferencePoint = group.transform.TransformPoint(group.localReferencePoint);

		// Figure out a distance based on the percentage
		var distance = LODUtility.CalculateDistance(SceneView.lastActiveSceneView.camera, desiredPercentage <= 0.0f ? 0.000001f : desiredPercentage, group);

		if (SceneView.lastActiveSceneView.camera.orthographic)
			distance = Mathf.Sqrt((distance * distance) * (1 + SceneView.lastActiveSceneView.camera.aspect));

		SceneView.lastActiveSceneView.LookAtDirect(worldReferencePoint, SceneView.lastActiveSceneView.camera.transform.rotation, distance);
	}

	private void UpdateSelectedLODFromCamera(IEnumerable<LODInfo> lods, float cameraPercent)
	{
		foreach (var lod in lods)
		{
			if (cameraPercent > lod.RawScreenPercent)
			{
				m_SelectedLOD = lod.m_LODLevel;
				break;
			}
		}
	}

	private static float GetCameraPercentForCurrentQualityLevel (float clickPosition, float sliderStart, float sliderWidth)
	{
		var percentage = Mathf.Clamp (1.0f - (clickPosition - sliderStart)/sliderWidth, 0.01f, 1.0f);
		percentage = LinearizeScreenPercentage (percentage);
		return percentage;
	}

	private readonly int m_LODSliderId = "LODSliderIDHash".GetHashCode();
	private readonly int m_CameraSliderId = "LODCameraIDHash".GetHashCode();
	private void DrawLODLevelSlider (Rect sliderPosition, List<LODInfo> lods)
	{
		int sliderId = GUIUtility.GetControlID (m_LODSliderId, FocusType.Passive);
		int camerId = GUIUtility.GetControlID (m_CameraSliderId, FocusType.Passive);
		Event evt = Event.current;

		var group = target as LODGroup;
		if (group == null)
			return;

		switch (evt.GetTypeForControl (sliderId))
		{
			case EventType.Repaint:
			{
				// Draw the background... make it a few px bigger so that it looks nice.
				var backGround = sliderPosition;
				backGround.width += 2;
				backGround.height += 2;
				backGround.center -= new Vector2 (1.0f, 1.0f);
				s_Styles.m_LODSliderBG.Draw (sliderPosition, GUIContent.none, false, false, false, false);
				for (int i = 0; i < lods.Count; i++)
				{
					var lod = lods[i];
					DrawLODRange (lod, i == 0 ? 1.0f : lods[i - 1].RawScreenPercent);
					DrawLODButton (lod);
				}

				// Draw the last range (culled)
				DrawCulledRange (sliderPosition, lods.Count > 0 ? lods[lods.Count - 1].RawScreenPercent : 1.0f);
				break;
			}
			case EventType.MouseDown:
			{
				// Handle right click first
				if ( evt.button == 1 && sliderPosition.Contains (evt.mousePosition) )
				{
					var percentage = CalculatePercentageFromBar (sliderPosition, evt.mousePosition);
					var pm = new GenericMenu();
					if (lods.Count >= 8)
					{
						pm.AddDisabledItem(EditorGUIUtility.TextContent("Insert Before"));
					}
					else
					{
						pm.AddItem (EditorGUIUtility.TextContent ("Insert Before"), false,
						            new LODAction (lods, LinearizeScreenPercentage (percentage), evt.mousePosition, m_Object, null).
						            	InsertLOD);
					}

					// Figure out if we clicked in the culled region
					var disabledRegion = true;
					if (lods.Count > 0 && lods[lods.Count - 1].ScreenPercent < percentage)
						disabledRegion = false;

					if (disabledRegion)
						pm.AddDisabledItem(EditorGUIUtility.TextContent("Delete"));
					else
						pm.AddItem (EditorGUIUtility.TextContent ("Delete"), false,
						            new LODAction (lods, LinearizeScreenPercentage (percentage), evt.mousePosition, m_Object, DeletedLOD).
						            	DeleteLOD);
					pm.ShowAsContext();

					// Do selection
					bool selected = false;
					foreach (var lod in lods)
					{
						if (lod.m_RangePosition.Contains(evt.mousePosition))
						{
							m_SelectedLOD = lod.m_LODLevel;
							selected = true;
							break;
						}
					}

					if (!selected)
						m_SelectedLOD = -1;

					evt.Use();

					break;
				}

				// Slightly grow position on the x because edge buttons overflow by 5 pixels
				var barPosition = sliderPosition;
				barPosition.x -= 5;
				barPosition.width += 10;
				
				if (barPosition.Contains (evt.mousePosition)) 
				{
					evt.Use ();
					GUIUtility.hotControl = sliderId;
					
					// Check for button click
					var clickedButton = false;

					// case:464019 have to re-sort the LOD array for these buttons to get the overlaps in the right order...
					var lodsLeft = lods.Where (lod => lod.ScreenPercent > 0.5f).OrderByDescending (x => x.m_LODLevel);
					var lodsRight = lods.Where(lod => lod.ScreenPercent <= 0.5f).OrderBy(x => x.m_LODLevel);

					var lodButtonOrder = new List<LODInfo> ();
					lodButtonOrder.AddRange (lodsLeft);
					lodButtonOrder.AddRange (lodsRight);

					foreach (var lod in lodButtonOrder) 
					{
						if (lod.m_ButtonPosition.Contains (evt.mousePosition)) 
						{
							m_SelectedLODSlider = lod.m_LODLevel;
							clickedButton = true;

							if (SceneView.lastActiveSceneView != null && SceneView.lastActiveSceneView.camera != null && !m_IsPrefab)
							{
								// Bias by 0.1% so that there is no skipping when sliding
								UpdateCamera (lod.RawScreenPercent + 0.001f, group);
								SceneView.lastActiveSceneView.ClearSearchFilter ();
								SceneView.lastActiveSceneView.SetSceneViewFiltering (true);
								HierarchyProperty.FilterSingleSceneObject (group.gameObject.GetInstanceID (), false);
								SceneView.RepaintAll ();
							}

							break;
						}
					}
					
					if (!clickedButton)
					{
						// Check for range click
						foreach (var lod in lods) 
						{
							if (lod.m_RangePosition.Contains (evt.mousePosition)) 
							{
								m_SelectedLOD = lod.m_LODLevel;
								break;
							}
						}
					}
				}
				break;
			}
		
			case EventType.MouseDrag:
			{
				if (GUIUtility.hotControl == sliderId && m_SelectedLODSlider >= 0 && lods[m_SelectedLODSlider] != null) 
				{
					evt.Use ();

					var newScreenPercentage = Mathf.Clamp01 (1.0f - (evt.mousePosition.x - sliderPosition.x)/sliderPosition.width);
					// Bias by 0.1% so that there is no skipping when sliding
					SetSelectedLODLevelPercentage (newScreenPercentage - 0.001f, lods);

					if (SceneView.lastActiveSceneView != null && SceneView.lastActiveSceneView.camera != null && !m_IsPrefab)
					{
						UpdateCamera (LinearizeScreenPercentage(newScreenPercentage), group);
						SceneView.RepaintAll ();
					}
				}
				break;
			}
			
			case EventType.MouseUp:
			{
				if (GUIUtility.hotControl == sliderId) 
				{
					GUIUtility.hotControl = 0;
					m_SelectedLODSlider = -1;

					if (SceneView.lastActiveSceneView != null)
					{
						SceneView.lastActiveSceneView.SetSceneViewFiltering (false);
						SceneView.lastActiveSceneView.ClearSearchFilter ();
					}

					evt.Use ();
				}
				break;
			}
			
			case EventType.DragUpdated:
			case EventType.DragPerform:
			{
				// -2 = invalid region
				// -1 = culledregion
				// rest = LOD level
				var lodLevel = -2;
				// Is the mouse over a valid LOD level range?
				foreach (var lod in lods)
				{
					if (lod.m_RangePosition.Contains (evt.mousePosition))
					{
						lodLevel = lod.m_LODLevel;
						break;
					}
				}

				if (lodLevel == -2)
				{
					var culledRange = GetCulledBox (sliderPosition, lods.Count > 0 ? lods[lods.Count - 1].ScreenPercent : 1.0f);
					if (culledRange.Contains (evt.mousePosition))
					{
						lodLevel = -1;
					}
				}

				if (lodLevel >= -1)
				{
					// Actually set LOD level now
					m_SelectedLOD = lodLevel;
				
					if (DragAndDrop.objectReferences.Count() > 0)
					{
						DragAndDrop.visualMode = m_IsPrefab ? DragAndDropVisualMode.None : DragAndDropVisualMode.Copy; 
						
						if (evt.type == EventType.DragPerform)
						{
							// First try gameobjects...
							var selectedGameObjects = from go in DragAndDrop.objectReferences
													  where go as GameObject != null
													  select go as GameObject;
							var renderers = GetRenderers (selectedGameObjects, true);

							if( lodLevel == -1)
							{
								var lodArray = m_Object.FindProperty(kLODRootPath);
								lodArray.arraySize++;
								var pixelHeightNew = m_Object.FindProperty (string.Format (kPixelHeightDataPath,lods.Count));
								
								if( lods.Count == 0 )
									pixelHeightNew.floatValue = 0.5f;
								else
								{
									var pixelHeightPrevious = m_Object.FindProperty (string.Format (kPixelHeightDataPath,lods.Count-1));
									pixelHeightNew.floatValue = pixelHeightPrevious.floatValue / 2.0f;
								}
								
								m_SelectedLOD = lods.Count;
								AddGameObjectRenderers (renderers, false);
							}
							else
							{
								AddGameObjectRenderers (renderers, true);
							}
							DragAndDrop.AcceptDrag ();
						}
					}
					evt.Use ();
					break;
				}
				
				break;
			}
			case EventType.DragExited:
			{
				evt.Use ();
				break;
			}
		}
		if (SceneView.lastActiveSceneView != null && SceneView.lastActiveSceneView.camera != null && !m_IsPrefab)
		{
			var camera = SceneView.lastActiveSceneView.camera;

			var info = LODUtility.CalculateVisualizationData (camera, group, -1);
			var linearHeight = info.activeRelativeScreenSize/QualitySettings.lodBias;
			var relativeHeight = DelinearizeScreenPercentage (linearHeight);

			var vectorFromObjectToCamera =
				(SceneView.lastActiveSceneView.camera.transform.position - ((LODGroup)target).transform.position).normalized;

			if (Vector3.Dot(camera.transform.forward, vectorFromObjectToCamera) > 0f)
				relativeHeight = 1.0f;

			var cameraRect = CalcLODButton (sliderPosition, Mathf.Clamp01 (relativeHeight));
			var cameraIconRect = new Rect (cameraRect.center.x - 15, cameraRect.y - 25, 32, 32);
			var cameraLineRect = new Rect (cameraRect.center.x - 1, cameraRect.y, 2, cameraRect.height);
			var cameraPercentRect = new Rect(cameraIconRect.center.x - 5, cameraLineRect.yMax, 35, 20);

			switch (evt.GetTypeForControl(camerId))
			{
				case EventType.Repaint:
				{
					// Draw a marker to indicate the current scene camera distance
					var colorCache = GUI.backgroundColor;
					GUI.backgroundColor = new Color (colorCache.r, colorCache.g, colorCache.b, 0.8f);
					s_Styles.m_LODCameraLine.Draw (cameraLineRect, false, false, false, false);
					GUI.backgroundColor = colorCache;
					GUI.Label(cameraIconRect, s_Styles.m_CameraIcon, GUIStyle.none);
					s_Styles.m_LODSliderText.Draw (cameraPercentRect, String.Format ("{0:0}%", Mathf.Clamp01(linearHeight) * 100.0f), false, false, false, false);
					break;
				}
				case EventType.MouseDown:
				{
					if (cameraIconRect.Contains(evt.mousePosition))
					{
						evt.Use ();
						var cameraPercent = GetCameraPercentForCurrentQualityLevel (evt.mousePosition.x, sliderPosition.x, sliderPosition.width);
	
						UpdateCamera (cameraPercent, group);
						// Update the selected LOD to be where the camera is if we click the camera
						UpdateSelectedLODFromCamera(lods, cameraPercent);
						GUIUtility.hotControl = camerId;

						SceneView.lastActiveSceneView.ClearSearchFilter ();
						SceneView.lastActiveSceneView.SetSceneViewFiltering (true);
						HierarchyProperty.FilterSingleSceneObject (group.gameObject.GetInstanceID (), false);
						SceneView.RepaintAll ();
					}
					break;
				}
				case EventType.MouseDrag:
				{
					if (GUIUtility.hotControl == camerId)
					{
						evt.Use();
						var cameraPercent = GetCameraPercentForCurrentQualityLevel (evt.mousePosition.x, sliderPosition.x, sliderPosition.width);

						// Change the active LOD level if the camera moves into a new LOD level
						UpdateSelectedLODFromCamera (lods, cameraPercent);

						UpdateCamera (cameraPercent, group);
						SceneView.RepaintAll ();
					}
					break;
				}
				case EventType.MouseUp:
				{
					if (GUIUtility.hotControl == camerId)
					{
						SceneView.lastActiveSceneView.SetSceneViewFiltering (false);
						SceneView.lastActiveSceneView.ClearSearchFilter ();
						GUIUtility.hotControl = 0;
						evt.Use ();
					}
					break;
				}
			}
		}
	}
	
	void SetSelectedLODLevelPercentage( float newScreenPercentage, List<LODInfo> lods )
	{
		// Find the lower detail lod... clamp value to stop overlapping slider
		var lowerLOD = from lod in lods
						where lod.m_LODLevel == lods[m_SelectedLODSlider].m_LODLevel + 1
						select lod;
		var minimum = 0.0f;
		if( lowerLOD.FirstOrDefault () != null )
			minimum = lowerLOD.FirstOrDefault().ScreenPercent;
			
		// Find the higher detail lod... clamp value to stop overlapping slider
		var higherLOD = from lod in lods
						where lod.m_LODLevel == lods[m_SelectedLODSlider].m_LODLevel - 1
						select lod;
		var maximum = 1.0f;
		if( higherLOD.FirstOrDefault () != null )
			maximum = higherLOD.FirstOrDefault().ScreenPercent;
		
		maximum = Mathf.Clamp01( maximum );
		minimum = Mathf.Clamp01( minimum );
		
		// Set that value
		lods[m_SelectedLODSlider].ScreenPercent = Mathf.Clamp( newScreenPercentage, minimum, maximum);
		var percentageProperty = m_Object.FindProperty (string.Format (kPixelHeightDataPath, lods[m_SelectedLODSlider].m_LODLevel));
		percentageProperty.floatValue = lods[m_SelectedLODSlider].RawScreenPercent;
	}

	static void DrawLODButton (LODInfo currentLOD)
	{
		// Make the lod button areas a horizonal resizer
		EditorGUIUtility.AddCursorRect (currentLOD.m_ButtonPosition, MouseCursor.ResizeHorizontal); 
	}

	void DrawLODRange(LODInfo currentLOD, float previousLODPercentage)
	{
		var tempColor = GUI.backgroundColor;
		var startPercentageString = string.Format("LOD: {0}\n{1:0}%", currentLOD.m_LODLevel, previousLODPercentage * 100);
		if (currentLOD.m_LODLevel == activeLOD)
		{
			var foreground = currentLOD.m_RangePosition;
			foreground.width -= Styles.kSelectedLODRangePadding * 2;
			foreground.height -= Styles.kSelectedLODRangePadding * 2;
			foreground.center += new Vector2 (Styles.kSelectedLODRangePadding, Styles.kSelectedLODRangePadding);
			s_Styles.m_LODSliderRangeSelected.Draw (currentLOD.m_RangePosition, GUIContent.none, false, false, false, false);
			GUI.backgroundColor = kLODColors[currentLOD.m_LODLevel];
			if (foreground.width > 0)
				s_Styles.m_LODSliderRange.Draw (foreground, GUIContent.none, false, false, false, false);
			s_Styles.m_LODSliderText.Draw (currentLOD.m_RangePosition, startPercentageString, false, false, false, false);
		}
		else
		{
			GUI.backgroundColor = kLODColors[currentLOD.m_LODLevel];
			GUI.backgroundColor *= 0.6f;
			s_Styles.m_LODSliderRange.Draw (currentLOD.m_RangePosition, GUIContent.none, false, false, false, false);
			s_Styles.m_LODSliderText.Draw (currentLOD.m_RangePosition, startPercentageString, false, false, false, false);
		}
		GUI.backgroundColor = tempColor;
		
		}

	static Rect GetCulledBox (Rect totalRect, float previousLODPercentage)
	{
		var r = CalcLODRange (totalRect, previousLODPercentage, 0.0f);
		r.height -= 2;
		r.width -= 1;
		r.center += new Vector2 (0f, 1.0f);
		return r;
	}

	static void DrawCulledRange(Rect totalRect, float previousLODPercentage)
	{
		if( Mathf.Approximately( previousLODPercentage, 0.0f) ) return;

		var r = GetCulledBox (totalRect, DelinearizeScreenPercentage(previousLODPercentage));
		// Draw the range of a lod level on the slider
		var tempColor = GUI.color;
		GUI.color = kCulledLODColor;
		s_Styles.m_LODSliderRange.Draw( r, GUIContent.none, false, false, false, false );
		GUI.color = tempColor;
		
		// Draw some details for the current marker
		var startPercentageString = string.Format ("Culled\n{0:0}%", previousLODPercentage*100);
		s_Styles.m_LODSliderText.Draw (r, startPercentageString, false, false, false, false);
	}
	
	private static float CalculatePercentageFromBar (Rect totalRect, Vector2 clickPosition)
	{
		clickPosition.x -= totalRect.x;
		totalRect.x = 0.0f;
		
		return totalRect.width > 0.0f ? 1.0f - (clickPosition.x / totalRect.width) : 0.0f; 
	}
	
	private static Rect CalcLODButton (Rect totalRect, float percentage )
	{
		return new Rect (totalRect.x + (Mathf.Round (totalRect.width * (1.0f - percentage) ) ) - 5, totalRect.y, 10, totalRect.height);
	}
	
	private static Rect CalcLODRange (Rect totalRect, float startPercent, float endPercent)
	{
		var startX = Mathf.Round (totalRect.width * (1.0f - startPercent));
		var endX = Mathf.Round (totalRect.width * (1.0f - endPercent));
		
		return new Rect (totalRect.x + startX, totalRect.y, endX - startX, totalRect.height);
	}
	
	//Code to be able to send percentages to this gameobjects lightmap scale
	private class LODLightmapScale
	{
		public readonly float m_Scale;
		public readonly List<SerializedProperty> m_Renderers;

		public LODLightmapScale (float scale, List<SerializedProperty> renderers)
		{
			m_Scale = scale;
			m_Renderers = renderers;
		}
	}

	private void SendPercentagesToLightmapScale()
	{
		//List of renderers per LOD
		var lodRenderers = new List<LODLightmapScale>();
		
		for (var i = 0; i < m_NumberOfLODs; i++)
		{
			var renderersProperty = m_Object.FindProperty (string.Format (kRenderRootPath, i));
			var renderersAtLOD = new List<SerializedProperty> ();

			for (var k = 0; k < renderersProperty.arraySize; k++)
			{
				var rendererRef = renderersProperty.GetArrayElementAtIndex (k).FindPropertyRelative ("renderer");

				if (rendererRef != null)
					renderersAtLOD.Add(rendererRef);
			}
			var pixelHeight = i == 0 ? 1.0f : m_Object.FindProperty (string.Format (kPixelHeightDataPath, i-1)).floatValue;
			lodRenderers.Add (new LODLightmapScale (pixelHeight, renderersAtLOD));
		}

		for (var i = 0; i < m_NumberOfLODs; i++)
		{
			SetLODLightmapScale (lodRenderers[i]);
		}
	}

	private static void SetLODLightmapScale (LODLightmapScale lodRenderer)
	{
		foreach (var renderer in lodRenderer.m_Renderers)
		{
			var so = new SerializedObject (renderer.objectReferenceValue);
			var lightmapScaleProp = so.FindProperty ("m_ScaleInLightmap");
			lightmapScaleProp.floatValue = Mathf.Max(0.0f, lodRenderer.m_Scale * (1.0f / LightmapVisualization.GetLightmapLODLevelScale((Renderer)renderer.objectReferenceValue)));
			so.ApplyModifiedProperties ();
		}
	}

	// / PREVIEW GUI CODE BELOW
	public override bool HasPreviewGUI ()
	{
		return (target != null);
	}
	
	private PreviewRenderUtility m_PreviewUtility;
	static private readonly GUIContent[] kSLightIcons = {null, null};
	private Vector2 m_PreviewDir = new Vector2 (0,-20);

	public override void OnPreviewGUI (Rect r, GUIStyle background)
	{
		if (!ShaderUtil.hardwareSupportsRectRenderTexture)
		{
			if (Event.current.type == EventType.Repaint)
				EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 40), "LOD preview \nnot available");
			return;
		}

		InitPreview ();
		m_PreviewDir = PreviewGUI.Drag2D (m_PreviewDir, r);
		m_PreviewDir.y = Mathf.Clamp (m_PreviewDir.y, -89.0f, 89.0f);
		
		if (Event.current.type != EventType.Repaint)
			return;
		
		m_PreviewUtility.BeginPreview (r, background);
		
		DoRenderPreview();
		
		Texture renderedTexture = m_PreviewUtility.EndPreview ();
		GUI.DrawTexture (r, renderedTexture, ScaleMode.StretchToFill, false);
	}
	
	void InitPreview ()
	{
		if (m_PreviewUtility == null)
			m_PreviewUtility = new PreviewRenderUtility ();

		if (kSLightIcons[0] == null)
		{
			kSLightIcons[0] = EditorGUIUtility.IconContent ("PreMatLight0");
			kSLightIcons[1] = EditorGUIUtility.IconContent ("PreMatLight1");
		}
	}
	
	protected void DoRenderPreview()
	{
		if (m_PreviewUtility.m_RenderTexture.width <= 0 
			|| m_PreviewUtility.m_RenderTexture.height <= 0
			|| m_NumberOfLODs <= 0
			|| activeLOD < 0 )
			return;
		
		var bounds = new Bounds( Vector3.zero, Vector3.zero);
		bool boundsSet = false;
		
		var meshsToRender = new List<MeshFilter>();
		var renderers = m_Object.FindProperty (string.Format (kRenderRootPath, activeLOD));
		for (int i = 0; i < renderers.arraySize; i++)
		{
			var lodRenderRef = renderers.GetArrayElementAtIndex (i).FindPropertyRelative ("renderer");
			var renderer = lodRenderRef.objectReferenceValue as Renderer;
			
			if (renderer == null)
				continue;
			
			var meshFilter = renderer.GetComponent<MeshFilter> ();
			if( meshFilter != null && meshFilter.sharedMesh != null && meshFilter.sharedMesh.subMeshCount > 0 )
			{
				meshsToRender.Add (meshFilter);
			}
			
			if (!boundsSet)
			{
				bounds = renderer.bounds;
				boundsSet = true;
			}
			else
				bounds.Encapsulate (renderer.bounds);
		}
		
		if (!boundsSet)
			return;
		
		var halfSize = bounds.extents.magnitude;
		var distance = halfSize * 10.0f;
		
		var viewDir = -(m_PreviewDir / 100.0f);
		
		m_PreviewUtility.m_Camera.transform.position = bounds.center + (new Vector3 (Mathf.Sin (viewDir.x) * Mathf.Cos (viewDir.y), Mathf.Sin (viewDir.y), Mathf.Cos (viewDir.x)* Mathf.Cos (viewDir.y)) * distance);
		
		m_PreviewUtility.m_Camera.transform.LookAt (bounds.center);
		m_PreviewUtility.m_Camera.nearClipPlane = 0.05f;
		m_PreviewUtility.m_Camera.farClipPlane = 1000.0f;

		m_PreviewUtility.m_Light[0].intensity = .5f;
		m_PreviewUtility.m_Light[0].transform.rotation = Quaternion.Euler (50f, 50f, 0);
		m_PreviewUtility.m_Light[1].intensity = .5f;
		var amb = new Color (.2f, .2f, .2f, 0);

		InternalEditorUtility.SetCustomLighting (m_PreviewUtility.m_Light, amb);
		
		foreach (var meshFilter in meshsToRender)
		{
			for (int k = 0; k<meshFilter.sharedMesh.subMeshCount; k++)
			{
				if (k < meshFilter.renderer.sharedMaterials.Length)
				{
					var matrix = Matrix4x4.TRS (meshFilter.transform.position, meshFilter.transform.rotation, meshFilter.transform.localScale);
					m_PreviewUtility.DrawMesh ( 
						meshFilter.sharedMesh, 
						matrix,
						meshFilter.renderer.sharedMaterials[k],
						k);
				}
			}
		}
		
		bool oldFog = RenderSettings.fog;
		Unsupported.SetRenderSettingsUseFogNoDirty (false);
		m_PreviewUtility.m_Camera.Render ();
		Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
		InternalEditorUtility.RemoveCustomLighting ();
	}

	override public string GetInfoString()
	{
		if (SceneView.lastActiveSceneView == null
		    || SceneView.lastActiveSceneView.camera == null
		    || m_NumberOfLODs <= 0
		    || activeLOD < 0)
			return "";

		var materials = new List<Material>();
		var renderers = m_Object.FindProperty (string.Format (kRenderRootPath, activeLOD));
		for (int i = 0; i < renderers.arraySize; i++)
		{
			var renderRef = renderers.GetArrayElementAtIndex (i).FindPropertyRelative ("renderer");
			var renderer = renderRef.objectReferenceValue as Renderer;

			if (renderer != null)
				materials.AddRange (renderer.sharedMaterials);
		}

		var camera = SceneView.lastActiveSceneView.camera;
		var group = target as LODGroup;

		var info = LODUtility.CalculateVisualizationData(camera, group, activeLOD);
		return activeLOD != -1 ? string.Format( "{0} Renderer(s)\n{1} Triangle(s)\n{2} Material(s)", renderers.arraySize, info.triangleCount, materials.Distinct().Count()) : "LOD: culled";
	}
}
}
