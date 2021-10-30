using System;
using System.Linq;
using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;
using UnityEditor.VersionControl;
using Object = UnityEngine.Object;

namespace UnityEditor
{

[CustomEditor(typeof(GameObject))]
[CanEditMultipleObjects]
internal class GameObjectInspector : Editor
{
	SerializedProperty m_Name;
	SerializedProperty m_IsActive;
	SerializedProperty m_Layer;
	SerializedProperty m_Tag;
	SerializedProperty m_StaticEditorFlags;
	SerializedProperty m_Icon;
	
	class Styles
	{
		public GUIContent goIcon = EditorGUIUtility.IconContent ("GameObject Icon");
		public GUIContent typelessIcon = EditorGUIUtility.IconContent ("Prefab Icon");
		public GUIContent prefabIcon = EditorGUIUtility.IconContent ("PrefabNormal Icon");
		public GUIContent modelIcon = EditorGUIUtility.IconContent ("PrefabModel Icon");
		public GUIContent dataTemplateIcon = EditorGUIUtility.IconContent ("PrefabNormal Icon");
		public GUIContent dropDownIcon = EditorGUIUtility.IconContent ("Icon Dropdown");
		
		public float staticFieldToggleWidth = EditorStyles.toggle.CalcSize(EditorGUIUtility.TempContent("Static")).x + 6;
		public float tagFieldWidth = EditorStyles.boldLabel.CalcSize (EditorGUIUtility.TempContent("Tag")).x;
		public float layerFieldWidth = EditorStyles.boldLabel.CalcSize (EditorGUIUtility.TempContent("Layer")).x;
		public float navLayerFieldWidth = EditorStyles.boldLabel.CalcSize (EditorGUIUtility.TempContent("Nav Layer")).x;
			
		public GUIStyle staticDropdown = "StaticDropdown";
		
		public GUIStyle instanceManagementInfo = new GUIStyle (EditorStyles.helpBox);
		
		public GUIContent goTypeLabelMultiple = new GUIContent ("Multiple");
		
		public GUIContent[] goTypeLabel = 
		{
			null,//				None = 0,
			EditorGUIUtility.TextContent ("GameObjectTypePrefab"),              // Prefab = 1
			EditorGUIUtility.TextContent ("GameObjectTypeModel"),               // ModelPrefab = 2
			EditorGUIUtility.TextContent ("GameObjectTypePrefab"),              // PrefabInstance = 3
			EditorGUIUtility.TextContent ("GameObjectTypeModel"),               // ModelPrefabInstance = 4
			EditorGUIUtility.TextContent ("GameObjectTypeMissing"),             // MissingPrefabInstance
			EditorGUIUtility.TextContent ("GameObjectTypeDisconnectedPrefab"),   // DisconnectedPrefabInstance
			EditorGUIUtility.TextContent ("GameObjectTypeDisconnectedModel"),    // DisconnectedModelPrefabInstance
		};
		
		public Styles ()
		{
			GUIStyle miniButtonMid = "MiniButtonMid";
			instanceManagementInfo.padding = miniButtonMid.padding;
			instanceManagementInfo.alignment = miniButtonMid.alignment;
		}
	}
	static Styles s_styles;
	const float kTop = 4;
	const float kTop2 = 24;
	const float kTop3 = 44;
	const float kIconSize = 24;
	const float kLeft = 52;
	const float kToggleSize = 14;
	Vector2 previewDir;

	PreviewRenderUtility m_PreviewUtility;
	List<GameObject> m_PreviewInstances;
	const int kPreviewLayer = 31;

	bool m_HasInstance = false;
	bool m_AllOfSamePrefabType = true;

	GameObjectInspector()
	{
#if ENABLE_SPRITES
		if (EditorSettings.defaultBehaviorMode == EditorBehaviorMode.Mode2D)
			previewDir = new Vector2(0, 0);
		else
#endif
			previewDir = new Vector2(120, -20);
	}

