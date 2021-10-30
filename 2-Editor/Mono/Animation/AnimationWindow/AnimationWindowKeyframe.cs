using UnityEngine;
using UnityEditor;

namespace UnityEditorInternal
{
	internal class AnimationWindowKeyframe
	{
		public float			m_InTangent;
		public float			m_OutTangent;
		public int 				m_TangentMode;
		public int				m_TimeHash;
		int						m_Hash;

		float					m_time;

		object					m_value;
		
		AnimationWindowCurve	m_curve;
		public float time
		{
			get { return m_time; }
			set 
			{ 
				m_time = value;
				m_Hash = 0;
				m_TimeHash = value.GetHashCode();
			}
		}

		public object value
		{
			get { return m_value; }
			set
			{
				m_value = value;
				m_Hash = 0;
			}
		}

		public AnimationWindowCurve curve
		{
			get { return m_curve; }
			set 
			{
				m_curve = value;
				m_Hash = 0;
			}
		}

		public bool isPPtrCurve { get { return curve.isPPtrCurve; } }

		public AnimationWindowKeyframe()
		{
		}

		public AnimationWindowKeyframe (AnimationWindowKeyframe key)
		{
			this.time = key.time;
			this.value = key.value;
			this.curve = key.curve;
			this.m_InTangent = key.m_InTangent;
			this.m_OutTangent = key.m_OutTangent;
			this.m_TangentMode = key.m_TangentMode;
		}

		public AnimationWindowKeyframe (AnimationWindowCurve curve, Keyframe key)
		{
			this.time = key.time;
			this.value = key.value;
			this.curve = curve;
			this.m_InTangent = key.inTangent;
			this.m_OutTangent = key.outTangent;
			this.m_TangentMode = key.tangentMode;
		}

		public AnimationWindowKeyframe (AnimationWindowCurve curve, ObjectReferenceKeyframe key)
		{
			this.time = key.time;
			this.value = key.value;
			this.curve = curve;
		}

		public int GetHash ()
		{
			if (m_Hash == 0)
			{
				// Berstein hash
				unchecked
				{
					m_Hash = curve.GetHashCode();
					m_Hash = 33 * m_Hash + time.GetHashCode();
				}
			}

			return m_Hash;
		}
	}
}
