using UnityEngine;
using UnityEditor;
using System.Collections;

#if false
namespace UnityEditor {

internal class GUIDebugHelper : EditorWindow {
	//[MenuItem ("Help/Debug GUI")]	
	static void TestConsole () {
		GUIDebugHelper win = new GUIDebugHelper ();
		win.Show ();
		win.OnEnable ();
	}

	void OnEnable () {
		EditorApplication.update += Repaint;
	}

	void OnGUI () {
		#if true
		
		string t = System.String.Format (
			"Hot: {0}\n\nGrabbed:\n{1}\n\nReleased:\n{2}",
			GUIUtility.hotControl,
			GUIUtility.s_WhoGrabbedHotControl,	
			GUIUtility.s_WhoReleasedHotControl
			
		);
		if (Event.current.type == EventType.MouseDown)
			Debug.Log (t);
		GUILayout.TextArea (t);		

		#else
		
		if (Event.current.type == EventType.MouseDown)
			Debug.Log ("Unsupported Must enable HOT_CONTROL_DEBUG in MonoCompile.cpp");
		
		#endif
	}
}

} //namespace
#endif