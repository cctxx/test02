using System;
using UnityEngine;

#if TESTING
using Object = UnityEngine.Graphs.Testing.Object;
#else
using Object = UnityEngine.Object;
#endif

namespace UnityEngine.Graphs.LogicGraph
{
	public interface IMonoBehaviourEventCaller
	{
		event GraphBehaviour.VoidDelegate OnAwake;
		event GraphBehaviour.VoidDelegate OnStart;
		event GraphBehaviour.VoidUpdateDelegate OnUpdate;
		event GraphBehaviour.VoidUpdateDelegate OnLateUpdate;
		event GraphBehaviour.VoidUpdateDelegate OnFixedUpdate;
	}

	//TODO: we can optimize this, now say Update will get called always for each graph even though nothing is using it
	// we can generate code for those in generated graph instead

	// Component that represents graph in the hierarchy. 
	// When used in editor it stores all LogicGraph state as well, but since those are editor side classes they get stripped out when the player is built.
	public class GraphBehaviour :
	#if TESTING
	Object, IMonoBehaviourEventCaller
	#else
	MonoBehaviour, IMonoBehaviourEventCaller
	#endif
	{
		[NonSerialized]
		private bool m_IsInitialized;

		#region Nodes
		public delegate void VoidDelegate ();
		public delegate void VoidUpdateDelegate (float deltaTime);

		[Logic]
		[Title("Flow Events/On Awake")]
		public event VoidDelegate OnAwake;

		[Logic]
		[Title("Flow Events/On Start")]
		public event VoidDelegate OnStart;

		[Logic]
		[Title("Flow Events/On Update")]
		public event VoidUpdateDelegate OnUpdate;

		[Logic]
		[Title("Flow Events/On Late Update")]
		public event VoidUpdateDelegate OnLateUpdate;

		[Logic]
		[Title("Flow Events/On Fixed Update")]
		public event VoidUpdateDelegate OnFixedUpdate;

		protected virtual void @ΣInit()
		{
		}

		private void Initialize()
		{
			if (m_IsInitialized)
				return;
			m_IsInitialized = true;
			@ΣInit();
		}

		public void OnEnable()
		{
			Initialize ();
		}

		public void Awake ()
		{
			Initialize ();

			if (OnAwake != null)
				OnAwake();
		}

		public void Start ()
		{
			if (OnStart != null)
				OnStart ();
		}

		public void Update ()
		{
			if (OnUpdate != null)
				OnUpdate (Time.deltaTime);
		}

		public void LateUpdate ()
		{
			if (OnLateUpdate != null)
				OnLateUpdate (Time.deltaTime);
		}

		public void FixedUpdate ()
		{
			if (OnFixedUpdate != null)
				OnFixedUpdate (Time.deltaTime);
		}
		#endregion

		protected void PullSceneReferenceVariables(string references)
		{
			foreach (var asm in System.AppDomain.CurrentDomain.GetAssemblies())
				if (asm.GetName().Name == "UnityEditor.Graphs.LogicGraph")
				{
					var method = asm.GetType("UnityEditor.Graphs.LogicGraph.CompilerUtils").GetMethod("SetEditorModeGeneratedGraphSceneReferences");
					method.Invoke(null, new object[] {this, references});
					break;
				}
		}
	}
}
