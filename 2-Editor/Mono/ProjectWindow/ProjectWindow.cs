using UnityEditor.ProjectWindowCallback;
using UnityEngine;
using UnityEditorInternal;
using UnityEditorInternal.VersionControl;
using System.Collections;
using System.IO;
using System.Collections.Generic;

namespace UnityEditor {
// Used to drag the mouse down location, so that we can delay a drag start event until the mouse was dragged a couple of pixels
internal class DragAndDropDelay
{
	public Vector2 mouseDownPosition;
	
	public bool CanStartDrag ()
	{
		return Vector2.Distance(mouseDownPosition, Event.current.mousePosition) > 6;
	}
}

internal class HierarchyWindow : BaseProjectWindow
{	
	public HierarchyWindow ()
	{
		m_HierarchyType = HierarchyType.GameObjects;
	}
}

internal class BaseProjectWindow : SearchableEditorWindow
{
	const double kDropExpandTimeout = 0.7;
	float IconWidth { get { return m_HierarchyType == HierarchyType.Assets ? 14 : -3; } }
	static float m_RowHeight = 16;
	static float m_DropNextToAreaHeight = 6; // Pixel height in which the drop next to target ui is used instead of drop onto.
	const float kFoldoutSize = 14;
	const float kIndent = 16;
	const float kBaseIndent = kFoldoutSize + 3f;
	[SerializeField]
	int[] m_ExpandedArray = new int[0];
	bool m_ProjectViewInternalSelectionChange = false; // to know when selection change originated in project view itself

	public Vector2 m_ScrollPosition;
	public Vector2 m_ScrollPositionFiltered;

	[System.NonSerialized]
	Rect    m_ScreenRect;
	
	PingData m_Ping = new PingData();

	float m_FocusTime;

    List<int> m_CurrentDragSelectionIDs = new List<int>();	// To make selection a bit nicer. In this window items are selected on mouse up instead of mouse down, 
															// cannot do on mouse down because it might mean user wants to drag item onto previously selected item (e.g. texture onto material)
															// Use this to draw item with selected style on mouse down without really selecting it 

	class DropData
	{
		public int[]    expandedArrayBeforeDrag;
		public int      lastControlID;
		public int      dropOnControlID;
		public int      dropPreviousControlID;
		public double   expandItemBeginTimer;
	}
	
	DropData m_DropData = null;
	bool m_StillWantsNameEditing = false;
	
	internal class Styles
	{
		public GUIStyle foldout = "IN Foldout";
		public GUIStyle insertion = "PR Insertion";
		public GUIStyle label = "PR Label";
		public GUIStyle hiLabel = "HI Label";
		public GUIStyle ping = "PR Ping";
	}
	// Text colors for hierarchy window
	static Color []s_HierarchyColors = {
		Color.black,						// Game Object
		new Color (0,.15f, .51f, 1),		// Prefab
		new Color (.25f, .05f, .05f,1),		// Broken Prefab
		Color.black,						// ????
		Color.white,						
		new Color (.67f, .76f, 1), 
		new Color (1, .71f, .71f, 1), 
		Color.white
	};
	static Color []s_DarkColors = {
		new Color (.705f,.705f,.705f,1),	// Game Object
		new Color (.3f,.5f, .85f, 1),		// Prefab
		new Color (.7f, .4f, .4f,1),		// Broken Prefab
		new Color (.705f,.705f,.705f,1),	// ????
		Color.white,						
		new Color (.67f, .76f, 1), 
		new Color (1, .71f, .71f, 1), 
		Color.white
	};
	
	Rect m_NameEditRect = new Rect (0,0,1,1);
	string m_NameEditString = "";
	int m_EditNameInstanceID = 0;
	
	enum NameEditMode
	{
		None, 
		// We found the selected asset, but are not renaming it
		Found, 
		// We are renaming an existing asset
		Renaming,
		// We are faking an asset exists but haven't imported it yet, as we're doing the initial rename
		PreImportNaming
	}
	NameEditMode m_EditNameMode { 
		get { return m_RealEditNameMode; } 
		set {
			if (value == m_RealEditNameMode) return;
			m_RealEditNameMode = value;
		}
	}
	NameEditMode m_RealEditNameMode = NameEditMode.None;

	private EndNameEditAction m_EndNameEditAction;

	// The folder a yet-to-be-imported asset is in.
	string m_NewAssetFolder;
	// The original name of a yet-to-be-imported asset is in.
	string m_NewAssetName;
	// The icon to use for the yet-to-be-imported asset 
	Texture2D m_NewAssetIcon;
	// The file extension of the asset
	string m_NewAssetExtension;
	// How far is it indented
	int m_NewAssetIndent = 0;
	// What is the instanceID of the element following this asset. This is used as we iterate over the 
	// project window details
	int m_NewAssetSortInstanceID = 0;	
	string m_NewAssetResourceFile;

	bool m_DidSelectSearchResult = false;
	bool m_NeedsRelayout = false;
	int m_FrameAfterRelayout = 0;
	FilteredHierarchy m_FilteredHierarchy;
    private static Styles ms_Styles;    

	public IHierarchyProperty GetNewHierarchyProperty()
	{
		if (m_FilteredHierarchy == null)
		{
			m_FilteredHierarchy = new FilteredHierarchy (m_HierarchyType);
			m_FilteredHierarchy.searchFilter = CreateFilter (m_SearchFilter, m_SearchMode);
 		}
		return FilteredHierarchyProperty.CreateHierarchyPropertyForFilter(m_FilteredHierarchy);
	}
	

	void SetExpanded (int instanceID, bool expand)
	{
		Hashtable expandedHash = new Hashtable();
		for (int i=0;i<m_ExpandedArray.Length;i++)
			expandedHash.Add(m_ExpandedArray[i], null);

		if (expand != expandedHash.Contains(instanceID))
		{
			if (expand)
				expandedHash.Add(instanceID, null);
			else
				expandedHash.Remove(instanceID);
			
			m_ExpandedArray = new int[expandedHash.Count];
			int index = 0;
			foreach (int key in expandedHash.Keys)
			{
				m_ExpandedArray[index] = key;
				index++;
			}
		}
		
		if (m_HierarchyType == HierarchyType.Assets)
			InternalEditorUtility.expandedProjectWindowItems = m_ExpandedArray;
	}

	public void SetExpandedRecurse (int instanceID, bool expand)
	{
        IHierarchyProperty search = new HierarchyProperty(m_HierarchyType);
		if (search.Find(instanceID, m_ExpandedArray))
		{
			SetExpanded(instanceID, expand);
			
			int depth = search.depth;
			while (search.Next(null) && search.depth > depth)
			{
				SetExpanded(search.instanceID, expand);
			}
		}
	}
	void OnFocus ()
	{
		m_FocusTime = Time.realtimeSinceStartup;
	}
	void OnLostFocus ()
	{
		m_StillWantsNameEditing = false;
		EndNameEditing ();	
	}
		
	public override void OnEnable ()
	{
		base.OnEnable ();
		EditorApplication.playmodeStateChanged += OnPlayModeStateChanged;
	}

	public override void OnDisable ()
	{
		base.OnDisable ();
		EditorApplication.playmodeStateChanged -= OnPlayModeStateChanged;
	}
		
	IHierarchyProperty GetFirstSelected ()
	{
		int firstRow = 1000000000;
		IHierarchyProperty found = null;
		
		foreach (int id in Selection.instanceIDs)
		{
			IHierarchyProperty search = GetNewHierarchyProperty();
			if (search.Find(id, m_ExpandedArray) && search.row < firstRow)
			{
				firstRow = search.row;
				found = search;
			}
		}
		
		return found;		
	}

	
	/// TODO: Only repaint if we either had an asset selected, or we are about to get one.
	void OnProjectChange ()
	{
		if (m_HierarchyType == HierarchyType.Assets)
		{
			CancelNameEditing ();
			if (m_FilteredHierarchy != null)
				m_FilteredHierarchy.ResultsChanged ();
			Repaint();
		}
	}
	
	void OnSelectionChange ()
	{
		if (Selection.activeInstanceID != m_EditNameInstanceID)
			EndNameEditing ();
		m_StillWantsNameEditing = false;
		
		RevealObjects( Selection.instanceIDs );

		if (m_ProjectViewInternalSelectionChange)
		{
			m_ProjectViewInternalSelectionChange = false;
		}
		else
		if (m_HierarchyType == HierarchyType.GameObjects) 
		{
			FrameObject (Selection.activeInstanceID);
		}

		Repaint();
	}
	
