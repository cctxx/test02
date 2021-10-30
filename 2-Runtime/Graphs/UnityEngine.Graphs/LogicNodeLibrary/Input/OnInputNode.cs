using System;


namespace UnityEngine.Graphs.LogicGraph
{
	public partial class InputNodes
	{
		public abstract class OnInputNode
		{
			protected IMonoBehaviourEventCaller m_GraphBehaviour;

			protected float m_DeltaTime;
			public float deltaTime { get { return m_DeltaTime; } }

			public Action down;
			public Action up;

			protected OnInputNode () { }

			protected OnInputNode (GraphBehaviour graphBehaviour)
			{
				m_GraphBehaviour = graphBehaviour;
				m_GraphBehaviour.OnUpdate += OnBaseUpdate;
			}

			protected OnInputNode (IMonoBehaviourEventCaller graphBehaviour)
			{
				m_GraphBehaviour = graphBehaviour;
				m_GraphBehaviour.OnUpdate += OnBaseUpdate;
			}

			private void OnBaseUpdate (float deltaTime)
			{
				m_DeltaTime = deltaTime;
				OnUpdate ();
			}

			protected abstract void OnUpdate ();
		}

		public abstract class OnStateInputNode : OnInputNode
		{
			public Action onDown;
			public Action onUp;

			protected OnStateInputNode (GraphBehaviour graphBehaviour) : base (graphBehaviour) { }
			protected OnStateInputNode (IMonoBehaviourEventCaller graphBehaviour) : base (graphBehaviour) { }
		}
	}
}
