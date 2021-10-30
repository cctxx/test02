using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class ColliderNodes
	{
		// This is only used for node declaration. Implementation is in the OnMouseEventDummy monobehaviour.
		[Logic(typeof(Collider))]
		public class OnMouseEvent
		{
			[LogicTarget]
			public Collider self;

			public Action enter;
			public Action over;
			public Action exit;
			public Action down;
			public Action up;
			public Action drag;
		}
	}
}
