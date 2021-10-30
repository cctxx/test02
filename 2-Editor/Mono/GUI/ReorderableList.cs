using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System;
using System.Collections;
using System.Collections.Generic;

using Object = UnityEngine.Object;


namespace UnityEditor
{

internal class ReorderableList
{
	public delegate void HeaderCallbackDelegate (Rect rect);
	public delegate void ElementCallbackDelegate (Rect rect, int index, bool isActive, bool isFocused);
	
	public delegate void ReorderCallbackDelegate (ReorderableList list);
	public delegate void SelectCallbackDelegate (ReorderableList list);
	public delegate void AddCallbackDelegate (ReorderableList list);
	public delegate void AddDropdownCallbackDelegate (Rect buttonRect, ReorderableList list);
	public delegate void RemoveCallbackDelegate (ReorderableList list);
	public delegate bool CanRemoveCallbackDelegate (ReorderableList list);
	
	// draw callbacks
	public HeaderCallbackDelegate drawHeaderCallback;
	public ElementCallbackDelegate drawElementCallback;
		
	// interaction callbacks
	public ReorderCallbackDelegate onReorderCallback;
	public SelectCallbackDelegate onSelectCallback;
	public AddCallbackDelegate onAddCallback;
	public AddDropdownCallbackDelegate onAddDropdownCallback;
	public RemoveCallbackDelegate onRemoveCallback;
	public CanRemoveCallbackDelegate onCanRemoveCallback;
			
	private int m_ActiveElement = -1;
	private float m_DragOffset = 0;
	private GUISlideGroup m_SlideGroup;
		
	private SerializedObject m_SerializedObject;
	private SerializedProperty m_Elements;
	private IList m_ElementList;
	private bool m_Draggable;
	private float m_DraggedY;
	private bool m_Dragging;
	private List<int> m_NonDragTargetIndices;
		
	private bool m_DisplayHeader;
	public bool displayAdd;
	public bool displayRemove;
		
	private int id = -1;

	// class for default rendering and behavior of reorderable list - stores styles and is statically available as s_Defaults
	public class Defaults
	{
		public GUIContent iconToolbarPlus =	EditorGUIUtility.IconContent ("Toolbar Plus", "Add to list");
		public GUIContent iconToolbarPlusMore =	EditorGUIUtility.IconContent ("Toolbar Plus More", "Choose to add to list");
		public GUIContent iconToolbarMinus = EditorGUIUtility.IconContent ("Toolbar Minus", "Remove selection from list");
		public readonly GUIStyle draggingHandle = "RL DragHandle";
		public readonly GUIStyle headerBackground = "RL Header";
		public readonly GUIStyle footerBackground = "RL Footer";
		public readonly GUIStyle boxBackground = "RL Background";
		public readonly GUIStyle preButton = "RL FooterButton";
		public GUIStyle elementBackground = new GUIStyle ("RL Element");
		const int buttonWidth = 25;
		public const int padding = 6;
		public const int dragHandleWidth = 20;
		
		// draw the default footer
		public void DrawFooter (Rect rect, ReorderableList list)
		{
			float rightEdge = rect.xMax;
			float leftEdge = rightEdge - 8f;
			if (list.displayAdd)
				leftEdge -= 25;
			if (list.displayRemove)
				leftEdge -= 25;
			rect = new Rect(leftEdge, rect.y, rightEdge - leftEdge, rect.height);
			Rect addRect = new Rect(leftEdge + 4, rect.y - 3, 25, 13);
			Rect removeRect = new Rect(rightEdge - 29, rect.y - 3, 25, 13);
			if (Event.current.type == EventType.repaint)
			{
				footerBackground.Draw (rect, false, false, false, false);
			}
			if (list.displayAdd)
			{
				if (GUI.Button (addRect, list.onAddDropdownCallback != null ? iconToolbarPlusMore : iconToolbarPlus, preButton))
				{
					if (list.onAddDropdownCallback != null)
						list.onAddDropdownCallback(addRect, list);
					else if (list.onAddCallback != null)
						list.onAddCallback(list);
					else
						DoAddButton (list);
				}
			}
			if (list.displayRemove)
			{
				if (list.index < 0 || list.index >= list.Count)
					GUI.enabled = false;
				if (list.onCanRemoveCallback != null)
					GUI.enabled &= list.onCanRemoveCallback(list);

				if (GUI.Button (removeRect, iconToolbarMinus, preButton))
				{
					if (list.onRemoveCallback == null)
						DoRemoveButton (list);
					else
						list.onRemoveCallback(list);
				}
				GUI.enabled = true;
			}
		}
		
