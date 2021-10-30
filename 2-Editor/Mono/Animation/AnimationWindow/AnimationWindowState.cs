using System;
using System.Reflection;
using System.Linq;
using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Collections;
using Object = UnityEngine.Object;
using Random = UnityEngine.Random;

namespace UnityEditorInternal
{
	[System.Serializable]
	internal class AnimationWindowState
	{
		#region Persistent data
		
		[SerializeField] public AnimationWindowHierarchyState m_hierarchyState; // Persistent state of treeview on the left side of window
		[SerializeField] public AnimationClip m_ActiveAnimationClip;
		[SerializeField] public GameObject m_ActiveGameObject; // Same as selected GO, but persists if everything gets unselected
		[SerializeField] public GameObject m_RootGameObject; // Root GO of the active clip
		[SerializeField] public GameObject m_AnimatedGameObject; // GO that hold the animator component
		[SerializeField] public bool m_AnimationIsPlaying;
		[SerializeField] public bool m_ShowCurveEditor; // Do we show dopesheet or curves
		[SerializeField] public bool m_CurveEditorIsDirty; // Do we need to reload data in curveEditor
		[SerializeField] public float m_PlayTime; // TODO: Unify currentime + playTime + frame
		[SerializeField] public int m_Frame; // TODO: Unify currentime + playTime + frame

		[SerializeField] private Hashtable m_SelectedKeyHashes; // What is selected. Hashes persist cache reload, because they are from keyframe time+value
		[SerializeField] private int m_ActiveKeyframeHash; // Which keyframe is active (selected key that user previously interacted with)
		[SerializeField] private Rect m_ShownTimeArea = new Rect(0, 0, 2, 2);
		[SerializeField] private Rect m_ShownTimeAreaInsideMargins = new Rect (0, 0, 2, 2);

		#endregion

		#region Non Persistent data

		public Action m_OnHierarchySelectionChanged;
		public AnimationWindowHierarchyDataSource m_HierarchyData;		

		// TODO Do it right. This is a hack for add curve buttons EnsureAnimationMode
		public AnimationWindow m_AnimationWindow;

		private List<AnimationWindowCurve> m_AllCurvesCache;
		private List<AnimationWindowCurve> m_ActiveCurvesCache;
		private List<DopeLine> m_dopelinesCache;
		private List<AnimationWindowKeyframe> m_SelectedKeysCache;
		private AnimationWindowKeyframe m_ActiveKeyframeCache;
		private HashSet<int> m_ModifiedCurves = new HashSet<int> ();
		private EditorCurveBinding? m_lastAddedCurveBinding;
		
		// Window owning the animation Window state. Used to trigger repaints.
		private EditorWindow m_Window;
			
		// Hash of all the things that require animationWindow to refresh if they change
		private int m_PreviousRefreshHash;

		// Changing m_Refresh means you are ordering a refresh at the next OnGUI(). 
		// CurvesOnly means that there is no need to refresh the hierarchy, since only the keyframe data changed.
		public enum RefreshType { None = 0, CurvesOnly = 1, Everything = 2 };
		private RefreshType m_Refresh = RefreshType.None;
		
		#endregion

		#region Methods
		
		public void OnGUI ()
		{
			RefreshHashCheck ();
			Refresh ();
		}

		private void RefreshHashCheck()
		{
			int newRefreshHash = GetRefreshHash ();
			if (m_PreviousRefreshHash != newRefreshHash)
			{
				refresh = RefreshType.Everything;
				m_PreviousRefreshHash = newRefreshHash;
			}
		}
		
		// Hash for checking if any of these things is changed
		private int GetRefreshHash()
		{
			return
				(m_ActiveAnimationClip != null ? m_ActiveAnimationClip.GetHashCode() : 0) ^
				(m_RootGameObject != null ? m_RootGameObject.GetHashCode() : 0) ^
				(m_hierarchyState != null ? m_hierarchyState.expandedIDs.Count : 0) ^
				(m_hierarchyState != null ? m_hierarchyState.m_TallInstanceIDs.Count : 0) ^
				(m_ShowCurveEditor ? 1 : 0);
		}