	public void OnEnable ()
	{
		m_Name = serializedObject.FindProperty("m_Name");
		m_IsActive = serializedObject.FindProperty("m_IsActive");
		m_Layer = serializedObject.FindProperty("m_Layer");
		m_Tag = serializedObject.FindProperty("m_TagString");
		m_StaticEditorFlags = serializedObject.FindProperty("m_StaticEditorFlags");
		m_Icon = serializedObject.FindProperty ("m_Icon");
		
		CalculatePrefabStatus ();
	}
	
	void CalculatePrefabStatus ()
	{
		m_HasInstance = false;
		m_AllOfSamePrefabType = true;
		PrefabType firstType = PrefabUtility.GetPrefabType (targets[0] as GameObject);
		foreach (GameObject go in targets)
		{
			PrefabType type = PrefabUtility.GetPrefabType (go);
			if (type != firstType)
				m_AllOfSamePrefabType = false;
			if (type != PrefabType.None && type != PrefabType.Prefab && type != PrefabType.ModelPrefab)
				m_HasInstance = true;
		}
	}
		
	
	void OnDisable (){}


	private static bool ShowMixedStaticEditorFlags(StaticEditorFlags mask)
	{
		uint countedBits = 0;
		uint numFlags = 0;
		foreach (var i in Enum.GetValues (typeof (StaticEditorFlags)))
		{
			numFlags++;
			if ((mask & (StaticEditorFlags) i) > 0)
				countedBits++;
		}

		//If we have more then one selected... but it is not all the flags
		//All indictates 'everything' which means it should be a tick!
		return countedBits > 0 && countedBits != numFlags;
	}

	protected override void OnHeaderGUI ()
	{
		Rect contentRect = GUILayoutUtility.GetRect (0, m_HasInstance ? 60 : 40);
		DrawInspector (contentRect);
	}
	
	public override void OnInspectorGUI () { }
	
	internal bool DrawInspector (Rect contentRect)
	{
		if (s_styles == null)
			s_styles = new Styles ();

		serializedObject.Update ();
		
		GameObject go = target as GameObject;
		EditorGUIUtility.labelWidth = 52;
		
		bool enabledTemp = GUI.enabled;
		GUI.enabled = true;
		GUI.Label (new Rect (contentRect.x, contentRect.y, contentRect.width, contentRect.height + 3), GUIContent.none, EditorStyles.inspectorBig);
		GUI.enabled = enabledTemp;

		float width = contentRect.width;
		float top = contentRect.y;

		GUIContent iconContent = null;

		PrefabType prefabType = PrefabType.None;
		// Leave iconContent to be null if multiple objects not the same type.
		if (m_AllOfSamePrefabType)
		{
			prefabType = PrefabUtility.GetPrefabType(go);
			switch (prefabType)
			{
				case PrefabType.None:
					iconContent = s_styles.goIcon;
					break;
				case PrefabType.Prefab:
				case PrefabType.PrefabInstance:
				case PrefabType.DisconnectedPrefabInstance:
					iconContent = s_styles.prefabIcon;
					break;
				case PrefabType.ModelPrefab:
				case PrefabType.ModelPrefabInstance:
				case PrefabType.DisconnectedModelPrefabInstance:
					iconContent = s_styles.modelIcon;
					break;
				case PrefabType.MissingPrefabInstance:
					iconContent = s_styles.prefabIcon;
					break;
			}
		}
		else
			iconContent = s_styles.typelessIcon;

		EditorGUI.ObjectIconDropDown (new Rect (3, kTop + top, kIconSize, kIconSize), targets, true, iconContent.image as Texture2D, m_Icon);
		
		EditorGUI.BeginDisabledGroup (prefabType == PrefabType.ModelPrefab);
		
		// IsActive
		EditorGUI.PropertyField(new Rect (34, kTop + top, kToggleSize, kToggleSize), m_IsActive, GUIContent.none);

		// Name
		var staticTotalwidth = s_styles.staticFieldToggleWidth + 15;
		float nameFieldWidth = width - kLeft - staticTotalwidth - 5;
		EditorGUI.BeginChangeCheck();
		EditorGUI.showMixedValue = m_Name.hasMultipleDifferentValues;
		string newName = EditorGUI.DelayedTextField(new Rect(kLeft, kTop + top + 1, nameFieldWidth, kLineHeight), go.name, null, EditorStyles.textField);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck ())
		{
			foreach (Object obj in targets)
				ObjectNames.SetNameSmart (obj as GameObject, newName);
		}
			
