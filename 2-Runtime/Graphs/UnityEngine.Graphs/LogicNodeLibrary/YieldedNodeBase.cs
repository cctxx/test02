using System.Collections;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public abstract class YieldedNodeBase
	{
		protected float m_Time;
		protected float m_Percentage;

		public Action done;
		public Action update;

		public virtual float totalTime { set { m_Time = value; } }
		public virtual float percentage { get { return m_Percentage; } }

		protected YieldedNodeBase () {}

		protected YieldedNodeBase (float time)
		{
			m_Time = time;
		}

		public IEnumerator Start ()
		{
			OnStart ();

			if (m_Time > 0.0f)
			{
				float doneTime = Time.time + m_Time;
				float t = 0;
				do
				{
					t += Time.deltaTime;
					m_Percentage = t / m_Time;

					OnUpdate();
					if (update != null)
						update();

					yield return 0;
				} while (Time.time < doneTime);
			}

			OnDone();
			if (done != null)
				done();
		}

		protected abstract void OnStart ();
		protected abstract void OnUpdate ();
		protected abstract void OnDone ();
	}
}
