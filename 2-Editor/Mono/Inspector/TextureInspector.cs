using UnityEngine;
using UnityEditor;


namespace UnityEditor
{
	internal class PreviewHelpers
	{
		//This assumes NPOT RenderTextures since Unity 4.3 has this as a requirment already.
		internal static void AdjustWidthAndHeightForStaticPreview(int textureWidth, int textureHeight, ref int width, ref int height)
		{
			int orgWidth = width;
			int orgHeight = height;
			
			if (textureWidth <= width && textureHeight <= height)
			{
				// For textures smaller than our wanted width and height we use the textures size
				// to prevent excessive magnification artifacts (as seen in the Asset Store).
				width = textureWidth;
				height = textureHeight;
			}
			else
			{
				// For textures larger than our wanted width and height we ensure to 
				// keep aspect ratio of the texture and fit it to best match our wanted width and height.
				float relWidth = height / (float)textureWidth;
				float relHeight = width / (float)textureHeight;
				
				float scale = Mathf.Min(relHeight, relWidth);
				
				width = Mathf.RoundToInt(textureWidth * scale);
				height = Mathf.RoundToInt(textureHeight * scale);
				
			}

			// Ensure we have not scaled size below 2 pixels
			width = Mathf.Clamp(width, 2, orgWidth);
			height = Mathf.Clamp(height, 2, orgHeight);
		}		
	}
	
	[CustomEditor(typeof(Texture2D))]
	[CanEditMultipleObjects]
	internal class TextureInspector : Editor
	{
		static GUIContent s_SmallZoom, s_LargeZoom, s_AlphaIcon, s_RGBIcon;
		static GUIStyle s_PreButton, s_PreSlider, s_PreSliderThumb, s_PreLabel;
		private bool m_bShowAlpha;

		// Plain Texture
		SerializedProperty m_WrapMode;
		SerializedProperty m_FilterMode;
		SerializedProperty m_Aniso;
	
		[SerializeField]
		protected Vector2 m_Pos;
		[SerializeField]
		float m_MipLevel = 0;
		
		public static bool IsNormalMap (Texture t)
		{
			TextureUsageMode mode = TextureUtil.GetUsageMode (t);
			return mode == TextureUsageMode.NormalmapPlain || mode == TextureUsageMode.NormalmapDXT5nm;
		}
		
		protected virtual void OnEnable ()
		{
			float time = Time.realtimeSinceStartup;
			if (Time.realtimeSinceStartup - time > 1)
				Debug.LogWarning("Took "+(Time.realtimeSinceStartup - time)+" seconds to create SerializedObject!");
			
			m_WrapMode = serializedObject.FindProperty ("m_TextureSettings.m_WrapMode");
			m_FilterMode = serializedObject.FindProperty ("m_TextureSettings.m_FilterMode");
			m_Aniso = serializedObject.FindProperty ("m_TextureSettings.m_Aniso");
		}
		
		// Note: Even though this is a custom editor for Texture2D, the target may not be a Texture2D,
		// since other editors inherit from this one, such as ProceduralTextureInspector.
		public override void OnInspectorGUI ()
		{
			serializedObject.Update ();
			
			// Wrap mode
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = m_WrapMode.hasMultipleDifferentValues;
			TextureWrapMode wrap = (TextureWrapMode)m_WrapMode.intValue;
			wrap = (TextureWrapMode)EditorGUILayout.EnumPopup (EditorGUIUtility.TempContent ("Wrap Mode"), wrap);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
				m_WrapMode.intValue = (int)wrap;
			
			// Filter mode
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = m_FilterMode.hasMultipleDifferentValues;
			FilterMode filter = (FilterMode)m_FilterMode.intValue;
			filter = (FilterMode)EditorGUILayout.EnumPopup (EditorGUIUtility.TempContent ("Filter Mode"), filter);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
				m_FilterMode.intValue = (int)filter;
			
			// Aniso
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = m_Aniso.hasMultipleDifferentValues;
			int aniso = m_Aniso.intValue;
			aniso = EditorGUILayout.IntSlider("Aniso Level", aniso, 0, 9);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
				m_Aniso.intValue = aniso;
			
			serializedObject.ApplyModifiedProperties ();
		}
		
