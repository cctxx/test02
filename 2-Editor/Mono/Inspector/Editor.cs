using System;
using UnityEngine;

using Object = UnityEngine.Object;

namespace UnityEditor
{
public partial class Editor : ScriptableObject
{
	static Styles s_Styles;

	private const float kImageSectionWidth = 44;

	const int kPreviewLabelHeight = 12;
	const int kPreviewMinSize = 55;
	const int kGridTargetCount = 25;
	const int kGridSpacing = 10;
	const int kPreviewLabelPadding = 5;
	
	class Styles
	{
		public GUIStyle inspectorBig = new GUIStyle (EditorStyles.inspectorBig);
		public GUIStyle inspectorBigInner = new GUIStyle ("IN BigTitle inner");
		public GUIStyle centerStyle = new GUIStyle ();
		public GUIStyle preBackground = "preBackground";
		public GUIStyle preBackgroundSolid = new GUIStyle ("preBackground");
		public GUIStyle previewMiniLabel = new GUIStyle (EditorStyles.whiteMiniLabel);
		public GUIStyle dropShadowLabelStyle = new GUIStyle ("PreOverlayLabel");
		
		public Styles ()
		{
			centerStyle.alignment = TextAnchor.MiddleCenter;
			preBackgroundSolid.overflow = preBackgroundSolid.border;
			previewMiniLabel.alignment = TextAnchor.UpperCenter;
			inspectorBig.padding.bottom -= 1;
		}
	}
	
	internal static bool DoDrawDefaultInspector(SerializedObject obj)
	{
		EditorGUI.BeginChangeCheck();
		obj.Update();

		// Loop through properties and create one field (including children) for each top level property.
		SerializedProperty property = obj.GetIterator();
		bool expanded = true;
		while (property.NextVisible(expanded))
		{
			EditorGUILayout.PropertyField(property, true);
			expanded = false;
		}

		obj.ApplyModifiedProperties();
		return EditorGUI.EndChangeCheck();
	}

	internal bool DoDrawDefaultInspector ()
	{
		return DoDrawDefaultInspector(serializedObject);
	}
	
	// This is the method that should be called from externally e.g. myEditor.DrawHeader ();
	// Do not make this method virtual - override OnHeaderGUI instead.
	public void DrawHeader ()
	{
		// If we call DrawHeader from inside an an editor's OnInspectorGUI call, we have to do special handling.
		// (See DrawHeaderFromInsideHierarchy for details.)
		// We know we're inside the OnInspectorGUI block (or a similar vertical block) if hierarchyMode is set to true.
		if (EditorGUIUtility.hierarchyMode)
			DrawHeaderFromInsideHierarchy ();
		else
			OnHeaderGUI ();
	}
	
	// This is the method to override to create custom header GUI.
	// Do not make this method internal or public - call DrawHeader instead.
	protected virtual void OnHeaderGUI ()
	{
		DrawHeaderGUI (this, targetTitle);
	}
	
	internal virtual void OnHeaderControlsGUI ()
	{
		// We could maybe show the type on line two per default?
		//GUILayout.Label (ObjectNames.NicifyVariableName (ObjectNames.GetTypeName (target)), EditorStyles.miniLabel);
		
		// Ensure we take up the same amount of height as regular controls
		GUILayoutUtility.GetRect (10, 10, 16, 16, EditorStyles.layerMaskField);
		
		GUILayout.FlexibleSpace ();
		
		bool showOpenButton = true;
		if (!(this is AssetImporterInspector))
		{
			// Don't show open button if the target is not an asset
			if (!AssetDatabase.IsMainAsset (targets[0]))
				showOpenButton = false;
			// Don't show open button if the target has an importer
			// (but ignore AssetImporters since they're not shown)
			AssetImporter importer = AssetImporter.GetAtPath (AssetDatabase.GetAssetPath (targets[0]));
			if (importer && importer.GetType () != typeof (AssetImporter))
				showOpenButton = false;
		}
		
		if (showOpenButton)
		{
			if (GUILayout.Button ("Open", EditorStyles.miniButton))
			{
				if (this is AssetImporterInspector)
					AssetDatabase.OpenAsset ((this as AssetImporterInspector).assetEditor.targets);
				else
					AssetDatabase.OpenAsset (targets);
				GUIUtility.ExitGUI ();
			}
		}
	}
	
