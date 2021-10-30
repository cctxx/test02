using UnityEngine;
using UnityEditor;
using System.Reflection;
using System.Collections.Generic;
using System;


namespace UnityEditor
{
	[CustomEditor(typeof(LightProbes))]
	class LightProbesInspector : Editor
	{
		public override void OnInspectorGUI()
		{
			GUILayout.BeginVertical(EditorStyles.helpBox);
			LightProbes lp = target as LightProbes;
			GUIStyle labelStyle = EditorStyles.wordWrappedMiniLabel;
			GUILayout.Label("Light probe count: " + lp.count, labelStyle);
			GUILayout.Label("Cell count: " + lp.cellCount, labelStyle);
			GUILayout.EndVertical();
		}
	}
}