		// default add button behavior
		public void DoAddButton (ReorderableList list)
		{
			if (list.serializedProperty != null)
			{
				list.serializedProperty.arraySize += 1;
				list.index = list.serializedProperty.arraySize - 1;
			}
			else
			{
				// this is ugly but there are a lot of cases like null types and default constructors
				Type elementType = list.list.GetType().GetElementType();	
				if (elementType == typeof(string))
					list.index = list.list.Add ("");
				else if (elementType != null && elementType.GetConstructor(Type.EmptyTypes) == null)
					Debug.LogError ("Cannot add element. Type " + elementType.ToString () + " has no default constructor. Implement a default constructor or implement your own add behaviour.");
				else if (list.list.GetType().GetGenericArguments()[0] != null)
					list.index = list.list.Add (Activator.CreateInstance(list.list.GetType().GetGenericArguments()[0]));
				else if (elementType != null)
					list.index = list.list.Add (Activator.CreateInstance(elementType));
				else
					Debug.LogError ("Cannot add element of type Null.");
			}
		}
	
		// default remove button behavior
		public void DoRemoveButton (ReorderableList list)
		{
			if (list.serializedProperty != null)
			{
				list.serializedProperty.DeleteArrayElementAtIndex(list.index);
				if (list.index >= list.serializedProperty.arraySize - 1)
					list.index = list.serializedProperty.arraySize - 1;
			}
			else
			{
				list.list.RemoveAt(list.index);
				if (list.index >= list.list.Count - 1)
					list.index = list.list.Count - 1;
			}
			
		}
		
		// draw the default header background
		public void DrawHeaderBackground (Rect headerRect)
		{
			if (Event.current.type == EventType.repaint)
				headerBackground.Draw (headerRect, false, false, false, false);
		}
		
		// draw the default header
		public void DrawHeader (Rect headerRect, SerializedObject serializedObject, SerializedProperty element, IList elementList)
		{
			EditorGUI.LabelField(headerRect, EditorGUIUtility.TempContent ((element != null)? "Serialized Property" : "IList"));
		}
		
		// draw the default element background
		public void DrawElementBackground (Rect rect, bool selected, bool focused, bool draggable)
		{
			if (Event.current.type == EventType.repaint)
			{
				elementBackground.Draw (rect, false, selected, selected, focused);
				if (draggable)
					draggingHandle.Draw (new Rect(rect.x+5, rect.y+7, 10, rect.height-(rect.height - 7)), false, false, false, false);
			}
		}
		
		// draw the default element
		public void DrawElement (Rect rect, SerializedProperty element, System.Object listItem, bool selected, bool focused, bool draggable)
		{
			EditorGUI.LabelField (rect, EditorGUIUtility.TempContent ((element != null)? element.displayName : listItem.ToString()));
		}
		
		// draw the default element
		public void DrawNoneElement (Rect rect, bool draggable)
		{
			EditorGUI.LabelField (rect, EditorGUIUtility.TempContent ("List is Empty"));
		}
	}
	public static Defaults s_Defaults;
		
	// constructors
	public ReorderableList (IList elements, Type elementType)
	{
		InitList(null, null, elements, true, true, true, true);
	}
		
	public ReorderableList (IList elements, Type elementType, bool draggable, bool displayHeader, bool displayAddButton, bool displayRemoveButton)
	{
		InitList(null, null, elements, draggable, displayHeader, displayAddButton, displayRemoveButton);
	}
		
	public ReorderableList (SerializedObject serializedObject, SerializedProperty elements)
	{
		InitList(serializedObject, elements, null, true, true, true, true);		
	}
		
	public ReorderableList (SerializedObject serializedObject, SerializedProperty elements, bool draggable, bool displayHeader, bool displayAddButton, bool displayRemoveButton)
	{
		InitList(serializedObject, elements, null, draggable, displayHeader, displayAddButton, displayRemoveButton);
	}
		