		public void OnEnable (EditorWindow window)
		{
			m_Window = window;
			AnimationUtility.onCurveWasModified += CurveWasModified;
			Undo.undoRedoPerformed += UndoRedoPerformed;
		}
		
		public void OnDisable ()
		{
			m_Window = null;
			AnimationUtility.onCurveWasModified -= CurveWasModified;
			Undo.undoRedoPerformed -= UndoRedoPerformed;
		}

		// Set this property to ask for refresh at the next OnGUI.
		public RefreshType refresh
		{
			get { return m_Refresh; }
			// Make sure that if full refresh is already ordered, nobody gets to f*** with it
			set
			{
				if ((int) m_Refresh < (int) value)
					m_Refresh = value;
			}
		}

		public void UndoRedoPerformed ()
		{
			refresh = RefreshType.Everything;
		}

		// When curve is modified, we never trigger refresh right away. We order a refresh at later time by setting refresh to appropriate value.
		void CurveWasModified (AnimationClip clip, EditorCurveBinding binding, AnimationUtility.CurveModifiedType type)
		{
			// AnimationWindow doesn't care if some other clip somewhere changed
			if (clip != m_ActiveAnimationClip)
				return;
			 
			// Refresh curves that already exist.
			if (type == AnimationUtility.CurveModifiedType.CurveModified)
			{
				bool didFind = false;
				int hashCode = binding.GetHashCode ();
				foreach (AnimationWindowCurve curve in allCurves)
				{
					int curveHash = curve.binding.GetHashCode ();
					if (curveHash == hashCode)
					{
						m_ModifiedCurves.Add(curveHash);
						didFind = true;
					}
				}
				
				if (didFind)
					refresh = RefreshType.CurvesOnly;
				else
				{	
					// New curve was added, so let's save binding and make it active selection when Refresh is called next time
					m_lastAddedCurveBinding = binding;
					refresh = RefreshType.Everything;
				}
			}
			else
			{
				// Otherwise do a full reload
				refresh = RefreshType.Everything;
			}
		}		

		public void SaveCurve (AnimationWindowCurve curve)
		{
			curve.m_Keyframes.Sort ((a, b) => a.time.CompareTo (b.time));

			Undo.RegisterCompleteObjectUndo(m_ActiveAnimationClip, "Edit Curve");
			
			if (curve.isPPtrCurve)
			{
				ObjectReferenceKeyframe[] objectCurve = curve.ToObjectCurve ();

				if (objectCurve.Length == 0)
					objectCurve = null;

				AnimationUtility.SetObjectReferenceCurve (m_ActiveAnimationClip, curve.binding, objectCurve);
			}
			else
			{
				AnimationCurve animationCurve = curve.ToAnimationCurve ();

				if (animationCurve.keys.Length == 0)
					animationCurve = null;
				else
					QuaternionCurveTangentCalculation.UpdateTangentsFromMode(animationCurve, m_ActiveAnimationClip, curve.binding);
				
				AnimationUtility.SetEditorCurve (m_ActiveAnimationClip, curve.binding, animationCurve);
			}
			
			Repaint();
		}

		public void SaveSelectedKeys(List<AnimationWindowKeyframe> currentSelectedKeys)
		{
			List<AnimationWindowCurve> saveCurves = new List<AnimationWindowCurve> ();

			// Find all curves that need saving
			foreach (AnimationWindowKeyframe selectedKeyFrame in currentSelectedKeys)
			{
				if (!saveCurves.Contains (selectedKeyFrame.curve))
					saveCurves.Add (selectedKeyFrame.curve);

				List<AnimationWindowKeyframe> toBeDeleted = new List<AnimationWindowKeyframe> ();

				// If selected keys are dragged over non-selected keyframe at exact same time, then delete the unselected ones underneath
				foreach (AnimationWindowKeyframe other in selectedKeyFrame.curve.m_Keyframes)
					if (!currentSelectedKeys.Contains (other) && AnimationKeyTime.Time (selectedKeyFrame.time, frameRate).frame == AnimationKeyTime.Time (other.time, frameRate).frame)
						toBeDeleted.Add (other);

				foreach (AnimationWindowKeyframe deletedKey in toBeDeleted)
					selectedKeyFrame.curve.m_Keyframes.Remove (deletedKey);
			}

			foreach (AnimationWindowCurve curve in saveCurves)
				SaveCurve (curve);
		}

