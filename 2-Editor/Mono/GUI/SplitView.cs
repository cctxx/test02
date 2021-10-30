using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

namespace UnityEditor {


/// orders all children along an axis. Resizing, splitting, etc..
class SplitView : View, ICleanuppable, IDropArea {
	public bool vertical = false;
	public int controlID = 0;
	// Extra info for dropping.
	class ExtraDropInfo {
		public Rect dropRect;
		public int idx;
		public ExtraDropInfo (Rect _dropRect, int _idx)
		{
			dropRect = _dropRect;
			idx = _idx;
		}
	}

	SplitterState splitState = null;
	
	void SetupSplitter() {
		int[] actualSizes = new int[children.Length];
		int[] minSizes = new int[children.Length];
		//int[] maxSizes = new int[children.Length];

		for (int j = 0; j < children.Length; j++) 
		{
			View c = (View)children[j];
			actualSizes[j] = vertical ? (int)c.position.height : (int)c.position.width;
			minSizes[j] = (int) (vertical ? c.minSize.y : c.minSize.x);
			//maxSizes[j] = (int) (vertical ? c.maxSize.y : c.maxSize.x);
		}

		splitState = new SplitterState (actualSizes, minSizes, null);
		splitState.splitSize = 10;
	}

	void SetupRectsFromSplitter() {
		if (children.Length == 0) 
			return;
		
		int cursor = 0;

		int total = 0;
		foreach (int size in splitState.realSizes) {
			total += size;
		}
		float scale = 1;
		if (total > (vertical ? position.height : position.width))
			scale = (vertical ? position.height : position.width) / total;

		for (int i = 0; i < children.Length; i++)
		{
//			Rect r = children[i].position;
			int size = (int)Mathf.Round (splitState.realSizes[i] * scale);
			if (vertical)
				children[i].position = new Rect(0, cursor, position.width, size);
			else 
				children[i].position = new Rect(cursor, 0, size, position.height);

			cursor += size;
		}
	}

	// 2-part process: recalc children sizes bottomup, the reflow top-down
	static void RecalcMinMaxAndReflowAll (SplitView start) {
	
		// search upwards and find the topmost
		SplitView root = start, next = start;
		do {
			root = next;
			next = root.parent as SplitView;
		} while (next);
		
		RecalcMinMaxRecurse (root);
		ReflowRecurse (root);
	}

	static void RecalcMinMaxRecurse (SplitView node) {
		foreach (View i in node.children) {
			SplitView sv = i as SplitView;
			if (sv)
				RecalcMinMaxRecurse (sv);
		}
		node.ChildrenMinMaxChanged ();
	}

	static void ReflowRecurse (SplitView node) {
		node.Reflow ();
		foreach (View i in node.children) {
			SplitView sv = i as SplitView;
			if (sv)
				RecalcMinMaxRecurse (sv);
		}		
	}

	internal override void Reflow () {
//		base.Reflow ();

		SetupSplitter ();

		for (int k = 0; k < children.Length - 1; k++) 
			splitState.DoSplitter(k, k + 1, 0);
		splitState.RelativeToRealSizes(vertical ? (int)position.height : (int)position.width);
		SetupRectsFromSplitter();
	}
		
	void PlaceView (int i, float pos, float size) {
		float roundPos = Mathf.Round (pos);
		if (vertical) 
			children[i].position = new Rect (0, roundPos, position.width, Mathf.Round (pos + size) - roundPos);
		else 
			children[i].position = new Rect (roundPos, 0, Mathf.Round (pos + size) - roundPos, position.height);	
	}
	
	public override void AddChild (View child, int idx) {
		base.AddChild (child,idx);
		ChildrenMinMaxChanged ();
		splitState = null;
	}
	
