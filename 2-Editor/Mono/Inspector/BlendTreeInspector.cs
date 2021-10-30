using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

using Object = UnityEngine.Object;


namespace UnityEditor
{
[CustomEditor(typeof(BlendTree))]
internal class BlendTreeInspector : Editor
{
	class Styles
	{
		public readonly GUIStyle background = "MeBlendBackground";
		public readonly GUIStyle triangleLeft = "MeBlendTriangleLeft";
		public readonly GUIStyle triangleRight = "MeBlendTriangleRight";
		public readonly GUIStyle blendPosition = "MeBlendPosition";
		public GUIStyle clickDragFloatFieldLeft = new GUIStyle (EditorStyles.miniTextField);
		public GUIStyle clickDragFloatFieldRight = new GUIStyle (EditorStyles.miniTextField);
		public GUIStyle clickDragFloatLabelLeft = new GUIStyle (EditorStyles.miniLabel);
		public GUIStyle clickDragFloatLabelRight = new GUIStyle (EditorStyles.miniLabel);
		public GUIStyle headerIcon = new GUIStyle ();
		public GUIStyle errorStyle = new GUIStyle(EditorStyles.wordWrappedLabel);
		public GUIContent speedIcon = new GUIContent(EditorGUIUtility.IconContent("SpeedScale"));
		public GUIContent mirrorIcon = new GUIContent(EditorGUIUtility.IconContent("Mirror"));
		public Texture2D pointIcon = EditorGUIUtility.LoadIcon ("blendKey");
		public Texture2D pointIconSelected = EditorGUIUtility.LoadIcon ("blendKeySelected");
		public Texture2D pointIconOverlay = EditorGUIUtility.LoadIcon ("blendKeyOverlay");
		public Texture2D samplerIcon = EditorGUIUtility.LoadIcon ("blendSampler");
		public Styles ()
		{
			errorStyle.alignment = TextAnchor.MiddleCenter;	
			speedIcon.tooltip = "Changes animation speed.";
			mirrorIcon.tooltip = "Mirror animation.";
			headerIcon.alignment = TextAnchor.MiddleCenter;
			
			clickDragFloatFieldLeft.alignment = TextAnchor.MiddleLeft;
			clickDragFloatFieldRight.alignment = TextAnchor.MiddleRight;
			clickDragFloatLabelLeft.alignment = TextAnchor.MiddleLeft;
			clickDragFloatLabelRight.alignment = TextAnchor.MiddleRight;
		}
	}
	static Styles styles;
    internal static AnimatorController currentController = null;
	internal static Animator currentAnimator = null;
	internal static BlendTree parentBlendTree = null;
	internal static Action<BlendTree> blendTreeHierarchyChanged = null;
	internal static Action<BlendTree> blendParameterInputChanged = null;
	private readonly int m_BlendAnimationID = "BlendAnimationIDHash".GetHashCode ();		
	private readonly int m_ClickDragFloatID = "ClickDragFloatIDHash".GetHashCode ();
	private ReorderableList m_ReorderableList;
	private SerializedProperty m_Childs;
	private SerializedProperty m_BlendParameter;
	private SerializedProperty m_BlendParameterY;
	private BlendTree m_BlendTree;
	private SerializedProperty m_UseAutomaticThresholds;
		
	private SerializedProperty m_MinThreshold;
	private SerializedProperty m_MaxThreshold;
	private SerializedProperty m_Name;
		
	private SerializedProperty m_BlendType;
		
	private AnimValueManager m_Anims = new AnimValueManager ();
	private AnimBool m_ShowGraph = new AnimBool();
	private AnimBool m_ShowCompute = new AnimBool();
	private AnimBool m_ShowAdjust = new AnimBool();
	private bool m_ShowGraphValue = false;
	
	private bool m_HierarchyChanged = false;
		
	private string m_PrevBlendValueName = "";
	private string m_PrevBlendValueYName = "";
	
	private float[] m_Weights;
	private bool m_DirtyVisualization = true;
	private const int kVisResolution = 64;
	private Texture2D m_BlendTex = null;
	private List<Texture2D> m_WeightTexs = new List<Texture2D> ();
	private string m_WarningMessage = null;

	private PreviewBlendTree m_PreviewBlendTree;
	private VisualizationBlendTree m_VisBlendTree;
	[NonSerialized]
	private GameObject m_VisInstance = null;
	
	private int ParameterCount { get { return m_BlendType.intValue > 0 ? 2 : 1; } }

	public void OnEnable()
	{
		m_Name = serializedObject.FindProperty ("m_Name");
		m_BlendParameter = serializedObject.FindProperty("m_BlendParameter");
		m_BlendParameterY = serializedObject.FindProperty("m_BlendParameterY");
		m_UseAutomaticThresholds = serializedObject.FindProperty("m_UseAutomaticThresholds");
		m_MinThreshold = serializedObject.FindProperty("m_MinThreshold");
		m_MaxThreshold = serializedObject.FindProperty("m_MaxThreshold");
		m_BlendType = serializedObject.FindProperty("m_BlendType");
		m_DirtyVisualization = true;
	}

	void Init ()
	{
		if (styles == null)
			styles = new Styles();
		if (m_BlendTree == null)
			m_BlendTree = target as BlendTree;
		if (styles == null)
			styles = new Styles();
		if (m_PreviewBlendTree == null)
			m_PreviewBlendTree = new PreviewBlendTree();
		if (m_VisBlendTree == null)
			m_VisBlendTree = new VisualizationBlendTree ();
		if (m_Childs == null)
		{
			m_Childs = serializedObject.FindProperty ("m_Childs");
			m_ReorderableList = new ReorderableList(serializedObject, m_Childs);
			m_ReorderableList.drawHeaderCallback = DrawHeader;
			m_ReorderableList.drawElementCallback = DrawChild;
			m_ReorderableList.onReorderCallback = EndDragChild;
			m_ReorderableList.onAddDropdownCallback = AddButton;
			m_ReorderableList.onRemoveCallback = RemoveButton;			
			if (m_BlendType.intValue == 0)
			SortByThreshold();
			m_ShowGraphValue = (m_Childs.arraySize >= 2)? true : false;
			m_ShowGraph.value = m_ShowGraphValue;
			m_ShowAdjust.value = AllMotions();
			m_ShowCompute.value =! m_UseAutomaticThresholds.boolValue;
			m_Anims.Add (m_ShowGraph);
			m_Anims.Add (m_ShowAdjust);
			m_Anims.Add (m_ShowCompute);
		}

		m_PreviewBlendTree.Init (m_BlendTree, currentAnimator);
		
		if (m_VisInstance == null)
		{
			GameObject go = (GameObject)EditorGUIUtility.Load ("Avatar/DefaultAvatar.fbx");
			m_VisInstance = (GameObject)EditorUtility.InstantiateRemoveAllNonAnimationComponents (go, Vector3.zero, Quaternion.identity);
			InitInstantiatedPreviewRecursive (m_VisInstance);

			foreach (Renderer renderer in m_VisInstance.GetComponentsInChildren<Renderer>())
				renderer.enabled = false;
		}
		m_VisBlendTree.Init (m_BlendTree, m_VisInstance.GetComponent<Animator> ());
	}
		
	static void InitInstantiatedPreviewRecursive (GameObject go)
	{
		go.hideFlags = HideFlags.HideAndDontSave;		
		foreach (Transform c in go.transform)
			InitInstantiatedPreviewRecursive (c.gameObject);
	}
		
	internal override void OnHeaderIconGUI (Rect iconRect)
	{
		Texture2D icon = AssetPreview.GetMiniThumbnail (target);
		GUI.Label (iconRect, icon);
	}

	internal override void OnHeaderTitleGUI (Rect titleRect, string header)
	{
		serializedObject.Update ();

		Rect textFieldRect = titleRect;
		textFieldRect.height = 16f;

		EditorGUI.BeginChangeCheck ();
		EditorGUI.showMixedValue = m_Name.hasMultipleDifferentValues;
		string newName = EditorGUI.DelayedTextField (textFieldRect, m_Name.stringValue, null, EditorStyles.textField);
		EditorGUI.showMixedValue = false;
		if (EditorGUI.EndChangeCheck () && !String.IsNullOrEmpty (newName))
		{
			foreach (Object obj in targets)
				ObjectNames.SetNameSmart (obj, newName);

			m_HierarchyChanged = true;
		}
		serializedObject.ApplyModifiedProperties ();
	}
	
	internal override void OnHeaderControlsGUI ()
	{
		EditorGUIUtility.labelWidth = 80;
		serializedObject.Update ();
		EditorGUI.BeginChangeCheck ();
		EditorGUILayout.PropertyField (m_BlendType);
		if (EditorGUI.EndChangeCheck ())
			m_DirtyVisualization = true;
		serializedObject.ApplyModifiedProperties ();
	}

