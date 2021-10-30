using UnityEngine;
using UnityEditor;
using System.Collections;

namespace UnityEditor
{

// NOTE: do _not_ use GUILayout to get the rectangle for zoomable area,
// will not work and will start failing miserably (mouse events not hitting it, etc.).
// That's because ZoomableArea is using GUILayout itself, and needs an actual
// physical rectangle.

[System.Serializable]
internal class ZoomableArea
{
	// Global state
	private static Vector2 m_MouseDownPosition = new Vector2(-1000000,-1000000); // in transformed space
	private static int zoomableAreaHash = "ZoomableArea".GetHashCode();
	
	// Range lock settings
	private bool m_HRangeLocked;
	private bool m_VRangeLocked;
	public bool hRangeLocked { get { return m_HRangeLocked; } set { m_HRangeLocked = value; } }
	public bool vRangeLocked { get { return m_VRangeLocked; } set { m_VRangeLocked = value; } }
	
	private float m_HRangeMin = Mathf.NegativeInfinity;
	private float m_HRangeMax = Mathf.Infinity;
	private float m_VRangeMin = Mathf.NegativeInfinity;
	private float m_VRangeMax = Mathf.Infinity;
	public float hRangeMin { get { return m_HRangeMin; } set { m_HRangeMin = value; } }
	public float hRangeMax { get { return m_HRangeMax; } set { m_HRangeMax = value; } }
	public float vRangeMin { get { return m_VRangeMin; } set { m_VRangeMin = value; } }
	public float vRangeMax { get { return m_VRangeMax; } set { m_VRangeMax = value; } }
	
	private float m_HScaleMin = 0.001f;
	private float m_HScaleMax = 100000.0f;
	private float m_VScaleMin = 0.001f;
	private float m_VScaleMax = 100000.0f;
	
	// Window resize settings
	private bool m_ScaleWithWindow = false;
	public bool scaleWithWindow { get { return m_ScaleWithWindow; } set { m_ScaleWithWindow = value; } }
	
	// Slider settings
	private bool m_HSlider = true;
	private bool m_VSlider = true;
	public bool hSlider { get { return m_HSlider; } set { Rect r = rect; m_HSlider = value; rect = r; } }
	public bool vSlider { get { return m_VSlider; } set { Rect r = rect; m_VSlider = value; rect = r; } }
	
	private bool m_IgnoreScrollWheelUntilClicked = false;
	public bool ignoreScrollWheelUntilClicked { get { return m_IgnoreScrollWheelUntilClicked; } set { m_IgnoreScrollWheelUntilClicked = value; } }
	
	// View and drawing settings
	private Rect m_DrawArea = new Rect (0,0,100,100);
	internal void SetDrawRectHack (Rect r, bool scrollbars) { m_DrawArea = r; m_HSlider = m_VSlider = scrollbars; } 
	internal Vector2 m_Scale = new Vector2(1, -1);
	internal Vector2 m_Translation = new Vector2(0, 0);
	private float m_MarginLeft, m_MarginRight, m_MarginTop, m_MarginBottom;
	private Rect m_LastShownAreaInsideMargins = new Rect(0, 0, 100, 100);
	
	public Vector2 scale { get { return m_Scale; } }
	public float margin { set { m_MarginLeft = m_MarginRight = m_MarginTop = m_MarginBottom = value; } }
	public float leftmargin { get { return m_MarginLeft; } set { m_MarginLeft = value; } }
	public float rightmargin { get { return m_MarginRight; } set { m_MarginRight = value; } }
	public float topmargin { get { return m_MarginTop; } set { m_MarginTop = value; } }
	public float bottommargin { get { return m_MarginBottom; } set { m_MarginBottom = value; } }

	// IDs for scrollbars
	int verticalScrollbarID, horizontalScrollbarID;
	
	bool m_MinimalGUI;

	[System.Serializable]
	public class Styles 
	{
		public GUIStyle horizontalScrollbar;
		public GUIStyle horizontalMinMaxScrollbarThumb;
		public GUIStyle horizontalScrollbarLeftButton;
		public GUIStyle horizontalScrollbarRightButton;
		public GUIStyle verticalScrollbar;
		public GUIStyle verticalMinMaxScrollbarThumb;
		public GUIStyle verticalScrollbarUpButton;
		public GUIStyle verticalScrollbarDownButton;

		public float sliderWidth;
		public float visualSliderWidth;
		public Styles (bool minimalGUI)
		{
			if (minimalGUI)
			{
				visualSliderWidth = 0;
				sliderWidth = 15;
			} 
			else
			{
				visualSliderWidth = 15;
				sliderWidth = 15;
			}
		}
		public void InitGUIStyles (bool minimalGUI)
		{		
			if (minimalGUI)
			{
				horizontalMinMaxScrollbarThumb = "MiniMinMaxSliderHorizontal";
				horizontalScrollbarLeftButton = GUIStyle.none;
				horizontalScrollbarRightButton = GUIStyle.none;
				horizontalScrollbar = GUIStyle.none;
				verticalMinMaxScrollbarThumb = "MiniMinMaxSlidervertical";
				verticalScrollbarUpButton = GUIStyle.none;
				verticalScrollbarDownButton = GUIStyle.none;
				verticalScrollbar = GUIStyle.none;
			} 
			else
			{
				horizontalMinMaxScrollbarThumb = "horizontalMinMaxScrollbarThumb";
				horizontalScrollbarLeftButton = "horizontalScrollbarLeftbutton";
				horizontalScrollbarRightButton = "horizontalScrollbarRightbutton";
				horizontalScrollbar = GUI.skin.horizontalScrollbar;
				verticalMinMaxScrollbarThumb = "verticalMinMaxScrollbarThumb";
				verticalScrollbarUpButton = "verticalScrollbarUpbutton";
				verticalScrollbarDownButton = "verticalScrollbarDownbutton";
				verticalScrollbar = GUI.skin.verticalScrollbar;
			}
		}

	}
	Styles styles;
	