	public void RemoveChildNice (View child) {
		if (children.Length != 1) {
			// Make neighbors to grow to take space
			int idx = IndexOfChild (child);
			float moveToPos = 0;
			if (idx == 0) 
				moveToPos = 0;
			else if (idx == children.Length - 1)
				moveToPos = 1;
			else
				moveToPos = .5f;
			
			moveToPos = vertical ? 
				Mathf.Lerp (child.position.yMin, child.position.yMax, moveToPos) :
				Mathf.Lerp (child.position.xMin, child.position.xMax, moveToPos);
			
			
			if (idx > 0) {
				View c = (View)children[idx - 1];
				Rect r = c.position;
				if (vertical)
					r.yMax = moveToPos;
				else 
					r.xMax = moveToPos;
				c.position = r;

				if (c is SplitView)
					((SplitView) c).Reflow ();
			}

			if (idx < children.Length - 1) {
				View c = (View)children[idx + 1];
				Rect r = c.position;
				if (vertical)
					c.position = new Rect (r.x, moveToPos, r.width, r.yMax - moveToPos);
				else 
					c.position = new Rect (moveToPos, r.y, r.xMax - moveToPos, r.height);

				if (c is SplitView)
					((SplitView) c).Reflow ();
			}
		} 
		RemoveChild (child);
	}
	
	public override void RemoveChild (View child) {
		splitState = null;
		base.RemoveChild (child);
	}
	
	DropInfo DoDropZone (int idx, Vector2 mousePos, Rect sourceRect, Rect previewRect)
	{
		if (!sourceRect.Contains (mousePos))
			return null;
			
		DropInfo di = new DropInfo(this);
		di.type = DropInfo.Type.Pane;
		di.userData = idx;
		di.rect = previewRect;
		return di;
	}

	DropInfo CheckRootWindowDropZones (Vector2 mouseScreenPosition) {
		DropInfo di = null;
		if (!(parent is SplitView) && !(children.Length == 1 && DockArea.s_IgnoreDockingForView == children[0])) {
			Rect sr = screenPosition;
			// We are the root splitview in this window
			// Attach to bottom of this window
			if (parent is MainWindow) {
				di = DoDropZone (-1, mouseScreenPosition, new Rect (sr.x, sr.yMax, sr.width, 100), new Rect (sr.x, sr.yMax - 200, sr.width, 200));
			} else {
				di = DoDropZone (-1, mouseScreenPosition, new Rect (sr.x, sr.yMax - 20, sr.width, 100), new Rect (sr.x, sr.yMax - 50, sr.width, 200));
			}
	
			if (di != null)
				return di;
	
			// Attach to left of this window
			di = DoDropZone (-2, mouseScreenPosition, new Rect (sr.x-30, sr.y, 50, sr.height), new Rect (sr.x-50, sr.y, 100, sr.height));
			if (di != null)
				return di;
	
			// Attach to right of this window
			di = DoDropZone (-3, mouseScreenPosition, new Rect (sr.xMax-20, sr.y, 50, sr.height), new Rect (sr.xMax-50, sr.y, 100, sr.height));
		}
		return di;
	}

