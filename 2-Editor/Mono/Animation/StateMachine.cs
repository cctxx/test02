using NUnit.Framework;
using UnityEngine;
using System.Collections.Generic;
using UnityEditor;
using System.Linq;

namespace UnityEditorInternal
{
public partial class Transition : Object
{
	private static readonly GUIContent s_TransitionTempContent = new GUIContent ();

	public GUIContent GetTransitionContentForRect (Rect rect)
	{
		s_TransitionTempContent.text = uniqueName; 

		Vector2 size = EditorStyles.label.CalcSize (s_TransitionTempContent);

		if (size.x > rect.width)
			s_TransitionTempContent.text = shortDisplayName;

		return s_TransitionTempContent;
	}	
}

public partial class StateMachine : Object
{
	internal IEnumerable<State> states
	{
		get
		{
			var states = new State[stateCount];
			for (int i = 0; i < stateCount; i++)
				states[i] = GetState (i);
			return states;
		}
	}

	internal List<State> statesRecursive
	{
		get
		{		
			List<State> ret = new List<State>();							
			ret.AddRange(states);

			for (int j = 0; j < stateMachineCount; j++)
			{				
				ret.AddRange(GetStateMachine(j).statesRecursive);				
			}
			return ret;
		}		
	}

	internal IEnumerable<StateMachine> stateMachines
	{
		get
		{
			var stateMachines = new StateMachine[stateMachineCount];
			for (int i = 0; i < stateMachineCount; i++)
				stateMachines[i] = GetStateMachine (i);
			return stateMachines;
		}
	}

	internal List<StateMachine> stateMachinesRecursive
	{
		get
		{
			List<StateMachine> ret = new List<StateMachine>();
			ret.AddRange(stateMachines);

			for (int j = 0; j < stateMachineCount; j++)
			{
				ret.AddRange(GetStateMachine(j).stateMachinesRecursive);
			}
			return ret;
		}
	}

	internal List<Transition> transitions
	{
		get
		{
			List<State> allStates = statesRecursive;
			List<Transition> transitions = new List<Transition>();

			foreach (State state in allStates)
			{
				transitions.AddRange(GetTransitionsFromState(state));
			}

			transitions.AddRange(GetTransitionsFromState(null));

			return transitions;
		}
	}

	internal Vector3 GetStateMachinePosition(StateMachine stateMachine)
	{
		for (int i = 0; i < stateMachineCount; i++)
			if (stateMachine == GetStateMachine (i))
				return GetStateMachinePosition (i);

		Assert.Fail ("Can't find state machine (" + stateMachine.name + ") in parent state machine (" + name + ").");
		return Vector3.zero;
	}

	internal void SetStateMachinePosition(StateMachine stateMachine, Vector3 position)
	{
		for (int i = 0; i < stateMachineCount; i++)
			if (stateMachine == GetStateMachine (i))
			{
				SetStateMachinePosition (i, position);
				return;
			}

		Assert.Fail ("Can't find state machine (" + stateMachine.name + ") in parent state machine (" + name + ").");
	}

	internal State CreateState(Vector3 position)
	{
		Undo.RegisterCompleteObjectUndo (this, "State added");		
	
		State newState = AddState ("New State");
		newState.position = position;		

		return newState;
	}


	internal State FindState(string stateUniqueName)
	{
		return statesRecursive.Find(s => s.uniqueName == stateUniqueName);		
	}

	internal State FindState(int stateUniqueNameHash)
	{
		return statesRecursive.Find(s => s.uniqueNameHash == stateUniqueNameHash);		
	}


	internal Transition FindTransition(string transitionUniqueName)
	{
		return transitions.Find( t => t.uniqueName == transitionUniqueName);	
	}

	internal Transition FindTransition(int transitionUniqueName)
	{
		return transitions.Find( t => t.uniqueNameHash == transitionUniqueName);	
	}

	internal bool HasState(State state)
	{
		return statesRecursive.Any(s => s == state);
	}

	internal bool IsDirectParent(StateMachine stateMachine)
	{
		return stateMachines.Any(sm => sm == stateMachine);
	}

	internal bool HasStateMachine(StateMachine child) 
	{
		return stateMachinesRecursive.Any(sm => sm == child);	
	}

	internal bool HasTransition(State stateA, State stateB)
	{		
		return GetTransitionsFromState(stateA).Any(t => t.dstState == stateB) || GetTransitionsFromState(stateB).Any(t => t.dstState == stateA);
	}

}
}