	public void OnEnable ()
	{
		styles = new Styles (m_MinimalGUI);
	}

	public Rect rect
	{
		get { return new Rect(drawRect.x, drawRect.y, drawRect.width + (m_VSlider ? styles.visualSliderWidth : 0), drawRect.height + (m_HSlider ? styles.visualSliderWidth : 0)); }
		set
		{
			Rect newDrawArea = new Rect(value.x, value.y, value.width - (m_VSlider ? styles.visualSliderWidth : 0), value.height - (m_HSlider ? styles.visualSliderWidth : 0));
			if (newDrawArea != m_DrawArea)
			{
				if (m_ScaleWithWindow)
				{
					m_DrawArea = newDrawArea;
					shownAreaInsideMargins = m_LastShownAreaInsideMargins;
				}
				else
				{
					m_Translation += new Vector2((newDrawArea.width - m_DrawArea.width) / 2, (newDrawArea.height - m_DrawArea.height) / 2);
					m_DrawArea = newDrawArea;
				}
			}
			EnforceScaleAndRange();
		}
	}
	public Rect drawRect { get { return m_DrawArea; } }
	
	public void SetShownHRangeInsideMargins (float min, float max)
	{
		m_Scale.x = (drawRect.width - leftmargin - rightmargin) / (max-min);
		m_Translation.x = -min * m_Scale.x + leftmargin;
		EnforceScaleAndRange();
	}
	
	public void SetShownHRange (float min, float max)
	{
		m_Scale.x = drawRect.width / (max-min);
		m_Translation.x = -min * m_Scale.x;
		EnforceScaleAndRange();
	}
	
	public void SetShownVRangeInsideMargins (float min, float max)
	{
		m_Scale.y = -(drawRect.height - topmargin - bottommargin) / (max-min);
		m_Translation.y = drawRect.height - min * m_Scale.y - topmargin;
		EnforceScaleAndRange();
	}
	
	public void SetShownVRange (float min, float max)
	{
		m_Scale.y = -drawRect.height / (max-min);
		m_Translation.y = drawRect.height - min * m_Scale.y;
		EnforceScaleAndRange();
	}
	
	// ShownArea is in curve space
	public Rect shownArea
	{
		set
		{
			m_Scale.x = drawRect.width / value.width;
			m_Scale.y = -drawRect.height / value.height;
			m_Translation.x = -value.x * m_Scale.x;
			m_Translation.y = drawRect.height - value.y * m_Scale.y;
			EnforceScaleAndRange();
		}
		get
		{
			return new Rect (
				-m_Translation.x / m_Scale.x,
				-(m_Translation.y - drawRect.height ) / m_Scale.y,
				drawRect.width / m_Scale.x,
				drawRect.height / -m_Scale.y
			);
		}
	}
	
	public Rect shownAreaInsideMargins
	{
		set
		{
			shownAreaInsideMarginsInternal = value;
			EnforceScaleAndRange();
		}
		get
		{
			return shownAreaInsideMarginsInternal;
		}
	}
	
	private Rect shownAreaInsideMarginsInternal
	{
		set
		{
			m_Scale.x = (drawRect.width - leftmargin - rightmargin) / value.width;
			m_Scale.y = -(drawRect.height - topmargin - bottommargin) / value.height;
			m_Translation.x = -value.x * m_Scale.x + leftmargin;
			m_Translation.y = drawRect.height - value.y * m_Scale.y - topmargin;
		}
		get
		{
			float leftmarginRel = leftmargin / m_Scale.x;
			float rightmarginRel = rightmargin / m_Scale.x;
			float topmarginRel = topmargin / m_Scale.y;
			float bottommarginRel = bottommargin / m_Scale.y;

			Rect area = shownArea;
			area.x += leftmarginRel;
			area.y -= topmarginRel;
			area.width -= leftmarginRel + rightmarginRel;
			area.height += topmarginRel + bottommarginRel;
			return area;
		}
	}
	
	public virtual Bounds drawingBounds
	{
		get
		{
			bool finiteH = hRangeMin > Mathf.NegativeInfinity && hRangeMax < Mathf.Infinity;
			bool finiteV = vRangeMin > Mathf.NegativeInfinity && vRangeMax < Mathf.Infinity;
			return new Bounds (
				new Vector3 (finiteH ? (hRangeMin+hRangeMax) * 0.5f : 0, finiteV ? (vRangeMin+hRangeMax) * 0.5f : 0, 0),
				new Vector3 (finiteH ? hRangeMax-hRangeMin : 2, finiteV ? vRangeMax-vRangeMin : 2, 1)
			);
		}
	}
	
	
	// Utility transform functions
	
