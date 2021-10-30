using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor
{
	internal class ScreenShots
	{
		[MenuItem ("Window/Screenshot/Set Window Size %&l", false, 1000, true)]
		public static void SetMainWindowSize ()
		{
			MainWindow main = Resources.FindObjectsOfTypeAll(typeof(MainWindow))[0] as MainWindow;
			main.window.position = new Rect (0,0,1024,768);
		}
		
		[MenuItem ("Window/Screenshot/Set Window Size Small", false, 1000, true)]
		public static void SetMainWindowSizeSmall ()
		{
			MainWindow main = Resources.FindObjectsOfTypeAll(typeof(MainWindow))[0] as MainWindow;
			main.window.position = new Rect (0,0,800-38,600);
		}
		
		[MenuItem ("Window/Screenshot/Snap View %&j", false, 1000, true)]
		public static void Screenshot ()
		{
			GUIView v = GetMouseOverView();
			if (v != null)
			{
				Rect r = v.screenPosition;
				SaveScreenShot (r);
			}
		}
		
		[MenuItem ("Window/Screenshot/Snap View Toolbar", false, 1000, true)]
		public static void ScreenshotToolbar ()
		{
			GUIView v = GetMouseOverView();
			if (v != null)
			{
				Rect r = v.screenPosition;
				r.y += 19;
				r.height = 16;
				r.width -= 2;
				SaveScreenShotWithBorder (r);
			}
		}
		
		[MenuItem ("Window/Screenshot/Snap View Extended Right %&k", false, 1000, true)]
		public static void ScreenshotExtendedRight ()
		{
			GUIView v = GetMouseOverView();
			if (v != null)
			{
				MainWindow main = Resources.FindObjectsOfTypeAll(typeof(MainWindow))[0] as MainWindow;
				Rect r = v.screenPosition;
				r.xMax = main.window.position.xMax;
				SaveScreenShot (r);
			}
		}
		
		[MenuItem ("Window/Screenshot/Toggle DeveloperBuild", false, 1000, true)]
		public static void ToggleFakeNonDeveloperBuild ()
		{
			Unsupported.fakeNonDeveloperBuild = !Unsupported.fakeNonDeveloperBuild;
			InternalEditorUtility.RequestScriptReload ();
			InternalEditorUtility.RepaintAllViews ();
		}
		
		static GUIView GetMouseOverView ()
		{
			GUIView v = GUIView.mouseOverView;
			if (v == null)
			{
				EditorApplication.Beep ();
				Debug.LogWarning("Could not take screenshot.");
			}
			return v;
		}
		
		static void SaveScreenShot (Rect r)
		{
			r.y -= 1;
			r.height += 2;
			SaveScreenShot ((int)r.width, (int)r.height, InternalEditorUtility.ReadScreenPixel (new Vector2 (r.x, r.y), (int)r.width, (int)r.height));
		}
		
		// Adds a gray border around the screenshot
		// Useful for e.g. toolbars because they don't have a nice border all the way round due to the tabs
		static void SaveScreenShotWithBorder (Rect r)
		{
			int w = (int)r.width;
			int h = (int)r.height;
			Color[] colors1 = InternalEditorUtility.ReadScreenPixel (new Vector2 (r.x, r.y), w, h);
			Color[] colors2 = new Color[(w+2) * (h+2)];
			for (int x = 0; x<w; x++)
			{
				for (int y = 0; y<h; y++)
				{
					colors2[x+1 + (w+2)*(y+1)] = colors1[x + w*y];
				}
			}
			Color border = new Color(0.54f, 0.54f, 0.54f, 1f);
			for (int x = 0; x<w+2; x++)
			{
				colors2[x] = border;
				colors2[x+(w+2)*(h+1)] = border;
			}
			for (int y = 0; y<h+2; y++)
			{
				colors2[y * (w+2)] = border;
				colors2[y * (w+2) + (w+1)] = border;
			}
			
			SaveScreenShot ((int)(r.width+2), (int)(r.height+2), colors2);
		}
		
		static void SaveScreenShot (int width, int height, Color[] pixels)
		{
			Texture2D t = new Texture2D (width, height);
			t.SetPixels (pixels, 0);
			t.Apply (true);
			
			byte[] bytes = t.EncodeToPNG ();
			Object.DestroyImmediate (t, true);
			int i = 0;
			while (System.IO.File.Exists (string.Format ("{0}/../../SavedScreen{1:000}.png", Application.dataPath, i)))
				i++;
			System.IO.File.WriteAllBytes(string.Format ("{0}/../../SavedScreen{1:000}.png", Application.dataPath, i), bytes);
			Debug.Log(string.Format ("Saved screenshot at {0}/../../SavedScreen{1:000}.png", Application.dataPath, i));
		}
	}
}