	void OnHierarchyChange ()
	{
		if (m_HierarchyType == HierarchyType.GameObjects)
		{
			EndNameEditing ();
			if (m_FilteredHierarchy != null)
				m_FilteredHierarchy.ResultsChanged ();
			Repaint();	
		}
	}

	IHierarchyProperty GetLastSelected ()
	{
		int firstRow = -1;
		IHierarchyProperty found = null;
		
		foreach (int id in Selection.instanceIDs)
		{
			IHierarchyProperty search = GetNewHierarchyProperty();
			if (search.Find(id, m_ExpandedArray) && search.row > firstRow)
			{
				firstRow = search.row;
				found = search;
			}
		}
		
		return found;		
	}

	IHierarchyProperty GetActiveSelected ()
	{
		return GetFirstSelected ();
	}

	IHierarchyProperty GetLast ()
	{
		IHierarchyProperty search = GetNewHierarchyProperty();
		int offset = search.CountRemaining(m_ExpandedArray);
		if (offset == 0)
			return null;
		search.Reset();
		if (search.Skip(offset, m_ExpandedArray))
			return search;
		else
			return null;
	}

	IHierarchyProperty GetFirst ()
	{
		IHierarchyProperty search = GetNewHierarchyProperty();
		if (search.Next(m_ExpandedArray))
			return search;
		else
			return null;
	}

	void OpenAssetSelection ()
	{
		foreach (int id in Selection.instanceIDs)
		{
			if (AssetDatabase.Contains(id))
				AssetDatabase.OpenAsset (id);	
		}
	}

	internal override void SetSearchFilter (string searchFilter, SearchMode searchMode, bool setAll)
	{
		int firstSelected = 0;
		
		if (m_DidSelectSearchResult && GetFirstSelected () != null)		
			firstSelected = GetFirstSelected ().instanceID;

		base.SetSearchFilter(searchFilter, searchMode, setAll);

		if (m_FilteredHierarchy == null)
			GetNewHierarchyProperty ();

		m_FilteredHierarchy.searchFilter = CreateFilter (searchFilter, searchMode); 

		if (m_DidSelectSearchResult)
		{	
			if (searchFilter == "")
			{
				m_DidSelectSearchResult = false;
				m_NeedsRelayout = true;
			}
			
			if (firstSelected != 0)
			{
				// The allowClearSearchFilter is false to prevent SetSearchFilter to be called recursivly. Fix for case 405711
				FrameObject (firstSelected, false);
			}
		}
	}
	
	internal override void ClickedSearchField()
	{
		EndNameEditing();
	}

	void ProjectWindowTitle ()
	{
		// Title bar
		GUILayout.BeginHorizontal("Toolbar");
		
		if (m_HierarchyType == HierarchyType.Assets)
		{
			CreateAssetPopup ();
			GUILayout.Space (6);
		}
		else
		{
			CreateGameObjectPopup ();
			GUILayout.Space (6);
		}
		
		if (m_FilteredHierarchy == null)
			GetNewHierarchyProperty ();
		
		int id = GUIUtility.GetControlID (FocusType.Keyboard);
		if (Event.current.GetTypeForControl (id) == EventType.KeyDown)		
		{
			//prevent search filter edit field from eating arrow keys.
			switch (Event.current.keyCode)
			{
				case KeyCode.LeftArrow:
				case KeyCode.RightArrow:
					if(!hasSearchFilter)
					{
						EditorGUILayout.EndHorizontal ();
						return;
					}
					break;
					
				case KeyCode.UpArrow:
				case KeyCode.DownArrow:
				case KeyCode.Return:
				case KeyCode.KeypadEnter:
					EditorGUILayout.EndHorizontal ();
					return;
			}
		}

		GUILayout.FlexibleSpace ();

		SearchFieldGUI();
		
		GUILayout.EndHorizontal();
	}

	void SearchResultPathGUI ()
	{
		if (!hasSearchFilter)
			return;
			
		EditorGUILayout.BeginVertical (EditorStyles.inspectorBig);
		GUILayout.Label ("Path:");
		
		IHierarchyProperty activeSelected = GetActiveSelected ();
		if (activeSelected != null)
		{
			IHierarchyProperty activeSelectedInHierachy = new HierarchyProperty (m_HierarchyType);
			activeSelectedInHierachy.Find(activeSelected.instanceID, null);
			do
			{
				EditorGUILayout.BeginHorizontal ();
				GUILayout.Label (activeSelectedInHierachy.icon);
				GUILayout.Label (activeSelectedInHierachy.name);
				GUILayout.FlexibleSpace ();
				EditorGUILayout.EndHorizontal ();
			}
			while (activeSelectedInHierachy.Parent ());
		}		
		EditorGUILayout.EndVertical ();		
		GUILayout.Space (0);
	}
	
	void OnGUI ()
	{
		if (ms_Styles == null)
			ms_Styles = new Styles();

		ProjectWindowTitle ();
		
		Rect screenRect = GUILayoutUtility.GetRect (0, Screen.width, 0, Screen.height);

		if (m_HierarchyType == HierarchyType.Assets && AssetDatabase.isLocked)
		{
			Debug.LogError("Repainting while performing asset operations!");
			GUILayout.Label("Performing Asset operations");
			return;
		}

		m_ScreenRect = screenRect;
				
		EditorGUIUtility.SetIconSize (new Vector2 (16, 16));
		HierarchyView ();
		SearchResultPathGUI ();

		EditorGUIUtility.SetIconSize (Vector2.zero);
		
		if (Event.current.type == EventType.Repaint)
		{
			if (m_NeedsRelayout)
			{
				m_NeedsRelayout = false;
				if(m_FrameAfterRelayout != 0)
				{
					FrameObject(m_FrameAfterRelayout);
					m_FrameAfterRelayout = 0; 
				}
			}
		}
	}
	
	/// En the current name editing entry. This will apply the changes before it stops.
	void EndNameEditing ()
	{
		switch (m_EditNameMode)
		{
		case NameEditMode.Renaming:
			m_EditNameMode = NameEditMode.None;
			ObjectNames.SetNameSmartWithInstanceID(m_EditNameInstanceID, m_NameEditString);
			return;
		case NameEditMode.PreImportNaming:
			m_EditNameMode = NameEditMode.None;
			// If we have an empty string, revert to the original
			if (m_NameEditString == "")
			m_NameEditString = m_NewAssetName;
			ProjectWindowUtil.EndNameEditAction (m_EndNameEditAction, m_EditNameInstanceID, m_NewAssetFolder + "/" + m_NameEditString + m_NewAssetExtension, m_NewAssetResourceFile);
			m_EndNameEditAction = null;
			return;
		}
	}
	
	/// Cancels the currently active editing (if any).
	/// Changes are lost.
	void CancelNameEditing ()
	{
		// TODO: Check if we need to clean up: If m_EditNameInstanceID is valid we should delete it as we did not create a serialized asset from it!
		m_EditNameMode = NameEditMode.None;
		m_EndNameEditAction = null;
	}
	
	/// Begin editing the selected item.
	void BeginNameEditing (int instanceID)
	{
		if (NamingAsset ())
			EndNameEditing ();

		if (hasSearchFilter)
		{
			m_EditNameMode = NameEditMode.None;
			return;
		}
		IHierarchyProperty property = new HierarchyProperty (m_HierarchyType);
		property.Find(instanceID, null);
		if (property.isValid)
		{
			m_EditNameMode = NameEditMode.Renaming;
			m_EditNameInstanceID = instanceID;
			EditorGUI.s_RecycledEditor.content.text = property.name;
			EditorGUI.s_RecycledEditor.SelectAll ();
		}
		else
		{
			m_EditNameMode = NameEditMode.None;
		}
	}
	
	void OnPlayModeStateChanged ()
	{
		EndNameEditing ();	
	}