		public void RemoveCurve (AnimationWindowCurve curve)
		{
			Undo.RegisterCompleteObjectUndo(m_ActiveAnimationClip, "Remove Curve");
			
			if (curve.isPPtrCurve)
				AnimationUtility.SetObjectReferenceCurve (m_ActiveAnimationClip, curve.binding, null);
			else
				AnimationUtility.SetEditorCurve (m_ActiveAnimationClip, curve.binding, null);
		}

		public List<AnimationWindowCurve> allCurves
		{
			get
			{
				if (m_AllCurvesCache == null)
				{
					m_AllCurvesCache = new List<AnimationWindowCurve> ();
					if (m_ActiveAnimationClip != null && m_ActiveGameObject != null)
					{
						EditorCurveBinding[] curveBindings = AnimationUtility.GetCurveBindings (m_ActiveAnimationClip);
						EditorCurveBinding[] objectCurveBindings = AnimationUtility.GetObjectReferenceCurveBindings (m_ActiveAnimationClip);

						foreach (EditorCurveBinding curveBinding in curveBindings)
						{
							if (AnimationWindowUtility.ShouldShowAnimationWindowCurve (curveBinding))
								m_AllCurvesCache.Add (new AnimationWindowCurve (m_ActiveAnimationClip, curveBinding, AnimationUtility.GetEditorCurveValueType (m_RootGameObject, curveBinding)));
						}

						foreach (EditorCurveBinding curveBinding in objectCurveBindings)
							m_AllCurvesCache.Add (new AnimationWindowCurve (m_ActiveAnimationClip, curveBinding, AnimationUtility.GetEditorCurveValueType (m_RootGameObject, curveBinding)));

						// Curves need to be sorted with path/type/property name so it's possible to construct hierarchy from them
						// Sorting logic in AnimationWindowCurve.CompareTo()
						m_AllCurvesCache.Sort();
					}
				}

				return m_AllCurvesCache;
			}
		}
		
		public List<AnimationWindowCurve> activeCurves
		{
			get
			{
				if (m_ActiveCurvesCache == null)
				{
					m_ActiveCurvesCache = new List<AnimationWindowCurve> ();
					if (m_hierarchyState != null && m_HierarchyData != null)
					{
						foreach (int id in m_hierarchyState.selectedIDs)
						{
							TreeViewItem node = m_HierarchyData.FindItem (id);
							AnimationWindowHierarchyNode hierarchyNode = node as AnimationWindowHierarchyNode;

							if (hierarchyNode == null)
								continue;

							AnimationWindowCurve[] curves = GetCurves (hierarchyNode, true);

							foreach (AnimationWindowCurve curve in curves)
								if (!m_ActiveCurvesCache.Contains (curve))
									m_ActiveCurvesCache.Add (curve);
						}
					}
				}

				return m_ActiveCurvesCache;
			}
		}

