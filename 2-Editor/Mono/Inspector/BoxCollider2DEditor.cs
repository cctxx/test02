using System;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor (typeof (BoxCollider2D))]
	[CanEditMultipleObjects]
	internal class BoxCollider2DEditor : Collider2DEditorBase
	{
		private static readonly int s_BoxHash = "BoxCollider2DEditor".GetHashCode ();
		private readonly BoxEditor m_BoxEditor = new BoxEditor (true, s_BoxHash, true);

		public override void OnEnable ()
		{
			base.OnEnable ();
			m_BoxEditor.OnEnable ();
		}

		public void OnDisable ()
		{
			m_BoxEditor.OnDisable ();
		}
		
		public void OnSceneGUI ()
		{
			BoxCollider2D boxCollider = (BoxCollider2D) target;

			Vector3 center = boxCollider.center;
			Vector3 size = boxCollider.size;

			if (m_BoxEditor.OnSceneGUI (boxCollider.transform, Handles.s_ColliderHandleColor, ref center, ref size))
			{
				Undo.RecordObject (boxCollider, "Modify collider");
				boxCollider.center = center;
				boxCollider.size = size;
			}
		}
	}
}