	public Matrix4x4 drawingToViewMatrix
	{ get { 
		return Matrix4x4.TRS(m_Translation, Quaternion.identity, new Vector3(m_Scale.x, m_Scale.y, 1));
	} }
	
	public Vector2 DrawingToViewTransformPoint (Vector2 lhs)
		{ return new Vector2(lhs.x * m_Scale.x + m_Translation.x, lhs.y * m_Scale.y + m_Translation.y); }
	public Vector3 DrawingToViewTransformPoint (Vector3 lhs)
		{ return new Vector3(lhs.x * m_Scale.x + m_Translation.x, lhs.y * m_Scale.y + m_Translation.y, 0); }
	
	public Vector2 ViewToDrawingTransformPoint (Vector2 lhs)
		{ return new Vector2((lhs.x - m_Translation.x) / m_Scale.x , (lhs.y - m_Translation.y) / m_Scale.y); }
	public Vector3 ViewToDrawingTransformPoint (Vector3 lhs)
		{ return new Vector3((lhs.x - m_Translation.x) / m_Scale.x , (lhs.y - m_Translation.y) / m_Scale.y, 0); }
	
	public Vector2 DrawingToViewTransformVector (Vector2 lhs)
		{ return new Vector2(lhs.x * m_Scale.x, lhs.y * m_Scale.y); }
	public Vector3 DrawingToViewTransformVector (Vector3 lhs)
		{ return new Vector3(lhs.x * m_Scale.x, lhs.y * m_Scale.y, 0); }
	
	public Vector2 ViewToDrawingTransformVector (Vector2 lhs)
		{ return new Vector2(lhs.x / m_Scale.x, lhs.y / m_Scale.y); }
	public Vector3 ViewToDrawingTransformVector (Vector3 lhs)
		{ return new Vector3(lhs.x / m_Scale.x, lhs.y / m_Scale.y, 0); }
	
	public Vector2 mousePositionInDrawing
	{
		get { return ViewToDrawingTransformPoint(Event.current.mousePosition); }
	}
	
	public Vector2 NormalizeInViewSpace(Vector2 vec)
	{
		vec = Vector2.Scale(vec,m_Scale);
		vec /= vec.magnitude;
		return Vector2.Scale(vec,new Vector2(1/m_Scale.x,1/m_Scale.y));
	}
	
	// Utility mouse event functions
	
	private bool IsZoomEvent ()
	{
		return (
			(Event.current.button == 1 && Event.current.alt) // right+alt drag
			//|| (Event.current.button == 0 && Event.current.command) // left+commend drag
			//|| (Event.current.button == 2 && Event.current.command) // middle+command drag
			
		);
	}
	
	private bool IsPanEvent ()
	{
		return (
			(Event.current.button == 0 && Event.current.alt) // left+alt drag
			|| (Event.current.button == 2 && !Event.current.command) // middle drag
		);
	}
	public ZoomableArea () {
		m_MinimalGUI = false;
		styles = new Styles (false);
	}
	
	public ZoomableArea (bool minimalGUI) {
		m_MinimalGUI = minimalGUI;
		styles = new Styles (minimalGUI);
	}
	
	public void BeginViewGUI ()
	{
		if (styles.horizontalScrollbar == null)
			styles.InitGUIStyles (m_MinimalGUI);

		HandleZoomAndPanEvents (m_DrawArea);

		horizontalScrollbarID = GUIUtility.GetControlID (EditorGUIExt.s_MinMaxSliderHash, FocusType.Passive);
		verticalScrollbarID = GUIUtility.GetControlID (EditorGUIExt.s_MinMaxSliderHash, FocusType.Passive);

		if (!m_MinimalGUI || Event.current.type != EventType.Repaint)
			SliderGUI();
	}

	public void HandleZoomAndPanEvents(Rect area)
	{
		GUILayout.BeginArea(area);
		
		area.x = 0;
		area.y = 0;
		int id = GUIUtility.GetControlID(zoomableAreaHash, FocusType.Native, area);

		switch (Event.current.GetTypeForControl(id))
		{
			case EventType.mouseDown:
				if (area.Contains(Event.current.mousePosition))
				{
					// Catch keyboard control when clicked inside zoomable area
					// (used to restrict scrollwheel)
					GUIUtility.keyboardControl = id;

					if (IsZoomEvent() || IsPanEvent())
					{
						GUIUtility.hotControl = id;
						m_MouseDownPosition = mousePositionInDrawing;

						Event.current.Use();
					}
				}
				break;
			case EventType.mouseUp:
				//Debug.Log("mouse-up!");
				if (GUIUtility.hotControl == id)
				{
					GUIUtility.hotControl = 0;

					// If we got the mousedown, the mouseup is ours as well
					// (no matter if the click was in the area or not)
					m_MouseDownPosition = new Vector2(-1000000, -1000000);
					//Event.current.Use();
				}
				break;
			case EventType.mouseDrag:
				if (GUIUtility.hotControl != id) break;

				if (IsZoomEvent())
				{
					// Zoom in around mouse down position
					Zoom(m_MouseDownPosition, false);
					Event.current.Use();
				}
				else if (IsPanEvent())
				{
					// Pan view
					Pan();
					Event.current.Use();
				}
				break;
			case EventType.scrollWheel:
				if (!area.Contains(Event.current.mousePosition))
					break;
				if (m_IgnoreScrollWheelUntilClicked && GUIUtility.keyboardControl != id)
					break;
				// Zoom in around cursor position
				Zoom(mousePositionInDrawing, true);
				Event.current.Use();
				break;
		}

		GUILayout.EndArea();
	}
	
