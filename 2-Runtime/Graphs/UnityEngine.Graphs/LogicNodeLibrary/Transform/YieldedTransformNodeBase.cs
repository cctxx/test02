using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public abstract class YieldedTransformNodeBase : YieldedNodeBase
	{
		[LogicTarget]
		public Transform self;

		protected Transform m_Target;
		protected AnimationCurve m_Curve;

		public virtual Transform target { set { m_Target = value; } }
		public virtual AnimationCurve curve { set { m_Curve = value; } }

		protected YieldedTransformNodeBase ()
		{
			m_Curve = new AnimationCurve ();
		}

		protected YieldedTransformNodeBase (Transform self, Transform target, float time) : base (time)
		{
			this.self = self;
			m_Target = target;
		}
	}
}
