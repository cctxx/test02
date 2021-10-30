using UnityEngine;
using UnityEditorInternal;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditor
{

internal class PopupList : EditorWindow
{
	public delegate void OnSelectCallback (ListElement element);
	static EditorGUI.RecycledTextEditor s_RecycledEditor = new EditorGUI.RecycledTextEditor ();
	static int s_TextFieldHash = "ProjectBrowserPopUpTextField".GetHashCode ();
	public enum Gravity
	{
		Top,
		Bottom
	}

	public class ListElement
	{
		public GUIContent m_Content;
		private float m_FilterScore;
		private bool m_Selected;
		private bool m_WasSelected;
		
		public ListElement(string text, bool selected, float score) {
			m_Content = new GUIContent(text);
			if (!string.IsNullOrEmpty (m_Content.text))
			{
				char[] a = m_Content.text.ToCharArray();
				a[0] = char.ToUpper(a[0]);
				m_Content.text = new string(a);
			}	
			m_Selected = selected;
			filterScore = score;
		}
		
		public ListElement(string text, bool selected) {
			m_Content = new GUIContent(text);
			m_Selected = selected;
			filterScore = 0;
		}
		
		public ListElement(string text) {
			m_Content = new GUIContent(text);
			m_Selected = false;
			filterScore = 0;
		}
		
		public float filterScore {
			get {
				return m_WasSelected ? float.MaxValue : m_FilterScore;
			}
			set {
				m_FilterScore = value;
				m_WasSelected = m_Selected;
			}
		}
		
		public bool selected {
			get {
				return m_Selected;
			}
			set {
				m_Selected = value;
				if (m_Selected)
					m_WasSelected = true;
			}
		}

		public string text {
			get {
				return m_Content.text;
			}
			set {
				m_Content.text = value;
			}
		}
		
		public void ResetScores() {
			m_WasSelected = m_Selected;
		}
	}

	public class InputData
	{
		public List<ListElement> m_ListElements;
		public bool m_CloseOnSelection;
		public bool m_AllowCustom;
		public bool m_SortAlphabetically;
		public OnSelectCallback m_OnSelectCallback;
		public int m_MaxCount;
		int m_SelectedCompletion = 0;
		string m_LastPrefix =  "";
		string m_LastCompletion = "";
		
		public InputData() {
			m_ListElements = new List<ListElement>();
		}
		
		public void DeselectAll ()
		{
			foreach (ListElement element in m_ListElements)
				element.selected = false;
		}

		public void ResetScores() {
			foreach( var element in m_ListElements )
				element.ResetScores();
		}
		
		private IEnumerable<ListElement> BuildQuery (string prefix)
		{
			if ( prefix == "")
				return m_ListElements;
			else	
				return m_ListElements.Where( 
					element => element.m_Content.text.StartsWith(prefix, System.StringComparison.OrdinalIgnoreCase)
				);
		}
		
		public IEnumerable<ListElement> GetFilteredList(string prefix) {
			IEnumerable<ListElement> res = BuildQuery(prefix);
			if (m_MaxCount > 0) {
				res = res.OrderByDescending( element => element.filterScore ).Take(m_MaxCount) ;
			}
			if (m_SortAlphabetically)
				return res.OrderBy( element => element.text.ToLower() );
			else
				return res;
		}
		
		public int GetFilteredCount(string prefix) {
			IEnumerable<ListElement> res = BuildQuery(prefix);
			if (m_MaxCount > 0)
				res = res.Take(m_MaxCount) ;
			return res.Count();
		}
		
		public ListElement NewOrMatchingElement(string label) {
			m_LastCompletion="";
			ListElement res = m_ListElements
				.Where( element =>  element.text.Equals(label, System.StringComparison.OrdinalIgnoreCase) )
				.DefaultIfEmpty(null)
				.FirstOrDefault();
			if ( res == null )
			{
				res = new ListElement (label, false, -1);
				m_ListElements.Add(res);
			}
			return res;
		}
		
		public string GetCompletion(string prefix) {
			IEnumerable<string> query = GetFilteredList(prefix).Select(element => element.text);
			m_LastPrefix = prefix;
			
			if ( m_LastCompletion != "" && m_LastCompletion.StartsWith(prefix, System.StringComparison.OrdinalIgnoreCase) )
			{
				m_SelectedCompletion = query.TakeWhile( element => element != m_LastCompletion ).Count();
				return m_LastCompletion;
			}
			
			return (m_LastCompletion=query.Skip(m_SelectedCompletion).DefaultIfEmpty("").FirstOrDefault());
		}
		
		public void SelectCompletion(int change) {
			int count = GetFilteredList(m_LastPrefix).Count();
			if (m_SelectedCompletion == -1 && change < 0)  // specal case for initial selection
				m_SelectedCompletion = count;

			m_SelectedCompletion = count > 0 ? (m_SelectedCompletion + change) % count : 0;
			if (m_SelectedCompletion < 0 )
				m_SelectedCompletion += count;
			m_LastCompletion = "";
		}
		
		public void ResetSelectedCompletion() {
			m_SelectedCompletion = -1;
			m_LastCompletion = "";
		}
	}

	private class Styles
	{
		public GUIStyle label = "PR Label";
		public GUIStyle background = "grey_border";
		public GUIStyle autocompleteOverlay ;
		public GUIStyle customTextField;
		public GUIStyle customTextFieldCancelButton;
		public GUIStyle customTextFieldCancelButtonEmpty;
		public Styles ()
		{			
			customTextField = new GUIStyle (EditorStyles.toolbarSearchField);
			customTextFieldCancelButton = new GUIStyle (EditorStyles.toolbarSearchFieldCancelButton);
			customTextFieldCancelButtonEmpty = new GUIStyle (EditorStyles.toolbarSearchFieldCancelButtonEmpty);
			autocompleteOverlay = new GUIStyle(background); // TODO: create a specific style for this
		}
	}


	// Static
	static PopupList s_PopupList = null;
	static float s_LastClosedTime;
	static Vector2 s_LastPos = Vector2.zero;
	static Styles s_Styles;

	// State
	private InputData m_Data;

	// Layout
	const float scrollBarWidth = 14;
	const float listElementHeight = 18;
	const float gizmoRightAlign = 23;
	const float iconRightAlign = 64;
	const float frameWidth = 1f;
	const float k_LineHeight = 16;
	const float k_TextFieldHeight = 16;
	const float k_Margin = 10;
	Vector2 m_ScrollPosition;
	Vector2 m_ScreenPos;
	string m_CustomLabel = "";
	string m_CustomLabelPrefix = "";
	Gravity m_Gravity;

	internal static bool isOpen { get { return s_PopupList != null; } }

	PopupList ()
	{
		hideFlags = HideFlags.DontSave;
	}

	void OnEnable()
	{
	}

	void OnDisable ()
	{
		m_Data.ResetScores();
		s_LastClosedTime = Time.realtimeSinceStartup;
		s_PopupList = null;
	}

	internal static bool ShowAtPosition (Vector2 pos, InputData inputData, Gravity gravity)
	{
		bool justClosed = Mathf.Abs (Time.realtimeSinceStartup - s_LastClosedTime) < 0.05f;
		bool newPos = (s_LastPos - pos).magnitude > 0.1f;

		if (newPos || !justClosed)
		{
			Event.current.Use ();
			if (s_PopupList == null) {
				s_PopupList = CreateInstance <PopupList>();
				s_PopupList.hideFlags = HideFlags.HideAndDontSave;
			}
			s_LastPos = pos;
			s_PopupList.Init(pos, inputData, gravity);
			return true;
		}
		return false;
	}
	
	internal static void CloseList ()
	{
		if (s_PopupList != null)
			s_PopupList.Close ();
	}

	float GetWindowHeight ()
	{
		return m_Data.GetFilteredCount(m_CustomLabelPrefix) * k_LineHeight + 2 * k_Margin + (m_Data.m_AllowCustom ? k_TextFieldHeight : 0 );
	}

	float GetWindowWidth ()
	{
		return 150f;
	}

	void Init (Vector2 pos, InputData inputData, Gravity gravity)
	{
		m_Data = inputData;
		m_Data.ResetScores();
		m_Data.ResetSelectedCompletion();
		m_Gravity = gravity;
		m_ScreenPos = GUIUtility.GUIToScreenPoint(pos);


		Rect buttonRect = new Rect (m_ScreenPos.x, m_ScreenPos.y - 16, 16, 16); // fake a button: we know we are showing it below the bottonRect if possible
		ShowAsDropDown (buttonRect, new Vector2 (GetWindowWidth(), GetWindowHeight()));
	}

	void Cancel ()
	{
		// Undo changes we have done. 
		// PerformTemporaryUndoStack must be called before Close() below
		// to ensure that we properly undo changes before closing window.
		//Undo.PerformTemporaryUndoStack();

		Close();
		GUI.changed = true;
		GUIUtility.ExitGUI();
	}

	private void ResetWindowSize()
	{
		// Window size
		float windowHeight = GetWindowHeight (); 
		Rect rect = new Rect
		            	{
		            		x = m_ScreenPos.x,
		            		y = m_ScreenPos.y - (m_Gravity == Gravity.Bottom ? windowHeight : 0 ),
		            		width = GetWindowWidth (),
							height = windowHeight
		            	};

		float minHeight = windowHeight;
		position = m_Parent.window.FitPopupWindowRectToScreen (rect, minHeight);
		minSize = new Vector2 (position.width, position.height);
		maxSize = new Vector2 (position.width, position.height);
	}

	internal void OnGUI ()
	{
		Event evt = Event.current;		
		// We do not use the layout event
		if (evt.type == EventType.layout)
			return;
	
		if (s_Styles == null)
			s_Styles = new Styles ();

		if (evt.type == EventType.KeyDown && evt.keyCode == KeyCode.Escape)
			Cancel ();

		if (m_Gravity == Gravity.Bottom)
		{
			DrawList ();
			DrawCustomTextField();
		}
		else
		{
			DrawCustomTextField();
			DrawList ();
		}

		// Background with 1 pixel border (rendered above content)
		if ( evt.type == EventType.Repaint)
			s_Styles.background.Draw(new Rect(0, 0, position.width, position.height), false, false, false, false);
	}
	

	private void DrawCustomTextField()
	{
		if (!m_Data.m_AllowCustom) 
			return;

		Event evt = Event.current;
		bool enableAutoCompletion = true;
		bool updateCompletion = false;
		bool closeWindow = false;

		string origLabel = m_CustomLabel;
		if (evt.type == EventType.keyDown ) {
			switch (evt.keyCode)
			{
				case KeyCode.Comma:
					goto case KeyCode.Return;
				case KeyCode.Space:
					goto case KeyCode.Return;
				case KeyCode.Tab:
					goto case KeyCode.Return;
				case KeyCode.Return:
					if (m_CustomLabel != "" ) {
						// Toggle state
						if (m_Data.m_OnSelectCallback != null)
							m_Data.m_OnSelectCallback(m_Data.NewOrMatchingElement(m_CustomLabel));
						
						if (m_Data.GetFilteredCount(m_CustomLabelPrefix) <= 2)
						{
							updateCompletion = true;
							m_CustomLabelPrefix="";
							enableAutoCompletion = false;
							m_Data.ResetSelectedCompletion();
						
							origLabel=m_CustomLabelPrefix;
							m_CustomLabel=m_CustomLabelPrefix;
							s_RecycledEditor.content.text = m_CustomLabelPrefix;
							EditorGUI.s_OriginalText = m_CustomLabelPrefix;
						}

						// Auto close 
						if (m_Data.m_CloseOnSelection || evt.keyCode==KeyCode.Return) 
							closeWindow=true;
						// Recalculate size
						else
							ResetWindowSize();
					}
					evt.Use();
					break;
				case KeyCode.Backspace:
					enableAutoCompletion = false;
					m_Data.ResetSelectedCompletion();
					break;

				case KeyCode.DownArrow:
					m_Data.SelectCompletion(1);
					updateCompletion = true;
					evt.Use();	
					break;
					
				case KeyCode.UpArrow:
					m_Data.SelectCompletion(-1);
					updateCompletion = true;
					evt.Use();
					break;
				case KeyCode.None:
					if ( evt.character == ' ' || evt.character == ',')
						evt.Use();
					break;
				
			}
			
		}
		
		{
				bool dummy = false;
				Rect pos = new Rect (k_Margin/2, m_Gravity == Gravity.Top?(k_Margin/2):(position.height-k_TextFieldHeight-k_Margin/2), position.width-k_Margin-14, k_TextFieldHeight);
				int id = EditorGUIUtility.GetControlID (s_TextFieldHash, FocusType.Keyboard, pos);
				
				if ( GUIUtility.keyboardControl == 0)
					GUIUtility.keyboardControl = id;
	
				m_CustomLabel =  EditorGUI.DoTextField (s_RecycledEditor, id, pos, m_CustomLabel, s_Styles.customTextField, null, out dummy, false, false, false);
				//GUI.TextField (, m_CustomLabel, 25);
				Rect buttonRect = pos;
				buttonRect.x += pos.width;
				buttonRect.width = 14;
				if (GUI.Button (buttonRect, GUIContent.none, m_CustomLabel != "" ? s_Styles.customTextFieldCancelButton : s_Styles.customTextFieldCancelButtonEmpty) && m_CustomLabel != "")
				{
					m_CustomLabelPrefix="";
					origLabel=m_CustomLabelPrefix;
					m_CustomLabel=m_CustomLabelPrefix;
					s_RecycledEditor.content.text = m_CustomLabelPrefix;
					EditorGUI.s_OriginalText = m_CustomLabelPrefix;

					s_RecycledEditor.pos=0;
					s_RecycledEditor.selectPos=0;
					enableAutoCompletion = false;
					m_Data.ResetSelectedCompletion();
					ResetWindowSize();					
				}
		}
			
		if(m_CustomLabel != origLabel || updateCompletion ) {
			m_CustomLabelPrefix = s_RecycledEditor.pos < m_CustomLabel.Length && s_RecycledEditor.pos >=0 ? m_CustomLabel.Substring(0,s_RecycledEditor.pos) : m_CustomLabel;
			ResetWindowSize();
			
			if(enableAutoCompletion) {
				string completed=m_Data.GetCompletion(m_CustomLabelPrefix);
				if (completed != "") {
					int cursor = s_RecycledEditor.pos;//m_CustomLabel.Length;
					int selectPos = completed.Length;
					m_CustomLabel = completed;
					s_RecycledEditor.content.text = m_CustomLabel;
					EditorGUI.s_OriginalText = m_CustomLabel;
					s_RecycledEditor.pos = cursor;
					s_RecycledEditor.selectPos = selectPos;
				}
			}
		}
		if(closeWindow)
			Close();
	}	


	void DrawList ()
	{
		Event evt = Event.current;

		int i=-1;
		foreach (var element in m_Data.GetFilteredList(m_CustomLabelPrefix) )
		{
			i++;
			Rect rect = new Rect(0, k_Margin + i * k_LineHeight + (m_Gravity == Gravity.Top && m_Data.m_AllowCustom?k_TextFieldHeight:0), position.width, k_LineHeight);

			switch (evt.type)
			{
				case EventType.Repaint:
					{
						GUIStyle style = s_Styles.label;
						style.padding.left = (int)k_Margin;

						bool selected = element.selected; ;
						bool focused = m_Parent.hasFocus;
						bool isHover = false;
						bool isActive = selected;

						GUIContent content = element.m_Content;
						style.Draw(rect, content, isHover, isActive, selected, focused);
						
						
						// Draw a border around the current autoselect item
						if (element.text.Equals(m_CustomLabel, System.StringComparison.OrdinalIgnoreCase))
						{
							rect.x+=1;
							rect.width-=2;
							s_Styles.autocompleteOverlay.Draw(rect, false, false, false, false);
						}
					}
					break;
				case EventType.MouseDown:
					{
						if (Event.current.button == 0 && rect.Contains (Event.current.mousePosition))
						{
							// Toggle state
							if (m_Data.m_OnSelectCallback != null)
								m_Data.m_OnSelectCallback(element);
							
							evt.Use();

							// Auto close 
							if (m_Data.m_CloseOnSelection)
								Close ();
						}
					}
					break;
			}
		}
	}
}


}