	private void ParameterGUI ()
	{
		EditorGUILayout.BeginHorizontal ();
		
		// Label
		if (ParameterCount == 1)
			EditorGUILayout.PrefixLabel (EditorGUIUtility.TempContent ("Parameter"));
		else
			EditorGUILayout.PrefixLabel (EditorGUIUtility.TempContent ("Parameters"));

		// Available parameters
		// Populate parameters list and find indexes of used blend parameters
		List<string> parameterList = new List<string>();
		string currentParameter = m_BlendTree.blendParameter;
		string currentParameterY = m_BlendTree.blendParameterY;
			int index = 0;
		int indexY = 0;
		if (currentController != null)
		{
			for (int i = 0; i < currentController.parameterCount; i++)
			{
				// only deal with floats
                if (currentController.GetParameterType(i) == AnimatorControllerParameterType.Float)
				{
                    string name = currentController.GetParameterName(i);
					if (name == currentParameter)
						index = parameterList.Count;
					if (name == currentParameterY)
						indexY = parameterList.Count;
					parameterList.Add(name);
				}
			}
		}
		
		bool reset = false;

		// Show first blend parameter
		{
			int newIdx = EditorGUILayout.Popup(index, parameterList.ToArray());
			if (newIdx != index)
			{
				m_BlendParameter.stringValue = parameterList[newIdx];				
			}
			if (m_PrevBlendValueName != currentParameter)
			{
				m_PrevBlendValueName = currentParameter;
				reset = true;
			}
		}
		
		if (ParameterCount > 1)
		{
			// Show second blend parameter
			int newIdxY = EditorGUILayout.Popup(indexY, parameterList.ToArray());
			if (newIdxY != indexY)
			{
				m_BlendParameterY.stringValue = parameterList[newIdxY];
			}
			if (m_PrevBlendValueYName != currentParameterY)
			{
				m_PrevBlendValueYName = currentParameterY;
				reset = true;
			}
		}
		
		if (reset)
		{
			m_PreviewBlendTree.ResetStateMachine ();
			m_VisBlendTree.Reset ();
			m_DirtyVisualization = true;
		}
		
		EditorGUILayout.EndHorizontal ();
	}
	
	public override void OnInspectorGUI ()
	{
		Init ();
		if (m_Anims.callback == null)
				m_Anims.callback = Repaint;
		serializedObject.Update ();
		
		// Parameters
		ParameterGUI ();
		
		m_ShowGraphValue = (m_Childs.arraySize >= 2)? true : false;
		m_ShowGraph.target = m_ShowGraphValue;

		m_UseAutomaticThresholds = serializedObject.FindProperty("m_UseAutomaticThresholds");
		GUI.enabled = true;
		if (EditorGUILayout.BeginFadeGroup (m_ShowGraph.faded))
		{
			if (ParameterCount == 1)
			{
				BlendGraph (EditorGUILayout.GetControlRect(false, 40, styles.background));
				ThresholdValues();
			}
			else
			{
				GUILayout.Space (1);
				GUILayout.BeginHorizontal ();
				GUILayout.FlexibleSpace ();
				Rect graphRect = GUILayoutUtility.GetAspectRect (1, GUILayout.MaxWidth (235));
				GUI.Label (new Rect (graphRect.x-1, graphRect.y-1, graphRect.width+2, graphRect.height+2), GUIContent.none, EditorStyles.textField);
				GUI.BeginGroup (graphRect);
				graphRect.x = 0;
				graphRect.y = 0;
				BlendGraph2D (graphRect);
				GUI.EndGroup ();
				GUILayout.FlexibleSpace ();
				GUILayout.EndHorizontal ();
			}
			
			GUILayout.Space (5);
		}
		EditorGUILayout.EndFadeGroup ();
		if (m_ReorderableList != null)
			m_ReorderableList.DoList ();
		if (m_ShowGraphValue)
		{
			GUILayout.Space (10);
			AutoCompute();
		}
		serializedObject.ApplyModifiedProperties ();		
		DoHierarchyChangedCallback ();
	}
	
	private void SetMinMaxThresholds ()
	{
		float min = Mathf.Infinity;
		float max = Mathf.NegativeInfinity;
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
			SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
			min = (threshold.floatValue < min)? threshold.floatValue : min;
			max = (threshold.floatValue > max)? threshold.floatValue : max;
		}
		m_MinThreshold.floatValue = m_Childs.arraySize > 0 ? min : 0;
		m_MaxThreshold.floatValue = m_Childs.arraySize > 0 ? max : 1;
	}
		
	private void ThresholdValues ()
	{
		Rect r = EditorGUILayout.GetControlRect();
		Rect r1 = r;
		Rect r2 = r;
		r1.width /= 4;
		r2.width /= 4;
		r2.x = r.x + r.width - r2.width;
		
		float min = m_MinThreshold.floatValue;
		float max = m_MaxThreshold.floatValue;
		
		EditorGUI.BeginChangeCheck ();
		min = ClickDragFloat(r1, min);
		max = ClickDragFloat(r2, max, true);	
			
		if (EditorGUI.EndChangeCheck())
		{
			
			if (m_Childs.arraySize >= 2)
			{
				SerializedProperty firstChild = m_Childs.GetArrayElementAtIndex(0);
				SerializedProperty lastChild = m_Childs.GetArrayElementAtIndex(m_Childs.arraySize -1);
				SerializedProperty firstThreshold = firstChild.FindPropertyRelative("m_Threshold");
				SerializedProperty lastThreshold = lastChild.FindPropertyRelative("m_Threshold");
				firstThreshold.floatValue = Mathf.Min (min, max);
				lastThreshold.floatValue = Mathf.Max(min, max);
			}
			if (!m_UseAutomaticThresholds.boolValue)
			{
				// since this isn't being automatically calculated, we need to scale the values
				for (int i = 0; i < m_Childs.arraySize; i++)
				{
					SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
					SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
					float ratio = Mathf.InverseLerp(m_MinThreshold.floatValue, m_MaxThreshold.floatValue, threshold.floatValue);
					threshold.floatValue = Mathf.Lerp (Mathf.Min (min, max), Mathf.Max (min, max), ratio);
				}	
			}

			m_MinThreshold.floatValue = Mathf.Min (min, max);
			m_MaxThreshold.floatValue = Mathf.Max (min, max);
		}
	}
		
	
	private static bool s_ClickDragFloatDragged;
	private static float s_ClickDragFloatDistance;
	public float ClickDragFloat (Rect position, float value)
	{
		return ClickDragFloat(position, value, false);
	}
		
	public float ClickDragFloat (Rect position, float value, bool alignRight)
	{
		bool changed;
		
		// TODO: Why does the cursor change to arrow when editing the text?
		
		string allowedCharacters = "inftynaeINFTYNAE0123456789.,-";
		int id = EditorGUIUtility.GetControlID (m_ClickDragFloatID, FocusType.Keyboard, position);
		Event evt = Event.current;
		string str;
		switch (evt.type)
		{
			case EventType.MouseUp:
				if (GUIUtility.hotControl != id)
					break;
				evt.Use ();
				if (position.Contains(evt.mousePosition) && !s_ClickDragFloatDragged)
				{
					EditorGUIUtility.editingTextField = true;
				}
				else
				{
					GUIUtility.keyboardControl = 0;
					GUIUtility.hotControl = 0;
					s_ClickDragFloatDragged = false;
				}
				break;
			case EventType.MouseDown:
				if (GUIUtility.keyboardControl == id && EditorGUIUtility.editingTextField)
					break;
				if (position.Contains(evt.mousePosition))
				{
					evt.Use ();
					s_ClickDragFloatDragged = false;
					s_ClickDragFloatDistance = 0f;
					GUIUtility.hotControl = id;
					GUIUtility.keyboardControl = id;
					EditorGUIUtility.editingTextField = false;
					break;
				}
				break;
			case EventType.MouseDrag:
				if (GUIUtility.hotControl != id || EditorGUIUtility.editingTextField)
					break;
				s_ClickDragFloatDistance += Mathf.Abs (HandleUtility.niceMouseDelta);
				if (s_ClickDragFloatDistance >= 5f)
				{
					s_ClickDragFloatDragged = true;
					value += HandleUtility.niceMouseDelta * .03f;
					value = MathUtils.RoundBasedOnMinimumDifference (value, .03f);
					GUI.changed = true;	
				}
				evt.Use ();
				break;
		}
		
		GUIStyle style = (GUIUtility.keyboardControl == id && EditorGUIUtility.editingTextField) ?
			(alignRight ? styles.clickDragFloatFieldRight : styles.clickDragFloatFieldLeft) :
			(alignRight ? styles.clickDragFloatLabelRight : styles.clickDragFloatLabelLeft);
		if (GUIUtility.keyboardControl == id) {
			if (!EditorGUI.s_RecycledEditor.IsEditingControl (id)) {
				str = EditorGUI.s_RecycledCurrentEditingString = value.ToString ("g7");
			} else {			
				str = EditorGUI.s_RecycledCurrentEditingString;
				if(evt.type == EventType.ValidateCommand && evt.commandName == "UndoRedoPerformed")
					str = value.ToString ("g7");
			}
				
			str = EditorGUI.DoTextField (EditorGUI.s_RecycledEditor, id, position, str, style , allowedCharacters, out changed, false, false,false);
			if (changed) 
			{
				GUI.changed = true;
				EditorGUI.s_RecycledCurrentEditingString = str;
				string lowered = str.ToLower();
				if (lowered == "inf" || lowered == "infinity") {
					value = Mathf.Infinity;
				} else if (lowered == "-inf" || lowered == "-infinity") {
					value = Mathf.NegativeInfinity;
				} else {
					str = str.Replace (',', '.');
					if (!float.TryParse (str, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out value))
					{
						value = EditorGUI.s_RecycledCurrentEditingFloat = 0;
						return value;
					}
					if (System.Single.IsNaN (value))
						value = 0;
					EditorGUI.s_RecycledCurrentEditingFloat = value;
				}
			}
		} else {
			str = value.ToString ("g7");
			str = EditorGUI.DoTextField (EditorGUI.s_RecycledEditor, id, position, str, style, allowedCharacters, out changed, false, false,false);
		}
		return value;
	}
		
