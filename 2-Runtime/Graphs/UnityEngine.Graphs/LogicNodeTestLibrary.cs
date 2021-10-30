
#if NOT
// Nodes for trying-out purposes. Complete mess.

using System.Collections;
using UnityEngine;
using EmptyDelegate = UnityEngine.Graphs.LogicGraph.LogicNodeUtility.EmptyDelegate;
using ColliderDelegate = UnityEngine.Graphs.LogicGraph.NodeLibrary.ColliderDelegate;

namespace UnityEngine.Graphs.LogicGraph
{
	public class LogicNodeTestLibrary
	{
		[Logic]
		public static IEnumerator DoSomethingWithAnimationCurve(AnimationCurve curve)
		{
			var go = GameObject.Find("Cube");
		
			for (float f = 0; f < 2f; f+=0.01f)
			{
				go.transform.Rotate(new Vector3(curve.Evaluate(f) * 10f, 0,0));
				yield return new WaitForFixedUpdate();
			}
		}

		[Logic]
		public class LimitedInvoker
		{
			public int invokeTimes = 500;
			int currentInvokeIndex = 0;

			public delegate void VoidDelegate();

			public VoidDelegate myOut;

			public void In()
			{
				if (++currentInvokeIndex > invokeTimes)
					return;

				if (currentInvokeIndex % 500 == 0)
					Debug.Log(currentInvokeIndex);

				if (myOut != null)
					myOut();
			}
		}

		[Logic(null, null, typeof(int))]
		public static void Counter(ref int state, int UpTo, EmptyDelegate done)
		{
			state++;
			if (state >= UpTo)
				if (done != null)
					done();
		}

		[Logic]
		public static void HasTag(GameObject go, string tag, EmptyDelegate True, EmptyDelegate False)
		{
			if (go.tag == tag)
			{
				if (True != null)
					True();
			}
			else
			{
				if (False != null)
					False();
			}
		}

		[Logic]
		public static event EmptyDelegate StaticEvent;

		public static void CauseStaticEvent()
		{
			if (StaticEvent != null)
				StaticEvent();
		}

		// Enums for in slots
		public enum StartStopEvents { Start, Stop }
		public enum StartPauseStopEvents { Start, Pause, Stop }

		[LogicEval(typeof(Transform))]
		public static float GetPositionY(Transform self)
		{
			return self.position.y;
		}

		[Logic]
		public static string PassString()
		{
			return "Some String";
		}

		[Logic]
		public static string AddStrings(string a, string b)
		{
			return a + " + " + b;
		}

		[Logic]
		public static void LogCollidersGO(Collider collider)
		{
			Debug.Log("Ouch!.. " + collider.gameObject.name);
		}

		[Logic(typeof(Collider))]
		public static void ColliderFunction(Collider self)
		{
			Debug.Log("ColliderFunction " + self.gameObject.name);
		}

		[Logic]
		public static IEnumerator YieldedFunction(string strParam)
		{
			yield return new WaitForSeconds(3);
			Debug.Log(strParam);
		}

		public enum MyActions { Start, Stop, Pause }

		[Logic(typeof(AudioSource), typeof(MyActions))]
		public static void PlayAudio(AudioSource self, MyActions action)
		{
			switch (action)
			{
				case MyActions.Start:
					self.Play();
					break;
				case MyActions.Stop:
					self.Stop();
					break;
				case MyActions.Pause:
					self.Pause();
					break;
			}
		}

		public class Expressions
		{
			[LogicExpression]
			public static int CustomFn(int f)
			{
				return f*f;
			}
	
			[LogicExpression]
			public static float Prop
			{
				get
				{
					return 3f;
				}
			}
		}

		[Logic]
		public static int intVar;

		[Logic]
		public static string strProperty { get { return "0"; } set { Debug.Log("setting to: " + value); } }

		[Logic]
		public static int SetInt (int a)
		{
			return a;
		}

		[Logic]
		public static void EvaluateBool (bool b, EmptyDelegate True, EmptyDelegate False)
		{
			if (b)
				True();
			else
				False();
		}

		[Logic]
		public static void Destroy (GameObject self)
		{
			Object.Destroy(self);
		}

		[Logic]
		public static GameObject Instantiate (GameObject obj, Vector3 position, Quaternion rotation)
		{
			return (GameObject)GameObject.Instantiate(obj, position, rotation);
		}

		[Logic]
		public static void Do() { }

