using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

#if ENABLE_2D_PHYSICS

namespace UnityEditor
{

[CustomEditor(typeof(Physics2DSettings))]
internal class Physics2DSettingsInspector : Editor
{
	Vector2 scrollPos;
	bool show = true;

	bool GetValue (int layerA, int layerB)
	{
		return !Physics2D.GetIgnoreLayerCollision (layerA, layerB);
	}
	void SetValue (int layerA, int layerB, bool val)
	{
		Physics2D.IgnoreLayerCollision (layerA, layerB, !val);
	}
	
	public override void OnInspectorGUI ()
	{
		DrawDefaultInspector();
		LayerMatrixGUI.DoGUI ("Layer Collision Matrix", ref show, ref scrollPos, GetValue, SetValue);
	}
}
}

#endif // #if ENABLE_2D_PHYSICS
