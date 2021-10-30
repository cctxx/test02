using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class ColliderNodes
	{
		// This is only used for node declaration. Implementation is in the OnCollisionEventDummy monobehaviour.
		[Logic(typeof(Collider))]
		public class OnCollisionEvent
		{
			[LogicTarget]
			public Collider self;

			public Action enter;
			public Action exit;
			public Action stay;

			private Vector3 m_RelativeVelocity;
			public Vector3 relativeVelocity
			{
				get { return m_RelativeVelocity; }
			}

			private Collider m_Other;
			public Collider other
			{
				get { return m_Other; }
			}

			//TODO: would be nice to have, but no nodes in graphs yet know about how to work with arrays
			//private ContactPoint[] m_Contacts;
			//public ContactPoint[] contacts
			//{
			//	get { return m_Contacts; }
			//}

			internal void EnterDummy(Collision collision)
			{
				if (enter == null)
					return;

				m_RelativeVelocity = collision.relativeVelocity;
				m_Other = collision.collider;
				//m_Contacts = collision.contacts;

				enter ();
			}

			internal void ExitDummy(Collision collision)
			{
				if (exit == null)
					return;

				m_RelativeVelocity = collision.relativeVelocity;
				m_Other = collision.collider;
				//m_Contacts = collision.contacts;

				exit();
			}

			internal void StayDummy(Collision collision)
			{
				if (stay == null)
					return;

				m_RelativeVelocity = collision.relativeVelocity;
				m_Other = collision.collider;
				//m_Contacts = collision.contacts;

				stay();
			}
		}
	}
}