		[Logic]
		public static void FindCollidersInRadius (Vector3 center, float radius, ColliderDelegate affected, ColliderDelegate done)
		{
			Collider[] colliders = Physics.OverlapSphere(center, radius);
			foreach (Collider col in colliders)
			{
				affected(col);
			}
			if (done == null)
				Debug.LogWarning("done delegate is null");
			else
				done(colliders[0]);
		}

		// Eval

		[LogicEval]
		public static Vector3 Vector3FromFloats (float x, float y, float z)
		{
			return new Vector3(x, y, z);
		}

		[LogicEval]
		public static int Add (int a, int b)
		{
			return a + b;
		}

		[LogicEval]
		public static float Random (float min, float max)
		{
			return UnityEngine.Random.Range(min, max);
		}

		[LogicEval]
		public static float InputAxis (string axisName)
		{
			return Input.GetAxis(axisName);
		}

		[LogicEval]
		public static GameObject GameObjectVar (GameObject obj)
		{
			return obj;
		}

		[LogicEval]
		public static Vector3 InverseDistVector (Vector3 from, Vector3 to, float multiplier)
		{
			float dist = Vector3.Distance(from, to);
			if (dist == 0)
				return Vector3.zero;
			else
				return (to - from) / (dist * dist) * multiplier;
		}

	}

	[Logic]
	public class NodeInClass
	{
		public int simpleVariable;
		public string onlyGet
		{
			get { return string.Empty; }
		}

		public string onlySet
		{
			set { }
		}

		public delegate void VoidDelegate();
		public VoidDelegate ExitLink1;
		public VoidDelegate ExitLink2;

		public void Input1() { Debug.Log("input1");}
		public IEnumerator YieldedInput() { return null; }
	}

	[Logic(typeof(Collider))]
	public class ColliderNodeInClass
	{
		public Collider target;
		public void DoSomething() { Debug.Log(target);}
	}

	[Logic]
	public class TitledStuff
	{
		public enum TitledEnum { [Title("Crazy")]Start, [Title("Thing")] Stop, [Title("ToDo")] Pause }

		public delegate void TwoStringTitledDelegate([Title("String 1")]string str1, [Title("String 2")]string str2);
		public delegate void TwoObjectsTitledEvent([Title("GO arg")]Object go, [Title("other arg")]Object other);

		[Logic]
		[Title("-::Custom Named Fn::-")]
		[return: Title("My ¿€T")]
		public static int FunctionWithCustomTitles([Title("First variable")]string var1, [Title("Second variable")]int var2) { return 0; }

		[Logic]
		[Title("-::Custom Named delegate Fn::-")]
		public static void CustomNameFnWithDelegates([Title("Str input")]string string1, [Title("Output 1")]TwoStringTitledDelegate out1, [Title("Output 2")]TwoStringTitledDelegate out2) { }

		[Logic]
		[Title("@#^@#$")]
		public static TwoObjectsTitledEvent TitledEvent;

		[Logic]
		[Title("Ghy")]
		public static int titledVar;
		[Logic]
		[Title("Ghy@$^")]
		public static int titledProp { get { return 0; } }

		[LogicEval]
		[Title("Eval 123")]
		[return: Title("Eval Ret")]
		public static int TitledEval([Title("eval arg")]string str) { return 0; }

		[Logic(null, typeof(TitledEnum))]
		[Title("it's really titled")]
		public static void TitledMultiInputFunction(TitledEnum actions, [Title("Var In 1")]int prm1, [Title("Var In 2")]string prm2) { }
	}

	[Logic]
	[Title("-::Custom Named Class::-")]
	public class CustomTitleNodeInClass
	{
		[Title("Var Custom")]
		public int simpleVariable;

		[Title("Property Custom")]
		public string onlyGet { get { return string.Empty; } }

		public delegate void VoidDelegate();
		[Title("Exit link custom")]
		public VoidDelegate ExitLink1;

		[Title("Input custom")]
		public void Input1() { }
	}

	[Logic]
	public class ValidatingNodes
	{
		public delegate void	GOEvent(GameObject go);
		[Logic(typeof(GameObject)), Validate("MyValidate")] public static GOEvent ValidatedDelegate;
		[Logic(typeof(GameObject)), Validate("MyValidate")] public static event GOEvent ValidatedEvent;

		[Logic(typeof(GameObject)), Validate("MyValidate")]
		public static void FunctionNodeWithValidate(GameObject target){}

		public static bool MyValidate(GameObject target) { return target.name == "ShowMe"; }
	}

	[Logic(typeof(GameObject)), Validate("ValidatingNodes.MyValidate")]
	public class ClassWithValidate
	{
		public GameObject target;
		public void Input(){}
	}
}
#endif