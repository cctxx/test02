using System;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	// For now we do triggers by attaching this MonoBehaviour to needed gameobjects. Class then sends events to logic graph nodes.
	public class OnMouseEventDummy : ColliderDummyBase
	{
		private Action m_OnEnter;
		private Action m_OnOver;
		private Action m_OnExit;
		private Action m_OnDown;
		private Action m_OnUp;
		private Action m_OnDrag;

		public static void AttachToCollider (ColliderNodes.OnMouseEvent node)
		{
			var attached = AttachToCollider(node.self, typeof(OnMouseEventDummy)) as OnMouseEventDummy;
			attached.m_OnEnter += node.enter;
			attached.m_OnOver += node.over;
			attached.m_OnExit += node.exit;
			attached.m_OnDown += node.down;
			attached.m_OnUp += node.up;
			attached.m_OnDrag += node.drag;
		}

		public void OnMouseEnter () { CallEventDelegate (m_OnEnter); }
		public void OnMouseOver () { CallEventDelegate (m_OnOver); }
		public void OnMouseExit () { CallEventDelegate (m_OnExit); }
		public void OnMouseDown () { CallEventDelegate (m_OnDown); }
		public void OnMouseUp () { CallEventDelegate (m_OnUp); }
		public void OnMouseDrag () { CallEventDelegate (m_OnDrag); }

		protected static void CallEventDelegate(Action eventDelegate)
		{
			if (eventDelegate != null) 
				eventDelegate();
		}
	}
}