	private void BlendGraph (Rect area)
	{			
		// Adjust padding for rect
		// (This is normally not needed anymore, but this style has some overdraw that needs to be compensated.)
		area.xMin += 1;
		area.xMax -= 1;
		
		int sliderId = GUIUtility.GetControlID (m_BlendAnimationID, FocusType.Passive);
		
		// get points array from child objects
		int childCount = m_Childs.arraySize;
		float[] points = new float[childCount]; 
		for (int i = 0; i < childCount; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
			SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
			points[i] = threshold.floatValue;
		}
		
		// move points to GUI space
		float min = Mathf.Min (points);
		float max = Mathf.Max (points);
		for (int i = 0; i < points.Length; i++)
		{
			points[i] = area.x + (Mathf.InverseLerp(min, max, points[i]) * area.width);
		}
		
		// get blend bar info
		string currentParameter = m_BlendTree.blendParameter;
		float blendBar = area.x + (Mathf.InverseLerp(min, max, m_BlendTree.GetInputBlendValue(currentParameter)) * area.width);
		Rect barRect = new Rect(blendBar - 4f, area.y, 9f, 42f);
		
		Event evt = Event.current;
		switch (evt.GetTypeForControl (sliderId))
		{
			case EventType.Repaint:
				styles.background.Draw (area, GUIContent.none, false, false, false, false);
				if (m_Childs.arraySize >= 2)
				{		
					for (int i = 0; i < points.Length; i++)
					{
						// draw the animation triangle
						float last = (i == 0)? points[i] : points[i - 1];
						float next = (i == points.Length - 1)? points[i] : points[i + 1];
						bool drawSelected = (m_ReorderableList.index == i)? true : false;
						DrawAnimation(points[i], last, next, drawSelected, area);
					}
					Color oldColor = Handles.color;
					Handles.color = new Color(0f, 0f, 0f, 0.25f);
					Handles.DrawLine(new Vector3(area.x, area.y+area.height, 0f), new Vector3(area.x+area.width, area.y+area.height, 0f));
					Handles.color = oldColor;
					// draw the current input bar
					styles.blendPosition.Draw(barRect, GUIContent.none, false, false, false, false);
				}
				else
				{
					GUI.Label (area, EditorGUIUtility.TempContent("Please Add Motion Fields or Blend Trees"), styles.errorStyle);
				}
				break;
			case EventType.MouseDown:
				if (barRect.Contains (evt.mousePosition))
				{
					evt.Use();
					GUIUtility.hotControl = sliderId;
					m_ReorderableList.index = -1;
					m_ReorderableList.index = -1;
					float mousePosition = Mathf.InverseLerp (0, area.width, evt.mousePosition.x-4);
					mousePosition = Mathf.Lerp (min, max, mousePosition);
					m_BlendTree.SetInputBlendValue(currentParameter, mousePosition);

					if (parentBlendTree)
					{
						parentBlendTree.SetInputBlendValue(currentParameter, mousePosition);
						if (blendParameterInputChanged != null) blendParameterInputChanged(parentBlendTree);
					}

					if (blendParameterInputChanged != null) blendParameterInputChanged(m_BlendTree);
				}
				else if (area.Contains (evt.mousePosition))
				{
					evt.Use ();
					GUIUtility.hotControl = sliderId;
					GUIUtility.keyboardControl = sliderId;
					float clickPosition = evt.mousePosition.x; 

					// determine closest animation or blend tree
					float distance = Mathf.Infinity;
					for (int i = 0; i < points.Length; i++)
					{
						float last = (i == 0)? points[i] : points[i - 1];
						float next = (i == points.Length - 1)? points[i] : points[i + 1];
						if (Mathf.Abs (clickPosition - points[i]) < distance)
						{
							if (clickPosition < next && clickPosition > last)
							{
								distance = Mathf.Abs (clickPosition - points[i]);
								m_ReorderableList.index = i;
							}
						}
					}
					// turn off automatic thresholds
					m_UseAutomaticThresholds.boolValue = false;					
				}
				break;
			case EventType.MouseDrag:
				if (GUIUtility.hotControl != sliderId)
					break;
				evt.Use();
				if (m_ReorderableList.index == -1)
				{
					// the user is dragging the blend position
					float mousePosition = Mathf.InverseLerp (0, area.width, evt.mousePosition.x-4);
					mousePosition = Mathf.Lerp (min, max, mousePosition);
					m_BlendTree.SetInputBlendValue(currentParameter, mousePosition);

					if (parentBlendTree)
					{
						parentBlendTree.SetInputBlendValue(currentParameter, mousePosition);
						if (blendParameterInputChanged != null) blendParameterInputChanged(parentBlendTree);
					}

					if (blendParameterInputChanged != null) blendParameterInputChanged(m_BlendTree);
				}
				else
				{
					
					// convert mouse position to blend space
					float mousePosition = Mathf.InverseLerp (0, area.width, evt.mousePosition.x);
					mousePosition = Mathf.Lerp (min, max, mousePosition);
					
					// set the new threshold based on mousePosition
					SerializedProperty child = m_Childs.GetArrayElementAtIndex(m_ReorderableList.index);
					SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
				
					// get neighboring thresholds
					SerializedProperty lastChild = (m_ReorderableList.index <= 0)?  child : m_Childs.GetArrayElementAtIndex(m_ReorderableList.index - 1);
					SerializedProperty nextChild = (m_ReorderableList.index == m_Childs.arraySize - 1)?  child : m_Childs.GetArrayElementAtIndex(m_ReorderableList.index + 1);
					SerializedProperty lastThreshold = lastChild.FindPropertyRelative("m_Threshold");
					SerializedProperty nextThreshold = nextChild.FindPropertyRelative("m_Threshold");
				
					// change threshold based on movement
					float blendUnitsPerPixel = (max-min)/area.width;
					float pixelsMoved = evt.delta.x;
					threshold.floatValue += pixelsMoved * blendUnitsPerPixel;
				
					// reorder if dragged beyond range
					if (threshold.floatValue < lastThreshold.floatValue && m_ReorderableList.index != 0)
					{
						m_Childs.MoveArrayElement(m_ReorderableList.index, m_ReorderableList.index - 1);
						m_ReorderableList.index -= 1;
					}
					if (threshold.floatValue > nextThreshold.floatValue && m_ReorderableList.index < m_Childs.arraySize - 1)
					{
						m_Childs.MoveArrayElement(m_ReorderableList.index, m_ReorderableList.index + 1);
						m_ReorderableList.index += 1;
					}
				
				 	// snap to near thresholds
					float snapThreshold = 3f * ((max-min)/area.width);
					if (threshold.floatValue - lastThreshold.floatValue <= snapThreshold)
					{
						threshold.floatValue = lastThreshold.floatValue;
					}
					else if (nextThreshold.floatValue - threshold.floatValue <= snapThreshold)
					{	
						threshold.floatValue = nextThreshold.floatValue;
					}
				}
				break;
			
			case EventType.MouseUp:
				if (GUIUtility.hotControl == sliderId)
				{
					evt.Use ();
					GUIUtility.hotControl = 0;
					m_ReorderableList.index = -1;
				}
				break;
		}
	}
	
	private void UpdateBlendVisualization ()
	{
		Vector2[] points = GetActiveMotionPositions ();
		
		if (m_BlendTex == null)
		{
			m_BlendTex = new Texture2D (kVisResolution, kVisResolution, TextureFormat.RGBA32, false);
			m_BlendTex.hideFlags = HideFlags.DontSave;
			m_BlendTex.wrapMode = TextureWrapMode.Clamp;
		}
		while (m_WeightTexs.Count < points.Length)
		{
			Texture2D tex = new Texture2D (kVisResolution, kVisResolution, TextureFormat.RGBA32, false);
			tex.wrapMode = TextureWrapMode.Clamp;
			tex.hideFlags = HideFlags.DontSave;
			m_WeightTexs.Add (tex);
		}
		while (m_WeightTexs.Count > points.Length)
		{
			DestroyImmediate (m_WeightTexs[m_WeightTexs.Count-1]);
			m_WeightTexs.RemoveAt (m_WeightTexs.Count-1);
		}
		
		// Calculate min and max for all the points
		if (GUIUtility.hotControl == 0)
			m_BlendRect = Get2DBlendRect (GetMotionPositions ());
		
		m_VisBlendTree.Reset ();
		
		System.Diagnostics.Stopwatch watch = new System.Diagnostics.Stopwatch ();
		watch.Start ();
		Texture2D[] textures = m_WeightTexs.ToArray ();
		// While dragging, only update the weight texture that's being dragged.
		if (GUIUtility.hotControl != 0 && m_ReorderableList.index >= 0)
		{
			int[] indices = GetMotionToActiveMotionIndices ();
			for (int i=0; i<textures.Length; i++)
				if (indices[m_ReorderableList.index] != i)
					textures[i] = null;
		}
		AnimatorUtility.CalculateBlendTexture (m_VisBlendTree.animator, 0, m_VisBlendTree.animator.GetCurrentAnimatorStateInfo (0).nameHash,
			m_BlendTex, textures, m_BlendRect);
		watch.Stop ();
		
		//Debug.Log ("CalculateBlendTexture took "+watch.ElapsedMilliseconds+" ms");
		
		m_DirtyVisualization = false;
	}
	
	private Vector2[] GetMotionPositions ()
	{
		int childCount = m_Childs.arraySize;
		Vector2[] points = new Vector2[childCount]; 
		for (int i = 0; i < childCount; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
			SerializedProperty position = child.FindPropertyRelative("m_Position");
			points[i] = position.vector2Value;
		}
		return points;
	}
	