	internal void BeginPreimportedNameEditing(int instanceID, EndNameEditAction endAction, string pathName, Texture2D icon, string resourceFile)
	{
		if (NamingAsset ())
			EndNameEditing ();

		m_EndNameEditAction = endAction;

		if (!pathName.StartsWith ("assets/", System.StringComparison.CurrentCultureIgnoreCase))
		{
			pathName = AssetDatabase.GetUniquePathNameAtSelectedPath (pathName);
		} else {
			pathName = AssetDatabase.GenerateUniqueAssetPath (pathName);
		}
		

		m_NewAssetFolder = Path.GetDirectoryName (pathName);
		m_NewAssetIcon = icon;
		m_NewAssetName = Path.GetFileNameWithoutExtension (pathName);
		m_NewAssetExtension = Path.GetExtension (pathName);
		m_NewAssetIndent = 0;
		m_NewAssetSortInstanceID = 0;
		m_NewAssetResourceFile = resourceFile;
		
		Selection.activeObject = EditorUtility.InstanceIDToObject (instanceID);
			
		// Clear any search filter we might have
		if (hasSearchFilter)
		{
			m_SearchFilter = "";
			m_SearchMode = SearchMode.All;
		}
		
		// First, make sure that all parents are expanded. 
		int parentID = AssetDatabase.LoadAssetAtPath (m_NewAssetFolder, typeof (Object)).GetInstanceID ();  
		RevealObject (parentID);
		SetExpanded (parentID, true);

		// Go through the hierarchy, figure out where to put the asset name.
		// We do this by going through the hierarchy and counting lines (AWESOME!)
		IHierarchyProperty property = GetNewHierarchyProperty ();
		property.Reset ();
		
		float yPos = 0;
		if (!m_NewAssetFolder.Equals ("assets", System.StringComparison.CurrentCultureIgnoreCase))
		{	
			while (property.Next(m_ExpandedArray))
			{
				string propertyPath = AssetDatabase.GetAssetPath (property.instanceID);
				yPos += m_RowHeight;
				// Find the right path
				if (string.Equals (propertyPath, m_NewAssetFolder, System.StringComparison.CurrentCultureIgnoreCase))
				{
					yPos = FindPositionInsideDir (property, yPos);
					break;
				}
			}
		} else {
			yPos = FindPositionInsideDir (property, yPos);
		}

		float indent = kBaseIndent + kIndent * m_NewAssetIndent + IconWidth;
		m_NameEditRect = new Rect (indent, yPos, 100, m_RowHeight);
		
		// Scroll the view so the edit field is visible
		float scrollTop = yPos;
		float scrollBottom = scrollTop - position.height + 20 + m_RowHeight;
		m_ScrollPosition.y = Mathf.Clamp(m_ScrollPosition.y, scrollBottom, scrollTop);

		m_EditNameMode = NameEditMode.PreImportNaming;
		m_EditNameInstanceID = instanceID;
		EditorGUI.s_RecycledEditor.content.text = m_NameEditString = m_NewAssetName;
		EditorGUI.s_RecycledEditor.SelectAll ();
	}
	
	float FindPositionInsideDir (IHierarchyProperty property, float yPos)
	{
		m_NewAssetIndent = property.depth + 1;
		string dirPath = m_NewAssetFolder + "/";
		while (property.Next (m_ExpandedArray))
		{
			string propertyPath = AssetDatabase.GetAssetPath (property.instanceID);
			// Are we still inside the same subdir?
			string propDir = propertyPath + "/";
			if (propDir.Length >= dirPath.Length && propDir.Substring (0, dirPath.Length).Equals (dirPath, System.StringComparison.CurrentCultureIgnoreCase))
			{
				// if we're inside a subfolder, skip this element
				if (m_NewAssetFolder.Equals (Path.GetDirectoryName (propertyPath), System.StringComparison.CurrentCultureIgnoreCase))
				{
					if (EditorUtility.SemiNumericCompare (Path.GetFileNameWithoutExtension (propertyPath), m_NewAssetName) > 0)
					{
						m_NewAssetIndent = property.depth;
						m_NewAssetSortInstanceID = property.instanceID;
						break;
					}
				}
			} else {
				// We moved outside the directory, so we want to insert here instead
				m_NewAssetSortInstanceID = property.instanceID;
				break;
			}
			yPos += m_RowHeight;
		}
		return yPos;
	}
		

	bool NamingAsset ()
	{
		return m_EditNameMode == NameEditMode.Renaming || m_EditNameMode == NameEditMode.PreImportNaming;
	}
		
	void EditName ()
	{
		if (!NamingAsset ()) 
			return;
		Event evt = Event.current;
		if (evt.type == EventType.KeyDown)
		{
			if (evt.keyCode == KeyCode.Escape)
			{
				evt.Use ();
				CancelNameEditing ();
			}
			if (evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter)
			{
				evt.Use ();
				EndNameEditing ();
				GUIUtility.ExitGUI();
			}
		}
		GUI.changed = false;
		GUIUtility.GetControlID (67897456, FocusType.Passive);
		GUI.SetNextControlName ("ProjectWindowRenameField");
		EditorGUI.FocusTextInControl ("ProjectWindowRenameField");
		GUIStyle t = EditorStyles.textField;
		EditorStyles.s_Current.m_TextField = "PR TextField";
		m_NameEditRect.xMax = GUIClip.visibleRect.width;

		Rect r= m_NameEditRect;
		m_NameEditString = EditorGUI.TextField (r, m_NameEditString);
		EditorStyles.s_Current.m_TextField = t;

		if (evt.type == EventType.ScrollWheel)
			evt.Use ();
	}
	
	private GUIStyle labelStyle {
		get { return m_HierarchyType == HierarchyType.Assets ? ms_Styles.label : ms_Styles.hiLabel; }
	}
	
