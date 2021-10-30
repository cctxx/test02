using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;

namespace UnityEditor
{

internal class RectSelection
{
	Vector2 m_SelectStartPoint;
	Vector2 m_SelectMousePoint;
	Object[] m_SelectionStart;
	bool m_RectSelecting;
	Dictionary<GameObject, bool> m_LastSelection;
	enum SelectionType { Normal, Additive, Subtractive }
	Object[] m_CurrentSelection = null;
	EditorWindow m_Window;
	
	static int s_RectSelectionID = GUIUtility.GetPermanentControlID();

	public RectSelection (EditorWindow window)
	{
		m_Window = window;
	}
	
	public void OnGUI ()
	{
		Event evt = Event.current; 

		Handles.BeginGUI ();

		Vector2 mousePos = evt.mousePosition;
		int id = s_RectSelectionID;

		switch (evt.GetTypeForControl (id)) {
		
		case EventType.layout:
			if (!Tools.viewToolActive)
				HandleUtility.AddDefaultControl (id);
			break;
			
		case EventType.mouseDown:
			if (HandleUtility.nearestControl == id && evt.button == 0)
			{
#if ENABLE_SPRITES
				if (SceneView.lastActiveSceneView.m_OneClickDragObject == null && !evt.shift && !EditorGUI.actionKey)
					TryOneClickDrag ();
#endif
				GUIUtility.hotControl = id;
				m_SelectStartPoint = mousePos;
				m_SelectionStart = Selection.objects;
				m_RectSelecting = false;
			}
			break;
		case EventType.mouseDrag:
			if (GUIUtility.hotControl == id)
			{
				if (!m_RectSelecting && (mousePos - m_SelectStartPoint).magnitude > 6f)
				{
					EditorApplication.modifierKeysChanged += SendCommandsOnModifierKeys;
					m_RectSelecting = true;
					m_LastSelection = null;
					m_CurrentSelection = null;
				}
				if (m_RectSelecting)
				{
					m_SelectMousePoint = new Vector2(Mathf.Max(mousePos.x, 0), Mathf.Max(mousePos.y, 0));
					GameObject[] rectObjs = HandleUtility.PickRectObjects(EditorGUIExt.FromToRect(m_SelectStartPoint, m_SelectMousePoint));
					m_CurrentSelection = rectObjs;
					bool setIt = false;		
					if (m_LastSelection == null)
					{
						m_LastSelection = new Dictionary<GameObject, bool> ();	
						setIt = true;
					}
					setIt |= m_LastSelection.Count != rectObjs.Length;
					if (!setIt)
					{
						Dictionary<GameObject, bool> set = new Dictionary<GameObject, bool> (rectObjs.Length);
						foreach (GameObject g in rectObjs)
							set.Add (g, false);
						foreach (GameObject g in m_LastSelection.Keys) {
							if (!set.ContainsKey (g)) {
								setIt = true;
								break;
							}
						}
					}
					if (setIt)
					{
						m_LastSelection = new Dictionary<GameObject, bool> (rectObjs.Length);
						foreach (GameObject g in rectObjs)
							m_LastSelection.Add (g, false);
						if (rectObjs != null)
						{
							if (evt.shift)
								UpdateSelection(rectObjs, SelectionType.Additive);
							else if (EditorGUI.actionKey)
								UpdateSelection(rectObjs, SelectionType.Subtractive);
							else
								UpdateSelection(rectObjs, SelectionType.Normal);
						}
					}
				}
				evt.Use();
			}
			break;
			
		case EventType.repaint:
			if (GUIUtility.hotControl == id && m_RectSelecting)
				EditorStyles.selectionRect.Draw(EditorGUIExt.FromToRect(m_SelectStartPoint, m_SelectMousePoint), GUIContent.none, false, false, false, false);
			break;
			
		case EventType.mouseUp:
			if (GUIUtility.hotControl == id && evt.button == 0)
			{
				GUIUtility.hotControl = 0;
				if (m_RectSelecting)
				{
					EditorApplication.modifierKeysChanged -= SendCommandsOnModifierKeys;                
					m_RectSelecting = false;
					m_SelectionStart = new Object[0];
					evt.Use();
				}
				else
				{
					Object picked = HandleUtility.PickGameObject(Event.current.mousePosition, true);
					if (evt.shift)
					{
						if (Selection.activeGameObject == picked)
							UpdateSelection(picked, SelectionType.Subtractive);
						else
							UpdateSelection(picked, SelectionType.Additive);
					}
					else if (EditorGUI.actionKey && picked != null)
					{
						if (Selection.Contains(picked))
							UpdateSelection(picked, SelectionType.Subtractive);
						else
							UpdateSelection(picked, SelectionType.Additive);
					}
					else
					{
						UpdateSelection(picked, SelectionType.Normal);
					}
					evt.Use();
				}
			}
			break;
		case EventType.ExecuteCommand:
			if (id == GUIUtility.hotControl && evt.commandName == "ModifierKeysChanged")
			{
				if (evt.shift)
					UpdateSelection(m_CurrentSelection, SelectionType.Additive);
				else if (EditorGUI.actionKey) 
					UpdateSelection(m_CurrentSelection, SelectionType.Subtractive);
				else
					UpdateSelection(m_CurrentSelection, SelectionType.Normal);
				evt.Use ();
			}
			break;
		}

		Handles.EndGUI ();
	}
	