	public void EndViewGUI ()
	{
		if (m_MinimalGUI && Event.current.type == EventType.Repaint)
			SliderGUI();
		
	}
	
	void SliderGUI () {
		if (!m_HSlider && !m_VSlider)
			return;

		Bounds editorBounds = drawingBounds;
		Rect area = shownAreaInsideMargins;
		float min, max;
		
		float inset = styles.sliderWidth - styles.visualSliderWidth;
		float otherInset = (vSlider && hSlider) ? inset : 0;
		// Horizontal range slider
		if (m_HSlider)
		{
			Rect hRangeSliderRect = new Rect(drawRect.x + 1, drawRect.yMax - inset, drawRect.width - otherInset, styles.sliderWidth);
			float shownXRange = area.width;
			float shownXMin = area.xMin;
			EditorGUIExt.MinMaxScroller (hRangeSliderRect, horizontalScrollbarID,
				ref shownXMin, ref shownXRange,
				editorBounds.min.x, editorBounds.max.x,
				Mathf.NegativeInfinity, Mathf.Infinity,
				styles.horizontalScrollbar, styles.horizontalMinMaxScrollbarThumb,
				styles.horizontalScrollbarLeftButton, styles.horizontalScrollbarRightButton, true);
			min = shownXMin;
			max = shownXMin+shownXRange;
			if (min > area.xMin)
				min = Mathf.Min(min, max - m_HScaleMin);
			if (max < area.xMax)
				max = Mathf.Max(max, min + m_HScaleMin);
			SetShownHRangeInsideMargins(min, max);
		}
		
		// Vertical range slider
		// Reverse y values since y increses upwards for the draw area but downwards for the slider
		if (m_VSlider)
		{
			Rect vRangeSliderRect = new Rect(drawRect.xMax - inset, drawRect.y, styles.sliderWidth, drawRect.height - otherInset);
			float shownYRange = area.height;
			float shownYMin = -area.yMax;
			EditorGUIExt.MinMaxScroller (vRangeSliderRect, verticalScrollbarID, 
				ref shownYMin, ref shownYRange,
				-editorBounds.max.y, -editorBounds.min.y,
				Mathf.NegativeInfinity, Mathf.Infinity,
				styles.verticalScrollbar, styles.verticalMinMaxScrollbarThumb,
				styles.verticalScrollbarUpButton, styles.verticalScrollbarDownButton, false);
			min = -(shownYMin+shownYRange);
			max = -shownYMin;
			if (min > area.yMin)
				min = Mathf.Min(min, max - m_VScaleMin);
			if (max < area.yMax)
				max = Mathf.Max(max, min + m_VScaleMin);
			SetShownVRangeInsideMargins(min, max);
		}
	}
	
	private void Pan ()
	{
		if (!m_HRangeLocked)
			m_Translation.x += Event.current.delta.x;
		if (!m_VRangeLocked)
			m_Translation.y += Event.current.delta.y;
		
		EnforceScaleAndRange();
	}
	
	private void Zoom (Vector2 zoomAround, bool scrollwhell)
	{
		// Get delta (from scroll wheel or mouse pad)
		// Add x and y delta to cover all cases
		// (scrool view has only y or only x when shift is pressed,
		// while mouse pad has both x and y at all times)
		float delta = Event.current.delta.x + Event.current.delta.y;
		
		if (scrollwhell)
			delta = -delta;
		
		// Scale multiplier. Don't allow scale of zero or below!
		float scale = Mathf.Max(0.01F, 1 + delta * 0.01F);
		
		if (!m_HRangeLocked)
		{
			// Offset to make zoom centered around cursor position
			m_Translation.x -= zoomAround.x*(scale-1)*m_Scale.x;
			
			// Apply zooming
			m_Scale.x *= scale;
		}
		if (!m_VRangeLocked)
		{
			// Offset to make zoom centered around cursor position
			m_Translation.y -= zoomAround.y*(scale-1)*m_Scale.y;
			
			// Apply zooming
			m_Scale.y *= scale;
		}
		
		EnforceScaleAndRange();
	}
	
