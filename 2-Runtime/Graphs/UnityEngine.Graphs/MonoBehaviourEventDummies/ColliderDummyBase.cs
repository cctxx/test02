using System;
using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	// For now we do triggers by attaching this MonoBehaviour to needed gameobjects. Class then sends events to logic graph nodes.
	public abstract class ColliderDummyBase : MonoBehaviour
	{
		protected static Component AttachToCollider(Collider self, Type dummyType)
		{
			var attached = GetAndAddComponentIfNeeded(self.gameObject, dummyType);

			if (attached == null)
				throw new ArgumentException("Failed to attach Logic Graph Collider Event handler to a game object of component '" + self + "'.");

			return attached;
		}

		private static Component GetAndAddComponentIfNeeded(GameObject go, Type type)
		{
			return go.GetComponent(type) ?? go.AddComponent(type);
		}
	}
}

