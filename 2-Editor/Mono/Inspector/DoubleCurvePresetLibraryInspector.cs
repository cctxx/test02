using System;
using System.IO;
using UnityEngine;

namespace UnityEditor
{
[CustomEditor(typeof(DoubleCurvePresetLibrary))]
internal class DoubleCurvePresetLibraryEditor : Editor
{
	private GenericPresetLibraryInspector<DoubleCurvePresetLibrary> m_GenericPresetLibraryInspector;

	public void OnEnable()
	{
		string filePath = AssetDatabase.GetAssetPath(target.GetInstanceID());
		m_GenericPresetLibraryInspector = new GenericPresetLibraryInspector<DoubleCurvePresetLibrary>(target, GetHeader(filePath),  null);
		m_GenericPresetLibraryInspector.presetSize = new Vector2(72, 20);
		m_GenericPresetLibraryInspector.lineSpacing = 5f;
	}

	private string GetHeader(string filePath)
	{
		return "Particle Curve Preset Library";

		// Header per extension (keep if needed)
		/*string extension = Path.GetExtension(filePath).TrimStart('.');
		string doublesigned = PresetLibraryLocations.GetParticleCurveLibraryExtension(false, true);
		string doubleunsigned = PresetLibraryLocations.GetParticleCurveLibraryExtension(false, false);
		string singlesigned = PresetLibraryLocations.GetParticleCurveLibraryExtension(true, true);
		string singleunsigned = PresetLibraryLocations.GetParticleCurveLibraryExtension(true, false);
		if (extension.Equals(doublesigned, StringComparison.OrdinalIgnoreCase))
			return "Double Curve Preset Library"; //" (-1 to 1)";

		if (extension.Equals(doubleunsigned, StringComparison.OrdinalIgnoreCase))
			return "Double Curve Preset Library"; // " (0 to 1)";

		if (extension.Equals(singlesigned, StringComparison.OrdinalIgnoreCase))
			return "Single Curve Preset Library"; // " (-1 to 1)";

		if (extension.Equals(singleunsigned, StringComparison.OrdinalIgnoreCase))
			return "Single Curve Preset Library"; // " (0 to 1)";

		Debug.LogError("Extension not handled");
		return "Particle Curve Preset Library";*/
	}

	public void OnDestroy()
	{
		if (m_GenericPresetLibraryInspector != null)
			m_GenericPresetLibraryInspector.OnDestroy();
	}

	public override void OnInspectorGUI()
	{
		if (m_GenericPresetLibraryInspector != null)
			m_GenericPresetLibraryInspector.OnInspectorGUI();
	}
}

} // namespace


