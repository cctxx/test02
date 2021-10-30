using UnityEditor;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor(typeof(AudioDistortionFilter))]
	class AudioDistortionFilterEditor : Editor
	{
		public override void OnInspectorGUI() {			
			if (!Application.HasAdvancedLicense())
			{
				GUILayout.BeginHorizontal();
				GUIContent c = new GUIContent("This is only available in the Pro version of Unity.");
				GUILayout.Label(c, EditorStyles.helpBox);
				GUILayout.EndHorizontal();
			}		
			base.OnInspectorGUI();
		}
	}
}