		static void Init ()
		{
			s_SmallZoom = EditorGUIUtility.IconContent ("PreTextureMipMapLow");
			s_LargeZoom = EditorGUIUtility.IconContent ("PreTextureMipMapHigh");
			s_AlphaIcon = EditorGUIUtility.IconContent ("PreTextureAlpha");
			s_RGBIcon = EditorGUIUtility.IconContent ("PreTextureRGB");
			s_PreButton = "preButton";
			s_PreSlider = "preSlider";
			s_PreSliderThumb = "preSliderThumb";
			s_PreLabel = "preLabel";
		}
		
		public override void OnPreviewSettings ()
		{
			Init ();
			// TextureInspector code is reused for RenderTexture and Cubemap inspectors.
			// Make sure we can handle the situation where target is just a Texture and
			// not a Texture2D.
			Texture tex = target as Texture;
			Texture2D tex2D = target as Texture2D;
			bool showMode = true;
			bool alphaOnly = false;
			bool hasAlpha = true;
			int mipCount = 1;
			if (tex2D != null)
			{
				alphaOnly = true;
				hasAlpha = false;
				foreach (Texture2D t2 in targets)
				{
					TextureFormat format = t2.format;
					if (!TextureUtil.IsAlphaOnlyTextureFormat (format))
						alphaOnly = false;
					if (TextureUtil.HasAlphaTextureFormat (format))
						hasAlpha = true;
					mipCount = Mathf.Max (mipCount, TextureUtil.CountMipmaps (tex2D));
				}
			}
			
			if (alphaOnly)
			{
				m_bShowAlpha = true;
				showMode = false;
			}
			else if (!hasAlpha)
			{
				m_bShowAlpha = false;
				showMode = false;
			}

			if (showMode && !IsNormalMap(tex))
				m_bShowAlpha = GUILayout.Toggle(m_bShowAlpha, m_bShowAlpha ? s_AlphaIcon : s_RGBIcon, s_PreButton);
			
			GUI.enabled = (mipCount != 1);
			GUILayout.Box (s_SmallZoom, s_PreLabel);
			GUI.changed = false;
			m_MipLevel = Mathf.Round (GUILayout.HorizontalSlider (m_MipLevel, mipCount - 1, 0, s_PreSlider, s_PreSliderThumb, GUILayout.MaxWidth (64)));
			GUILayout.Box (s_LargeZoom, s_PreLabel);
			GUI.enabled = true;
		}
		
		public override bool HasPreviewGUI()
		{
			return (target != null);
		}
		
		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			if (Event.current.type == EventType.Repaint)
				background.Draw(r, false, false, false, false);
			
			// show texture
			Texture t = target as Texture;
			
			// Render target must be created before we can display it (case 491797)
			RenderTexture rt = t as RenderTexture;
			if (rt != null && !rt.IsCreated())
			    rt.Create();

			// Substances can report zero sizes in some cases just after a parameter change;
			// guard against that.
			int texWidth = Mathf.Max (t.width,1);
			int texHeight = Mathf.Max (t.height, 1);

			float mipLevel = t is Texture2D ? Mathf.Min (m_MipLevel, TextureUtil.CountMipmaps (t as Texture2D) - 1) : 0;
			float zoomLevel = Mathf.Min (Mathf.Min (r.width / texWidth, r.height / texHeight), 1);
			Rect wantedRect = new Rect (r.x,r.y, texWidth * zoomLevel, texHeight * zoomLevel);
			PreviewGUI.BeginScrollView (r, m_Pos, wantedRect, "PreHorizontalScrollbar", "PreHorizontalScrollbarThumb");
			float oldBias = t.mipMapBias;
			TextureUtil.SetMipMapBiasNoDirty (t, mipLevel - Log2 (texWidth / wantedRect.width));
			FilterMode oldFilter = t.filterMode;
			TextureUtil.SetFilterModeNoDirty (t, FilterMode.Point);
			
