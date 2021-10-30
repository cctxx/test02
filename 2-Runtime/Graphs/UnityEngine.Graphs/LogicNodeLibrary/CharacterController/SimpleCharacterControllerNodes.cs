using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public sealed class SimpleCharacterControllerNodes
	{
		[Logic(typeof(CharacterController))]
		public static void SimpleMove (CharacterController self, Vector3 speed, Action grounded, Action airborne)
		{
			if (self.SimpleMove (speed))
			{
				if (grounded != null) grounded ();
			}
			else if (airborne != null) airborne ();
		}

		[Logic(typeof(CharacterController))]
		public static CollisionFlags Move (CharacterController self, Vector3 motion)
		{
			return self.Move (motion);
		}
	}
}

