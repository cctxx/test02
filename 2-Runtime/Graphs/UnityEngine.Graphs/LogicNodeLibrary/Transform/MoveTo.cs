using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class TransformNodes
	{
		[Logic (typeof (Transform))]
		public sealed class MoveTo : YieldedTransformNodeBase
		{
			private readonly IMoveToPositionCalculator m_PositionCalculator;
			private Vector3 m_InitialPosition;
			private Vector3 m_TargetRelativePosition;

			public override Transform target { set { m_Target = value; } }
			public Vector3 targetOffset { set { m_TargetRelativePosition = value; } }

			public MoveTo()
			{
				m_PositionCalculator = StandardMoveToPositionCalculator.s_Instance;
			}

			public MoveTo(Transform self, Transform target, Vector3 targetRelativePosition, float time, IMoveToPositionCalculator positionCalculator) : base (self, target, time)
			{
				m_TargetRelativePosition = targetRelativePosition;
				m_PositionCalculator = positionCalculator;
			}


			protected override void OnStart()
			{
				m_InitialPosition = self.position;
			}

			protected override void OnUpdate()
			{
				self.position = m_PositionCalculator.CalculatePosition(m_Target, m_TargetRelativePosition, m_InitialPosition, m_Percentage, m_Curve);
			}

			protected override void OnDone()
			{
				self.position = m_PositionCalculator.CalculatePosition(m_Target, m_TargetRelativePosition, m_InitialPosition, 1.0f, m_Curve);
			}
		}
	}
}
