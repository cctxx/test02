using System.Collections.Generic;
using System.IO;
using System.Xml.Serialization;
using UnityEditorInternal;
using UnityEngine;
using System.Linq;
using Object = UnityEngine.Object;

namespace UnityEditor
{
	internal class LightProbeGroupEditor : IEditablePoint
	{
		private const int kLightProbeCoefficientCount = 27;
		private const float kDuplicateEpsilonSq = 0.1f;
		private bool m_Editing;

		private List<Vector3> m_SourcePositions;
		private List<int> m_Selection = new List<int> ();

		private readonly LightProbeGroupSelection m_SerializedSelectedProbes;

		private readonly LightProbeGroup m_Group;
		private bool m_ShouldRecalculateTetrahedra;

		public LightProbeGroupEditor (LightProbeGroup group)
		{
			m_Group = group;
			MarkTetrahedraDirty ();
			m_SerializedSelectedProbes = ScriptableObject.CreateInstance<LightProbeGroupSelection> ();
			m_SerializedSelectedProbes.hideFlags = HideFlags.HideAndDontSave;
		}

		public void SetEditing (bool editing)
		{
			m_Editing = editing;
		}

		public void AddProbe(Vector3 position)
		{
			Undo.RegisterCompleteObjectUndo(new Object[] { m_Group, m_SerializedSelectedProbes }, "Add Probe");
			m_SourcePositions.Add(position);
			SelectProbe (m_SourcePositions.Count - 1);

			MarkTetrahedraDirty ();
		}

		private void SelectProbe (int i)
		{
			if (!m_Selection.Contains (i))
				m_Selection.Add(i);
		}

		public void SelectAllProbes()
		{
			DeselectProbes();
			for (var i = 0; i < m_SourcePositions.Count; i++)
				SelectProbe(i);
		}

		public void DeselectProbes()
		{
			m_Selection.Clear();
		}

		private IEnumerable<Vector3> SelectedProbePositions()
		{
			return m_Selection.Select(t => m_SourcePositions[t]).ToList();
		}

		public void DuplicateSelectedProbes()
		{
			var selectionCount = m_Selection.Count;
			if(selectionCount == 0) return;

			Undo.RegisterCompleteObjectUndo(new Object[] { m_Group, m_SerializedSelectedProbes }, "Duplicate Probes");

			foreach (var position in SelectedProbePositions () )
			{
				m_SourcePositions.Add (position);
			}

			MarkTetrahedraDirty ();
		}

		private void CopySelectedProbes()
		{
			//Convert probes to world position for serialization
			var localPositions = SelectedProbePositions ();

			var serializer = new XmlSerializer(typeof(Vector3[]));
			var writer = new StringWriter();

			serializer.Serialize(writer, localPositions.Select (pos => m_Group.transform.TransformPoint (pos)).ToArray ());
			writer.Close ();
			GUIUtility.systemCopyBuffer = writer.ToString ();
		}

		private static bool CanPasteProbes()
		{
			try
			{
				var deserializer = new XmlSerializer (typeof (Vector3[]));
				var reader = new StringReader (GUIUtility.systemCopyBuffer);
				deserializer.Deserialize (reader);
				reader.Close ();
				return true;
			}
			catch
			{
				return false;
			}
		}

		private bool PasteProbes()
		{
			//If we can't paste / paste buffer is bad do nothing
			try
			{
				var deserializer = new XmlSerializer(typeof(Vector3[]));
				var reader = new StringReader(GUIUtility.systemCopyBuffer);
				var pastedProbes = (Vector3[])deserializer.Deserialize(reader);
				reader.Close();

				if(pastedProbes.Length == 0) return false;

				Undo.RegisterCompleteObjectUndo(new Object[] { m_Group, m_SerializedSelectedProbes }, "Paste Probes");

				var oldLength = m_SourcePositions.Count;

				//Need to convert into local space...
				foreach (var position in pastedProbes)
				{
					m_SourcePositions.Add (m_Group.transform.InverseTransformPoint (position));
				}

				//Change selection to be the newly pasted probes
				DeselectProbes ();
				for (int i = oldLength; i < oldLength+pastedProbes.Length; i++)
				{
					SelectProbe(i);
				}
				MarkTetrahedraDirty ();

				return true;
			}
			catch
			{
				return false;
			}
		}

		public void RemoveSelectedProbes()
		{
			int selectionCount = m_Selection.Count;
			if (selectionCount == 0)
				return;
			
			Undo.RegisterCompleteObjectUndo(new Object[] { m_Group, m_SerializedSelectedProbes }, "Delete Probes");

			var reverseSortedIndicies = m_Selection.OrderByDescending(x => x);
			foreach (var index in reverseSortedIndicies)
			{
				m_SourcePositions.RemoveAt(index);
			}
			DeselectProbes ();
			MarkTetrahedraDirty ();
		}
		
