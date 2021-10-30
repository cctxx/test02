using System.Collections;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public class NodeLibrary
	{
		public enum ToggleEnum { On, Off, Toggle }

		public static string version
		{
			get { return "0.3a"; }
		}

		// this is actually used, leave here when cleaning up this class
		public static float Iff(bool a, float t, float f)
		{
			return a ? t : f;
		}

		public static int Iffint(bool a, int t, int f)
		{
			return a ? t : f;
		}

		public static bool IsTrigger(Collider target)
		{
			return target.isTrigger;
		}

		public static bool IsNotTrigger(Collider target)
		{
			return !target.isTrigger;
		}

		[Logic]
		[Title("Logic/Log")]
		public static void Log (string str)
		{
			Debug.Log(str);
		}

		[Logic]
		[Title("Logic/Wait")]
		public static IEnumerator Wait (float waitSeconds)
		{
			yield return new WaitForSeconds(waitSeconds);
		}

		[Logic]
		[Title("Logic/Timer")]
		public static IEnumerator Timer (float waitSeconds, int repeatCount, Action tick, Action done)
		{
			for (int i = 0; i < repeatCount; i++)
			{
				yield return new WaitForSeconds(waitSeconds);
				if (tick != null)
					tick();
			}

			if (done != null)
				done();
		}

		[Logic]
		[Title("Logic/Nop")]
		public static T Nop<T>(T arg)
		{
			return arg;
		}

		[Logic]
		[Title("Object/Instantiate")]
		[return: Title("Instantiated Object")]
		public static Object Instantiate ([Title("Object")] Object obj, Vector3 position, Quaternion rotation)
		{
			return Object.Instantiate(obj, position, rotation);
		}

		[Logic]
		[Title("Object/Destroy")]
		public static void Destroy ([Title("Object")] Object obj)
		{
			Object.Destroy(obj);
		}

		[Logic]
		[Title("Object/Dont Destroy On Load")]
		public static void DontDestroyOnLoad([Title("Object")] Object obj)
		{
			Object.DontDestroyOnLoad (obj);
		}
	}
}
