#if ENABLE_SPRITES

using System;
using System.IO;
using UnityEngine;
using UnityEditorInternal;
using System.Collections.Generic;

namespace UnityEditor
{
	[Serializable]
	internal class SpriteRect
	{
		[SerializeField]
		public string m_Name = string.Empty;

		[SerializeField]
		public Vector2 m_Pivot = Vector2.zero;

		[SerializeField]
		public SpriteAlignment m_Alignment;

		[SerializeField]
		public Rect m_Rect;
#if ENABLE_SPRITECOLLIDER
		[SerializeField] 
		public int m_ColliderAlphaCutoff;

		[SerializeField] 
		public float m_ColliderDetail;
#endif
	}
	
	[Serializable]
	internal class SpriteRectCache : ScriptableObject
	{
		[SerializeField]
		public List<SpriteRect> m_Rects;

		public int Count
		{
			get 
			{ 
				if(m_Rects != null) 
					return m_Rects.Count;
				return 0;
			}
		}

		public SpriteRect RectAt (int i)
		{
			if (i >= Count)
				return null;

			return m_Rects[i];
		}

		public void AddRect (SpriteRect r)
		{
			if (m_Rects != null)
				m_Rects.Add (r);
		}

		public void RemoveRect (SpriteRect r)
		{
			if (m_Rects != null)
				m_Rects.Remove (r);
		}
		
		public void ClearAll ()
		{
			if (m_Rects != null)
				m_Rects.Clear ();
		}

		public int GetIndex (SpriteRect spriteRect)
		{
			if (m_Rects != null) 
				return m_Rects.FindIndex (p => p.Equals (spriteRect));

			return 0;
		}

		public bool Contains (SpriteRect spriteRect)
		{
			if (m_Rects != null) 
				return m_Rects.Contains (spriteRect);

			return false;
		}

		void OnEnable ()
		{
			if (m_Rects == null)
				m_Rects = new List<SpriteRect> ();
		}
	}

	internal class SpriteEditorWindow : SpriteUtilityWindow
	{
		internal static PrefKey k_SpriteEditorTrim = new PrefKey ("Sprite Editor/Trim", "t");
		const float maxSnapDistance = 14f;
#if ENABLE_SPRITECOLLIDER
		const float k_ColliderFadeoutTime = 3f;
		const float k_ColliderFadeoutSpeed = 0.2f;
		const float k_ColliderFadeinSpeed = 0.75f;
#endif
		
		public static SpriteEditorWindow s_Instance;
		public bool m_ResetOnNextRepaint;
		public bool m_IgnoreNextPostprocessEvent;
		public Texture2D m_OriginalTexture;

		private SpriteRectCache m_RectsCache;
		private SerializedObject m_TextureImporterSO;
		private TextureImporter m_TextureImporter;
		private SerializedProperty m_TextureImporterSprites;
		
		public static bool s_OneClickDragStarted = false;
		private bool m_TextureIsDirty;
		private static bool[] s_AlphaPixelCache;
#if ENABLE_SPRITECOLLIDER
		private Vector2[][] m_ColliderPreviewCache;
		private AnimFloat m_ColliderPreviewAlpha;
		private AnimValueManager m_AnimValueManager;
		private double? m_LastEditOfColliderTime;
#endif

		public string m_SelectedAssetPath;

		[SerializeField]
		private SpriteRect m_Selected;

		public void RefreshPropertiesCache()
		{
			m_OriginalTexture = GetSelectedTexture2D ();

			if (m_OriginalTexture == null)
				return;
			
			m_TextureImporter = TextureImporter.GetAtPath (m_SelectedAssetPath) as TextureImporter;
			
			if (m_TextureImporter == null)
				return;
			
			m_TextureImporterSO = new SerializedObject (m_TextureImporter);
			m_TextureImporterSprites = m_TextureImporterSO.FindProperty ("m_SpriteSheet.m_Sprites");

			if(m_RectsCache != null)
				selected = m_TextureImporterSprites.arraySize > 0 ? m_RectsCache.RectAt (0) : null;

			int width = 0;
			int height = 0;
			m_TextureImporter.GetWidthAndHeight(ref width, ref height); // Get the original asset size
			m_Texture = CreateTemporaryDuplicate (AssetDatabase.LoadMainAssetAtPath (m_TextureImporter.assetPath) as Texture2D, width, height);
			//TODO[BUG 569578]:optimize texture usage. We don't need to duplicate it scaled up. Instead we could scale the window.
			
			if (m_Texture == null)
				return;
			
			m_Texture.filterMode = FilterMode.Point;
		}

		public void InvalidatePropertiesCache()
		{
			if (m_RectsCache)
			{
				m_RectsCache.ClearAll();
				DestroyImmediate (m_RectsCache);
			}
			if (m_Texture)
				DestroyImmediate (m_Texture);

#if ENABLE_SPRITECOLLIDER
			m_ColliderPreviewCache = null;
#endif
			m_OriginalTexture = null;
			m_TextureImporter = null;
			m_TextureImporterSO = null;
			m_TextureImporterSprites = null;
			s_AlphaPixelCache = null;
		}

		public Texture2D originalTexture { get { return m_OriginalTexture; } }

		private SpriteRect selected { 
			get { return m_Selected; } 
			set {
				if (value != m_Selected)
				{
					m_Selected = value;
#if ENABLE_SPRITECOLLIDER
					m_ColliderPreviewAlpha.value = 0f;
#endif
				}
			} 
		}

		private int defaultColliderAlphaCutoff
		{
#if ENABLE_SPRITECOLLIDER
			get { return m_TextureImporterSO.FindProperty ("m_SpriteColliderAlphaCutoff").intValue; }
#else
			get { return 254; }
#endif
		}
		private float defaultColliderDetail
		{
#if ENABLE_SPRITECOLLIDER
			get { return m_TextureImporterSO.FindProperty ("m_SpriteColliderDetail").floatValue; }
#else
			get { return 0.25f; }
#endif
		}

