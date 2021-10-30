using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public class RigidbodyNode
	{
		#if REIMPLEMENT_USING_CLASS_NODES
		[Logic(typeof(Rigidbody), typeof(NodeLibrary.StartStopEvents))]
		public static IEnumerator Force (Rigidbody self, ByRef<NodeLibrary.StartStopEvents> evt, Vector3 force, bool localSpace, bool ignoreMass)
		{
			if (evt.Value == NodeLibrary.StartStopEvents.Stop)
				yield break;
			if (self)
			{
				ForceMode mode = (ignoreMass ? ForceMode.Acceleration : ForceMode.Force);
				
				if (localSpace)
				{
					while (evt.Value != NodeLibrary.StartStopEvents.Stop)
					{
						yield return new WaitForFixedUpdate();
						self.AddRelativeForce(force, mode);
					}
				}
				else
				{
					while (evt.Value != NodeLibrary.StartStopEvents.Stop)
					{
						yield return new WaitForFixedUpdate();
						self.AddForce(force, mode);
					}
				}
			}
			else
				Debug.LogWarning("Force self parameter is null");
		}
		
		[Logic(typeof(Rigidbody), typeof(NodeLibrary.StartStopEvents))]
		public static IEnumerator Torque (Rigidbody self, ByRef<NodeLibrary.StartStopEvents> evt, Vector3 torque, bool localSpace, bool ignoreMass)
		{
			if (evt.Value == NodeLibrary.StartStopEvents.Stop)
				yield break;
			if (self)
			{
				ForceMode mode = (ignoreMass ? ForceMode.Acceleration : ForceMode.Force);
				
				if (localSpace)
				{
					while (evt.Value != NodeLibrary.StartStopEvents.Stop)
					{
						yield return new WaitForFixedUpdate();
						self.AddRelativeTorque(torque, mode);
					}
				}
				else
				{
					while (evt.Value != NodeLibrary.StartStopEvents.Stop)
					{
						yield return new WaitForFixedUpdate();
						self.AddTorque(torque, mode);
					}
				}
			}
			else
				Debug.LogWarning("Torque self parameter is null");
		}
		#endif

		[Logic(typeof(Rigidbody))]
		public static void ApplyForce (Rigidbody self, Vector3 force, Space relativeTo, ForceMode forceMode)
		{
			if (relativeTo == Space.Self)
				self.AddRelativeForce(force, forceMode);
			else
				self.AddForce(force, forceMode);
		}
		
		[Logic(typeof(Rigidbody))]
		public static void ApplyTorque (Rigidbody self, Vector3 torque, Space relativeTo, ForceMode forceMode)
		{
			if (relativeTo == Space.Self)
				self.AddRelativeTorque(torque, forceMode);
			else
				self.AddTorque(torque, forceMode);
		}
		
		[Logic(typeof(Rigidbody))]
		public static void SetVelocity (Rigidbody self, Vector3 velocity, Space relativeTo)
		{
			if (relativeTo == Space.Self)
				velocity = self.transform.rotation * velocity;
			self.velocity = velocity;
		}

		[Logic(typeof(Rigidbody))]
		public static void SetAngularVelocity (Rigidbody self, Vector3 angularVelocity, Space relativeTo)
		{
			if (relativeTo == Space.Self)
				angularVelocity = self.transform.rotation * angularVelocity;
			self.angularVelocity = angularVelocity;
		}
	}

}
