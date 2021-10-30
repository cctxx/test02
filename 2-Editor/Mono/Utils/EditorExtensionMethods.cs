using UnityEngine;

namespace UnityEditor
{

internal static class EditorExtensionMethods
{
	// Use this method when checking if user hit Space or Return in order to activate the main action
	// for a control, such as opening a popup menu or color picker.
	internal static bool MainActionKeyForControl (this UnityEngine.Event evt, int controlId)
	{
		if (EditorGUIUtility.keyboardControl != controlId)
			return false;
		
		bool anyModifiers = (evt.alt || evt.shift || evt.command || evt.control);
		
		// Block window maximize (on OSX ML, we need to show the menu as part of the KeyCode event, so we can't do the usual check)
		if (evt.type == EventType.KeyDown && evt.character == ' ' && !anyModifiers)
		{
			evt.Use ();
			return false;
		}
		
		// Space or return is action key
		return evt.type == EventType.KeyDown &&
			(evt.keyCode == KeyCode.Space || evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter) &&
			!anyModifiers;
	}
}

}

