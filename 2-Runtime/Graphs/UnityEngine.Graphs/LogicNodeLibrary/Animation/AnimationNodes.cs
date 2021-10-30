using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class AnimationNodes
	{
		[Logic(typeof(Animation))]
		[return: Title("Animation State")]
		public static AnimationState GetAnimationState (Animation self, [Setting] string animationStateName)
		{
			return self[animationStateName];
		}

		[Logic(typeof(Animation))]
		[return: Title("Animation State")]
		public static AnimationState PlayAnimation (Animation self, [Setting] string animationName, [Setting] bool crossfade, [Setting] float fadeLength, [Setting] PlayMode playMode)
		{
			AnimationState animationState = self[animationName == "" ? self.clip.name : animationName];

			if (crossfade)
				self.CrossFade (animationState.name, fadeLength, playMode);
			else
				self.Play (animationState.name, playMode);

			return animationState;
		}

		[Logic(typeof(Animation))]
		[return: Title("Animation State")]
		public static AnimationState PlayQueuedAnimation (Animation self, [Setting] string animationName, [Setting] bool crossfade, [Setting] float fadeLength, [Setting] QueueMode queueMode, [Setting] PlayMode playMode)
		{
			if (animationName == "")
				animationName = self.clip.name;

			var animationState = crossfade ? 
				self.CrossFadeQueued (animationName, fadeLength, queueMode, playMode) : 
				self.PlayQueued (animationName, queueMode, playMode);

			return animationState;
		}

		[Logic(typeof(Animation))]
		public static void StopAnimation (Animation self, [Setting] string animationName)
		{
			if (animationName == "")
				self.Stop();
			else
				self.Stop(animationName);
		}

		[Logic (typeof (Animation))]
		public static void SampleAnimation (Animation self)
		{
			self.Sample ();
		}

		[Logic(typeof(Animation))]
		public static void StopAnimationState (Animation self, AnimationState animationState)
		{
			self.Stop(animationState.name);
		}

		[Logic(typeof(Animation))]
		public static void BlendAnimationState (Animation self, AnimationState animationState, float targetWeight, [Setting] float fadeLength)
		{
			self.Blend (animationState.name, targetWeight, fadeLength);
		}

		[Logic(typeof(Animation))]
		public static void SyncAnimationLayer (Animation self, int layer)
		{
			self.SyncLayer (layer);
		}
	}
}

