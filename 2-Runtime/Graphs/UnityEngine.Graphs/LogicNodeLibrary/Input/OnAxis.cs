using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class InputNodes
	{
		[Logic]
		[Title("Input/On Axis")]
		public sealed class OnAxis : OnInputNode
		{
			private float m_Value;
			public float value { get { return m_Value; } }

			private string m_AxisName;
			public string axisName { set { m_AxisName = value; } }

			public OnAxis (GraphBehaviour graphBehaviour) : base (graphBehaviour) { }
			public OnAxis (IMonoBehaviourEventCaller graphBehaviour, string axisName) : base (graphBehaviour)
			{
				m_AxisName = axisName;
			}

			protected override void OnUpdate ()
			{
				if (down == null && up == null)
					return;

				m_Value = Input.GetAxis (m_AxisName);

				var stateDelegate = Input.GetButton (m_AxisName) ? down : up;
				if (stateDelegate != null)
					stateDelegate ();
			}
		}
	}
}

