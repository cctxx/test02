using UnityEngine;
using UnityEditor;

namespace UnityEditor {

	// This uses a normal editor window with a single view inside.
	internal class PaneDragTab : GUIView {
	    [SerializeField]
		bool m_Shadow;
	
		static PaneDragTab s_Get;
		const float kTopThumbnailOffset = 10;
	    [SerializeField]
	    Vector2 m_ThumbnailSize = new Vector2 (80, 60);
	
	    Rect m_StartRect;
	    [SerializeField]
	    Rect m_TargetRect;
	    [SerializeField]
	    static GUIStyle s_PaneStyle, s_TabStyle;
		
		AnimValueManager m_Anims = new AnimValueManager ();
		AnimBool m_PaneVisible = new AnimBool ();
		AnimBool m_TabVisible = new AnimBool ();
	
	    float m_StartAlpha = 1.0f;
	    float m_TargetAlpha = 1.0f;
	
	    bool m_DidResizeOnLastLayout = false;
	    DropInfo.Type m_Type = (DropInfo.Type)(-1);
	    float m_StartTime = 0.0f;
	
		public GUIContent content;
		Texture2D m_Thumbnail;
	    [SerializeField]
	    internal ContainerWindow m_Window;
	    [SerializeField]
	    ContainerWindow m_InFrontOfWindow = null;
	
		static public PaneDragTab get { 
			get { 
				if (!s_Get) { 
					Object[] objs = Resources.FindObjectsOfTypeAll(typeof (PaneDragTab));
					if (objs.Length != 0)
					s_Get = (PaneDragTab)objs[0];
					if (s_Get) {
						return s_Get;
					} 
					s_Get = ScriptableObject.CreateInstance<PaneDragTab>();
				} 
				return s_Get; 
			} 
		}
			
		public void OnEnable ()
		{
			m_Anims.Add (m_PaneVisible);
			m_Anims.Add (m_TabVisible);
		}
		
		public void GrabThumbnail() 
		{
			if (m_Thumbnail != null)
				DestroyImmediate(m_Thumbnail);
			m_Thumbnail = new Texture2D(Screen.width, Screen.height);
			m_Thumbnail.ReadPixels(new Rect(0,0,Screen.width, Screen.height),0,0);
			m_Thumbnail.Apply();
			
			// scale not to be larger then maxArea pixels.
			const float maxArea = 50000.0f;
			float area = m_Thumbnail.width * m_Thumbnail.height;
			m_ThumbnailSize = new Vector2 (m_Thumbnail.width, m_Thumbnail.height) * Mathf.Sqrt(Mathf.Clamp01(maxArea/area));
		}
	
		public void SetDropInfo (DropInfo di, Vector2 mouseScreenPos, ContainerWindow inFrontOf) {
			if (m_Type != di.type || (di.type == DropInfo.Type.Pane && di.rect != m_TargetRect))
			{
				m_Type = di.type;
				m_StartRect = GetInterpolatedRect(CalcFade());
				
				m_StartTime = Time.realtimeSinceStartup;	
				switch (di.type) {
					case DropInfo.Type.Window: 
			            m_TargetAlpha = 0.6f;
						break;
					case DropInfo.Type.Pane:
					case DropInfo.Type.Tab:
			            m_TargetAlpha = 1.0f;
						break;
				}
			}
	
			switch (di.type) {
				case DropInfo.Type.Window: 
					m_TargetRect = new Rect (mouseScreenPos.x - m_ThumbnailSize.x/2, mouseScreenPos.y - m_ThumbnailSize.y/2, 
					                       m_ThumbnailSize.x, m_ThumbnailSize.y);
					break;
				case DropInfo.Type.Pane:
				case DropInfo.Type.Tab:
					m_TargetRect = di.rect;
					break;
			}
	
			m_PaneVisible.target = (di.type == DropInfo.Type.Pane);
			m_TabVisible.target = (di.type == DropInfo.Type.Tab);
				
			m_TargetRect.x = Mathf.Round (m_TargetRect.x);
			m_TargetRect.y = Mathf.Round (m_TargetRect.y);
			m_TargetRect.width = Mathf.Round (m_TargetRect.width);
			m_TargetRect.height = Mathf.Round (m_TargetRect.height);
	
			m_InFrontOfWindow = inFrontOf;
			m_Window.MoveInFrontOf(m_InFrontOfWindow);
			
			// On Windows, repainting without setting proper size first results in one garbage frame... For some reason.
			SetWindowPos (GetInterpolatedRect(CalcFade()));
			// Yes, fucking repaint.
			Repaint ();
		}
	