		public List<DopeLine> dopelines
		{
			get 
			{
				if (m_dopelinesCache == null)
				{
					m_dopelinesCache = new List<DopeLine> ();

					if (m_HierarchyData != null)
					{
						foreach (TreeViewItem node in m_HierarchyData.GetVisibleRows ())
						{
							AnimationWindowHierarchyNode hierarchyNode = node as AnimationWindowHierarchyNode;

							if (hierarchyNode == null || hierarchyNode is AnimationWindowHierarchyAddButtonNode)
								continue;
							
							AnimationWindowCurve[] curves;

							if (node is AnimationWindowHierarchyMasterNode)
								curves = allCurves.ToArray();						
							else
								curves = GetCurves (hierarchyNode, true);
							
							DopeLine dopeLine = new DopeLine (node.id, curves);
							dopeLine.tallMode = m_hierarchyState.getTallMode (hierarchyNode);
							dopeLine.objectType = hierarchyNode.animatableObjectType;
							dopeLine.hasChildren = !(hierarchyNode is AnimationWindowHierarchyPropertyNode);
							dopeLine.isMasterDopeline = node is AnimationWindowHierarchyMasterNode;
							m_dopelinesCache.Add (dopeLine);
						}
					}
				}

				return m_dopelinesCache;
			}
		}
		
		public List<AnimationWindowHierarchyNode> selectedHierarchyNodes
		{
			get
			{
				List<AnimationWindowHierarchyNode> selectedHierarchyNodes = new List<AnimationWindowHierarchyNode> ();

				if (m_HierarchyData != null)
				{
					foreach (int id in m_hierarchyState.selectedIDs)
					{
						AnimationWindowHierarchyNode hierarchyNode = (AnimationWindowHierarchyNode) m_HierarchyData.FindItem (id);

						if (hierarchyNode == null || hierarchyNode is AnimationWindowHierarchyAddButtonNode)
							continue;

						selectedHierarchyNodes.Add (hierarchyNode);
					}
				}

				return selectedHierarchyNodes;
			}
		}

		public AnimationWindowKeyframe activeKeyframe
		{
			get
			{
				if (m_ActiveKeyframeCache == null)
				{
					foreach (AnimationWindowCurve curve in allCurves)
					{
						foreach (AnimationWindowKeyframe keyframe in curve.m_Keyframes)
						{
							if (keyframe.GetHash () == m_ActiveKeyframeHash)
								m_ActiveKeyframeCache = keyframe;
						}
					}
				}
				return m_ActiveKeyframeCache;
			}
			set 
			{ 
				m_ActiveKeyframeCache = null;
				m_ActiveKeyframeHash = value != null ? value.GetHash () : 0;
			}
		}

		public List<AnimationWindowKeyframe> selectedKeys
		{
			get
			{
				if (m_SelectedKeysCache == null)
				{
					m_SelectedKeysCache = new List<AnimationWindowKeyframe> ();
					foreach (AnimationWindowCurve curve in allCurves)
					{
						foreach (AnimationWindowKeyframe keyframe in curve.m_Keyframes)
						{
							if (KeyIsSelected (keyframe))
							{
								m_SelectedKeysCache.Add (keyframe);
							}
						}
					}
				}
				return m_SelectedKeysCache;
			}
		}
		
		private Hashtable selectedKeyHashes
		{
			get { return m_SelectedKeyHashes ?? (m_SelectedKeyHashes = new Hashtable ()); }
			set { m_SelectedKeyHashes = value; }
		}

		public bool AnyKeyIsSelected (DopeLine dopeline)
		{
			foreach (AnimationWindowKeyframe keyframe in dopeline.keys)
				if (KeyIsSelected (keyframe))
					return true;

			return false;
		}

		public bool KeyIsSelected(AnimationWindowKeyframe keyframe)
		{
			return selectedKeyHashes.Contains (keyframe.GetHash ());
		}

		public void SelectKey(AnimationWindowKeyframe keyframe)
		{
			int hash = keyframe.GetHash ();
			if(!selectedKeyHashes.Contains (hash))
				selectedKeyHashes.Add (hash, hash);

			m_SelectedKeysCache = null;
		}

		public void UnselectKey(AnimationWindowKeyframe keyframe)
		{
			int hash = keyframe.GetHash ();
			if (selectedKeyHashes.Contains (hash))
				selectedKeyHashes.Remove(hash);

			m_SelectedKeysCache = null;
		}