	public DropInfo DragOver (EditorWindow w, Vector2 mouseScreenPosition) {
		DropInfo rootWindowDropZone = CheckRootWindowDropZones (mouseScreenPosition);
		if (rootWindowDropZone != null)
			return rootWindowDropZone;

		for (int i = 0; i < children.Length; i++) {
			View child = children[i];

			// skip so you can't dock a view to a subview of itself
			if (child == DockArea.s_IgnoreDockingForView)
				continue;
				
			// Skip if child is a splitview (it'll handle it's client rect itself)
			if (child is SplitView)
				continue;
			
			// Skip if the mouse is not within this child (excluding tab-dock area at top)
			Rect sr = child.screenPosition;
			
			// Build a bitfield of which area the mouse is inside: 1=left, 2=bottom, 4=right
			int insideBits = 0;
			float borderWidth = Mathf.Round (Mathf.Min (sr.width / 3, 300));
			float borderHeight = Mathf.Round (Mathf.Min (sr.height / 3, 300));

			if (new Rect (sr.x, sr.y + DockArea.kDockHeight, borderWidth, sr.height - DockArea.kDockHeight).Contains (mouseScreenPosition))
				insideBits |= 1;
			
			if (new Rect (sr.x, sr.yMax - borderHeight, sr.width, borderHeight).Contains (mouseScreenPosition))
				insideBits |= 2;
				
			if (new Rect (sr.xMax - borderWidth, sr.y + DockArea.kDockHeight, borderWidth, sr.height - DockArea.kDockHeight).Contains (mouseScreenPosition))
				insideBits |= 4;
				
			// Mouse can be in both bottom and one of the sides. In that case, we need to check which one it's closest to.
			if (insideBits == 3)  // bottom-left quadrant
			{
				Vector2 v1 = new Vector2 (sr.x, sr.yMax) - mouseScreenPosition;
				Vector2 v2 = new Vector2 (borderWidth, -borderHeight);
				if (v1.x * v2.y - v1.y * v2.x < 0)
					insideBits = 1;
				else
					insideBits = 2;
			} else if (insideBits == 6)  // bottom-right quadrant
			{
				Vector2 v1 = new Vector2 (sr.xMax, sr.yMax) - mouseScreenPosition;
				Vector2 v2 = new Vector2 (-borderWidth, -borderHeight);
				if (v1.x * v2.y - v1.y * v2.x < 0)
					insideBits = 2;
				else
					insideBits = 4;
			}
			
			float dropWidth = Mathf.Round (Mathf.Max (sr.width / 3, 100));
			float dropHeight = Mathf.Round (Mathf.Max (sr.height / 3, 100));

			if (vertical) {
				switch (insideBits)
				{
					case 1:				// Left side
					{
						DropInfo di = new DropInfo (this);
						di.userData = i + 1000;
						di.type = DropInfo.Type.Pane;
						di.rect = new Rect (sr.x, sr.y, dropWidth, sr.height);
						return di;
					}
					case 2:				// Bottom side
					{
						DropInfo di = new DropInfo (this);
						di.userData = i + 1;
						di.type = DropInfo.Type.Pane;
						di.rect = new Rect (sr.x, sr.yMax - dropHeight, sr.width, dropHeight);
						return di;					
					}
					case 4:				// Right side
					{
						DropInfo di = new DropInfo (this);
						di.userData = i + 2000;
						di.type = DropInfo.Type.Pane;
						di.rect = new Rect (sr.xMax - dropWidth, sr.y, dropWidth, sr.height);
						return di;
					}
				}
			} else {
				switch (insideBits)
				{
				case 1:				// Left side
				{
					DropInfo di = new DropInfo (this);
					di.userData = i;
					di.type = DropInfo.Type.Pane;
					di.rect = new Rect (sr.x, sr.y, dropWidth, sr.height);
					return di;
				}
				case 2:				// Bottom side
				{
					DropInfo di = new DropInfo (this);
					di.userData = i + 2000;
					di.type = DropInfo.Type.Pane;
					di.rect = new Rect (sr.x, sr.yMax - dropHeight, sr.width, dropHeight);
					return di;					
				}
				case 4:				// Right side
				{
					DropInfo di = new DropInfo (this);
					di.userData = i + 1;
					di.type = DropInfo.Type.Pane;
					di.rect = new Rect (sr.xMax - dropWidth, sr.y, dropWidth, sr.height);
					return di;
				}
				}
			}
		}		

		if (screenPosition.Contains (mouseScreenPosition) && !(parent is SplitView)) {
			return new DropInfo (null);
		}

		return null;
	}
	/// Notification so other views can respond to this.
	protected override void ChildrenMinMaxChanged() {
		Vector2 min = Vector2.zero, max = Vector2.zero;
		if (vertical) {
			foreach (View child in children) {
				min.x = Mathf.Max (child.minSize.x, min.x);
				max.x = Mathf.Max (child.maxSize.x, max.x);
				min.y += child.minSize.y;
				max.y += child.maxSize.y;
			}
		} else {
			foreach (View child in children) {
				min.x += child.minSize.x;
				max.x += child.maxSize.x;
				min.y = Mathf.Max (child.minSize.y, min.y);
				max.y = Mathf.Max (child.maxSize.y, max.y);
			}
		}
		splitState = null;
/*		if (min.x > position.width || min.y > position.height) {
			Reflow ();
		}
*/		
		SetMinMaxSizes (min, max);
	}
	