	public void Close () {			
		if (m_Thumbnail != null)
			DestroyImmediate(m_Thumbnail);
		if (m_Window)
			m_Window.Close ();	
		DestroyImmediate (this,true);
		s_Get = null;
	}

	public void Show (Rect pixelPos, Vector2 mouseScreenPosition) {
		if (!m_Window) {
			m_Window = ScriptableObject.CreateInstance<ContainerWindow>();
			m_Window.m_DontSaveToLayout = true;
			SetMinMaxSizes (Vector2.zero, new Vector2(10000,10000));
			SetWindowPos (pixelPos);
			m_Window.mainView = this;
		} else {
			SetWindowPos (pixelPos);
		}
		m_Window.Show (ShowMode.NoShadow, true, false);
		
		m_TargetRect = pixelPos;
	}
	
	void SetWindowPos (Rect screenPosition) {
		m_Window.position = screenPosition;
	}
	
		float CalcFade ()
		{
			if (Application.platform == RuntimePlatform.WindowsEditor)
				// No fancy fading on windows. Windows repainting fucks it up too much.
				return 1;
			else
				return Mathf.SmoothStep(0,1,Mathf.Clamp01(5.0f*(Time.realtimeSinceStartup-m_StartTime)));
		}
		
		Rect GetInterpolatedRect (float fade)
		{
			return new Rect(
				Mathf.Lerp (m_StartRect.x, m_TargetRect.x, fade),
				Mathf.Lerp (m_StartRect.y, m_TargetRect.y, fade),
				Mathf.Lerp (m_StartRect.width, m_TargetRect.width, fade),
				Mathf.Lerp (m_StartRect.height, m_TargetRect.height, fade)
			);
		}
		
		void OnGUI () {
			float fade = CalcFade();	
			if (s_PaneStyle == null)
				{
					s_PaneStyle = "dragtabdropwindow";
					s_TabStyle = "dragtab";
				}
			if (Event.current.type == EventType.Layout)
			{
				// Resizing the window casuses new layout events,
				// which causes an endless recursion. Unity does catch that internally, but it slows down the 
				// animation a lot, so just avoid it, by only resizing on every other layout event. 
				m_DidResizeOnLastLayout = !m_DidResizeOnLastLayout;
				if(!m_DidResizeOnLastLayout) {
					SetWindowPos (GetInterpolatedRect(fade));
			
					if (Application.platform == RuntimePlatform.OSXEditor)      
						m_Window.SetAlpha (Mathf.Lerp (m_StartAlpha, m_TargetAlpha, fade));
					return;
				}
			}
			
			if (Event.current.type == EventType.Repaint)
			{
				Color oldGUIColor = GUI.color;
				GUI.color = new Color (1,1,1,1);
				if (m_Thumbnail != null)
					GUI.DrawTexture(new Rect (0,0,position.width, position.height), m_Thumbnail, ScaleMode.StretchToFill, false);
	
				
				// TODO: Fade the styles
				if (m_TabVisible.faded != 0f)
				{
					GUI.color = new Color (1,1,1,m_TabVisible.faded);
					s_TabStyle.Draw (new Rect (0,0,position.width, position.height), content, false,  false, true, true);
				}
				if (m_PaneVisible.faded != 0f)
				{
					GUI.color = new Color (1,1,1,m_PaneVisible.faded);
					s_PaneStyle.Draw (new Rect (0,0,position.width, position.height), content, false,  false, true, true);
				}
				GUI.color = oldGUIColor;
			}
		
			// On the mac we don't get any events inside a window until we release the mouse button in the source window. This window is used to visualize drags, 
			// so we can't wait for that.
			if(Application.platform!=RuntimePlatform.WindowsEditor)
				Repaint ();
		}
	}

} // namespace