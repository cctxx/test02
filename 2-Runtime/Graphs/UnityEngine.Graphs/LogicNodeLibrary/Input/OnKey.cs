using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class InputNodes
	{
		[Logic]
		[Title("Input/On Key")]
		public sealed class OnKey : OnStateInputNode
		{
			private KeyCode m_Key;
			public KeyCode key { set { m_Key = value; } }
			
			public OnKey (GraphBehaviour graphBehaviour) : base (graphBehaviour) { }
			public OnKey (IMonoBehaviourEventCaller graphBehaviour, KeyCode key) : base (graphBehaviour)
			{
				m_Key = key;
			}

			protected override void OnUpdate ()
			{
				if (onDown != null && Input.GetKeyDown (m_Key))
					onDown ();

				if (onUp != null && Input.GetKeyUp (m_Key))
					onUp ();

				if (down != null || up != null)
				{
					var stateDelegate = Input.GetKey (m_Key) ? down : up;
					if (stateDelegate != null)
						stateDelegate ();
				}
			}
		}
	}
}

