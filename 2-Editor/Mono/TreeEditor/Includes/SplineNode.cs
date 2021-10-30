using UnityEngine;

namespace TreeEditor
{
	[System.Serializable]
	public class SplineNode
	{
		public Vector3 point = Vector3.zero;
		public Quaternion rot = Quaternion.identity;
		public Vector3 normal = Vector3.zero;
		public Vector3 tangent = Vector3.zero;
		public float time;

		public SplineNode(Vector3 p, float t) { point = p; time = t; }
		public SplineNode(SplineNode o)
		{
			point = o.point;
			rot = o.rot;
			normal = o.normal;
			tangent = o.tangent;
			time = o.time;
		}
	}

}