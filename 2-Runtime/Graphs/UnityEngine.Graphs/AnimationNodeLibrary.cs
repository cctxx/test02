#if UNITY_ANIMATION_GRAPH

using UnityEngine.Graphs.LogicGraph;
using UnityEngine;

namespace UnityEngine.Graphs
{
	public class AnimationNodeLibrary
	{
		[AnimationLogic(typeof(Animation))]
		public static void SetSpeed1 (Animation self, string animName, float speed)
		{
			// Sanity check
			if (self)
				self[animName].speed = speed;
			else
				Debug.LogWarning("SetSpeed self parameter is null");
		}
	}
}

#endif
