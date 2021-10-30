using System.Collections.Generic;
using UnityEngine;
using NUnit.Framework;

// The ParticleEffectUI displays one or more ParticleSystemUIs.

namespace UnityEditor
{

internal class ParticleEffectUtils
{
	static List<GameObject> s_Planes = new List<GameObject>(); // Shared planes used by CollisionModuleUI

	// Shared planes used by CollisionModuleUI
	static public GameObject GetPlane (int index)
	{
		while (s_Planes.Count <= index)
		{
			GameObject plane = GameObject.CreatePrimitive(PrimitiveType.Plane);
			plane.hideFlags = HideFlags.HideAndDontSave;
			s_Planes.Add (plane);
		}
		
		return s_Planes[index];
	}

	static public void ClearPlanes ()
	{
		if (s_Planes.Count > 0)
		{
			foreach (GameObject plane in s_Planes)
				Object.DestroyImmediate (plane);
			s_Planes.Clear ();
		}
	}
}

} // namespace UnityEditor