	private void InitList (SerializedObject serializedObject, SerializedProperty elements, IList elementList, bool draggable, bool displayHeader, bool displayAddButton, bool displayRemoveButton)
	{
		id = GUIUtility.GetPermanentControlID();	
		m_SerializedObject = serializedObject;
		m_Elements = elements;
		m_ElementList = elementList;
		m_Draggable = draggable;
		m_Dragging = false;
		m_SlideGroup = new GUISlideGroup();
		displayAdd = displayAddButton;
		m_DisplayHeader = displayHeader;
		displayRemove = displayRemoveButton;
		if (m_Elements != null && m_Elements.editable == false)
			m_Draggable = false;
		if (m_Elements != null && m_Elements.isArray == false)
			Debug.LogError("Input elements should be an Array SerializedProperty");	
	}
	
	public SerializedProperty serializedProperty {
		get { return m_Elements; }
		set { m_Elements = value; }
	}
		
	public IList list {
		get { return m_ElementList; }
		set { m_ElementList = value; }
	}
		
		
	// active element index accessor
	public int index {
		get { return m_ActiveElement; }
		set { m_ActiveElement = value; }
	}
	
	// individual element height accessor
	public float elementHeight = 21;
	// header height accessor
	public float headerHeight = 18;
	// footer height accessor
	public float footerHeight = 13;
		
	// draggable accessor
	public bool draggable {
		get { return m_Draggable; }
		set { m_Draggable = value; }
	}
		
	
	private Rect GetContentRect (Rect rect)
	{
		Rect r = rect;
		
		if (draggable)
			r.xMin += Defaults.dragHandleWidth;
		else
			r.xMin += Defaults.padding;
		r.xMax -= Defaults.padding;
		return r;
	}
	
	public int Count { get { return (m_Elements != null) ? m_Elements.arraySize : m_ElementList.Count; } }
	
	public void DoList ()
	{
		if (s_Defaults == null)
			s_Defaults = new Defaults ();

		// do the parts of our list
		DoListHeader();
		DoListElements();
		DoListFooter();
	}


