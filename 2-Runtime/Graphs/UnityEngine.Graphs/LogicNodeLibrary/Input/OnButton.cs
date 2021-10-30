using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class InputNodes
	{
		[Logic]
		[Title("Input/On Button")]
		public sealed class OnButton : OnStateInputNode
		{
			private string m_ButtonName;
			public string buttonName { set { m_ButtonName = value; } }

			public OnButton (GraphBehaviour graphBehaviour) : base (graphBehaviour) { }
			public OnButton (IMonoBehaviourEventCaller graphBehaviour, string buttonName) : base (graphBehaviour)
			{
				m_ButtonName = buttonName;
			}

			protected override void OnUpdate ()
			{
				if (onDown != null && Input.GetButtonDown (m_ButtonName))
					onDown ();

				if (onUp != null && Input.GetButtonUp (m_ButtonName))
					onUp ();

				if (down != null || up != null)
				{
					var stateDelegate = Input.GetButton (m_ButtonName) ? down : up;
					if (stateDelegate != null)
						stateDelegate ();
				}
			}
		}
	}
}