		public void PullProbePositions()
		{
			//m_SourcePositions = new List<Vector3> ();
			m_SourcePositions = new List<Vector3>(m_Group.probePositions);
			m_Selection = new List<int> (m_SerializedSelectedProbes.m_Selection);
		}

		public void PushProbePositions()
		{
			var positionsChanged = false;

			//Check array sizes first
			if (m_Group.probePositions.Length != m_SourcePositions.Count
				|| m_SerializedSelectedProbes.m_Selection.Count != m_Selection.Count)
				positionsChanged = true;

			//Then check positions
			if (!positionsChanged)
			{
				if (m_Group.probePositions.Where ((t, i) => t != m_SourcePositions[i]).Any ())
					positionsChanged = true;

				for ( var i = 0; i < m_SerializedSelectedProbes.m_Selection.Count; i++)
				{
					if (m_SerializedSelectedProbes.m_Selection[i] != m_Selection[i] )
						positionsChanged = true;
				}
			}

			if (positionsChanged)
			{
				m_Group.probePositions = m_SourcePositions.ToArray();
				m_SerializedSelectedProbes.m_Selection = m_Selection;
				LightmappingWindow.ProbePositionsChanged();
			}
		}

		//private List<Line> m_TetrahedraLines = new List<Line>();
		private void DrawTetrahedra ()
		{
			if (Event.current.type != EventType.repaint)
				return;

			if (SceneView.lastActiveSceneView)
			{
				LightmapVisualization.DrawTetrahedra (	m_ShouldRecalculateTetrahedra,
														SceneView.lastActiveSceneView.camera.transform.position);
				m_ShouldRecalculateTetrahedra = false;
			}
		}

		public static void TetrahedralizeSceneProbes(out Vector3[] positions, out int[] indices)
		{
			var probeGroups = Object.FindObjectsOfType(typeof(LightProbeGroup)) as LightProbeGroup[];

			if (probeGroups == null)
			{
				positions = new Vector3[0];
				indices = new int[0];
				return;
			}
			var probePositions = new List<Vector3>();

			foreach (var group in probeGroups)
			{
				var localPositions = group.probePositions;
				foreach (var position in localPositions)
				{
					var wPosition = group.transform.TransformPoint(position);
					probePositions.Add(wPosition);
				}
			}

			if (probePositions.Count == 0)
			{
				positions = new Vector3[0];
				indices = new int[0];
				return;
			}

			Lightmapping.Tetrahedralize(probePositions.ToArray(), out indices, out positions);
		}