	private void DoListElements()
	{
		// How many elements? If none, make space for showing default line that shows no elements are present
		int arraySize = Count;
		if (arraySize == 0)
			arraySize = 1;

		// get the rect into which we will draw all elements
		Rect listRect = GUILayoutUtility.GetRect (10, (elementHeight * arraySize) + 7, GUILayout.ExpandWidth (true));

		// draw the background in repaint
		if (Event.current.type == EventType.repaint)
			s_Defaults.boxBackground.Draw (listRect, false, false, false, false);

		// resize to the area that we want to draw our elements into 
		listRect.yMin += 2; listRect.yMax -= 3;


		// create the rect for individual elements in the list
		Rect elementRect = listRect;
		elementRect.height = elementHeight;

		// the content rect is what we will actually draw into -- it doesn't include the drag handle or padding
		Rect elementContentRect = elementRect;

		
		if ((m_Elements != null && m_Elements.isArray == true && m_Elements.arraySize > 0) || (m_ElementList != null && m_ElementList.Count > 0 ))
		{
			// If there are elements, we need to draw them -- we will do this differently depending on if we are dragging or not
			if (IsDragging() && Event.current.type == EventType.Repaint)
			{
				// we are dragging, so we need to build the new list of target indices
				int targetIndex = CalculateRowIndex();
				m_NonDragTargetIndices.Clear ();
				for (int i = 0; i < arraySize; i++)
				{
					if (i != m_ActiveElement)
						m_NonDragTargetIndices.Add(i);	
				}
				m_NonDragTargetIndices.Insert(targetIndex, -1);
			
				// now draw each element in the list (excluding the active element)
				for (int i = 0; i < m_NonDragTargetIndices.Count; i++)
				{
					if (m_NonDragTargetIndices[i] != -1)
					{
						// update the position of the rect (based on element position and accounting for sliding)
						elementRect.y = listRect.y + i * elementHeight;
						elementRect = m_SlideGroup.GetRect(m_NonDragTargetIndices[i], elementRect);
						
						// actually draw the element
						s_Defaults.DrawElementBackground (elementRect, false, false, m_Draggable);
						elementContentRect = GetContentRect (elementRect);
						if (drawElementCallback == null )
						{
							if (m_Elements != null)
								s_Defaults.DrawElement(elementContentRect, m_Elements.GetArrayElementAtIndex(m_NonDragTargetIndices[i]), null, false, false, m_Draggable);
							else
								s_Defaults.DrawElement (elementContentRect, null, m_ElementList[m_NonDragTargetIndices[i]], false, false, m_Draggable);
						}
						else
						{
							drawElementCallback(elementContentRect, m_NonDragTargetIndices[i], false, false);
						} 
					}
				}
			
				// finally get the position of the active element
				elementRect.y = m_DraggedY - m_DragOffset + listRect.y;
				s_Defaults.DrawElementBackground (elementRect, true, true, m_Draggable);
				elementContentRect = GetContentRect (elementRect);

				// draw the active element
				if (drawElementCallback == null )
				{
					if (m_Elements != null)
						s_Defaults.DrawElement(elementContentRect, m_Elements.GetArrayElementAtIndex(m_ActiveElement), null, true, true, m_Draggable);
					else
						s_Defaults.DrawElement (elementContentRect, null, m_ElementList[m_ActiveElement], true, true, m_Draggable);
				}
				else
				{
					drawElementCallback(elementContentRect, m_ActiveElement, true, true);
				} 
			}
			else
			{
				// if we aren't dragging, we just draw all of the elements in order
				for (int i = 0; i < arraySize; i ++)
				{
					bool activeElement = (i == m_ActiveElement);
					bool focusedElement =  (i == m_ActiveElement && GUIUtility.keyboardControl == id);

					// update the position of the element
					elementRect.y = listRect.y + i * elementHeight;
					
					// draw the background
					s_Defaults.DrawElementBackground (elementRect, activeElement, focusedElement, m_Draggable);
					elementContentRect = GetContentRect (elementRect);

					// do the callback for the element
					if (drawElementCallback == null )
					{
						if (m_Elements != null)
							s_Defaults.DrawElement(elementContentRect, m_Elements.GetArrayElementAtIndex(i), null, activeElement, focusedElement, m_Draggable);
						else
							s_Defaults.DrawElement (elementContentRect, null, m_ElementList[i], activeElement, focusedElement, m_Draggable);
					}
					else
					{
						drawElementCallback(elementContentRect, i, activeElement, focusedElement);
					}
				}


			}

			// handle the interaction 
			DoDraggingAndSelection (listRect);
		}
		else
		{
			// there was no content, so we will draw an empty element
			elementRect.y = listRect.y;
			s_Defaults.DrawElementBackground (elementRect, false, false, false);
			elementContentRect = elementRect;
			elementContentRect.xMin += Defaults.padding;
			elementContentRect.xMax -= Defaults.padding;
			s_Defaults.DrawNoneElement (elementContentRect, m_Draggable);
		}

	}

	
	private void DoListHeader()
	{
		// do the custom or default header GUI
		Rect headerRect = GUILayoutUtility.GetRect (0, headerHeight, GUILayout.ExpandWidth (true));

		// draw the background on repaint
		if (Event.current.type == EventType.repaint)
			s_Defaults.DrawHeaderBackground (headerRect);

		// apply the padding to get the internal rect
		headerRect.xMin += Defaults.padding;
		headerRect.xMax -= Defaults.padding;
		headerRect.height -= 2;
		headerRect.y += 1;

		// perform the default or overridden callback
		if (drawHeaderCallback == null && m_DisplayHeader == true)
			s_Defaults.DrawHeader(headerRect, m_SerializedObject, m_Elements, m_ElementList);
		else if (drawHeaderCallback != null)
			drawHeaderCallback(headerRect);

	}

	private void DoListFooter()
	{
		// do the footer GUI
		Rect footerRect = GUILayoutUtility.GetRect (4, footerHeight, GUILayout.ExpandWidth (true));

		// draw the footer if the add or remove buttons are required
		if (displayAdd == true || displayRemove == true)
			 s_Defaults.DrawFooter(footerRect, this);
	}