	private void EnforceScaleAndRange () {
		float hScaleMin = m_HScaleMin;
		float vScaleMin = m_VScaleMin;
		float hScaleMax = m_HScaleMax;
		float vScaleMax = m_VScaleMax;
		if (hRangeMax != Mathf.Infinity && hRangeMin != Mathf.NegativeInfinity)
			hScaleMax = Mathf.Min(m_HScaleMax, hRangeMax - hRangeMin);
		if (vRangeMax != Mathf.Infinity && vRangeMin != Mathf.NegativeInfinity)
			vScaleMax = Mathf.Min(m_VScaleMax, vRangeMax - vRangeMin);
		
		Rect oldArea = m_LastShownAreaInsideMargins;
		Rect newArea = shownAreaInsideMargins;
		if (newArea == oldArea)
			return;
		
		float epsilon = 0.00001f;
		
		if (newArea.width < oldArea.width - epsilon)
		{
			float xLerp = Mathf.InverseLerp(oldArea.width, newArea.width,  hScaleMin);
			newArea = new Rect(
				Mathf.Lerp(oldArea.x, newArea.x, xLerp),
				newArea.y,
				Mathf.Lerp(oldArea.width,  newArea.width,  xLerp),
				newArea.height
			);
		}
		if (newArea.height < oldArea.height - epsilon)
		{
			float yLerp = Mathf.InverseLerp(oldArea.height, newArea.height, vScaleMin);
			newArea = new Rect(
				newArea.x,
				Mathf.Lerp(oldArea.y, newArea.y, yLerp),
				newArea.width,
				Mathf.Lerp(oldArea.height, newArea.height, yLerp)
			);
		}
		if (newArea.width > oldArea.width + epsilon)
		{
			float xLerp = Mathf.InverseLerp(oldArea.width, newArea.width,  hScaleMax);
			newArea = new Rect(
				Mathf.Lerp(oldArea.x, newArea.x, xLerp),
				newArea.y,
				Mathf.Lerp(oldArea.width,  newArea.width,  xLerp),
				newArea.height
			);
		}
		if (newArea.height > oldArea.height + epsilon)
		{
			float yLerp = Mathf.InverseLerp(oldArea.height, newArea.height, vScaleMax);
			newArea = new Rect(
				newArea.x,
				Mathf.Lerp(oldArea.y, newArea.y, yLerp),
				newArea.width,
				Mathf.Lerp(oldArea.height, newArea.height, yLerp)
			);
		}
		
		// Enforce ranges
		if (newArea.xMin < hRangeMin)
			newArea.x = hRangeMin;
		if (newArea.xMax > hRangeMax)
			newArea.x = hRangeMax - newArea.width;
		if (newArea.yMin < vRangeMin)
			newArea.y = vRangeMin;
		if (newArea.yMax > vRangeMax)
			newArea.y = vRangeMax - newArea.height;
		
		shownAreaInsideMarginsInternal = newArea;
		m_LastShownAreaInsideMargins = newArea;
	}
	
	public float PixelToTime (float pixelX, Rect rect)
	{
		return ((pixelX - rect.x) * shownArea.width / rect.width + shownArea.x);
	}
	
	public float TimeToPixel (float time, Rect rect)
	{
		return (time - shownArea.x) / shownArea.width * rect.width + rect.x;
	}
	
	public float PixelDeltaToTime (Rect rect)
	{
		return shownArea.width / rect.width;
	}
}

[System.Serializable]
class TimeArea : ZoomableArea
{
	private TickHandler m_HTicks;
	public TickHandler hTicks { get { return m_HTicks; } set { m_HTicks = value; } }
	private TickHandler m_VTicks;
	public TickHandler vTicks { get { return m_VTicks; } set { m_VTicks = value; } }
		
	internal const int kTickRulerDistMin   =  3; // min distance between ruler tick marks before they disappear completely
	internal const int kTickRulerDistFull  = 80; // distance between ruler tick marks where they gain full strength
	internal const int kTickRulerDistLabel = 40; // min distance between ruler tick mark labels
	internal const float kTickRulerHeightMax    = 0.7f; // height of the ruler tick marks when they are highest
	internal const float kTickRulerFatThreshold = 0.5f; // size of ruler tick marks at which they begin getting fatter
	
	class Styles2 {
		public GUIStyle TimelineTick = "AnimationTimelineTick";
		public GUIStyle labelTickMarks = "CurveEditorLabelTickMarks";
	}
	static Styles2 styles;

	private CurveEditorSettings m_Settings = new CurveEditorSettings();
	public CurveEditorSettings settings { get { return m_Settings; } set { if (value!=null) { m_Settings = value; ApplySettings(); } } }
		
	static void InitStyles() 
	{
		if (styles == null)
			styles = new Styles2 ();	
	}
	
	public TimeArea (bool minimalGUI) : base (minimalGUI)
	{
		float[] modulos = new float[]{
			0.0000001f, 0.0000005f, 0.000001f, 0.000005f, 0.00001f, 0.00005f, 0.0001f, 0.0005f,
			0.001f, 0.005f, 0.01f, 0.05f, 0.1f, 0.5f, 1, 5, 10, 50, 100, 500,
			1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000, 10000000
		};
		hTicks = new TickHandler();
		hTicks.SetTickModulos(modulos);
		vTicks = new TickHandler();
		vTicks.SetTickModulos(modulos);
	}
		
	private void ApplySettings () {
		hRangeLocked = settings.hRangeLocked;
		vRangeLocked = settings.vRangeLocked;
		hRangeMin = settings.hRangeMin;
		hRangeMax = settings.hRangeMax;
		vRangeMin = settings.vRangeMin;
		vRangeMax = settings.vRangeMax;
		scaleWithWindow = settings.scaleWithWindow;
		hSlider = settings.hSlider;
		vSlider = settings.vSlider;
		//RecalculateBounds();
	}		
	