			if (m_bShowAlpha)
			{
				EditorGUI.DrawTextureAlpha(wantedRect, t);
			}
			else
			{
				Texture2D t2d = t as Texture2D;
				if (t2d != null && t2d.alphaIsTransparency)
				{
					EditorGUI.DrawTextureTransparent(wantedRect, t);
				}
				else
				{
					EditorGUI.DrawPreviewTexture(wantedRect, t);
				}
			}


#if ENABLE_SPRITES
			
			// TODO: Less hacky way to prevent sprite rects to not appear in smaller previews like icons.
			if (wantedRect.width > 32 && wantedRect.height > 32)
			{
				string path = AssetDatabase.GetAssetPath(t);
				TextureImporter textureImporter = AssetImporter.GetAtPath(path) as TextureImporter;
				SpriteMetaData[] spritesheet = textureImporter != null ? textureImporter.spritesheet : null;

				if (spritesheet != null && textureImporter.spriteImportMode == SpriteImportMode.Multiple)
				{
					Rect screenRect = new Rect();
					Rect sourceRect = new Rect();
					GUI.CalculateScaledTextureRects(wantedRect, ScaleMode.StretchToFill, (float) t.width / (float) t.height, ref screenRect, ref sourceRect);

					int origWidth = t.width;
					int origHeight = t.height;
					textureImporter.GetWidthAndHeight(ref origWidth, ref origHeight);
					float definitionScale = (float)t.width / (float)origWidth;

					HandleUtility.handleWireMaterial.SetPass(0);
					GL.PushMatrix();
					GL.MultMatrix(Handles.matrix);
					GL.Begin(GL.LINES);
					GL.Color(new Color(1f,1f,1f,0.5f));
					foreach (SpriteMetaData sprite in spritesheet)
					{
						Rect spriteRect = sprite.rect;
						Rect spriteScreenRect = new Rect();
						spriteScreenRect.xMin = screenRect.xMin + screenRect.width * (spriteRect.xMin / t.width * definitionScale);
						spriteScreenRect.xMax = screenRect.xMin + screenRect.width * (spriteRect.xMax / t.width * definitionScale);
						spriteScreenRect.yMin = screenRect.yMin + screenRect.height * (1f - spriteRect.yMin / t.height * definitionScale);
						spriteScreenRect.yMax = screenRect.yMin + screenRect.height * (1f - spriteRect.yMax / t.height * definitionScale);
						DrawRect(spriteScreenRect);
					}
					GL.End();
					GL.PopMatrix();
				}
			}
#endif

			TextureUtil.SetMipMapBiasNoDirty (t, oldBias);
			TextureUtil.SetFilterModeNoDirty (t, oldFilter);
		
			m_Pos = PreviewGUI.EndScrollView ();
			if (mipLevel != 0)
				EditorGUI.DropShadowLabel (new Rect (r.x, r.y, r.width, 20), "Mip " + mipLevel);
		}

		private void DrawRect (Rect rect)
		{
			GL.Vertex (new Vector3 (rect.xMin, rect.yMin, 0f));
			GL.Vertex (new Vector3 (rect.xMax, rect.yMin, 0f));
			GL.Vertex (new Vector3 (rect.xMax, rect.yMin, 0f));
			GL.Vertex (new Vector3 (rect.xMax, rect.yMax, 0f));
			GL.Vertex (new Vector3 (rect.xMax, rect.yMax, 0f));
			GL.Vertex (new Vector3 (rect.xMin, rect.yMax, 0f));
			GL.Vertex (new Vector3 (rect.xMin, rect.yMax, 0f));
			GL.Vertex (new Vector3 (rect.xMin, rect.yMin, 0f));
		}


		public override Texture2D RenderStaticPreview(string assetPath, Object[] subAssets, int width, int height)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			{
				return null;
			}
			
			Texture texture = target as Texture;
			PreviewHelpers.AdjustWidthAndHeightForStaticPreview(texture.width, texture.height, ref width, ref height);
			
			EditorUtility.SetTemporarilyAllowIndieRenderTexture (true);
			RenderTexture savedRT = RenderTexture.active;
			Rect savedViewport = ShaderUtil.rawViewportRect;

			RenderTexture tmp = RenderTexture.GetTemporary(
									width,
									height,
									0,
									RenderTextureFormat.Default,
									RenderTextureReadWrite.Linear);
			