	private Vector2[] GetActiveMotionPositions ()
	{
		List<Vector2> points = new List<Vector2> ();
		int childCount = m_Childs.arraySize;
		for (int i = 0; i < childCount; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex (i);
			SerializedProperty motion = child.FindPropertyRelative ("m_Motion");
			if (motion.objectReferenceValue != null)
			{
				SerializedProperty position = child.FindPropertyRelative ("m_Position");
				points.Add (position.vector2Value);
			}
		}
		return points.ToArray ();
	}
	
	private int[] GetMotionToActiveMotionIndices ()
	{
		int childCount = m_Childs.arraySize;
		int[] indices = new int[childCount]; 
		int activeMotion = 0;
		for (int i = 0; i < childCount; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
			SerializedProperty motion = child.FindPropertyRelative("m_Motion");
			if (motion.objectReferenceValue == null)
				indices[i] = -1;
			else
			{
				indices[i] = activeMotion;
				activeMotion++;
			}
		}
		return indices;
	}
	
	private Rect Get2DBlendRect (Vector2[] points)
	{
		Vector2 center = Vector2.zero;
		float maxDist = 0;
		
		if (m_BlendType.intValue == 3)
		{
			// Make min and max rect with center at the bounds center
			Vector2 min = points[0];
			Vector2 max = points[0];
			for (int i=1; i<points.Length; i++)
			{
				max.x = Mathf.Max (max.x, points[i].x);
				max.y = Mathf.Max (max.y, points[i].y);
				min.x = Mathf.Min (min.x, points[i].x);
				min.y = Mathf.Min (min.y, points[i].y);
			}
			center = (min + max) * 0.5f;
			maxDist = Mathf.Max (max.x - min.x, max.y - min.y) * 0.5f;
		}
		else
		{
			// Make min and max a rect with the origin in the center
			for (int i=0; i<points.Length; i++)
			{
				maxDist = Mathf.Max (maxDist, points[i].x);
				maxDist = Mathf.Max (maxDist, -points[i].x);
				maxDist = Mathf.Max (maxDist, points[i].y);
				maxDist = Mathf.Max (maxDist, -points[i].y);
			}
		}
		
		if (maxDist == 0)
			maxDist = 1;
		maxDist *= 1.35f;
		return new Rect (center.x-maxDist, center.y-maxDist, maxDist*2, maxDist*2);
	}
	
	private Rect m_BlendRect;
	private int m_SelectedPoint = -1;
	private bool s_DraggingPoint = false;
	
	private float ConvertFloat (float input, float fromMin, float fromMax, float toMin, float toMax)
	{
		float lerp = (input - fromMin) / (fromMax - fromMin);
		return toMin * (1 - lerp) + toMax * lerp;
	}
	
	private static Color s_VisBgColor          = !EditorGUIUtility.isProSkin ? new Color (0.95f, 0.95f, 1.00f)        : new Color (0.20f, 0.20f, 0.20f);
	private static Color s_VisWeightColor      = !EditorGUIUtility.isProSkin ? new Color (0.50f, 0.60f, 0.90f, 0.80f) : new Color (0.65f, 0.75f, 1.00f, 0.65f);
	private static Color s_VisWeightShapeColor = !EditorGUIUtility.isProSkin ? new Color (0.40f, 0.65f, 1.00f, 0.15f) : new Color (0.40f, 0.65f, 1.00f, 0.12f);
	private static Color s_VisWeightLineColor  = !EditorGUIUtility.isProSkin ? new Color (0    , 0    , 0    , 0.30f) : new Color (1    , 1    , 1    , 0.60f);
	private static Color s_VisPointColor       = !EditorGUIUtility.isProSkin ? new Color (0.50f, 0.70f, 1.00f)        : new Color (0.50f, 0.70f, 1.00f);
	private static Color s_VisPointEmptyColor  = !EditorGUIUtility.isProSkin ? new Color (0.80f, 0.80f, 0.80f)        : new Color (0.60f, 0.60f, 0.60f);
	private static Color s_VisPointOverlayColor= !EditorGUIUtility.isProSkin ? new Color (0    , 0    , 0    , 0.20f) : new Color (1    , 1    , 1    , 0.40f);
	private static Color s_VisSamplerColor     = !EditorGUIUtility.isProSkin ? new Color (1.00f, 0.40f, 0.40f)        : new Color (1.00f, 0.40f, 0.40f);
	
	private void BlendGraph2D (Rect area)
	{			
		if (m_DirtyVisualization)
		{
			UpdateBlendVisualization ();
			ValidatePositions ();
		}
		
		// Get points array from child objects
		Vector2[] points = GetMotionPositions ();
		int[] presences = GetMotionToActiveMotionIndices ();
		
		Vector2 min = new Vector2 (m_BlendRect.xMin, m_BlendRect.yMin);
		Vector2 max = new Vector2 (m_BlendRect.xMax, m_BlendRect.yMax);
		
		// Move points to GUI space
		for (int i=0; i<points.Length; i++)
		{
			points[i].x = ConvertFloat (points[i].x, min.x, max.x, area.xMin, area.xMax);
			points[i].y = ConvertFloat (points[i].y, min.y, max.y, area.yMax, area.yMin);
		}
		
		// Get the input blend info
		string currentParameterX = m_BlendTree.blendParameter;
		string currentParameterY = m_BlendTree.blendParameterY;
		float inputX = m_BlendTree.GetInputBlendValue(currentParameterX);
		float inputY = m_BlendTree.GetInputBlendValue(currentParameterY);
		
		// Get child weights
		int activeChildCount = GetActiveMotionPositions ().Length;
		if (m_Weights == null || activeChildCount != m_Weights.Length)
			m_Weights = new float[activeChildCount];
		
		AnimatorUtility.CalculateRootBlendTreeChildWeights (m_VisBlendTree.animator, 0, m_VisBlendTree.animator.GetCurrentAnimatorStateInfo (0).nameHash, m_Weights, inputX, inputY);
		
		// Move input into GUI space
		inputX = area.x + Mathf.InverseLerp (min.x, max.x, inputX) * area.width;
		inputY = area.y + (1-Mathf.InverseLerp (min.y, max.y, inputY)) * area.height;
		Rect inputRect = new Rect (inputX - 5, inputY - 5, 11, 11);
		
		int drag2dId = GUIUtility.GetControlID (m_BlendAnimationID, FocusType.Native);
		
		Event evt = Event.current;
		switch (evt.GetTypeForControl (drag2dId))
		{
			case EventType.Repaint:
				GUI.color = s_VisBgColor;
				GUI.DrawTexture (area, EditorGUIUtility.whiteTexture);
				
				// Draw weight texture
				if (m_ReorderableList.index < 0)
				{
					Color col = s_VisWeightColor;
					col.a *= 0.75f;
					GUI.color = col;
					GUI.DrawTexture (area, m_BlendTex);
				}
				else if (presences[m_ReorderableList.index] >= 0)
				{
					GUI.color = s_VisWeightColor;
					GUI.DrawTexture (area, m_WeightTexs[presences[m_ReorderableList.index]]);
				}
				GUI.color = Color.white;
				
				// Draw the weight circles
				if (!s_DraggingPoint)
				{
					for (int i=0; i<points.Length; i++)
						if (presences[i] >= 0)
							DrawWeightShape (points[i], m_Weights[presences[i]], 0);
					for (int i=0; i<points.Length; i++)
						if (presences[i] >= 0)
							DrawWeightShape (points[i], m_Weights[presences[i]], 1);
				}
				
				// Draw the animation points
				for (int i=0; i<points.Length; i++)
				{
					Rect pointRect = new Rect (points[i].x-6, points[i].y-6, 13, 13);
					bool drawSelected = (m_ReorderableList.index == i) ? true : false;
					
					if (presences[i] < 0)
						GUI.color = s_VisPointEmptyColor;
					else
						GUI.color = s_VisPointColor;
					GUI.DrawTexture (pointRect, drawSelected ? styles.pointIconSelected : styles.pointIcon);
					
					if (drawSelected)
					{
						GUI.color = s_VisPointOverlayColor;
						GUI.DrawTexture (pointRect, styles.pointIconOverlay);
					}
				}
				
				// Draw the input (sampler) point
				if (!s_DraggingPoint)
				{
					GUI.color = s_VisSamplerColor;
					GUI.DrawTexture (inputRect, styles.samplerIcon);
				}
				GUI.color = Color.white;
				
				break;
			case EventType.MouseDown:
				if (inputRect.Contains (evt.mousePosition))
				{
					evt.Use ();
					GUIUtility.hotControl = drag2dId;
					m_SelectedPoint = -1;
				}
				else if (area.Contains (evt.mousePosition))
				{
					m_ReorderableList.index = -1;
					
					for (int i=0; i<points.Length; i++)
					{
						Rect pointRect = new Rect (points[i].x-4, points[i].y-4, 9, 9);
						if (pointRect.Contains (evt.mousePosition))
						{
							evt.Use ();
							GUIUtility.hotControl = drag2dId;
							m_SelectedPoint = i;
							m_ReorderableList.index = i;
						}
					}
					
					// Use in any case so we deselect point and get repaint.
					evt.Use ();
				}
				break;
			case EventType.MouseDrag:
				if (GUIUtility.hotControl != drag2dId)
					break;
				
				if (m_SelectedPoint == -1)
				{
					// Convert mouse position to point in blend space
					Vector2 mousePosition;
					mousePosition.x = ConvertFloat (evt.mousePosition.x, area.xMin, area.xMax, min.x, max.x);
					mousePosition.y = ConvertFloat (evt.mousePosition.y, area.yMax, area.yMin, min.y, max.y);
					
					// Set blend values
					m_BlendTree.SetInputBlendValue(currentParameterX, mousePosition.x);
					m_BlendTree.SetInputBlendValue(currentParameterY, mousePosition.y);
					if (parentBlendTree)
					{
						parentBlendTree.SetInputBlendValue(currentParameterX, mousePosition.x);
						parentBlendTree.SetInputBlendValue(currentParameterY, mousePosition.y);
						if (blendParameterInputChanged != null)
							blendParameterInputChanged (parentBlendTree);
					}
					
					if (blendParameterInputChanged != null)
						blendParameterInputChanged (m_BlendTree);
					
					evt.Use ();
				}
				else
				{
					for (int i=0; i<points.Length; i++)
					{
						if (m_SelectedPoint == i)
						{
							// Convert mouse position to point in blend space
							Vector2 mousePosition;
							mousePosition.x = ConvertFloat (evt.mousePosition.x, area.xMin, area.xMax, min.x, max.x);
							mousePosition.y = ConvertFloat (evt.mousePosition.y, area.yMax, area.yMin, min.y, max.y);
							
							float minDiff = (max.x - min.x) / area.width;
							mousePosition.x = MathUtils.RoundBasedOnMinimumDifference (mousePosition.x, minDiff);
							mousePosition.y = MathUtils.RoundBasedOnMinimumDifference (mousePosition.y, minDiff);
							mousePosition.x = Mathf.Clamp (mousePosition.x, -10000, 10000);
							mousePosition.y = Mathf.Clamp (mousePosition.y, -10000, 10000);
							
							SerializedProperty child = m_Childs.GetArrayElementAtIndex (i);
							SerializedProperty position = child.FindPropertyRelative ("m_Position");
							position.vector2Value = mousePosition;
							
							evt.Use ();
							m_DirtyVisualization = true;
							s_DraggingPoint = true;
						}
					}
				}
				break;
			case EventType.MouseUp:
				if (GUIUtility.hotControl != drag2dId)
					break;
				
				evt.Use ();
				GUIUtility.hotControl = 0;
				s_DraggingPoint = false;
				
				for (int i=0; i<points.Length; i++)
					if (m_SelectedPoint == i)
						m_DirtyVisualization = true;
				break;
		}
		
		// Draw message
		if (m_ReorderableList.index >= 0 && presences[m_ReorderableList.index] < 0)
			ShowHelp (area, EditorGUIUtility.TempContent ("The selected child has no Motion assigned."));
		else if (m_WarningMessage != null)
			ShowHelp (area, EditorGUIUtility.TempContent (m_WarningMessage));
	}
	