	public override string ToString () {
		return vertical ? "SplitView (vert)" : "SplitView (horiz)";
	}

	public bool PerformDrop (EditorWindow w, DropInfo di, Vector2 screenPos) {
		int idx = (int)di.userData;
		DockArea da = ScriptableObject.CreateInstance<DockArea>();
//		Debug.Log ("Dropping in " + idx);
		Rect dropRect = di.rect;
		if (idx == -1 || idx == -2 || idx == -3) {		// left or right of the window
			bool beginning = idx == -2;
			bool wantsVertical = idx == -1;
			
			splitState = null;
			if (vertical == wantsVertical || children.Length < 2) { // if we're horizontal, we'll just the new thingy into us
				vertical = wantsVertical;
				dropRect.x -= screenPosition.x;
				dropRect.y -= screenPosition.y;
				MakeRoomForRect (dropRect);
				AddChild (da, beginning ? 0 : children.Length);
				da.position = dropRect;
			} else {
				SplitView sw = ScriptableObject.CreateInstance<SplitView>();
				Rect r = position;
				sw.vertical = wantsVertical;
				sw.position = new Rect (r.x, r.y, r.width, r.height/* + dpz.dropRect.height*/);
				if (window.mainView == this)
					window.mainView = sw;
				else 
					parent.AddChild (sw, parent.IndexOfChild (this));
				sw.AddChild (this);
				this.position = new Rect (0,0,r.width, r.height);
			
				Rect localRect = dropRect;
				localRect.x -= screenPosition.x;
				localRect.y -= screenPosition.y;
				
				sw.MakeRoomForRect (localRect);
			
				da.position = localRect;
				sw.AddChild (da, beginning ? 0 : 1);
			}
		} else if (idx < 1000) {
			Rect localRect = dropRect;
			localRect.x -= screenPosition.x;
			localRect.y -= screenPosition.y;
			MakeRoomForRect (localRect);
			AddChild (da, idx);			
			da.position = localRect;
		} else {
			int srcIdx = idx % 1000;
			if (children.Length != 1) {
				SplitView sw = ScriptableObject.CreateInstance<SplitView>();
				sw.vertical = !vertical;
								
				Rect r = children[srcIdx].position;
				sw.AddChild (children[srcIdx]);
				AddChild (sw, srcIdx);
				sw.position = r;
				r.x = r.y = 0;
				sw.children[0].position = r;
				
				Rect localRect = dropRect;
				localRect.x -= sw.screenPosition.x;
				localRect.y -= sw.screenPosition.y;
				
				sw.MakeRoomForRect (localRect);
				sw.AddChild (da, idx < 2000 ? 0 : 1);
				da.position = localRect;
			} else {
				vertical = !vertical;
				Rect localRect = dropRect;
				localRect.x -= screenPosition.x;
				localRect.y -= screenPosition.y;
				MakeRoomForRect (localRect);
				AddChild (da, idx == 1000 ? 0 : 1);
				da.position = localRect;
			}			
		}
		DockArea.s_OriginalDragSource.RemoveTab (w);
		w.m_Parent = da;
		da.AddTab (w);		
		Reflow ();
		RecalcMinMaxAndReflowAll (this);
        da.MakeVistaDWMHappyDance ();
        return true;
	}
	
	static string PosVals (float[] posVals) {
		string s = "[";
		foreach (float p in posVals)
			s += "" + p + ", ";
		s += "]";
		return (s);
	}

