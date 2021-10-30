using System;
using System.Collections.Generic;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	// This component gets attached to a GameObject with Animation component and handles special LogicGraphEvent.
	public class OnAnimationEventDummy : MonoBehaviour
	{
		private Dictionary<string, Action> m_Events = new Dictionary<string, Action> ();

		public static void AttachToGameObject(Animation component, string eventName, Action delegateToCall)
		{
			var animEventDummy = component.gameObject.GetComponent (typeof (OnAnimationEventDummy)) as OnAnimationEventDummy ??
			                     component.gameObject.AddComponent(typeof(OnAnimationEventDummy)) as OnAnimationEventDummy;

			if (animEventDummy == null)
				throw new ArgumentException("Failed to attach Logic Graph Animation Event handler to a game object of component '" + component + "'.");

			animEventDummy.AddNewEvent(eventName, delegateToCall);
		}

		private void AddNewEvent(string eventName, Action delegateToCall)
		{
			if (!m_Events.ContainsKey(eventName))
				m_Events.Add(eventName, delegateToCall);
			else
				m_Events[eventName] += delegateToCall;
		}

		public void LogicGraphEvent(string eventName)
		{
			Action delegateToCall;

			if (!m_Events.TryGetValue(eventName, out delegateToCall))
			{
				Debug.LogError("Logic Graph failed to handle Animation Event '" + eventName + "'. Receiver was not found.");
				return;
			}

			if (delegateToCall == null)
			{
				Debug.LogError("Logic Graph failed to handle Animation Event '" + eventName + "'. Receiver was null.");
				return;
			}

			delegateToCall ();
		}
	}
}