			Material mat = EditorGUI.GetMaterialForSpecialTexture(texture);

			if (mat)
			{
				// We don't want Materials in Editor Resources Project to be modified in the end, so we use an duplicate.
				if (Unsupported.IsDeveloperBuild ())
					mat = new Material (mat);
				Graphics.Blit(texture, tmp, mat);
			}
			else
			{
				Graphics.Blit(texture, tmp);
			}

			RenderTexture.active = tmp;
			// Setting color space on this texture does not matter... internally we just grab the data array
			// when we call GetAssetPreview it generates a new texture from that data...
			Texture2D copy;
			Texture2D tex2d = target as Texture2D;
			if (tex2d != null && tex2d.alphaIsTransparency)
			{
				copy = new Texture2D(width, height, TextureFormat.ARGB32, false);
			}
			else
			{
				copy = new Texture2D(width, height, TextureFormat.RGB24, false);
			}
			copy.ReadPixels(new Rect(0, 0, width, height), 0, 0);
			copy.Apply();
			RenderTexture.ReleaseTemporary(tmp);
			
			EditorGUIUtility.SetRenderTextureNoViewport (savedRT);
			ShaderUtil.rawViewportRect = savedViewport;
			EditorUtility.SetTemporarilyAllowIndieRenderTexture (false);
			
			// Kill the duplicate
			if (mat && Unsupported.IsDeveloperBuild ())
				Object.DestroyImmediate (mat);
			
			return copy;
		}
		
		float Log2 (float x)
		{
			return (float)(System.Math.Log(x)/System.Math.Log(2));
		}
		
		public override string GetInfoString ()
		{
			// TextureInspector code is reused for RenderTexture and Cubemap inspectors.
			// Make sure we can handle the situation where target is just a Texture and
			// not a Texture2D.
			Texture t = target as Texture;
			Texture2D t2 = target as Texture2D;
			string info = t.width.ToString() + "x" + t.height.ToString();
			
			if (QualitySettings.desiredColorSpace == ColorSpace.Linear)
				info += " " + TextureUtil.GetTextureColorSpaceString (t);

			bool showSize = true;
			
			bool isNormalmap = IsNormalMap(t);
			bool stillNeedsCompression = TextureUtil.DoesTextureStillNeedToBeCompressed(AssetDatabase.GetAssetPath(t));
			bool isNPOT = t2 != null && TextureUtil.IsNonPowerOfTwo (t2);
			TextureFormat format = TextureUtil.GetTextureFormat(t);
			
			showSize = !stillNeedsCompression;
			if (isNPOT)
				info += " (NPOT)";
			if (stillNeedsCompression)
				info += " (Not yet compressed)";
			else
			{
				if (isNormalmap)
				{
					switch (format)
					{
					case TextureFormat.DXT5:
						info += "  DXTnm";
						break;
					case TextureFormat.ARGB32:
						info += "  Nm 32 bit";
						break;
					case TextureFormat.ARGB4444:
						info += "  Nm 16 bit";
						break;
					default:
						info += "  " + TextureUtil.GetTextureFormatString (format);
						break;
					}
				}
				else
					info += "  " + TextureUtil.GetTextureFormatString (format);
			}
			
			if (showSize)
				info += "\n" + EditorUtility.FormatBytes(TextureUtil.GetStorageMemorySize(t));
			return info;
		}
	}
}


class PreviewGUI
{
	static int sliderHash = "Slider".GetHashCode ();
	static Rect s_ViewRect, s_Position;
	static Vector2 s_ScrollPos;
	
	internal static void BeginScrollView (Rect position, Vector2 scrollPosition, Rect viewRect, GUIStyle horizontalScrollbar, GUIStyle verticalScrollbar)
	{
		s_ScrollPos = scrollPosition;
		s_ViewRect = viewRect;
		s_Position = position;
		GUIClip.Push (position, new Vector2 (Mathf.Round (-scrollPosition.x - viewRect.x - (viewRect.width - position.width) * .5f), Mathf.Round (-scrollPosition.y - viewRect.y - (viewRect.height - position.height) * .5f)), Vector2.zero, false);
	}
	