	void HierarchyView ()
	{
		if (Event.current.type == EventType.MouseDown || Event.current.type == EventType.KeyDown) 
			m_StillWantsNameEditing = false;

		bool focused = m_Parent.hasFocus;

		Hashtable selection = new Hashtable();
		foreach (int id in Selection.instanceIDs)
		{
			selection.Add(id, null);
		}
		
		IHierarchyProperty property = GetNewHierarchyProperty();
		float offset = 0;
		
		int elements = 0;
		elements = property.CountRemaining(m_ExpandedArray);
		property.Reset();
		
		Rect contentRect = new Rect (0, 0, 1, elements * m_RowHeight);
		
		if (m_EditNameMode == NameEditMode.PreImportNaming)
			contentRect.height += m_RowHeight;
		
		int invisibleRows;
		if (hasSearchFilter)
		{
			m_ScrollPositionFiltered = GUI.BeginScrollView (m_ScreenRect, m_ScrollPositionFiltered, contentRect);
			invisibleRows = Mathf.RoundToInt (m_ScrollPositionFiltered.y) / Mathf.RoundToInt (m_RowHeight);
		}
		else
		{
			m_ScrollPosition = GUI.BeginScrollView (m_ScreenRect, m_ScrollPosition, contentRect);
			invisibleRows = Mathf.RoundToInt (m_ScrollPosition.y) / Mathf.RoundToInt (m_RowHeight);
		}
		
		// Show the name editing field (if we have one)
		EditName ();
		
		if (Event.current.type == EventType.ExecuteCommand || Event.current.type == EventType.ValidateCommand)	
		{
			ExecuteCommandGUI ();
			
			// Early out only validation, copy & paste might still be happening later
			if (Event.current.type == EventType.ValidateCommand)
			{
				GUI.EndScrollView ();
				return;
			}
		}
		KeyboardGUI();

		// Early out layout events. Edit name using focus control so we actually need that.
		if (Event.current.type == EventType.Layout)
		{
			GUI.EndScrollView ();			
			return;
		}


		int contentControlID = ControlIDForProperty(null);
		bool foundNewAsset = false;
		
		offset = invisibleRows * m_RowHeight;
		float endOffset = m_ScreenRect.height + offset + 16;
		property.Skip(invisibleRows, m_ExpandedArray);
		bool drawDropHere = false;
		Rect dropHereRect = new Rect (0,0,0,0);
		Vector2 mousePos = Event.current.mousePosition;
		GUIContent content = new GUIContent ();
		Event evt = Event.current;

		int activeID = Selection.activeInstanceID;

		if (!NamingAsset())
			m_EditNameMode = NameEditMode.None;
		
		GUIStyle s = labelStyle;
		Color[] colors = EditorGUIUtility.isProSkin ? s_DarkColors : s_HierarchyColors;
		while (property.Next(m_ExpandedArray) && offset <= endOffset)
		{
			Rect selectionRect = new Rect(0, offset, GUIClip.visibleRect.width, m_RowHeight);
			if (m_EditNameMode == NameEditMode.PreImportNaming && !foundNewAsset && property.instanceID == m_NewAssetSortInstanceID)
			{
				foundNewAsset = true;
				offset += m_RowHeight;
				selectionRect = new Rect(0, offset, GUIClip.visibleRect.width, m_RowHeight);
				DrawPreImportedIcon (offset);
			}
			int instanceID = property.instanceID;

			int rowSelectionControlID = ControlIDForProperty(property);
			float indent = (kBaseIndent + kIndent * property.depth);
			
			if ((evt.type == EventType.MouseUp || evt.type == EventType.KeyDown) && activeID == instanceID && Selection.instanceIDs.Length == 1)
			{
				m_NameEditString = property.name;

				m_NameEditRect = new Rect (selectionRect.x + indent + IconWidth, selectionRect.y, selectionRect.width - indent, selectionRect.height); 
				m_EditNameInstanceID = instanceID;
				if (m_EditNameMode == NameEditMode.None && property.isMainRepresentation)
					m_EditNameMode = NameEditMode.Found;
			}
			
			if (m_EditNameMode == NameEditMode.Renaming && m_EditNameInstanceID == instanceID) 
			{
				m_NameEditRect = new Rect (selectionRect.x + indent + IconWidth, selectionRect.y, selectionRect.width - indent, selectionRect.height);				
			}

			if (evt.type == EventType.ContextClick && selectionRect.Contains (evt.mousePosition))
			{
				m_NameEditRect = new Rect (selectionRect.x + indent + IconWidth, selectionRect.y, selectionRect.width - indent, selectionRect.height);
				m_EditNameInstanceID = instanceID;
				m_NameEditString = property.name;
				if (m_EditNameMode == NameEditMode.None && property.isMainRepresentation)
					m_EditNameMode = NameEditMode.Found;
			}
			
			// Draw icon & name
			if (Event.current.type == EventType.Repaint)
			{
				if (m_HierarchyType == HierarchyType.GameObjects)
				{
					int i = property.colorCode;
					Color textColor = colors[i & 3];
					Color onTextColor = colors[(i & 3) + 4];
					if (i >= 4)
						textColor.a = onTextColor.a = .6f;
					else
						textColor.a = onTextColor.a = 1; 
					s.normal.textColor = textColor;
					s.focused.textColor = textColor;
					s.hover.textColor = textColor;
					s.active.textColor = textColor;
					s.onNormal.textColor = onTextColor;
					s.onHover.textColor = onTextColor;
					s.onActive.textColor = onTextColor;
					s.onFocused.textColor = onTextColor;
				}
				
				bool dragHover1 = (m_DropData != null && ( m_DropData.dropPreviousControlID == rowSelectionControlID));
				bool dragHover2 = (m_DropData != null && ( m_DropData.dropOnControlID == rowSelectionControlID));
				content.text = property.name;
				content.image = property.icon;
				s.padding.left = (int)indent;

				bool showSelected = m_CurrentDragSelectionIDs.Contains(instanceID);
				bool selected = (m_CurrentDragSelectionIDs.Count == 0 && selection.Contains(instanceID)) ||
					showSelected ||
					(showSelected && (evt.control || evt.shift) && selection.Contains(instanceID) ||
					(showSelected && selection.Contains(instanceID) && selection.Contains(m_CurrentDragSelectionIDs))
					); //   || GUIUtility.hotControl == rowSelectionControlID 
				if (m_EditNameMode == NameEditMode.Renaming && instanceID == m_EditNameInstanceID)
				{
					content.text = "";
					selected = false;
				}

				s.Draw (selectionRect, content, 
						dragHover2, dragHover2, 
						selected,
						focused);
				
				if (dragHover1)
				{
					drawDropHere = true;
					dropHereRect = new Rect (selectionRect.x + indent, selectionRect.y - m_RowHeight, selectionRect.width - indent, selectionRect.height);
				}
			}

			var drawRect = selectionRect;
			drawRect.x += indent;
			drawRect.width -= indent;

			// OnGUI hooks for each list element
			if (m_HierarchyType == HierarchyType.Assets)
			{
				ProjectHooks.OnProjectWindowItem (property.guid, drawRect);

				if (EditorApplication.projectWindowItemOnGUI != null)
					EditorApplication.projectWindowItemOnGUI (property.guid, drawRect);
			}

			if (m_HierarchyType == HierarchyType.GameObjects && EditorApplication.hierarchyWindowItemOnGUI != null)
				EditorApplication.hierarchyWindowItemOnGUI (property.instanceID, drawRect);


			// Draw foldout
			if (property.hasChildren && !hasSearchFilter)
			{
				bool expanded = property.IsExpanded(m_ExpandedArray);
				
				GUI.changed = false;
				Rect foldoutRect = new Rect (kBaseIndent + kIndent * property.depth - kFoldoutSize, offset, kFoldoutSize, m_RowHeight);

				expanded = GUI.Toggle(foldoutRect, expanded, GUIContent.none, ms_Styles.foldout);
				
				// When clicking toggle, add instance id to expanded list
				if (GUI.changed)
				{
					EndNameEditing();

					// Recursive set expanded
					if (Event.current.alt)
					{
						SetExpandedRecurse(instanceID, expanded);	
					}
					// Expand one element only
					else
					{
						SetExpanded(instanceID, expanded);	
					}
				}
			}

			// Handle mouse down based selection change
			if (evt.type == EventType.MouseDown && Event.current.button == 0 && selectionRect.Contains (Event.current.mousePosition))
			{
				// Open Asset
				if (Event.current.clickCount == 2)
				{
					AssetDatabase.OpenAsset (instanceID);
					if (m_HierarchyType != HierarchyType.Assets && SceneView.lastActiveSceneView != null)
						SceneView.lastActiveSceneView.FrameSelected();
					GUIUtility.ExitGUI();
				}
				// Prepare for mouse up selection change or drag&drop
				else
				{
					EndNameEditing ();
                    m_CurrentDragSelectionIDs = GetSelection(property, true);
					GUIUtility.hotControl = rowSelectionControlID;
					GUIUtility.keyboardControl = 0;
					DragAndDropDelay delay = (DragAndDropDelay)GUIUtility.GetStateObject (typeof(DragAndDropDelay), rowSelectionControlID);
					delay.mouseDownPosition = Event.current.mousePosition;
				}
				evt.Use ();
			}
			// On Mouse drag, start drag & drop
			else if (evt.type == EventType.MouseDrag && GUIUtility.hotControl == rowSelectionControlID)
			{
				DragAndDropDelay delay = (DragAndDropDelay)GUIUtility.GetStateObject (typeof(DragAndDropDelay), rowSelectionControlID);
				if (delay.CanStartDrag ())
				{
					StartDrag (property);
					GUIUtility.hotControl = 0;
				}
				
				evt.Use ();
			}
			// On Mouse up change selection
			else if (evt.type == EventType.MouseUp && GUIUtility.hotControl == rowSelectionControlID)
			{
				if (selectionRect.Contains(evt.mousePosition))
				{
					if (property.isMainRepresentation && Selection.activeInstanceID == property.instanceID && Time.realtimeSinceStartup - m_FocusTime > .5f && !EditorGUIUtility.HasHolddownKeyModifiers(evt)) 
					{
						m_StillWantsNameEditing = true;
						EditorApplication.CallDelayed (BeginMouseEditing, .5f);
					}
					else
					{			
						SelectionClick (property);
					}
					GUIUtility.hotControl = 0;
				}
                m_CurrentDragSelectionIDs.Clear();
				evt.Use ();
			}
			// Handle context menu click
			else if (evt.type == EventType.ContextClick && selectionRect.Contains(evt.mousePosition))
			{
				evt.Use ();
				if (m_HierarchyType == HierarchyType.GameObjects)
				{
					SelectionClickContextMenu(property);
					var menu = new GenericMenu ();

					menu.AddItem (EditorGUIUtility.TextContent ("HierarchyPopupCopy"), false, CopyGO);
					menu.AddItem (EditorGUIUtility.TextContent ("HierarchyPopupPaste"), false, PasteGO);

					menu.AddSeparator ("");
					if (m_EditNameMode == NameEditMode.Found && !hasSearchFilter)
						menu.AddItem (EditorGUIUtility.TextContent ("HierarchyPopupRename"), false, RenameGO, property.pptrValue);
					else
						menu.AddDisabledItem (EditorGUIUtility.TextContent ("HierarchyPopupRename"));
					menu.AddItem (EditorGUIUtility.TextContent ("HierarchyPopupDuplicate"), false, DuplicateGO);
					menu.AddItem (EditorGUIUtility.TextContent ("HierarchyPopupDelete"), false, DeleteGO);

					menu.AddSeparator ("");
					var prefab = PrefabUtility.GetPrefabParent (property.pptrValue);
					if (prefab != null)
					{
						menu.AddItem(EditorGUIUtility.TextContent("HierarchyPopupSelectPrefab"), false, () =>
						{
							Selection.activeObject = prefab; 
							EditorGUIUtility.PingObject(prefab.GetInstanceID());
						} );
					}
					else
						menu.AddDisabledItem (EditorGUIUtility.TextContent ("HierarchyPopupSelectPrefab"));

					menu.ShowAsContext ();
				}
			}
			else if ((evt.type == EventType.DragUpdated || evt.type == EventType.DragPerform))
			{
				Rect dropRect = selectionRect;
				dropRect.yMin -= m_DropNextToAreaHeight * 2;
				
				if (dropRect.Contains (mousePos))
				{
					if (mousePos.y - selectionRect.y < m_DropNextToAreaHeight * 0.5f) // Drag next to previous element into parent
						DragElement(property, false);
					else
						DragElement(property, true);
					
					GUIUtility.hotControl = 0;
				}
			}

			offset += m_RowHeight;

		} // End of: while (property.Next(m_ExpandedArray) && offset <= endOffset)
		
		
		if (drawDropHere)
		{
			GUIStyle ins = ms_Styles.insertion;
			if (evt.type == EventType.repaint)
				ins.Draw (dropHereRect, false, false, false, false);
		}
		
		if (m_EditNameMode == NameEditMode.PreImportNaming && m_NewAssetSortInstanceID == 0)
			DrawPreImportedIcon (offset + 16);

		HandlePing ();

		GUI.EndScrollView ();			
		
		switch (evt.type)
		{
		case EventType.DragUpdated:
            if (m_SearchFilter == "")
			    DragElement(null, true);
			else
			{
				if (m_DropData == null)
					m_DropData = new DropData();
				m_DropData.dropOnControlID = 0;
				m_DropData.dropPreviousControlID = 0;
			}
            break;
        case EventType.DragPerform:
            //if (m_SearchFilter == "")
			//{
	            m_CurrentDragSelectionIDs.Clear();
	            DragElement(null, true);
			//} else 
			break;
		// Drag exit cleanup
		case EventType.DragExited:
            m_CurrentDragSelectionIDs.Clear();
			DragCleanup(true);
			break;
		case EventType.ContextClick:
			if (m_HierarchyType == HierarchyType.Assets && m_ScreenRect.Contains (evt.mousePosition))
			{
				EditorUtility.DisplayPopupMenu(new Rect (evt.mousePosition.x, evt.mousePosition.y, 0, 0), "Assets/", null);
				evt.Use();
			}
			break;
		case EventType.MouseDown:
		
			if (evt.button == 0 && m_ScreenRect.Contains (evt.mousePosition))
			{
				GUIUtility.hotControl = contentControlID;
				Selection.activeObject = null;			
				EndNameEditing ();
				evt.Use ();
			}
			break;
		case EventType.MouseUp:
			// Handle mouse up on empty area, select nothing
			if (GUIUtility.hotControl == contentControlID)
			{
				GUIUtility.hotControl = 0;
				evt.Use ();
			}
			break;
		case EventType.KeyDown:
			if (m_EditNameMode == NameEditMode.Found &&
				(((evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter) && Application.platform == RuntimePlatform.OSXEditor)
					|| (evt.keyCode == KeyCode.F2 && Application.platform == RuntimePlatform.WindowsEditor)))
			{
				
				BeginNameEditing (Selection.activeInstanceID);
				evt.Use ();
			} 
			break;
		}
	}