	internal virtual void OnHeaderIconGUI (Rect iconRect)
	{
		if (s_Styles == null)
			s_Styles = new Styles ();
		
		Texture2D icon = null;
		if (!HasPreviewGUI ())
		{
			//  Fetch isLoadingAssetPreview to ensure that there is no situation where a preview needs a repaint because it hasn't finished loading yet.
			bool isLoadingAssetPreview = AssetPreview.IsLoadingAssetPreview (target.GetInstanceID());
			icon = AssetPreview.GetAssetPreview (target);
			if (!icon)
			{
				// We have a static preview it just hasn't been loaded yet. Repaint until we have it loaded.
				if (isLoadingAssetPreview)
					Repaint ();
				icon = AssetPreview.GetMiniThumbnail (target);
			}
		}
		
		if (HasPreviewGUI ())
			// OnPreviewGUI must have all events; not just Repaint, or else the control IDs will mis-match.
			OnPreviewGUI (iconRect, s_Styles.inspectorBigInner);
		else if (icon)
			GUI.Label (iconRect, icon, s_Styles.centerStyle);
	}
	
	internal virtual void OnHeaderTitleGUI (Rect titleRect, string header)
	{
		titleRect.yMin -= 2;
		titleRect.yMax += 2;
		GUI.Label (titleRect, header, EditorStyles.largeLabel);
	}
	
	internal virtual void DrawHeaderHelpAndSettingsGUI (Rect r)
	{
		if (EditorGUI.s_HelpIcon == null) {
			EditorGUI.s_HelpIcon = EditorGUIUtility.IconContent ("_Help");
			EditorGUI.s_TitleSettingsIcon = EditorGUIUtility.IconContent ("_Popup");
		}
		
		// Help
		Object helpObject = target;
			
		// Show help if available.
		EditorGUI.HelpIconButton (new Rect (r.xMax-36, r.y+5, 14, 14), helpObject);
		
		// Settings
		EditorGUI.BeginDisabledGroup (!IsEnabled ());
		Rect settingsRect = new Rect (r.xMax-18, r.y+5, 14, 14);
		if (EditorGUI.ButtonMouseDown (settingsRect, EditorGUI.s_TitleSettingsIcon, FocusType.Native, EditorStyles.inspectorTitlebarText))
			EditorUtility.DisplayObjectContextMenu (settingsRect, targets, 0);
		EditorGUI.EndDisabledGroup ();
	}
	
	// If we call DrawHeaderGUI from inside an an editor's OnInspectorGUI call, we have to do special handling.
	// Since OnInspectorGUI is wrapped inside a BeginVertical/EndVertical block that adds padding,
	// and we don't want this padding for headers, we have to stop the vertical block,
	// draw the header, and then start a new vertical block with the same style.
	private void DrawHeaderFromInsideHierarchy ()
	{
		GUIStyle style = GUILayoutUtility.current.topLevel.style;
		EditorGUILayout.EndVertical ();
		OnHeaderGUI ();
		EditorGUILayout.BeginVertical (style);
	}
	
	internal static Rect DrawHeaderGUI (Editor editor, string header)
	{
		if (s_Styles == null)
			s_Styles = new Styles ();
		
		GUILayout.BeginHorizontal (s_Styles.inspectorBig);
		GUILayout.Space (kImageSectionWidth-6);
		GUILayout.BeginVertical ();
		GUILayout.Space (19);
		GUILayout.BeginHorizontal ();
		if (editor)
			editor.OnHeaderControlsGUI ();
		else
			EditorGUILayout.GetControlRect ();
		GUILayout.EndHorizontal ();
		GUILayout.EndVertical ();
		GUILayout.EndHorizontal ();
		Rect r = GUILayoutUtility.GetLastRect ();
		
		float width = r.width;
		
		// Icon
		Rect iconRect = new Rect (r.x+6, r.y+6, 32, 32);
		
		if (editor)
			editor.OnHeaderIconGUI (iconRect);
		else
			GUI.Label (iconRect, AssetPreview.GetMiniTypeThumbnail (typeof (Object)), s_Styles.centerStyle);
		
		// Title
		Rect titleRect = new Rect (r.x+kImageSectionWidth, r.y+6, width-kImageSectionWidth-38-4, 16);
		if (editor)
			editor.OnHeaderTitleGUI (titleRect, header);
		else
			GUI.Label (titleRect, header, EditorStyles.largeLabel);
		
		// Help and Settings
		if (editor)
			editor.DrawHeaderHelpAndSettingsGUI (r);
		
		// Context Menu
		Event evt = Event.current;
		if (editor != null && !editor.IsEnabled () &&
			evt.type == EventType.MouseDown && evt.button == 1 && r.Contains (evt.mousePosition)) 
		{
			EditorUtility.DisplayObjectContextMenu (new Rect (evt.mousePosition.x, evt.mousePosition.y, 0, 0), editor.targets, 0);
			evt.Use ();
		}
		
		return r;
	}

