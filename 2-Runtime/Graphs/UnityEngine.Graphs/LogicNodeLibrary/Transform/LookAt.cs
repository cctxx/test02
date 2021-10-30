using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class TransformNodes
	{
		[Logic(typeof(Transform))]
		public sealed class LookAt : YieldedTransformNodeBase
		{
			private readonly ILookAtRotationCalculator m_RotationCalculator;
			private Quaternion m_InitialRotation;
			private Vector3 m_TargetRelativePosition;

			public override Transform target { set { m_Target = value; } }
			public Vector3 targetOffset { set { m_TargetRelativePosition = value; } }


			public LookAt ()
			{
				m_RotationCalculator = StandardLookAtRotationCalculator.s_Instance;
			}

			public LookAt (Transform self, Transform target, Vector3 targetRelativePosition, float time, ILookAtRotationCalculator rotationCalculator) : base (self, target, time)
			{
				m_TargetRelativePosition = targetRelativePosition;
				m_RotationCalculator = rotationCalculator;
			}

			protected override void OnStart()
			{
				m_InitialRotation = self.rotation;
			}

			protected override void OnUpdate()
			{
				self.rotation = m_RotationCalculator.CalculateRotation(self, m_Target, m_TargetRelativePosition, m_InitialRotation, m_Percentage, m_Curve);
			}

			protected override void OnDone()
			{
				self.rotation = m_RotationCalculator.CalculateRotation(self, m_Target, m_TargetRelativePosition, m_InitialRotation, 1.0f, m_Curve);
			}
		}
	}
}