	void HandlePing()
	{
		m_Ping.HandlePing();

		if (m_Ping.isPinging)
			Repaint();
	}

	void DrawPreImportedIcon (float offset)
	{
		if (Event.current.type == EventType.Repaint && m_NewAssetIcon)
		{
			ms_Styles.label.padding.left = 0;
			ms_Styles.label.Draw (new Rect (m_NewAssetIndent * 16 + kBaseIndent, offset - 16, 16,16), EditorGUIUtility.TempContent (m_NewAssetIcon), 0);
		}
	}

	void CopyGO ()
	{
		Unsupported.CopyGameObjectsToPasteboard();
	}
	
	void PasteGO ()
	{
		Unsupported.PasteGameObjectsFromPasteboard();
	}
	
	void DuplicateGO () 
	{
		Unsupported.DuplicateGameObjectsUsingPasteboard();
	}

	void RenameGO (object obj)
	{
		GameObject go = (GameObject)obj;
		Selection.activeObject = go;
		BeginNameEditing (Selection.activeInstanceID);
		Repaint ();
	}
	
	void DeleteGO ()
	{
		Unsupported.DeleteGameObjectSelection ();	
	}
		
	void BeginMouseEditing ()
	{	
		if (m_StillWantsNameEditing) 
			BeginNameEditing (Selection.activeInstanceID);
		Repaint ();
	}
	
	void StartDrag (IHierarchyProperty property)
	{
		DragAndDrop.PrepareStartDrag();
		DragAndDrop.objectReferences = GetDragAndDropObjects(property.pptrValue);
		DragAndDrop.paths = GetDragAndDropPaths(property.instanceID);
		if (DragAndDrop.objectReferences.Length > 1)
			DragAndDrop.StartDrag("<Multiple>");
		else
			DragAndDrop.StartDrag(ObjectNames.GetDragAndDropTitle(property.pptrValue));
	}

	void DragCleanup (bool revertExpanded)
	{
		if (m_DropData != null)
		{	
			if (m_DropData.expandedArrayBeforeDrag != null && revertExpanded)
			{
				m_ExpandedArray = m_DropData.expandedArrayBeforeDrag;
			}
			m_DropData = null;
			Repaint();
		}
	}
	
	void DragElement (IHierarchyProperty property, bool dropOnTopOfElement)
	{
		if (Event.current.type == EventType.DragPerform)
		{
			IHierarchyProperty targetProperty = property;

			DragAndDropVisualMode mode = DragAndDropVisualMode.None;

			// Try Drop on top of element
			if (dropOnTopOfElement)
			{
				mode = DoDrag (targetProperty, true);
#if ENABLE_EDITOR_HIERARCHY_ORDERING
				DragTransformHierarchy(targetProperty, true);
#endif
			}
			
			// Fall back to dropping next to element (Actually calls drag operation on top of parent)
			if (mode == DragAndDropVisualMode.None && property != null )
			{
#if ENABLE_EDITOR_HIERARCHY_ORDERING
				IHierarchyProperty oldTargetProperty = targetProperty;
#endif
				targetProperty = GetParentProperty(targetProperty);
				mode = DoDrag (targetProperty, true);
#if ENABLE_EDITOR_HIERARCHY_ORDERING
				DragTransformHierarchy(oldTargetProperty, false);
#endif
			}

			// Finalize drop
			if (mode != DragAndDropVisualMode.None)
			{
				DragAndDrop.AcceptDrag();
				DragCleanup(false);

				//Select dragged objects, but only those which have actually been moved.
				//If we add, for example, a script to a GO, do *not* select the script, as this is very 
				//annoying when editing a GO.
				ArrayList newSelection = new ArrayList();			
				foreach(Object obj in DragAndDrop.objectReferences)
				{
					IHierarchyProperty draggedProperty = GetNewHierarchyProperty();
					if (draggedProperty.Find (obj.GetInstanceID(), null))
					{
						IHierarchyProperty parentProperty;
							parentProperty = GetParentProperty(draggedProperty);
						
						if (parentProperty != null && (targetProperty == null  || parentProperty.pptrValue == targetProperty.pptrValue))
							newSelection.Add(obj);
					}
				}

				if(newSelection.Count > 0)
				{
					Selection.objects = (Object[])newSelection.ToArray(typeof(Object));
					RevealObjects( Selection.instanceIDs);
					FrameObject (Selection.activeInstanceID);
				}
				GUIUtility.ExitGUI();				
			}
			else
			{
				DragCleanup(true);	
			}
		}
		else
		{
			if (m_DropData == null)
				m_DropData = new DropData();
			m_DropData.dropOnControlID = 0;
			m_DropData.dropPreviousControlID = 0;
			

			// Handle auto expansion
			int controlID = ControlIDForProperty(property);
			if (controlID != m_DropData.lastControlID)
			{
				m_DropData.lastControlID = ControlIDForProperty(property);
				m_DropData.expandItemBeginTimer = Time.realtimeSinceStartup;
			}
			
			bool mayExpand = Time.realtimeSinceStartup - m_DropData.expandItemBeginTimer > kDropExpandTimeout;

			// Auto open folders we are about to drag into
			if (property != null && property.hasChildren && mayExpand && !property.IsExpanded(m_ExpandedArray))
			{
				// Store the expanded array prior to drag so we can revert it with a delay later
				if (m_DropData.expandedArrayBeforeDrag == null)
					m_DropData.expandedArrayBeforeDrag = m_ExpandedArray;
				
				SetExpanded(property.instanceID, true);
			}

			DragAndDropVisualMode mode = DragAndDropVisualMode.None;

			// Try drop on top of element
			if (dropOnTopOfElement)
				mode = DoDrag (property, false);
			
			if (mode != DragAndDropVisualMode.None)
			{
				m_DropData.dropOnControlID = controlID;
				DragAndDrop.visualMode = mode;
			}
			// Fall back to dropping next to element
			else if (property != null && m_SearchFilter == "")
			{
				IHierarchyProperty parentProperty;
					parentProperty = GetParentProperty(property);

				mode = DoDrag (parentProperty, false);
				
				if (mode != DragAndDropVisualMode.None)
				{
					m_DropData.dropPreviousControlID = controlID;
					m_DropData.dropOnControlID = ControlIDForProperty(parentProperty);
					DragAndDrop.visualMode = mode;
				}
			}

			Repaint();
		}
		
		Event.current.Use();
	}
	