		// Static flags toggle
		var staticRect = new Rect (width - staticTotalwidth, kTop + top, s_styles.staticFieldToggleWidth, kLineHeight);
		EditorGUI.BeginProperty (staticRect, GUIContent.none, m_StaticEditorFlags);
		EditorGUI.BeginChangeCheck();
		var toggleRect = staticRect;
		EditorGUI.showMixedValue |= ShowMixedStaticEditorFlags ((StaticEditorFlags)m_StaticEditorFlags.intValue);
		// Ignore mouse clicks that are not with the primary (left) mouse button so those can be grabbed by other things later.
		Event evt = Event.current;
		EventType origType = evt.type;
		bool nonLeftClick = (evt.type == EventType.MouseDown && evt.button != 0);
		if (nonLeftClick)
			evt.type = EventType.Ignore;
		var toggled = EditorGUI.ToggleLeft(toggleRect, "Static", go.isStatic);
		if (nonLeftClick)
			evt.type = origType;
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
		{
			SceneModeUtility.SetStaticFlags (targets, ~0, toggled);
			serializedObject.SetIsDifferentCacheDirty ();
		}
		EditorGUI.EndProperty ();

		// Static flags dropdown
		EditorGUI.BeginChangeCheck();
		EditorGUI.showMixedValue = m_StaticEditorFlags.hasMultipleDifferentValues;
		int changedFlags;
		bool changedToValue;
		EditorGUI.EnumMaskField(
			new Rect (staticRect.x + s_styles.staticFieldToggleWidth, staticRect.y, 10, 14),
			GameObjectUtility.GetStaticEditorFlags(go),
			s_styles.staticDropdown,
			out changedFlags, out changedToValue
			);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck())
		{
			SceneModeUtility.SetStaticFlags (targets, changedFlags, changedToValue);
			serializedObject.SetIsDifferentCacheDirty ();
		}
		float spacing = 4;
		float margin = 4;
		EditorGUIUtility.fieldWidth = (width - spacing - kLeft - s_styles.layerFieldWidth - margin) / 2;
		Rect r;
		
		// Tag
		string tagName = null;
		try
		{
			tagName = go.tag;
		}
		catch (System.Exception)
		{
			tagName = "Undefined";
		}
		EditorGUIUtility.labelWidth = s_styles.tagFieldWidth;
		r = new Rect (kLeft - EditorGUIUtility.labelWidth, kTop2 + top, EditorGUIUtility.labelWidth + EditorGUIUtility.fieldWidth, kLineHeight);
		EditorGUI.BeginProperty (r, GUIContent.none, m_Tag);
		EditorGUI.BeginChangeCheck ();
		string tag = EditorGUI.TagField (r, EditorGUIUtility.TempContent ("Tag"), tagName);
		if (EditorGUI.EndChangeCheck ())
		{
			m_Tag.stringValue = tag;
			Undo.RecordObjects (targets, "Change Tag of " + targetTitle);
			foreach (Object obj in targets)
				(obj as GameObject).tag = tag;
		}
		EditorGUI.EndProperty ();

		// Layer
		EditorGUIUtility.labelWidth = s_styles.layerFieldWidth;
		r = new Rect (kLeft + EditorGUIUtility.fieldWidth + spacing, kTop2 + top, EditorGUIUtility.labelWidth + EditorGUIUtility.fieldWidth, kLineHeight);
		EditorGUI.BeginProperty (r, GUIContent.none, m_Layer);
		EditorGUI.BeginChangeCheck ();
		int layer =  EditorGUI.LayerField (r, EditorGUIUtility.TempContent ("Layer"), go.layer);
		if (EditorGUI.EndChangeCheck ())
		{
			GameObjectUtility.ShouldIncludeChildren includeChildren = GameObjectUtility.DisplayUpdateChildrenDialogIfNeeded (targets.OfType<GameObject> (),
				"Change Layer", "Do you want to set layer to " + InternalEditorUtility.GetLayerName (layer) + " for all child objects as well?");
			if (includeChildren != GameObjectUtility.ShouldIncludeChildren.Cancel)
			{
				m_Layer.intValue = layer;
				SetLayer (layer, includeChildren == GameObjectUtility.ShouldIncludeChildren.IncludeChildren);
			}
		}
		EditorGUI.EndProperty ();

