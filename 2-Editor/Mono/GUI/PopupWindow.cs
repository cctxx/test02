using UnityEngine;

namespace UnityEditor
{

/*
 Note that content of PopupWindow do not survive assembly reloading because it derives from interface IPopupWindowContent. 
 E.g use it for short lived content where closing on lost focus is ok.
 */


internal interface IPopupWindowContent
{
	void OnGUI (EditorWindow caller, Rect rect);
	Vector2 GetWindowSize ();
	void OnDisable ();
}


class PopupWindow : EditorWindow
{
	IPopupWindowContent m_WindowContent;
	Vector2 m_LastWantedSize = Vector2.zero;
	Rect m_ActivatorRect;
	static double s_LastClosedTime;
	static Rect s_LastActivatorRect;
	
	public PopupWindow ()
	{
		hideFlags = HideFlags.DontSave;
		wantsMouseMove = true;
	}

	public static void Show (Rect activatorRect, IPopupWindowContent windowContent)
	{
		Show (activatorRect, windowContent, null);
	}

	// Shown on top of any previous windows
	public static void Show (Rect activatorRect, IPopupWindowContent windowContent, PopupLocationHelper.PopupLocation[] locationPriorityOrder)
	{
		if (ShouldShowWindow (activatorRect))
		{
			PopupWindow win = CreateInstance<PopupWindow>();
			if (win != null)
			{
				win.Init(activatorRect, windowContent, locationPriorityOrder);
			}
			EditorGUIUtility.ExitGUI (); // Needed to prevent GUILayout errors on OSX
		}
	}

	static bool ShouldShowWindow (Rect activatorRect)
	{
		const double kJustClickedTime = 0.05;
		bool justClosed = (EditorApplication.timeSinceStartup - s_LastClosedTime) < kJustClickedTime;
		if (!justClosed || activatorRect != s_LastActivatorRect)
		{
			s_LastActivatorRect = activatorRect;
			return true;
		}
		return false;
	}

	void Init (Rect activatorRect, IPopupWindowContent windowContent, PopupLocationHelper.PopupLocation[] locationPriorityOrder)
	{
		m_WindowContent = windowContent;
		m_ActivatorRect = GUIUtility.GUIToScreenRect(activatorRect);
		ShowAsDropDown(m_ActivatorRect, m_WindowContent.GetWindowSize(), locationPriorityOrder);
	}

	public void OnGUI()
	{
		FitWindowToContent ();
		Rect windowRect = new Rect(0, 0, position.width, position.height);
		m_WindowContent.OnGUI (this, windowRect);
		GUI.Label(windowRect, GUIContent.none, "grey_border");
	}

	private void FitWindowToContent ()
	{
		Vector2 wantedSize = m_WindowContent.GetWindowSize ();
		if (m_LastWantedSize != wantedSize) 
		{
			m_LastWantedSize = wantedSize;
			
			Rect screenRect = m_Parent.window.GetDropDownRect (m_ActivatorRect, wantedSize, wantedSize);
			m_Pos = screenRect;
			minSize = maxSize = new Vector2(screenRect.width, screenRect.height);
		}
	}

	void OnDisable ()
	{
		s_LastClosedTime = EditorApplication.timeSinceStartup;
		if (m_WindowContent != null)
			m_WindowContent.OnDisable ();
	}
}

}