	DragAndDropVisualMode DoDrag (IHierarchyProperty property, bool perform)
	{
		//cannot use interface directly here. need to get a real HierarchyProperty class, which can be casted 
		//in C++.
		HierarchyProperty search = new HierarchyProperty(m_HierarchyType);
		if (property == null || !search.Find(property.instanceID, null))
			search = null;

		// Easier to handle sprite drop here than in c++
		if (HandleSpriteDragIntoHierarchy (property, perform))
			return DragAndDropVisualMode.Link;
		
		if (m_HierarchyType == HierarchyType.Assets) 				
			return InternalEditorUtility.ProjectWindowDrag(search, perform);
		else
			return InternalEditorUtility.HierarchyWindowDrag(search, perform);
	}
	
#if ENABLE_EDITOR_HIERARCHY_ORDERING
	bool DragTransformHierarchy (IHierarchyProperty property, bool dropUpon)
	{
		if (property == null)
			return false;

		if (m_HierarchyType == HierarchyType.GameObjects) 	
			return InternalEditorUtility.DragTransformHierarchy(property as HierarchyProperty, dropUpon);

		return false;
	}
#endif

	private bool HandleSpriteDragIntoHierarchy(IHierarchyProperty property, bool perform)
	{
		Sprite[] sprites = SpriteUtility.GetSpritesFromDraggedObjects ();
		if (sprites.Length == 1)
			return SpriteUtility.HandleSingleSpriteDragIntoHierarchy (property, sprites[0], perform);
		if (sprites.Length > 1)
			return SpriteUtility.HandleMultipleSpritesDragIntoHierarchy (property, sprites, perform);

		return false;
	}
	
	IHierarchyProperty GetPreviousParentProperty(IHierarchyProperty property)
	{
		IHierarchyProperty previousProperty = GetNewHierarchyProperty();
		if (previousProperty.Find(property.instanceID, m_ExpandedArray) && previousProperty.Previous(m_ExpandedArray))
		{
			return GetParentProperty(previousProperty);
		}
		return null;
	}

	IHierarchyProperty GetParentProperty (IHierarchyProperty property)
	{
		IHierarchyProperty parentProperty = GetNewHierarchyProperty();
		if (parentProperty.Find(property.instanceID, m_ExpandedArray) && parentProperty.Parent())
		{
			int parentInstanceID = parentProperty.instanceID;
			parentProperty.Reset();
			if (parentProperty.Find(parentInstanceID, m_ExpandedArray))
				return parentProperty;
		}
		return null;
	}
	

	int ControlIDForProperty (IHierarchyProperty property)
	{
		///@TODO> WHY NOT BY DEFAULT RESERVE 0-1000000 for manually generated ids
		if (property != null)
			return property.instanceID + 10000000;	
		else
			return -1;
	}

	string[] GetDragAndDropPaths (int dragged)
	{
		string dragPath = AssetDatabase.GetAssetPath (dragged);
				
		if (m_HierarchyType == HierarchyType.Assets)
		{
			ArrayList all = new ArrayList();
			all.AddRange(GetMainSelectedPaths ());
			
			if (all.Contains(dragPath))
				return all.ToArray(typeof(string)) as string[];
			else
			{
				string[] paths = { dragPath };
				return paths;
			}
		}
		else
		{
			return new string[0];
		}
	}

	Object[] GetDragAndDropObjects (Object dragged)
	{
		ArrayList all = new ArrayList();
		all.AddRange(GetSelectedReferences ());
		
		if (all.Contains(dragged))
			return all.ToArray(typeof(Object)) as Object[];
		else
		{
			Object[] objs = { dragged };
			return objs;
		}
	}

	Object[] GetSelectedReferences ()
	{
		ArrayList selection = new ArrayList();
		foreach (Object obj in Selection.objects)
		{
			if (AssetDatabase.Contains(obj) != (m_HierarchyType == HierarchyType.GameObjects))
				selection.Add(obj);
		}
		
		return selection.ToArray(typeof(Object)) as Object[];
	}

	static string[] GetMainSelectedPaths ()
	{
		ArrayList paths = new ArrayList();
		foreach (int instanceID in Selection.instanceIDs)
		{
			if (AssetDatabase.IsMainAsset(instanceID))
			{
				string path = AssetDatabase.GetAssetPath (instanceID);
				paths.Add(path);
			}
		}
		
		return paths.ToArray(typeof(string)) as string[];
	}

	void ExecuteCommandGUIHierarchy ()
	{
		bool execute = Event.current.type == EventType.ExecuteCommand;

		if (Event.current.commandName == "Delete" || Event.current.commandName == "SoftDelete")
		{
			if (execute)
				Unsupported.DeleteGameObjectSelection();
			Event.current.Use();
			GUIUtility.ExitGUI();
		}
		else if (Event.current.commandName == "Duplicate")
		{
			if (execute)
				Unsupported.DuplicateGameObjectsUsingPasteboard();
			Event.current.Use();
			GUIUtility.ExitGUI();
		}
		else if (Event.current.commandName == "Copy")
		{
			if (execute)
				Unsupported.CopyGameObjectsToPasteboard();
			Event.current.Use();
			GUIUtility.ExitGUI();
		}
		else if (Event.current.commandName == "Paste")
		{
			if (execute)
				Unsupported.PasteGameObjectsFromPasteboard();
			Event.current.Use();
			GUIUtility.ExitGUI();
		}
        else if (Event.current.commandName == "SelectAll")
        {
            if (execute)
            	SelectAll();
			Event.current.Use();
			GUIUtility.ExitGUI();
        }
 	}

	static internal void DuplicateSelectedAssets ()
	{
		AssetDatabase.Refresh();

		// If we are duplicating an animation clip which comes from a imported model. Instead of duplicating the fbx file, we duplicate the animation clip.
		// Thus the user can edit and add for example animation events.
		Object[] selectedAnimations = Selection.objects;
		bool duplicateClip = true;
		foreach (Object asset in selectedAnimations)
		{
			AnimationClip clip = asset as AnimationClip;
			if (clip == null || (clip.hideFlags & HideFlags.NotEditable) == 0 || !AssetDatabase.Contains(clip))
				duplicateClip = false;
		}

		ArrayList copiedPaths = new ArrayList();
		bool failed = false;
		
		if (duplicateClip)
		{
			foreach (Object asset in selectedAnimations)
			{
				AnimationClip sourceClip = asset as AnimationClip;
				if (sourceClip != null && (sourceClip.hideFlags & HideFlags.NotEditable) != 0)
				{
					string path = AssetDatabase.GetAssetPath(asset);
					path = Path.Combine(Path.GetDirectoryName(path), sourceClip.name) + ".anim";
					string newPath = AssetDatabase.GenerateUniqueAssetPath (path);

					AnimationClip newClip = new AnimationClip();
					EditorUtility.CopySerialized(sourceClip, newClip);
					AssetDatabase.CreateAsset(newClip, newPath);
					copiedPaths.Add(newPath);
				}
			}
		}
		else
		{
			Object[] selectedAssets = Selection.GetFiltered(typeof(Object), SelectionMode.Assets);
			
			foreach (Object asset in selectedAssets)
			{
				string assetPath = AssetDatabase.GetAssetPath(asset);
				string newPath = AssetDatabase.GenerateUniqueAssetPath (assetPath);
				
				// Copy 
				if (newPath.Length != 0)
					failed |= !AssetDatabase.CopyAsset (assetPath, newPath);
				else
					failed |= true;
		
				if (!failed)
				{
					copiedPaths.Add(newPath);
				}
			}
		}
		
//				if (failed)
//					UnityBeep ();
	
		AssetDatabase.Refresh();
		
		Object[] copiedAssets = new Object[copiedPaths.Count];
		for (int i=0;i<copiedPaths.Count;i++)
		{
			copiedAssets[i] = AssetDatabase.LoadMainAssetAtPath(copiedPaths[i] as string);
		}
					
		Selection.objects = copiedAssets;	
	
	}