	internal virtual void DrawPreview (Rect previewPosition)
	{
		if (s_Styles == null)
			s_Styles = new Styles ();

		string text = string.Empty;
		Event evt = Event.current;

		// If multiple targets, draw a grid of previews
		if (targets.Length > 1)
		{
			// Draw the previews inside the region of the background that's solid colored
			Rect previewPositionInner = new RectOffset (16, 16, 20, 25).Remove (previewPosition);

			// Number of previews to aim at
			int maxRows = Mathf.Max (1, Mathf.FloorToInt ((previewPositionInner.height + kGridSpacing) / (kPreviewMinSize + kGridSpacing + kPreviewLabelHeight)));
			int maxCols = Mathf.Max (1, Mathf.FloorToInt ((previewPositionInner.width + kGridSpacing) / (kPreviewMinSize + kGridSpacing)));
			int countWithMinimumSize = maxRows * maxCols;
			int neededCount = Mathf.Min (targets.Length, kGridTargetCount);

			// Get number of columns and rows
			bool fixedSize = true;
			int[] division = new int[2] { maxCols, maxRows };
			if (neededCount < countWithMinimumSize)
			{
				division = GetGridDivision (previewPositionInner, neededCount, kPreviewLabelHeight);
				fixedSize = false;
			}

			// The available cells in the grid may be slightly higher than what was aimed at.
			// If the number of targets is also higher, we might as well fill in the remaining cells.
			int count = Mathf.Min (division[0] * division[1], targets.Length);

			// Calculations become simpler if we add one spacing to the width and height,
			// so there is the same number of spaces and previews.
			previewPositionInner.width += kGridSpacing;
			previewPositionInner.height += kGridSpacing;

			Vector2 cellSize = new Vector2 (
				Mathf.FloorToInt (previewPositionInner.width / division[0] - kGridSpacing),
				Mathf.FloorToInt (previewPositionInner.height / division[1] - kGridSpacing)
			);
			float previewSize = Mathf.Min (cellSize.x, cellSize.y - kPreviewLabelHeight);
			if (fixedSize)
				previewSize = Mathf.Min (previewSize, kPreviewMinSize);

			bool selectingOne = (evt.type == EventType.mouseDown && evt.button == 0 && evt.clickCount == 2 &&
				previewPosition.Contains (evt.mousePosition));

			int oldIndex = referenceTargetIndex;
			for (int i = 0; i < count; i++)
			{
				referenceTargetIndex = i;
				Rect r = new Rect (
					previewPositionInner.x + (i % division[0]) * previewPositionInner.width / division[0],
					previewPositionInner.y + (i / division[0]) * previewPositionInner.height / division[1],
					cellSize.x,
					cellSize.y
				);

				if (selectingOne && r.Contains (Event.current.mousePosition))
					Selection.objects = new Object[] { target };

				// Make room for label underneath
				r.height -= kPreviewLabelHeight;
				// Make preview square
				Rect rSquare = new Rect (r.x + (r.width - previewSize) * 0.5f, r.y + (r.height - previewSize) * 0.5f, previewSize, previewSize);

				// Draw preview inside a group to prevent overdraw
				// @TODO: Make style with solid color that doesn't have overdraw
				GUI.BeginGroup (rSquare);
				Editor.m_AllowMultiObjectAccess = false;
				OnInteractivePreviewGUI (new Rect (0, 0, previewSize, previewSize), s_Styles.preBackgroundSolid);
				Editor.m_AllowMultiObjectAccess = true;
				GUI.EndGroup ();

				// Draw the name of the object
				r.y = rSquare.yMax;
				r.height = 16;
				GUI.Label (r, target.name, s_Styles.previewMiniLabel);
			}
			referenceTargetIndex = oldIndex;

			if (Event.current.type == EventType.Repaint)
				text = "Previewing " + count + " of " + targets.Length + " Objects";
		}

		// If only a single target, just draw that one
		else
		{
			// Ironically, only allow multi object access inside OnPreviewGUI if editor does NOT support multi-object editing.
			// since there's no harm in going through the serializedObject there if there's always only one target.
			Editor.m_AllowMultiObjectAccess = !canEditMultipleObjects;
			OnInteractivePreviewGUI (previewPosition, s_Styles.preBackground);
			Editor.m_AllowMultiObjectAccess = true;

			if (Event.current.type == EventType.Repaint)
			{
				// TODO: This should probably be calculated during import and stored together with the asset somehow. Or maybe not. Not sure, really...
				text = GetInfoString ();
				if (text != string.Empty)
				{
					text = text.Replace ("\n", "   ");
					text = targetTitle + "\n" + text;
				}
			}
		}

		// Draw the asset info.
		if (Event.current.type == EventType.Repaint && text != string.Empty)
		{
			var textHeight = s_Styles.dropShadowLabelStyle.CalcHeight (GUIContent.Temp (text), previewPosition.width);
			EditorGUI.DropShadowLabel (new Rect (previewPosition.x, previewPosition.yMax - textHeight - kPreviewLabelPadding, previewPosition.width, textHeight), text);
		}
	}

