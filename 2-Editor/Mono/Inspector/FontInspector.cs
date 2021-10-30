using UnityEngine;
using UnityEditor;


namespace UnityEditor
{
	[CustomEditor(typeof(Font))]
	[CanEditMultipleObjects]
	internal class FontInspector : Editor
	{
		public override void OnInspectorGUI ()
		{
			foreach (Object o in targets)
			{
				// Dont draw the default inspector for imported font assets.
				// It can be very slow when there is a lot of embedded font data, 
				// and the presented information is not useful to the user anyways.
				// We still need it for editable, "Custom Font" assets, though.
				if (o.hideFlags == HideFlags.NotEditable)
					return;
			}

			DrawDefaultInspector ();
		}
	}
}