	private void ShowHelp (Rect area, GUIContent content)
	{
		float height = EditorStyles.helpBox.CalcHeight (content, area.width);
		GUI.Label (new Rect (area.x, area.y, area.width, height), content, EditorStyles.helpBox);
	}
	
	private void ValidatePositions ()
	{
		m_WarningMessage = null;
		Vector2[] points = GetMotionPositions ();
		
		// Check for duplicate positions (relevant for all blend types)
		bool duplicatePositions = m_BlendRect.width == 0 || m_BlendRect.height == 0;
		for (int i=0; i<points.Length; i++)
		{
			for (int j=0; j<i && !duplicatePositions; j++)
			{
				if (((points[i]-points[j]) / m_BlendRect.height).sqrMagnitude < 0.0001f)
				{
					duplicatePositions = true;
					break;
				}
			}
		}
		if (duplicatePositions)
		{
			m_WarningMessage = "Two or more of the positions are too close to each other.";
			return;
		}
		
		// Checks for individual blend types below
		
		if (m_BlendType.intValue == 1)
		{
			List<float> angles = points.Where (e => e != Vector2.zero).Select (e => Mathf.Atan2 (e.y, e.x)).OrderBy (e => e).ToList ();
			float maxAngle = 0;
			float minAngle = 180;
			for (int i=0; i<angles.Count; i++)
			{
				float angle = angles[(i+1) % angles.Count] - angles[i];
				if (i == angles.Count-1)
					angle += Mathf.PI * 2;
				if (angle > maxAngle)
					maxAngle = angle;
				if (angle < minAngle)
					minAngle = angle;
			}
			if (maxAngle * Mathf.Rad2Deg >= 180)
				m_WarningMessage = "Simple Directional blend should have motions with directions less than 180 degrees apart.";
			else if (minAngle * Mathf.Rad2Deg < 2)
				m_WarningMessage = "Simple Directional blend should not have multiple motions in almost the same direction.";
		}
		
		else if (m_BlendType.intValue == 2)
		{
			// Check if this blend type has a motion in the center.
			bool hasCenter = false;
			for (int i=0; i<points.Length; i++)
			{
				if (points[i] == Vector2.zero)
				{
					hasCenter = true;
					break;
				}
			}
			if (!hasCenter)
				m_WarningMessage = "Freeform Directional blend should have one motion at position (0,0) to avoid discontinuities.";
		}
	}
	
	private int kNumCirclePoints = 20;
	private void DrawWeightShape (Vector2 point, float weight, int pass)
	{
		if (weight <= 0)
			return;
		point.x = Mathf.Round (point.x);
		point.y = Mathf.Round (point.y);
		float radius = 20 * Mathf.Sqrt (weight);
		
		// Calculate points in a circle
		Vector3[] points = new Vector3[kNumCirclePoints+2];
		for (int i=0; i<kNumCirclePoints; i++)
		{
			float v = (float)i / kNumCirclePoints;
			points[i+1] = new Vector3 (point.x + 0.5f, point.y + 0.5f, 0) + new Vector3 (Mathf.Sin (v * 2* Mathf.PI), Mathf.Cos (v * 2 * Mathf.PI), 0) * radius;
		}
		// First and last point have to meet each other in a straight line; otherwise we'll get a gap
		points[0] = points[kNumCirclePoints+1] = (points[1] + points[kNumCirclePoints]) * 0.5f;
		
		if (pass == 0)
		{
			// Draw disc
			Handles.color = s_VisWeightShapeColor;
			Handles.DrawSolidDisc (point + new Vector2 (0.5f, 0.5f), -Vector3.forward, radius);
		}
		else
		{
			// Draw outline
			Handles.color = s_VisWeightLineColor;
			Handles.DrawAAPolyLine (points);
		}
	}
	
	private void DrawAnimation (float val, float min, float max, bool selected, Rect area)
	{
		float top = area.y;
		Rect leftRect = new Rect(min, top, val-min, area.height);
		Rect rightRect = new Rect(val, top, max-val, area.height);
		styles.triangleLeft.Draw(leftRect, selected, selected, false, false);
		styles.triangleRight.Draw(rightRect, selected, selected, false, false);
		area.height -= 1;
		Color oldColor = Handles.color;
		Color newColor = selected? new Color(1f, 1f, 1f, 0.6f) : new Color(1f, 1f, 1f, 0.4f);
		Handles.color = newColor;
		if (selected)
			Handles.DrawLine(new Vector3(val, top, 0), new Vector3(val, top+area.height, 0));
		Vector3[] points = new Vector3[2]{new Vector3(min, top+area.height, 0f), new Vector3(val, top, 0f)};
		Handles.DrawAAPolyLine(points);
		points = new Vector3[2]{new Vector3(val, top, 0f), new Vector3(max, top+area.height, 0f)};
		Handles.DrawAAPolyLine(points);
		Handles.color = oldColor;
	}
	