	void ExecuteCommandGUIProject ()
	{
		bool execute = Event.current.type == EventType.ExecuteCommand;

		if (Event.current.commandName == "Delete" || Event.current.commandName == "SoftDelete")
		{
			Event.current.Use();
			if (execute)
			{
				bool askIfSure = Event.current.commandName == "SoftDelete";
				DeleteSelectedAssets(askIfSure);
			}
			GUIUtility.ExitGUI();
		}
		else if (Event.current.commandName == "Duplicate")
		{
			if (execute)
			{
				Event.current.Use();
				DuplicateSelectedAssets ();
				GUIUtility.ExitGUI();
			}
			else
			{
				Object[] selectedAssets = Selection.GetFiltered(typeof(Object), SelectionMode.Assets);
				if (selectedAssets.Length != 0)
					Event.current.Use();
			}
		}
		else if (Event.current.commandName == "FocusProjectWindow")
		{
			if (Event.current.type == EventType.ExecuteCommand)
			{
				FrameObject(Selection.activeInstanceID);
				Event.current.Use();
				Focus();
				GUIUtility.ExitGUI();
			}
			else
			{
				Event.current.Use();
			}
		}
        else if (Event.current.commandName == "SelectAll")        
        {
            if (Event.current.type == EventType.ExecuteCommand)
			{
				
				SelectAll();				
			}    
			Event.current.Use(); 
        }

	}

	static internal void DeleteSelectedAssets(bool askIfSure)
	{
		string[] paths = GetMainSelectedPaths();

		if (paths.Length == 0) return;
		
		if (askIfSure)
		{
			string title = "Delete selected asset";
			if (paths.Length > 1) title = title + "s";
			title = title + "?";
			if (!EditorUtility.DisplayDialog(title, "You cannot undo this action.", "Delete", "Cancel"))
				return;
		}

		AssetDatabase.StartAssetEditing();
		foreach (string path in paths)
		{
			AssetDatabase.MoveAssetToTrash(path);
		}
		AssetDatabase.StopAssetEditing();
	}

    private void SelectAll()
    {
		IHierarchyProperty property = GetNewHierarchyProperty();

        List<int> allSelected = new List<int>();
        while (property.Next(m_ExpandedArray))
		{
			allSelected.Add(property.instanceID);
		}

        Selection.instanceIDs = allSelected.ToArray();
    }

	internal void PingTargetObject (int targetInstanceID)
	{
		if (targetInstanceID == 0)
			return;

		if (ms_Styles == null)
			ms_Styles = new Styles();

		// Frame and setup ping
		if (FrameObject(targetInstanceID))
		{
			IHierarchyProperty temp = GetNewHierarchyProperty();
			int elements = 0;
			elements = temp.CountRemaining(m_ExpandedArray);

			IHierarchyProperty property = GetNewHierarchyProperty();
			if (property.Find(targetInstanceID, m_ExpandedArray))
			{
				int row = property.row;
				float scrollTop = m_RowHeight * row;

				m_Ping.m_TimeStart = Time.realtimeSinceStartup;
				m_Ping.m_PingStyle = ms_Styles.ping;

				float scrollBarOffset = elements * m_RowHeight > m_ScreenRect.height ? -16f : 0f;
				m_Ping.m_AvailableWidth = position.width + scrollBarOffset;
				
				GUIContent pingContent = new GUIContent(property.name, property.icon);
				Vector2 contentSize = ms_Styles.ping.CalcSize(pingContent);
				contentSize.y += 3; // proper size adjustment (makes text centered on ping background)
				m_Ping.m_ContentRect = new Rect(kBaseIndent + kIndent * property.depth, scrollTop, contentSize.x, contentSize.y);
				m_Ping.m_ContentDraw = (Rect r) =>
				{
					// get parameters from closure
					DrawPingContent(r, pingContent);
				};	

				Repaint();
				return;
			}
		}
		
		// Try with game object
		targetInstanceID = InternalEditorUtility.GetGameObjectInstanceIDFromComponent(targetInstanceID);
		if (targetInstanceID != 0)
			PingTargetObject(targetInstanceID);
	}

	void DrawPingContent(Rect rect, GUIContent content)
	{
		GUIStyle s = labelStyle;
		s.padding.left = 0;
		s.Draw(rect, content, false, false, false, false);
	}

	void ExecuteCommandGUI ()
	{
		if (m_HierarchyType == HierarchyType.Assets)
			ExecuteCommandGUIProject ();
		else
			ExecuteCommandGUIHierarchy ();
		
		// Frame select assets
		if (Event.current.commandName == "FrameSelected")
		{
			if (Event.current.type == EventType.ExecuteCommand)
				FrameObject(Selection.activeInstanceID);
			Event.current.Use();
			GUIUtility.ExitGUI();
		}
        else if (Event.current.commandName == "Find")
        {
			if (Event.current.type == EventType.ExecuteCommand)
				FocusSearchField();	
			Event.current.Use();
        }
	}

	public void SelectPrevious () 
	{
		IHierarchyProperty firstSelected = GetFirstSelected ();
		// Select previous
		if (firstSelected != null)
		{
			if (firstSelected.Previous(m_ExpandedArray))
			{
				FrameObject(firstSelected.instanceID);
				SelectionClick(firstSelected);
			}
		}
		// No previous selection -> Select last
		else if (GetLast () != null)
		{
			Selection.activeInstanceID = GetLast ().instanceID;
			FrameObject(Selection.activeInstanceID);
		}
	}
	
	public void SelectNext ()
	{
		IHierarchyProperty lastSelected = GetLastSelected ();
		// Select next
		if (lastSelected != null)
		{
			if (lastSelected.Next(m_ExpandedArray))
			{
				SelectionClick(lastSelected);
				FrameObject(lastSelected.instanceID);
			}
		}
		// No previous selection -> Select first
		else if (GetFirst () != null)
		{
			Selection.activeInstanceID = GetFirst ().instanceID;
			FrameObject(Selection.activeInstanceID);
		}	
	}
	