	void MakeRoomForRect (Rect r) {
		Rect[] sources = new Rect[children.Length];
		for (int i = 0; i < sources.Length; i++) 
			sources[i] = children[i].position;
		
		CalcRoomForRect (sources, r);
//		string s = "Making room for " + r +" \nchildren {\n";
		for (int i = 0; i < sources.Length; i++)  {
//			s += "    " + sources[i] + " \n";
			children[i].position = sources[i];
		}
//		s+= "}";
//		Debug.Log (s);
	}
	
	void CalcRoomForRect (Rect[] sources, Rect r) {
		float start = vertical ? r.y : r.x;
		float end = start + (vertical ? r.height : r.width);
		float mid = (start + end) * .5f;

		// Find out where we should split
		int splitPos;
		for (splitPos = 0; splitPos < sources.Length; splitPos++) {
			float midPos = vertical ? 
				(sources[splitPos].y + sources[splitPos].height * .5f) : 
				(sources[splitPos].x + sources[splitPos].width * .5f);
			if (midPos > mid)
				break;
		}
		
		float p2 = start;
		for (int i = splitPos - 1; i >= 0; i--) {
			if (vertical) {
				sources[i].yMax = p2;
				if (sources[i].height < children[i].minSize.y)
					p2 = sources[i].yMin = sources[i].yMax - children[i].minSize.y;			
				else
					break;
			} else {
				sources[i].xMax = p2;
				if (sources[i].width < children[i].minSize.x) 
					p2 = sources[i].xMin = sources[i].xMax - children[i].minSize.x;
				else
					break;				
			}
		}
		// if we're below zero, move everything forward
		if (p2 < 0) {
			float delta = -p2;
			for (int i = 0; i < splitPos - 1; i++) {
				if (vertical)
					sources[i].y += delta;
				else 
					sources[i].x += delta;
			}
			end += delta;
		} 
		
		p2 = end;
		for (int i = splitPos; i < sources.Length; i++) {
			if (vertical) {
				float tmp = sources[i].yMax;
				sources[i].yMin = p2;
				sources[i].yMax = tmp;
				if (sources[i].height < children[i].minSize.y)
					p2 = sources[i].yMax = sources[i].yMin + children[i].minSize.y;			
				else
					break;
			} else {
				float tmp = sources[i].xMax;
				sources[i].xMin = p2;
				sources[i].xMax = tmp;
				if (sources[i].width < children[i].minSize.x) 
					p2 = sources[i].xMax = sources[i].xMin + children[i].minSize.x;
				else
					break;
			}
		}
		// if we're above max, move everything forward
		float limit = vertical ? position.height : position.width;
		if (p2 > limit) {
			float delta = limit - p2;
			for (int i = 0; i < splitPos - 1; i++) {
				if (vertical)
					sources[i].y += delta;
				else 
					sources[i].x += delta;
			}
			end += delta;
		}

		return;
	}

	/// clean up this view & propagate down
	public void Cleanup () {
		// if I'm a one-view splitview, I can propagate my child up and kill myself

		SplitView sp = parent as SplitView;

		if (children.Length == 1 && sp != null) {
			View c = children[0];
			c.position = position;
			if (parent != null) {
				parent.AddChild (c, parent.IndexOfChild (this));
				parent.RemoveChild (this);
				if (sp)
					sp.Cleanup ();
				c.position = position;
				if (!Unsupported.IsDestroyScriptableObject (this))
					DestroyImmediate (this);
				return;
			} else if (c is SplitView) {
				RemoveChild (c);
				window.mainView = c;
				c.position = new Rect (0,0, c.window.position.width, window.position.height);
				c.Reflow ();
				if (!Unsupported.IsDestroyScriptableObject (this))
					DestroyImmediate (this);
				return;
			}
		}

		if (sp) {

			sp.Cleanup ();
			// the parent might have moved US up and gotten rid of itself
			sp = parent as SplitView;
			if (sp) {
				// If the parent has the same orientation as us, we can move our views up and kill ourselves
				if (sp.vertical == vertical) {
					int idx = new List<View> (parent.children).IndexOf (this);
					foreach (View child in children) {
						sp.AddChild (child, idx++);
						child.position = new Rect (position.x + child.position.x, position.y + child.position.y, child.position.width, child.position.height);
					}
				}
			}	
		}
		if (children.Length == 0) {
		
			if (parent == null && window != null) {
				// if we're root in the window, we'll remove ourselves
				window.Close ();
			} else {
				ICleanuppable ic = parent as ICleanuppable;
				if (parent is SplitView) {
					((SplitView)parent).RemoveChildNice (this);
					if (!Unsupported.IsDestroyScriptableObject (this))
						DestroyImmediate (this, true);
				} else {
					// This is we're root in the main window.
					// We want to stay, but tell the parent (MainWindow) to Cleanup, so he can reduce us to zero-size
/*					parent.RemoveChild (this);*/
				}
				ic.Cleanup ();
			}
			return;
		} else {
			splitState = null;
			Reflow ();
		}
	}
	
