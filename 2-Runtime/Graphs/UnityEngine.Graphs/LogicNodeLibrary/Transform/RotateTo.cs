using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class TransformNodes
	{
		[Logic (typeof (Transform))]
		public sealed class RotateTo : YieldedTransformNodeBase
		{
			private readonly IRotateToRotationCalculator m_RotationCalculator;
			private Quaternion m_InitialRotation;
			private Quaternion m_TargetRelativeRotation;

			public override Transform target { set { m_Target = value; } }
			public Quaternion targetOffset { set { m_TargetRelativeRotation = value; } }

			public RotateTo ()
			{
				m_RotationCalculator = StandardRotateToRotationCalculator.s_Instance;
			}

			public RotateTo (Transform self, Transform target, Quaternion targetRelativeRotation, float time, IRotateToRotationCalculator rotationCalculator)
				: base (self, target, time)
			{
				m_TargetRelativeRotation = targetRelativeRotation;
				m_RotationCalculator = rotationCalculator;
			}

			protected override void OnStart ()
			{
				m_InitialRotation = self.rotation;
			}

			protected override void OnUpdate ()
			{
				self.rotation = m_RotationCalculator.CalculateRotation (m_Target, m_TargetRelativeRotation, m_InitialRotation, m_Percentage, m_Curve);
			}

			protected override void OnDone ()
			{
				self.rotation = m_RotationCalculator.CalculateRotation (m_Target, m_TargetRelativeRotation, m_InitialRotation, 1.0f, m_Curve);
			}
		}
	}
}