	void SetTickMarkerRanges ()
	{
		hTicks.SetRanges(shownArea.xMin, shownArea.xMax, drawRect.xMin, drawRect.xMax);
		vTicks.SetRanges(shownArea.yMin, shownArea.yMax, drawRect.yMin, drawRect.yMax);
	}
		
	public void DrawMajorTicks (Rect position, float frameRate)
	{
		Color backupCol = Handles.color;
		GUI.BeginGroup(position);
		if (Event.current.type != EventType.Repaint)
		{
			GUI.EndGroup();
			return;
		}
		InitStyles ();
				
		SetTickMarkerRanges ();
		hTicks.SetTickStrengths(kTickRulerDistMin, kTickRulerDistFull, true);
		
		Color tickColor = styles.TimelineTick.normal.textColor;
		tickColor.a = 0.1f;
		Handles.color = tickColor;
		// Draw tick markers of various sizes
		for (int l=0; l<hTicks.tickLevels; l++)
		{
			float strength = hTicks.GetStrengthOfLevel(l) * .9f;
			if (strength > kTickRulerFatThreshold)
			{
				float[] ticks = hTicks.GetTicksAtLevel(l, true);
				for (int i=0; i<ticks.Length; i++)
				{
					if (ticks[i] < 0) continue;
					int frame = Mathf.RoundToInt (ticks[i]*frameRate);
					float x = FrameToPixel (frame, frameRate, position);
					// Draw line
					Handles.DrawLine(new Vector3(x, 0, 0), new Vector3(x, position.height, 0));
				}		
			}
		}
		GUI.EndGroup();
		Handles.color = backupCol;			
	}
	
	public void TimeRuler (Rect position, float frameRate)
	{
		Color backupCol = GUI.color;
		GUI.BeginGroup(position);
		if (Event.current.type != EventType.Repaint)
		{
			GUI.EndGroup();
			return;
		}
		InitStyles ();
		
		HandleUtility.handleWireMaterial.SetPass (0);
		GL.Begin (GL.LINES);
		
		Color tempBackgroundColor = GUI.backgroundColor;
		
		SetTickMarkerRanges ();
		hTicks.SetTickStrengths(kTickRulerDistMin, kTickRulerDistFull, true);
		
		Color baseColor = styles.TimelineTick.normal.textColor;
		baseColor.a = 0.75f;
		
		// Draw tick markers of various sizes
		for (int l=0; l<hTicks.tickLevels; l++)
		{
			float strength = hTicks.GetStrengthOfLevel(l) * .9f;
			float[] ticks = hTicks.GetTicksAtLevel(l, true);
			for (int i=0; i<ticks.Length; i++)
			{
				if (ticks[i] < hRangeMin || ticks[i] > hRangeMax)
					continue;
				int frame = Mathf.RoundToInt (ticks[i]*frameRate);
				
				float height = position.height * Mathf.Min(1,strength) * kTickRulerHeightMax;
				float x = FrameToPixel (frame, frameRate, position);
				// Draw line
				
				GL.Color (new Color(1,1,1,strength/kTickRulerFatThreshold) * baseColor);
				GL.Vertex (new Vector3 (x, position.height-height+0.5f, 0));
				GL.Vertex (new Vector3 (x, position.height       -0.5f, 0));
				
				// Draw extra line one pixel offset to get extra thickness
				if (strength > kTickRulerFatThreshold)
				{
					GL.Color (new Color(1,1,1,strength/kTickRulerFatThreshold-1) * baseColor);
					GL.Vertex (new Vector3 (x+1, position.height-height+0.5f, 0));
					GL.Vertex (new Vector3 (x+1, position.height       -0.5f, 0));
				}
			}
		}
		
		GL.End ();
		
		// Draw tick labels
		int labelLevel = hTicks.GetLevelWithMinSeparation(kTickRulerDistLabel);
		float[] labelTicks = hTicks.GetTicksAtLevel(labelLevel, false);
		for (int i=0; i<labelTicks.Length; i++)
		{
			if (labelTicks[i] < hRangeMin || labelTicks[i] > hRangeMax)
				continue;
			int frame = Mathf.RoundToInt(labelTicks[i]*frameRate);
			// Important to take floor of positions of GUI stuff to get pixel correct alignment of
			// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
			
			float labelpos = Mathf.Floor(FrameToPixel(frame,frameRate, rect));
			string label = FormatFrame(frame, frameRate);
			GUI.Label(new Rect(labelpos+3, -3, 40, 20), label, styles.TimelineTick);
		}
		GUI.EndGroup();
		
		GUI.backgroundColor = tempBackgroundColor;
		GUI.color = backupCol;			
	}
	public enum TimeRulerDragMode
	{
		None, Start, End, Dragging, Cancel
	}
	static float s_OriginalTime;
	static float s_PickOffset;
	public TimeRulerDragMode BrowseRuler (Rect position, ref float time, float frameRate, bool pickAnywhere, GUIStyle thumbStyle)
	{
		int id = GUIUtility.GetControlID (3126789, FocusType.Passive);
		return BrowseRuler (position, id, ref time, frameRate, pickAnywhere, thumbStyle);
	}
	
