using UnityEngine;
using System;
using System.Collections.Generic;
using System.Linq;

#if ENABLE_2D_PHYSICS

namespace UnityEditor
{
	[CustomEditor(typeof(PolygonCollider2D))]
	[CanEditMultipleObjects]
	internal class PolygonColliderEditor : Collider2DEditorBase
	{
		private static bool m_Editing;

		private PolygonEditorUtility m_PolyUtility = new PolygonEditorUtility();
		private bool m_ShowColliderInfo;

		bool editModeActive {
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
			HandleDragAndDrop ();
			BeginColliderInspector ();
			ColliderInfoGUI();
			EndColliderInspector();
		}

		// Copy collider from sprite if it is drag&dropped to the inspector
		private void HandleDragAndDrop ()
		{
			if (Event.current.type != EventType.DragPerform && Event.current.type != EventType.DragUpdated)
				return;

			foreach (var obj in DragAndDrop.objectReferences.Where (obj => obj is Sprite || obj is Texture2D))
			{
				DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
				
				if (Event.current.type == EventType.DragPerform)
				{
					var sprite = obj is Sprite ? obj as Sprite : SpriteUtility.TextureToSprite (obj as Texture2D);

					// Copy collider to all selected components
					foreach (var collider in targets.Select (target => target as PolygonCollider2D))
					{
#if ENABLE_SPRITECOLLIDER
						collider.pathCount = sprite.colliderPathCount;
						for (int i = 0; i < sprite.colliderPathCount; ++i)
							collider.SetPath (i, sprite.GetColliderPath (i));
#else
						Vector2[][] paths;
						UnityEditor.Sprites.DataUtility.GenerateOutlineFromSprite(sprite, 0.25f, 200, true, out paths);
						collider.pathCount = paths.Length;
						for (int i = 0; i < paths.Length; ++i)
							collider.SetPath (i, paths[i]);
#endif

						// Stop editing
						m_Editing = false;
						m_PolyUtility.StopEditing ();

						DragAndDrop.AcceptDrag ();
					}
				}

				return;
			}

			DragAndDrop.visualMode = DragAndDropVisualMode.Rejected;
		}

		private void ColliderInfoGUI ()
		{
			// Hide values on multiselect
			EditorGUI.BeginDisabledGroup(targets.Length != 1);

			m_ShowColliderInfo = EditorGUILayout.Foldout (m_ShowColliderInfo, "Collider Info");

			if (m_ShowColliderInfo)
			{
				var collider = targets[0] as PolygonCollider2D;
				if (collider)
				{
					int pointCount = collider.GetTotalPointCount ();

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
				Rect mouseScreenRect = new Rect (mousePos.x - 16, mousePos.y - 16, 32, 32);
				EditorGUIUtility.AddCursorRect (mouseScreenRect, MouseCursor.Arrow);
			}
		}
		
#endregion
	}
}

#endif // #if ENABLE_2D_PHYSICS
