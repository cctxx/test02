using UnityEngine;
using UnityEditor;

namespace UnityEditor
{
	internal partial class LicenseManagementWindow : EditorWindow
	{
		static private int width = 600;
		static private int height = 330;
		static private int left = 0;
		static private int top = 0;
		static private int offset = 50;
		static private int buttonWidth = 140;
		
		static private Rect windowArea;
		static private Rect rectArea = new Rect(offset, offset, width-offset*2, height-offset*2);
		static LicenseManagementWindow win = null;
		
		static void ShowWindow()
		{
			Resolution res = Screen.currentResolution;
			left = (res.width - width) / 2;
			top = (res.height - height) / 2;
			windowArea = new Rect(left, top, width, height);
			win = EditorWindow.GetWindowWithRect<LicenseManagementWindow>(windowArea, true, "License Management");
			win.position = windowArea;
			win.Show();
		}
		
		void OnGUI()
		{
			GUILayout.BeginArea(rectArea);
			GUILayout.FlexibleSpace();
			
			GUILayout.BeginHorizontal();
			if (GUILayout.Button("Check for updates", GUILayout.ExpandHeight(true), GUILayout.Width(buttonWidth)))
				CheckForUpdates();
			GUI.skin.label.wordWrap = true;
			GUILayout.Label ("Checks for updates to the currently installed license. If you have purchased "
				+ "addons you can install them by pressing this button (Internet access required)");
			GUILayout.EndHorizontal();
			
			GUILayout.Space(20);
			
			GUILayout.BeginHorizontal();
			if (GUILayout.Button("Activate new license", GUILayout.ExpandHeight(true), GUILayout.Width(buttonWidth)))
			{
				ActivateNewLicense();
				win.Close();
			}
			GUILayout.Label ("Activate Unity with a different serial number, switch to a free serial or start "
				+ "a trial period if you are eligable for it (Internet access required).");
			GUILayout.EndHorizontal();
			
			GUILayout.Space(20);

			GUILayout.BeginHorizontal();
			if (GUILayout.Button("Return license", GUILayout.ExpandHeight(true), GUILayout.Width(buttonWidth)))
				ReturnLicense();
			GUILayout.Label("Return this license and free an activation for the serial it is using. You can then"
				+ " reuse the activation on another machine (Internet access required).");
			GUILayout.EndHorizontal();

			GUILayout.Space(20);

			GUILayout.BeginHorizontal();
			if (GUILayout.Button("Manual activation", GUILayout.ExpandHeight(true), GUILayout.Width(buttonWidth)))
				ManualActivation();
			GUILayout.Label("Start the manual activation process, you can save this machines license activation "
				+ "request file or deploy a license file you have already activated manually.");
			GUILayout.EndHorizontal();

			GUILayout.FlexibleSpace();
			GUILayout.EndArea();

		}
	}
}