	public TimeRulerDragMode BrowseRuler (Rect position, int id, ref float time, float frameRate, bool pickAnywhere, GUIStyle thumbStyle)
	{
		Event evt = Event.current;
		Rect pickRect = position;
		if (time != -1)
		{
			pickRect.x = Mathf.Round (TimeToPixel (time, position)) - thumbStyle.overflow.left;
			pickRect.width = thumbStyle.fixedWidth + thumbStyle.overflow.horizontal;
		}	
			
		switch (evt.GetTypeForControl (id))
		{
		case EventType.Repaint:
			if (time != -1)
			{
				bool hover = position.Contains (evt.mousePosition);
				pickRect.x += thumbStyle.overflow.left;
				thumbStyle.Draw (pickRect, id == GUIUtility.hotControl, hover || id == GUIUtility.hotControl, false, false);
			}
			break;
		case EventType.MouseDown:
			if (pickRect.Contains (evt.mousePosition))
			{
				GUIUtility.hotControl = id;
				s_PickOffset = evt.mousePosition.x - TimeToPixel (time, position);
				evt.Use ();
				return TimeRulerDragMode.Start;
			}
			else if (pickAnywhere && position.Contains (evt.mousePosition))
			{
				GUIUtility.hotControl = id;
				
				float newT = SnapTimeToWholeFPS (PixelToTime (evt.mousePosition.x, position), frameRate);
				s_OriginalTime = time;
				if (newT != time)
					GUI.changed = true;
				time = newT;
				s_PickOffset = 0;
				evt.Use ();
				return TimeRulerDragMode.Start;
			}
			break;
		case EventType.MouseDrag:
			if (GUIUtility.hotControl == id)
			{
				float newT = SnapTimeToWholeFPS (PixelToTime (evt.mousePosition.x - s_PickOffset, position), frameRate);
				if (newT != time)
					GUI.changed = true;
				time = newT;
					
				evt.Use ();
				return TimeRulerDragMode.Dragging;
			}
			break;
		case EventType.MouseUp:
			if (GUIUtility.hotControl == id)
			{
				GUIUtility.hotControl = 0;
				evt.Use ();
				return TimeRulerDragMode.End;
			}
			break;
		case EventType.KeyDown:
			if (GUIUtility.hotControl == id && evt.keyCode == KeyCode.Escape)
			{
				if (time != s_OriginalTime)
					GUI.changed = true;
				time = s_OriginalTime;
				
				GUIUtility.hotControl = 0;
				evt.Use ();
				return TimeRulerDragMode.Cancel;
			}
			break;
		}
		return TimeRulerDragMode.None;
	}

/*	public void GridGUI ()
	{
		GUI.BeginGroup(drawRect);
		
		if (Event.current.type != EventType.Repaint)
		{
			GUI.EndGroup();
			return;
		}
		
		InitStyles ();
		SetTickMarkerRanges();
		
		Color tempCol = GUI.color;
			
		HandleUtility.handleWireMaterial.SetPass (0);
		GL.Begin (GL.LINES);
		
		
		// Cache framed area rect as fetching the property takes some calculations
		Rect rect = shownArea;
		
		float lineStart, lineEnd;
		// Draw time markers of various strengths
		hTicks.SetTickStrengths(settings.hTickStyle.distMin, settings.hTickStyle.distFull, false);
		if (settings.hTickStyle.stubs)
		{
			lineStart = rect.yMin;
			lineEnd = rect.yMin - 40 / scale.y;
		}
		else
		{
			lineStart = Mathf.Max(rect.yMin, vRangeMin);
			lineEnd = Mathf.Min(rect.yMax, vRangeMax);
		}
		for (int l=0; l<hTicks.tickLevels; l++)
		{
			float strength = hTicks.GetStrengthOfLevel(l);
			GL.Color (settings.hTickStyle.color * new Color(1,1,1,strength) * new Color (1, 1, 1, 0.75f));
			float[] ticks = hTicks.GetTicksAtLevel(l, true);
			for (int j=0; j<ticks.Length; j++)
				if (ticks[j] > hRangeMin && ticks[j] < hRangeMax)
					DrawLine(new Vector2 (ticks[j], lineStart), new Vector2 (ticks[j], lineEnd));
		}
		// Draw bounds of allowed range
		GL.Color (settings.hTickStyle.color * new Color(1,1,1,1) * new Color (1, 1, 1, 0.75f));
		if (hRangeMin != Mathf.NegativeInfinity)
			DrawLine(new Vector2 (hRangeMin, lineStart), new Vector2 (hRangeMin, lineEnd));
		if (hRangeMax != Mathf.Infinity)
			DrawLine(new Vector2 (hRangeMax, lineStart), new Vector2 (hRangeMax, lineEnd));
		
		// Draw value markers of various strengths
		vTicks.SetTickStrengths(settings.vTickStyle.distMin, settings.vTickStyle.distFull, false);
		if (settings.vTickStyle.stubs)
		{
			lineStart = rect.xMin;
			lineEnd = rect.xMin + 40 / scale.x;
		}
		else
		{
			lineStart = Mathf.Max(rect.xMin, hRangeMin);
			lineEnd = Mathf.Min(rect.xMax, hRangeMax);
		}
		for (int l=0; l<vTicks.tickLevels; l++)
		{
			float strength = vTicks.GetStrengthOfLevel(l);
			GL.Color (settings.vTickStyle.color * new Color(1,1,1,strength) * new Color (1, 1, 1, 0.75f));
			float[] ticks = vTicks.GetTicksAtLevel(l, true);
			for (int j=0; j<ticks.Length; j++)
				if (ticks[j] > vRangeMin && ticks[j] < vRangeMax)
					DrawLine(new Vector2 (lineStart, ticks[j]), new Vector2 (lineEnd, ticks[j]));
		}
		// Draw bounds of allowed range
		GL.Color (settings.vTickStyle.color * new Color(1,1,1,1) * new Color (1, 1, 1, 0.75f));
		if (vRangeMin != Mathf.NegativeInfinity)
			DrawLine(new Vector2 (lineStart, vRangeMin), new Vector2 (lineEnd, vRangeMin));
		if (vRangeMax != Mathf.Infinity)
			DrawLine(new Vector2 (lineStart, vRangeMax), new Vector2 (lineEnd, vRangeMax));
		
		GL.End ();
		
		if (settings.hTickStyle.distLabel > 0)
		{
			// Draw time labels
			GUI.color = settings.hTickStyle.labelColor;
			int labelLevel = hTicks.GetLevelWithMinSeparation(settings.hTickStyle.distLabel);
			
			// Calculate how many decimals are needed to show the differences between the labeled ticks
			int decimals = MathUtils.GetNumberOfDecimalsForMinimumDifference(hTicks.GetPeriodOfLevel(labelLevel));
			
			// now draw
			float[] ticks = hTicks.GetTicksAtLevel(labelLevel, false);
			float vpos = Mathf.Floor(drawRect.height);
			for (int i=0; i<ticks.Length; i++)
			{
				if (ticks[i] < hRangeMin || ticks[i] > hRangeMax)
					continue;
				Vector2 pos = DrawingToViewTransformPoint(new Vector2 (ticks[i], 0));
				// Important to take floor of positions of GUI stuff to get pixel correct alignment of
				// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
				pos = new Vector2(Mathf.Floor(pos.x), vpos);
				GUI.Label(new Rect(pos.x-50+22, pos.y-16 - settings.hTickLabelOffset, 50, 16), ticks[i].ToString("n"+decimals)+settings.hTickStyle.unit, styles.labelTickMarks);
			}
		}
		
		if (settings.vTickStyle.distLabel > 0)
		{
			// Draw value labels
			GUI.color = settings.vTickStyle.labelColor;
			int labelLevel = vTicks.GetLevelWithMinSeparation(settings.vTickStyle.distLabel);
			
			float[] ticks = vTicks.GetTicksAtLevel(labelLevel, false);
			
			// Calculate how many decimals are needed to show the differences between the labeled ticks
			int decimals = MathUtils.GetNumberOfDecimalsForMinimumDifference(vTicks.GetPeriodOfLevel(labelLevel));
			
			// Calculate the size of the biggest shown label
			float labelSize = 35;
			if (!settings.vTickStyle.stubs && ticks.Length > 1)
			{
				string minNumber = ticks[1             ].ToString("n"+decimals)+settings.vTickStyle.unit;
				string maxNumber = ticks[ticks.Length-2].ToString("n"+decimals)+settings.vTickStyle.unit;
				labelSize = Mathf.Max(
					styles.labelTickMarks.CalcSize(new GUIContent(minNumber)).x,
					styles.labelTickMarks.CalcSize(new GUIContent(maxNumber)).x
				) + 6;
			}
				
			// now draw
			for (int i=0; i<ticks.Length; i++)
			{
				if (ticks[i] < vRangeMin || ticks[i] > vRangeMax)
					continue;
				Vector2 pos = DrawingToViewTransformPoint(new Vector2 (0, ticks[i]));
				// Important to take floor of positions of GUI stuff to get pixel correct alignment of
				// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
				pos = new Vector2(25, Mathf.Floor(pos.y));
				GUI.Label(new Rect(pos.x, pos.y, labelSize, 16), ticks[i].ToString("n"+decimals)+settings.vTickStyle.unit, styles.labelTickMarks);
			}
		}
		
		GUI.color = tempCol;
		
		GUI.EndGroup();
	}	
	*/

	// FIXME remove when grid drawing function has been properly rewritten
	void DrawLine (Vector2 lhs, Vector2 rhs)
	{
		GL.Vertex (DrawingToViewTransformPoint(new Vector3 (lhs.x, lhs.y, 0)));
		GL.Vertex (DrawingToViewTransformPoint(new Vector3 (rhs.x, rhs.y, 0)));
	}

	public float FrameToPixel (float i, float frameRate, Rect rect)
	{
		return (i - shownArea.xMin * frameRate) * rect.width / (shownArea.width * frameRate);
	}
	
	public string FormatFrame (int frame, float frameRate)
	{
		int frameDigits = ((int)frameRate).ToString().Length;
		string sign = string.Empty;
		if (frame < 0)
		{
			sign = "-";
			frame = -frame;
		}
		return sign + (frame / (int)frameRate).ToString() + ":" + (frame % frameRate).ToString().PadLeft(frameDigits, '0');
	}
	
	public static float SnapTimeToWholeFPS (float time, float frameRate)
	{
		if (frameRate == 0)
			return time;
		return Mathf.Round (time * frameRate) / frameRate;
	}

}

} // namespace