	public void EndDragChild (ReorderableList list)
	{
		List<float> dragThresholds = new List<float>();
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
			SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
			dragThresholds.Add(threshold.floatValue);	
		}
		dragThresholds.Sort();
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			SerializedProperty child = m_Childs.GetArrayElementAtIndex(i);
			SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
			threshold.floatValue = dragThresholds[i];	
		}
		serializedObject.ApplyModifiedProperties ();
		m_HierarchyChanged = true;
		m_DirtyVisualization = true;
	}

	private void DrawHeader (Rect headerRect)
	{
		headerRect.xMin += 14; // Ignore width used by drag-handles while calculating column widths.
		headerRect.y++;
		headerRect.height = 16;
		
		Rect[] rects = GetRowRects (headerRect, ParameterCount);
		int col = 0;
		
		rects[col].xMin = rects[col].xMin - 14; // Make first column extend into space of drag-handles.
		GUI.Label(rects[col], EditorGUIUtility.TempContent("Motion"), EditorStyles.label);
		col++;
		if (m_Childs.arraySize >= 1)
		{
			if (m_BlendType.intValue == 0)
			{
				GUI.Label(rects[col], EditorGUIUtility.TempContent("Threshold"), EditorStyles.label);
				col++;
			}
			else
			{
				GUI.Label(rects[col], EditorGUIUtility.TempContent("Pos X"), EditorStyles.label);
				col++;
				GUI.Label(rects[col], EditorGUIUtility.TempContent("Pos Y"), EditorStyles.label);
				col++;
			}
			
			GUI.Label(rects[col], styles.speedIcon, styles.headerIcon);
			col++;
			GUI.Label(rects[col], styles.mirrorIcon, styles.headerIcon);
		}		
	}	
		
		
	public void AddButton (Rect rect, ReorderableList list)
	{
		GenericMenu menu = new GenericMenu();
		menu.AddItem(new GUIContent("Add Motion Field"), false, AddChildAnimation);
		menu.AddItem(EditorGUIUtility.TempContent("New Blend Tree"), false, AddBlendTreeCallback);
		menu.Popup(rect, 0);	
	}

	public void RemoveButton (ReorderableList list)
	{
		m_Childs.DeleteArrayElementAtIndex(list.index);
		if (list.index >= m_Childs.arraySize)
			list.index = m_Childs.arraySize -1;
		SetMinMaxThresholds();
		m_HierarchyChanged = true;
		m_DirtyVisualization = true;
	}
	
	private Rect[] GetRowRects (Rect r, int parameterCount)
	{
		Rect[] rects = new Rect[3 + parameterCount];
		
		float remainingWidth = r.width;
		float mirrorWidth = 16;
		remainingWidth -= mirrorWidth;
		remainingWidth -= 8+8+8+(4*(parameterCount-1));
		float numberWidth = Mathf.FloorToInt (remainingWidth * 0.2f);
		float motionWidth = remainingWidth - numberWidth * (parameterCount + 1);
		
		float x = r.x;
		int col = 0;
		
		rects[col] = new Rect (x, r.y, motionWidth, r.height);
		x += motionWidth + 8;
		col++;
		
		for (int i=0; i<parameterCount; i++)
		{
			rects[col] = new Rect (x, r.y, numberWidth, r.height);
			x += numberWidth + 4;
			col++;
		}
		x += 4;

		rects[col] = new Rect (x, r.y, numberWidth, r.height);
		x += numberWidth + 8;
		col++;
		
		rects[col] = new Rect (x, r.y, mirrorWidth, r.height);
		
		return rects;
	}
	
	public void DrawChild (Rect r, int index, bool isActive, bool isFocused)
	{
		SerializedProperty child = m_Childs.GetArrayElementAtIndex(index);
		SerializedProperty motion = child.FindPropertyRelative("m_Motion");
		
		r.y++;
		r.height = 16;
		Rect[] rects = GetRowRects (r, ParameterCount);
		int col = 0;
		
		// show a property field for the motion clip
		EditorGUI.BeginChangeCheck();
		
		Motion prevMotion = m_BlendTree.GetMotion(index);
		EditorGUI.PropertyField(rects[col], motion, GUIContent.none);
		col++;
		if (EditorGUI.EndChangeCheck())
		{
			if (prevMotion is BlendTree && prevMotion != (motion.objectReferenceValue as Motion))
			{
				if(EditorUtility.DisplayDialog("Changing BlendTree will delete previous BlendTree", "You cannot undo this action.", "Delete", "Cancel"))
				{
					MecanimUtilities.DestroyBlendTreeRecursive(prevMotion as BlendTree);										
				}
				else
				{
					motion.objectReferenceValue = prevMotion;
				}				
			}
			
			m_HierarchyChanged = true;
			m_DirtyVisualization = true;
		}
		
		// use a delayed text field and force re-sort if value is manually changed
		if (ParameterCount == 1)
		{
			// Threshold in 1D blending
			SerializedProperty threshold = child.FindPropertyRelative("m_Threshold");
			EditorGUI.BeginDisabledGroup (m_UseAutomaticThresholds.boolValue);
			float nr = threshold.floatValue;
			EditorGUI.BeginChangeCheck();
			string floatStr = EditorGUI.DelayedTextField (rects[col], nr.ToString(), "inftynaeINFTYNAE0123456789.,-", EditorStyles.textField);
			col++;
			if (EditorGUI.EndChangeCheck())
			{
				if (float.TryParse (floatStr, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out nr))
				{ 
					//GUIUtility.keyboardControl = 0;
					threshold.floatValue = nr;
					serializedObject.ApplyModifiedProperties ();
					m_BlendTree.SortChildren();
					SetMinMaxThresholds();
					GUI.changed = true;
				}
			}
			EditorGUI.EndDisabledGroup ();
		}
		else
		{
			// Position in 2D blending
			SerializedProperty position = child.FindPropertyRelative("m_Position");
			Vector2 pos = position.vector2Value;
			for (int i=0; i<2; i++)
			{
				EditorGUI.BeginChangeCheck();
				string valStr = EditorGUI.DelayedTextField (rects[col], pos[i].ToString(), "inftynaeINFTYNAE0123456789.,-", EditorStyles.textField);
				col++;
				if (EditorGUI.EndChangeCheck())
				{
					float coord;
					if (float.TryParse (valStr, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture.NumberFormat, out coord))
					{ 
						//GUIUtility.keyboardControl = 0;
						pos[i] = Mathf.Clamp (coord, -10000, 10000);
						position.vector2Value = pos;
						serializedObject.ApplyModifiedProperties ();
						m_DirtyVisualization = true;
						GUI.changed = true;
					}
				}
			}
		}
		
		// If this is an animation, include the time scale.
		if (motion.objectReferenceValue is AnimationClip)
		{
			SerializedProperty timeScale = child.FindPropertyRelative("m_TimeScale");
			EditorGUI.PropertyField(rects[col], timeScale, GUIContent.none);
		}
		else
		{
			// Otherwise show disabled dummy field with default value of 1.
			EditorGUI.BeginDisabledGroup (true);
			EditorGUI.IntField (rects[col], 1);
			EditorGUI.EndDisabledGroup ();
		}
		col++;
		
		// If this is a humanoid animation, include the mirror toggle.
		if (motion.objectReferenceValue is AnimationClip && (motion.objectReferenceValue as AnimationClip).isHumanMotion)
		{
			SerializedProperty mirror = child.FindPropertyRelative("m_Mirror");
			EditorGUI.PropertyField(rects[col], mirror, GUIContent.none);
			
			SerializedProperty cycle = child.FindPropertyRelative("m_CycleOffset");
			cycle.floatValue = mirror.boolValue ? 0.5f : 0.0f;
		}
		else
		{
			// Otherwise show disabled dummy toggle that's disabled.
			EditorGUI.BeginDisabledGroup (true);
			EditorGUI.Toggle (rects[col], false);
			EditorGUI.EndDisabledGroup ();
		}
	}
		
	private bool AllMotions()
	{
		bool allClips = true;
		for (int i = 0; i < m_Childs.arraySize && allClips; i++)
		{
			SerializedProperty motion = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_Motion");
			allClips = motion.objectReferenceValue is AnimationClip;
		}
		return allClips;
	}
		
	private void AutoCompute()
	{
		
		if (ParameterCount == 1)
		{
			EditorGUILayout.PropertyField (m_UseAutomaticThresholds, EditorGUIUtility.TempContent ("Automate Thresholds"));
			m_ShowCompute.target = !m_UseAutomaticThresholds.boolValue;
		}
		else
		{
			m_ShowCompute.target = true;
		}
		m_ShowAdjust.target = AllMotions();
		
		if (EditorGUILayout.BeginFadeGroup (m_ShowCompute.faded))
		{
			Rect controlRect = EditorGUILayout.GetControlRect ();
			GUIContent label = (ParameterCount == 1) ?
				EditorGUIUtility.TempContent ("Compute Thresholds") :
				EditorGUIUtility.TempContent ("Compute Positions");
			controlRect = EditorGUI.PrefixLabel (controlRect, 0, label);
			
			if (EditorGUI.ButtonMouseDown (controlRect, EditorGUIUtility.TempContent ("Select"), FocusType.Passive, EditorStyles.popup))
			{
				GenericMenu menu = new GenericMenu ();
				if (ParameterCount == 1)
				{
					AddComputeMenuItems (menu, string.Empty, ChildPropertyToCompute.Threshold);
				}
				else
				{
					menu.AddItem (new GUIContent ("Velocity XZ"), false, ComputePositionsFromVelocity);
					menu.AddItem (new GUIContent ("Speed And Angular Speed"), false, ComputePositionsFromSpeedAndAngularSpeed);
					AddComputeMenuItems (menu, "X Position From/", ChildPropertyToCompute.PositionX);
					AddComputeMenuItems (menu, "Y Position From/", ChildPropertyToCompute.PositionY);
				}
				menu.DropDown (controlRect);
			}
		}
		EditorGUILayout.EndFadeGroup();
		
		if (EditorGUILayout.BeginFadeGroup (m_ShowAdjust.faded))
		{
			Rect controlRect = EditorGUILayout.GetControlRect ();
			controlRect = EditorGUI.PrefixLabel (controlRect, 0, EditorGUIUtility.TempContent ("Adjust Time Scale"));
			
			if (EditorGUI.ButtonMouseDown (controlRect, EditorGUIUtility.TempContent ("Select"), FocusType.Passive, EditorStyles.popup))
			{
				GenericMenu menu = new GenericMenu ();
				menu.AddItem (new GUIContent ("Homogeneous Speed"), false, ComputeTimeScaleFromSpeed);
				menu.AddItem (new GUIContent ("Reset Time Scale"), false, ResetTimeScale);
				menu.DropDown (controlRect);
			}
		}
		EditorGUILayout.EndFadeGroup();
	}
	
	enum ChildPropertyToCompute
	{
		Threshold,
		PositionX,
		PositionY
	}
	 
	delegate float GetFloatFromMotion (Motion motion, float mirrorMultiplier);
		
	void AddComputeMenuItems (GenericMenu menu, string menuItemPrefix, ChildPropertyToCompute prop)
	{
		menu.AddItem (new GUIContent (menuItemPrefix + "Speed"), false, ComputeFromSpeed, prop);
		menu.AddItem (new GUIContent (menuItemPrefix + "Velocity X"), false, ComputeFromVelocityX, prop);
		menu.AddItem (new GUIContent (menuItemPrefix + "Velocity Y"), false, ComputeFromVelocityY, prop);
		menu.AddItem (new GUIContent (menuItemPrefix + "Velocity Z"), false, ComputeFromVelocityZ, prop);
		menu.AddItem (new GUIContent (menuItemPrefix + "Angular Speed (Rad)"), false, ComputeFromAngularSpeedRadians, prop);
		menu.AddItem (new GUIContent (menuItemPrefix + "Angular Speed (Deg)"), false, ComputeFromAngularSpeedDegrees, prop);
	}
	
	private void ComputeFromSpeed (object obj)
	{
		ChildPropertyToCompute prop = (ChildPropertyToCompute)obj;
		ComputeProperty ((Motion m, float mirrorMultiplier) => m.apparentSpeed, prop);
	}
	
	private void ComputeFromVelocityX (object obj)
	{
		ChildPropertyToCompute prop = (ChildPropertyToCompute)obj;
		ComputeProperty ((Motion m, float mirrorMultiplier) => m.averageSpeed.x * mirrorMultiplier, prop);
	}
	
	private void ComputeFromVelocityY (object obj)
	{
		ChildPropertyToCompute prop = (ChildPropertyToCompute)obj;
		ComputeProperty ((Motion m, float mirrorMultiplier) => m.averageSpeed.y, prop);
	}
	
	private void ComputeFromVelocityZ (object obj)
	{
		ChildPropertyToCompute prop = (ChildPropertyToCompute)obj;
		ComputeProperty ((Motion m, float mirrorMultiplier) => m.averageSpeed.z, prop);
	}
	
	private void ComputeFromAngularSpeedDegrees (object obj)
	{
		ChildPropertyToCompute prop = (ChildPropertyToCompute)obj;
		ComputeProperty ((Motion m, float mirrorMultiplier) => m.averageAngularSpeed * 180.0f / Mathf.PI * mirrorMultiplier, prop);
	}
		
	private void ComputeFromAngularSpeedRadians (object obj)
	{
		ChildPropertyToCompute prop = (ChildPropertyToCompute)obj;
		ComputeProperty ((Motion m, float mirrorMultiplier) => m.averageAngularSpeed * mirrorMultiplier, prop);
	}
		
	private void ComputeProperty (GetFloatFromMotion func, ChildPropertyToCompute prop)
	{
		m_UseAutomaticThresholds.boolValue = false;
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			SerializedProperty motion = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_Motion");
			SerializedProperty mirror = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_Mirror");
			Motion motionObj = motion.objectReferenceValue as Motion;
			if (motionObj != null)
			{
				float val = func (motionObj, mirror.boolValue ? -1 : 1);
				if (prop == ChildPropertyToCompute.Threshold)
				{
					
					SerializedProperty threshold = m_Childs.GetArrayElementAtIndex (i).FindPropertyRelative ("m_Threshold");
					threshold.floatValue = val;
				}
				else
				{
					SerializedProperty position = m_Childs.GetArrayElementAtIndex (i).FindPropertyRelative ("m_Position");
					Vector2 pos = position.vector2Value;
					if (prop == ChildPropertyToCompute.PositionX)
						pos.x = val;
					else
						pos.y = val;
					position.vector2Value = pos;
				}
			}
		}
		m_SerializedObject.ApplyModifiedProperties();
		
		if (prop == ChildPropertyToCompute.Threshold)
		{
			SortByThreshold();
			SetMinMaxThreshold();
		}
		m_DirtyVisualization = true;
	}
		
	private void ComputePositionsFromVelocity ()
	{
		ComputeFromVelocityX (ChildPropertyToCompute.PositionX);
		ComputeFromVelocityZ (ChildPropertyToCompute.PositionY);
	}

	private void ComputePositionsFromSpeedAndAngularSpeed ()
	{
		ComputeFromAngularSpeedRadians (ChildPropertyToCompute.PositionX);
		ComputeFromSpeed (ChildPropertyToCompute.PositionY);
	}
	
	private void ComputeTimeScaleFromSpeed ()
	{
		float apparentSpeed = m_BlendTree.apparentSpeed;
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			SerializedProperty motion = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_Motion");
			AnimationClip clip = motion.objectReferenceValue as AnimationClip;
			if (clip !=null) 
			{
				if (clip.isAnimatorMotion)
				{
					SerializedProperty timeScale = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_TimeScale");
					timeScale.floatValue = apparentSpeed / clip.apparentSpeed;
				}
				else
				{
					Debug.LogWarning("Could not adjust time scale for " + clip.name + " because it is not a muscle clip");
				}
			}
		}
		m_SerializedObject.ApplyModifiedProperties ();
	}
	
	private void ResetTimeScale ()
	{
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			SerializedProperty motion = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_Motion");
			AnimationClip clip = motion.objectReferenceValue as AnimationClip;
			if (clip != null && clip.isAnimatorMotion)
			{
				SerializedProperty timeScale = m_Childs.GetArrayElementAtIndex(i).FindPropertyRelative("m_TimeScale");
				timeScale.floatValue = 1;
			}
		}
		m_SerializedObject.ApplyModifiedProperties ();
	}
	
	private void SortByThreshold ()
	{
		m_SerializedObject.Update();
		for (int i = 0; i < m_Childs.arraySize; i++)
		{
			float minThreshold = Mathf.Infinity;
			int minIndex = -1;
			for (int j = i; j < m_Childs.arraySize; j++)
			{
				SerializedProperty testElement = m_Childs.GetArrayElementAtIndex(j);
				float testThreshold = testElement.FindPropertyRelative("m_Threshold").floatValue;
				if (testThreshold < minThreshold)
				{
					minThreshold = testThreshold;
					minIndex = j;
				}
			}
			m_Childs.MoveArrayElement(minIndex, i);
		}
			
		m_SerializedObject.ApplyModifiedProperties();
		m_HierarchyChanged = true;
		m_DirtyVisualization = true;
		}
		
	private void SetMinMaxThreshold ()
	{
		m_SerializedObject.Update();
		SerializedProperty minThreshold = m_Childs.GetArrayElementAtIndex(0).FindPropertyRelative("m_Threshold");
		SerializedProperty maxThreshold = m_Childs.GetArrayElementAtIndex(m_Childs.arraySize - 1).FindPropertyRelative("m_Threshold");
		m_MinThreshold.floatValue = Mathf.Min (minThreshold.floatValue, maxThreshold.floatValue);
		m_MaxThreshold.floatValue = Mathf.Max (minThreshold.floatValue, maxThreshold.floatValue);
		m_SerializedObject.ApplyModifiedProperties();	
	}

	void AddChildAnimation()
	{
		m_BlendTree.AddAnimationClip(null);
		SetNewThresholdAndPosition(m_BlendTree.childCount-1);
		m_ReorderableList.index = m_Childs.arraySize - 1;
		m_HierarchyChanged = true;
		m_DirtyVisualization = true;
	}

	void AddBlendTreeCallback()
	{
		BlendTree childBlendTree = m_BlendTree.AddNewBlendTree();
		childBlendTree.name = "Blend Tree";		
		SetNewThresholdAndPosition(m_BlendTree.childCount - 1);
		
		m_ReorderableList.index = m_Childs.arraySize - 1;
		m_HierarchyChanged = true;
		m_DirtyVisualization = true;
	}
		
	
	void SetNewThresholdAndPosition (int index)
	{
		serializedObject.Update();
		
		// Set new threshold
		if (!m_UseAutomaticThresholds.boolValue)
		{
			float newThreshold = 0f;
			if (m_Childs.arraySize >= 3 && index == m_Childs.arraySize -1)
			{
				float threshold1 = m_Childs.GetArrayElementAtIndex(index-2).FindPropertyRelative("m_Threshold").floatValue;
				float threshold2 = m_Childs.GetArrayElementAtIndex(index-1).FindPropertyRelative("m_Threshold").floatValue;
				newThreshold = threshold2 + (threshold2 - threshold1);
			}
			else
			{
				if (m_Childs.arraySize == 1)
					newThreshold = 0;
				else
					newThreshold = m_Childs.GetArrayElementAtIndex(m_Childs.arraySize -1).FindPropertyRelative("m_Threshold").floatValue + 1;
			}
			SerializedProperty addedThreshold = m_Childs.GetArrayElementAtIndex(index).FindPropertyRelative("m_Threshold");
			addedThreshold.floatValue = newThreshold;
			SetMinMaxThresholds();
		}
		
		// Set new position
		Vector2 newPosition = Vector2.zero;
		if (m_Childs.arraySize >= 1)
		{
			Vector2 center = m_BlendRect.center;
			Vector2[] points = GetMotionPositions ();
			float goodMinDist = m_BlendRect.width * 0.07f;
			bool satisfied = false;
			// Try to place new point along circle around center until successful
			for (int iter=0; iter<24; iter++)
			{
				satisfied = true;
				for (int i=0; i<points.Length && satisfied; i++)
					if (i != index && Vector2.Distance (points[i], newPosition) < goodMinDist)
						satisfied = false;
				
				if (satisfied)
					break;
				
				float radians = iter * 15 * Mathf.Deg2Rad;
				newPosition = center + new Vector2 (-Mathf.Cos (radians), Mathf.Sin (radians)) * 0.37f * m_BlendRect.width;
				newPosition.x = MathUtils.RoundBasedOnMinimumDifference (newPosition.x, m_BlendRect.width * 0.005f);
				newPosition.y = MathUtils.RoundBasedOnMinimumDifference (newPosition.y, m_BlendRect.width * 0.005f);
			}
		}
		SerializedProperty addedPosition = m_Childs.GetArrayElementAtIndex (index).FindPropertyRelative ("m_Position");
		addedPosition.vector2Value = newPosition;
		
		serializedObject.ApplyModifiedProperties();
	}
		
	public override bool HasPreviewGUI ()
	{
		if (m_PreviewBlendTree != null)
		{
			return m_PreviewBlendTree.HasPreviewGUI();
		}
		return false;
	}
	
	public override void OnPreviewSettings ()
	{
		if (m_PreviewBlendTree != null)
			m_PreviewBlendTree.OnPreviewSettings();
	}
	
	public override void OnInteractivePreviewGUI (Rect r, GUIStyle background)
	{
		if (m_PreviewBlendTree != null)
			m_PreviewBlendTree.OnInteractivePreviewGUI(r, background);
	}
	
	public void OnDisable ()
	{
		if (m_PreviewBlendTree != null)
			m_PreviewBlendTree.OnDisable ();
	}
	
	public void OnDestroy ()
	{
		if (m_PreviewBlendTree != null)
			m_PreviewBlendTree.OnDestroy ();
		if (m_VisBlendTree != null)
			m_VisBlendTree.Destroy ();
		if (m_VisInstance != null)
			DestroyImmediate (m_VisInstance);
		for (int i=0; i<m_WeightTexs.Count; i++)
			DestroyImmediate (m_WeightTexs[i]);
		if (m_BlendTex != null)
			DestroyImmediate(m_BlendTex);
		
	}
	
	public void DoHierarchyChangedCallback ()
	{
		if (m_HierarchyChanged && blendTreeHierarchyChanged != null)
			blendTreeHierarchyChanged (m_BlendTree);
		
		m_HierarchyChanged = false;
	}	
}	
	
