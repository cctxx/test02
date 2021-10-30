using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class InputNodes
	{
		[Logic]
		[Title("Input/On Mouse Button")]
		public sealed class OnMouseButton : OnStateInputNode
		{
			private int m_MouseButton;
			public int mouseButton { set { m_MouseButton = value; } }

			public OnMouseButton (GraphBehaviour graphBehaviour) : base (graphBehaviour) { }
			public OnMouseButton (IMonoBehaviourEventCaller graphBehaviour, int mouseButton) : base (graphBehaviour)
			{
				m_MouseButton = mouseButton;
			}

			protected override void OnUpdate ()
			{
				if (onDown != null && Input.GetMouseButtonDown (m_MouseButton))
					onDown ();

				if (onUp != null && Input.GetMouseButtonUp (m_MouseButton))
					onUp ();

				if (down != null || up != null)
				{
					var stateDelegate = Input.GetMouseButton (m_MouseButton) ? down : up;
					if (stateDelegate != null)
						stateDelegate ();
				}
			}
		}
	}
}