		private Rect inspectorRect 
		{
			get
			{
				return new Rect (
					position.width - k_InspectorWidth - k_InspectorWindowMargin - k_ScrollbarMargin,
					position.height - k_InspectorHeight - k_InspectorWindowMargin - k_ScrollbarMargin,
					k_InspectorWidth, 
					k_InspectorHeight);
			}
		}

		private bool activeTextureHasSprites
		{
			get
			{
				if (m_TextureImporter != null)
					return m_TextureImporter.spriteImportMode == SpriteImportMode.Multiple;
				return false;
			}
		}

#if ENABLE_SPRITECOLLIDER
		private bool showColliderGizmo
		{
			get
			{
				InitializeAnimVariables ();
				return m_ColliderPreviewAlpha.value > 0f;
			}
			set
			{
				InitializeAnimVariables ();
				if (value)
				{
					m_ColliderPreviewAlpha.speedMultiplier = k_ColliderFadeinSpeed;
					m_ColliderPreviewAlpha.target = 1f;
				}
				else
				{
					m_ColliderPreviewAlpha.speedMultiplier = k_ColliderFadeoutSpeed;
					m_ColliderPreviewAlpha.target = 0f;
				}
			}
		}
#endif

		private void InitializeAnimVariables ()
		{
#if ENABLE_SPRITECOLLIDER
			if (m_ColliderPreviewAlpha == null)
			{
				m_ColliderPreviewAlpha = new AnimFloat ();
				m_ColliderPreviewAlpha.value = 0f;
				m_ColliderPreviewAlpha.target = 0f;
			}
			if (m_AnimValueManager == null)
			{
				m_AnimValueManager = new AnimValueManager ();
				m_AnimValueManager.Add (m_ColliderPreviewAlpha);
			}
			m_AnimValueManager.callback = Repaint;
#endif
		}

		bool activeTextureSelected
		{
			get { return m_TextureImporter != null && m_Texture != null && m_OriginalTexture != null; }
		}

		public enum AutoSlicingMethod
		{
			DeleteAll = 0,
			Smart = 1,
			Safe = 2
		}

		public readonly GUIContent[] spriteAlignmentOptions =
			{				
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Center"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopLeft"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Top"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopRight"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Left"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Right"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomLeft"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Bottom"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomRight"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Custom"),
			};

		public static GUIContent s_PivotLabel = EditorGUIUtility.TextContent ("Pivot");
		public static GUIContent s_NoSelectionWarning = EditorGUIUtility.TextContent ("SpriteEditor.NoTextureOrSpriteSelected");


		public bool textureIsDirty
		{
			get { return m_TextureIsDirty; }
			set
			{
				m_TextureIsDirty = value;
			}
		}

		public bool selectedTextureChanged
		{
			get
			{
				Texture2D newTexture = GetSelectedTexture2D ();
				return newTexture != null && m_OriginalTexture != newTexture;
			}
		}

		void OnSelectionChange()
		{
			if (selectedTextureChanged)
				HandleApplyRevertDialog ();

			InvalidatePropertiesCache ();
			Reset ();
			UpdateSelectedSprite ();
			Repaint ();
		}

		public void Reset ()
		{
			InvalidatePropertiesCache ();
			
			selected = null;
			m_Zoom = -1;
			RefreshPropertiesCache ();
			RefreshRects ();
			Repaint();
#if ENABLE_SPRITECOLLIDER
			showColliderGizmo = false;
#endif
		}

		void OnEnable ()
		{
			minSize = new Vector2 (800f, 512f);
			title = EditorGUIUtility.TextContent ("SpriteEditorWindow.WindowTitle").text;
			s_Instance = this;
			Undo.undoRedoPerformed += UndoRedoPerformed;
			Reset ();
		}

		private void UndoRedoPerformed()
		{
			Texture2D newTexture = GetSelectedTexture2D ();

			// Was selected texture changed by undo?
			if (newTexture != null && m_OriginalTexture != newTexture)
				OnSelectionChange ();

			if (m_RectsCache != null && !m_RectsCache.Contains (selected))
				selected = null;

			Repaint ();
		}

		private void OnDisable ()
		{
			Undo.undoRedoPerformed -= UndoRedoPerformed;
			HandleApplyRevertDialog ();
			InvalidatePropertiesCache ();			
			s_Instance = null;
		}

		void HandleApplyRevertDialog()
		{
			if (textureIsDirty && m_TextureImporter != null)
			{
				if (EditorUtility.DisplayDialog ("Unapplied import settings", "Unapplied import settings for \'" + m_TextureImporter.assetPath + "\'", "Apply", "Revert"))
					DoApply ();
				else
					DoRevert ();
			}
		}

		void RefreshRects ()
		{
			if (m_TextureImporterSprites == null)
				return;

			if (m_RectsCache)
			{
				m_RectsCache.ClearAll();
				DestroyImmediate (m_RectsCache);
			}
			m_RectsCache = CreateInstance <SpriteRectCache> ();

			for (int i = 0; i < m_TextureImporterSprites.arraySize; i++)
			{
				SpriteRect spriteRect = new SpriteRect();

				spriteRect.m_Rect = m_TextureImporterSprites.GetArrayElementAtIndex (i).FindPropertyRelative ("m_Rect").rectValue;
				spriteRect.m_Name = m_TextureImporterSprites.GetArrayElementAtIndex (i).FindPropertyRelative ("m_Name").stringValue;
				spriteRect.m_Alignment = (SpriteAlignment)m_TextureImporterSprites.GetArrayElementAtIndex (i).FindPropertyRelative ("m_Alignment").intValue;
				spriteRect.m_Pivot = SpriteEditorUtility.GetPivotValue (spriteRect.m_Alignment, m_TextureImporterSprites.GetArrayElementAtIndex(i).FindPropertyRelative("m_Pivot").vector2Value);
#if ENABLE_SPRITECOLLIDER
				spriteRect.m_ColliderAlphaCutoff = m_TextureImporterSprites.GetArrayElementAtIndex (i).FindPropertyRelative ("m_ColliderAlphaCutoff").intValue;
				spriteRect.m_ColliderDetail = m_TextureImporterSprites.GetArrayElementAtIndex (i).FindPropertyRelative ("m_ColliderDetail").floatValue;
#endif

				m_RectsCache.AddRect (spriteRect);
			}
		}

#if ENABLE_SPRITECOLLIDER
		void Update()
		{
			// We hide (fadeout) the collider if user haven't touched the settings for awhile
			if (m_LastEditOfColliderTime != null && m_LastEditOfColliderTime + k_ColliderFadeoutTime < EditorApplication.timeSinceStartup)
			{
				showColliderGizmo = false;

				// setting this to null means that there is no "last time" which we haven't already handled
				m_LastEditOfColliderTime = null;
			}
		}
#endif