class VisualizationBlendTree
{
    private AnimatorController m_Controller;
	private StateMachine m_StateMachine;
	private State m_State;
	private BlendTree m_BlendTree;
	private Animator m_Animator;
	
	public Animator animator { get { return m_Animator; } }
	
	public void Init (BlendTree blendTree, Animator animator)
	{
		m_BlendTree = blendTree;
		m_Animator = animator;
		m_Animator.logWarnings = false;
        m_Animator.fireEvents = false;
		CreateStateMachine ();
	}

	private void CreateParameters()
	{
		int parameterCount = m_Controller.parameterCount;
		for (int i = 0; i < parameterCount; i++)
			m_Controller.RemoveParameter (0);
		for (int i = 0; i < m_BlendTree.recursiveBlendParameterCount; i++)
			m_Controller.AddParameter(m_BlendTree.GetRecursiveBlendParameter(i), AnimatorControllerParameterType.Float);
	}
	
	private void CreateStateMachine ()
	{

		if (m_Controller == null)
		{
            m_Controller = new AnimatorController();
			m_Controller.hideFlags = HideFlags.DontSave;
			m_Controller.AddLayer("preview");
		    m_StateMachine = m_Controller.GetLayerStateMachine(0);
			CreateParameters();		
			m_State = m_StateMachine.AddState ("preview");
			m_State.SetMotionInternal(m_BlendTree);
			m_State.iKOnFeet = false;
			m_State.hideFlags = HideFlags.DontSave;

            AnimatorController.SetAnimatorController(m_Animator, m_Controller);			
		}
	}
	