		public void DeleteSelectedKeys ()
		{
			if (selectedKeys.Count == 0)
				return;

			foreach (AnimationWindowKeyframe keyframe in selectedKeys)
			{
				UnselectKey (keyframe);
				keyframe.curve.m_Keyframes.Remove (keyframe);

				// TODO: optimize by not saving curve for each keyframe
				SaveCurve (keyframe.curve);
			}
		}

		public void MoveSelectedKeys(float deltaTime)
		{
			MoveSelectedKeys (deltaTime, false);
		}

		public void MoveSelectedKeys (float deltaTime, bool snapToFrame)
		{
			MoveSelectedKeys (deltaTime, snapToFrame, true);
		}

		public void MoveSelectedKeys(float deltaTime, bool snapToFrame, bool saveToClip)
		{
			// Let's take snapshot of selected keys. We can't trust on hashes stuff during this operation
			List<AnimationWindowKeyframe> oldSelected = new List<AnimationWindowKeyframe> (selectedKeys);
			
			foreach (AnimationWindowKeyframe keyframe in oldSelected)
			{
				keyframe.time += deltaTime;
				if (snapToFrame)
					keyframe.time = SnapToFrame (keyframe.time, !saveToClip);
			}
			
			if (saveToClip)
				SaveSelectedKeys (oldSelected);

			// Clear selections since all hashes are now different
			ClearKeySelections ();
			// Reselect keys with new hashes
			foreach (AnimationWindowKeyframe keyframe in oldSelected)
				SelectKey (keyframe);
		}

		public void ClearKeySelections()
		{
			selectedKeyHashes.Clear ();
			m_SelectedKeysCache = null;
		}

		private void ReloadModifiedDopelineCache ()
		{
			if (m_dopelinesCache == null)
				return;
			foreach (DopeLine dopeLine in m_dopelinesCache)
			{
				foreach (var curve in dopeLine.m_Curves)
				{
					if (m_ModifiedCurves.Contains (curve.binding.GetHashCode ()))
						dopeLine.LoadKeyframes();
				}
			}
		}
		
		private void ReloadModifiedAnimationCurveCache ()
		{
			if (m_AllCurvesCache == null)
				return;
			foreach (AnimationWindowCurve curve in m_AllCurvesCache)
			{
				if (m_ModifiedCurves.Contains (curve.binding.GetHashCode ()))
					curve.LoadKeyframes (m_ActiveAnimationClip);
			}
		}
		
		private void Refresh ()
		{
			if (refresh == RefreshType.Everything)
			{
				CurveRendererCache.ClearCurveRendererCache ();
				m_ActiveKeyframeCache = null;
				m_AllCurvesCache = null;
				m_ActiveCurvesCache = null;
				m_CurveEditorIsDirty = true;
				m_dopelinesCache = null;
				m_SelectedKeysCache = null;

				if (refresh == RefreshType.Everything && m_HierarchyData != null)
					m_HierarchyData.UpdateData ();

				// If there was new curve added, set it as active selection
				if (m_lastAddedCurveBinding != null)
					OnNewCurveAdded ((EditorCurveBinding)m_lastAddedCurveBinding);

				// select top dopeline if there is no selection available
				if (activeCurves.Count == 0 && dopelines.Count > 0)
					SelectHierarchyItem (dopelines[0], false, false);

				m_Refresh = RefreshType.None;
			}
			else if (refresh == RefreshType.CurvesOnly)
			{
				m_ActiveKeyframeCache = null;
				m_ActiveCurvesCache = null;
				m_SelectedKeysCache = null;

				ReloadModifiedAnimationCurveCache ();
				ReloadModifiedDopelineCache ();
			
				CurveRendererCache.ClearCurveRendererCache ();
				m_CurveEditorIsDirty = true;
				m_Refresh = RefreshType.None;
				m_ModifiedCurves.Clear();
			}
		}