		private Vector3 m_LastPosition = Vector3.zero;
		private Quaternion m_LastRotation = Quaternion.identity;
		private Vector3 m_LastScale = Vector3.one;
		public bool OnSceneGUI(Transform transform)
		{
			if (Event.current.type == EventType.layout)
			{
				//If the group has moved / scaled since last frame need to retetra);)
				if ( m_LastPosition != m_Group.transform.position
				    || m_LastRotation != m_Group.transform.rotation
				    || m_LastScale != m_Group.transform.localScale)
				{
					MarkTetrahedraDirty ();
				}

				m_LastPosition = m_Group.transform.position;
				m_LastRotation = m_Group.transform.rotation;
				m_LastScale = m_Group.transform.localScale;
			}

			//See if we should enter edit mode!
			bool firstSelect = false;
			if (Event.current.type == EventType.MouseDown && Event.current.button == 0)
			{
				//We have no probes selected and have clicked the mouse... Did we click a probe
				if (SelectedCount == 0)
				{
					var selected = PointEditor.FindNearest (Event.current.mousePosition, transform, this);
					var clickedProbe = selected != -1;
					
					if (clickedProbe && !m_Editing)
					{
						m_Editing = true;
						firstSelect = true;
					}
				}
			}
			
			//Need to cache this as select points will use it!
			var mouseUpEvent = Event.current.type == EventType.MouseUp;
			
			if (m_Editing)
			{
				if (PointEditor.SelectPoints(this, transform, ref m_Selection, firstSelect))
				{
					Undo.RegisterCompleteObjectUndo(new Object[] { m_Group, m_SerializedSelectedProbes }, "Select Probes");
				}
			}
			
			if (m_Editing && mouseUpEvent && SelectedCount == 0)
			{
				m_Editing = false;
				MarkTetrahedraDirty ();
			}

			//Special handling for paste (want to be able to paste when not in edit mode!)
			
			if ( (Event.current.type == EventType.ValidateCommand || Event.current.type == EventType.ExecuteCommand)
			    && Event.current.commandName == "Paste" )
			{
				if (Event.current.type == EventType.ValidateCommand)
				{
					if (CanPasteProbes ())
						Event.current.Use();
				}
				if (Event.current.type == EventType.ExecuteCommand)
				{
					if (PasteProbes())
					{
						Event.current.Use ();
						m_Editing = true;
					}
				}
			}

			DrawTetrahedra();
			PointEditor.Draw (this, transform, m_Selection, true);

			if (!m_Editing)
				return m_Editing;
			
			//Handle other events!
			if (Event.current.type == EventType.ValidateCommand 
				|| Event.current.type == EventType.ExecuteCommand)
			{
				bool execute = Event.current.type == EventType.ExecuteCommand;
				switch (Event.current.commandName)
				{
					case "SoftDelete":
					case "Delete":
						if (execute) RemoveSelectedProbes ();
						Event.current.Use ();
						break;
					case "Duplicate":
						if (execute) DuplicateSelectedProbes ();
						Event.current.Use ();
						break;
					case "SelectAll":
						if (execute)
							SelectAllProbes ();
						Event.current.Use ();
						break;
					case "Cut":
						if (execute)
						{
							CopySelectedProbes ();
							RemoveSelectedProbes ();
						}
						Event.current.Use();
						break;
					case "Copy":
						if (execute) CopySelectedProbes();
						Event.current.Use();
						break;
					
					default:
						break; 
				}
			}

			if (m_Editing && PointEditor.MovePoints(this, transform, m_Selection))
			{
				Undo.RegisterCompleteObjectUndo(new Object[] { m_Group, m_SerializedSelectedProbes }, "Move Probes");
				if (LightmapVisualization.dynamicUpdateLightProbes)
					MarkTetrahedraDirty ();
			}

			if (m_Editing && mouseUpEvent && !LightmapVisualization.dynamicUpdateLightProbes)
			{
				MarkTetrahedraDirty ();
			}

			return m_Editing;
		}

		public void MarkTetrahedraDirty ()
		{
			m_ShouldRecalculateTetrahedra = true;
		}

		public Bounds selectedProbeBounds
		{
			get
			{
				if (m_Selection.Count == 0)
					return new Bounds();

				if (m_Selection.Count == 1)
					return new Bounds(GetWorldPosition(m_Selection[0]), new Vector3(1f, 1f, 1f));

				var min = new Vector3(float.MaxValue, float.MaxValue, float.MaxValue);
				var max = new Vector3(float.MinValue, float.MinValue, float.MinValue);

				foreach (var index in m_Selection)
				{
					var position = GetWorldPosition (index);

					if (position.x < min.x) min.x = position.x;
					if (position.y < min.y) min.y = position.y;
					if (position.z < min.z) min.z = position.z;

					if (position.x > max.x) max.x = position.x;
					if (position.y > max.y) max.y = position.y;
					if (position.z > max.z) max.z = position.z;
				}
				var bounds = new Bounds();
				bounds.SetMinMax (min, max);
				return bounds;
			}
		}
		
		/// Get the world-space position of a specific point
		public Vector3 GetPosition(int idx)
		{
			return m_SourcePositions[idx];
		}

		public Vector3 GetWorldPosition(int idx)
		{
			return m_Group.transform.TransformPoint (m_SourcePositions[idx]);
		}

		public void SetPosition(int idx, Vector3 position)
		{
			if (m_SourcePositions[idx] == position)
				return;

			m_SourcePositions[idx] = position;
		}

		private static readonly Color kCloudColor = new Color(200f/255f, 200f / 255f, 20f / 255f, 0.85f);
		private static readonly Color kSelectedCloudColor = new Color (.3f, .6f, 1, 1);

		public Color GetDefaultColor()
		{
			return kCloudColor;
		}

		public Color GetSelectedColor()
		{
			return kSelectedCloudColor;
		}

		public float GetPointScale ()
		{
			return 10.0f * AnnotationUtility.iconSize;
		}

		public Vector3[] GetSelectedPositions ()
		{
			var result = new Vector3[SelectedCount];
			for (int i = 0; i < SelectedCount; i++)
			{
				result[i] = m_SourcePositions[m_Selection[i]];
			}
			return result;
		}

		public IEnumerable<Vector3> GetPositions()
		{
			return m_SourcePositions;
		}

		public Vector3[] GetUnselectedPositions()
		{
			return m_SourcePositions.Where((t, i) => !m_Selection.Contains(i)).ToArray();
		}