		void OnGUI ()
		{
			if (m_ResetOnNextRepaint || selectedTextureChanged)
			{
				Reset ();
				m_ResetOnNextRepaint = false;
			}
			Matrix4x4 oldHandlesMatrix = Handles.matrix;

			if (!activeTextureSelected)
			{
				EditorGUI.BeginDisabledGroup(true);
				GUILayout.Label(s_NoSelectionWarning);
				EditorGUI.EndDisabledGroup();
				return;
			}

			InitStyles ();

			if (!activeTextureHasSprites)
				selected = null;


			EditorGUI.BeginDisabledGroup(!activeTextureHasSprites);

			// Top menu bar
			Rect toolbarRect = EditorGUILayout.BeginHorizontal (GUIContent.none, "Toolbar");
			DoToolbarGUI ();
			GUILayout.FlexibleSpace ();
			DoApplyRevertGUI ();
			DoAlphaZoomToolbarGUI ();
			EditorGUILayout.EndHorizontal ();

			// Texture view
			EditorGUILayout.BeginHorizontal();
			m_TextureViewRect = new Rect(0f, toolbarRect.yMax, position.width - k_ScrollbarMargin, position.height - k_ScrollbarMargin - toolbarRect.height);
			GUILayout.FlexibleSpace ();
			DoTextureGUI();
			EditorGUILayout.EndHorizontal();

			// Selected frame inspector
			if (activeTextureHasSprites)
				DoSelectedFrameInspector ();

			Handles.matrix = oldHandlesMatrix;

			EditorGUI.EndDisabledGroup();
		}

		protected override void DoTextureGUIExtras()
		{
			if (activeTextureHasSprites)
			{
				HandlePivotHandle ();
				HandleScalingHandles ();
				HandleSelection ();
				HandleDragging ();

				HandleCreate ();
				HandleDelete ();
				HandleDuplicate ();
			}
		}

		private void DoToolbarGUI ()
		{
			EditorGUI.BeginDisabledGroup(!activeTextureHasSprites);

			Rect r = EditorGUILayout.BeginHorizontal ();
			if (GUILayout.Button ("Slice", "toolbarPopup"))
			{
				SpriteEditorMenu.s_SpriteEditor = this;
				if (SpriteEditorMenu.ShowAtPosition (r))
					GUIUtility.ExitGUI ();
			}
			EditorGUILayout.EndHorizontal();

			EditorGUI.EndDisabledGroup();
		}

