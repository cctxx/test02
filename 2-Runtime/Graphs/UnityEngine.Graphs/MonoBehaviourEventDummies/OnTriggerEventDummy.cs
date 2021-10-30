using System;
using UnityEngine;
using System.Collections.Generic;

namespace UnityEngine.Graphs.LogicGraph
{
	// For now we do triggers by attaching this MonoBehaviour to needed gameobjects. Class then sends events to logic graph nodes.
	public class OnTriggerEventDummy : ColliderDummyBase
	{
		public delegate void TriggerOutDelegate (Collider other);
		private TriggerOutDelegate m_OnEnter;
		private TriggerOutDelegate m_OnExit;
		private TriggerOutDelegate m_OnStay;

		public static void AttachToCollider(ColliderNodes.OnTriggerEvent node)
		{
			var attached = AttachToCollider(node.self, typeof(OnTriggerEventDummy)) as OnTriggerEventDummy;
			attached.m_OnEnter += node.EnterDummy;
			attached.m_OnExit += node.ExitDummy;
			attached.m_OnStay += node.StayDummy;
		}

		public void OnTriggerEnter(Collider other)
		{
			if (m_OnEnter != null)
				m_OnEnter (other);
		}
		public void OnTriggerExit(Collider other)
		{
			if (m_OnExit != null)
				m_OnExit(other);
		}
		public void OnTriggerStay(Collider other)
		{
			if (m_OnStay != null)
				m_OnStay (other);
		}
	}
}