	// Get the number or columns and rows for a grid with a certain minimum number of cells
	// such that the cells are as close to square as possible.
	private int[] GetGridDivision (Rect rect, int minimumNr, int labelHeight)
	{
		// The edge size of a square calculated based on area
		float approxSize = Mathf.Sqrt (rect.width * rect.height / minimumNr);
		int xCount = Mathf.FloorToInt (rect.width / approxSize);
		int yCount = Mathf.FloorToInt (rect.height / (approxSize + labelHeight));
		// This heuristic is not entirely optimal and could probably be improved
		while (xCount * yCount < minimumNr)
		{
			float ratioIfXInc = AbsRatioDiff ((xCount + 1) / rect.width, yCount / (rect.height - yCount * labelHeight));
			float ratioIfYInc = AbsRatioDiff (xCount / rect.width, (yCount + 1) / (rect.height - (yCount + 1) * labelHeight));
			if (ratioIfXInc < ratioIfYInc)
			{
				xCount++;
				if (xCount * yCount > minimumNr)
					yCount = Mathf.CeilToInt ((float)minimumNr / xCount);
			}
			else
			{
				yCount++;
				if (xCount * yCount > minimumNr)
					xCount = Mathf.CeilToInt ((float)minimumNr / yCount);
			}
		}
		return new int[] { xCount, yCount };
	}

	private float AbsRatioDiff (float x, float y)
	{
		return Mathf.Max (x / y, y / x);
	}
	
	internal virtual bool IsEnabled ()
	{
		// disable editor if any objects in the editor are not editable
		foreach (Object target in targets)
		{
			if ((target.hideFlags & HideFlags.NotEditable) != 0)
				return false;

			if (EditorUtility.IsPersistent(target) && !AssetDatabase.IsOpenForEdit(target))
				return false;
		}

		return true;
	}

	internal bool IsOpenForEdit ()
	{
		string message;
		return IsOpenForEdit (out message);
	}

	internal bool IsOpenForEdit (out string message)
	{
		message = "";

		foreach (Object target in targets)
		{
			if (EditorUtility.IsPersistent(target) && !AssetDatabase.IsOpenForEdit(target, out message))
				return false;
		}
		
		return true;
	}
	
	public virtual bool UseDefaultMargins ()
	{
		return true;
	}
}	
}

