using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{

[CustomEditor (typeof (TagManager))]
internal class TagManagerInspector : Editor
{
	protected SerializedProperty m_Tags;
	protected SerializedProperty m_SortingLayers;
	protected SerializedProperty[] m_Layers;
	protected bool m_LayersExpanded = true;
	ReorderableList m_SortLayersList;

	public TagManager tagManager
	{
		get { return target as TagManager; }
	}

	public virtual void OnEnable ()
	{
		m_Tags = serializedObject.FindProperty ("tags");
		m_SortingLayers = serializedObject.FindProperty ("m_SortingLayers");
		m_Layers = new SerializedProperty[32];
		for (int i = 0; i < 32; ++i)
		{
			string layerName = ((i >= 8) ? "User Layer " : "Builtin Layer ") + i;
			m_Layers[i] = serializedObject.FindProperty (layerName);
		}

		if (m_SortLayersList == null)
		{
			m_SortLayersList = new ReorderableList (serializedObject, m_SortingLayers, true, false, true, true);
			m_SortLayersList.onReorderCallback = ReorderSortLayerList;
			m_SortLayersList.onAddCallback = AddToSortLayerList;
			m_SortLayersList.onRemoveCallback = RemoveFromSortLayerList;
			m_SortLayersList.onCanRemoveCallback = CanRemoveSortLayerEntry;
			m_SortLayersList.drawElementCallback = DrawSortLayerListElement;
			m_SortLayersList.elementHeight = EditorGUIUtility.singleLineHeight + 2;
			m_SortLayersList.headerHeight = 3;
		}

		m_LayersExpanded = false;
		m_SortingLayers.isExpanded = false;
		m_Tags.isExpanded = false;

		switch (tagManager.m_DefaultExpandedFoldout)
		{
			case "Tags":
				m_Tags.isExpanded = true;
				break;
			case "SortingLayers":
				m_SortingLayers.isExpanded = true;
				break;
			case "Layers":
				m_LayersExpanded = true;
				break;
			default:
				m_LayersExpanded = true;
				break;
		}
		
	}

	void AddToSortLayerList (ReorderableList list)
	{
		serializedObject.ApplyModifiedProperties ();
		InternalEditorUtility.AddSortingLayer ();
		serializedObject.Update ();
		list.index = list.serializedProperty.arraySize - 1; // select just added one
	}

	public void ReorderSortLayerList (ReorderableList list)
	{
		InternalEditorUtility.UpdateSortingLayersOrder ();
	}
	private void RemoveFromSortLayerList (ReorderableList list)
	{
		ReorderableList.s_Defaults.DoRemoveButton (list);
		serializedObject.ApplyModifiedProperties ();
		serializedObject.Update ();
		InternalEditorUtility.UpdateSortingLayersOrder ();
	}
	private bool CanEditSortLayerEntry (int index)
	{
		if (index < 0 || index >= InternalEditorUtility.GetSortingLayerCount())
			return false;
		return !InternalEditorUtility.IsSortingLayerDefault(index);
	}
	private bool CanRemoveSortLayerEntry(ReorderableList list)
	{
		return CanEditSortLayerEntry(list.index);
	}


	private void DrawSortLayerListElement (Rect rect, int index, bool selected, bool focused)
	{
		rect.height -= 2; // nicer looking with selected list row and a text field in it

		// De-indent by the drag handle width, so the text field lines up with others in the inspector.
		// Will have space in front of label for more space between it and the drag handle.
		rect.xMin -= ReorderableList.Defaults.dragHandleWidth;

		bool oldEnabled = GUI.enabled;
		GUI.enabled = CanEditSortLayerEntry(index);

		string oldName = InternalEditorUtility.GetSortingLayerName (index);
		int userID = InternalEditorUtility.GetSortingLayerUserID (index);
		string newName = EditorGUI.TextField (rect, " Layer " + userID, oldName);
		if (newName != oldName)
		{
			serializedObject.ApplyModifiedProperties ();
			InternalEditorUtility.SetSortingLayerName (index, newName);
			serializedObject.Update ();
		}

		GUI.enabled = oldEnabled;
	}

	// Want something better than "TagManager"
	internal override string targetTitle
	{
		get { return "Tags & Layers"; }
	}

	public override void OnInspectorGUI ()
	{
		serializedObject.Update ();

		// Tags
		EditorGUILayout.PropertyField (m_Tags, true);

		// Sorting layers
		m_SortingLayers.isExpanded = EditorGUILayout.Foldout (m_SortingLayers.isExpanded, "Sorting Layers");
		if (m_SortingLayers.isExpanded)
		{
			EditorGUI.indentLevel++;
			m_SortLayersList.DoList ();
			EditorGUI.indentLevel--;
		}

		// Layers
		m_LayersExpanded = EditorGUILayout.Foldout (m_LayersExpanded, "Layers", EditorStyles.foldout);
		if (m_LayersExpanded)
		{
			EditorGUI.indentLevel++;
			for (var i = 0; i < m_Layers.Length; ++i)
				EditorGUILayout.PropertyField (m_Layers[i]);
			EditorGUI.indentLevel--;
		}

		serializedObject.ApplyModifiedProperties ();
	}
}
}