	static float[] s_StartDragPos, s_DragPos;
	internal const float kGrabDist = 5;
	public void SplitGUI (Event evt) {
	
		if (splitState == null)
			SetupSplitter();
	
		SplitView sp = parent as SplitView;
		if (sp) {
			Event e = new Event (evt);
			e.mousePosition += new Vector2 (position.x, position.y);
			sp.SplitGUI (e);
			if (e.type == EventType.Used)
				evt.Use();
		}
		
		float pos = vertical ? evt.mousePosition.y : evt.mousePosition.x;
		int id = GUIUtility.GetControlID (546739, FocusType.Passive);
		controlID = id;

		switch (evt.GetTypeForControl (id)) {
		case EventType.MouseDown:
			if (children.Length != 1) // is there a splitter
			{
				int cursor = vertical ? (int)children[0].position.y : (int)children[0].position.x;

				for (int i = 0; i < children.Length - 1; i++)
				{
					if (i >= splitState.realSizes.Length)
					{
						DockArea dock = GUIView.current as DockArea;
						string name = "Non-dock area " + GUIView.current.GetType();
						if (dock && dock.m_Selected < dock.m_Panes.Count && dock.m_Panes[dock.m_Selected])
							name = dock.m_Panes[dock.m_Selected].GetType().ToString();
						
						if (Unsupported.IsDeveloperBuild())
							Debug.LogError("Real sizes out of bounds for: " + name + " index: " + i + " RealSizes: " + splitState.realSizes.Length);

						SetupSplitter();
					}
					Rect splitterRect = vertical ? 
						new Rect(children[0].position.x, cursor + splitState.realSizes[i] - splitState.splitSize / 2, children[0].position.width, splitState.splitSize) :
						new Rect(cursor + splitState.realSizes[i] - splitState.splitSize / 2, children[0].position.y, splitState.splitSize, children[0].position.height);

					if (splitterRect.Contains (evt.mousePosition))
					{
						splitState.splitterInitialOffset = (int)pos;
						splitState.currentActiveSplitter = i;
						GUIUtility.hotControl = id;
						evt.Use();
						break;
					}

					cursor += splitState.realSizes[i];
				}
			}
			break;
		case EventType.MouseDrag:
				if (children.Length > 1 && (GUIUtility.hotControl == id) && (splitState.currentActiveSplitter >= 0))
				{
					int diff = (int)pos - splitState.splitterInitialOffset;

					if (diff != 0)
					{
						splitState.splitterInitialOffset = (int)pos;
						splitState.DoSplitter(splitState.currentActiveSplitter, splitState.currentActiveSplitter + 1, diff);
					}

					SetupRectsFromSplitter();
					

					evt.Use ();
				}
			break;
			
		case EventType.MouseUp:
			if (GUIUtility.hotControl == id)
				GUIUtility.hotControl = 0;
			break;
		}
	}
	
	protected override void SetPosition (Rect newPos) {
		base.SetPosition (newPos);
		Reflow ();
	}
}
	
} // namespace
