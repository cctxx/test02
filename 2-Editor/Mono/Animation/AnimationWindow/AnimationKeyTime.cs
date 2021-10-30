using System;

namespace UnityEditorInternal
{
	struct AnimationKeyTime
	{
		private float	m_FrameRate;
		private int		m_Frame;
		private float	m_Time;
		
		public float	time 		{ get { return m_Time; 		} }
		public int		frame		{ get { return m_Frame; 	} }
		public float	frameRate	{ get { return m_FrameRate; 	} }
		
		/// <summary>
		/// A frame has a range of time. This is the beginning of the frame.
		/// </summary>
		public float frameFloor
		{
			get
			{
				return ((float)frame - 0.5f) / frameRate;
			}
		}
		
		/// <summary>
		/// A frame has a range of time. This is the end of the frame.
		/// </summary>
		public float frameCeiling
		{
			get
			{
				return ((float)frame + 0.5f) / frameRate;
			}
		}
		
		public static AnimationKeyTime  Time (float time, float frameRate)
		{
			AnimationKeyTime key = new AnimationKeyTime ();
			key.m_Time = time;
			key.m_FrameRate = frameRate;
			key.m_Frame = UnityEngine.Mathf.RoundToInt (time * frameRate);
			return key;
		}
		
		public static AnimationKeyTime Frame (int frame, float frameRate)
		{
			AnimationKeyTime key = new AnimationKeyTime ();
			key.m_Time = frame / frameRate;
			key.m_FrameRate = frameRate;
			key.m_Frame = frame;
			return key;
		}
		
		// Check if a time in seconds overlaps with the frame
		public bool ContainsTime (float time)
		{
			return time >= frameFloor && time < frameCeiling;
		}
	}
}