		// @TODO: If/when we support multi-editing of prefab/model instances,
		// handle it here. Only show prefan bar if all are same type?
		if (m_HasInstance && !EditorApplication.isPlayingOrWillChangePlaymode)
		{
			float elemWidth = (width - kLeft - 5) / 3;
			Rect b1Rect = new Rect (kLeft + elemWidth * 0, kTop3 + top, elemWidth, kLineHeight - 1);
			Rect b2Rect = new Rect (kLeft + elemWidth * 1, kTop3 + top, elemWidth, kLineHeight - 1);
			Rect b3Rect = new Rect (kLeft + elemWidth * 2, kTop3 + top, elemWidth, kLineHeight - 1);
			Rect bAllRect = new Rect (kLeft, kTop3 + top, elemWidth * 3, kLineHeight - 1);
			
			// Prefab information
			GUIContent prefixLabel = targets.Length > 1 ? s_styles.goTypeLabelMultiple : s_styles.goTypeLabel[(int)prefabType];
			
			if (prefixLabel != null)
			{
				float prefixWidth = GUI.skin.label.CalcSize (prefixLabel).x;
							
				if (prefabType == PrefabType.DisconnectedModelPrefabInstance || prefabType == PrefabType.MissingPrefabInstance || prefabType == PrefabType.DisconnectedPrefabInstance)
				{
					GUI.contentColor = GUI.skin.GetStyle ("CN StatusWarn").normal.textColor;
					if (prefabType == PrefabType.MissingPrefabInstance)
						GUI.Label (new Rect (kLeft, kTop3 + top, (width - kLeft - 5), kLineHeight + 2), prefixLabel, EditorStyles.whiteLabel);
					else
						GUI.Label (new Rect (kLeft - prefixWidth - 5, kTop3 + top, (width - kLeft - 5), kLineHeight + 2), prefixLabel, EditorStyles.whiteLabel);
					GUI.contentColor = Color.white;
				}
				else
				{
					Rect prefixRect = new Rect (kLeft - prefixWidth - 5, kTop3 + top, prefixWidth, kLineHeight + 2);
					GUI.Label (prefixRect, prefixLabel);
				}
			}
			
			if (targets.Length > 1)
			{
				GUI.Label (bAllRect, "Instance Management Disabled", s_styles.instanceManagementInfo);
			}
			else
			{
				// Select prefab
				if (prefabType != PrefabType.MissingPrefabInstance)
				{
					if (GUI.Button (b1Rect, "Select", "MiniButtonLeft"))
					{
						Selection.activeObject = PrefabUtility.GetPrefabParent (target);
						EditorGUIUtility.PingObject (Selection.activeObject);
					}
				}
				
				// Reconnect prefab
				if (prefabType == PrefabType.DisconnectedModelPrefabInstance || prefabType == PrefabType.DisconnectedPrefabInstance)
				{
					if (GUI.Button (b2Rect, "Revert", "MiniButtonMid"))
					{
						Undo.RegisterFullObjectHierarchyUndo(go);
					
						PrefabUtility.ReconnectToLastPrefab(go);
						PrefabUtility.RevertPrefabInstance(go);
						CalculatePrefabStatus ();
						
						Undo.RegisterCreatedObjectUndo(go, "Reconnect prefab");
					}
				}
				
				// Revert this gameobject and components to prefab
				if (prefabType == PrefabType.ModelPrefabInstance || prefabType == PrefabType.PrefabInstance)
				{
					if (GUI.Button (b2Rect, "Revert", "MiniButtonMid"))
					{
						Undo.RegisterCompleteObjectUndo (go, "Revert Prefab Instance");
						PrefabUtility.RevertPrefabInstance(go);
						CalculatePrefabStatus ();
					}
				}

				// Apply to prefab
				if (prefabType == PrefabType.PrefabInstance || prefabType == PrefabType.DisconnectedPrefabInstance)
				{
					GameObject rootUploadGameObject = PrefabUtility.FindValidUploadPrefabInstanceRoot(go);
					
					bool enabledPrefabTemp = GUI.enabled;
					GUI.enabled = rootUploadGameObject != null;

					if (GUI.Button (b3Rect, "Apply", "MiniButtonRight"))
					{
						Object prefabParent = PrefabUtility.GetPrefabParent (rootUploadGameObject);
						string prefabAssetPath = AssetDatabase.GetAssetPath(prefabParent);

						bool editablePrefab  = Provider.PromptAndCheckoutIfNeeded(
							new string[] { prefabAssetPath },
							"The version control requires you to checkout the prefab before applying changes.");
						

						if (editablePrefab)
						{
							PrefabUtility.ReplacePrefab(rootUploadGameObject, prefabParent, ReplacePrefabOptions.ConnectToPrefab);
							CalculatePrefabStatus ();
	
							// This is necessary because ReplacePrefab can potentially destroy game objects and components
							// In that case the Editor classes would be destroyed but still be invoked. (case 468434)
							GUIUtility.ExitGUI();					
						}
					}
					GUI.enabled = enabledPrefabTemp;
				}
				
				// Edit model prefab
				if (prefabType == PrefabType.DisconnectedModelPrefabInstance || prefabType == PrefabType.ModelPrefabInstance)
				{
					if (GUI.Button (b3Rect, "Open", "MiniButtonRight"))
					{
						AssetDatabase.OpenAsset(PrefabUtility.GetPrefabParent (target));
						GUIUtility.ExitGUI();					
					}
				}
			}
		}
		
