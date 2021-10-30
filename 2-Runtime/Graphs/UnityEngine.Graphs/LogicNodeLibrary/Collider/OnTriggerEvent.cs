using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class ColliderNodes
	{
		// This is only used for node declaration. Implementation is in the OnCollisionEventDummy monobehaviour.
		[Logic(typeof(Collider))]
		public class OnTriggerEvent
		{
			[LogicTarget]
			public Collider self;

			public Action enter;
			public Action exit;
			public Action stay;

			private Collider m_Other;

			public Collider other
			{
				get { return m_Other; }
			}

			internal void EnterDummy(Collider other)
			{
				if (enter == null)
					return;
				m_Other = other;
				enter();
			}
			internal void ExitDummy(Collider other)
			{
				if (exit == null)
					return;
				m_Other = other;
				exit();
			}
			internal void StayDummy(Collider other)
			{
				if (stay == null)
					return;
				m_Other = other;
				stay();
			}
		}
	}
}