	void KeyboardGUI ()
	{
		int id = GUIUtility.GetControlID (FocusType.Keyboard);
		if (Event.current.GetTypeForControl(id) != EventType.KeyDown)
			return;
		
		switch (Event.current.keyCode)
		{
			// Fold in
			case KeyCode.LeftArrow:
			{
				int[] selectionIDs = Selection.instanceIDs;
				for (int i=0;i<selectionIDs.Length;i++)
					SetExpanded(selectionIDs[i], false);
			
				Event.current.Use();
				break;
			}
			// Fold out
			case KeyCode.RightArrow:
			{
				int[] selectionIDs = Selection.instanceIDs;
				for (int i=0;i<selectionIDs.Length;i++)
					SetExpanded(selectionIDs[i], true);
				Event.current.Use();
				break;
			}
			case KeyCode.UpArrow:
			{
				Event.current.Use();

				SelectPrevious();
								
				break;
			}
			// Select next or first
			case KeyCode.DownArrow:
			{
				Event.current.Use();

				// cmd-down -> open selection
				if (Application.platform == RuntimePlatform.OSXEditor && Event.current.command)
				{
					OpenAssetSelection ();
					GUIUtility.ExitGUI();
				}
				else 
					SelectNext();
					
				break;
			}
            case KeyCode.Home:
            if (GetFirst() != null)
            {
                Selection.activeObject = GetFirst().pptrValue;
                FrameObject(Selection.activeInstanceID);
            }
            break;
            case KeyCode.End:
            if (GetLast() != null)
            {
                Selection.activeObject = GetLast().pptrValue;
                FrameObject(Selection.activeInstanceID);
            }
            break;
            case KeyCode.PageUp:
            Event.current.Use();

			if (Application.platform == RuntimePlatform.OSXEditor)
            {
                m_ScrollPosition.y -= m_ScreenRect.height;

                if (m_ScrollPosition.y < 0)
                    m_ScrollPosition.y = 0;
            }
            else
            {
                IHierarchyProperty firstSelected = GetFirstSelected();

                if (firstSelected != null)
                {
                    for (int i = 0; i < m_ScreenRect.height / m_RowHeight; i++)
                    {
                        if (!firstSelected.Previous(m_ExpandedArray))
                        {
                            firstSelected = GetFirst();
                            break;
                        }
                    }

                    int newSelectedObject = firstSelected.instanceID;
                    SelectionClick(firstSelected);
                    FrameObject(newSelectedObject);
                }
                // No previous selection -> Select last
                else if (GetFirst() != null)
                {
                    Selection.activeObject = GetFirst().pptrValue;
                    FrameObject(Selection.activeInstanceID);
                }
            }
            break;
            case KeyCode.PageDown:
            Event.current.Use();

			if (Application.platform == RuntimePlatform.OSXEditor)
            {
                m_ScrollPosition.y += m_ScreenRect.height;
            }
            else
            {
                IHierarchyProperty lastSelected = GetLastSelected();

                if (lastSelected != null)
                {
                    for (int i = 0; i < m_ScreenRect.height / m_RowHeight; i++)
                    {
                        if (!lastSelected.Next(m_ExpandedArray))
                        {
                            lastSelected = GetLast();
                            break;
                        }
                    }

                    int newSelectedObject = lastSelected.instanceID;
                    SelectionClick(lastSelected);
                    FrameObject(newSelectedObject);
                }
                // No next selection -> Select last
                else if (GetLast() != null)
                {
                    Selection.activeObject = GetLast().pptrValue;
                    FrameObject(Selection.activeInstanceID);
                }
            }
            break;
			// Select next or first
			case KeyCode.KeypadEnter:
			case KeyCode.Return:
				if (Application.platform == RuntimePlatform.WindowsEditor) {
					OpenAssetSelection ();
					GUIUtility.ExitGUI();					
				}
				break;
		}
	}

	bool RevealObject (int targetInstanceID)
	{
		return RevealObject (targetInstanceID, true);
	}

	bool RevealObject (int targetInstanceID, bool allowClearSearchFilter) 
	{
		IHierarchyProperty property = GetNewHierarchyProperty();
		if (property.Find (targetInstanceID, null))
		{
			while (property.Parent())
			{
				SetExpanded(property.instanceID, true);
			}
			return true;
		}
		
		if (allowClearSearchFilter)
		{
			// If the object is in the main hierachy, disable search Filter.
			property = new HierarchyProperty(m_HierarchyType);
			if (property.Find (targetInstanceID, null))
			{
				ClearSearchFilter();
				return RevealObject(targetInstanceID, true);
			}
		}
		
		return false;
	}
	
	void RevealObjects (int[] targetInstanceIDs ) 
	{
		IHierarchyProperty property = GetNewHierarchyProperty();
		int[] ancestors = property.FindAllAncestors( targetInstanceIDs );
		for (int i=0;i<ancestors.Length;i++)
			SetExpanded(ancestors[i], true);
	}
	
	internal bool FrameObject (int targetInstanceID)
	{
		return FrameObject (targetInstanceID, true);
	}

	internal bool FrameObject (int targetInstanceID, bool allowClearSearchFilter)
	{
		IHierarchyProperty property = GetNewHierarchyProperty();

		if (m_NeedsRelayout)
		{
			m_FrameAfterRelayout = targetInstanceID;
			return property.Find(targetInstanceID, m_ExpandedArray);
		}

		RevealObject (targetInstanceID, allowClearSearchFilter);

		property = GetNewHierarchyProperty();
		if (property.Find(targetInstanceID, m_ExpandedArray))
		{
			int row = property.row;
			
			float scrollTop = m_RowHeight * row;
			float scrollBottom = scrollTop - m_ScreenRect.height + m_RowHeight;
			if (!hasSearchFilter)
				m_ScrollPosition.y = Mathf.Clamp(m_ScrollPosition.y, scrollBottom, scrollTop);
			else
				m_ScrollPositionFiltered.y = Mathf.Clamp(m_ScrollPositionFiltered.y, scrollBottom, scrollTop);
			
			Repaint();
			return true;
		}
		else
		{
			return false;
		}
	}

	void SelectionClickContextMenu (IHierarchyProperty property)
	{
        if (!Selection.Contains(property.instanceID))
            Selection.activeInstanceID = property.instanceID;
	}

	List<int> GetSelection(IHierarchyProperty property, bool mouseDown)
	{
		List<int> newSelection = new List<int>();

		// Toggle selected property from selection
		if (EditorGUI.actionKey)
		{
			newSelection.AddRange(Selection.instanceIDs);

			if (newSelection.Contains(property.instanceID))
				newSelection.Remove(property.instanceID);
			else
				newSelection.Add(property.instanceID);

			return newSelection;
		}
		// Select everything between the first selected object and the selected
		else if (Event.current.shift)
		{
			IHierarchyProperty firstSelectedProperty = GetFirstSelected();
			IHierarchyProperty lastSelectedProperty = GetLastSelected();

			if (firstSelectedProperty == null || !firstSelectedProperty.isValid)
			{
				newSelection.Add(property.instanceID);
				return newSelection;
			}

			IHierarchyProperty from, to;
			if (property.row > lastSelectedProperty.row)
			{
				from = firstSelectedProperty;
				to = property;
			}
			else
			{
				from = property;
				to = lastSelectedProperty;
			}

			newSelection.Add(from.instanceID);
			while (from.Next(m_ExpandedArray))
			{
				newSelection.Add(from.instanceID);

				if (from.instanceID == to.instanceID)
					break;
			}

			return newSelection;
		}
		// Just set the selection to the clicked object
		else
		{
			if (mouseDown)
			{
				// Don't change selection on mouse down when clicking on selected item. 
				// This is for dragging in case with multiple items selected (mouse down should not unselect the rest).
				newSelection.AddRange(Selection.instanceIDs);
				if (newSelection.Contains(property.instanceID))
				{
					return newSelection;
				}
				newSelection.Clear();
			}

			newSelection.Add(property.instanceID);
			return newSelection;
		}
	}

	void SelectionClick (IHierarchyProperty property)
	{
		List<int> newSelection = GetSelection(property, false);

		if (newSelection.Count == 1)
			Selection.activeInstanceID = newSelection[0];
		else
			Selection.instanceIDs = newSelection.ToArray();

		m_ProjectViewInternalSelectionChange = true;

		if (hasSearchFilter)
		{
			m_DidSelectSearchResult = true;
			m_NeedsRelayout = true;
		}
	}
	
	void CreateAssetPopup ()
	{
		GUIContent content = new GUIContent ("Create");
		
		Rect rect = GUILayoutUtility.GetRect(content, EditorStyles.toolbarDropDown, null);
		if (Event.current.type == EventType.Repaint)
			EditorStyles.toolbarDropDown.Draw (rect, content, false, false, false, false);
		
		if (Event.current.type == EventType.MouseDown && rect.Contains (Event.current.mousePosition))
		{
			GUIUtility.hotControl = 0;
			EditorUtility.DisplayPopupMenu(rect, "Assets/Create", null);
			Event.current.Use ();
		}
	}
	
	void CreateGameObjectPopup ()
	{
		GUIContent content = new GUIContent ("Create");
		
		Rect rect = GUILayoutUtility.GetRect(content, EditorStyles.toolbarDropDown, null);
		if (Event.current.type == EventType.Repaint)
			EditorStyles.toolbarDropDown.Draw (rect, content, false, false, false, false);
		
		if (Event.current.type == EventType.MouseDown && rect.Contains (Event.current.mousePosition))
		{
			GUIUtility.hotControl = 0;
			
			EditorUtility.DisplayPopupMenu(rect, "GameObject/Create Other", null);
			Event.current.Use ();
		}
	}

	void Awake ()
	{
		if (m_HierarchyType == HierarchyType.Assets)
			m_ExpandedArray = InternalEditorUtility.expandedProjectWindowItems;
	}
	
	public new void Show ()
	{
		// Awake is called before the m_IsProjectWindow assignment, thus expanded array is not setup -> Call Manually
		Awake ();	
		
		base.Show();
	}
}

}
