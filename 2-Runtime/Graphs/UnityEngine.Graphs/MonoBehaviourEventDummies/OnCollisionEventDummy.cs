using System;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	// For now we do triggers by attaching this MonoBehaviour to needed gameobjects. Class then sends events to logic graph nodes.
	public class OnCollisionEventDummy : ColliderDummyBase
	{
		public delegate void CollisionOutDelegate(Collision other);

		private CollisionOutDelegate m_OnEnter;
		private CollisionOutDelegate m_OnExit;
		private CollisionOutDelegate m_OnStay;

		public static void AttachToCollider(ColliderNodes.OnCollisionEvent node)
		{
			var attached = AttachToCollider(node.self, typeof(OnCollisionEventDummy)) as OnCollisionEventDummy;
			attached.m_OnEnter += node.EnterDummy;
			attached.m_OnExit += node.ExitDummy;
			attached.m_OnStay += node.StayDummy;
		}

		public void OnCollisionEnter(Collision collision)
		{
			if (m_OnEnter == null)
				return;
			m_OnEnter (collision);
		}
		public void OnCollisionExit(Collision collision)
		{
			if (m_OnExit == null)
				return;
			m_OnExit(collision);
		}
		public void OnCollisionStay(Collision collision)
		{
			if (m_OnStay == null)
				return;
			m_OnStay(collision);
		}
	}
}

