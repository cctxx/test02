using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditor
{
	internal interface TimeUpdater
	{
		void UpdateTime (float time);
	}

	internal class CurveWrapper
	{
		public delegate Vector2 GetAxisScalarsCallback ();
		public delegate void SetAxisScalarsCallback (Vector2 newAxisScalars);

		public CurveWrapper ()
		{
			id = 0;
			groupId = -1;
			regionId = -1;
			hidden = false;
			readOnly = false;
			listIndex = -1;
			getAxisUiScalarsCallback = null;
			setAxisUiScalarsCallback = null;
		}

		internal enum SelectionMode
		{
			None = 0,
			Selected = 1,
			SemiSelected = 2
		}
		
		// Curve management
		private CurveRenderer m_Renderer;
		public CurveRenderer renderer { get { return m_Renderer; } set { m_Renderer = value; } }
		public AnimationCurve curve { get { return renderer.GetCurve(); } }
		
		// Input - should not be changed by curve editor
		public int id;
		public int groupId;
		public int regionId;									// Regions are defined by two curves added after each other with the same regionId.
		public Color color;
		public bool readOnly;
		public bool hidden;
		public GetAxisScalarsCallback getAxisUiScalarsCallback;	// Delegate used to fetch values that are multiplied on x and y axis ui values
		public SetAxisScalarsCallback setAxisUiScalarsCallback;	// Delegate used to set values back that has been changed by this curve editor
		
		// Should be updated by curve editor as appropriate
		public bool changed;
		public SelectionMode selected;
		public int listIndex;										// Index into m_AnimationCurves list
		
		// An additional vertical min / max range clamp when editing multiple curves with different ranges
		public float vRangeMin = -Mathf.Infinity;
		public float vRangeMax = Mathf.Infinity;

	}

	[System.Serializable]
	internal class CurveSelection : System.IComparable
	{
		internal enum SelectionType
		{
			Key = 0,
			InTangent = 1,
			OutTangent = 2,
			Count = 3,
		}
		
		[System.NonSerialized]
		internal CurveEditor m_Host = null;
		
		private int m_CurveID = 0;
		private int m_Key = -1;
		internal bool semiSelected = false;
		internal SelectionType type;
	
		internal CurveWrapper curveWrapper { get { return m_Host.GetCurveFromID (m_CurveID); } }
		internal AnimationCurve curve { get { return curveWrapper.curve; } }
		public int curveID { get { return m_CurveID; } set { m_CurveID = value; } }
		public int key { get { return m_Key; } set { m_Key = value; } }
		
		internal bool validKey()
		{
			return (curve!=null && m_Key>=0 && m_Key<curve.length);
		}
		internal Keyframe keyframe
		{
			get
			{
				if (validKey()) return curve[m_Key];
				return new Keyframe();
			}
		}
		internal CurveSelection (int curveID, CurveEditor host, int keyIndex)
		{
			m_CurveID = curveID;
			m_Host = host;
			m_Key = keyIndex;
			type = SelectionType.Key;
		}
		
		internal CurveSelection (int curveID, CurveEditor host, int keyIndex, SelectionType t)
		{
			m_CurveID = curveID;
			m_Host = host;
			m_Key = keyIndex;
			type = t;
		}
		
		public int CompareTo(object _other)
		{
	        CurveSelection other = (CurveSelection)_other;
	        int cmp = curveID - other.curveID;
			if (cmp != 0) 
				return cmp;	
			
			cmp = key - other.key;
			if (cmp != 0) 
				return cmp;	
	
			return (int)type - (int)other.type;
	    }
		
		public override bool Equals (object _other)
		{
			CurveSelection other = (CurveSelection)_other;
	        return other.curveID == curveID && other.key == key && other.type == type;
		}
		                            
		public override int GetHashCode ()
		{
			return curveID *729 + key * 27 + (int)type;
		}
	}

	[System.Serializable]
	internal class CurveEditor : TimeArea, CurveUpdater
	{
		[System.NonSerialized]
		CurveWrapper[] m_AnimationCurves;
	
		public bool hasSelection { get { return m_Selection.Count != 0; } }
		
		static int s_SelectKeyHash = "SelectKeys".GetHashCode();
		
		public delegate void CallbackFunction ();
		
		public CallbackFunction curvesUpdated;

		public CurveWrapper[] animationCurves { 
			get {
				if(m_AnimationCurves == null)
					m_AnimationCurves = new CurveWrapper[0];

				return m_AnimationCurves;
			}
			set { 
				m_AnimationCurves = value;
				for (int i = 0; i < m_AnimationCurves.Length; ++i)
					m_AnimationCurves[i].listIndex = i;
				SyncDrawOrder ();
				SyncSelection();
				ValidateCurveList();
			}
		}

		public bool GetTopMostCurveID (out int curveID)
		{
			if (m_DrawOrder.Count > 0)
			{
				curveID = m_DrawOrder [m_DrawOrder.Count - 1];
				return true;
			}

			curveID = -1;	
			return false;
		}

		private List<int> m_DrawOrder = new List<int>(); // contains curveIds (last element topmost)
		void SyncDrawOrder()
		{
			// Init
			if (m_DrawOrder.Count == 0)
			{
				m_DrawOrder = m_AnimationCurves.Select(cw => cw.id).ToList();
				return;
			}

			List<int> newDrawOrder = new List<int>(m_AnimationCurves.Length);
			// First add known curveIds (same order as before)
			for (int i = 0; i < m_DrawOrder.Count; ++i)
			{
				int curveID = m_DrawOrder[i];
				for (int j = 0; j < m_AnimationCurves.Length; ++j)
				{
					if (m_AnimationCurves[j].id == curveID)
					{
						newDrawOrder.Add(curveID);
						break;
					}
				}
			}
			m_DrawOrder = newDrawOrder;

			// Found them all
			if (m_DrawOrder.Count == m_AnimationCurves.Length)
				return; 

			// Add nonexisting curveIds (new curves are top most)
			for (int i = 0; i < m_AnimationCurves.Length; ++i)
			{
				int curveID = m_AnimationCurves[i].id;
				bool found = false;
				for (int j = 0; j < m_DrawOrder.Count; ++j)
				{
					if (m_DrawOrder[j] == curveID)
					{
						found = true;
						break;
					}
				}
				if (!found)
					m_DrawOrder.Add(curveID);
			}

			// Fallback if invalid setup with multiple curves with same curveID (see case 482048)
			if (m_DrawOrder.Count != m_AnimationCurves.Length)
			{
				// Ordering can fail if we have a hierarchy like:
				//
				// Piston
				//		Cylinder
				// 			InnerCyl
				// 		Cylinder
				// 			InnerCyl
				// Since we cannot generate unique curve ids for identical paths like Cylinder and InnerCyl.
				m_DrawOrder = m_AnimationCurves.Select(cw => cw.id).ToList();
			}
		}


		public CurveWrapper getCurveWrapperById (int id)
		{
			foreach(CurveWrapper cw in m_AnimationCurves)
				if (cw.id == id)
					return cw;
			return null;
		}
		
		internal TimeUpdater m_TimeUpdater;
		
		public float activeTime { set
		{
			if (m_TimeUpdater != null)
				m_TimeUpdater.UpdateTime(value);
		} }
		
		internal Bounds m_DefaultBounds = new Bounds(new Vector3(0.5f,0.5f,0), new Vector3(1, 1, 0));

		private void ApplySettings () {
			hRangeLocked = settings.hRangeLocked;
			vRangeLocked = settings.vRangeLocked;
			hRangeMin = settings.hRangeMin;
			hRangeMax = settings.hRangeMax;
			vRangeMin = settings.vRangeMin;
			vRangeMax = settings.vRangeMax;
			scaleWithWindow = settings.scaleWithWindow;
			hSlider = settings.hSlider;
			vSlider = settings.vSlider;
			RecalculateBounds();
		}
		
/*		private TickHandler m_HTicks;
		public TickHandler hTicks { get { return m_HTicks; } set { m_HTicks = value; } }
		private TickHandler m_VTicks;
		public TickHandler vTicks { get { return m_VTicks; } set { m_VTicks = value; } }
*/		
		// Other style settings
		private Color m_TangentColor = new Color (1,1,1, 0.5f);
		public Color tangentColor { get { return m_TangentColor; } set { m_TangentColor = value; } }
			
		/// 1/time to snap all keyframes to while dragging. Set to 0 for no snap (default)
		public float invSnap = 0;
		
		private CurveMenuManager m_MenuManager;
		
		static int s_TangentControlIDHash = "s_TangentControlIDHash".GetHashCode ();
		
		///  Array of selected points
		List<CurveSelection> m_Selection = new List<CurveSelection> ();
		internal List<CurveSelection> selectedCurves
		{
			get { return m_Selection; }
		}

		///  Do not serialize DisplayedSelection since it must be null after script reloads
		///  and the serialization system makes it be not null
		[System.NonSerialized] private List<CurveSelection> m_DisplayedSelection;
		
		internal void ClearDisplayedSelection () { m_DisplayedSelection = null; }
		
		// Array of tangent points that have been revealed
		CurveSelection m_SelectedTangentPoint;

		// Selection tracking:
		// What the selection was at the start of a drag
		List<CurveSelection> s_SelectionBackup;
		// Time range selection, is it active and what was the mousedown time (start) and the current end time.
		float s_TimeRangeSelectionStart, s_TimeRangeSelectionEnd;
		bool s_TimeRangeSelectionActive = false;
		
		Bounds m_Bounds = new Bounds(Vector3.zero, Vector3.zero);
		public override Bounds drawingBounds { get { return m_Bounds; } }

		// Helpers for temporarily saving a bunch of keys.
		class SavedCurve
		{
			public class SavedKeyFrame : System.IComparable
			{
				public Keyframe key;
				public CurveWrapper.SelectionMode selected;
				
				public SavedKeyFrame (Keyframe key, CurveWrapper.SelectionMode selected)
				{
					this.key = key;
					this.selected = selected;
				}
					
				public int CompareTo(object _other)
				{
			        SavedKeyFrame other = (SavedKeyFrame)_other;
					
					float cmp = key.time - other.key.time;
					return cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);
			    }
	
			}
			public int curveId;
			public List<SavedKeyFrame> keys;
		}
		List<SavedCurve> m_CurveBackups;
		
		CurveWrapper m_DraggingKey = null;
		Vector2 m_DraggedCoord;
		Vector2 m_MoveCoord;
		
		// Used to avoid drawing points too close to each other.
		private Vector2 m_PreviousDrawPointCenter;

		// The square of the maximum pick distance in pixels.
		// The mouse will select a key if it's within this distance from the key point.
		const float kMaxPickDistSqr = 8 * 8;
		const float kExactPickDistSqr = 4 * 4;
		
		const float kCurveTimeEpsilon = 0.00001f;

		public CurveEditor (Rect rect, CurveWrapper[] curves, bool minimalGUI) : base (minimalGUI)
		{
			this.rect = rect;
			animationCurves = curves;
			
			float[] modulos = new float[]{
				0.0000001f, 0.0000005f, 0.000001f, 0.000005f, 0.00001f, 0.00005f, 0.0001f, 0.0005f,
				0.001f, 0.005f, 0.01f, 0.05f, 0.1f, 0.5f, 1, 5, 10, 50, 100, 500,
				1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000, 10000000
			};
			hTicks = new TickHandler();
			hTicks.SetTickModulos(modulos);
			vTicks = new TickHandler();
			vTicks.SetTickModulos(modulos);
			margin = 40.0f;
			
			Undo.undoRedoPerformed += UndoRedoPerformed;
		}
		
		public void OnDisable ()
		{
			Undo.undoRedoPerformed -= UndoRedoPerformed;
		}
		
		void UndoRedoPerformed ()
		{
			SelectNone();
		}

		private void ValidateCurveList ()
		{
			// Validate that regions are valid (they should consist of two curves after each other with same regionId)
			for (int i = 0; i < m_AnimationCurves.Length; ++i)
			{
				CurveWrapper cw = m_AnimationCurves[i];
				int regId1 = cw.regionId;
				if (regId1 >= 0)
				{
					if (i == m_AnimationCurves.Length - 1)
					{
						Debug.LogError("Region has only one curve last! Regions should be added as two curves after each other with same regionId");
						return;
					}

					CurveWrapper cw2 = m_AnimationCurves[++i];
					int regId2 = cw2.regionId;
					if (regId1 != regId2)
					{
						Debug.LogError("Regions should be added as two curves after each other with same regionId: " + regId1 + " != " + regId2);
						return;
					}
				}
			}

			if (m_DrawOrder.Count != m_AnimationCurves.Length)
			{
				Debug.LogError ("DrawOrder and AnimationCurves mismatch: DrawOrder " + m_DrawOrder.Count + ", AnimationCurves: " + m_AnimationCurves.Length);
				return;
			}

			// Validate draw order regions			
			int numCurves = m_DrawOrder.Count;
			for (int i = 0; i < numCurves; ++i)
			{
				int curveID = m_DrawOrder[i];
				// If curve is part of a region then find other curve
				int regionId = getCurveWrapperById(curveID).regionId;
				if (regionId >= 0)
				{
					if (i == numCurves -1)
					{
						Debug.LogError("Region has only one curve last! Regions should be added as two curves after each other with same regionId");
						return;
					}
					
					// Ensure next curve has a matching regionId
					int curveId2 = m_DrawOrder[++i];
					int regionId2 = getCurveWrapperById(curveId2).regionId;
					if (regionId != regionId2)
					{
						Debug.LogError ("DrawOrder: Regions not added correctly after each other. RegionIds: " + regionId + " , " + regionId2);
						return;
					}
				}
			}

			// Debug.Log all curves and their state (outcomment if needed)
			/*
			string info = "Count: " + m_AnimationCurves.Length + " (Click me for more info)\n";
			foreach (CurveWrapper cw in m_AnimationCurves)
				info += ("Curve: id " + cw.id + ", regionId " + cw.regionId + ", hidden " + cw.hidden + "\n");
			Debug.Log(info);
			*/
		}		


		void UpdateTangentsFromSelection () 
		{
			foreach (CurveSelection cs in m_Selection) 
			{
				CurveUtility.UpdateTangentsFromModeSurrounding (cs.curveWrapper.curve, cs.key);
			}
		}
		
		private void SyncSelection ()
		{
			Init ();
			
			List<CurveSelection> newSelection = new List<CurveSelection> (m_Selection.Count);
	
			foreach (CurveSelection cs in m_Selection)
			{
				CurveWrapper cw = cs.curveWrapper;
				if (cw != null && (!cw.hidden || cw.groupId != -1))
				{
					cw.selected = CurveWrapper.SelectionMode.Selected;
					newSelection.Add (cs);
				}		
			}
			m_Selection = newSelection;
			
			RecalculateBounds();
		}
		
		public void RecalculateBounds ()
		{
			const float kMinRange = 0.1F;

			m_Bounds = m_DefaultBounds;
			
			// If any of the four sides are unbounded, calculate bounds based on curve bounds
			if (animationCurves != null &&
				(hRangeMin == Mathf.NegativeInfinity ||
				hRangeMax == Mathf.Infinity ||
				vRangeMin == Mathf.NegativeInfinity ||
				vRangeMax == Mathf.Infinity))
			{
				bool assigned = false;
				foreach (CurveWrapper wrapper in animationCurves)
				{
					if (wrapper.hidden)
						continue;
					if (wrapper.curve.length == 0)
						continue;
					if (!assigned)
					{
						m_Bounds = wrapper.renderer.GetBounds();
						assigned = true;
					}
					else
						m_Bounds.Encapsulate(wrapper.renderer.GetBounds());
				}
			}
			
			// For sides that have a set bound, overwrite with that one
			if (hRangeMin != Mathf.NegativeInfinity)
				m_Bounds.min = new Vector3(hRangeMin, m_Bounds.min.y, m_Bounds.min.z);
			if (hRangeMax != Mathf.Infinity)
				m_Bounds.max = new Vector3(hRangeMax, m_Bounds.max.y, m_Bounds.max.z);
			if (vRangeMin != Mathf.NegativeInfinity)
				m_Bounds.min = new Vector3(m_Bounds.min.x, vRangeMin, m_Bounds.min.z);
			if (vRangeMax != Mathf.Infinity)
				m_Bounds.max = new Vector3(m_Bounds.max.y, vRangeMax, m_Bounds.max.z);
			
			// Enforce minimum size of bounds
			m_Bounds.size = new Vector3(Mathf.Max(m_Bounds.size.x, kMinRange), Mathf.Max(m_Bounds.size.y, kMinRange), 0);
		}

		// Frame Selects the curve to be completely visible
		public void FrameSelected (bool horizontally, bool vertically)
		{
			// Initiate bounds from first keyframe in first curve
			Bounds frameBounds = new Bounds();
			
			if (!hasSelection)
			{
				frameBounds = drawingBounds;
				if (frameBounds.size == Vector3.zero)
					return;
			}
			else
			{
				frameBounds = new Bounds (new Vector2 (m_Selection[0].keyframe.time, m_Selection[0].keyframe.value), Vector2.zero);
						                 
				foreach (CurveSelection cs in m_Selection)
				{
					// Encapsulate key in bounds
					frameBounds.Encapsulate (new Vector2 (cs.curve[cs.key].time, cs.curve[cs.key].value));
					
					// Include neighboring keys in bounds
					if (cs.key-1 >= 0)
						frameBounds.Encapsulate (new Vector2 (cs.curve[cs.key-1].time, cs.curve[cs.key-1].value));
					if (cs.key+1 < cs.curve.length)
						frameBounds.Encapsulate (new Vector2 (cs.curve[cs.key+1].time, cs.curve[cs.key+1].value));
				}
				
				// Enforce minimum size of bounds
				frameBounds.size = new Vector3(Mathf.Max(frameBounds.size.x, 0.1F), Mathf.Max(frameBounds.size.y, 0.1F), 0);
			}
			
			if (horizontally)
				SetShownHRangeInsideMargins(frameBounds.min.x, frameBounds.max.x);
			if (vertically)
				SetShownVRangeInsideMargins(frameBounds.min.y, frameBounds.max.y);
		}
		
		public void UpdateCurves (List<int> curveIds, string undoText)
		{
			foreach (int id in curveIds)
			{
				CurveWrapper cw = GetCurveFromID(id);
				cw.changed = true;
			}
			if (curvesUpdated != null)
				curvesUpdated();
		}
		
		internal CurveWrapper GetCurveFromID (int curveID)
		{
			// TODO: Optimize using hashtable
			if (m_AnimationCurves == null) return null;
			foreach (CurveWrapper wrapper in m_AnimationCurves)
			{
				if (wrapper.id == curveID) return wrapper;
			}
			return null;
		}
			
		void Init () 
		{
			// Hook up selection to know we're the owner if they lost it (due to assembly reloading)
			if (m_Selection != null && hasSelection && m_Selection[0].m_Host == null) 
			{
				foreach (CurveSelection cs in m_Selection)
					cs.m_Host = this;
			}
		}	
	
		internal void InitStyles () 
		{
			if (ms_Styles == null)
				ms_Styles = new Styles ();	
		}
		
		public void OnGUI ()
		{
			BeginViewGUI();
			GridGUI();
			CurveGUI();
			EndViewGUI();
		}
		
		public void CurveGUI ()
		{
			InitStyles ();
				
			GUI.BeginGroup(drawRect);
			
			Init ();
			GUIUtility.GetControlID (s_SelectKeyHash, FocusType.Passive);
			GUI.contentColor = GUI.backgroundColor = Color.white;
	
			Color oldColor = GUI.color;
			
			Event evt = Event.current;
			switch (evt.type) 
			{
			case EventType.ValidateCommand:
			case EventType.ExecuteCommand:
				bool execute = evt.type == EventType.ExecuteCommand;
				switch (evt.commandName)
				{
				case "Delete":
					if (hasSelection) 
					{
						if (execute)
						{
							DeleteSelectedPoints ();
						}
						evt.Use ();
					}
					break;
				case "FrameSelected":
					if (execute)
						FrameSelected(true, true);
					evt.Use ();
					break;
				case "SelectAll":
					if (execute)
						SelectAll ();
					evt.Use ();
					break;
				}
				break;
			case EventType.KeyDown:
				if ((evt.keyCode == KeyCode.Backspace || evt.keyCode == KeyCode.Delete) && hasSelection) 
				{
					DeleteSelectedPoints ();
					evt.Use ();
				}
				break;
	
			case EventType.ContextClick:
				CurveSelection mouseKey = FindNearest ();
				if (mouseKey != null)
				{
					List<KeyIdentifier> keyList = new List<KeyIdentifier> ();

					// Find out if key under mouse is part of selected keys
					bool inSelected = false;
					foreach (CurveSelection sel in m_Selection)
					{
						keyList.Add (new KeyIdentifier (sel.curveWrapper.renderer, sel.curveID, sel.key));
						if (sel.curveID == mouseKey.curveID && sel.key == mouseKey.key)
							inSelected = true;
					}
					if (!inSelected)
					{
						keyList.Clear ();
						keyList.Add (new KeyIdentifier (mouseKey.curveWrapper.renderer, mouseKey.curveID, mouseKey.key));
						m_Selection.Clear ();
						m_Selection.Add (mouseKey);
					}

					m_MenuManager = new CurveMenuManager (this);
					GenericMenu menu = new GenericMenu ();

					string str;
					if (keyList.Count > 1)
						str = "Delete Keys";
					else
						str = "Delete Key";
					menu.AddItem (new GUIContent (str), false, DeleteKeys, keyList);
					menu.AddSeparator ("");

					m_MenuManager.AddTangentMenuItems (menu, keyList);
					menu.ShowAsContext ();
					Event.current.Use ();
					
				}
				break;
	
			case EventType.Repaint:
				DrawCurves (animationCurves);
				break;
			}
			bool oldChanged = GUI.changed;
			GUI.changed = false;
			GUI.color = oldColor;
			DragTangents ();
			EditAxisLabels();
			SelectPoints ();
			if (GUI.changed)
			{
				RecalcSecondarySelection ();
				RecalcCurveSelection ();
			}
	
			GUI.changed = false;
			Vector2 move = MovePoints ();
			if (GUI.changed && m_DraggingKey != null)
			{
				activeTime = move.x + s_StartClickedTime;
				m_MoveCoord = move;
			}
			
			GUI.changed = oldChanged;

			GUI.color = oldColor;
			
			GUI.EndGroup();
		}
		
		// Recalculate curve.selected from m_Selection
		void RecalcCurveSelection ()
		{
			// Reset selection state of all curves
			foreach (CurveWrapper cw in m_AnimationCurves)
				cw.selected = CurveWrapper.SelectionMode.None;

			// Now sync with m_Selection
			foreach (CurveSelection cs in m_Selection)
				cs.curveWrapper.selected = cs.semiSelected ? CurveWrapper.SelectionMode.SemiSelected : CurveWrapper.SelectionMode.Selected;
		}
		
		void RecalcSecondarySelection () {
			// The new list of secondary selections
			List<CurveSelection> newSelection = new List<CurveSelection> ();
			
			// Go through selection, find curveselections that need syncing, add those for the sync points.
			foreach (CurveSelection cs in m_Selection) {
				CurveWrapper cw = cs.curveWrapper;
				int groupId = cs.curveWrapper.groupId;
				if (groupId != -1 && !cs.semiSelected) {
					newSelection.Add (cs);
					foreach (CurveWrapper cw2 in m_AnimationCurves) 
					{
						if (cw2.groupId == groupId && cw2 != cw) 
						{
							CurveSelection newCS = new CurveSelection (cw2.id, this, cs.key);
							newCS.semiSelected = true;
							newSelection.Add (newCS);
						}
					}
				} else {
					newSelection.Add (cs);	
				}
			}
			newSelection.Sort ();

			// the selection can contain duplicate keys. We go through the selection and remove any duplicates we find. 
			// Since the selection is already sorted, the duplicates are next to each other.
			for (int i = 0; i < newSelection.Count - 1;)
			{
				CurveSelection cs1 = newSelection[i];
				CurveSelection cs2 = newSelection[i + 1];
				if (cs1.curveID == cs2.curveID && cs1.key == cs2.key)
				{
					// If we have a collision, one can be fully selected, while the other can be semiselected. Make sure we always get the most selected one.
					if (!cs1.semiSelected || !cs2.semiSelected)
						cs1.semiSelected = false;
					newSelection.RemoveAt (i + 1);
				} else {
					i++;	
				}
			}
			
			// Assign back
			m_Selection = newSelection;
		}
			
		void DragTangents () 
		{
			Event evt = Event.current;
			int tangentId = GUIUtility.GetControlID (s_TangentControlIDHash, FocusType.Passive);
			switch (evt.GetTypeForControl (tangentId)) {
			case EventType.MouseDown:
				if (evt.button == 0 && !evt.alt) 
				{
					m_SelectedTangentPoint = null;
					float nearestDist = kMaxPickDistSqr;
					Vector2 mousePos = Event.current.mousePosition;
					foreach (CurveSelection cs in m_Selection) 
					{
						Keyframe key = cs.keyframe;
						if (CurveUtility.GetKeyTangentMode(key, 0) == TangentMode.Editable) 
						{
							CurveSelection tangent = new CurveSelection(cs.curveID, this, cs.key, CurveSelection.SelectionType.InTangent);
							float d = (DrawingToViewTransformPoint(GetPosition (tangent)) - mousePos).sqrMagnitude;
							if (d <= nearestDist)
							{
								m_SelectedTangentPoint = tangent;
								nearestDist = d;
							}
						}
						
						if (CurveUtility.GetKeyTangentMode(key, 1) == TangentMode.Editable)
						{
							CurveSelection tangent = new CurveSelection(cs.curveID, this, cs.key, CurveSelection.SelectionType.OutTangent);
							float d = (DrawingToViewTransformPoint(GetPosition (tangent)) - mousePos).sqrMagnitude;
							if (d <= nearestDist)
							{
								m_SelectedTangentPoint = tangent;
								nearestDist = d;
							}
						}
						
					}
		
					if (m_SelectedTangentPoint != null) 
					{
						GUIUtility.hotControl = tangentId;
						activeTime = m_SelectedTangentPoint.keyframe.time;
						evt.Use ();
					}
				}
				break;
	
			case EventType.MouseDrag:
				if (GUIUtility.hotControl == tangentId)
				{
					//Debug.Log ("Dragging tangent: " + tangentId);	
					Vector2 newPosition = mousePositionInDrawing;// + m_MouseDownOffset;
					CurveSelection dragged = m_SelectedTangentPoint;
					Keyframe key = dragged.keyframe;
	
					if (dragged.type == CurveSelection.SelectionType.InTangent)
					{
						Vector2 tangentDirection = newPosition - new Vector2 (key.time, key.value);
						if (tangentDirection.x < -0.0001F)
							key.inTangent = tangentDirection.y / tangentDirection.x;
						else
							key.inTangent = Mathf.Infinity;
						CurveUtility.SetKeyTangentMode(ref key, 0, TangentMode.Editable);
						
						if (!CurveUtility.GetKeyBroken(key))
						{
							key.outTangent = key.inTangent;
							CurveUtility.SetKeyTangentMode(ref key, 1, TangentMode.Editable);
						}
					}
					else if (dragged.type == CurveSelection.SelectionType.OutTangent)
					{
						Vector2 tangentDirection = newPosition - new Vector2 (key.time, key.value);
						if (tangentDirection.x > 0.0001F)
							key.outTangent = tangentDirection.y / tangentDirection.x;
						else
							key.outTangent = Mathf.Infinity;
						CurveUtility.SetKeyTangentMode(ref key, 1, TangentMode.Editable);
		
						if (!CurveUtility.GetKeyBroken(key))
						{
							key.inTangent = key.outTangent;
							CurveUtility.SetKeyTangentMode(ref key, 0, TangentMode.Editable);
						}
					}
				
					dragged.key = dragged.curve.MoveKey(dragged.key, key);
					CurveUtility.UpdateTangentsFromModeSurrounding(dragged.curveWrapper.curve, dragged.key);
					
					dragged.curveWrapper.changed = true;
				
					Event.current.Use();
				} 
				
				break;
			
			case EventType.MouseUp:
				if (GUIUtility.hotControl == tangentId) 
				{
					GUIUtility.hotControl = 0;
					evt.Use ();
				}
				break;
			}
		}
	
		struct KeyFrameCopy
		{
			public float time, value, inTangent, outTangent;
			public int idx, selectionIdx;
			public KeyFrameCopy (int idx, int selectionIdx, Keyframe source) 
			{
				this.idx = idx;
				this.selectionIdx = selectionIdx;
				time = source.time;
				value = source.value;
				inTangent = source.inTangent;
				outTangent = source.outTangent;
			}	
		}
		
		void DeleteSelectedPoints () 
		{
			// Go over selection backwards and delete (avoids wrecking indices)
			for (int i = m_Selection.Count - 1; i >= 0; i--)
			{
				CurveSelection k = m_Selection[i];
				CurveWrapper cw = k.curveWrapper;

				if (!settings.allowDeleteLastKeyInCurve)
					if (cw.curve.keys.Length == 1)
						continue;

				cw.curve.RemoveKey (k.key);
				CurveUtility.UpdateTangentsFromMode(cw.curve);
				cw.changed = true;
			}
			SelectNone ();
		}
		
		private void DeleteKeys (object obj)
		{
			List<KeyIdentifier> keyList = (List<KeyIdentifier>)obj;
			
			// Go over selection backwards and delete (avoids wrecking indices)
			List<int> curveIds = new List<int>();
			for (int i = keyList.Count - 1; i >= 0; i--)
			{
				if (!settings.allowDeleteLastKeyInCurve)
					if (keyList[i].curve.keys.Length == 1)
						continue;

				keyList[i].curve.RemoveKey (keyList[i].key);
				CurveUtility.UpdateTangentsFromMode(keyList[i].curve);
				curveIds.Add(keyList[i].curveId);
			}
			string str;
			if (keyList.Count > 1)
				str = "Delete Keys";
			else
				str = "Delete Key";
			UpdateCurves(curveIds, str);
			SelectNone ();
		}
		
		float ClampVerticalValue (float value, int curveID)
		{
			// Clamp by global value
			value = Mathf.Clamp(value, vRangeMin, vRangeMax);

			// Clamp with per curve settings.
			CurveWrapper cw = GetCurveFromID(curveID);
			if (cw != null)
				value = Mathf.Clamp(value, cw.vRangeMin, cw.vRangeMax);

			return value;
		}

		
		public void UpdateCurvesFromPoints (Vector2 movement)
		{
			// Starting up:
			m_DisplayedSelection = new List<CurveSelection> ();
			// Go over all saved curves - each of these has at least one selected point.
			foreach (SavedCurve sc in m_CurveBackups)
			{
				// Go through each curve and build a new working set of points.
				List<SavedCurve.SavedKeyFrame> working = new List<SavedCurve.SavedKeyFrame> (sc.keys.Count);
				
				// Make sure to process in the right order so dragging keys don't remove each other
				int start, end, step;
				if (movement.x <= 0) 
				{
					start = 0; end = sc.keys.Count; step = 1;
				} 
				else
				{
					start = sc.keys.Count - 1; end = -1; step = -1;
				}
				
				for (int i = start; i != end; i += step)
				{
					SavedCurve.SavedKeyFrame keyframe = sc.keys[i];
					if (keyframe.selected != CurveWrapper.SelectionMode.None) 
					{
						// Duplicate it - so we don't modify the backup copy
						keyframe = new SavedCurve.SavedKeyFrame (keyframe.key, keyframe.selected);
						// Slide in time
						keyframe.key.time = Mathf.Clamp(keyframe.key.time + movement.x, hRangeMin, hRangeMax);
						// clamp key
						
						// if it's fully selected, also move on Y
						if (keyframe.selected == CurveWrapper.SelectionMode.Selected)
							keyframe.key.value = ClampVerticalValue (keyframe.key.value + movement.y, sc.curveId);
						
						// we've moved the key. Let's see if we're on top of other keys (that would then need deletion)
						for (int j = working.Count - 1; j >= 0; j--) 
						{
							if (Mathf.Abs (working[j].key.time - keyframe.key.time) < kCurveTimeEpsilon)
								working.RemoveAt (j);
						}
					}
					
					// Add this key to the working set
					working.Add (new SavedCurve.SavedKeyFrame (keyframe.key, keyframe.selected));
				}
				// Sliding things around can get them out of sync. Let's sort so we're safe.
				working.Sort ();
				// Working now contains a set of points with everything set up correctly.
				// Each point has it's selection set, but m_DisplayCurves has a more traditional key array. 
				// Go through the working points and sort those for display.
				Keyframe[] keysToAssign = new Keyframe[working.Count];
				for (int idx = 0; idx < working.Count; idx++)
				{
					SavedCurve.SavedKeyFrame sk = working[idx];
					keysToAssign[idx] = sk.key;
					if (sk.selected != CurveWrapper.SelectionMode.None) 
					{
						CurveSelection cs = new CurveSelection (sc.curveId, this, idx);
						if (sk.selected == CurveWrapper.SelectionMode.SemiSelected)
							cs.semiSelected = true;
						m_DisplayedSelection.Add (cs);
					}
				}
				
				// We now have the list of keys to assign - let's get them back into the animation clip
				CurveWrapper cw = GetCurveFromID(sc.curveId);
				cw.curve.keys = keysToAssign;
				cw.changed = true;
			}
			
			UpdateTangentsFromSelection ();
		}
	
		float SnapTime (float t)
		{
			if (EditorGUI.actionKey)
			{
				int snapLevel = hTicks.GetLevelWithMinSeparation(5);
				float snap = hTicks.GetPeriodOfLevel(snapLevel);
				t = Mathf.Round (t / snap) * snap;
			}
			else
			{
				if (invSnap != 0.0f)
					t = Mathf.Round (t * invSnap) / invSnap;
			}
			return t;
		}
		
		float SnapValue (float v)
		{
			if (EditorGUI.actionKey)
			{
				int snapLevel = vTicks.GetLevelWithMinSeparation(5);
				float snap = vTicks.GetPeriodOfLevel(snapLevel);
				v = Mathf.Round (v / snap) * snap;
			}
			return v;
		}
		
		/*string DebugSelection ()
		{
			string s = "";
			foreach (int i in m_PointSelection)
				s += i + ", ";
			s += "\n";
			foreach (CurveSelection k in m_Selection)
				s += "[" + k.curveID + ", " + k.key+ "], ";
			return s;
		}*/
		
		internal new class Styles
		{
			public Texture2D pointIcon = EditorGUIUtility.LoadIcon ("curvekeyframe");
			public Texture2D pointIconSelected = EditorGUIUtility.LoadIcon ("curvekeyframeselected");
			public Texture2D pointIconSelectedOverlay = EditorGUIUtility.LoadIcon ("curvekeyframeselectedoverlay");
			public Texture2D pointIconSemiSelectedOverlay = EditorGUIUtility.LoadIcon ("curvekeyframesemiselectedoverlay");
	
			public GUIStyle none = new GUIStyle ();
			public GUIStyle labelTickMarksY = "CurveEditorLabelTickMarks";
			public GUIStyle labelTickMarksX;
			public GUIStyle selectionRect = "SelectionRect";
			
			public GUIStyle dragLabel = "ProfilerBadge";
			public GUIStyle axisLabelNumberField = new GUIStyle (EditorStyles.miniTextField);

			public Styles ()
			{
				axisLabelNumberField.alignment = TextAnchor.UpperRight;
				labelTickMarksY.contentOffset = Vector2.zero; // TODO: Fix this in style when Editor has been merged to Trunk (31/8/2011)
				labelTickMarksX = new GUIStyle(labelTickMarksY);
				labelTickMarksX.clipping = TextClipping.Overflow;
			}
		}
		internal Styles ms_Styles;
	
		Vector2 GetGUIPoint (Vector3 point) 
		{
			return HandleUtility.WorldToGUIPoint (DrawingToViewTransformPoint(point));
		}
		
		Vector2 s_StartMouseDragPosition, s_EndMouseDragPosition, s_StartKeyDragPosition;
		float s_StartClickedTime;
		PickMode s_PickMode;
		
		int OnlyOneEditableCurve ()
		{
			int index = -1;
			int curves = 0;
			for (int i=0; i<m_AnimationCurves.Length; i++)
			{
				CurveWrapper wrapper = m_AnimationCurves[i];
				if (wrapper.hidden || wrapper.readOnly)
					continue;
				curves++;
				index = i;
			}
			if (curves == 1)
				return index;
			else
				return -1;
		}
		
		// Returns an index into m_AnimationCurves
		int GetCurveAtPosition (Vector2 position, out Vector2 closestPointOnCurve)
		{
			Vector2 viewPos = DrawingToViewTransformPoint(position);
			
			// Find the closest curve at the time corresponding to the position
			int maxPixelDist = (int)Mathf.Sqrt(kMaxPickDistSqr);
			float smallestDist = kMaxPickDistSqr;
			int closest = -1;
			closestPointOnCurve = Vector3.zero;
			
			// Use drawOrder to ensure we pick the topmost curve
			for (int i = m_DrawOrder.Count - 1; i >= 0; --i)
			{
				CurveWrapper wrapper = getCurveWrapperById(m_DrawOrder[i]);

				if (wrapper.hidden || wrapper.readOnly)
					continue;
				
				// Sample the curves at pixel intervals in the area around the desired time,
				// corresponding to the max cursor distance allowed.
				Vector2 valL;
				valL.x = position.x - maxPixelDist / scale.x;
				valL.y = wrapper.renderer.EvaluateCurveSlow(valL.x);
				valL = DrawingToViewTransformPoint(valL);
				for (int x=-maxPixelDist; x<maxPixelDist; x++)
				{
					Vector2 valR;
					valR.x = position.x + (x+1) / scale.x;
					valR.y = wrapper.renderer.EvaluateCurveSlow(valR.x);
					valR = DrawingToViewTransformPoint(valR);
					
					float dist = HandleUtility.DistancePointLine(viewPos, valL, valR);
					dist = dist*dist;
					if (dist < smallestDist)
					{
						smallestDist = dist;
						closest = wrapper.listIndex;
						closestPointOnCurve = HandleUtility.ProjectPointLine(viewPos, valL, valR);
					}
					
					valL = valR;
				}
			}
			closestPointOnCurve = ViewToDrawingTransformPoint(closestPointOnCurve);
			return closest;
		}
		
		void CreateKeyFromClick (object obj)
		{
			List<int> ids = CreateKeyFromClick((Vector2)obj);
			UpdateCurves(ids, "Add Key");
		}
		
		List<int> CreateKeyFromClick (Vector2 position)
		{
			List<int> curveIds = new List<int>();
			
			// Check if there is only one curve to edit
			int singleCurveIndex = OnlyOneEditableCurve();
			if (singleCurveIndex >= 0)
			{
				// If there is only one curve, allow creating keys on it by double/right-clicking anywhere
				// if the click is to the left or right of the existing keys, or if there are no existing keys.
				float time = position.x;
				CurveWrapper cw = m_AnimationCurves[singleCurveIndex];
				if (cw.curve.keys.Length == 0 || time < cw.curve.keys[0].time || time > cw.curve.keys[cw.curve.keys.Length-1].time)
				{
					CreateKeyFromClick(singleCurveIndex, position);
					curveIds.Add(cw.id);
					return curveIds;
				}
			}
			
			// If we didn't create a key above, only allow creating keys
			// when double/right-clicking on an existing curve
			Vector2 closestPointOnCurve;
			int curveIndex = GetCurveAtPosition(position, out closestPointOnCurve);
			CreateKeyFromClick (curveIndex, closestPointOnCurve.x);
			if (curveIndex >= 0)
				curveIds.Add(m_AnimationCurves[curveIndex].id);
			return curveIds;
		}
		
		void CreateKeyFromClick (int curveIndex, float time)
		{
			time = Mathf.Clamp(time, settings.hRangeMin, settings.hRangeMax);
			
			// Add a key on a curve at a specified time
			if (curveIndex >= 0)
			{
				CurveSelection selectedPoint = null;
				CurveWrapper cw = m_AnimationCurves[curveIndex];
				if (cw.groupId == -1) {
					selectedPoint = AddKeyAtTime(cw, time);
				} else {
					foreach (CurveWrapper cw2 in m_AnimationCurves)
					{
						if (cw2.groupId == cw.groupId) 
						{
							CurveSelection cs = AddKeyAtTime (cw2, time);
							if (cw2.id == cw.id)
								selectedPoint = cs;
						}
					}
					
				}
				if (selectedPoint != null)
				{
					m_Selection = new List<CurveSelection> (1);
					m_Selection.Add (selectedPoint);
					RecalcSecondarySelection ();
				}
				else
				{
					SelectNone ();
				}
			}
		}
		
		void CreateKeyFromClick (int curveIndex, Vector2 position)
		{
			position.x = Mathf.Clamp(position.x, settings.hRangeMin, settings.hRangeMax);
			
			// Add a key on a curve at a specified time
			if (curveIndex >= 0)
			{
				CurveSelection selectedPoint = null;
				CurveWrapper cw = m_AnimationCurves[curveIndex];
				if (cw.groupId == -1)
				{
					selectedPoint = AddKeyAtPosition(cw, position);
				}
				else
				{
					foreach (CurveWrapper cw2 in m_AnimationCurves)
					{
						if (cw2.groupId == cw.groupId) 
						{
							if (cw2.id == cw.id)
								selectedPoint = AddKeyAtPosition (cw2, position);
							else
								AddKeyAtTime (cw2, position.x);
						}
					}
					
				}
				if (selectedPoint != null)
				{
					m_Selection = new List<CurveSelection> (1);
					m_Selection.Add (selectedPoint);
					RecalcSecondarySelection ();
				}
				else
				{
					SelectNone ();
				}
			}
		}
		
		// Add a key to cw at time.
		// Returns the inserted key as a curveSelection
		CurveSelection AddKeyAtTime (CurveWrapper cw, float time)
		{
			// Find out if there's already a key there
			time = SnapTime(time);
			float halfFrame;
			if (invSnap != 0.0f)
				halfFrame = 0.5f / invSnap;
			else
				halfFrame = 0.0001f;
			if (CurveUtility.HaveKeysInRange(cw.curve, time-halfFrame, time+halfFrame))
				return null;
			
			// Add the key
			float slope = cw.renderer.EvaluateCurveDeltaSlow(time);
			float value = ClampVerticalValue (SnapValue(cw.renderer.EvaluateCurveSlow(time)), cw.id);
			Keyframe key = new Keyframe(time, value, slope, slope);
			return AddKeyframeAndSelect(key, cw);
		}
		
		// Add a key to cw at time.
		// Returns the inserted key as a curveSelection
		CurveSelection AddKeyAtPosition (CurveWrapper cw, Vector2 position)
		{
			// Find out if there's already a key there
			position.x = SnapTime(position.x);
			float halfFrame;
			if (invSnap != 0.0f)
				halfFrame = 0.5f / invSnap;
			else
				halfFrame = 0.0001f;
			if (CurveUtility.HaveKeysInRange(cw.curve, position.x-halfFrame, position.x+halfFrame))
				return null;
			
			// Add the key
			float slope = 0;
			Keyframe key = new Keyframe(position.x, SnapValue(position.y), slope, slope);
			return AddKeyframeAndSelect(key, cw);
		}
		
		CurveSelection AddKeyframeAndSelect (Keyframe key, CurveWrapper cw)
		{
			int keyIndex = cw.curve.AddKey(key);
			CurveUtility.SetKeyModeFromContext(cw.curve, keyIndex);
			CurveUtility.UpdateTangentsFromModeSurrounding(cw.curve, keyIndex);
			
			// Select the key
			CurveSelection selectedPoint = new CurveSelection(cw.id, this, keyIndex);
			cw.selected = CurveWrapper.SelectionMode.Selected;
			cw.changed = true;
			activeTime = key.time;
			return selectedPoint;
		}
		
		// Find keyframe nearest to the mouse. We use the draw order to ensure to return the
		// key that is topmost rendered if several keys are overlapping. The user can 
		// click on another curve to bring it to front and hereby be able to better select its keys.
		// Returns null if nothing is within Sqrt(kMaxPickDistSqr) pixels.
		CurveSelection FindNearest ()
		{
			Vector2 mousePos = Event.current.mousePosition;

			int bestCurveID = -1;
			int bestKey = -1;
			float nearestDist = kMaxPickDistSqr;
			
			// Last element in draw order list is topmost so reverse traverse list
			for (int index = m_DrawOrder.Count - 1; index >= 0; --index )
			{
				CurveWrapper cw = getCurveWrapperById(m_DrawOrder[index]);
				if (cw.readOnly || cw.hidden)
					continue;

				for (int i = 0; i < cw.curve.keys.Length; ++i)
				{
					Keyframe k = cw.curve.keys[i];
					float d = (GetGUIPoint(new Vector2(k.time, k.value)) - mousePos).sqrMagnitude;
					// If we have an exact hit we just return that key
					if (d <= kExactPickDistSqr)
						return new CurveSelection(cw.id, this, i);
					
					// Otherwise find closest
					if (d < nearestDist)
					{
						bestCurveID = cw.id;
						bestKey = i;
						nearestDist = d;
					}
				}
				// If top curve has key within range make it harder for keys below to get selected
				if (index == m_DrawOrder.Count - 1 && bestCurveID >= 0)
					nearestDist = kExactPickDistSqr; 
			}

			if (bestCurveID >= 0)
				return new CurveSelection(bestCurveID, this, bestKey);

			return null;
		}
		
		public void SelectNone ()
		{
			m_Selection = new List<CurveSelection> ();
			foreach (CurveWrapper cw in m_AnimationCurves)
				cw.selected = CurveWrapper.SelectionMode.None;
		}
		
		public void SelectAll ()
		{
			int totalLength = 0;
			foreach (CurveWrapper cw in m_AnimationCurves)
			{
				if (cw.hidden)
					continue;
				totalLength += cw.curve.length;
			}
			m_Selection = new List<CurveSelection> (totalLength);
			
			foreach (CurveWrapper cw in m_AnimationCurves)
			{
				cw.selected = CurveWrapper.SelectionMode.Selected;
				for (int j = 0; j < cw.curve.length; j++) 
					m_Selection.Add (new CurveSelection (cw.id, this, j));
			}
		}

		public bool IsDraggingCurveOrRegion ()
		{
			return m_DraggingCurveOrRegion != null;
		}

		public bool IsDraggingCurve (CurveWrapper cw)
		{
			return (m_DraggingCurveOrRegion != null && m_DraggingCurveOrRegion.Length == 1 && m_DraggingCurveOrRegion[0] == cw);
		}

		public bool IsDraggingRegion (CurveWrapper cw1, CurveWrapper cw2)
		{
			return (m_DraggingCurveOrRegion != null && m_DraggingCurveOrRegion.Length == 2 && (m_DraggingCurveOrRegion[0] == cw1 || m_DraggingCurveOrRegion[0] == cw2));
		}

		bool HandleCurveAndRegionMoveToFrontOnMouseDown (ref Vector2 timeValue, ref CurveWrapper[] curves)
		{
			// Did we click on a curve
			Vector2 closestPointOnCurve;
			int clickedCurveIndex = GetCurveAtPosition (mousePositionInDrawing, out closestPointOnCurve);
			if (clickedCurveIndex >= 0)
			{
				MoveCurveToFront (m_AnimationCurves[clickedCurveIndex].id);
				timeValue = mousePositionInDrawing;
				curves = new [] { m_AnimationCurves[clickedCurveIndex] };
				return true;
			}

			// Did we click in a region
			for (int i = m_DrawOrder.Count-1; i >= 0; --i)
			{
				CurveWrapper cw = getCurveWrapperById(m_DrawOrder[i]);

				if (cw == null)
					continue;
				if (cw.hidden)
					continue;
				if (cw.curve.length == 0)
					continue;

				CurveWrapper cw2 = null;
				if (i > 0)
					cw2 = getCurveWrapperById(m_DrawOrder[i - 1]);

				if (IsRegion(cw, cw2))
				{
					float mouseY = mousePositionInDrawing.y;
					float time = mousePositionInDrawing.x; // / scale.x;
					float v1 = cw.renderer.EvaluateCurveSlow(time);
					float v2 = cw2.renderer.EvaluateCurveSlow(time);
					if (v1 > v2)
					{
						float tmp = v1;
						v1 = v2; v2 = tmp;
					}
					if (mouseY >= v1 && mouseY <= v2)
					{
						timeValue = mousePositionInDrawing;
						curves = new[] {cw, cw2};
						MoveCurveToFront (cw.id);
						return true;
					}
					i--; // we handled two curves
				}
				
			}
			return false; // No curves or regions hit
		}

		void SelectPoints ()
		{
			int id = GUIUtility.GetControlID (897560, FocusType.Passive);
			Event evt = Event.current;
			bool addToSelection = evt.shift;
			switch (evt.GetTypeForControl (id)) 
			{
			case EventType.Layout:
				HandleUtility.AddDefaultControl (id);
				break;
			
			case EventType.ContextClick:
				if (drawRect.Contains (GUIClip.Unclip (Event.current.mousePosition)))
				{
					Vector2 closestPositionOnCurve;
					int curveIndex = GetCurveAtPosition (mousePositionInDrawing, out closestPositionOnCurve);
					if (curveIndex >= 0)
					{
						GenericMenu menu = new GenericMenu ();
						menu.AddItem (new GUIContent ("Add Key"), false, CreateKeyFromClick, mousePositionInDrawing);
						menu.ShowAsContext ();
						Event.current.Use ();
					}
				}
				break;
			
			case EventType.MouseDown:
				if (evt.clickCount == 2 && evt.button == 0)
				{
					CreateKeyFromClick (mousePositionInDrawing);
				}
				else if (evt.button == 0)
				{
					CurveSelection selectedPoint = FindNearest ();
					if (selectedPoint == null || selectedPoint.semiSelected) 
					{
						if (!addToSelection)
							SelectNone();
						// If we did not hit a key then check if a curve or region was clicked
						Vector2 timeValue = Vector2.zero;
						CurveWrapper[] curves = null;
						HandleCurveAndRegionMoveToFrontOnMouseDown(ref timeValue, ref curves);

						GUIUtility.hotControl = id;
						s_EndMouseDragPosition = s_StartMouseDragPosition = evt.mousePosition;
						s_PickMode = PickMode.Click;
					}
					else
					{
						MoveCurveToFront (selectedPoint.curveID);
						activeTime = selectedPoint.keyframe.time;
						s_StartKeyDragPosition = new Vector2(selectedPoint.keyframe.time, selectedPoint.keyframe.value);
						
						if (addToSelection) 
						{
							List<CurveSelection> newSelection = new List<CurveSelection> (m_Selection);
							if (m_Selection.IndexOf (selectedPoint) == -1)
							{
								newSelection.Add (selectedPoint);
								newSelection.Sort ();
							}
							// TODO click removing from selection isnt't working. Following code will only break axis lock dragging. Change group selection from shift to ctrl?
							//else
							//	newSelection.Remove (selectedPoint);
	
							m_Selection = newSelection;
						} 
						else if (m_Selection.IndexOf (selectedPoint) == -1) 
						{
							m_Selection = new List<CurveSelection> (1);
							m_Selection.Add (selectedPoint);
						}
					}
					GUI.changed = true;
					HandleUtility.Repaint ();
				}
				break;
	
			case EventType.MouseDrag:
				if (GUIUtility.hotControl == id)
				{
					s_EndMouseDragPosition = evt.mousePosition;
					if (s_PickMode == PickMode.Click) 
					{
						s_PickMode = PickMode.Marquee;
						if (addToSelection)
							s_SelectionBackup = new List<CurveSelection> (m_Selection);
						else 
							s_SelectionBackup = new List<CurveSelection> ();
										
					}
					else
					{						
						Rect r = EditorGUIExt.FromToRect (s_StartMouseDragPosition, evt.mousePosition);
								
						List<CurveSelection> newSelection = new List<CurveSelection> (s_SelectionBackup);
						foreach (CurveWrapper cw in m_AnimationCurves)
						{
							if (cw.readOnly || cw.hidden)
								continue;
							int i = 0;
							foreach (Keyframe k in cw.curve.keys) 
							{
								if (r.Contains (GetGUIPoint (new Vector2 (k.time, k.value))))
								{
									newSelection.Add (new CurveSelection (cw.id, this, i));
									MoveCurveToFront (cw.id);
								}
								i++;
							}
						}
						newSelection.Sort ();
						m_Selection = newSelection;
						GUI.changed = true;
						
					}
					evt.Use ();
				}
				break;
	
			case EventType.MouseUp:
				if (GUIUtility.hotControl == id)
				{
					GUIUtility.hotControl = 0;
					s_PickMode = PickMode.None;
					Event.current.Use ();
				}
				break;
			}
			
			if (s_PickMode == PickMode.Marquee)
			{
				GUI.Label (EditorGUIExt.FromToRect (s_StartMouseDragPosition, s_EndMouseDragPosition), GUIContent.none, ms_Styles.selectionRect);
			}
			
		}



		string m_AxisLabelFormat = "n1";
	
		private void EditAxisLabels ()
		{
			int id = GUIUtility.GetControlID(18975602, FocusType.Keyboard);
			
			List<CurveWrapper> curvesInSameSpace = new List<CurveWrapper>();
			Vector2 axisUiScalars = GetAxisUiScalars (curvesInSameSpace);
			bool isEditable = axisUiScalars.y >= 0 && curvesInSameSpace.Count > 0 && curvesInSameSpace[0].setAxisUiScalarsCallback != null;
			if (!isEditable)
				return;

			
			Rect editRect = new Rect(0, topmargin - 8, leftmargin - 4, 16);
			Rect dragRect = editRect;
			dragRect.y -= editRect.height;

			Event evt = Event.current;
			switch (evt.GetTypeForControl (id))
			{
				case EventType.repaint:
					if (GUIUtility.hotControl == 0)
						EditorGUIUtility.AddCursorRect(dragRect, MouseCursor.SlideArrow);
					break;
				
				case EventType.MouseDown:
					if (evt.button == 0)
					{
						if (dragRect.Contains(Event.current.mousePosition))
						{
							if (GUIUtility.hotControl == 0)
							{
								GUIUtility.keyboardControl = 0;
								GUIUtility.hotControl = id;
								GUI.changed = true;
								evt.Use();
							}
						}
						if (!editRect.Contains(Event.current.mousePosition))
							GUIUtility.keyboardControl = 0; // If not hitting our FloatField ensure it loses focus
					}
					break;

				case EventType.MouseDrag:
					if (GUIUtility.hotControl == id)
					{
						float dragSensitity = Mathf.Clamp01(Mathf.Max(axisUiScalars.y, Mathf.Pow(Mathf.Abs(axisUiScalars.y), 0.5f)) * .01f);
						axisUiScalars.y += HandleUtility.niceMouseDelta * dragSensitity;
						if (axisUiScalars.y < 0.001f)
							axisUiScalars.y = 0.001f; // Since the scalar is a magnitude we do not want to drag to 0 and below.. find nicer solution
						SetAxisUiScalars (axisUiScalars, curvesInSameSpace);
						evt.Use();
					}
					break;

				case EventType.MouseUp:
					if (GUIUtility.hotControl == id)
					{
						// Reset dragging
						GUIUtility.hotControl = 0;
					}
					break;
			}
			
			// Show input text field
			string orgFormat = EditorGUI.kFloatFieldFormatString;
			EditorGUI.kFloatFieldFormatString = m_AxisLabelFormat; 
			float newValue = EditorGUI.FloatField (editRect, axisUiScalars.y, ms_Styles.axisLabelNumberField);
			if (axisUiScalars.y != newValue)
				SetAxisUiScalars (new Vector2 (axisUiScalars.x, newValue), curvesInSameSpace);
			EditorGUI.kFloatFieldFormatString = orgFormat;
		}


		public void BeginTimeRangeSelection (float time, bool addToSelection)
		{
			if (s_TimeRangeSelectionActive)
			{
				Debug.LogError ("BeginTimeRangeSelection can only be called once");
				return;
			}

			s_TimeRangeSelectionActive = true;
			s_TimeRangeSelectionStart = s_TimeRangeSelectionEnd = time;
			if (addToSelection)
				s_SelectionBackup = new List<CurveSelection> (m_Selection);
			else 
				s_SelectionBackup = new List<CurveSelection> ();
		}
		
		public void TimeRangeSelectTo (float time)
		{
			if (!s_TimeRangeSelectionActive)
			{
				Debug.LogError ("TimeRangeSelectTo can only be called after BeginTimeRangeSelection");
				return;
			}

			s_TimeRangeSelectionEnd = time;
			
			m_Selection = new List<CurveSelection> (s_SelectionBackup);
			float minTime = Mathf.Min (s_TimeRangeSelectionStart, s_TimeRangeSelectionEnd);
			float maxTime = Mathf.Max (s_TimeRangeSelectionStart, s_TimeRangeSelectionEnd);
			foreach (CurveWrapper cw in m_AnimationCurves)
			{
				if (cw.readOnly || cw.hidden)
					continue;
				int i = 0;
				foreach (Keyframe k in cw.curve.keys) 
				{
					if (k.time >= minTime && k.time < maxTime) {
						m_Selection.Add (new CurveSelection (cw.id, this, i));
					}
					i++;
				}
			}
			m_Selection.Sort ();
			RecalcSecondarySelection ();
			RecalcCurveSelection ();
		}
		
		public void EndTimeRangeSelection () 
		{
			if (!s_TimeRangeSelectionActive)
			{
				Debug.LogError ("EndTimeRangeSelection can only be called after BeginTimeRangeSelection");
				return;
			}

			s_TimeRangeSelectionStart = s_TimeRangeSelectionEnd = 0;
			s_TimeRangeSelectionActive = false;
		}
		
		public void CancelTimeRangeSelection ()
		{
			if (!s_TimeRangeSelectionActive)
			{
				Debug.LogError ("CancelTimeRangeSelection can only be called after BeginTimeRangeSelection");
				return;
			}
			
			m_Selection = s_SelectionBackup;	
			s_TimeRangeSelectionActive = false;
		}
		
		public void DrawTimeRange () 
		{
			if (s_TimeRangeSelectionActive && Event.current.type == EventType.Repaint) {
				float minTime = Mathf.Min (s_TimeRangeSelectionStart, s_TimeRangeSelectionEnd);
				float maxTime = Mathf.Max (s_TimeRangeSelectionStart, s_TimeRangeSelectionEnd);
				float start = GetGUIPoint (new Vector3 (minTime, 0,0)).x;
				float end = GetGUIPoint (new Vector3 (maxTime, 0,0)).x;
				GUI.Label (new Rect (start, -10000, end-start, 20000), GUIContent.none, ms_Styles.selectionRect);
			}
		}

		// m_DraggingCurveOrRegion is null if nothing is being dragged, has one entry if single curve being dragged or has two entries if region is being dragged
		CurveWrapper[] m_DraggingCurveOrRegion = null;

		void SetupKeyOrCurveDragging (Vector2 timeValue, CurveWrapper cw, int id, Vector2 mousePos)
		{
			m_DraggedCoord = timeValue;
			m_DraggingKey = cw;
			GUIUtility.hotControl = id;
			MakeCurveBackups();
			s_StartMouseDragPosition = mousePos;
			activeTime = timeValue.x;
			s_StartClickedTime = timeValue.x;
		
		}

		public Vector2 MovePoints () 
		{			
			int id = GUIUtility.GetControlID (FocusType.Passive);

			if (!hasSelection && !settings.allowDraggingCurvesAndRegions) 
				return Vector2.zero;

			Event evt = Event.current;
			switch (evt.GetTypeForControl(id)) 
			{
			case EventType.MouseDown:
				if (evt.button == 0) 
				{
					// Key dragging
					foreach (CurveSelection cs in m_Selection)
					{
						if (cs.curveWrapper.hidden)
							continue;
						if ((DrawingToViewTransformPoint(GetPosition (cs)) - evt.mousePosition).sqrMagnitude <= kMaxPickDistSqr)
						{
							SetupKeyOrCurveDragging(new Vector2(cs.keyframe.time, cs.keyframe.value), cs.curveWrapper, id, evt.mousePosition);
							break;
						}
					}

					// Curve dragging. Moving keys has highest priority, therefore we check curve/region dragging AFTER key dragging above
					if (settings.allowDraggingCurvesAndRegions && m_DraggingKey == null)
					{
						// We use the logic as for moving keys when we drag entire curves or regions: We just
						// select all keyFrames in a curve or region before dragging and ensure to hide tangents when drawing.
						Vector2 timeValue = Vector2.zero;
						CurveWrapper[] curves = null;
						if (HandleCurveAndRegionMoveToFrontOnMouseDown (ref timeValue, ref curves))
						{
							// Add all keys of curves to selection to reuse code of key dragging
							foreach (CurveWrapper cw in curves)
							{
								for (int i = 0; i < cw.curve.keys.Length; ++i)
									m_Selection.Add (new CurveSelection (cw.id, this, i));
								MoveCurveToFront (cw.id);
							}

							// Call after selection above
							SetupKeyOrCurveDragging(timeValue, curves[0], id, evt.mousePosition);
							m_DraggingCurveOrRegion = curves;
						}
					}
				}
				break;
			case EventType.MouseDrag:
				if (GUIUtility.hotControl == id)
				{
					Vector2 delta = evt.mousePosition - s_StartMouseDragPosition;
					Vector3 motion;
					
					if (m_DraggingCurveOrRegion != null)
					{
						// Curve/Region dragging only in y axis direction (for now)
						delta.x = 0;
						motion = ViewToDrawingTransformVector(delta);
						motion.y = SnapValue (motion.y + s_StartKeyDragPosition.y) - s_StartKeyDragPosition.y;
					}
					else
					{
						// Only drag along x OR y when shift is held down
						if (evt.shift)
						{
							if (Mathf.Abs(delta.x) > Mathf.Abs(delta.y))
							{
								delta.y = 0;
								motion = ViewToDrawingTransformVector(delta);
								motion.x = SnapTime(motion.x + s_StartKeyDragPosition.x) - s_StartKeyDragPosition.x;
							}
							else
							{
								delta.x = 0;
								motion = ViewToDrawingTransformVector(delta);
								motion.y = SnapValue (motion.y + s_StartKeyDragPosition.y) - s_StartKeyDragPosition.y;
							}
						}
						else
						{
							motion = ViewToDrawingTransformVector(delta);
							motion.x = SnapTime(motion.x + s_StartKeyDragPosition.x) - s_StartKeyDragPosition.x;
							motion.y = SnapValue(motion.y + s_StartKeyDragPosition.y) - s_StartKeyDragPosition.y;
						}
					}

					UpdateCurvesFromPoints (motion);
					GUI.changed = true;
					evt.Use ();
					return motion;
				}
				break;
			case EventType.KeyDown:
				if (GUIUtility.hotControl == id && evt.keyCode == KeyCode.Escape)
				{
					UpdateCurvesFromPoints (Vector2.zero);
					ResetDragging ();
					GUI.changed = true;
					evt.Use ();
				}
				break;
			case EventType.MouseUp:
				if (GUIUtility.hotControl == id)
				{
					ResetDragging();
					GUI.changed = true;
					evt.Use();
				}
				break;
			case EventType.Repaint:
				if (m_DraggingCurveOrRegion != null)
					EditorGUIUtility.AddCursorRect(new Rect(evt.mousePosition.x - 10, evt.mousePosition.y - 10, 20, 20), MouseCursor.ResizeVertical);
				break;
			}

			return Vector2.zero;
		}

		void ResetDragging ()
		{
			// If we are dragging entire curve we have selected all keys we therefore ensure to deselect them again...
			if (m_DraggingCurveOrRegion != null)
				SelectNone(); 

			// Cleanup
			GUIUtility.hotControl = 0;
			m_DraggingKey = null;
			m_DraggingCurveOrRegion = null;
			m_DisplayedSelection = null;
			m_MoveCoord = Vector2.zero;
		}
	
		internal void MakeCurveBackups ()
		{
			m_CurveBackups = new List<SavedCurve> ();
			int lastCurveID = -1;
			SavedCurve sc = null;
			for (int i = 0; i < m_Selection.Count; i++)
			{
				CurveSelection cs = m_Selection[i];
				// if it's a different curve than last point, we need to back up this curve.
				if (cs.curveID != lastCurveID) 
				{
					// Make a new saved curve with copy of all keyframes. No need to mark them as selected
					sc = new SavedCurve ();
					lastCurveID = sc.curveId = cs.curveID;
					Keyframe[] keys = cs.curve.keys;
					sc.keys = new List<SavedCurve.SavedKeyFrame> (keys.Length);
					foreach (Keyframe k in keys)
						sc.keys.Add (new SavedCurve.SavedKeyFrame(k, CurveWrapper.SelectionMode.None));
					m_CurveBackups.Add (sc);
				}
				
				// Mark them as selected
				sc.keys[cs.key].selected = cs.semiSelected ? CurveWrapper.SelectionMode.SemiSelected : CurveWrapper.SelectionMode.Selected;
			}
		}
		
		// Get the position of a CurveSelection. This will correctly offset tangent handles 
		Vector2 GetPosition (CurveSelection selection)
		{
			//AnimationCurve curve = selection.curve;
			Keyframe key = selection.keyframe;
			Vector2 position = new Vector2 (key.time, key.value);
			
			float tangentLength = 50F;
			
			if (selection.type == CurveSelection.SelectionType.InTangent)
			{
				Vector2 dir = new Vector2(1.0F, key.inTangent);
				if (key.inTangent == Mathf.Infinity) dir = new Vector2(0, -1);
				dir = NormalizeInViewSpace(dir);
				return position - dir*tangentLength;
			}
			else if (selection.type == CurveSelection.SelectionType.OutTangent)
			{
				Vector2 dir = new Vector2(1.0F, key.outTangent);
				if (key.outTangent == Mathf.Infinity) dir = new Vector2(0, -1);
				dir = NormalizeInViewSpace(dir);
				return position + dir*tangentLength;
			}
			else
				return position;
		}

		void MoveCurveToFront (int curveID)
		{
			int numCurves = m_DrawOrder.Count;
			for (int i = 0; i < numCurves; ++i)
			{
				// Find curveID in draw order list
				if (m_DrawOrder[i] == curveID)
				{
					// If curve is part of a region then find other curve
					int regionId = getCurveWrapperById (curveID).regionId;
					if (regionId >= 0)
					{
						// The other region curve can be on either side of current
						int indexOffset = 0;
						int curveID2 = -1;

						if (i - 1 >= 0)
						{
							int id = m_DrawOrder[i - 1];
							if (regionId == getCurveWrapperById(id).regionId)
							{
								curveID2 = id;
								indexOffset = -1;
							}
						}
						if (i + 1 < numCurves && curveID2 < 0)
						{
							int id = m_DrawOrder[i + 1];
							if (regionId == getCurveWrapperById(id).regionId)
							{
								curveID2 = id;
								indexOffset = 0;
							}
						}

						if (curveID2 >= 0)
						{
							m_DrawOrder.RemoveRange(i + indexOffset, 2);
							m_DrawOrder.Add(curveID2);
								m_DrawOrder.Add(curveID);	// ensure curveID is topMost (last)
								ValidateCurveList();
								return;
							}

						Debug.LogError ("Unhandled region");
					}
					else // Single curve
					{
						if (i == numCurves - 1)
							return; // curve already last (topmost)

						m_DrawOrder.RemoveAt(i);
						m_DrawOrder.Add(curveID);
						ValidateCurveList();
						return;
					}
					
				}
			}
		}

		bool IsCurveSelected (CurveWrapper cw)
		{
			if (cw != null)
				return cw.selected != CurveWrapper.SelectionMode.None;
			return false;
		}

		bool IsRegionCurveSelected (CurveWrapper cw1, CurveWrapper cw2)
		{
			return IsCurveSelected (cw1) ||
				   IsCurveSelected (cw2);
		}

		bool IsRegion (CurveWrapper cw1, CurveWrapper cw2)
		{
			if (cw1 != null && cw2 != null)
				if (cw1.regionId >= 0)
					return cw1.regionId == cw2.regionId;
			return false;
		}

		void DrawCurvesAndRegion(CurveWrapper cw1, CurveWrapper cw2, List<CurveSelection> selection, bool hasFocus)
		{
			DrawRegion (cw1, cw2, hasFocus);
			DrawCurveAndPoints (cw1, IsCurveSelected (cw1) ? selection : null, hasFocus);
			DrawCurveAndPoints (cw2, IsCurveSelected (cw2) ? selection : null, hasFocus);
		}


		void DrawCurveAndPoints (CurveWrapper cw, List<CurveSelection> selection, bool hasFocus)
		{
			DrawCurve(cw, hasFocus);
			DrawPointsOnCurve(cw, selection, hasFocus);			
		}

		bool ShouldCurveHaveFocus (int indexIntoDrawOrder, CurveWrapper cw1, CurveWrapper cw2)
		{
			bool focus = false;
			if (indexIntoDrawOrder == m_DrawOrder.Count - 1)
				focus = true;
			else if (hasSelection)
				focus = IsCurveSelected(cw1) || IsCurveSelected(cw2);
			return focus;
		}


		void DrawCurves (CurveWrapper[] curves)
		{	
			if (Event.current.type != EventType.Repaint)
				return;
			
			List<CurveSelection> selection = (m_DisplayedSelection != null) ? m_DisplayedSelection : m_Selection;
			
			// Draw all curves
			for (int i=0; i< m_DrawOrder.Count; ++i)
			{
				CurveWrapper cw = getCurveWrapperById (m_DrawOrder[i]);

				if (cw == null)
					continue;
				if (cw.hidden)
					continue;
				if (cw.curve.length == 0)
					continue;


				CurveWrapper cw2 = null;
				if (i < m_DrawOrder.Count- 1)
					cw2 = getCurveWrapperById(m_DrawOrder[i + 1]);
				
				if (IsRegion (cw, cw2))
				{
					i++; // we handle two curves

					bool focus = ShouldCurveHaveFocus(i, cw, cw2);
					DrawCurvesAndRegion(cw, cw2, IsRegionCurveSelected(cw, cw2) ? selection : null, focus);
					
				}
				else
				{
					bool focus = ShouldCurveHaveFocus(i, cw, null);
					DrawCurveAndPoints(cw, IsCurveSelected(cw) ? selection : null, focus);
				}
			}
			
			if (m_DraggingCurveOrRegion != null)
				return;

			// Draw left and right tangents lines
			HandleUtility.handleWireMaterial.SetPass (0);
			GL.Begin (GL.LINES);
			GL.Color (m_TangentColor * new Color (1, 1, 1, 0.75f));
			foreach (CurveSelection sel in selection)
			{
				if (sel.semiSelected)
					continue;
				Vector2 keyPoint = GetPosition(sel);
	
				if (CurveUtility.GetKeyTangentMode(sel.keyframe, 0) == TangentMode.Editable && sel.keyframe.time != sel.curve.keys[0].time)
				{
					Vector2 leftTangent = GetPosition(new CurveSelection(sel.curveID, this, sel.key, CurveSelection.SelectionType.InTangent));
					DrawLine (leftTangent, keyPoint);
				}
	
				if (CurveUtility.GetKeyTangentMode(sel.keyframe, 1) == TangentMode.Editable && sel.keyframe.time != sel.curve.keys[sel.curve.keys.Length-1].time)
				{
					Vector2 rightTangent = GetPosition(new CurveSelection(sel.curveID, this, sel.key, CurveSelection.SelectionType.OutTangent));
					DrawLine (keyPoint, rightTangent);
				}	
			}
			GL.End ();
			
			// Draw left and right tangents handles
			GUI.color = m_TangentColor;
			foreach (CurveSelection sel in selection)
			{
				if (sel.semiSelected)
					continue;
	
				if (CurveUtility.GetKeyTangentMode(sel.keyframe, 0) == TangentMode.Editable && sel.keyframe.time != sel.curve.keys[0].time)
				{
					Vector2 leftTangent = GetPosition(new CurveSelection(sel.curveID, this, sel.key, CurveSelection.SelectionType.InTangent));
					DrawPoint (leftTangent.x, leftTangent.y, ms_Styles.pointIcon);
				}
	
				if (CurveUtility.GetKeyTangentMode(sel.keyframe, 1) == TangentMode.Editable && sel.keyframe.time != sel.curve.keys[sel.curve.keys.Length-1].time)
				{
					Vector2 rightTangent = GetPosition(new CurveSelection(sel.curveID, this, sel.key, CurveSelection.SelectionType.OutTangent));
					DrawPoint (rightTangent.x, rightTangent.y, ms_Styles.pointIcon);
				}	
			}

			// Draw label with values while dragging
			if (m_DraggingKey != null)
			{
				// Clamp only using the currently dragged curve (we could have more selected but we only show the coord info for this one).
				float smallestVRangeMin = vRangeMin;
				float smallestVRangeMax = vRangeMax;
				smallestVRangeMin = Mathf.Max(smallestVRangeMin, m_DraggingKey.vRangeMin);
				smallestVRangeMax = Mathf.Min(smallestVRangeMax, m_DraggingKey.vRangeMax);

				Vector2 newPoint = m_DraggedCoord + m_MoveCoord;
				newPoint.x = Mathf.Clamp(newPoint.x, hRangeMin, hRangeMax);
				newPoint.y = Mathf.Clamp(newPoint.y, smallestVRangeMin, smallestVRangeMax);
				Vector2 p = DrawingToViewTransformPoint(newPoint);
				
				int hDecimals;
				if (invSnap != 0)
					hDecimals = MathUtils.GetNumberOfDecimalsForMinimumDifference(1 / invSnap);
				else
					hDecimals =  MathUtils.GetNumberOfDecimalsForMinimumDifference(shownArea.width / drawRect.width);
				int vDecimals =  MathUtils.GetNumberOfDecimalsForMinimumDifference(shownArea.height / drawRect.height);

				Vector2 axisUiScalars = m_DraggingKey.getAxisUiScalarsCallback != null ? m_DraggingKey.getAxisUiScalarsCallback() : Vector2.one;
				if (axisUiScalars.x >= 0f)
					newPoint.x *= axisUiScalars.x;
				if (axisUiScalars.y >= 0f)
					newPoint.y *= axisUiScalars.y;
				GUIContent content = new GUIContent (string.Format("{0}, {1}", newPoint.x.ToString("N"+hDecimals), newPoint.y.ToString("N"+vDecimals)));
				Vector2 size = ms_Styles.dragLabel.CalcSize (content);
				EditorGUI.DoDropShadowLabel(
					new Rect(p.x, p.y-size.y, size.x, size.y), content, ms_Styles.dragLabel, 0.3f
				);
			}
		}

		static List<Vector3> CreateRegion(CurveWrapper minCurve, CurveWrapper maxCurve, float deltaTime)
		{
			// Create list of triangle points
			List<Vector3> region = new List<Vector3>();

			List<float> sampleTimes = new List<float>();
			float sampleTime = deltaTime;
			for (; sampleTime <= 1.0f; sampleTime += deltaTime)
				sampleTimes.Add(sampleTime);

			if (sampleTime != 1.0f)
				sampleTimes.Add(1.0f);

			// To handle constant curves (high gradient) we add key time samples on both side of the keys as well
			// the key time itself.
			foreach (Keyframe key in maxCurve.curve.keys)
				if (key.time > 0f && key.time < 1.0f)
				{
					sampleTimes.Add(key.time - 0.0001f);
					sampleTimes.Add(key.time);
					sampleTimes.Add(key.time + 0.0001f);
				}
			foreach (Keyframe key in minCurve.curve.keys)
				if (key.time > 0f && key.time < 1.0f)
				{
					sampleTimes.Add(key.time - 0.0001f);
					sampleTimes.Add(key.time);
					sampleTimes.Add(key.time + 0.0001f);
				}
			
			sampleTimes.Sort ();

			Vector3 prevA = new Vector3(0.0f, maxCurve.renderer.EvaluateCurveSlow(0.0f), 0.0f);
			Vector3 prevB = new Vector3(0.0f, minCurve.renderer.EvaluateCurveSlow(0.0f), 0.0f);
			for (int i=0; i<sampleTimes.Count; ++i)
			{
				float time = sampleTimes[i];
				Vector3 valueA = new Vector3(time, maxCurve.renderer.EvaluateCurveSlow(time), 0.0f);
				Vector3 valueB = new Vector3(time, minCurve.renderer.EvaluateCurveSlow(time), 0.0f);

				// Add triangles 
				if (prevA.y >= prevB.y && valueA.y >= valueB.y)
				{
					// max is top
					region.Add(prevA);
					region.Add(valueB);
					region.Add(prevB);

					region.Add(prevA);
					region.Add(valueA);
					region.Add(valueB);
				}
				else if (prevA.y <= prevB.y && valueA.y <= valueB.y)
				{
					// min is top
					region.Add(prevB);
					region.Add(valueA);
					region.Add(prevA);

					region.Add(prevB);
					region.Add(valueB);
					region.Add(valueA);
				}
				else
				{
					// Find intersection
					Vector2 intersection = Vector2.zero;
					if (Mathf.LineIntersection(prevA, valueA, prevB, valueB, ref intersection))
					{
						region.Add(prevA);
						region.Add(intersection);
						region.Add(prevB);

						region.Add(valueA);
						region.Add(intersection);
						region.Add(valueB);
					}
					else
					{
						Debug.Log ("Error: No intersection found! There should be one...");
					}
				}


				prevA = valueA;
				prevB = valueB;
			}

			return region;
		}


		public void DrawRegion (CurveWrapper curve1, CurveWrapper curve2, bool hasFocus)
		{
			float deltaTime = 1.0f / (rect.width / 10.0f);
			List<Vector3> points = CreateRegion(curve1, curve2, deltaTime);
			Color color = curve1.color;

			// Transform points from normalized drawing space to screen space
			for (int i = 0; i < points.Count; i++)
				points[i] = drawingToViewMatrix.MultiplyPoint(points[i]);

			if (IsDraggingRegion(curve1, curve2))
			{
				color = Color.Lerp(color, Color.black, 0.1f);
				color.a = 0.4f;
			}
			else if (settings.useFocusColors && !hasFocus)
			{
				color *= 0.4f; 
				color.a = 0.1f;
			}
			else
			{
				color *= 1.0f;
				color.a = 0.4f;
			}

			Shader.SetGlobalColor("_HandleColor", color);
			//Shader.SetGlobalFloat("_HandleSize", 1);
			HandleUtility.handleWireMaterial.SetPass(0);
			GL.Begin(GL.TRIANGLES);
			int numTriangles = points.Count / 3;
			for (int i = 0; i < numTriangles; i++)
			{
				GL.Color(color);
				GL.Vertex(points[i * 3]);
				GL.Vertex(points[i * 3 + 1]);
				GL.Vertex(points[i * 3 + 2]);
			}
			GL.End();			
		}

		void DrawCurve (CurveWrapper cw, bool hasFocus)
		{			
			CurveRenderer renderer = cw.renderer;
			Color color = cw.color;

			if (IsDraggingCurve (cw) || cw.selected == CurveWrapper.SelectionMode.Selected)
			{
				color = Color.Lerp (color, Color.white, 0.3f);
			}
			else if (settings.useFocusColors && !hasFocus)
			{
				color *= 0.5f;
				color.a = 0.8f;
			}

			Rect framed = shownArea;
			renderer.DrawCurve(framed.xMin, framed.xMax, color, drawingToViewMatrix, settings.wrapColor);
		}
		
		void DrawPointsOnCurve (CurveWrapper cw, List<CurveSelection> selected, bool hasFocus)
		{
			m_PreviousDrawPointCenter = new Vector2(float.MinValue, float.MinValue);

			if (selected == null)
			{
				Color color = cw.color;
				if (settings.useFocusColors && !hasFocus)
					color *= 0.5f;
				GUI.color = color;
				foreach (Keyframe k in cw.curve.keys)
				{
					DrawPoint (k.time, k.value, ms_Styles.pointIcon);
				}
			}
			else
			{
				Color keyColor = Color.Lerp (cw.color, Color.white, .2f);
				GUI.color = keyColor;
			
				int selectionIdx = 0;
				// Find the point in m_Selection that matches the curve we're about to draw.
				while (selectionIdx < selected.Count && selected[selectionIdx].curveID != cw.id)
					selectionIdx++;
				// we're now at the right point in the selection.
				int pointIdx = 0;
				foreach (Keyframe k in cw.curve.keys)
				{
					if (selectionIdx < selected.Count && selected[selectionIdx].key == pointIdx && selected[selectionIdx].curveID == cw.id)
					{
						Vector3 pos = DrawingToViewTransformPoint(new Vector3 (k.time, k.value, 0));
						// Important to take floor of positions of GUI stuff to get pixel correct alignment of
						// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
						pos = new Vector3(Mathf.Floor(pos.x), Mathf.Floor(pos.y), 0);
						Rect r = new Rect (pos.x - 4, pos.y - 4, ms_Styles.pointIcon.width, ms_Styles.pointIcon.height);
						// TODO: GUIStyle.none has hopping margins that need to be fixed
						GUI.Label (r, ms_Styles.pointIconSelected, ms_Styles.none);
						GUI.color = Color.white;
						if (!selected[selectionIdx].semiSelected)
							GUI.Label (r, ms_Styles.pointIconSelectedOverlay, ms_Styles.none);
						else
							GUI.Label (r, ms_Styles.pointIconSemiSelectedOverlay, ms_Styles.none);
						GUI.color = keyColor;
						selectionIdx++;
					} else
						DrawPoint (k.time, k.value, ms_Styles.pointIcon);
					pointIdx++;
				}
				GUI.color = Color.white;
			}
		}

		void DrawPoint (float x, float y, Texture2D icon)
		{
			Vector3 pos = DrawingToViewTransformPoint(new Vector3 (x, y, 0));

			// Important to take floor of positions of GUI stuff to get pixel correct alignment of
			// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
			pos = new Vector3(Mathf.Floor(pos.x), Mathf.Floor(pos.y), 0);
			// TODO: GUIStyle.none has hopping margins that need to be fixed
			Rect rect = new Rect(pos.x - 4, pos.y - 4, ms_Styles.pointIcon.width, ms_Styles.pointIcon.height);

			Vector2 center = rect.center;
			if ((center - m_PreviousDrawPointCenter).magnitude > 8)
			{
				GUI.Label(rect, icon, GUIStyleX.none);
				m_PreviousDrawPointCenter = center;
			}
		}
		
		// FIXME remove when grid drawing function has been properly rewritten
		void DrawLine (Vector2 lhs, Vector2 rhs)
		{
			GL.Vertex (DrawingToViewTransformPoint(new Vector3 (lhs.x, lhs.y, 0)));
			GL.Vertex (DrawingToViewTransformPoint(new Vector3 (rhs.x, rhs.y, 0)));
		}
		


		// The return value for each axis can be -1, if so then we do not have any proper value 
		// to use.
		private Vector2 GetAxisUiScalars (List<CurveWrapper> curvesWithSameParameterSpace)
		{
			// If none or just one selected curve then use top most rendered curve value
			if (m_Selection.Count <= 1)
			{
				if (m_DrawOrder.Count > 0)
				{
					CurveWrapper cw = getCurveWrapperById(m_DrawOrder[m_DrawOrder.Count - 1]);
					if (cw.getAxisUiScalarsCallback != null)
					{
						// Save list
						if (curvesWithSameParameterSpace != null)
							curvesWithSameParameterSpace.Add(cw);
						return cw.getAxisUiScalarsCallback();
					}
				}
				return Vector2.one;
			}

			// If multiple curves selected we have to check if they are in the same value space
			Vector2 axisUiScalars = new Vector2(-1, -1);
			if (m_Selection.Count > 1)
			{
				// Find common axis scalars if more than one key selected
				bool xAllSame = true;
				bool yAllSame = true;
				Vector2 scalars = Vector2.one;
				for (int i = 0; i < m_Selection.Count; ++i)
				{
					CurveWrapper cw = m_Selection[i].curveWrapper;
					if (cw.getAxisUiScalarsCallback != null)
					{
						Vector2 temp = cw.getAxisUiScalarsCallback();
						if (i == 0)
						{
							scalars = temp; // init scalars
						}
						else
						{
							if (Mathf.Abs(temp.x - scalars.x) > 0.00001f)
								xAllSame = false;
							if (Mathf.Abs(temp.y - scalars.y) > 0.00001f)
								yAllSame = false;
							scalars = temp;
						}
						
						// Save list
						if (curvesWithSameParameterSpace != null)
							curvesWithSameParameterSpace.Add(cw);
					}
				}
				if (xAllSame)
					axisUiScalars.x = scalars.x;
				if (yAllSame)
					axisUiScalars.y = scalars.y;
			}

			return axisUiScalars;
		}


		private void SetAxisUiScalars (Vector2 newScalars, List<CurveWrapper> curvesInSameSpace)
		{
			foreach (CurveWrapper cw in curvesInSameSpace)
			{
				// Only set valid values (-1 indicate invalid value, if so use original value)
				Vector2 scalar = cw.getAxisUiScalarsCallback();
				if (newScalars.x >= 0)
					scalar.x = newScalars.x;
				if (newScalars.y >= 0)
					scalar.y = newScalars.y;

				if (cw.setAxisUiScalarsCallback != null)
					cw.setAxisUiScalarsCallback (scalar);
			}
		}
	
		internal enum PickMode { None, Click, Marquee };	
		
		public void GridGUI ()
		{
			GUI.BeginGroup(drawRect);
			
			if (Event.current.type != EventType.Repaint)
			{
				GUI.EndGroup();
				return;
			}

			InitStyles();

			Color tempCol = GUI.color;

			// Get axis scalars
			Vector2 axisUiScalars = GetAxisUiScalars (null);

			// Cache framed area rect as fetching the property takes some calculations
			Rect shownRect = shownArea;

			hTicks.SetRanges(shownRect.xMin * axisUiScalars.x, shownRect.xMax * axisUiScalars.x, drawRect.xMin, drawRect.xMax);
			vTicks.SetRanges(shownRect.yMin * axisUiScalars.y, shownRect.yMax * axisUiScalars.y, drawRect.yMin, drawRect.yMax);

			// Draw time markers of various strengths
			HandleUtility.handleWireMaterial.SetPass(0);
			GL.Begin(GL.LINES);

			float lineStart, lineEnd;
			
			hTicks.SetTickStrengths(settings.hTickStyle.distMin, settings.hTickStyle.distFull, false);
			if (settings.hTickStyle.stubs)
			{
				lineStart = shownRect.yMin;
				lineEnd = shownRect.yMin - 40 / scale.y;
			}
			else
			{
				lineStart = Mathf.Max(shownRect.yMin, vRangeMin);
				lineEnd = Mathf.Min(shownRect.yMax, vRangeMax);
			}

			for (int l = 0; l < hTicks.tickLevels; l++)
			{
				float strength = hTicks.GetStrengthOfLevel(l);
				if (strength > 0f)
				{
					GL.Color(settings.hTickStyle.color * new Color(1, 1, 1, strength) * new Color(1, 1, 1, 0.75f));
					float[] ticks = hTicks.GetTicksAtLevel(l, true);
					for (int j = 0; j < ticks.Length; j++)
					{
						ticks[j] /= axisUiScalars.x;
						if (ticks[j] > hRangeMin && ticks[j] < hRangeMax)
							DrawLine(new Vector2(ticks[j], lineStart), new Vector2(ticks[j], lineEnd));
					}
				}
			}

			// Draw bounds of allowed range
			GL.Color (settings.hTickStyle.color * new Color(1,1,1,1) * new Color (1, 1, 1, 0.75f));
			if (hRangeMin != Mathf.NegativeInfinity)
				DrawLine(new Vector2 (hRangeMin, lineStart), new Vector2 (hRangeMin, lineEnd));
			if (hRangeMax != Mathf.Infinity)
				DrawLine(new Vector2 (hRangeMax, lineStart), new Vector2 (hRangeMax, lineEnd));

			vTicks.SetTickStrengths(settings.vTickStyle.distMin, settings.vTickStyle.distFull, false);
			if (settings.vTickStyle.stubs)
			{
				lineStart = shownRect.xMin;
				lineEnd = shownRect.xMin + 40 / scale.x;
			}
			else
			{
				lineStart = Mathf.Max(shownRect.xMin, hRangeMin);
				lineEnd = Mathf.Min(shownRect.xMax, hRangeMax);
			}
	
			// Draw value markers of various strengths
			for (int l=0; l<vTicks.tickLevels; l++)
			{
				float strength = vTicks.GetStrengthOfLevel(l);
				if (strength > 0f)
				{
					GL.Color(settings.vTickStyle.color * new Color(1, 1, 1, strength) * new Color(1, 1, 1, 0.75f));
					float[] ticks = vTicks.GetTicksAtLevel(l, true);
					for (int j = 0; j < ticks.Length; j++)
					{
						ticks[j] /= axisUiScalars.y;
						if (ticks[j] > vRangeMin && ticks[j] < vRangeMax)
							DrawLine(new Vector2(lineStart, ticks[j]), new Vector2(lineEnd, ticks[j]));
					}
				}
			}
			// Draw bounds of allowed range
			GL.Color (settings.vTickStyle.color * new Color(1,1,1,1) * new Color (1, 1, 1, 0.75f));
			if (vRangeMin != Mathf.NegativeInfinity)
				DrawLine(new Vector2 (lineStart, vRangeMin), new Vector2 (lineEnd, vRangeMin));
			if (vRangeMax != Mathf.Infinity)
				DrawLine(new Vector2 (lineStart, vRangeMax), new Vector2 (lineEnd, vRangeMax));

			GL.End();


			if (settings.showAxisLabels)
			{
				// X Axis labels
				if (settings.hTickStyle.distLabel > 0 && axisUiScalars.x > 0f)
				{
					GUI.color = settings.hTickStyle.labelColor;
					int labelLevel = hTicks.GetLevelWithMinSeparation(settings.hTickStyle.distLabel);
					
					// Calculate how many decimals are needed to show the differences between the labeled ticks
				int decimals = MathUtils.GetNumberOfDecimalsForMinimumDifference(hTicks.GetPeriodOfLevel(labelLevel));
					
					// now draw
					float[] ticks = hTicks.GetTicksAtLevel(labelLevel, false);
					float[] ticksPos = (float[])ticks.Clone();
					float vpos = Mathf.Floor(drawRect.height);
					for (int i=0; i<ticks.Length; i++)
					{
						ticksPos[i] /= axisUiScalars.x;
						if (ticksPos[i] < hRangeMin || ticksPos[i] > hRangeMax)
							continue;
						Vector2 pos = DrawingToViewTransformPoint(new Vector2 (ticksPos[i], 0));
						// Important to take floor of positions of GUI stuff to get pixel correct alignment of
						// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
						pos = new Vector2(Mathf.Floor(pos.x), vpos);

						float uiValue = ticks[i];
						Rect labelRect;
						TextAnchor wantedAlignment;
						if (settings.hTickStyle.centerLabel)
						{
							wantedAlignment = TextAnchor.UpperCenter;
							labelRect = new Rect(pos.x, pos.y - 16 - settings.hTickLabelOffset, 1, 16);
						}
						else
						{
							wantedAlignment = TextAnchor.UpperLeft;
							labelRect = new Rect(pos.x, pos.y - 16 - settings.hTickLabelOffset, 50, 16);
						}

						if (ms_Styles.labelTickMarksX.alignment != wantedAlignment)
							ms_Styles.labelTickMarksX.alignment = wantedAlignment;

						GUI.Label(labelRect, uiValue.ToString("n" + decimals) + settings.hTickStyle.unit, ms_Styles.labelTickMarksX);
					}
				}

				// Y Axis labels
				if (settings.vTickStyle.distLabel > 0 && axisUiScalars.y > 0f)
				{
					// Draw value labels
					GUI.color = settings.vTickStyle.labelColor;
					int labelLevel = vTicks.GetLevelWithMinSeparation(settings.vTickStyle.distLabel);
					
					float[] ticks = vTicks.GetTicksAtLevel(labelLevel, false);
					float[] ticksPos = (float[])ticks.Clone();

					// Calculate how many decimals are needed to show the differences between the labeled ticks
				int decimals =  MathUtils.GetNumberOfDecimalsForMinimumDifference(vTicks.GetPeriodOfLevel(labelLevel));
					string format = "n" + decimals;
					m_AxisLabelFormat = format;

					// Calculate the size of the biggest shown label
					float labelSize = 35;
					if (!settings.vTickStyle.stubs && ticks.Length > 1)
					{
						float min = ticks[1];
						float max = ticks[ticks.Length - 1];
						string minNumber = min.ToString(format) + settings.vTickStyle.unit;
						string maxNumber = max.ToString(format) + settings.vTickStyle.unit;
						labelSize = Mathf.Max(
							ms_Styles.labelTickMarksY.CalcSize(new GUIContent(minNumber)).x,
							ms_Styles.labelTickMarksY.CalcSize(new GUIContent(maxNumber)).x
						) + 6 ;
					}

					// Now draw
					for (int i = 0; i < ticks.Length; i++)
					{
						ticksPos[i] /= axisUiScalars.y;
						if (ticksPos[i] < vRangeMin || ticksPos[i] > vRangeMax)
							continue;
						Vector2 pos = DrawingToViewTransformPoint(new Vector2(0, ticksPos[i]));
						// Important to take floor of positions of GUI stuff to get pixel correct alignment of
						// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
						pos = new Vector2(pos.x, Mathf.Floor(pos.y));

						float uiValue = ticks[i];
						Rect labelRect;
						if (settings.vTickStyle.centerLabel)
							labelRect = new Rect(0, pos.y - 8, leftmargin -4, 16);  // text expands to the left starting from where grid starts (leftmargin size must ensure text is visible)
						else
							labelRect = new Rect(0, pos.y - 13, labelSize, 16);		// text expands to the right starting from left side of window
							

						GUI.Label(labelRect, uiValue.ToString (format) + settings.vTickStyle.unit, ms_Styles.labelTickMarksY);
					}
				}
			}
			// Cleanup
			GUI.color = tempCol;

			GUI.EndGroup();
		}		
	}
} // namespace