	private void ClearStateMachine ()
	{
		if (m_Animator != null)
            AnimatorController.SetAnimatorController(m_Animator, null);
		Object.DestroyImmediate(m_Controller);
		Object.DestroyImmediate(m_State);
		m_StateMachine = null;
		m_Controller = null;
		m_State = null;
	}
	
	public void Reset ()
	{
		ClearStateMachine ();
		CreateStateMachine ();
	}
	
	public void Destroy()
	{
		ClearStateMachine();
	}
	
	public void Update ()
	{
		for (int i = 0; i < m_BlendTree.recursiveBlendParameterCount; i++)
		{
			string blendParameter = m_BlendTree.GetRecursiveBlendParameter(i);
			float value = m_BlendTree.GetInputBlendValue (blendParameter);
			animator.SetFloat (blendParameter, value);
		}
		animator.EvaluateSM ();
	}
}
		
class PreviewBlendTree
{
    private AnimatorController m_Controller;
	private AvatarPreview m_AvatarPreview;
	private StateMachine m_StateMachine;
	private State m_State;
	private BlendTree m_BlendTree;
	
	bool m_PrevIKOnFeet;
		
	public Animator PreviewAnimator { get { return m_AvatarPreview.Animator; } }
	
	public void Init(BlendTree blendTree, Animator animator)
	{
		m_BlendTree = blendTree;
		if (m_AvatarPreview == null)
		{				
			m_AvatarPreview = new AvatarPreview(animator, m_BlendTree);
			m_AvatarPreview.OnAvatarChangeFunc = OnPreviewAvatarChanged;
			m_PrevIKOnFeet = m_AvatarPreview.IKOnFeet;
		}		

		CreateStateMachine();
	}

	public void CreateParameters()
	{
		int parameterCount = m_Controller.parameterCount;
		for (int i = 0; i < parameterCount; i++)
		{
			m_Controller.RemoveParameter(0);
		}
		for (int i = 0; i < m_BlendTree.recursiveBlendParameterCount; i++)
		{
			m_Controller.AddParameter(m_BlendTree.GetRecursiveBlendParameter(i), AnimatorControllerParameterType.Float); 
		}
	}
	
	private void CreateStateMachine()
	{
		if (m_AvatarPreview != null && m_AvatarPreview.Animator != null)			 
		{				
			if (m_Controller == null)
			{
                m_Controller = new AnimatorController();
				m_Controller.hideFlags = HideFlags.DontSave;
				m_Controller.AddLayer("preview");
			    m_StateMachine = m_Controller.GetLayerStateMachine(0);
				CreateParameters();							
			
				m_State = m_StateMachine.AddState("preview");
				m_State.SetMotionInternal(m_BlendTree);
				m_State.iKOnFeet = m_AvatarPreview.IKOnFeet;
				m_State.hideFlags = HideFlags.DontSave;

                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);				
			}

            if (AnimatorController.GetEffectiveAnimatorController(m_AvatarPreview.Animator) != m_Controller)
                AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, m_Controller);						
		}
	}
	
	private void ClearStateMachine()
	{
        if (m_AvatarPreview != null && m_AvatarPreview.Animator != null) AnimatorController.SetAnimatorController(m_AvatarPreview.Animator, null);		
		Object.DestroyImmediate(m_Controller);			
		Object.DestroyImmediate(m_State);
		m_StateMachine = null;
		m_Controller = null;
		m_State = null;
	}
	
	private void OnPreviewAvatarChanged()
	{
		ResetStateMachine();
	}
	
	public void ResetStateMachine()
	{
		ClearStateMachine();
		CreateStateMachine();
	}
	
	public void OnDisable()
	{
		ClearStateMachine();
		m_AvatarPreview.OnDestroy();
	}
	
	public void OnDestroy()
	{
		ClearStateMachine();
		if (m_AvatarPreview != null)
		{
			m_AvatarPreview.OnDestroy();
			m_AvatarPreview = null;
		}
	}
	
	private void UpdateAvatarState()
	{
		if (Event.current.type != EventType.Repaint)
			return;
		
		Animator animator = m_AvatarPreview.Animator;
		
		if (animator)
		{
			if (m_PrevIKOnFeet != m_AvatarPreview.IKOnFeet)
			{
				m_PrevIKOnFeet = m_AvatarPreview.IKOnFeet;
				Vector3 prevPos = m_AvatarPreview.Animator.rootPosition;
				Quaternion prevRotation = m_AvatarPreview.Animator.rootRotation;
				ResetStateMachine();
				m_AvatarPreview.Animator.Update(m_AvatarPreview.timeControl.currentTime);
				m_AvatarPreview.Animator.Update(0); // forces deltaPos/Rot to 0,0,0				
				m_AvatarPreview.Animator.rootPosition = prevPos;
				m_AvatarPreview.Animator.rootRotation = prevRotation;
			}
			
			if (animator)
			{
				for (int i = 0; i < m_BlendTree.recursiveBlendParameterCount; i++)
				{
					string blendParameter = m_BlendTree.GetRecursiveBlendParameter(i);
					float value = m_BlendTree.GetInputBlendValue(blendParameter);
					animator.SetFloat(blendParameter, value);
				}
			}
			
			m_AvatarPreview.timeControl.loop = true;
			
			float stateLength = 1.0f;
			float stateTime = 0.0f;
			
			if (animator.layerCount > 0)
			{
				AnimatorStateInfo stateInfo = animator.GetCurrentAnimatorStateInfo(0);
				stateLength = stateInfo.length;
				stateTime = stateInfo.normalizedTime;
			}
			
			m_AvatarPreview.timeControl.startTime = 0.0f;
			m_AvatarPreview.timeControl.stopTime = stateLength;
			
			m_AvatarPreview.timeControl.Update();
			
			float deltaTime = m_AvatarPreview.timeControl.deltaTime;
			
			if (!m_BlendTree.isLooping)
			{
				if (stateTime >= 1.0f)
				{
					deltaTime -= stateLength;
				}
				else if (stateTime < 0.0f)
				{
					deltaTime += stateLength;
				}
			}
			
			animator.Update(deltaTime);
		}
	}
	
	public void TestForReset()
	{
		if ((m_State != null && m_AvatarPreview != null && m_State.iKOnFeet != m_AvatarPreview.IKOnFeet))
		{
			ResetStateMachine();
		}
	}
	
	public bool HasPreviewGUI()
	{
		return true;
	}
	
	public void OnPreviewSettings ()
	{
		m_AvatarPreview.DoPreviewSettings ();
	}
	
	public void OnInteractivePreviewGUI (Rect r, GUIStyle background)
	{
		UpdateAvatarState();
		
		m_AvatarPreview.DoAvatarPreview(r, background);
	}
}
	

}
