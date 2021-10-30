#if TESTING

using UnityEngine;

namespace UnityEngine.Graphs
{
	public class Testing
	{
		public class Object
		{
			public string name;
		}

		public class ObjectSub1 : Object { }
		public class ObjectSub2 : Object { }

		// fake animation curve. can't seem to be able to initialize real one out of Unity
		public class AnimationCurve
		{
			public WrapMode postWrapMode;
			public WrapMode preWrapMode;
			public Keyframe[] keys;
			public AnimationCurve(params Keyframe[] frames) { keys = frames; }
		}

		public static bool AmIRunningInMono()
		{
			// internet says this is a supported way of detecting mono runtime
			return System.Type.GetType ("Mono.Runtime") != null;
		}
	}
}
#endif