	internal class Styles
	{
		public static GUIStyle preButton;
		public static void Init ()
		{
			preButton = "preButton";
		}
	}
	
	public static int CycleButton (int selected, GUIContent[] options)
	{
		Styles.Init ();
		return EditorGUILayout.CycleButton (selected, options, Styles.preButton);
	}
	
	public static Vector2 EndScrollView ()
	{
		GUIClip.Pop ();	
		
		Rect clipRect = s_Position, position = s_Position, viewRect = s_ViewRect;
		
		Vector2 scrollPosition = s_ScrollPos;
		switch (Event.current.type) {
		case EventType.Layout:
			GUIUtility.GetControlID (sliderHash, FocusType.Passive);
			GUIUtility.GetControlID (sliderHash, FocusType.Passive);
			break;
		case EventType.Used:
			break;
		default:
			bool needsVertical = false, needsHorizontal = false;
			
			// Check if we need a horizontal scrollbar
			if (needsHorizontal || viewRect.width > clipRect.width) 
				needsHorizontal = true;
			
			if (needsVertical || viewRect.height > clipRect.height)
				needsVertical = true;
			int id = GUIUtility.GetControlID (sliderHash, FocusType.Passive);				
			
			if (needsHorizontal) {
				GUIStyle horizontalScrollbar = "PreHorizontalScrollbar";
				GUIStyle horizontalScrollbarThumb = "PreHorizontalScrollbarThumb";
				float offset = (viewRect.width - clipRect.width) * .5f;
				scrollPosition.x = GUI.Slider (new Rect (position.x, position.yMax - horizontalScrollbar.fixedHeight, clipRect.width - (needsVertical ? horizontalScrollbar.fixedHeight : 0) , horizontalScrollbar.fixedHeight), 
									 scrollPosition.x, clipRect.width + offset, -offset, viewRect.width, 
									 horizontalScrollbar, horizontalScrollbarThumb, true, id);
			} else {
				// Get the same number of Control IDs so the ID generation for childrent don't depend on number of things above
				scrollPosition.x = 0;
			}
				
			id = GUIUtility.GetControlID (sliderHash, FocusType.Passive);
			
			if (needsVertical) {
				GUIStyle verticalScrollbar = "PreVerticalScrollbar";
				GUIStyle verticalScrollbarThumb = "PreVerticalScrollbarThumb";
				float offset = (viewRect.height - clipRect.height) * .5f;
				scrollPosition.y = GUI.Slider (new Rect (clipRect.xMax - verticalScrollbar.fixedWidth, clipRect.y, verticalScrollbar.fixedWidth, clipRect.height),
									 scrollPosition.y, clipRect.height + offset, -offset, viewRect.height,
									 verticalScrollbar, verticalScrollbarThumb, false, id);
			} else {
				scrollPosition.y = 0;
			} 
			break;
		}
//		scrollPosition = Drag2D (scrollPosition, position);
		return scrollPosition;
	}

	public static Vector2 Drag2D (Vector2 scrollPosition, Rect position)
	{
		int id = GUIUtility.GetControlID (sliderHash, FocusType.Passive);
		Event evt = Event.current;
		switch (evt.GetTypeForControl (id))
		{
		case EventType.MouseDown:
			if (position.Contains (evt.mousePosition) && position.width > 50)
			{
				GUIUtility.hotControl = id;
				evt.Use ();
				EditorGUIUtility.SetWantsMouseJumping(1);
			}
			break;
		case EventType.MouseDrag:
			if (GUIUtility.hotControl == id)
			{
				scrollPosition -= evt.delta * (evt.shift ? 3 : 1) / Mathf.Min (position.width, position.height) * 140.0f;
				scrollPosition.y = Mathf.Clamp (scrollPosition.y, -90, 90);
				evt.Use ();
				GUI.changed = true;
			}
			break;
		case EventType.MouseUp:
			if (GUIUtility.hotControl == id)
				GUIUtility.hotControl = 0;
			EditorGUIUtility.SetWantsMouseJumping(0);
			break;	
		}
		return scrollPosition;
	}
}
