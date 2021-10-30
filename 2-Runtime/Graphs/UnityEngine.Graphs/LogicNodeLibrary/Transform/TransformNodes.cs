using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class TransformNodes
	{
		#region Nodes
		[Logic(typeof(Transform))]
		public static void Translate(Transform self, Vector3 translation, Space relativeTo)
		{
			self.Translate(translation, relativeTo);
		}

		[Logic(typeof(Transform))]
		public static void Rotate(Transform self, Vector3 axis, float angle, Space relativeTo)
		{
			self.Rotate(axis, angle, relativeTo);
		}

		[Logic(typeof(Transform))]
		public static void Mimic(Transform self, Transform target, bool mimicPosition, bool mimicRotation, bool mimicScale, bool useLocalSpace)
		{
			if (mimicPosition)
				if (useLocalSpace)
					self.localPosition = target.localPosition;
				else
					self.position = target.position;

			if (mimicRotation)
				if (useLocalSpace)
					self.localRotation = target.localRotation;
				else
					self.rotation = target.rotation;

			if (mimicScale)
				self.localScale = target.localScale;
		}

		[LogicEval(typeof(Transform))]
		[Title("Get Position")]
		public static Vector3 GetPosition(Transform target)
		{
			if (target == null)
				return Vector3.zero;
			return target.position;
		}

		[Logic(typeof(Transform))]
		[Title("Set Position")]
		public static void SetPosition(Transform target, Vector3 position)
		{
			if (target == null)
				return;
			target.position = position;
		}
		#endregion

		#region Node Helpers
		private static Quaternion LookAtLookRotation(Transform self, Transform target, Vector3 targetRelativePosition)
		{
			return Quaternion.LookRotation(AbsoluteTargetPosition(target, targetRelativePosition) - self.position);
		}

		private static Vector3 AbsoluteTargetPosition(Transform target, Vector3 targetRelativePosition)
		{
			if (target != null)
				return target.position + targetRelativePosition;
			return targetRelativePosition;
		}
		#endregion

		#region Transform Calculators
		public interface IMoveToPositionCalculator
		{
			Vector3 CalculatePosition (Transform target, Vector3 targetRelativePosition, Vector3 initialPosition, float percentage, AnimationCurve curve);
		}

		class StandardMoveToPositionCalculator : IMoveToPositionCalculator
		{
			public static readonly IMoveToPositionCalculator s_Instance = new StandardMoveToPositionCalculator ();

			public Vector3 CalculatePosition (Transform target, Vector3 targetRelativePosition, Vector3 initialPosition, float percentage, AnimationCurve curve)
			{
				return Vector3.Lerp (initialPosition, AbsoluteTargetPosition (target, targetRelativePosition), curve.Evaluate (percentage));
			}
		}


		public interface IRotateToRotationCalculator
		{
			Quaternion CalculateRotation (Transform target, Quaternion targetRelativeRotation, Quaternion initialRotation, float percentage, AnimationCurve curve);
		}

		class StandardRotateToRotationCalculator : IRotateToRotationCalculator
		{
			public static readonly IRotateToRotationCalculator s_Instance = new StandardRotateToRotationCalculator ();

			public Quaternion CalculateRotation (Transform target, Quaternion targetRelativeRotation, Quaternion initialRotation, float percentage, AnimationCurve curve)
			{
				return Quaternion.Lerp (initialRotation, targetRelativeRotation * target.rotation, curve.Evaluate (percentage));
			}
		}


		public interface ILookAtRotationCalculator
		{
			Quaternion CalculateRotation (Transform self, Transform target, Vector3 targetRelativePosition, Quaternion initialRotation, float percentage, AnimationCurve curve);
		}

		class StandardLookAtRotationCalculator : ILookAtRotationCalculator
		{
			public static readonly ILookAtRotationCalculator s_Instance = new StandardLookAtRotationCalculator ();

			public Quaternion CalculateRotation (Transform self, Transform target, Vector3 targetRelativePosition, Quaternion initialRotation, float percentage, AnimationCurve curve)
			{
				return Quaternion.Lerp (initialRotation, LookAtLookRotation (self, target, targetRelativePosition), curve.Evaluate (percentage));
			}
		}
		#endregion
	}
}