		EditorGUI.EndDisabledGroup ();
		
		serializedObject.ApplyModifiedProperties ();
		
		return true;
	}

	Object[] GetObjects (bool includeChildren)
	{
		return SceneModeUtility.GetObjects (targets, includeChildren);
	}

	void SetLayer (int layer, bool includeChildren)
	{
		Object[] objects = GetObjects (includeChildren);
		Undo.RecordObjects (objects, "Change Layer of " + targetTitle);
		foreach (GameObject go in objects)
			go.layer = layer;
	}

	public static void InitInstantiatedPreviewRecursive(GameObject go)
	{
		go.hideFlags = HideFlags.HideAndDontSave;
		go.layer = kPreviewLayer;

		foreach (Transform c in go.transform)
			InitInstantiatedPreviewRecursive(c.gameObject);
	}

	public static void SetEnabledRecursive(GameObject go, bool enabled)
	{
		foreach (Renderer renderer in go.GetComponentsInChildren<Renderer>())
			renderer.enabled = enabled;
	}

	void CreatePreviewInstances()
	{
		DestroyPreviewInstances();

		if (m_PreviewInstances == null)
			m_PreviewInstances = new List<GameObject>(targets.Length);

		for (int i = 0; i < targets.Length; ++i)
		{
			GameObject instance = EditorUtility.InstantiateRemoveAllNonAnimationComponents(
				targets[i], Vector3.zero, Quaternion.identity) as GameObject;
			InitInstantiatedPreviewRecursive(instance);
			Animator animator = instance.GetComponent(typeof(Animator)) as Animator;
			if (animator)
			{
				animator.enabled = false;
				animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;
				animator.logWarnings = false;
				animator.fireEvents = false;
			}
			SetEnabledRecursive(instance, false);
			m_PreviewInstances.Add(instance);
		}
	}

	void DestroyPreviewInstances()
	{
		if (m_PreviewInstances == null || m_PreviewInstances.Count == 0)
			return;

		foreach (GameObject instance in m_PreviewInstances)
			Object.DestroyImmediate(instance);
		m_PreviewInstances.Clear();
	}

	void InitPreview ()
	{
		if (m_PreviewUtility == null)
		{
			m_PreviewUtility = new PreviewRenderUtility(true);
			m_PreviewUtility.m_CameraFieldOfView = 30.0f;
			m_PreviewUtility.m_Camera.cullingMask = 1 << kPreviewLayer;
			CreatePreviewInstances();
		}
	}
	public void OnDestroy ()
	{
		DestroyPreviewInstances();
		if (m_PreviewUtility != null)
		{
			m_PreviewUtility.Cleanup ();
			m_PreviewUtility = null;
		}
	}

	public static bool HasRenderablePartsRecurse (GameObject go)
	{
		// Do we have a mesh?
		MeshRenderer renderer = go.GetComponent (typeof (MeshRenderer)) as MeshRenderer;
		MeshFilter filter = go.GetComponent (typeof (MeshFilter)) as MeshFilter;
		if (renderer && filter && filter.sharedMesh)
			return true;

		// Do we have a skinned mesh?
		SkinnedMeshRenderer skin = go.GetComponent (typeof (SkinnedMeshRenderer)) as SkinnedMeshRenderer;
		if (skin && skin.sharedMesh)
			return true;

#if ENABLE_SPRITES
		// Do we have a Sprite?
		SpriteRenderer sprite = go.GetComponent (typeof(SpriteRenderer)) as SpriteRenderer;
		if (sprite && sprite.sprite)
			return true;
#endif

		// Recurse into children
		foreach (Transform t in go.transform)
		{
			if (HasRenderablePartsRecurse (t.gameObject))
				return true;
		}

		// Nope, we don't have it.
		return false;
	}

	public static void GetRenderableBoundsRecurse (ref Bounds bounds, GameObject go)
	{
		// Do we have a mesh?
		MeshRenderer renderer = go.GetComponent (typeof (MeshRenderer)) as MeshRenderer;
		MeshFilter filter = go.GetComponent (typeof (MeshFilter)) as MeshFilter;
		if (renderer && filter && filter.sharedMesh)
		{
			// To prevent origo from always being included in bounds we initialize it 
			// with renderer.bounds. This ensures correct bounds for meshes with origo outside the mesh.
			if (bounds.extents == Vector3.zero)
				bounds = renderer.bounds;	
			else
				bounds.Encapsulate(renderer.bounds);	
		}

		// Do we have a skinned mesh?
		SkinnedMeshRenderer skin = go.GetComponent (typeof (SkinnedMeshRenderer)) as SkinnedMeshRenderer;
		if (skin && skin.sharedMesh)
		{
			if (bounds.extents == Vector3.zero)
				bounds = skin.bounds;
			else
				bounds.Encapsulate(skin.bounds);
		}

#if ENABLE_SPRITES
		// Do we have a Sprite?
		SpriteRenderer sprite = go.GetComponent(typeof(SpriteRenderer)) as SpriteRenderer;
		if (sprite && sprite.sprite)
		{
			if (bounds.extents == Vector3.zero)
				bounds = sprite.bounds;
			else
				bounds.Encapsulate(sprite.bounds);
		}
#endif

		// Recurse into children
		foreach (Transform t in go.transform)
		{
			GetRenderableBoundsRecurse (ref bounds, t.gameObject);
		}
	}

	private static float GetRenderableCenterRecurse(ref Vector3 center, GameObject go, int depth, int minDepth, int maxDepth)
    {
		if (depth > maxDepth)
			return 0;

		float ret = 0;

		if (depth > minDepth)
		{
			// Do we have a mesh?
			MeshRenderer renderer = go.GetComponent(typeof (MeshRenderer)) as MeshRenderer;
			MeshFilter filter = go.GetComponent(typeof (MeshFilter)) as MeshFilter;
			SkinnedMeshRenderer skin = go.GetComponent(typeof (SkinnedMeshRenderer)) as SkinnedMeshRenderer;
#if ENABLE_SPRITES
			SpriteRenderer sprite = go.GetComponent(typeof(SpriteRenderer)) as SpriteRenderer;
#endif

			if (renderer == null && filter == null && skin == null
#if ENABLE_SPRITES
				&& sprite == null
#endif
				)
			{
				ret = 1;
				center = center + go.transform.position;
			}
			else if (renderer != null && filter != null)
			{
				// case 542145, epsilon is too small. Accept up to 1 centimeter before discarding this model.
				if (Vector3.Distance(renderer.bounds.center, go.transform.position) < 0.01F)
				{
					ret = 1;
					center = center + go.transform.position;
				}
			}
			else if (skin != null)
			{
				// case 542145, epsilon is too small. Accept up to 1 centimeter before discarding this model.
				if (Vector3.Distance(skin.bounds.center, go.transform.position) < 0.01F)
				{
					ret = 1;
					center = center + go.transform.position;
				}
			}
#if ENABLE_SPRITES
			else if (sprite != null)
			{
				if (Vector3.Distance(sprite.bounds.center, go.transform.position) < 0.01F)
				{
					ret = 1;
					center = center + go.transform.position;
				}
			}
#endif
		}

		depth++;
    	// Recurse into children
        foreach (Transform t in go.transform)
        {
			ret += GetRenderableCenterRecurse(ref center, t.gameObject, depth, minDepth, maxDepth);
        }

        return ret;
    }

	public static Vector3 GetRenderableCenterRecurse(GameObject go, int minDepth, int maxDepth)
    {
		Vector3 center = Vector3.zero;

        float sum = GetRenderableCenterRecurse(ref center, go, 0, minDepth, maxDepth);

        if (sum > 0)
        {
            center = center / sum;
        }
        else
        {
            center = go.transform.position;
        }

        return center;
    }
 
	public override bool HasPreviewGUI ()
	{
		if (targets.Length > 1)
			return true;
		
		if (target == null)
			return false;
		
		GameObject go = target as GameObject;

		// Is this a camera?
		Camera camera = go.GetComponent (typeof (Camera)) as Camera;
		if ( camera )
			return true;
		
		return HasRenderablePartsRecurse (go);
	}

	public override void OnPreviewSettings ()
	{
		if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			return;
		GUI.enabled = true;
		InitPreview ();
	}

	private void DoRenderPreview()
	{
		GameObject go = m_PreviewInstances[referenceTargetIndex];

		Bounds bounds = new Bounds (go.transform.position, Vector3.zero);
		GetRenderableBoundsRecurse (ref bounds, go);
		float halfSize = Mathf.Max (bounds.extents.magnitude, 0.0001f);
		float distance = halfSize * 3.8f;

		Quaternion rot = Quaternion.Euler(-previewDir.y, -previewDir.x, 0);
		Vector3 pos = bounds.center - rot * (Vector3.forward * distance);

		m_PreviewUtility.m_Camera.transform.position = pos;
		m_PreviewUtility.m_Camera.transform.rotation = rot;
		m_PreviewUtility.m_Camera.nearClipPlane = distance - halfSize * 1.1f;
		m_PreviewUtility.m_Camera.farClipPlane = distance + halfSize * 1.1f;

		m_PreviewUtility.m_Light[0].intensity = .7f;
		m_PreviewUtility.m_Light[0].transform.rotation = rot * Quaternion.Euler(40f, 40f, 0);
		m_PreviewUtility.m_Light[1].intensity = .7f;
		m_PreviewUtility.m_Light[1].transform.rotation = rot * Quaternion.Euler(340, 218, 177);
		Color amb = new Color (.1f, .1f, .1f, 0);

		InternalEditorUtility.SetCustomLighting (m_PreviewUtility.m_Light, amb);
		bool oldFog = RenderSettings.fog;
		Unsupported.SetRenderSettingsUseFogNoDirty (false);

		SetEnabledRecursive(go, true);
		m_PreviewUtility.m_Camera.Render ();
		SetEnabledRecursive(go, false);
		
		Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
		InternalEditorUtility.RemoveCustomLighting ();
	}
	
	public override Texture2D RenderStaticPreview(string assetPath, Object[] subAssets, int width, int height)
	{
		if (!HasPreviewGUI() || !ShaderUtil.hardwareSupportsRectRenderTexture) {
			return null;
		}
		
		InitPreview ();

		m_PreviewUtility.BeginStaticPreview (new Rect(0,0,width,height));
		
		DoRenderPreview();

		return m_PreviewUtility.EndStaticPreview ();
	}

	
	public override void OnPreviewGUI (Rect r, GUIStyle background)
	{
		if (!ShaderUtil.hardwareSupportsRectRenderTexture)
		{
			if (Event.current.type == EventType.Repaint)
				EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 40), "Preview requires\nrender texture support");
			return;
		}
		InitPreview ();

		previewDir = PreviewGUI.Drag2D (previewDir, r);

		if (Event.current.type != EventType.Repaint)
			return;

		m_PreviewUtility.BeginPreview (r, background);

		DoRenderPreview();
		
		Texture renderedTexture = m_PreviewUtility.EndPreview ();
		GUI.DrawTexture (r, renderedTexture, ScaleMode.StretchToFill, false);
	}
	
	// Handle dragging in scene view
	public static GameObject dragObject;
	
	public void OnSceneDrag (SceneView sceneView)
	{		
		GameObject go = target as GameObject;
		PrefabType prefabType = PrefabUtility.GetPrefabType(go);
		if (prefabType != PrefabType.Prefab && prefabType != PrefabType.ModelPrefab)
			return;
			
		Event evt = Event.current;
		switch (evt.type) {
		case EventType.DragUpdated:
			if (dragObject == null)
			{
				dragObject = (GameObject)PrefabUtility.InstantiatePrefab(PrefabUtility.FindPrefabRoot(go));
				HandleUtility.ignoreRaySnapObjects = dragObject.GetComponentsInChildren<Transform>();
				dragObject.hideFlags = HideFlags.HideInHierarchy;
				dragObject.name = go.name;	
			}
			DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
			object hit = HandleUtility.RaySnap (HandleUtility.GUIPointToWorldRay(evt.mousePosition));
			if (hit != null) 
			{
				RaycastHit rh = (RaycastHit)hit;
				float offset = 0;
				if (Tools.pivotMode == PivotMode.Center)
				{
					float geomOffset = HandleUtility.CalcRayPlaceOffset (HandleUtility.ignoreRaySnapObjects, rh.normal);
					if (geomOffset != Mathf.Infinity)
						offset = Vector3.Dot (dragObject.transform.position, rh.normal) - geomOffset;
				}
				dragObject.transform.position = Matrix4x4.identity.MultiplyPoint (rh.point + (rh.normal * offset));
			}
			else
				dragObject.transform.position = HandleUtility.GUIPointToWorldRay(evt.mousePosition).GetPoint (10);

#if ENABLE_SPRITES

			// Use prefabs original z position when in 2D mode
			if (sceneView.in2DMode)
			{
				Vector3 dragPosition = dragObject.transform.position;
				dragPosition.z = PrefabUtility.FindPrefabRoot(go).transform.position.z;
				dragObject.transform.position = dragPosition;
			}
#endif

			evt.Use ();
			break;
		case EventType.DragPerform:
			dragObject.hideFlags = 0;
			Undo.RegisterCreatedObjectUndo(dragObject, "Place " + dragObject.name);
			EditorUtility.SetDirty (dragObject);
			DragAndDrop.AcceptDrag ();
			Selection.activeObject = dragObject;
			HandleUtility.ignoreRaySnapObjects = null;
			SceneView.mouseOverWindow.Focus ();
			dragObject = null;
			evt.Use ();
			break;
		case EventType.DragExited:
			if (dragObject)
			{
				Object.DestroyImmediate(dragObject, false);
				HandleUtility.ignoreRaySnapObjects = null;
				dragObject = null;
				evt.Use();
			}
			break;
		}
	}

}

}