		// This is called when there is a new curve, but after the data refresh. 
		// This means that hierarchynodes and dopeline(s) for new curve are already available.
		private void OnNewCurveAdded (EditorCurveBinding newCurve)
		{
			string propertyName = AnimationWindowUtility.GetPropertyGroupName (newCurve.propertyName);
			
			// This is the ID of the AnimationWindowHierarchyNode that represents the new group.
			// For example if we got position.z as our newCurve, this ID will point to the node for entire position, which has 3 child nodes x,y,z
			int groupNodeID = AnimationWindowUtility.GetPropertyNodeID (newCurve.path, newCurve.type, propertyName);

			SelectHierarchyItem (groupNodeID, false, false);

			// We want the pptr curves to be in tall mode by default
			if(newCurve.isPPtrCurve)
				m_hierarchyState.m_TallInstanceIDs.Add (groupNodeID);

			m_lastAddedCurveBinding = null;
		}

		public void Repaint ()
		{
			if (m_Window != null)
				m_Window.Repaint();
		}
		
		public AnimationWindowCurve[] GetCurves (AnimationWindowHierarchyNode hierarchyNode, bool entireHierarchy)
		{
			return AnimationWindowUtility.FilterCurves (allCurves.ToArray (), hierarchyNode.path, hierarchyNode.animatableObjectType, hierarchyNode.propertyName);
		}
		
		public List<AnimationWindowKeyframe> GetAggregateKeys (AnimationWindowHierarchyNode hierarchyNode)
		{
			DopeLine dopeline = dopelines.FirstOrDefault (e => e.m_HierarchyNodeID == hierarchyNode.id);
			if (dopeline == null)
				return null;
			return dopeline.keys;
		}

		public void OnHierarchySelectionChanged (int[] selectedInstanceIDs)
		{
			HandleHierarchySelectionChanged (selectedInstanceIDs, true);
			m_OnHierarchySelectionChanged ();
		}

		public void HandleHierarchySelectionChanged (int[] selectedInstanceIDs, bool triggerSceneSelectionSync)
		{
			m_CurveEditorIsDirty = true;
			m_ActiveCurvesCache = null;
			
			if (triggerSceneSelectionSync)
				SyncSceneSelection (selectedInstanceIDs);
		}

		public void SelectHierarchyItem (DopeLine dopeline, bool additive)
		{
			SelectHierarchyItem (dopeline.m_HierarchyNodeID, additive, true);
		}

		public void SelectHierarchyItem (DopeLine dopeline, bool additive, bool triggerSceneSelectionSync)
		{
			SelectHierarchyItem (dopeline.m_HierarchyNodeID, additive, triggerSceneSelectionSync);
		}

		public void SelectHierarchyItem (int hierarchyNodeID, bool additive, bool triggerSceneSelectionSync)
		{
			if (!additive)
				ClearHierarchySelection ();

			m_hierarchyState.selectedIDs.Add (hierarchyNodeID);

			int[] selectedInstanceIDs = m_hierarchyState.selectedIDs.ToArray ();

			// We need to manually trigger this event, because injecting data to m_SelectedInstanceIDs directly doesn't trigger one via TreeView
			HandleHierarchySelectionChanged (selectedInstanceIDs, true);
		}

		public void UnSelectHierarchyItem (DopeLine dopeline)
		{
			UnSelectHierarchyItem (dopeline.m_HierarchyNodeID);
		}

		public void UnSelectHierarchyItem (int hierarchyNodeID)
		{
			m_hierarchyState.selectedIDs.Remove (hierarchyNodeID);
		}

		public void ClearHierarchySelection ()
		{
			m_hierarchyState.selectedIDs.Clear ();
		}

		// Set scene active go to be the same as the one selected from hierarchy
		private void SyncSceneSelection (int[] selectedNodeIDs)
		{
			List<int> selectedGameObjectIDs = new List<int> ();

			foreach (var selectedNodeID in selectedNodeIDs)
			{
				AnimationWindowHierarchyNode node = m_HierarchyData.FindItem (selectedNodeID) as AnimationWindowHierarchyNode;

				if (m_RootGameObject == null || node == null)
					continue;

				if (node is AnimationWindowHierarchyMasterNode)
					continue;

				Transform t = m_RootGameObject.transform.Find (node.path);

				// In the case of nested animation component, we don't want to sync the scene selection (case 569506)
				// When selection changes, animation window will always pick nearest animator component in terms of hierarchy depth
				// Automatically syncinc scene selection in nested scenarios would cause unintuitive clip & animation change for animation window so we check for it and deny sync if necessary
			
				if (t != null && m_RootGameObject != null && m_RootGameObject.transform == AnimationWindowUtility.GetClosestAnimationComponentInParents (t))
					selectedGameObjectIDs.Add (t.gameObject.GetInstanceID());
			}

			Selection.instanceIDs = selectedGameObjectIDs.ToArray();
		}

