using System;
using UnityEditor;
using UnityEngine;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditorInternal
{
	internal class AnimationWindowHierarchyGUI : TreeViewGUI
	{
		public AnimationWindowState state { get; set; }
		GUIStyle plusButtonStyle;
		GUIStyle animationRowEvenStyle;
		GUIStyle animationRowOddStyle;
		GUIStyle animationSelectionTextField;
		GUIStyle animationLineStyle;
		GUIStyle animationCurveDropdown;

		private Color m_LightSkinPropertyTextColor = new Color (.35f, .35f, .35f);

		private const float k_RowRightOffset = 10;	
		private const float k_PlusButtonWidth = 17;
		private const float k_TallModeButtonWidth = 17;
		private const float k_ValueFieldWidth = 50;
		private const float k_ValueFieldOffsetFromRightSide = 75;
		private const float k_ColorIndicatorTopMargin = 3;
		
		// Colors can't be real const, but are still meant as const
		private static Color k_KeyColorInDopesheetMode = new Color (0.7f, 0.7f, 0.7f, 1);
		private static Color k_KeyColorForNonCurves = new Color (0.7f, 0.7f, 0.7f, 0.5f);

		public const float k_DopeSheetRowHeight = 16f;
		public const float k_DopeSheetRowHeightTall = 32f;
		public const float k_AddCurveButtonNodeHeight = 40f;

		public const float k_RowBackgroundColorBrightness = 0.28f;		
		internal static int s_WasInsideValueRectFrame = -1;

		public AnimationWindowHierarchyGUI (TreeView treeView, AnimationWindowState state)
			: base (treeView)
		{
			this.state = state;
		}

		public void InitStyles ()
		{
			if (plusButtonStyle == null)
				plusButtonStyle = new GUIStyle ("OL Plus");
			if (animationRowEvenStyle == null)
				animationRowEvenStyle = new GUIStyle("AnimationRowEven");
			if (animationRowOddStyle == null)
				animationRowOddStyle = new GUIStyle("AnimationRowOdd");
			if (animationSelectionTextField == null)
				animationSelectionTextField = new GUIStyle ("AnimationSelectionTextField");
			if (animationLineStyle == null)
			{
				animationLineStyle = new GUIStyle (s_Styles.lineStyle);
				animationLineStyle.padding.left = 0;
			}
			if (animationCurveDropdown == null)
				animationCurveDropdown = new GUIStyle ("AnimPropDropdown");
		}

		protected void DoNodeGUI (Rect rect, AnimationWindowHierarchyNode node, bool selected, bool focused, int row)
		{
			InitStyles ();

			if (node is AnimationWindowHierarchyMasterNode)
				return;

			float indent = k_BaseIndent + (node.depth + node.indent) * k_IndentWidth;

			if (node is AnimationWindowHierarchyAddButtonNode)
			{
				if (Event.current.type == EventType.MouseMove && s_WasInsideValueRectFrame >= 0)
				{
					if (s_WasInsideValueRectFrame >= Time.frameCount - 1)
						Event.current.Use ();
					else
						s_WasInsideValueRectFrame = -1;
				}
				
				bool canAddCurves = state.m_ActiveGameObject && AnimationWindowUtility.GameObjectIsAnimatable (state.m_ActiveGameObject, state.m_ActiveAnimationClip);
				EditorGUI.BeginDisabledGroup(!canAddCurves);
				DoAddCurveButton (rect);
				EditorGUI.EndDisabledGroup();
			}
			else
			{
				DoRowBackground (rect, row);
				DoIconAndName (rect, node, selected, focused, indent);
				DoFoldout (node, rect, indent);

				EditorGUI.BeginDisabledGroup(state.IsReadOnly);

				DoValueField (rect, node, row);
				HandleContextMenu (rect, node);

				EditorGUI.EndDisabledGroup();
				
				DoCurveDropdown (rect, node);
				DoCurveColorIndicator (rect, node);
			}			
			EditorGUIUtility.SetIconSize (Vector2.zero);
		}

		public override void BeginRowGUI()
		{
			base.BeginRowGUI();
			HandleDelete();
		}

		private void DoAddCurveButton (Rect rect)
		{
			float xMargin = 24f;
			float yMargin = 10f;

			Rect rectWithMargin = new Rect (rect.xMin + xMargin, rect.yMin + yMargin, rect.width - xMargin * 2f, rect.height - yMargin * 2f);
			if (GUI.Button (rectWithMargin, "Add Curve"))
			{
				// TODO: Remove state having a reference to animation window once the whole deprecated animation window is rewritten and we hopefully have better way of ensuring we are ready to add curves
				if (!state.m_AnimationWindow.EnsureAnimationMode())
					return;

				AddCurvesPopup.gameObject = state.m_RootGameObject;
				AddCurvesPopupHierarchyDataSource.showEntireHierarchy = true;
				
				if (AddCurvesPopup.ShowAtPosition (rectWithMargin, state))
				{
					GUIUtility.ExitGUI ();
				}
			}
		}

		private void DoRowBackground(Rect rect, int row)
		{
			if (Event.current.type != EventType.Repaint)
				return;

			// Different background for even rows
			if (row % 2 == 0)
				animationRowEvenStyle.Draw (rect, false, false, false, false);
			else
				animationRowOddStyle.Draw (rect, false, false, false, false);
		}

		// Draw foldout (after text content above to ensure drop down icon is rendered above selection highlight)
		private void DoFoldout (AnimationWindowHierarchyNode node, Rect rect, float indent)
		{
			if (m_TreeView.data.IsExpandable (node))
			{
				Rect toggleRect = rect;
				toggleRect.x = indent;
				toggleRect.width = k_FoldoutWidth;
				EditorGUI.BeginChangeCheck ();
				bool newExpandedValue = GUI.Toggle (toggleRect, m_TreeView.data.IsExpanded (node), GUIContent.none, s_Styles.foldout);
				if (EditorGUI.EndChangeCheck ())
				{
					if (Event.current.alt)
						m_TreeView.data.SetExpandedWithChildren (node, newExpandedValue);
					else
						m_TreeView.data.SetExpanded(node, newExpandedValue);

					if (newExpandedValue)
						m_TreeView.UserExpandedNode (node);
				}
			}
			else
			{
				AnimationWindowHierarchyPropertyNode hierarchyPropertyNode = node as AnimationWindowHierarchyPropertyNode;
				AnimationWindowHierarchyState hierarchyState = m_TreeView.state as AnimationWindowHierarchyState;

				if (hierarchyPropertyNode != null && hierarchyPropertyNode.isPptrNode)
				{
					Rect toggleRect = rect;
					toggleRect.x = indent;
					toggleRect.width = k_FoldoutWidth;

					EditorGUI.BeginChangeCheck ();
					bool tallMode = hierarchyState.getTallMode (hierarchyPropertyNode);
					tallMode = GUI.Toggle (toggleRect, tallMode, GUIContent.none, s_Styles.foldout);
					if (EditorGUI.EndChangeCheck ())
						hierarchyState.setTallMode (hierarchyPropertyNode, tallMode);
				}
			}
		}

		private void DoIconAndName (Rect rect, AnimationWindowHierarchyNode node, bool selected, bool focused, float indent)
		{
			EditorGUIUtility.SetIconSize (new Vector2 (13, 13)); // If not set we see icons scaling down if text is being cropped

			int nodeControlID = TreeView.GetItemControlID (node);
			
			// TODO: All this is horrible. SHAME FIX!
			if (Event.current.type == EventType.Repaint)
			{
				bool isDropTarget = m_TreeView.dragging.GetDropTargetControlID () == nodeControlID && m_TreeView.data.CanBeParent (node);
				
				animationLineStyle.Draw (rect, GUIContent.none, isDropTarget, isDropTarget, selected, focused);
				
				if (AnimationMode.InAnimationMode ())
				rect.width -= k_ValueFieldOffsetFromRightSide + 2;
				
				bool isLeftOverCurve = AnimationWindowUtility.IsNodeLeftOverCurve(node, state.m_RootGameObject);

				if (node.depth == 0)
				{
					Transform transform = state.m_RootGameObject.transform.Find (node.path);
					if (transform == null)
						isLeftOverCurve = true;

					GUIContent content = new GUIContent (GetGameObjectName (node.path) + " : " + node.displayName, GetIconForNode (node));

					Color oldColor = animationLineStyle.normal.textColor;

					Color textColor = EditorGUIUtility.isProSkin ? Color.gray * 1.35f : Color.black;
					textColor = isLeftOverCurve ? Color.red : textColor;
					SetStyleTextColor (animationLineStyle, textColor);
					
					rect.xMin += (int) (indent + k_FoldoutWidth);
					animationLineStyle.Draw (rect, content, isDropTarget, isDropTarget, selected, focused);
					
					SetStyleTextColor (animationLineStyle, oldColor);
				}
				else
				{
					s_Styles.content.text = node.displayName;

					Texture icon = GetIconForNode (node);

					s_Styles.content.image = icon;

					Color oldColor = animationLineStyle.normal.textColor;

					Color textColor = EditorGUIUtility.isProSkin ? Color.gray : m_LightSkinPropertyTextColor;
					textColor = isLeftOverCurve ? Color.red : textColor;
					SetStyleTextColor(animationLineStyle, textColor);

					rect.xMin += (int) (indent + k_IndentWidth + k_FoldoutWidth);
					animationLineStyle.Draw (rect, s_Styles.content, isDropTarget, isDropTarget, selected, focused);

					SetStyleTextColor (animationLineStyle, oldColor);
					// Show marker below this node
					if (m_TreeView.dragging.GetRowMarkerControlID () == nodeControlID)
						m_DraggingInsertionMarkerRect = new Rect(rect.x + indent + k_FoldoutWidth, rect.y, rect.width - indent, rect.height);
				}
			}
		}

		private string GetGameObjectName (string path)
		{
			if (string.IsNullOrEmpty(path))
				return state.m_RootGameObject.name;

			string[] splits = path.Split ('/');
			return splits[splits.Length - 1];
		}

		private void DoValueField(Rect rect, AnimationWindowHierarchyNode node, int row)
		{
			// Don't show value fields when not in animation mode since there is no "current time" in the clip to show values for.
			if (!AnimationMode.InAnimationMode ())
				return;
			
			EditorGUI.BeginDisabledGroup (state.IsReadOnly);

			if (node is AnimationWindowHierarchyPropertyNode)
			{
				AnimationWindowCurve[] curves = state.GetCurves (node, false);
				if (curves == null || curves.Length == 0)
					return;

				// We do valuefields for dopelines that only have single curve
				AnimationWindowCurve curve = curves[0];
				
				object objectValue = AnimationWindowUtility.GetCurrentValue(state.m_RootGameObject, curve.binding);
				Type valueType = AnimationUtility.GetEditorCurveValueType(state.m_RootGameObject, curve.binding);
					
				if (objectValue is float)
				{
					float value = (float)objectValue;

					Rect valueFieldRect = new Rect (rect.xMax - k_ValueFieldOffsetFromRightSide, rect.y, k_ValueFieldWidth, rect.height);
					
					if (Event.current.type == EventType.MouseMove && valueFieldRect.Contains (Event.current.mousePosition))
						s_WasInsideValueRectFrame = Time.frameCount;

					EditorGUI.BeginChangeCheck();
					
					if (valueType == typeof (bool))
					{
						value = EditorGUI.Toggle (valueFieldRect, value != 0) ? 1 : 0;
					}
					else
					{
						int id = GUIUtility.GetControlID (123456544, FocusType.Keyboard, valueFieldRect);
						bool enterInTextField = (EditorGUIUtility.keyboardControl == id
							&& EditorGUIUtility.editingTextField
							&& Event.current.type == EventType.KeyDown
							&& (Event.current.character == '\n' || (int)Event.current.character == 3));
						value = EditorGUI.DoFloatField (EditorGUI.s_RecycledEditor,
							valueFieldRect,
							new Rect (0,0,0,0),
							id,
							value,
							EditorGUI.kFloatFieldFormatString,
							animationSelectionTextField,
							false);
						if (enterInTextField)
						{
							GUI.changed = true;
							Event.current.Use ();
						}
					}

					if (float.IsInfinity (value) || float.IsNaN (value))
						value = 0;

					if (EditorGUI.EndChangeCheck())
					{
						AnimationWindowKeyframe existingKeyframe = null;
						foreach (AnimationWindowKeyframe keyframe in curve.m_Keyframes)
						{
							if (Mathf.Approximately (keyframe.time, state.time.time))
								existingKeyframe = keyframe;
						}

						if (existingKeyframe == null)
							AnimationWindowUtility.AddKeyframeToCurve (curve, value, valueType, state.time);
						else
							existingKeyframe.value = value;

						state.SaveCurve(curve);
					}
				}
			}

			EditorGUI.EndDisabledGroup();
		}
		
		private void DoCurveDropdown (Rect rect, AnimationWindowHierarchyNode node)
		{
			rect = new Rect (
				rect.xMax - k_RowRightOffset - 12,
				rect.yMin + 2,
				22, 12);
			if (GUI.Button (rect, GUIContent.none, animationCurveDropdown))
			{
				state.SelectHierarchyItem (node.id, false, false);
				state.m_AnimationWindow.RefreshShownCurves(true); 
				GenericMenu menu = GenerateMenu(new AnimationWindowHierarchyNode[] { node }.ToList());
				menu.DropDown(rect);
				Event.current.Use();
			}
		}
		
		private void DoCurveColorIndicator (Rect rect, AnimationWindowHierarchyNode node)
		{		
			if (Event.current.type != EventType.repaint)
				return;

			Color originalColor = GUI.color;
			
			if (!state.m_ShowCurveEditor)
				GUI.color = k_KeyColorInDopesheetMode;
			else if (node.curves.Length == 1 && !node.curves[0].isPPtrCurve)
				GUI.color = CurveUtility.GetPropertyColor (node.curves[0].binding.propertyName);
			else
				GUI.color = k_KeyColorForNonCurves;

			bool hasKey = false;
			if (AnimationMode.InAnimationMode ())
			{
				foreach (var curve in node.curves)
				{
					if (curve.m_Keyframes.Any (key => state.time.ContainsTime (key.time)))
					{
						hasKey = true;
					}
				}
			}
			
			Texture icon = hasKey ? CurveUtility.GetIconKey () : CurveUtility.GetIconCurve ();
			rect = new Rect (rect.xMax - k_RowRightOffset - (icon.width / 2) - 5, rect.yMin + k_ColorIndicatorTopMargin, icon.width, icon.height);
			GUI.DrawTexture (rect, icon, ScaleMode.ScaleToFit, true, 1);

			GUI.color = originalColor;
		}

		private void HandleDelete ()
		{
			if (m_TreeView.HasFocus ())
			{
				switch (Event.current.type)
				{
					case EventType.ExecuteCommand:
						if ((Event.current.commandName == "SoftDelete" || Event.current.commandName == "Delete"))
						{
							if (Event.current.type == EventType.ExecuteCommand)
								RemoveCurvesFromSelectedNodes ();
							Event.current.Use ();
						}
						break;

					case EventType.KeyDown:
						if (Event.current.keyCode == KeyCode.Backspace || Event.current.keyCode == KeyCode.Delete)
						{
							RemoveCurvesFromSelectedNodes ();
							Event.current.Use ();
						}
						break;
				}
			}
		}

		private void HandleContextMenu (Rect rect, AnimationWindowHierarchyNode node)
		{
			if (Event.current.type != EventType.ContextClick)
				return;

			if (rect.Contains (Event.current.mousePosition))
			{
				state.SelectHierarchyItem(node.id, false, true);
				state.m_AnimationWindow.RefreshShownCurves (true);
				GenerateMenu(state.selectedHierarchyNodes).ShowAsContext();
				Event.current.Use();
			}
		}
		
		private GenericMenu GenerateMenu (List<AnimationWindowHierarchyNode> interactedNodes)
		{
			List<AnimationWindowCurve> curves = GetCurvesAffectedByNodes (interactedNodes, false);
			// Linked curves are like regular affected curves but always include transform siblings
			List<AnimationWindowCurve> linkedCurves = GetCurvesAffectedByNodes (interactedNodes, true);

			// If its Transform curve, we will remove all the components (xyz)
			bool singleTransformCurve = curves.Count == 1 ? curves[0].type == typeof (Transform) : false;

			GenericMenu menu = new GenericMenu();
			
			// Remove curves
			menu.AddItem (new GUIContent (curves.Count > 1 || singleTransformCurve ? "Remove Properties" : "Remove Property"), false, RemoveCurvesFromSelectedNodes);

			// Change rotation interpolation
			bool showInterpolation = true;
			EditorCurveBinding[] curveBindings = new EditorCurveBinding[linkedCurves.Count];
			for (int i = 0; i < curves.Count; i++)
				curveBindings[i] = linkedCurves[i].binding;
			RotationCurveInterpolation.Mode rotationInterpolation = GetRotationInterpolationMode (curveBindings);
			if (rotationInterpolation == RotationCurveInterpolation.Mode.Undefined)
			{
				showInterpolation = false;
			}
			else
			{
				foreach (var node in interactedNodes)
				{
					if (!(node is AnimationWindowHierarchyPropertyGroupNode))
						showInterpolation = false;
				}
			}
			if (showInterpolation)
			{
				menu.AddItem (new GUIContent ("Interpolation/Euler Angles"), rotationInterpolation == RotationCurveInterpolation.Mode.Baked, ChangeRotationInterpolation, RotationCurveInterpolation.Mode.Baked);
				menu.AddItem (new GUIContent ("Interpolation/Quaternion"), rotationInterpolation == RotationCurveInterpolation.Mode.NonBaked, ChangeRotationInterpolation, RotationCurveInterpolation.Mode.NonBaked);
				//menu.AddItem (new GUIContent ("Interpolation/Raw quaternion"), rotationInterpolation == RotationCurveInterpolation.Mode.RawQuaternions, ChangeRotationInterpolation, RotationCurveInterpolation.Mode.RawQuaternions);
			}
			
			// Menu items that are only applicaple when in animation mode:
			if (AnimationMode.InAnimationMode ())
			{
				menu.AddSeparator("");
				
				bool allHaveKeys = true;
				bool noneHaveKeys = true;
				bool noRealCurvesHaveKeys = true;
				foreach (AnimationWindowCurve curve in curves)
				{
					bool curveHasKey = curve.HasKeyframe (state.time);
					if (!curveHasKey)
						allHaveKeys = false;
					else
					{
						noneHaveKeys = false;
						if (!curve.isPPtrCurve)
							noRealCurvesHaveKeys = false;
					}
				}
				
				string str;
				
				str = "Add Key";
				if (allHaveKeys)
					menu.AddDisabledItem (new GUIContent (str));
				else
					menu.AddItem (new GUIContent (str), false, AddKeysAtCurrentTime, curves);
				
				str = "Delete Key";
				if (noneHaveKeys)
					menu.AddDisabledItem (new GUIContent (str));
				else
					menu.AddItem (new GUIContent (str), false, DeleteKeysAtCurrentTime, curves);
				
				if (!noRealCurvesHaveKeys)
				{
					menu.AddSeparator (string.Empty);
					
					List<KeyIdentifier> keyList = new List<KeyIdentifier> ();
					foreach (AnimationWindowCurve curve in curves)
					{
						if (curve.isPPtrCurve)
							continue;
						
						int index = curve.GetKeyframeIndex (state.time);
						if (index == -1)
							continue;
						
						CurveRenderer renderer = CurveRendererCache.GetCurveRenderer (state.m_ActiveAnimationClip, curve.binding);
						int id = CurveUtility.GetCurveID (state.m_ActiveAnimationClip, curve.binding);
						keyList.Add (new KeyIdentifier (renderer, id, index));
					}
					
					CurveMenuManager menuManager = new CurveMenuManager (state.m_AnimationWindow);
					menuManager.AddTangentMenuItems (menu, keyList);
				}
			}
			
			return menu;
		}
		
		private void AddKeysAtCurrentTime (object obj) { AddKeysAtCurrentTime ((List<AnimationWindowCurve>)obj); }
		private void AddKeysAtCurrentTime (List<AnimationWindowCurve> curves)
		{
			foreach (AnimationWindowCurve curve in curves)
				AnimationWindowUtility.AddKeyframeToCurve (state, curve, state.time);
		}
		
		private void DeleteKeysAtCurrentTime (object obj) { DeleteKeysAtCurrentTime ((List<AnimationWindowCurve>)obj); }
		private void DeleteKeysAtCurrentTime (List<AnimationWindowCurve> curves)
		{
			foreach (AnimationWindowCurve curve in curves)
			{
				curve.RemoveKeyframe (state.time);
				state.SaveCurve (curve);
			}
		}

		private void ChangeRotationInterpolation (System.Object interpolationMode)
		{
			RotationCurveInterpolation.Mode mode = (RotationCurveInterpolation.Mode)interpolationMode;

			AnimationWindowCurve[] activeCurves = state.activeCurves.ToArray();
			EditorCurveBinding[] curveBindings = new EditorCurveBinding[activeCurves.Length];

			for (int i=0; i<activeCurves.Length; i++)
			{
				curveBindings[i] = activeCurves[i].binding;
			}
			
			RotationCurveInterpolation.SetInterpolation (state.m_ActiveAnimationClip, curveBindings, mode);
			MaintainTreeviewStateAfterRotationInterpolation (mode);
			state.m_HierarchyData.ReloadData ();
		}
		
		private void RemoveCurvesFromSelectedNodes ()
		{
			RemoveCurvesFromNodes (state.selectedHierarchyNodes);
		}
		
		private void RemoveCurvesFromNodes (List<AnimationWindowHierarchyNode> nodes)
		{
			foreach (var node in nodes)
			{
				AnimationWindowHierarchyNode hierarchyNode = (AnimationWindowHierarchyNode)node;
				
				// For Transform, we want to always delete all components (xyz)
				if (hierarchyNode.parent is AnimationWindowHierarchyPropertyGroupNode && hierarchyNode.binding != null && hierarchyNode.binding.Value.type == typeof (Transform))
					hierarchyNode = (AnimationWindowHierarchyNode)hierarchyNode.parent;

				AnimationWindowCurve[] curves = null;

				// Property or propertygroup
				if (hierarchyNode is AnimationWindowHierarchyPropertyGroupNode || hierarchyNode is AnimationWindowHierarchyPropertyNode)
					curves = AnimationWindowUtility.FilterCurves(state.allCurves.ToArray(), hierarchyNode.path, hierarchyNode.animatableObjectType, hierarchyNode.propertyName);
				else
					curves = AnimationWindowUtility.FilterCurves(state.allCurves.ToArray(), hierarchyNode.path, hierarchyNode.animatableObjectType);

				foreach (AnimationWindowCurve animationWindowCurve in curves)
					state.RemoveCurve(animationWindowCurve);
			}

			m_TreeView.ReloadData();	
		}
		
		private List<AnimationWindowCurve> GetCurvesAffectedByNodes (List<AnimationWindowHierarchyNode> nodes, bool includeLinkedCurves)
		{
			List<AnimationWindowCurve> curves = new List<AnimationWindowCurve> ();
			foreach (var node in nodes)
			{
				AnimationWindowHierarchyNode hierarchyNode = node;

				if (hierarchyNode.parent is AnimationWindowHierarchyPropertyGroupNode && (includeLinkedCurves || hierarchyNode.binding != null && hierarchyNode.binding.Value.type == typeof (Transform)))
					hierarchyNode = (AnimationWindowHierarchyNode) hierarchyNode.parent;

				// Property or propertygroup
				if (hierarchyNode is AnimationWindowHierarchyPropertyGroupNode || hierarchyNode is AnimationWindowHierarchyPropertyNode)
					curves.AddRange (AnimationWindowUtility.FilterCurves(state.allCurves.ToArray(), hierarchyNode.path, hierarchyNode.animatableObjectType, hierarchyNode.propertyName));
				else
					curves.AddRange (AnimationWindowUtility.FilterCurves(state.allCurves.ToArray(), hierarchyNode.path, hierarchyNode.animatableObjectType));
			}
			return curves.Distinct ().ToList ();
		}

		// Changing rotation interpolation will change the propertynames of the curves
		// Propertynames are used in treeview node IDs, so we need to anticipate the new IDs by injecting them into treeview state
		// This way treeview state (selection and expanding) will be preserved once the curve data is eventually reloaded
		private void MaintainTreeviewStateAfterRotationInterpolation (RotationCurveInterpolation.Mode newMode)
		{
			List<int> selectedInstaceIDs = state.m_hierarchyState.selectedIDs;
			List<int> expandedInstaceIDs = state.m_hierarchyState.expandedIDs;
			
			List<int> oldIDs = new List<int> (); 
			List<int> newIds = new List<int> ();

			for (int i = 0; i < selectedInstaceIDs.Count; i++)
			{
				AnimationWindowHierarchyNode node = state.m_HierarchyData.FindItem (selectedInstaceIDs[i]) as AnimationWindowHierarchyNode;

				if (node != null && !node.propertyName.Equals (RotationCurveInterpolation.GetPrefixForInterpolation (newMode)))
				{
					string oldPrefix = node.propertyName.Split ('.')[0];
					string newPropertyName = node.propertyName.Replace (oldPrefix, RotationCurveInterpolation.GetPrefixForInterpolation (newMode));

					// old treeview node id
					oldIDs.Add (selectedInstaceIDs[i]);
					// and its new replacement
					newIds.Add ((node.path + node.animatableObjectType.Name + newPropertyName).GetHashCode ());		
				}
			}

			// Replace old IDs with new ones
			for (int i = 0; i < oldIDs.Count; i++)
			{
				if (selectedInstaceIDs.Contains (oldIDs[i]))
				{
					int index = selectedInstaceIDs.IndexOf (oldIDs[i]);
					selectedInstaceIDs[index] = newIds[i];
				}
				if (expandedInstaceIDs.Contains (oldIDs[i]))
				{
					int index = expandedInstaceIDs.IndexOf (oldIDs[i]);
					expandedInstaceIDs[index] = newIds[i];
				}
				if (state.m_hierarchyState.lastClickedID == oldIDs[i])
					state.m_hierarchyState.lastClickedID = newIds[i];
			}

			state.m_hierarchyState.selectedIDs = new List<int> (selectedInstaceIDs);
			state.m_hierarchyState.expandedIDs = new List<int> (expandedInstaceIDs);
		}

		private RotationCurveInterpolation.Mode GetRotationInterpolationMode (EditorCurveBinding[] curves)
		{
			if(curves == null || curves.Length == 0)
				return RotationCurveInterpolation.Mode.Undefined;

			RotationCurveInterpolation.Mode mode = RotationCurveInterpolation.GetModeFromCurveData (curves[0]);
			for (int i = 1; i < curves.Length; i++)
			{
				RotationCurveInterpolation.Mode nextMode = RotationCurveInterpolation.GetModeFromCurveData (curves[i]);
				if (mode != nextMode)
					return RotationCurveInterpolation.Mode.Undefined;
			}

			return mode;
		}

		// TODO: Make real styles, not this
		private void SetStyleTextColor (GUIStyle style, Color color)
		{
			style.normal.textColor = color;
			style.focused.textColor = color;
			style.active.textColor = color;
			style.hover.textColor = color;
		}

		public override void GetFirstAndLastRowVisible (List<TreeViewItem> rows, float topPixel, float heightInPixels, out int firstRowVisible, out int lastRowVisible)
		{
			firstRowVisible = 0;
			lastRowVisible = rows.Count - 1;
		}

		public override float GetTopPixelOfRow (int row, List<TreeViewItem> rows)
		{
			float top = 0f;
			for (int i = 0; i < row && i < rows.Count; i++)
			{
				AnimationWindowHierarchyNode node = rows[i] as AnimationWindowHierarchyNode;
				top += GetNodeHeight (node);
			}
			return top;
		}
		
		public float GetNodeHeight (AnimationWindowHierarchyNode node)
		{
			if (node is AnimationWindowHierarchyAddButtonNode)
				return k_AddCurveButtonNodeHeight;

			AnimationWindowHierarchyState hierarchyState = m_TreeView.state as AnimationWindowHierarchyState;
			return hierarchyState.getTallMode (node) ? k_DopeSheetRowHeightTall : k_DopeSheetRowHeight;
		}

		public override Vector2 GetTotalSize (List<TreeViewItem> rows)
		{
			float height = 0f;
			for (int i = 0; i < rows.Count; i++)
			{
				AnimationWindowHierarchyNode node = rows[i] as AnimationWindowHierarchyNode;
				height += GetNodeHeight (node);
			}

			return new Vector2(1, height);
		}

		public override Rect OnRowGUI (TreeViewItem node, int row, float rowWidth, bool selected, bool focused)
		{
			AnimationWindowHierarchyNode hierarchyNode = node as AnimationWindowHierarchyNode;
			
			if (hierarchyNode.topPixel == null)
				hierarchyNode.topPixel = GetTopPixelOfRow (row, m_TreeView.data.GetVisibleRows());

			float rowHeight = GetNodeHeight (hierarchyNode);
			Rect rowRect = new Rect (0, (float)hierarchyNode.topPixel, rowWidth, rowHeight);			

			DoNodeGUI (rowRect, hierarchyNode, selected, focused, row);
			
			return rowRect;
		}

		override protected void SyncFakeItem()
		{
			//base.SyncFakeItem();
		}

		override protected void RenameEnded()
		{
			//base.RenameEnded();
		}

		override protected Texture GetIconForNode(TreeViewItem item)
		{
			if (item != null)
				return item.icon;
			
			return null;
		}
	}
}
