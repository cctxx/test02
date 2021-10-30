using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Reflection;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditor
{
	internal class BodyMaskEditor
	{
		class Styles
		{
			public GUIContent UnityDude = EditorGUIUtility.IconContent("AvatarInspector/BodySIlhouette");
			public GUIContent PickingTexture = EditorGUIUtility.IconContent("AvatarInspector/BodyPartPicker");

			public GUIContent[] BodyPart =
			{
				EditorGUIUtility.IconContent("AvatarInspector/MaskEditor_Root"),
				EditorGUIUtility.IconContent("AvatarInspector/Torso"),

				EditorGUIUtility.IconContent("AvatarInspector/Head"),
				
				EditorGUIUtility.IconContent("AvatarInspector/LeftLeg"),
				EditorGUIUtility.IconContent("AvatarInspector/RightLeg"),
				
				EditorGUIUtility.IconContent("AvatarInspector/LeftArm"),
				EditorGUIUtility.IconContent("AvatarInspector/RightArm"),
				
				EditorGUIUtility.IconContent("AvatarInspector/LeftFingers"),
				EditorGUIUtility.IconContent("AvatarInspector/RightFingers"),

				EditorGUIUtility.IconContent("AvatarInspector/LeftFeetIk"),
				EditorGUIUtility.IconContent("AvatarInspector/RightFeetIk"),
				
				EditorGUIUtility.IconContent("AvatarInspector/LeftFingersIk"),
				EditorGUIUtility.IconContent("AvatarInspector/RightFingersIk"),
			};
		}

		static Styles styles = new Styles();


		static protected Color[] m_MaskBodyPartPicker =
		{
			new Color(255/255.0f,	144/255.0f,		0/255.0f), // root
			new Color(0/255.0f, 174/255.0f,	240/255.0f), // body
			new Color(171/255.0f, 160/255.0f,	0/255.0f), // head
			
			new Color(0/255.0f,	255/255.0f,		255/255.0f), // ll
			new Color(247/255.0f,	151/255.0f,	121/255.0f), // rl

			new Color(0/255.0f,	255/255.0f,	0/255.0f), // la
			new Color(86/255.0f,116/255.0f,	185/255.0f), // ra
															
			new Color(255/255.0f,	255/255.0f,		0/255.0f), // lh
			new Color(130/255.0f,	202/255.0f,	156/255.0f), // rh

			new Color(82/255.0f,	82/255.0f,		82/255.0f), // lfi
			new Color(255/255.0f,	115/255.0f,		115/255.0f), // rfi
			new Color(159/255.0f,	159/255.0f,		159/255.0f), // lhi
			new Color(202/255.0f,	202/255.0f,	202/255.0f), // rhi

			new Color(101/255.0f,	101/255.0f,	101/255.0f), // hi
		};

		static string sAvatarBodyMaskStr = "AvatarMask";
		static int s_Hint = sAvatarBodyMaskStr.GetHashCode();

		static public bool[] Show(bool[] bodyPartToggle, int count)
		{
			if (styles.UnityDude.image)
			{
				Rect rect = GUILayoutUtility.GetRect(styles.UnityDude, GUIStyle.none, GUILayout.MaxWidth(styles.UnityDude.image.width));
				rect.x += (Screen.width - rect.width) / 2;
				
				Color oldColor = GUI.color;
				
				if (bodyPartToggle[0])
					GUI.color = Color.green;
				else
					GUI.color = Color.red;
				
				if (styles.BodyPart[0].image)
					GUI.DrawTexture(rect, styles.BodyPart[0].image);
				
				GUI.color = new Color (0.2f, 0.2f, 0.2f, 1.0f);
				GUI.DrawTexture (rect, styles.UnityDude.image);
				
				for (int i = 1; i < count; i++)
				{
					if (bodyPartToggle[i])
						GUI.color = Color.green;
					else
						GUI.color = Color.red;
					
					if (styles.BodyPart[i].image)
						GUI.DrawTexture (rect, styles.BodyPart[i].image);
				}
				GUI.color = oldColor;
				
				DoPicking (rect, bodyPartToggle, count);
			}

			return bodyPartToggle;
		}

		static protected void DoPicking(Rect rect, bool[] bodyPartToggle, int count)
		{
			if (styles.PickingTexture.image)
			{
				int id = GUIUtility.GetControlID(s_Hint, FocusType.Native, rect);

				Event evt = Event.current;

				switch (evt.GetTypeForControl(id))
				{
					case EventType.MouseDown:
						{
							if (rect.Contains(evt.mousePosition))
							{								
								evt.Use();

								// Texture coordinate start at 0,0 at bottom, left
								// Screen coordinate start at 0,0 at top, left
								// So we need to convert from screen coord to texture coord
								int top = (int)evt.mousePosition.x - (int)rect.x;
								int left = styles.UnityDude.image.height - ((int)evt.mousePosition.y - (int)rect.y);

								Texture2D pickTexture = styles.PickingTexture.image as Texture2D;
								Color color = pickTexture.GetPixel(top, left);

								bool anyBodyPartPick = false;
								for (int i = 0; i < count; i++)
								{
									if (m_MaskBodyPartPicker[i] == color)
									{
										GUI.changed = true;
										bodyPartToggle[i] = !bodyPartToggle[i];
										anyBodyPartPick = true;
									}
								}

								if (!anyBodyPartPick)
								{
									bool atLeastOneSelected = false;

									for (int i = 0; i < count && !atLeastOneSelected; i++)
									{
										atLeastOneSelected = bodyPartToggle[i];
									}
  
									for (int i = 0; i < count; i++)
									{
										bodyPartToggle[i] = !atLeastOneSelected;
									}
									GUI.changed = true;
								}
							}
							break;
						}
				}
			}
		}

	}

	[CustomEditor(typeof (AvatarMask))]
	internal class AvatarMaskInspector : Editor
	{
		private class Styles
		{
			public GUIContent MaskDefinition = EditorGUIUtility.TextContent("AvatarMaskEditor.MaskDefinition");

			public GUIContent[] MaskDefinitionOpt = {
				EditorGUIUtility.TextContent("AvatarMaskEditor.CreateFromThisModel"),
				EditorGUIUtility.TextContent("AvatarMaskEditor.CopyFromOther")
			};

			public GUIContent BodyMask = EditorGUIUtility.TextContent("AvatarMaskEditor.BodyMask");
			public GUIContent TransformMask = EditorGUIUtility.TextContent("AvatarMaskEditor.TransformMask");
		}

		private static Styles styles = new Styles();

		// Body mask data
		protected bool[] m_BodyPartToggle;
		private bool m_ShowBodyMask = true;
		private bool m_BodyMaskFoldout = false;


		/// Transform mask data
		/// <summary>
		///  Accelaration scructure
		/// </summary>
		struct NodeInfo
		{
			public bool m_Expanded;
			public bool m_Show;
			public bool m_Enabled;
			public int m_ParentIndex;
			public List<int> m_ChildIndices;
			public int m_Depth;
		};


		private bool m_CanImport = true;
		public bool canImport
		{
			get { return m_CanImport; }
			set { m_CanImport = value; }
		}

		private SerializedProperty m_AnimationType = null;
		private AnimationClipInfoProperties m_ClipInfo = null;
		public AnimationClipInfoProperties clipInfo
		{
			get { return m_ClipInfo;  }
			set
			{
				m_ClipInfo = value;
				if (m_ClipInfo != null)
				{
					m_ClipInfo.MaskFromClip(target as AvatarMask);
					SerializedObject so = m_ClipInfo.maskTypeProperty.serializedObject;
					m_AnimationType = so.FindProperty("m_AnimationType");
				}
				else
				{
					m_AnimationType = null;
				}
			}
		}

		private ModelImporterAnimationType animationType
		{
			get 
			{ 
				if(m_AnimationType!=null)
					return (ModelImporterAnimationType)m_AnimationType.intValue;
				else
					return ModelImporterAnimationType.None;
			}
		}

		private NodeInfo[] m_NodeInfos;

		private Avatar m_RefAvatar;
		private ModelImporter m_RefImporter;
		private bool m_TransformMaskFoldout = false;
		private string[] m_HumanTransform = null;

		private void OnEnable()
		{
			AvatarMask bodyMask = target as AvatarMask;

			m_BodyPartToggle = new bool[bodyMask.humanoidBodyPartCount];
		}

		public bool showBody
		{
			get { return m_ShowBodyMask; }
			set { m_ShowBodyMask = value; }
		}

		public string[] humanTransforms
		{
			get
			{
				if (animationType == ModelImporterAnimationType.Human && clipInfo != null)
				{
					if (m_HumanTransform == null)
					{
						SerializedObject so = clipInfo.maskTypeProperty.serializedObject;
						ModelImporter modelImporter = so.targetObject as ModelImporter;

						m_HumanTransform = AvatarMaskUtility.GetAvatarHumanTransform(so, modelImporter.transformPaths);
					}
				}
				else
					m_HumanTransform = null;
				return m_HumanTransform;
			}
		}

		public override void OnInspectorGUI()
		{
			EditorGUI.BeginChangeCheck();

			bool showCopyFromOtherGUI = false;

			if (clipInfo != null)
			{
				EditorGUI.BeginChangeCheck();
				int maskType = (int)clipInfo.maskType;
				EditorGUI.showMixedValue = clipInfo.maskTypeProperty.hasMultipleDifferentValues;
				maskType = EditorGUILayout.Popup(styles.MaskDefinition, maskType, styles.MaskDefinitionOpt);
				EditorGUI.showMixedValue = false;
				if (EditorGUI.EndChangeCheck())
				{
					clipInfo.maskType = (ClipAnimationMaskType) maskType;
					UpdateMask(clipInfo.maskType);
				}

				showCopyFromOtherGUI = clipInfo.maskType == ClipAnimationMaskType.CopyFromOther;
			}

			if (showCopyFromOtherGUI)
				CopyFromOtherGUI();

			bool wasEnabled = GUI.enabled;
			GUI.enabled = !showCopyFromOtherGUI;
 
			OnBodyInspectorGUI();
			OnTransformInspectorGUI();

			GUI.enabled = wasEnabled;

			if (EditorGUI.EndChangeCheck() && clipInfo != null)
			{
				clipInfo.MaskToClip(target as AvatarMask);
			}
		}

		protected void CopyFromOtherGUI()
		{
			if (clipInfo == null)
				return;

			EditorGUILayout.BeginHorizontal();

			EditorGUI.BeginChangeCheck();
			EditorGUILayout.PropertyField(clipInfo.maskSourceProperty, GUIContent.Temp("Source"));
			AvatarMask maskSource = clipInfo.maskSourceProperty.objectReferenceValue as AvatarMask;
			if (EditorGUI.EndChangeCheck() && maskSource != null)
				UpdateMask(clipInfo.maskType);

			EditorGUILayout.EndHorizontal();
		}

		private void UpdateMask(ClipAnimationMaskType maskType)
		{
			if (clipInfo == null)
				return;

			if (maskType == ClipAnimationMaskType.CreateFromThisModel)
			{
				SerializedObject so = clipInfo.maskTypeProperty.serializedObject;
				ModelImporter modelImporter = so.targetObject as ModelImporter;

				AvatarMask mask = target as AvatarMask;
				AvatarMaskUtility.UpdateTransformMask(mask, modelImporter.transformPaths, humanTransforms);
				FillNodeInfos(mask);
			}
			else if (maskType == ClipAnimationMaskType.CopyFromOther)
			{
				AvatarMask maskSource = clipInfo.maskSourceProperty.objectReferenceValue as AvatarMask;
				if (maskSource != null)
				{
					AvatarMask mask = target as AvatarMask;
					mask.Copy(maskSource);
					FillNodeInfos(mask);
				}
			}
		}
		
		public void OnBodyInspectorGUI()
		{
			if (m_ShowBodyMask)
			{
				// Don't make toggling foldout cause GUI.changed to be true (shouldn't cause undoable action etc.)
				bool wasChanged = GUI.changed;
				m_BodyMaskFoldout = EditorGUILayout.Foldout(m_BodyMaskFoldout, styles.BodyMask);
				GUI.changed = wasChanged;
				if (m_BodyMaskFoldout)
				{
					AvatarMask bodyMask = target as AvatarMask;

					for (int i = 0; i < bodyMask.humanoidBodyPartCount; i++)
					{
						m_BodyPartToggle[i] = bodyMask.GetHumanoidBodyPartActive(i);
					}

					m_BodyPartToggle = BodyMaskEditor.Show(m_BodyPartToggle, bodyMask.humanoidBodyPartCount);

					bool changed = false;
					for (int i = 0; i < bodyMask.humanoidBodyPartCount; i++)
					{
						changed |= bodyMask.GetHumanoidBodyPartActive(i) != m_BodyPartToggle[i];
					}

					if (changed)
					{
						Undo.RegisterCompleteObjectUndo(bodyMask, "Body Mask Edit");

						for (int i = 0; i < bodyMask.humanoidBodyPartCount; i++)
						{
							bodyMask.SetHumanoidBodyPartActive(i, m_BodyPartToggle[i]);
						}

						EditorUtility.SetDirty(bodyMask);
					}
				}
			}
		}

		public void OnTransformInspectorGUI()
		{
			AvatarMask mask = target as AvatarMask;

			float left = 0 , top = 0 , right = 0, bottom = 0;

			// Don't make toggling foldout cause GUI.changed to be true (shouldn't cause undoable action etc.)
			bool wasChanged = GUI.changed;
			m_TransformMaskFoldout = EditorGUILayout.Foldout(m_TransformMaskFoldout, styles.TransformMask);
			GUI.changed = wasChanged;
			if (m_TransformMaskFoldout)
			{
				if (canImport)
					ImportAvatarReference();
				
				if (m_NodeInfos == null || mask.transformCount != m_NodeInfos.Length)
					FillNodeInfos(mask);

				ComputeShownElements();
				
				GUILayout.Space(1);				
				int prevIndent = EditorGUI.indentLevel;
				int size = mask.transformCount;
				for (int i = 1; i < size; i++)
				{
					if (m_NodeInfos[i].m_Show)
					{
						string transformPath = mask.GetTransformPath(i);
						string[] path = transformPath.Split('/');
						string shortName = path[path.Length - 1];

						GUILayout.BeginHorizontal();

						EditorGUI.indentLevel = m_NodeInfos[i].m_Depth + 1;

						EditorGUI.BeginChangeCheck();
						Rect toggleRect = GUILayoutUtility.GetRect(15, 15, GUILayout.ExpandWidth(false));
						GUILayoutUtility.GetRect(10, 15, GUILayout.ExpandWidth(false));
						toggleRect.x += 15;

						bool enabled = GUI.enabled;
						GUI.enabled = m_NodeInfos[i].m_Enabled;
						bool rightClick = Event.current.button == 1;
						mask.SetTransformActive(i, GUI.Toggle(toggleRect, mask.GetTransformActive(i), ""));
						GUI.enabled = enabled;
						if (EditorGUI.EndChangeCheck())
						{
							if (!rightClick)
								CheckChildren(mask, i, mask.GetTransformActive(i));
						}

						if (m_NodeInfos[i].m_ChildIndices.Count > 0)
							m_NodeInfos[i].m_Expanded = EditorGUILayout.Foldout(m_NodeInfos[i].m_Expanded, shortName);
						else
							EditorGUILayout.LabelField(shortName);


					    if (i == 1)
						{
							top =  toggleRect.yMin;							
							left = toggleRect.xMin;					        
						}

					    else if (i == size - 1)
						{
							bottom = toggleRect.yMax;
						}

						right = Mathf.Max(right, GUILayoutUtility.GetLastRect().xMax);					    					    

					    GUILayout.EndHorizontal();
					}
				}

				EditorGUI.indentLevel = prevIndent;
			}

			Rect bounds  = Rect.MinMaxRect(left,top,right,bottom);            

            if (Event.current != null && Event.current.type == EventType.MouseUp && Event.current.button == 1 && bounds.Contains(Event.current.mousePosition))
            {                
                var menu = new GenericMenu();
                menu.AddItem(new GUIContent("Select all"), false, SelectAll);
                menu.AddItem(new GUIContent("Deselect all"), false, DeselectAll);
                menu.ShowAsContext();
                Event.current.Use();
            }
		}

        private void SetAllTransformActive(bool active)
        {
            for (int i = 0; i < m_NodeInfos.Length; i++)
                if (m_NodeInfos[i].m_Enabled)
                    (target as AvatarMask).SetTransformActive(i, active); 
            
			if(clipInfo != null)
				clipInfo.MaskToClip(target as AvatarMask);
        }

        private void SelectAll()
        {
            SetAllTransformActive(true);
        }

        private void DeselectAll()
        {
            SetAllTransformActive(false);
        }

		private void ImportAvatarReference()
		{
			EditorGUI.BeginChangeCheck();
			m_RefAvatar = EditorGUILayout.ObjectField("Use skeleton from", m_RefAvatar, typeof(Avatar), true) as Avatar;
			if (EditorGUI.EndChangeCheck())
				m_RefImporter = AssetImporter.GetAtPath(AssetDatabase.GetAssetPath(m_RefAvatar)) as ModelImporter;

			if (m_RefImporter != null && GUILayout.Button("Import skeleton"))
				AvatarMaskUtility.UpdateTransformMask(target as AvatarMask, m_RefImporter.transformPaths, null);
		}

		private void FillNodeInfos(AvatarMask mask)
		{
			m_NodeInfos = new NodeInfo[mask.transformCount];

			for (int i = 1; i < m_NodeInfos.Length; i++)
			{
				string fullPath = mask.GetTransformPath(i);

				// Enable only transform that are not human, human transform in this case are handled my muscle curve and cannot be imported.	
				if (humanTransforms != null)
					m_NodeInfos[i].m_Enabled = ArrayUtility.FindIndex(humanTransforms, delegate(string s) { return fullPath == s; }) == -1;
				else
					m_NodeInfos[i].m_Enabled = true;
				
				m_NodeInfos[i].m_Expanded = true;
				m_NodeInfos[i].m_ParentIndex = -1;
				m_NodeInfos[i].m_ChildIndices = new List<int>();

				m_NodeInfos[i].m_Depth = i == 0 ? 0 : fullPath.Count(f => f == '/');

				string parentPath = "";
				int lastIndex = fullPath.LastIndexOf('/');
				if (lastIndex > 0)
					parentPath = fullPath.Substring(0, lastIndex);

				int size = mask.transformCount;
				for (int j = 0; j < size; j++)
				{
					string otherPath = mask.GetTransformPath(j);
					if (parentPath != "" && otherPath == parentPath)
						m_NodeInfos[i].m_ParentIndex = j;

					if (otherPath.StartsWith(fullPath) && otherPath.Count(f => f == '/') == m_NodeInfos[i].m_Depth + 1)
						m_NodeInfos[i].m_ChildIndices.Add(j);
				}
			}

		}

		private void ComputeShownElements()
		{
			for (int i = 0; i < m_NodeInfos.Length; i++)
			{
				if (m_NodeInfos[i].m_ParentIndex == -1)
					ComputeShownElements(i, true);
			}
		}

		private void ComputeShownElements(int currentIndex, bool show)
		{
			m_NodeInfos[currentIndex].m_Show = show;
			bool showChilds = show && m_NodeInfos[currentIndex].m_Expanded;
			foreach (int index in m_NodeInfos[currentIndex].m_ChildIndices)
			{
				ComputeShownElements(index, showChilds);
			}
		}

		private void CheckChildren(AvatarMask mask, int index, bool value)
		{
			foreach (int childIndex in m_NodeInfos[index].m_ChildIndices)
			{
				if (m_NodeInfos[childIndex].m_Enabled)
					mask.SetTransformActive(childIndex, value);
				CheckChildren(mask, childIndex, value);
			}
		}	
	}
}