	private void DoDraggingAndSelection (Rect listRect)
	{
		Event evt = Event.current;
		int oldIndex = m_ActiveElement;
		switch (evt.GetTypeForControl (id))
		{
		case EventType.KeyDown:
			if (GUIUtility.keyboardControl != id)
				return;
			// if we have keyboard focus, arrow through the list
			if (evt.keyCode == KeyCode.DownArrow)
			{
				m_ActiveElement += 1;
				evt.Use ();
			}		
			if (evt.keyCode == KeyCode.UpArrow)
			{
				m_ActiveElement -= 1;
				evt.Use ();
			}
			if (evt.keyCode == KeyCode.Escape && GUIUtility.hotControl == id)
			{
				GUIUtility.hotControl = 0;
				m_Dragging = false;
				evt.Use();
			}
			// don't allow arrowing through the ends of the list
			m_ActiveElement = Mathf.Clamp (m_ActiveElement, 0, (m_Elements != null)? m_Elements.arraySize -1 : m_ElementList.Count - 1);
			break;

		case EventType.MouseDown:
			if (!listRect.Contains (Event.current.mousePosition))
				break;
			// clicking on the list should end editing any existing edits
			EditorGUI.EndEditingActiveTextField ();
			// pick the active element based on click position
			m_ActiveElement = Mathf.FloorToInt ( (Event.current.mousePosition.y - listRect.y) / elementHeight);
		
			if (m_Draggable)
			{
				// if we can drag, set the hot control and start dragging (storing the offset)
				m_DragOffset = (Event.current.mousePosition.y - listRect.y) - (m_ActiveElement  * elementHeight);
				UpdateDraggedY(listRect);
				GUIUtility.hotControl = id;
				m_Dragging = true;
				m_SlideGroup.Reset();
				m_NonDragTargetIndices = new List<int>();
			}
			// set the keyboard control
			GUIUtility.keyboardControl = id;
			evt.Use ();
			break;
			
		case EventType.MouseDrag:
			if (!m_Draggable || GUIUtility.hotControl != id)
				break;
			// if we are dragging, update the position
			UpdateDraggedY(listRect);
			evt.Use ();
			break;

		case EventType.MouseUp:
			if (!m_Draggable || GUIUtility.hotControl != id)
				break;
			evt.Use ();
			m_Dragging = false;
			// wheat will be the index of this if we release?
			int targetIndex = CalculateRowIndex();
			if (m_ActiveElement != targetIndex)
			{
				// if the target index is different than the current index...
				if (m_SerializedObject != null && m_Elements != null)
				{
					// if we are working with Serialized Properties, we can handle it for you
					m_Elements.MoveArrayElement(m_ActiveElement, targetIndex);
					m_SerializedObject.ApplyModifiedProperties ();
					m_SerializedObject.Update ();
				}
				else if (m_ElementList != null)
				{
					// we are working with the IList, which is probably of a fixed length
					System.Object tempObject = m_ElementList[m_ActiveElement];
					for (int i = 0; i < m_ElementList.Count - 1; i++)
					{
						if (i >= m_ActiveElement)
							m_ElementList[i] = m_ElementList[i+1];
					}
					for (int i = m_ElementList.Count - 1; i > 0; i--)
					{
						if (i > targetIndex)
							m_ElementList[i] = m_ElementList[i - 1];	
					}
					m_ElementList[targetIndex] = tempObject;
				}
				// update the active element, now that we've moved it
				m_ActiveElement = targetIndex;
				// give the user a callback
				if (onReorderCallback != null)
					onReorderCallback(this);
			}
			GUIUtility.hotControl = 0;
			m_NonDragTargetIndices = null;
			break;
		}
		// if the index has changed and there is a selected callback, call it
		if (m_ActiveElement != oldIndex && onSelectCallback != null)
			onSelectCallback (this);
	}
	
	private void UpdateDraggedY (Rect listRect)
	{
		m_DraggedY = Mathf.Clamp(Event.current.mousePosition.y - listRect.y, m_DragOffset, listRect.height - (elementHeight - m_DragOffset));
	}
	
	private int CalculateRowIndex ()
	{
		int arraySize = (m_Elements != null)? m_Elements.arraySize : m_ElementList.Count;
		return Mathf.Clamp (Mathf.FloorToInt (m_DraggedY / elementHeight), 0, arraySize -1 );
	}
	

	private bool IsDragging ()
	{
		return m_Dragging;	
	}
	

}
}