	private void UpdateSelection(Object obj, SelectionType type)
	{
		Object[] objs;
		if (obj == null)
		{
			objs = new Object[0];
		}
		else
		{
			objs = new Object[1];
			objs[0] = obj;	
		}
		UpdateSelection(objs, type);	
	}
	private void UpdateSelection(Object[] objs, SelectionType type)
	{
		Object[] existingSelection = m_SelectionStart;
		Object[] newSelection;
		switch (type)
		{
			case SelectionType.Additive:
				if (objs.Length > 0)
				{
					newSelection = new Object[existingSelection.Length + objs.Length];
					System.Array.Copy (existingSelection, newSelection, existingSelection.Length);
					for (int i = 0; i < objs.Length; i++)
						newSelection [existingSelection.Length + i] = objs[i];
					if (!m_RectSelecting)
						Selection.activeObject = objs[0];
					else 
						Selection.activeObject = newSelection[0];

					Selection.objects = newSelection;
				}
				else
				{
					Selection.objects = existingSelection;	
				}
				break;
			case SelectionType.Subtractive:
				Dictionary<Object, bool> set = new Dictionary<Object, bool> (existingSelection.Length);
				foreach (GameObject g in existingSelection)
					set.Add (g, false);
				foreach (GameObject g in objs)
				{
					 if (set.ContainsKey (g))
						set.Remove(g);
				}
				newSelection = new Object[set.Keys.Count];
				set.Keys.CopyTo (newSelection, 0);
				Selection.objects = newSelection;
				break;
			case SelectionType.Normal:
			default:
				Selection.objects = objs;
				break;
		}
	}

#if ENABLE_SPRITES
	// If we click on SpriteRenderer, we want tell its editor to trigger "late" mousedown for one-click dragging.
	private void TryOneClickDrag()
	{
		switch (Event.current.type)
		{
			case EventType.MouseDown:
				Object picked = HandleUtility.PickGameObject (Event.current.mousePosition, true);

				if (picked)
				{
					GameObject pickedGameObject = picked as GameObject;

					bool alreadySelected = false;
					foreach (GameObject go in Selection.gameObjects)
					{
						if (pickedGameObject == go)
						{
							alreadySelected = true;
							break;
						}

					}

					if (!alreadySelected)
					{
						if (pickedGameObject.GetComponent<SpriteRenderer> () != null)
						{
							UpdateSelection (picked, SelectionType.Normal);
							SceneView.lastActiveSceneView.m_OneClickDragObject = picked;
							Event.current.Use ();
						}
					}
				}
				break;
		}
		
	}
#endif

	internal void SendCommandsOnModifierKeys () 
    {
        m_Window.SendEvent (EditorGUIUtility.CommandEvent("ModifierKeysChanged"));
    }

}

} // namespace
