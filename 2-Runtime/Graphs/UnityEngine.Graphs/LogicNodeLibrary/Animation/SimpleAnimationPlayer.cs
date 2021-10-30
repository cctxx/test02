using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class AnimationNodes
	{
		[Logic(typeof(Animation))]
		public sealed class SimpleAnimationPlayer
		{
			// Logic Target
			public Animation self;

			// Settings
			private AnimationState m_AnimationState;
			[Setting]
			public string animationName 
			{ 
				set 
				{
					m_AnimationState = self[value != "" ? value : self.clip.name];
				}
			}
			private bool m_Crossfade;
			[Setting]
			public bool crossfade { set { m_Crossfade = value; } }
			private float m_FadeLength;
			[Setting]
			public float fadeLength { set { m_FadeLength = value; } }

			private bool m_IsPaused;
			private float m_ResumeSpeed;

			public SimpleAnimationPlayer () { }

			public SimpleAnimationPlayer (Animation self, string animationName, bool crossfade, float fadeLength)
			{
				this.self = self;
				this.animationName = animationName;
				m_Crossfade = crossfade;
				m_FadeLength = fadeLength;
			}

			public void Play ()
			{
				if (m_Crossfade)
					self.CrossFade (m_AnimationState.name, m_FadeLength);
				else
					self.Play (m_AnimationState.name);
			}

			public void Stop ()
			{
				StopAnimationState (m_AnimationState);
			}
			
			[Title("Pause/Resume")]
			public void PauseResume ()
			{
				float tmpSpeed = m_AnimationState.speed;
				m_AnimationState.speed = m_IsPaused ? m_ResumeSpeed : 0.0f;

				m_ResumeSpeed = tmpSpeed;
				m_IsPaused = !m_IsPaused;
			}

			public void Rewind ()
			{
				m_AnimationState.time = 0.0f;
			}

			private static void StopAnimationState (AnimationState animationState)
			{
				animationState.enabled = false;
				animationState.time = 0.0f;
			}
		}
	}
}