		private void DoSelectedFrameInspector ()
		{
			if (selected != null)
			{
#if ENABLE_SPRITECOLLIDER
				bool colliderValuesChanged = false;
#endif

				EditorGUI.BeginDisabledGroup(!activeTextureHasSprites);

				EditorGUIUtility.wideMode = true;
				float oldLabelWidth = EditorGUIUtility.labelWidth;
				EditorGUIUtility.labelWidth = 135f;

				GUILayout.BeginArea (inspectorRect);
				GUILayout.BeginVertical(new GUIContent("Sprite"), GUI.skin.window);
				
				EditorGUI.BeginChangeCheck();
				string oldName = selected.m_Name;
				string newName = EditorGUILayout.TextField("Name", oldName);

				Rect oldRect = selected.m_Rect;
				Rect newRect = EditorGUILayout.RectField("Position", oldRect);

				Rect buttonRect = EditorGUILayout.s_LastRect;
				buttonRect = new Rect(buttonRect.xMax - 50f, buttonRect.yMin + buttonRect.height * .2f, 50f, buttonRect.height * .6f);

				if (GUI.Button (buttonRect, "Trim") || k_SpriteEditorTrim.activated)
				{
					newRect = TrimAlpha (newRect);
					GUI.changed = true;
					Event.current.Use ();
				}

#if ENABLE_SPRITECOLLIDER
				EditorGUI.BeginChangeCheck();
				float oldDetail = selected.m_ColliderDetail;
				float newDetail = EditorGUILayout.Slider ("Collider Detail", oldDetail, 0f, 1f);
				int oldCutoff = selected.m_ColliderAlphaCutoff;
				int newCutoff = EditorGUILayout.IntSlider ("Collider Alpha Cutoff", oldCutoff, 0, 254);
				if (EditorGUI.EndChangeCheck ())
				{
					colliderValuesChanged = true;
					showColliderGizmo = true;
					m_LastEditOfColliderTime = EditorApplication.timeSinceStartup;
				}
#endif

				selected.m_Alignment = (SpriteAlignment)EditorGUILayout.Popup (s_PivotLabel, (int)selected.m_Alignment, spriteAlignmentOptions);

				Vector2 oldPivot = selected.m_Pivot;
				Vector2 newPivot = oldPivot;

				EditorGUI.BeginDisabledGroup(selected.m_Alignment != SpriteAlignment.Custom);
				newPivot = EditorGUILayout.Vector2Field("Custom Pivot", oldPivot);
				EditorGUI.EndDisabledGroup();
				
				if (EditorGUI.EndChangeCheck())
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Modify sprite");
					
					// Clamp newRect to texture bounds
					newRect.x = Mathf.Clamp (newRect.x, 0, m_Texture.width - newRect.width);
					newRect.y = Mathf.Clamp (newRect.y, 0, m_Texture.height - newRect.height);
					newRect.width = Mathf.Clamp (newRect.width, 1, m_Texture.width - newRect.x);
					newRect.height = Mathf.Clamp (newRect.height, 1, m_Texture.height - newRect.y);

					selected.m_Name = newName;
					selected.m_Rect = newRect;
					selected.m_Pivot = SpriteEditorUtility.GetPivotValue ((SpriteAlignment)selected.m_Alignment, newPivot);
#if ENABLE_SPRITECOLLIDER
					selected.m_ColliderAlphaCutoff = newCutoff;
					selected.m_ColliderDetail = newDetail;
#endif


#if ENABLE_SPRITECOLLIDER
					if (!colliderValuesChanged)
						showColliderGizmo = false;

					m_ColliderPreviewCache = null;
#endif
					textureIsDirty = true;
				}
				
				GUILayout.EndVertical();
				GUILayout.EndArea();

				EditorGUIUtility.labelWidth = oldLabelWidth;

				EditorGUI.EndDisabledGroup();
			}
		}

		private void DoApplyRevertGUI ()
		{
			EditorGUI.BeginDisabledGroup(!textureIsDirty);

			if (GUILayout.Button ("Revert", EditorStyles.toolbarButton))
				DoRevert ();

			if (GUILayout.Button ("Apply", EditorStyles.toolbarButton))
				DoApply ();

			EditorGUI.EndDisabledGroup();
		}

		private void DoApply ()
		{
			m_TextureImporterSprites.ClearArray();
			for (int i = 0; i < m_RectsCache.Count; i++)
			{
				SpriteRect spriteRect = m_RectsCache.RectAt (i);
				m_TextureImporterSprites.InsertArrayElementAtIndex(i);
				SerializedProperty newRect = m_TextureImporterSprites.GetArrayElementAtIndex (i);
				newRect.FindPropertyRelative ("m_Rect").rectValue = spriteRect.m_Rect;
				newRect.FindPropertyRelative ("m_Name").stringValue = spriteRect.m_Name;
				newRect.FindPropertyRelative ("m_Alignment").intValue = (int)spriteRect.m_Alignment;
				newRect.FindPropertyRelative ("m_Pivot").vector2Value = spriteRect.m_Pivot;
#if ENABLE_SPRITECOLLIDER
				newRect.FindPropertyRelative ("m_ColliderAlphaCutoff").intValue = spriteRect.m_ColliderAlphaCutoff;
				newRect.FindPropertyRelative ("m_ColliderDetail").floatValue = spriteRect.m_ColliderDetail;
#endif
			}
			m_TextureImporterSO.ApplyModifiedProperties ();

			// Usually on postprocess event, we assume things are changed so much that we need to reset. However here we are the one triggering it, so we ignore it.
			m_IgnoreNextPostprocessEvent = true;
			DoTextureReimport (m_TextureImporter.assetPath);
			textureIsDirty = false;
		}

		private void DoRevert ()
		{
			m_TextureIsDirty = false;
			selected = null;
			RefreshRects ();
		}

		private void HandleDuplicate()
		{
			if(Event.current.commandName == "Duplicate" && (Event.current.type == EventType.ValidateCommand || Event.current.type == EventType.ExecuteCommand))
			{
				if (Event.current.type == EventType.ExecuteCommand)
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Duplicate sprite");
#if ENABLE_SPRITECOLLIDER
					selected = AddSprite (selected.m_Rect, (int) selected.m_Alignment, selected.m_ColliderAlphaCutoff, selected.m_ColliderDetail);
#else
					selected = AddSprite (selected.m_Rect, (int) selected.m_Alignment, defaultColliderAlphaCutoff, defaultColliderDetail);
#endif
				}

				Event.current.Use ();
			}
		}

		private void HandleCreate ()
		{
			if (!MouseOnTopOfInspector () && !Event.current.alt)
			{
				// Create new rects via dragging in empty space
				EditorGUI.BeginChangeCheck ();
				Rect newRect = SpriteEditorHandles.RectCreator (m_Texture.width, m_Texture.height, s_Styles.createRect);
				if (EditorGUI.EndChangeCheck () && newRect.width > 0f && newRect.height > 0f)
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Create sprite");
					selected = AddSprite (newRect, 0, defaultColliderAlphaCutoff, defaultColliderDetail);
				}
			}
		}

		private void HandleDelete ()
		{
			if ((Event.current.commandName == "SoftDelete" || Event.current.commandName == "Delete") && (Event.current.type == EventType.ValidateCommand || Event.current.type == EventType.ExecuteCommand))
			{
				if (Event.current.type == EventType.ExecuteCommand)
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Delete sprite");
					
					m_RectsCache.RemoveRect (selected);
					selected = null;
					textureIsDirty = true;
				}

				Event.current.Use ();
			}
		}

		private void HandleDragging ()
		{
			if (selected != null && !MouseOnTopOfInspector())
			{
				Rect textureBounds = new Rect (0, 0, m_Texture.width, m_Texture.height);
				EditorGUI.BeginChangeCheck ();

				SpriteRect spriteRect = selected;
				Rect oldRect = selected.m_Rect;
				Rect newRect = SpriteEditorUtility.ClampedRect (SpriteEditorUtility.RoundedRect (SpriteEditorHandles.SliderRect (oldRect)), textureBounds, true);

				if (EditorGUI.EndChangeCheck ())
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Move sprite");
					spriteRect.m_Rect = newRect;
					textureIsDirty = true;
#if ENABLE_SPRITECOLLIDER
					m_ColliderPreviewAlpha.value = 0f;
#endif
				}
			}
		}

		private void HandleSelection ()
		{
			if (Event.current.type == EventType.MouseDown && Event.current.button == 0 && GUIUtility.hotControl == 0 && !Event.current.alt && !MouseOnTopOfInspector ())
			{
				SpriteRect oldSelected = selected;
	
				selected = TrySelect (Event.current.mousePosition);
				if (selected != null)
					s_OneClickDragStarted = true;

				if (oldSelected != selected && selected != null)
				{
#if ENABLE_SPRITECOLLIDER
					m_ColliderPreviewCache = null;
#endif
					Event.current.Use ();
				}
			}
		}
		
		private void HandleScalingHandles ()
		{
			if (selected != null)
			{
				EditorGUI.BeginChangeCheck ();
				SpriteRect spriteRect = selected;

				Rect newRect = SpriteEditorUtility.RoundedRect (RectEditor (spriteRect.m_Rect));

				if (EditorGUI.EndChangeCheck ())
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Scale sprite");
					spriteRect.m_Rect = newRect;
					textureIsDirty = true;
#if ENABLE_SPRITECOLLIDER
					m_ColliderPreviewAlpha.value = 0f;
#endif
				}
			}
		}

		private void HandlePivotHandle ()
		{
			if (selected != null)
			{
				EditorGUI.BeginChangeCheck ();

				SpriteRect spriteRect = selected;
				spriteRect.m_Pivot = ApplySpriteAlignmentToPivot (spriteRect.m_Pivot, spriteRect.m_Rect, spriteRect.m_Alignment);
				Vector2 pivotHandlePosition = SpriteEditorHandles.PivotSlider (spriteRect.m_Rect, spriteRect.m_Pivot, s_Styles.pivotdot, s_Styles.pivotdotactive);
				
				if (EditorGUI.EndChangeCheck ())
				{
					Undo.RegisterCompleteObjectUndo (m_RectsCache, "Move sprite pivot");
					selected.m_Pivot = SnapPivot (pivotHandlePosition);
					textureIsDirty = true;
#if ENABLE_SPRITECOLLIDER
					showColliderGizmo = false;
#endif
				}
			}
		}

		private Vector2 SnapPivot (Vector2 pivot)
		{
			SpriteRect spriteRect = selected;
			Rect rect = spriteRect.m_Rect;

			// Convert from normalized space to texture space
			Vector2 texturePos = new Vector2 (rect.xMin + rect.width * pivot.x, rect.yMin + rect.height * pivot.y);

			Vector2[] snapPoints = GetSnapPointsArray (rect);
			
			bool snapped = false;
			for (int alignment = 0; alignment < snapPoints.Length; alignment++)
			{
				if ((texturePos - snapPoints[alignment]).magnitude * m_Zoom < maxSnapDistance)
				{
					texturePos = snapPoints[alignment];
					selected.m_Alignment = (SpriteAlignment)alignment;
					snapped = true;
					break;
				}
			}

			if(!snapped)
				selected.m_Alignment = SpriteAlignment.Custom;

			// Convert from texture space back to normalized space
			return ConvertFromTextureToNormalizedSpace (texturePos, rect);
		}

		public Vector2 ApplySpriteAlignmentToPivot (Vector2 pivot, Rect rect, SpriteAlignment alignment)
		{
			Vector2[] snapPoints = GetSnapPointsArray (rect);
			
			if (alignment != SpriteAlignment.Custom)
			{
				Vector2 texturePos = snapPoints[(int) alignment];
				return ConvertFromTextureToNormalizedSpace (texturePos, rect);
			}
			return pivot;
		}

		private Vector2 ConvertFromTextureToNormalizedSpace (Vector2 texturePos, Rect rect)
		{
			return new Vector2 ((texturePos.x - rect.xMin) / rect.width, (texturePos.y - rect.yMin) / rect.height);
		}

		private Vector2[] GetSnapPointsArray(Rect rect)
		{
			Vector2[] snapPoints = new Vector2[9];
			snapPoints[(int)SpriteAlignment.TopLeft] = new Vector2 (rect.xMin, rect.yMax);
			snapPoints[(int)SpriteAlignment.TopCenter] = new Vector2 (rect.center.x, rect.yMax);
			snapPoints[(int)SpriteAlignment.TopRight] = new Vector2 (rect.xMax, rect.yMax);
			snapPoints[(int)SpriteAlignment.LeftCenter] = new Vector2 (rect.xMin, rect.center.y);
			snapPoints[(int)SpriteAlignment.Center] = new Vector2 (rect.center.x, rect.center.y);
			snapPoints[(int)SpriteAlignment.RightCenter] = new Vector2 (rect.xMax, rect.center.y);
			snapPoints[(int)SpriteAlignment.BottomLeft] = new Vector2 (rect.xMin, rect.yMin);
			snapPoints[(int)SpriteAlignment.BottomCenter] = new Vector2 (rect.center.x, rect.yMin);
			snapPoints[(int)SpriteAlignment.BottomRight] = new Vector2 (rect.xMax, rect.yMin);
			return snapPoints;
		}

		void UpdateSelectedSprite()
		{
			if (Selection.activeObject is Sprite)
				SelectSpriteIndex (Selection.activeObject as Sprite);
			else if (Selection.activeGameObject != null && Selection.activeGameObject.GetComponent<SpriteRenderer> ())
			{
				Sprite sprite = Selection.activeGameObject.GetComponent<SpriteRenderer> ().sprite;
				SelectSpriteIndex (sprite);
			}
		}

		void SelectSpriteIndex(Sprite sprite)
		{
			if (sprite == null)
				return;

			selected = null;

			for (int i = 0; i < m_RectsCache.Count; i++)
			{
				if (sprite.rect == m_RectsCache.RectAt (i).m_Rect)
				{
					selected = m_RectsCache.RectAt (i);
					return;
				}
			}
		}

		private Texture2D GetSelectedTexture2D ()
		{
			Texture2D texture = null;

			if (Selection.activeObject is Texture2D)
			{
				texture = Selection.activeObject as Texture2D;
			}
			else if (Selection.activeObject is Sprite)
			{
				texture = UnityEditor.Sprites.DataUtility.GetSpriteTexture(Selection.activeObject as Sprite, false);
			}
			else if (Selection.activeGameObject)
			{
				if (Selection.activeGameObject.GetComponent<SpriteRenderer>())
				{
					if (Selection.activeGameObject.GetComponent<SpriteRenderer> ().sprite)
					{
						texture = UnityEditor.Sprites.DataUtility.GetSpriteTexture(Selection.activeGameObject.GetComponent<SpriteRenderer>().sprite, false);
					}
				}
			}

			if (texture != null)
				m_SelectedAssetPath = AssetDatabase.GetAssetPath (texture);
			
			return texture;
		}

		protected override void DrawGizmos ()
		{
			SpriteEditorUtility.BeginLines (new Color (0f, 0f, 0f, 0.25f));
			for (int i = 0; i < m_RectsCache.Count; i++)
			{
				Rect r = m_RectsCache.RectAt(i).m_Rect;
				if (m_RectsCache.RectAt (i) != selected)
					SpriteEditorUtility.DrawBox (new Rect (r.xMin + 1f / m_Zoom, r.yMin + 1f / m_Zoom, r.width, r.height));
			}
			SpriteEditorUtility.EndLines ();

			SpriteEditorUtility.BeginLines (new Color (1f, 1f, 1f, 0.5f));
			for (int i = 0; i < m_RectsCache.Count; i++)
			{
				if (m_RectsCache.RectAt (i) != selected)
					SpriteEditorUtility.DrawBox (m_RectsCache.RectAt(i).m_Rect);
			}
			SpriteEditorUtility.EndLines ();

			if (selected != null)
			{
				Rect r = selected.m_Rect;
				SpriteEditorUtility.BeginLines (new Color (0f, 0.1f, 0.3f, 0.25f));
				SpriteEditorUtility.DrawBox (new Rect (r.xMin + 1f / m_Zoom, r.yMin + 1f / m_Zoom, r.width, r.height));
				SpriteEditorUtility.EndLines ();
				SpriteEditorUtility.BeginLines (new Color (0.25f, 0.5f, 1f, 0.75f));
				SpriteEditorUtility.DrawBox (r);
				SpriteEditorUtility.EndLines ();
			}

#if ENABLE_SPRITECOLLIDER
			if (selected != null && showColliderGizmo)
			{
				if(m_ColliderPreviewCache == null)
					Sprites.DataUtility.GenerateOutline (m_Texture, selected.m_Rect, selected.m_ColliderDetail, (byte)selected.m_ColliderAlphaCutoff, true, out m_ColliderPreviewCache);

				if(m_ColliderPreviewCache != null)
				{
					SpriteEditorUtility.BeginLines (new Color (0f, 1f, 0f, m_ColliderPreviewAlpha.value));

					Vector2 offset = new Vector2 (selected.m_Rect.xMin + selected.m_Rect.width * .5f, selected.m_Rect.yMin + selected.m_Rect.height * .5f);

					foreach (Vector2[] poly in m_ColliderPreviewCache)
					{
						for (int i = 0; i < poly.Length - 1; i++)
						{
							Vector2 p1 = poly[i] + offset;
							Vector2 p2 = poly[i + 1] + offset;
							SpriteEditorUtility.DrawLine (p1, p2);
						}
						SpriteEditorUtility.DrawLine (poly[poly.Length-1] + offset, poly[0] + offset);
					}

					SpriteEditorUtility.EndLines ();
				}
			}
#endif
		}

		private SpriteRect TrySelect (Vector2 mousePosition)
		{
			float selectedSize = 10000000f;
			SpriteRect currentRect = null;

			for (int i = 0; i < m_RectsCache.Count; i++)
			{
				if (m_RectsCache.RectAt(i).m_Rect.Contains (Handles.s_InverseMatrix.MultiplyPoint (mousePosition)))
				{
					// If we clicked inside an already selected spriterect, always persist that selection
					if (m_RectsCache.RectAt (i) == selected)
						return m_RectsCache.RectAt (i);

					float width = m_RectsCache.RectAt(i).m_Rect.width;
					float height = m_RectsCache.RectAt(i).m_Rect.height;
					float newSize = width * height;
					if (width > 0f && height > 0f && newSize < selectedSize)
					{
						currentRect = m_RectsCache.RectAt (i);
						selectedSize = newSize;
					}
				}
			}

			return currentRect;
		}

		
        public SpriteRect AddSprite(Rect rect, int alignment, int colliderAlphaCutoff, float colliderDetail)
		{
			SpriteRect spriteRect = new SpriteRect();
	        int index = m_RectsCache.Count;

			spriteRect.m_Rect = rect;
	        spriteRect.m_Alignment = (SpriteAlignment)alignment;

#if ENABLE_SPRITECOLLIDER
	        spriteRect.m_ColliderDetail = colliderDetail;
	        spriteRect.m_ColliderAlphaCutoff = colliderAlphaCutoff;
#endif

			string prefix = Path.GetFileNameWithoutExtension((m_TextureImporter).assetPath);
			spriteRect.m_Name = prefix + "_" + index;
			textureIsDirty = true;

			m_RectsCache.AddRect (spriteRect);
			return spriteRect;
		}

		private Rect TrimAlpha (Rect rect)
		{
			int xMin = (int)rect.xMax;
			int xMax = (int)rect.xMin;
			int yMin = (int)rect.yMax;
			int yMax = (int)rect.yMin;

			for (int y = (int)rect.yMin; y < (int)rect.yMax; y++)
			{
				for (int x = (int)rect.xMin; x < (int)rect.xMax; x++)
				{
					if (PixelHasAlpha(x, y))
					{
						xMin = Mathf.Min(xMin, x);
						xMax = Mathf.Max(xMax, x);
						yMin = Mathf.Min(yMin, y);
						yMax = Mathf.Max(yMax, y);
					}
				}
			}
			return new Rect(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
		}

		public void DoTextureReimport (string path)
		{
			if (m_TextureImporterSO != null)
			{
				try
				{
					AssetDatabase.StartAssetEditing ();
					AssetDatabase.ImportAsset (path);
				}
				finally
				{
					AssetDatabase.StopAssetEditing ();
				}
				textureIsDirty = false;
			}
		}

        private Rect RectEditor (Rect rect)
		{
			float handleSize = s_Styles.dragdot.fixedWidth;

			Vector2 screenRectTopLeft = Handles.matrix.MultiplyPoint (new Vector3 (rect.xMin, rect.yMin));
			Vector2 screenRectBottomRight = Handles.matrix.MultiplyPoint (new Vector3 (rect.xMax, rect.yMax));
			float screenRectWidth = Mathf.Abs (screenRectBottomRight.x - screenRectTopLeft.x);
			float screenRectHeight = Mathf.Abs (screenRectBottomRight.y - screenRectTopLeft.y);
			float lineCursorRectSize = handleSize * .66f;

			Vector2 left = new Vector2 (rect.xMin, rect.center.y);
			Vector2 right = new Vector2 (rect.xMax, rect.center.y);
			Vector2 top = new Vector2 (rect.center.x, rect.yMin);
			Vector2 bottom = new Vector2 (rect.center.x, rect.yMax);

	        Rect cursorRect = new Rect (screenRectTopLeft.x - lineCursorRectSize * .5f, screenRectBottomRight.y + handleSize * .5f, lineCursorRectSize, screenRectHeight - handleSize);
			left = SpriteEditorHandles.ScaleSlider (left, Vector3.left, MouseCursor.ResizeHorizontal, false, cursorRect, s_Styles.dragdot, s_Styles.dragdotactive);
			cursorRect = new Rect (screenRectBottomRight.x - lineCursorRectSize * .5f, screenRectBottomRight.y + handleSize * .5f, lineCursorRectSize, screenRectHeight - handleSize);
			right = SpriteEditorHandles.ScaleSlider (right, Vector3.right, MouseCursor.ResizeHorizontal, false, cursorRect, s_Styles.dragdot, s_Styles.dragdotactive);
			cursorRect = new Rect (screenRectTopLeft.x + lineCursorRectSize * .5f, screenRectTopLeft.y - handleSize * .5f, screenRectWidth - handleSize, lineCursorRectSize);
			top = SpriteEditorHandles.ScaleSlider (top, Vector3.up, MouseCursor.ResizeVertical, false, cursorRect, s_Styles.dragdot, s_Styles.dragdotactive);
			cursorRect = new Rect (screenRectTopLeft.x + lineCursorRectSize * .5f, screenRectBottomRight.y - handleSize * .5f, screenRectWidth - handleSize, lineCursorRectSize);
			bottom = SpriteEditorHandles.ScaleSlider (bottom, Vector3.right, MouseCursor.ResizeVertical, false, cursorRect, s_Styles.dragdot, s_Styles.dragdotactive);
			
			rect.xMin = Mathf.Min (left.x, rect.xMax - 1);
			rect.xMax = Mathf.Max (rect.xMin + 1, right.x);
			rect.yMin = Mathf.Min (top.y, rect.yMax - 1);
			rect.yMax = Mathf.Max (rect.yMin + 1, bottom.y);

			Vector2 topleft = new Vector2 (rect.xMin, rect.yMin);
			topleft = SpriteEditorHandles.ScaleSlider (topleft, Vector3.left + Vector3.up, MouseCursor.ResizeUpRight, true, new Rect (), s_Styles.dragdot, s_Styles.dragdotactive);
			Vector2 topright = new Vector2 (rect.xMax, topleft.y);
			topright = SpriteEditorHandles.ScaleSlider (topright, Vector3.right + Vector3.up, MouseCursor.ResizeUpLeft, true, new Rect (), s_Styles.dragdot, s_Styles.dragdotactive);
			Vector2 bottomright = new Vector2 (topright.x, rect.yMax);
			bottomright = SpriteEditorHandles.ScaleSlider (bottomright, Vector3.right + Vector3.down, MouseCursor.ResizeUpRight, true, new Rect (), s_Styles.dragdot, s_Styles.dragdotactive);
			Vector2 bottomleft = new Vector2 (topleft.x, bottomright.y);
			bottomleft = SpriteEditorHandles.ScaleSlider (bottomleft, Vector3.left + Vector3.down, MouseCursor.ResizeUpLeft, true, new Rect (), s_Styles.dragdot, s_Styles.dragdotactive);

			rect.xMin = Mathf.Min (bottomleft.x, rect.xMax - 1);
			rect.yMin = Mathf.Min (topright.y, rect.yMax - 1);
			rect.xMax = Mathf.Max (rect.xMin + 1, bottomright.x);
			rect.yMax = Mathf.Max (rect.yMin + 1, bottomleft.y);

			Rect clamp = new Rect (0, 0, m_Texture.width, m_Texture.height);
			rect = SpriteEditorUtility.ClampedRect (rect, clamp, false);

			return rect;
		}

		public void DoAutomaticSlicing(int minimumSpriteSize, int alignment, AutoSlicingMethod slicingMethod)
		{
			Undo.RegisterCompleteObjectUndo(m_RectsCache, "Automatic Slicing");

			if (slicingMethod == AutoSlicingMethod.DeleteAll)
				m_RectsCache.ClearAll();

			List<Rect> frames = new List<Rect>(InternalSpriteUtility.GenerateAutomaticSpriteRectangles (m_Texture, minimumSpriteSize, 0));
			frames = SortRects (frames);
			
			foreach (Rect frame in frames)
				AddSprite (frame, alignment, slicingMethod);

			selected = null;
			textureIsDirty = true;
			Repaint();
		}

		public void DoGridSlicing(int spriteWidth, int spriteHeight, int alignment)
		{
			Undo.RegisterCompleteObjectUndo(m_RectsCache, "Grid Slicing");
			
			m_RectsCache.ClearAll();

			Rect[] frames = InternalSpriteUtility.GenerateGridSpriteRectangles(m_Texture, 0, 0, spriteWidth, spriteHeight, 0);

			foreach (Rect frame in frames)
				AddSprite(frame, alignment, defaultColliderAlphaCutoff, defaultColliderDetail);

			selected = null;
			textureIsDirty = true;
			Repaint();
		}

		// Aurajoki-Sweep Rect Sorting(tm)
		// 1. Find top-most rectangle
		// 2. Sweep it vertically to find out all rects from that "row"
		// 3. goto 1.
		// This will give us nicely sorted left->right top->down list of rectangles
		// Works for most sprite sheets pretty nicely
		List<Rect> SortRects (List<Rect> rects)
		{
			List<Rect> result = new List<Rect>();

			while (rects.Count > 0)
			{
				// Because the slicing algorithm works from bottom-up, the topmost rect is the last one in the array
				Rect r = rects[rects.Count-1];
				Rect sweepRect = new Rect (0, r.yMin, m_Texture.width, r.height);

				List<Rect> rowRects = RectSweep (rects, sweepRect);

				if (rowRects.Count > 0)
					result.AddRange (rowRects);
				else
				{
					// We didn't find any rects, just dump the remaining rects and continue
					result.AddRange (rects);
					break;
				}
			}
			return result;
		}

		List<Rect> RectSweep (List<Rect> rects, Rect sweepRect)
		{
			if (rects == null || rects.Count == 0)
				return new List<Rect>();

			List<Rect> containedRects = new List<Rect>();

			foreach (Rect rect in rects)
				if (Overlap (rect, sweepRect))
					containedRects.Add (rect);

			// Remove found rects from original list
			foreach (Rect rect in containedRects)
				rects.Remove (rect);

			// Sort found rects by x position
			containedRects.Sort ((a, b) => a.x.CompareTo (b.x));

			return containedRects;
		}

		private void AddSprite (Rect frame, int alignment, AutoSlicingMethod slicingMethod)
		{
			if (slicingMethod != AutoSlicingMethod.DeleteAll)
			{
				// Smart: Whenever we overlap, we just modify the existing rect and keep its other properties
				// Safe: We only add new rect if it doesn't overlap existing one
				
				SpriteRect existingSprite = GetExistingOverlappingSprite (frame);
				if (existingSprite != null)
				{
					if (slicingMethod == AutoSlicingMethod.Smart)
					{
						existingSprite.m_Rect = frame;
						existingSprite.m_Alignment = (SpriteAlignment)alignment;
					}
				}
				else
					AddSprite (frame, alignment, defaultColliderAlphaCutoff, defaultColliderDetail);
			}
			else
				AddSprite (frame, alignment, defaultColliderAlphaCutoff, defaultColliderDetail);
		}

		private SpriteRect GetExistingOverlappingSprite (Rect rect)
		{
			for (int i = 0; i < m_RectsCache.Count; i++)
			{
				Rect existingRect = m_RectsCache.RectAt(i).m_Rect;
				if (Overlap (existingRect, rect))
				{
					return m_RectsCache.RectAt (i);
				}
			}
			return null;
		}

		private bool Overlap (Rect a, Rect b)
		{
			return a.xMin < b.xMax && a.xMax > b.xMin && a.yMin < b.yMax && a.yMax > b.yMin;
		}

		private bool MouseOnTopOfInspector ()
		{
			if (selected == null)
				return false;

			Vector2 mousePosition = GUIClip.Unclip (Event.current.mousePosition);
			// TODO: Find out why there is 22px offset
			mousePosition += new Vector2(0f, -22f);
			return inspectorRect.Contains (mousePosition);
		}

		private bool PixelHasAlpha (int x, int y)
		{
			if (m_Texture == null)
				return false;

			if (s_AlphaPixelCache == null)
			{
				s_AlphaPixelCache = new bool[m_Texture.width * m_Texture.height];
				Color32[] pixels = m_Texture.GetPixels32 ();

				for (int i = 0; i < pixels.Length; i++)
					s_AlphaPixelCache[i] = pixels[i].a != 0;
			}
			int index = y * (int)m_Texture.width + x;
			return s_AlphaPixelCache[index];
		}

		
		Texture2D CreateTemporaryDuplicate(Texture2D original, int width, int height)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			{
				return null;
			}

			EditorUtility.SetTemporarilyAllowIndieRenderTexture (true);
			RenderTexture save = RenderTexture.active;

			RenderTexture tmp = RenderTexture.GetTemporary (
									width,
									height,
									0,
									RenderTextureFormat.Default,
									RenderTextureReadWrite.Linear);

			Graphics.Blit (original, tmp);

			UnityEngine.RenderTexture.active = tmp;

			Texture2D copy = new Texture2D (width, height, TextureFormat.ARGB32, false);
			copy.ReadPixels (new Rect (0, 0, width, height), 0, 0);
			copy.Apply ();
			RenderTexture.ReleaseTemporary (tmp);

			EditorGUIUtility.SetRenderTextureNoViewport (save);
			EditorUtility.SetTemporarilyAllowIndieRenderTexture (false);

			copy.alphaIsTransparency = original.alphaIsTransparency;
			return copy;
		}
	}

	internal class SpriteEditorTexturePostprocessor : AssetPostprocessor
	{
		public override int GetPostprocessOrder ()
		{
			return 1;
		}

		public void OnPostprocessTexture (Texture2D tex)
		{
			if (SpriteEditorWindow.s_Instance != null)
			{
				if (assetPath.Equals (SpriteEditorWindow.s_Instance.m_SelectedAssetPath))
				{
					if (!SpriteEditorWindow.s_Instance.m_IgnoreNextPostprocessEvent)
					{
						SpriteEditorWindow.s_Instance.m_ResetOnNextRepaint = true;
						SpriteEditorWindow.s_Instance.Repaint ();
					}
					else
					{
						SpriteEditorWindow.s_Instance.m_IgnoreNextPostprocessEvent = false;
					}
				}	
			}
		}
	}

}

#endif
