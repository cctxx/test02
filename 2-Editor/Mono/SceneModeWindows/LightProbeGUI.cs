using UnityEngine;

namespace UnityEditor
{
	internal class LightProbeGUI
	{
		const int kLightProbeCoefficientCount = 27;
	
		// TODO: maybe this should be a user-editable property, so that it can be tweaked to work with scene scale
		const float kDuplicateEpsilonSq = 0.1f;

		public void DisplayControls(SceneView sceneView)
		{
			LightmapVisualization.showLightProbeLocations = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("LightmapEditor.DisplayControls.ShowProbes"), LightmapVisualization.showLightProbeLocations);
			LightmapVisualization.showLightProbeCells = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("LightmapEditor.DisplayControls.ShowCells"), LightmapVisualization.showLightProbeCells);
			//LightmapVisualization.dynamicUpdateLightProbes = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("LightmapEditor.DisplayControls.DynamicUpdateProbes"), LightmapVisualization.dynamicUpdateLightProbes);
		}
	}

} // namespace