		public bool IsReadOnly
		{
			get
			{
				if (!m_ActiveAnimationClip)
					return true;

				if (!IsEditable)
					return true;				

				return false;
			}
		}

		public bool IsEditable
		{
			get
			{
				if (!m_ActiveGameObject)
					return false;
				
				// Clip is imported and shouldn't be edited
				if (m_ActiveAnimationClip && (m_ActiveAnimationClip.hideFlags & HideFlags.NotEditable) != 0)
					return false;

				// Object is a prefab - shouldn't be edited	
				if (IsPrefab)
					return false;

				return true;
			}
		}

		public bool IsClipEditable
		{
			get
			{
				if (!m_ActiveAnimationClip)
					return false;
				// Clip is imported and shouldn't be edited
				if ((m_ActiveAnimationClip.hideFlags & HideFlags.NotEditable) != 0)
					return false;
				if (!AssetDatabase.IsOpenForEdit (m_ActiveAnimationClip))
					return false;

				return true;
			}
		}

		public bool IsPrefab
		{
			get
			{
				// No gameObject selected
				if (!m_ActiveGameObject)
					return false;

				if (EditorUtility.IsPersistent (m_ActiveGameObject))
					return true;

				if ((m_ActiveGameObject.hideFlags & HideFlags.NotEditable) != 0)
					return true;

				return false;
			}
		}

		// Is the hierarchy in animator optimized
		public bool AnimatorIsOptimized
		{
			get
			{
				// No gameObject selected
				if (!m_RootGameObject)
					return false;

				Animator animator = m_RootGameObject.GetComponent<Animator> ();
				
				if (animator != null)
					return animator.isOptimizable && !animator.hasTransformHierarchy;
				else
					return false;
			}
		}

		public float frameRate
		{
			get
			{
				if (m_ActiveAnimationClip == null)
					return 60;
				return m_ActiveAnimationClip.frameRate;
			}
			set
			{
				// @TODO: Changing the clip in AnimationWindowState.frame rate feels a bit intrusive
				// Should probably be done explicitly from the UI and not go through AnimationWindowState...
				if (m_ActiveAnimationClip != null && value > 0 && value <= 10000)
				{
					// Reposition all keyframes to match the new sampling rate
					foreach (var curve in allCurves)
					{
						foreach (var key in curve.m_Keyframes)
						{
							int frame = AnimationKeyTime.Time (key.time, frameRate).frame;
							key.time = AnimationKeyTime.Frame (frame, value).time;
						}
						SaveCurve (curve);
					}

					m_ActiveAnimationClip.frameRate = value;
					m_CurveEditorIsDirty = true;
				}
			}
		}
		
		#endregion

		#region Time utilities 

		private TimeArea m_timeArea;
		public TimeArea timeArea {
			get { return m_timeArea; }
			set
			{
				if (value != m_timeArea && value != null)
				{
					value.SetShownHRangeInsideMargins (m_ShownTimeAreaInsideMargins.xMin, m_ShownTimeAreaInsideMargins.xMax);
				}

				m_timeArea = value;

				if (m_timeArea != null)
				{
					m_ShownTimeAreaInsideMargins = m_timeArea.shownAreaInsideMargins;
					m_ShownTimeArea = m_timeArea.shownArea;
				}
			}
		}

		// Pixel to time ratio (used for time-pixel conversions)
		public float pixelPerSecond
		{
			get { return timeArea.m_Scale.x; }
		}