		/// How many points are there in the array.
		public int Count { get { return m_SourcePositions.Count; } }


		/// How many points are selected in the array.
		public int SelectedCount { get { return m_Selection.Count; } }
	}

	[CustomEditor(typeof(LightProbeGroup))]
	class LightProbeGroupInspector : Editor
	{
		private LightProbeGroupEditor m_Editor;

		public void OnEnable () {
			m_Editor = new LightProbeGroupEditor (target as LightProbeGroup);
			m_Editor.PullProbePositions();
			m_Editor.DeselectProbes ();
			m_Editor.PushProbePositions ();
			SceneView.onSceneGUIDelegate += OnSceneGUIDelegate;
			Undo.undoRedoPerformed += UndoRedoPerformed;
		}

		private void StartEditProbes()
		{
			if (m_EditingProbes)
				return;
			
			m_EditingProbes = true;
			m_Editor.SetEditing (true);
			Tools.s_Hidden = true;
			SceneView.RepaintAll();
		}

		private void EndEditProbes()
		{
			if (!m_EditingProbes)
				return;

			m_Editor.DeselectProbes ();

			m_EditingProbes = false;
			Tools.s_Hidden = false;
		}

		public void OnDisable()
		{
			EndEditProbes();
			Undo.undoRedoPerformed -= UndoRedoPerformed;
			SceneView.onSceneGUIDelegate -= OnSceneGUIDelegate;
			if (target != null)
				m_Editor.PushProbePositions ();
		}

		private void UndoRedoPerformed ()
		{
			m_Editor.MarkTetrahedraDirty ();
		}

		private bool m_EditingProbes;
		private bool m_ShouldFocus;
		public override void OnInspectorGUI()
		{
            bool hasPro = Application.HasAdvancedLicense();

			EditorGUI.BeginDisabledGroup (!hasPro);
			m_Editor.PullProbePositions ();
			
			GUILayout.BeginHorizontal ();
			GUILayout.BeginVertical ();
			if (GUILayout.Button ("Add Probe"))
			{
				var position = Vector3.zero;
				if (SceneView.lastActiveSceneView)
				{
					position = SceneView.lastActiveSceneView.pivot;
					var probeGroup = target as LightProbeGroup;
					if (probeGroup) position = probeGroup.transform.InverseTransformPoint(position);
				}
				StartEditProbes();
				m_Editor.DeselectProbes ();
				m_Editor.AddProbe(position);
				//m_ShouldFocus = true;
			}
			
			if (GUILayout.Button("Delete Selected"))
			{
				StartEditProbes();
				m_Editor.RemoveSelectedProbes();
			}
			GUILayout.EndVertical ();

			GUILayout.BeginVertical ();
			if (GUILayout.Button("Select All"))
			{
				StartEditProbes();
				m_Editor.SelectAllProbes ();
			}

			if (GUILayout.Button("Duplicate Selected"))
			{
				StartEditProbes();
				m_Editor.DuplicateSelectedProbes ();
			}
			GUILayout.EndVertical ();
			GUILayout.EndHorizontal();
			
			m_Editor.PushProbePositions ();
			EditorGUI.EndDisabledGroup ();

			if (!hasPro)
			{
				GUIContent c = EditorGUIUtility.TextContent("LightProbeGroup.ProOnly");
				EditorGUILayout.HelpBox(c.text, MessageType.Warning, true);
			}
		}

		private void InternalOnSceneView ()
		{
			if (SceneView.lastActiveSceneView != null)
			{
				if (m_ShouldFocus)
				{
					m_ShouldFocus = false;
					SceneView.lastActiveSceneView.FrameSelected ();
				}
			}

			m_Editor.PullProbePositions ();
			var lpg = target as LightProbeGroup;
			if (lpg != null)
			{
				if (m_Editor.OnSceneGUI (lpg.transform))
					StartEditProbes ();
				else
					EndEditProbes ();
			}
			m_Editor.PushProbePositions ();
		}

		public void OnSceneGUI ()
		{
            if (!Application.HasAdvancedLicense())
                return;
            
            if (Event.current.type != EventType.Repaint)
				InternalOnSceneView ();
		}

		public void OnSceneGUIDelegate (SceneView sceneView)
		{
			if (Event.current.type == EventType.Repaint)
				InternalOnSceneView ();
		}
		
		public bool HasFrameBounds ()
		{
			return m_Editor.SelectedCount > 0;
		}
		
		public Bounds OnGetFrameBounds()
		{
			return m_Editor.selectedProbeBounds;
		}
	}
}
