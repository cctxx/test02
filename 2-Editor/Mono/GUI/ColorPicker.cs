using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor 
{

internal class ColorPicker : EditorWindow 
{
	static ColorPicker s_SharedColorPicker;
	public static string presetsEditorPrefID { get {return "Color";}}
	
	[SerializeField]
	Color m_Color = Color.black;

	[SerializeField]
	Color m_OriginalColor;

	[SerializeField]
	float m_R, m_G, m_B;
	[SerializeField]
	float m_H, m_S, m_V;
	[SerializeField]
	float m_A = 1;
	
	//1D slider stuff
	[SerializeField]
	float m_ColorSliderSize = 4; //default
	[SerializeField]
	Texture2D m_ColorSlider;
	[SerializeField]
	float m_SliderValue;

	[SerializeField]
	Color[] m_Colors;
	const int kHueRes = 64;
	const int kColorBoxSize = 8;
	[SerializeField]
	Texture2D m_ColorBox;
	static int s_Slider2Dhash = "Slider2D".GetHashCode();
	[SerializeField]
	bool m_ShowColors = true, m_ShowSliders = true, m_ShowPresets = true;
	[SerializeField]
	bool m_IsOSColorPicker = false;
	[SerializeField]
	bool m_resetKeyboardControl = false;
	[SerializeField]
	bool m_ShowAlpha = true;

	//draws RGBLayout
	Texture2D m_RTexture; float m_RTextureG = -1, m_RTextureB = -1;
	Texture2D m_GTexture; float m_GTextureR = -1, m_GTextureB = -1;
	Texture2D m_BTexture; float m_BTextureR = -1, m_BTextureG = -1;

	//draws HSVLayout
	[SerializeField]
	Texture2D m_HueTexture; float m_HueTextureS = -1, m_HueTextureV = -1;
	[SerializeField]
	Texture2D m_SatTexture; float m_SatTextureH = -1, m_SatTextureV = -1;
	[SerializeField]
	Texture2D m_ValTexture; float m_ValTextureH = -1, m_ValTextureS = -1;

	[SerializeField]
	int m_TextureColorSliderMode = -1;
	[SerializeField]
	Vector2 m_LastConstantValues = new Vector2(-1, -1);

	[System.NonSerialized]
	int m_TextureColorBoxMode = -1;
	[SerializeField]
	float m_LastConstant = -1;

	[SerializeField]
	ContainerWindow m_TrackingWindow;

	enum ColorBoxMode { SV_H, HV_S, HS_V, BG_R, BR_G, RG_B, EyeDropper }
	[SerializeField]
	ColorBoxMode m_ColorBoxMode = ColorBoxMode.BG_R, m_OldColorBoxMode;

	enum SliderMode { RGB, HSV }
	[SerializeField]
	SliderMode m_SliderMode = SliderMode.HSV;

	[SerializeField]
	Texture2D m_AlphaTexture; float m_OldAlpha = -1;

	[SerializeField]
	GUIView m_DelegateView;

	[SerializeField]
	int m_ModalUndoGroup = -1;


	PresetLibraryEditor<ColorPresetLibrary> m_ColorLibraryEditor;
	PresetLibraryEditorState m_ColorLibraryEditorState;
	bool colorChanged {get; set;}


	// Layout
	const int kEyeDropperHeight = 95;
	const int kSlidersHeight = 82;
	const int kColorBoxHeight = 162;
	const int kPresetsHeight = 300;

	public static bool visible
	{
		get { return s_SharedColorPicker != null; }
	}

	public static Color color
	{
		get { return ColorPicker.get.m_Color; }
		set { ColorPicker.get.SetColor(value); }
	}

	public static ColorPicker get
	{
		get
		{
			if (!s_SharedColorPicker)
			{
				Object[] hmm = Resources.FindObjectsOfTypeAll(typeof(ColorPicker));
				if (hmm != null && hmm.Length > 0)
					s_SharedColorPicker = (ColorPicker)hmm[0];
				if (!s_SharedColorPicker)
				{
					s_SharedColorPicker = ScriptableObject.CreateInstance<ColorPicker>();
					s_SharedColorPicker.wantsMouseMove = true;
				}
			}
			return s_SharedColorPicker;
		}
	}

	void OnSelectionChange()
	{
		m_resetKeyboardControl = true; // setting keyboardControl to 0 here won't work
		Repaint();
	}

	void RGBToHSV () 
	{
		EditorGUIUtility.RGBToHSV (new Color (m_R, m_G, m_B,1), out m_H, out m_S, out m_V);
	}

	void HSVToRGB () 
	{
		Color col = EditorGUIUtility.HSVToRGB (m_H, m_S, m_V);
		m_R = col.r;
		m_G = col.g;
		m_B = col.b;
	}

	// ------- Soerens 2D slider --------
	
	static void swap(ref float f1, ref float f2) { float tmp = f1; f1 = f2; f2 = tmp; }
	
	Vector2 Slider2D(Rect rect, Vector2 value, Vector2 maxvalue, Vector2 minvalue, GUIStyle backStyle, GUIStyle thumbStyle)
	{
		if (backStyle == null)
			return value;
		if (thumbStyle == null)
			return value;

		int id = GUIUtility.GetControlID(s_Slider2Dhash, FocusType.Native);
	         
		// test max and min
		if (maxvalue.x < minvalue.x) // swap
			swap(ref maxvalue.x,ref minvalue.x);      
		if (maxvalue.y < minvalue.y)
			swap(ref maxvalue.y,ref minvalue.y);

		// thumb
		float thumbHeight = thumbStyle.fixedHeight == 0 ? thumbStyle.padding.vertical : thumbStyle.fixedHeight;
		float thumbWidth = thumbStyle.fixedWidth == 0 ? thumbStyle.padding.horizontal : thumbStyle.fixedWidth; 
	         
		// value to px ratio vector
		Vector2 val2pxRatio =  new Vector2(
			(rect.width - (backStyle.padding.right + backStyle.padding.left) - (thumbWidth * 2) ) / (maxvalue.x - minvalue.x),
			(rect.height - (backStyle.padding.top + backStyle.padding.bottom) - (thumbHeight * 2)) / (maxvalue.y - minvalue.y));
	      
		Rect thumbRect = new Rect(
			rect.x + (value.x* val2pxRatio.x)  + (thumbWidth/2) + backStyle.padding.left - (minvalue.x * val2pxRatio.x),
			rect.y + (value.y* val2pxRatio.y) + (thumbHeight/2) + backStyle.padding.top - (minvalue.y * val2pxRatio.y),
			thumbWidth, thumbHeight);
	   
		Event e = Event.current;
	   
		switch(e.GetTypeForControl(id))
		{      
			case EventType.mouseDown:
			{               
				if (rect.Contains(e.mousePosition)) // inside this control
				{
					GUIUtility.hotControl = id;                  
					value.x = (((e.mousePosition.x - rect.x - thumbWidth) - backStyle.padding.left) / val2pxRatio.x) + (minvalue.x);
					value.y = (((e.mousePosition.y - rect.y - thumbHeight) - backStyle.padding.top) / val2pxRatio.y) + (minvalue.y);
					GUI.changed = true;
	               
					Event.current.Use ();
				}
				break;
			}
			case EventType.mouseUp:
				if (GUIUtility.hotControl == id) 
				{
					GUIUtility.hotControl = 0;
					e.Use ();
				}
				break;               
			case EventType.mouseDrag:
			{
				if (GUIUtility.hotControl != id)
					break;

				// move thumb to mouse position
                value.x = (((e.mousePosition.x - rect.x - thumbWidth) - backStyle.padding.left) / val2pxRatio.x) + (minvalue.x);
                value.y = (((e.mousePosition.y - rect.y - thumbHeight) - backStyle.padding.top) / val2pxRatio.y) + (minvalue.y);
	            
	            // clamp
	            value.x = Mathf.Clamp (value.x, minvalue.x, maxvalue.x);
	            value.y = Mathf.Clamp (value.y, minvalue.y, maxvalue.y);
	            GUI.changed = true;
	            Event.current.Use ();
	         }
	         break;
	        
			case EventType.repaint:
			{            
				// background            
				backStyle.Draw(rect,GUIContent.none, id);
				// thumb
				thumbStyle.Draw(thumbRect,GUIContent.none, id);
			}
			break;
		}               
		return value;         
	}
		
	//draws RGBLayout
	void RGBSliders () 
	{
		bool temp = GUI.changed;
		GUI.changed = false;
		/// Update the RGB slider textures
		m_RTexture = Update1DSlider (m_RTexture, kColorBoxSize, m_G, m_B, ref m_RTextureG, ref m_RTextureB, 0, false);
		m_GTexture = Update1DSlider (m_GTexture, kColorBoxSize, m_R, m_B, ref m_GTextureR, ref m_GTextureB, 1, false);
		m_BTexture = Update1DSlider (m_BTexture, kColorBoxSize, m_R, m_G, ref m_BTextureR, ref m_BTextureG, 2, false);

		float r = ((int)Mathf.Round (m_R * 255f));
		float g = ((int)Mathf.Round (m_G * 255f));
		float b = ((int)Mathf.Round (m_B * 255f));
		r = TexturedSlider(m_RTexture, "R", r, 0, 255);
		g = TexturedSlider(m_GTexture, "G", g, 0, 255);
		b = TexturedSlider(m_BTexture, "B", b, 0, 255);
		
		if (GUI.changed) 
		{
			m_R = r / 255f;
			m_G = g / 255f;
			m_B = b / 255f;
			RGBToHSV ();
		}
		
		GUI.changed |= temp;
	}
	
	
	static Texture2D Update1DSlider (Texture2D tex, int xSize, float const1, float const2, ref float oldConst1, 
						     ref float oldConst2, int idx, bool hsvSpace) 
	{
		if (!tex || const1 != oldConst1 || const2 != oldConst2) 
		{
			if (!tex) 
				tex = MakeTexture (xSize, 2);

			Color[] colors = new Color[xSize * 2];
			Color start = Color.black, step = Color.black;
			switch (idx) 
			{
			case 0:
				start = new Color (0, const1, const2, 1);
				step = new Color (1,0,0,0);
				break;
			case 1:
				start = new Color (const1, 0, const2, 1);
				step = new Color (0,1,0,0);
				break;
			case 2:
				start = new Color (const1, const2, 0, 1);
				step = new Color (0,0,1,0);
				break;
			case 3:
				start = new Color (0,0, 0, 1);
				step = new Color (1,1,1,0);
				break;
			}
			FillArea (xSize, 2, colors, start, step, new Color (0,0,0,0));
			if (hsvSpace)
				HSVToRGBArray (colors);

			oldConst1 = const1;
			oldConst2 = const2;
			tex.SetPixels (colors);
			tex.Apply ();
		}
		return tex;
	}
	
	float TexturedSlider (Texture2D background, string text, float val, float min, float max) 
	{
		Rect r = GUILayoutUtility.GetRect (16,16, GUI.skin.label);

		GUI.Label (new Rect (r.x, r.y-1, 20, 16), text);
		r.x += 14;
		r.width -= 50;
		if (Event.current.type == EventType.repaint)
		{
			Rect r2 = new Rect (r.x + 1, r.y + 2, r.width - 2, r.height - 4);
			Graphics.DrawTexture (r2, background, new Rect (.5f / background.width, .5f / background.height, 1 - 1f / background.width, 1 - 1f / background.height), 0, 0, 0, 0, Color.grey);
		}
		int id = EditorGUIUtility.GetControlID (869045, EditorGUIUtility.native, position);
		bool temp = GUI.changed;
		GUI.changed = false;
		val = GUI.HorizontalSlider (new Rect (r.x, r.y+1, r.width, r.height-2), val, min, max, styles.pickerBox, styles.thumbHoriz);

		if (GUI.changed && EditorGUI.s_RecycledEditor.IsEditingControl (id)) 
			EditorGUI.s_RecycledEditor.EndEditing ();

		Rect r3 = new Rect (r.xMax + 6, r.y, 30, 16);
		val = (int)EditorGUI.DoFloatField (EditorGUI.s_RecycledEditor, r3, new Rect (0,0,0,0), id, val, EditorGUI.kIntFieldFormatString, EditorStyles.numberField, false);
		val = Mathf.Clamp(val, min, max);

		GUI.changed |= temp;
		return val;
	}
	
	//draws HSVLayout
	void HSVSliders() 
	{
		bool temp = GUI.changed;
		GUI.changed = false;

		/// Update the HSV slider textures
		m_HueTexture = Update1DSlider (m_HueTexture, kHueRes, 1, 1, ref m_HueTextureS, ref m_HueTextureV, 0, true);
		m_SatTexture = Update1DSlider (m_SatTexture, kColorBoxSize, m_H, Mathf.Max (m_V, .2f), ref m_SatTextureH, ref m_SatTextureV, 1, true);
		m_ValTexture = Update1DSlider (m_ValTexture, kColorBoxSize, m_H, m_S, ref m_ValTextureH, ref m_ValTextureS, 2, true);

		float h = ((int)Mathf.Round (m_H * 359f));
		float s = ((int)Mathf.Round (m_S * 255f));
		float v = ((int)Mathf.Round (m_V * 255f));

		h = TexturedSlider(m_HueTexture, "H", h, 0, 359);
		s = TexturedSlider(m_SatTexture, "S", s, 0, 255);
		v = TexturedSlider(m_ValTexture, "V", v, 0, 255);

		if (GUI.changed) 
		{
			m_H = h / 359.0f;
			m_S = s / 255.0f;
			m_V = v / 255.0f;
			HSVToRGB ();
		}

		GUI.changed |= temp;
	}

	static void FillArea (int xSize, int ySize, Color[] retval, Color topLeftColor, Color rightGradient, Color downGradient) 
	{
		// Calc the deltas for stepping.
		Color rightDelta = new Color (0,0,0,0), downDelta  = new Color (0,0,0,0);
		if (xSize > 1)
			rightDelta = rightGradient / (xSize -1);
		if (ySize > 1)
			downDelta = downGradient / (ySize - 1);

		// Assign all colors into the array
		Color p = topLeftColor;
		int current = 0;
		for (int y = 0; y < ySize; y++) 
		{
			Color p2 = p;
			for (int x = 0; x < xSize; x++) 
			{
				retval[current++] = p2;
				p2 += rightDelta;
			}
			p += downDelta;
		}
/*		for (int y = 0; y < ySize; y++) 
			retval[y * xSize + xSize - 1] = retval[y * xSize] = Color.white;
		for (int x = 0; x < xSize; x++) 
			retval[(ySize - 1) * xSize + x] = retval[x] = Color.white;
*/	}
		
	static void HSVToRGBArray (Color[] colors) 
	{
		int s = colors.Length;
		for (int i = 0; i < s; i++) 
		{
			Color c = colors[i];
			Color c2 = EditorGUIUtility.HSVToRGB (c.r, c.g, c.b);
			c2.a = c.a;
			colors[i] = c2;
		}
	}
				
	void DrawColorSlider (Rect colorSliderRect, Vector2 constantValues) 
	{
		if (Event.current.type != EventType.repaint)
			return;

		// If we've switched mode, regenerate box
		if ((int)m_ColorBoxMode != m_TextureColorSliderMode) 
		{
			int newXSize = 0, newYSize = 0;
			newXSize = (int)m_ColorSliderSize;

			// it might want a new size
			if (m_ColorBoxMode  == ColorBoxMode.SV_H) 
				newYSize = (int)kHueRes;
			else
				newYSize = (int)m_ColorSliderSize;

			if (m_ColorSlider == null)
				m_ColorSlider = MakeTexture(newXSize, newYSize);

			if (m_ColorSlider.width != newXSize || m_ColorSlider.height != newYSize)
				m_ColorSlider.Resize(newXSize, newYSize);
		}

		if ((int)m_ColorBoxMode != m_TextureColorSliderMode || constantValues != m_LastConstantValues) 
		{
			Color[] sliderColors = m_ColorSlider.GetPixels(0);
						
			//last color drawing possibility
			int xSize = m_ColorSlider.width, ySize = m_ColorSlider.height; 
			switch(m_ColorBoxMode) 
			{
			case ColorBoxMode.SV_H:
				FillArea (xSize, ySize, sliderColors, new Color (0, 1, 1, 1), new Color (0,0,0,0), new Color (1,0,0,0));
				HSVToRGBArray (sliderColors);
				break;
			case ColorBoxMode.HV_S:
				FillArea (xSize, ySize, sliderColors, new Color (m_H, 0, Mathf.Max (m_V, .30f), 1), new Color (0,0,0,0), new Color (0,1,0,0));
				HSVToRGBArray (sliderColors);
				break;
			case ColorBoxMode.HS_V:
				FillArea (xSize, ySize, sliderColors, new Color (m_H, m_S, 0, 1), new Color (0,0,0,0), new Color (0,0,1,0));
				HSVToRGBArray (sliderColors);
				break;
			case ColorBoxMode.BG_R:
				FillArea (xSize, ySize, sliderColors, new Color (0, m_G, m_B, 1), new Color (0,0,0,0), new Color (1,0,0,0));
				break;
			case ColorBoxMode.BR_G:
				FillArea (xSize, ySize, sliderColors, new Color (m_R, 0, m_B, 1), new Color (0,0,0,0), new Color (0,1,0,0));
				break;
			case ColorBoxMode.RG_B:
				FillArea (xSize, ySize, sliderColors, new Color (m_R, m_G, 0, 1), new Color (0,0,0,0), new Color (0,0,1,0));
				break;
			}
			m_ColorSlider.SetPixels(sliderColors, 0);
			m_ColorSlider.Apply(true);
		}
		Graphics.DrawTexture(colorSliderRect, m_ColorSlider, new Rect(.5f / m_ColorSlider.width, .5f / m_ColorSlider.height, 1 - 1f / m_ColorSlider.width, 1 - 1f / m_ColorSlider.height), 0, 0, 0, 0, Color.grey);
//		Graphics.DrawTexture (colorSliderRect, m_ColorSlider, sourceRect, 0, 0, 0, 0, Color.grey);
	}
//	Rect sourceRect = new Rect (0,0,1,1);
	
	public static Texture2D MakeTexture (int width, int height) 
	{
		Texture2D tex = new Texture2D (width, height, TextureFormat.ARGB32, false);
        tex.hideFlags = HideFlags.HideAndDontSave;
		tex.wrapMode = TextureWrapMode.Clamp;
//		tex.filterMode = FilterMode.Point;
		tex.hideFlags = HideFlags.DontSave;
		return tex;
	}
	
	void DrawColorSpaceBox (Rect colorBoxRect, float constantValue) 
	{
		if (Event.current.type != EventType.Repaint)
			return;

		// If we've switched mode, regenerate box
		if ((int)m_ColorBoxMode != m_TextureColorBoxMode) 
		{
			int newXSize = 0, newYSize = 0;
			newYSize = (int)kColorBoxSize;

			// it might want a new size
			if (m_ColorBoxMode  == ColorBoxMode.HV_S || m_ColorBoxMode  == ColorBoxMode.HS_V) 
				newXSize = (int)kHueRes;
			else
				newXSize = (int)kColorBoxSize;

			if(m_ColorBox == null)
				m_ColorBox = MakeTexture (newXSize, newYSize);
			
			if (m_ColorBox.width != newXSize || m_ColorBox.height != newYSize)
				m_ColorBox.Resize (newXSize, newYSize);
		}

		if ((int)m_ColorBoxMode != m_TextureColorBoxMode || m_LastConstant != constantValue) 
		{
			m_Colors = m_ColorBox.GetPixels(0);
			
			int xSize = m_ColorBox.width;
			int ySize = m_ColorBox.height;
			switch(m_ColorBoxMode) 
			{
			case ColorBoxMode.BG_R:
				FillArea (xSize, ySize, m_Colors, new Color (m_R, 0, 0, 1), new Color (0,0,1,0), new Color (0,1,0,0));
				break;
			case ColorBoxMode.BR_G:
				FillArea(xSize, ySize, m_Colors, new Color(0, m_G, 0, 1), new Color(0, 0, 1, 0), new Color(1, 0, 0, 0));
				break;
			case ColorBoxMode.RG_B:
				FillArea(xSize, ySize, m_Colors, new Color(0, 0, m_B, 1), new Color(1, 0, 0, 0), new Color(0, 1, 0, 0));
				break;
			case ColorBoxMode.SV_H:
				FillArea(xSize, ySize, m_Colors, new Color(m_H, 0, 0, 1), new Color(0, 1, 0, 0), new Color(0, 0, 1, 0));
				HSVToRGBArray(m_Colors);
				break;
			case ColorBoxMode.HV_S:
				FillArea(xSize, ySize, m_Colors, new Color(0, m_S, 0, 1), new Color(1, 0, 0, 0), new Color(0, 0, 1, 0));
				HSVToRGBArray(m_Colors);
				break;
			case ColorBoxMode.HS_V:
				FillArea(xSize, ySize, m_Colors, new Color(0, 0, m_V, 1), new Color(1, 0, 0, 0), new Color(0, 1, 0, 0));
				HSVToRGBArray(m_Colors);
				break;
			}
			m_ColorBox.SetPixels(m_Colors, 0);
			m_ColorBox.Apply(true);
			m_LastConstant = constantValue;
			m_TextureColorBoxMode = (int)m_ColorBoxMode;
		}
		Graphics.DrawTexture (colorBoxRect, m_ColorBox, new Rect (.5f / m_ColorBox.width, .5f / m_ColorBox.height, 1 - 1f / m_ColorBox.width, 1 - 1f / m_ColorBox.height), 0, 0, 0, 0, Color.grey);
	}

	class Styles 
	{
		public GUIStyle pickerBox = "ColorPickerBox";
		public GUIStyle thumb2D= "ColorPicker2DThumb";
		public GUIStyle thumbHoriz = "ColorPickerHorizThumb";
		public GUIStyle thumbVert = "ColorPickerVertThumb";
		public GUIStyle headerLine = "IN Title";
		public GUIStyle colorPickerBox = "ColorPickerBox";
		public GUIStyle background = "ColorPickerBackground";
		public GUIContent eyeDropper = EditorGUIUtility.IconContent ("EyeDropper.Large");
		public GUIContent colorCycle = EditorGUIUtility.IconContent ("ColorPicker.CycleColor");
		public GUIContent colorToggle = EditorGUIUtility.TextContent ("ColorPicker.ColorFoldout");
		public GUIContent sliderToggle = EditorGUIUtility.TextContent ("ColorPicker.SliderFoldout");
		public GUIContent presetsToggle = new GUIContent("Presets");
		public GUIContent sliderCycle = EditorGUIUtility.IconContent ("ColorPicker.CycleSlider");
	}
	static Styles styles;

	public string currentPresetLibrary
	{
		get 
		{
			InitIfNeeded ();
			return m_ColorLibraryEditor.currentLibraryWithoutExtension; 
		}
		set 
		{
			InitIfNeeded ();
			m_ColorLibraryEditor.currentLibraryWithoutExtension = value; 
		}
	}

	void InitIfNeeded ()
	{
		if (styles == null)
			styles = new Styles();
	
		if (m_ColorLibraryEditorState == null)
		{
			m_ColorLibraryEditorState = new PresetLibraryEditorState(presetsEditorPrefID);
			m_ColorLibraryEditorState.TransferEditorPrefsState(true);
		}

		if (m_ColorLibraryEditor == null)
		{
			var saveLoadHelper = new ScriptableObjectSaveLoadHelper<ColorPresetLibrary> ("colors", SaveType.Text);
			m_ColorLibraryEditor = new PresetLibraryEditor<ColorPresetLibrary>(saveLoadHelper, m_ColorLibraryEditorState, PresetClickedCallback);
			m_ColorLibraryEditor.previewAspect = 1f;
			m_ColorLibraryEditor.minMaxPreviewHeight = new Vector2(ColorPresetLibrary.kSwatchSize, ColorPresetLibrary.kSwatchSize);
			m_ColorLibraryEditor.settingsMenuRightMargin = 2f;
			m_ColorLibraryEditor.useOnePixelOverlappedGrid = true;
			m_ColorLibraryEditor.alwaysShowScrollAreaHorizontalLines = false;
			m_ColorLibraryEditor.marginsForGrid = new RectOffset (0,0,0,0);
			m_ColorLibraryEditor.marginsForList = new RectOffset (0,5,2,2);
		}
	}

	void PresetClickedCallback(int clickCount, object presetObject)
	{
		Color color = (Color)presetObject;
		SetColor (color);
		colorChanged = true;
	}

	void DoColorSwatchAndEyedropper ()
	{
		GUILayout.BeginHorizontal();
		if (GUILayout.Button(styles.eyeDropper, GUIStyle.none, GUILayout.Width(40), GUILayout.ExpandWidth(false)))
		{
			EyeDropper.Start(m_Parent);
			m_ColorBoxMode = ColorBoxMode.EyeDropper;
			GUIUtility.ExitGUI();
		}
		Color c = new Color(m_R, m_G, m_B, m_A);
		Rect r = GUILayoutUtility.GetRect(20, 20, 20, 20, styles.colorPickerBox, GUILayout.ExpandWidth(true));
		EditorGUIUtility.DrawColorSwatch(r, c, m_ShowAlpha);
		// Draw eyedropper icon
		if (Event.current.type == EventType.Repaint)
			styles.pickerBox.Draw(r, GUIContent.none, false, false, false, false);
		GUILayout.EndHorizontal();
	}

	void DoColorSpaceGUI ()
	{
		GUILayout.BeginHorizontal();
		m_ShowColors = GUILayout.Toggle(m_ShowColors, styles.colorToggle, EditorStyles.foldout);

		GUI.enabled = m_ShowColors;

		if (GUILayout.Button(styles.colorCycle, GUIStyle.none, GUILayout.ExpandWidth(false)))
			m_OldColorBoxMode = m_ColorBoxMode = (ColorBoxMode)(((int)m_ColorBoxMode + 1) % 6);

		GUI.enabled = true;
		GUILayout.EndHorizontal();

		if (m_ShowColors)
		{
			bool temp = GUI.changed;
			Rect colorBoxRect, colorSliderBoxRect;
			GUILayout.BeginHorizontal(GUILayout.ExpandHeight(false));
			colorBoxRect = GUILayoutUtility.GetAspectRect(1, styles.pickerBox, GUILayout.MinWidth(64), GUILayout.MinHeight(64), GUILayout.MaxWidth(256), GUILayout.MaxHeight(256));
			EditorGUILayout.Space();
			colorSliderBoxRect = GUILayoutUtility.GetRect(8, 32, 64, 128, styles.pickerBox);
			colorSliderBoxRect.height = colorBoxRect.height;
			GUILayout.EndHorizontal();

			GUI.changed = false;
			switch (m_ColorBoxMode)
			{
				case ColorBoxMode.SV_H:
					Slider3D(colorBoxRect, colorSliderBoxRect, ref m_S, ref m_V, ref m_H, styles.pickerBox, styles.thumb2D, styles.thumbVert);
					if (GUI.changed)
						HSVToRGB();
					break;
				case ColorBoxMode.HV_S:
					Slider3D(colorBoxRect, colorSliderBoxRect, ref m_H, ref m_V, ref m_S, styles.pickerBox, styles.thumb2D, styles.thumbVert);
					if (GUI.changed)
						HSVToRGB();
					break;
				case ColorBoxMode.HS_V:
					Slider3D(colorBoxRect, colorSliderBoxRect, ref m_H, ref m_S, ref m_V, styles.pickerBox, styles.thumb2D, styles.thumbVert);
					if (GUI.changed)
						HSVToRGB();
					break;
				case ColorBoxMode.BG_R:
					Slider3D(colorBoxRect, colorSliderBoxRect, ref m_B, ref m_G, ref m_R, styles.pickerBox, styles.thumb2D, styles.thumbVert);
					if (GUI.changed)
						RGBToHSV();
					break;
				case ColorBoxMode.BR_G:
					Slider3D(colorBoxRect, colorSliderBoxRect, ref m_B, ref m_R, ref m_G, styles.pickerBox, styles.thumb2D, styles.thumbVert);
					if (GUI.changed)
						RGBToHSV();
					break;
				case ColorBoxMode.RG_B:
					Slider3D(colorBoxRect, colorSliderBoxRect, ref m_R, ref m_G, ref m_B, styles.pickerBox, styles.thumb2D, styles.thumbVert);
					if (GUI.changed)
						RGBToHSV();
					break;
				case ColorBoxMode.EyeDropper:
					EyeDropper.DrawPreview(Rect.MinMaxRect(colorBoxRect.x, colorBoxRect.y, colorSliderBoxRect.xMax, colorBoxRect.yMax));
					break;
			}
			
			GUI.changed |= temp;
		}
	}


	void DoColorSliders ()
	{
		GUILayout.BeginHorizontal();
		m_ShowSliders = GUILayout.Toggle(m_ShowSliders, styles.sliderToggle, EditorStyles.foldout);

		GUI.enabled = m_ShowSliders;
		if (GUILayout.Button(styles.sliderCycle, GUIStyle.none, GUILayout.ExpandWidth(false)))
		{
			m_SliderMode = (SliderMode)(((int)m_SliderMode + 1) % 2);
			GUI.changed = true;
		}
		GUI.enabled = true;
		GUILayout.EndHorizontal();

		if (m_ShowSliders)
		{
			switch (m_SliderMode)
			{
				case SliderMode.HSV:
					HSVSliders();
					break;
				case SliderMode.RGB:
					RGBSliders();
					break;
			}

			// Update the HSV slider textures
			if (m_ShowAlpha)
			{
				m_AlphaTexture = Update1DSlider(m_AlphaTexture, kColorBoxSize, 0, 0, ref m_OldAlpha, ref m_OldAlpha, 3, false);
				m_A = TexturedSlider(m_AlphaTexture, "A", Mathf.Round(m_A * 255), 0, 255) / 255;
			}
			
		}
	}

	void DoPresetsGUI ()
	{
		GUILayout.BeginHorizontal();
		EditorGUI.BeginChangeCheck();
			m_ShowPresets = GUILayout.Toggle(m_ShowPresets, styles.presetsToggle, EditorStyles.foldout);
		if (EditorGUI.EndChangeCheck())
			EditorPrefs.SetInt("CPPresetsShow", m_ShowPresets ? 1 : 0);
		GUILayout.Space(17f); // Make room for presets settings menu button
		GUILayout.EndHorizontal();

		if (m_ShowPresets)
		{
			GUILayout.Space(-18f); // pull up to reuse space
			Rect presetsRect = GUILayoutUtility.GetRect(0, Mathf.Clamp(m_ColorLibraryEditor.contentHeight, 40f, 250f));
			m_ColorLibraryEditor.OnGUI(presetsRect, m_Color);
		}
	}

	void OnGUI ()
	{
		InitIfNeeded ();
			
		if (m_resetKeyboardControl)
		{
			GUIUtility.keyboardControl = 0;
			m_resetKeyboardControl = false;
		}

		EventType type = Event.current.type;
		
		if (type == EventType.ExecuteCommand) 
		{
			switch (Event.current.commandName) 
			{
			case "EyeDropperUpdate":
				Repaint ();
				break;
			case "EyeDropperClicked":
				Color col = EyeDropper.GetLastPickedColor ();
				m_R = col.r;
				m_G = col.g;
				m_B = col.b;
				RGBToHSV ();
				m_ColorBoxMode = m_OldColorBoxMode;
				m_Color = new Color (m_R, m_G, m_B, m_A);
				SendEvent (true);
				break;
			case "EyeDropperCancelled":
				Repaint ();
				m_ColorBoxMode = m_OldColorBoxMode;
				break;
			}
		}
		
		EditorGUIUtility.labelWidth = 15;
		EditorGUIUtility.fieldWidth = 30;

		Rect contentRect = EditorGUILayout.BeginVertical (styles.background);

		// This section of GUI uses GUI.changed to check if color has changed :(
		EditorGUI.BeginChangeCheck ();
		DoColorSwatchAndEyedropper ();
		GUILayout.Space (10);
		DoColorSpaceGUI ();
		GUILayout.Space(10);
		DoColorSliders ();
		GUILayout.Space(10);
		if (EditorGUI.EndChangeCheck())
			colorChanged = true;
		// We leave presets GUI out of the change check because it has a scrollview that will 
		// set changed=true when used and we do not want to send color changed events when scrolling
		DoPresetsGUI (); 
		GUILayout.Space (10);

		if (colorChanged) 
		{
			EditorPrefs.SetInt ("CPSliderShow" , m_ShowSliders ? 1 : 0);
			EditorPrefs.SetInt ("CPSliderMode" , (int)m_SliderMode);
			EditorPrefs.SetInt ("CPColorShow" , m_ShowColors ? 1 : 0);
			EditorPrefs.SetInt ("CPColorMode" , (int)m_ColorBoxMode);
		}

		if (colorChanged) 
		{
			colorChanged = false;
			m_Color = new Color (m_R, m_G, m_B, m_A);
			//only register undo once per mouse click.
			SendEvent (true);
		}

		EditorGUILayout.EndVertical ();

		if (contentRect.height > 0)
			SetHeight(contentRect.height);

		if (Event.current.type == EventType.keyDown)
		{ 
			switch (Event.current.keyCode)
			{
			case KeyCode.Escape:
				Undo.RevertAllDownToGroup (m_ModalUndoGroup);
				m_Color = m_OriginalColor;
				SendEvent (false);
				Close ();
				GUIUtility.ExitGUI();
				break;
			case KeyCode.Return:
			case KeyCode.KeypadEnter:
				Close ();
				break;
			}
		}
		
	}

	void SetHeight (float newHeight) 
	{
		if (newHeight == position.height)
			return;
		minSize = new Vector2 (193, newHeight);
		maxSize = new Vector2 (193, newHeight);
	}
	
	void Slider3D (Rect boxPos, Rect sliderPos, ref float x, ref float y, ref float z, GUIStyle box, GUIStyle thumb2D, GUIStyle thumbHoriz) 
	{
		Rect r = boxPos;
		r.x += 1;
		r.y += 1;
		r.width -= 2;
		r.height -= 2;
		DrawColorSpaceBox (r, z);
		
		Vector2 xy = new Vector2 (x, 1 - y);
		xy = Slider2D (boxPos, xy, new Vector2 (0,0), new Vector2 (1, 1), box, thumb2D);
		x = xy.x; 
		y = 1 - xy.y;

		Rect r2 = new Rect (sliderPos.x + 1, sliderPos.y + 1, sliderPos.width - 2, sliderPos.height - 2);
		DrawColorSlider (r2, new Vector2 (x,y));

		z = GUI.VerticalSlider (sliderPos, z, 1, 0, box, thumbHoriz);
	}
	
	void SendEvent (bool exitGUI) 
	{
		if (m_DelegateView) 
		{
			Event e = EditorGUIUtility.CommandEvent ("ColorPickerChanged");
			if(!m_IsOSColorPicker)
				Repaint ();
			m_DelegateView.SendEvent (e);
			if(!m_IsOSColorPicker && exitGUI)
				GUIUtility.ExitGUI ();
		}
	}

	public void SetColor (Color c)
	{
		if(m_IsOSColorPicker)
			OSColorPicker.color = c;
		else
		{
			if (m_Color.r == c.r && m_Color.g == c.g && m_Color.b == c.b && m_Color.a == c.a)
				return;
		
            m_resetKeyboardControl = true;
			m_Color = c;
			m_R = c.r;
			m_G = c.g;
			m_B = c.b;
			RGBToHSV ();
			m_A = c.a;
			Repaint ();
		}
	}

	public static void Show (GUIView viewToUpdate, Color col)
	{
		Show (viewToUpdate, col, true); 
	}
	public static void Show (GUIView viewToUpdate, Color col, bool showAlpha) 
	{
		ColorPicker.get.m_DelegateView = viewToUpdate;
		ColorPicker.color = col;
		ColorPicker.get.m_OriginalColor = col;
		ColorPicker.get.m_ShowAlpha = showAlpha;
		ColorPicker.get.m_ModalUndoGroup = Undo.GetCurrentGroup();
		if(ColorPicker.get.m_IsOSColorPicker)
			OSColorPicker.Show (showAlpha);
		else 
		{
			ColorPicker cp = ColorPicker.get;
			cp.title = "Color";
			float width = EditorPrefs.GetInt("CPickerWidth", (int)cp.position.width);
			float height = EditorPrefs.GetInt("CPickerHeight", (int)cp.position.height);
			cp.minSize = new Vector2(width, height);
			cp.maxSize = new Vector2(width, height);
			cp.ShowAuxWindow();
		}
	}

	void PollOSColorPicker()
	{
		if(m_IsOSColorPicker)
		{
			if(!OSColorPicker.visible || Application.platform != RuntimePlatform.OSXEditor)
			{
				DestroyImmediate(this);				
			}
			else
			{
				Color c = OSColorPicker.color;
				if(m_Color != c)
				{
					m_Color = c;
					SendEvent(true);
				}
			}
		}
	}
	
	public ColorPicker ()
	{
		hideFlags = HideFlags.DontSave;
		m_ShowSliders = EditorPrefs.GetInt ("CPSliderShow" , 1) != 0;
		m_SliderMode = (SliderMode)EditorPrefs.GetInt ("CPSliderMode" , (int)SliderMode.RGB);
		m_ShowColors = EditorPrefs.GetInt ("CPColorShow" , 1) != 0;
		m_ColorBoxMode = (ColorBoxMode)EditorPrefs.GetInt ("CPColorMode" , (int)ColorBoxMode.SV_H);
		m_IsOSColorPicker = EditorPrefs.GetBool ("UseOSColorPicker");
		m_ShowPresets = EditorPrefs.GetInt ("CPPresetsShow" , 1) != 0;
   		EditorApplication.update += PollOSColorPicker;
		EditorGUIUtility.editingTextField = true; // To fix that color values is not directly editable when tabbing (case 557510)
	}
	
	public void OnDestroy ()
	{
		Undo.CollapseUndoOperations (m_ModalUndoGroup);
	
		if (m_ColorSlider)
            Object.DestroyImmediate (m_ColorSlider);
        if (m_ColorBox)
            Object.DestroyImmediate (m_ColorBox);
        if (m_RTexture)
            Object.DestroyImmediate (m_RTexture);
        if (m_GTexture)
            Object.DestroyImmediate (m_GTexture);
        if (m_BTexture)
            Object.DestroyImmediate (m_BTexture);
        if (m_HueTexture)
            Object.DestroyImmediate (m_HueTexture);
        if (m_SatTexture)
            Object.DestroyImmediate (m_SatTexture);
        if (m_ValTexture)
            Object.DestroyImmediate (m_ValTexture);
        if (m_AlphaTexture)
            Object.DestroyImmediate (m_AlphaTexture);
        s_SharedColorPicker = null;
		if(m_IsOSColorPicker)
			OSColorPicker.Close();

   		EditorApplication.update -= PollOSColorPicker;

		if (m_ColorLibraryEditorState != null)
			m_ColorLibraryEditorState.TransferEditorPrefsState(false);

		if (m_ColorLibraryEditor != null)
			m_ColorLibraryEditor.UnloadUsedLibraries();

		EditorPrefs.SetInt("CPickerWidth", (int)position.width);
		EditorPrefs.SetInt("CPickerHeight", (int)position.height);

	}
}

internal class EyeDropper : GUIView 
{
	const int kPixelSize = 10;
	const int kDummyWindowSize = 10000;
	static internal Color s_LastPickedColor;
	GUIView m_DelegateView;
	Texture2D m_Preview;
	static EyeDropper s_Instance;
	private static Vector2 s_PickCoordinates = Vector2.zero;

	public static void Start (GUIView viewToUpdate) 
	{
		get.Show (viewToUpdate);
	} 

	static EyeDropper get 
	{ 
		get 
		{
			if (!s_Instance)
				ScriptableObject.CreateInstance<EyeDropper>();
			return s_Instance;
		}
	}

	EyeDropper () 
	{
		s_Instance = this;
	}
	
	void Show (GUIView sourceView) 
	{
		m_DelegateView = sourceView;
        ContainerWindow win = ScriptableObject.CreateInstance<ContainerWindow>();
		win.m_DontSaveToLayout = true;
		win.title = "EyeDropper";
		win.hideFlags = HideFlags.DontSave;
		win.mainView = this;
		win.Show(ShowMode.PopupMenu, true, false);
		AddToAuxWindowList ();
		win.SetInvisible();
		SetMinMaxSizes(new Vector2(0, 0), new Vector2(kDummyWindowSize, kDummyWindowSize));
		win.position = new Rect(-kDummyWindowSize/2, -kDummyWindowSize/2, kDummyWindowSize, kDummyWindowSize);
		wantsMouseMove = true;
		StealMouseCapture ();
	}

	public static Color GetPickedColor () 
	{
		return InternalEditorUtility.ReadScreenPixel (s_PickCoordinates, 1,1)[0];
	}

	public static Color GetLastPickedColor () 
	{
		return s_LastPickedColor;
	}

	class Styles 
	{
		public GUIStyle eyeDropperHorizontalLine = "EyeDropperHorizontalLine";
		public GUIStyle eyeDropperVerticalLine = "EyeDropperVerticalLine";
		public GUIStyle eyeDropperPickedPixel = "EyeDropperPickedPixel";
	}
	static Styles styles;

	public static void DrawPreview (Rect position) 
	{
		if (Event.current.type != EventType.Repaint)
			return;
	
		if (styles == null)
			styles = new Styles();
	
		Texture2D preview = get.m_Preview;
		int width = (int)Mathf.Ceil (position.width / kPixelSize);
		int height = (int)Mathf.Ceil (position.height / kPixelSize);
		if (preview == null) 
		{
			get.m_Preview = preview = ColorPicker.MakeTexture (width, height);
			preview.filterMode = FilterMode.Point;
		}
		if (preview.width != width || preview.height != height) 
		{
			preview.Resize ((int)width, (int)height);
		}

		Vector2 p = GUIUtility.GUIToScreenPoint (Event.current.mousePosition);
		Vector2 mPos = p - new Vector2 ((int)(width /2), (int)(height / 2));
		preview.SetPixels (InternalEditorUtility.ReadScreenPixel (mPos, width, height), 0);
		preview.Apply (true);

		Graphics.DrawTexture (position, preview);
		
		// Draw grid on top
		float xStep = position.width / width;
		GUIStyle sep = styles.eyeDropperVerticalLine;
		for (float x = position.x; x < position.xMax; x+= xStep) 
		{
			Rect r = new Rect (Mathf.Round (x), position.y, xStep, position.height);
			sep.Draw (r, false, false, false, false);
		}

		float yStep = position.height / height;
		sep = styles.eyeDropperHorizontalLine;
		for (float y = position.y; y < position.yMax; y+= yStep) 
		{
			Rect r = new Rect (position.x, Mathf.Floor (y), position.width, yStep);
			sep.Draw (r, false, false, false, false);		
		}
		
		// Draw selected pixelSize
		Rect newR = new Rect ((p.x - mPos.x) * xStep + position.x, (p.y - mPos.y) * yStep + position.y, xStep, yStep);
		styles.eyeDropperPickedPixel.Draw (newR, false, false, false, false);
	}
	
	void OnGUI () 
	{
		// On mouse move/click we remember screen coordinates where we are. Then we'll use that
		// in GetPickedColor to read. The reason is that because GetPickedColor might be called from
		// an event which is different, so the coordinates would be already wrong.
		switch (Event.current.type) 
		{
		case EventType.MouseMove:
			s_PickCoordinates = GUIUtility.GUIToScreenPoint (Event.current.mousePosition);
			StealMouseCapture ();
			SendEvent ("EyeDropperUpdate");
			break;
		case EventType.MouseDown:
			if (Event.current.button == 0) 
			{
				s_PickCoordinates = GUIUtility.GUIToScreenPoint (Event.current.mousePosition);
				s_LastPickedColor = EyeDropper.GetPickedColor ();
				window.Close ();
				SendEvent ("EyeDropperClicked");
			}
			break;
		case EventType.KeyDown:
			if (Event.current.keyCode == KeyCode.Escape)
			{
				window.Close ();
				SendEvent ("EyeDropperCancelled");					
			}
			break;
		}
	}

	void SendEvent (string eventName) 
	{
		if (m_DelegateView) 
		{
			Event e = EditorGUIUtility.CommandEvent (eventName);
			m_DelegateView.SendEvent (e);
			GUIUtility.ExitGUI ();
		}
	}

    public new void OnDestroy () 
	{
        if (m_Preview)
            Object.DestroyImmediate (m_Preview);
    }
}
    
} // namespace
