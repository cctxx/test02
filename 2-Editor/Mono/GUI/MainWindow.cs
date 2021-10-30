using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor {

using System.Collections;

internal class MainWindow : View, ICleanuppable
{
	const float kStatusbarHeight = 20;
	const float kMinWidth = 711;

	protected override void SetPosition (Rect newPos)
	{
		base.SetPosition (newPos);
		if (children.Length == 0)
			return;
		Toolbar t = (Toolbar)children[0];
		children[0].position = new Rect (0, 0, newPos.width, t.CalcHeight ());
		if (children.Length > 2) {
			children[1].position = new Rect (0,  t.CalcHeight (), newPos.width, newPos.height - t.CalcHeight () - children[2].position.height);
			children[2].position = new Rect (0, newPos.height - children[2].position.height, newPos.width, children[2].position.height);
		} 
	}

	protected override void ChildrenMinMaxChanged()
	{
		if (children.Length == 3) 
		{
			Toolbar t = (Toolbar)children[0];
			SetMinMaxSizes (new Vector2 (kMinWidth, t.CalcHeight () + kStatusbarHeight + children[1].minSize.y), new Vector2 (10000, 10000));
		}
		base.ChildrenMinMaxChanged ();
	}

	public static void MakeMain () {
		//Set up default window size
        ContainerWindow cw = ScriptableObject.CreateInstance<ContainerWindow>();
		MainWindow main = ScriptableObject.CreateInstance<MainWindow>();
		main.SetMinMaxSizes (new Vector2 (kMinWidth, 300), new Vector2 (10000,10000));
		cw.mainView = main;

		Resolution res = InternalEditorUtility.GetDesktopResolution();
		int width = Mathf.Clamp (res.width*3/4, 800, 1400);
		int height = Mathf.Clamp (res.height*3/4, 600, 950);
		cw.position = new Rect (60, 20, width, height);
			
		cw.Show (ShowMode.MainWindow, true, true);
		cw.DisplayAllViews();
	}

	public void Cleanup ()
	{
		// If we only have one child left, this means all views have been dragged out. 
		// So we resize the window to be just the toolbar
		// On windows, this might need some special handling for the main menu
		if (children[1].children.Length == 0)
		{
			Rect r = window.position;
			Toolbar t = (Toolbar)children[0];
			r.height = t.CalcHeight () + kStatusbarHeight;
		}
	}
}

} //namespace