		// The GUI x-coordinate, where time==0 (used for time-pixel conversions)
		public float zeroTimePixel 
		{
			get { return timeArea.shownArea.xMin * timeArea.m_Scale.x * -1f; }
		}

		public float PixelToTime (float pixel)
		{
			return PixelToTime (pixel, false);
		}

		public float PixelToTime (float pixel, bool snapToFrame)
		{
			float time = pixel - zeroTimePixel;
			if (snapToFrame)
				return SnapToFrame (time / pixelPerSecond);

			return time / pixelPerSecond;
		}

		public float TimeToPixel (float time)
		{
			return TimeToPixel (time, false);
		}

		public float TimeToPixel (float time, bool snapToFrame)
		{
			return (snapToFrame ? SnapToFrame(time) : time) * pixelPerSecond + zeroTimePixel;
		}

		//@TODO: Move to animatkeytime??
		public float SnapToFrame(float time)
		{
			return Mathf.Round (time * frameRate) / frameRate;
		}
		public float SnapToFrame(float time, bool preventHashCollision)
		{
			// Adds small fraction of offset to prevent hash collision while dragging
			if (preventHashCollision)
				return Mathf.Round (time * frameRate) / frameRate + 0.01f / frameRate;
			else
				return SnapToFrame (time);
		}

		#endregion

		#region Keyframe and Time related Utilities (legacy)

		public string FormatFrame (int frame, int frameDigits)
		{
			return (frame / (int)frameRate).ToString () + ":" + (frame % frameRate).ToString ().PadLeft (frameDigits, '0');
		}

		public float minTime
		{
			get
			{
				return m_ShownTimeArea.xMin;
			}
		}

		public float maxTime
		{
			get
			{
				return m_ShownTimeArea.xMax;
			}
		}

		public float timeSpan
		{
			get { return maxTime - minTime; }
		}

		public float minFrame
		{
			get { return minTime * frameRate; }
		}

		public float maxFrame
		{
			get { return maxTime * frameRate; }
		}

		public float frameSpan
		{
			get { return timeSpan * frameRate; }
		}

		public AnimationKeyTime time
		{
			get { return AnimationKeyTime.Frame(m_Frame, frameRate); }
		}

		//@TODO: Remove. Replace with animationkeytime
		public float TimeToFrame (float time)
		{
			return time * frameRate;
		}

		//@TODO: Remove. Replace with animationkeytime
		public float FrameToTime (float frame)
		{
			return frame / frameRate;
		}

		// TODO: This is weird? Was it supposed to return int?
		public float FrameToTimeFloor (float frame)
		{
			return (frame - 0.5f) / frameRate;
		}

		// TODO: This is weird? Was it supposed to return int?
		public float FrameToTimeCeiling (float frame)
		{
			return (frame + 0.5f) / frameRate;
		}

		public int TimeToFrameFloor (float time)
		{
			return Mathf.FloorToInt (TimeToFrame (time));
		}

		public int TimeToFrameRound (float time)
		{
			return Mathf.RoundToInt(TimeToFrame(time));
		}

		public float GetTimeSeconds ()
		{
			if (m_AnimationIsPlaying)
				return m_PlayTime;
			else
				return FrameToTime (m_Frame);
		}

		public float FrameToPixel (float i, Rect rect)
		{
			return (i - minFrame) * rect.width / frameSpan;
		}

		public float FrameDeltaToPixel (Rect rect)
		{
			return rect.width / frameSpan;
		}

		public float TimeToPixel (float time, Rect rect)
		{
			return FrameToPixel (time * frameRate, rect);
		}

		public float PixelToTime (float pixelX, Rect rect)
		{
			return (pixelX * timeSpan / rect.width + minTime);
		}

		public float PixelDeltaToTime (Rect rect)
		{
			return timeSpan / rect.width;
		}

		public float SnapTimeToWholeFPS (float time)
		{
			return Mathf.Round (time * frameRate) / frameRate;
		}

		#endregion
	}
}
