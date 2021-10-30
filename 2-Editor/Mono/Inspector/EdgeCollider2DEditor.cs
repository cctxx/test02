using UnityEngine;
using System;
using System.Collections.Generic;
using System.Linq;

#if ENABLE_2D_PHYSICS

namespace UnityEditor
{
	[CustomEditor(typeof(EdgeCollider2D))]
	[CanEditMultipleObjects]
	internal class EdgeCollider2DEditor : Collider2DEditorBase
	{
		private static bool m_Editing;

		private PolygonEditorUtility m_PolyUtility = new PolygonEditorUtility();
		private bool m_ShowColliderInfo;

		bool editModeActive
		{
			get { return Event.current.shift || Event.current.control || Event.current.command; }
		}

		#region Enable / Disable

		public override void OnEnable ()
		{
			base.OnEnable();
		}

		public void OnDisable()
		{
			StopEditing();
		}

		#endregion

		#region OnInspectorGUI

		public override void OnInspectorGUI ()
		{
			BeginColliderInspector();
			ColliderInfoGUI();
			EndColliderInspector();
		}

		private void ColliderInfoGUI ()
		{
			// Hide values on multiselect
			EditorGUI.BeginDisabledGroup(targets.Length != 1);

			m_ShowColliderInfo = EditorGUILayout.Foldout (m_ShowColliderInfo, "Collider Info");

			if (m_ShowColliderInfo)
			{
				var collider = targets[0] as EdgeCollider2D;
				if (collider)
				{
					int pointCount = collider.pointCount;
					string pointValueLabel = GUI.enabled ? "" + pointCount : "---";

					EditorGUI.indentLevel++;
					EditorGUILayout.LabelField ("Vertices", pointValueLabel);
					EditorGUI.indentLevel--;
				}
			}

			EditorGUI.EndDisabledGroup();
		}

		#endregion

#region OnSceneGUI

		private void StartEditing()
		{
			// We don't support multiselection editing
			if (target == null)
				return;

			m_PolyUtility.StartEditing(target as Collider2D);
			m_Editing = true;
		}

		private void StopEditing()
		{
			m_PolyUtility.StopEditing();
			m_Editing = false;
		}

		public void OnSceneGUI()
		{
			if (editModeActive)
				if (!m_Editing)
					StartEditing();

			if (m_Editing && !editModeActive)
				StopEditing();

			if (m_Editing)
				m_PolyUtility.OnSceneGUI();

			if (m_Editing)
			{
				// Force arrow cursor on
				Vector3 mousePos = Event.current.mousePosition;
				Rect mouseScreenRect = new Rect(mousePos.x - 16, mousePos.y - 16, 32, 32);
				EditorGUIUtility.AddCursorRect(mouseScreenRect, MouseCursor.Arrow);
			}
		}

		#endregion
	}
}

#endif // #if ENABLE_2D_PHYSICS
