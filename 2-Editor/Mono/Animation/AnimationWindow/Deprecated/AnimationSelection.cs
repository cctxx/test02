#pragma warning disable 649

using System;
using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;
using Object = UnityEngine.Object;

namespace UnityEditor
{

internal class AnimationHierarchyData
{
	public AnimationWindow animationWindow;
	public AnimationSelection animationSelection;
	
	public SerializedStringTable expandedFoldouts { get { return animationWindow.expandedFoldouts; } set { animationWindow.expandedFoldouts = value; } }
	public bool showAllProperties { get { return animationWindow.showAllProperties; } }
	
	public GameObject animated { get { return animationSelection.animatedObject; } }
	public AnimationClip clip { get { return animationSelection.clip; } }
	public Hashtable animatedCurves { get { return animationSelection.animatedCurves; } }
	public Hashtable leftoverCurves { get { return animationSelection.leftoverCurves; } }
	public Hashtable animatedPaths { get { return animationSelection.animatedPaths; } }
	
	public FoldoutObjectState[] states;
}

[System.Serializable]
internal class AnimationSelection
{
	// Reference to AnimationWindow
	private AnimationWindow m_AnimationWindow;
	public AnimationWindow animationWindow { get { return m_AnimationWindow; } }
	
	// chosen object with animation component on
	private int m_AnimatedObjectIndex;
	public int animatedObjectIndex { get { return m_AnimatedObjectIndex; } }
    public GameObject animatedObject { get { return animatedOptions[m_AnimatedObjectIndex]; } }

    public GameObject avatarRootObject
    {
        get
        {            
            if (animatedObject)
            {
                Animator animator = animatedObject.GetComponent<Animator>();
                if (animator)
                {
                    Transform avatarRoot = animator.avatarRoot;
                    if (avatarRoot)
                        return avatarRoot.gameObject;
                }
            }

            return animatedObject;
        }
    }
	
	// possible objects with animation components
	private GameObject[] m_AnimatedOptions;
	public GameObject[] animatedOptions { get { return m_AnimatedOptions; } }
	
	public bool hasAnimationComponent
	{
		get
		{
			return animatedObject.GetComponent<Animation>() != null || animatedObject.GetComponent<Animator>() != null;
		}
	}
	
	private AnimationClip m_Clip;
	public AnimationClip clip { get { return m_Clip; } }
	public int clipIndex { get { return GetIndexOfClip(m_Clip); }}
	public bool CanClipAndGameObjectBeEdited
	{ get {
		if (m_Clip == null)
			return false;
		// Clip is imported and shouldn't be edited
		if ((m_Clip.hideFlags & HideFlags.NotEditable) != 0)
			return false;
		return GameObjectIsAnimatable;
	} }
	
	public bool GameObjectIsAnimatable
	{ get {
		if (animatedObject == null)
			return false;
		// Object is not editable
		if ((animatedObject.hideFlags & HideFlags.NotEditable) != 0)
			return false;
		// Object is a prefab - shouldn't be edited	
		if (EditorUtility.IsPersistent (animatedObject))
			return false;
			
		return true;
	} }
	
	private bool m_ClipCanceled = false;
	
	private FoldoutTree[] m_Trees = new FoldoutTree[0];
	public FoldoutTree[] trees { get { return m_Trees; } }
	
	public GameObject ShownRoot ()
	{
		if (trees.Length < 1)
			return null;
		return trees[0].rootGameObject;
	}
	
	private Hashtable m_AnimatedCurves;
	public Hashtable animatedCurves { get {
		if (m_AnimatedCurves == null)
			SetAnimationCurves();
		return m_AnimatedCurves;
	} }
	private Hashtable m_AnimatedPaths;
	public Hashtable animatedPaths { get {
		if (m_AnimatedPaths == null)
			SetAnimationCurves();
		return m_AnimatedPaths;
	} }
	private Hashtable m_LeftoverCurves;
	public Hashtable leftoverCurves { get {
		return m_LeftoverCurves;
	} }
	
	private AnimationClip GetClipAtIndex (int index)
	{
		if (hasAnimationComponent)
		{
			AnimationClip[] clips = AnimationUtility.GetAnimationClips(animatedObject);
			if (index == -1)
				return null; // new clip
			if (index >= clips.Length)
				return null; // new clip
			return clips[index];
		}
		return null; // new clip
	}
	
	private int GetIndexOfClip (AnimationClip clip)
	{
		if (hasAnimationComponent)
		{
			AnimationClip[] clips = AnimationUtility.GetAnimationClips(animatedObject);
			for (int i=0; i<clips.Length; i++)
			{
				if (clips[i] == clip) return i;
			}
		}
		return -1; // new clip
	}
	
	public AnimationSelection (GameObject[] animatedOptions, SerializedStringTable chosenAnimated, SerializedStringTable chosenClip, AnimationWindow editor)
	{
		m_AnimationWindow = editor;
		
		m_AnimatedOptions = animatedOptions;
		
		// Set which of the possible animated objects is chosen
		string animatedOptionsHash = GetStringArrayHashCode(GetAnimatedObjectNames());
		if (!chosenAnimated.Contains(animatedOptionsHash))
			chosenAnimated.Set(animatedOptionsHash, animatedOptions.Length-1);
		
		m_AnimatedObjectIndex = chosenAnimated.Get(animatedOptionsHash);
		
		RefreshChosenClip(chosenClip);
	}
	
	// Note: SetAnimationCurves must be called at some point after this method.
	// It is not called inside this method because it would then be impossible to avoid it being called twice.
	private void RefreshChosenClip (SerializedStringTable chosenClip)
	{
		if (hasAnimationComponent)
		{
			// Set which of the possible animation clips is chosen
			string clipsHash = GetStringArrayHashCode(GetClipNames());
			if (!chosenClip.Contains(clipsHash))
			{
				m_Clip = null;
				// Chose animation component clip if present
				AnimationClip[] clips = AnimationUtility.GetAnimationClips(animatedObject);
				for (int i=0; i<clips.Length; i++)
				{
					if (clips[i] != null)
					{
						m_Clip = clips[i];
						break;
					}
				}
			}
			else
			{
				m_Clip = GetClipAtIndex(chosenClip.Get(clipsHash));
			}
		}
	}
	
	private void SetAnimationCurves ()
	{
		m_AnimatedCurves = new Hashtable();
		m_AnimatedPaths = new Hashtable();
		m_LeftoverCurves = new Hashtable();
		if (clip != null)
		{
			// Add all the animated curves to a hashtable
			EditorCurveBinding[] curves = AnimationUtility.GetCurveBindings (clip);
			foreach (EditorCurveBinding cd in curves)
			{
				int id = CurveUtility.GetCurveID(clip, cd);
				m_AnimatedCurves[id] = true;
				if (!CheckIfPropertyExists(cd))
					m_LeftoverCurves[cd] = cd;
				else
				{
					// hash code for component
					m_AnimatedPaths[CurveUtility.GetPathAndTypeID(cd.path, cd.type)] = true;
					
					// hash codes for all parts of path up to the root
					// but stop as soon as one is encountered that has previously been added 
					string str = cd.path;
					while (true)
					{
						int hash = str.GetHashCode();
						if (m_AnimatedPaths.Contains(hash))
							break;
						
						m_AnimatedPaths[hash] = true;
						if (str.Length == 0)
							break;
						
						int index = str.LastIndexOf('/');
						if (index > 0)
							str = str.Substring(0, index);
						else
							str = "";
					}
				}
			}
		}
	}
	
	private bool CheckIfPropertyExists (EditorCurveBinding data)
	{
		return AnimationUtility.GetEditorCurveValueType(animatedObject, data) != null;
	}
	
	public void Refresh ()
	{
		SetAnimationCurves();
		
		foreach (FoldoutTree tree in trees)
		{
			tree.Refresh(GetAnimationHierarchyData(tree));
		}
	}
	
	public static void OnGUISelection (AnimationSelection animSel)
	{
		if (animSel == null)
		{
			bool enabledTemp = GUI.enabled;
			GUI.enabled = false;
			GUILayout.Label ("", EditorStyles.toolbarPopup);
			GUI.enabled = enabledTemp;
			return;
		}
		
		animSel.m_ClipCanceled = false;
				
		bool changedSelections = false;
				
		// Animation Clip dropdown selection item names list
		string[] clipNames = animSel.GetClipMenuContent ();
		// Animation Clip dropdown selection
		int curClipIndex = animSel.clipIndex;
		int newClipIndex = EditorGUILayout.Popup (curClipIndex, clipNames, EditorStyles.toolbarPopup);
		if (newClipIndex != curClipIndex)
		{
			AnimationClip newClip = animSel.GetClipAtIndex(newClipIndex);
			
			// Let user save new clip (newClip will remain null if user cancels save dialog)
			if (newClip == null)
				newClip = animSel.CreateNewClip();
			
			if (newClip != null)
			{
				animSel.ChooseClip(newClip);
				changedSelections = true;
			}
		}
		
		if (changedSelections)
		{
			GUI.changed = true;
			animSel.animationWindow.ReEnterAnimationMode();
			animSel.Refresh();
		}
	}
	
	public static string GetStringArrayHashCode (string[] array)
	{
		string hash = "";
		foreach (string str in array)
		{
			hash += "|" + str;
		}
		return hash;
	}
	
	public static string GetObjectListHashCode (GameObject[] animatedOptions)
	{
		string[] animatedObjectNames = new string[animatedOptions.Length];
		for (int i=0; i<animatedObjectNames.Length; i++) animatedObjectNames[i] = animatedOptions[i].name;
		return GetStringArrayHashCode(animatedObjectNames);
	}
	
	public string[] GetClipNames ()
	{
		string[] clipNames;
		if (hasAnimationComponent)
		{
			AnimationClip[] clips = AnimationUtility.GetAnimationClips(animatedObject);
			
			clipNames = new string[clips.Length];
			
			for (int i=0; i<clips.Length; i++)
				clipNames[i] = CurveUtility.GetClipName(clips[i]);
		}
		else
			clipNames = new string[0];

		return clipNames;
	}

	public string[] GetClipMenuContent ()
	{
		string[] content;
		if (hasAnimationComponent)
		{
			string[] clipNames = GetClipNames ();

			content = animationWindow.state.IsEditable ? new string[clipNames.Length + 2] : new string[clipNames.Length];

			for (int i = 0; i < clipNames.Length; i++)
				content[i] = clipNames[i];
		}
		else
			content = new string[1];

		if (animationWindow.state.IsEditable)
			content[content.Length - 1] = "[Create New Clip]";

		return content;
	}
	
	public string[] GetAnimatedObjectNames ()
	{
		string[] objects = new string[animatedOptions.Length];
		for (int i=0; i<objects.Length; i++) objects[i] = animatedOptions[i].name;
		return objects;
	}
	
	public AnimationHierarchyData GetAnimationHierarchyData (FoldoutTree tree)
	{
		AnimationHierarchyData data = new AnimationHierarchyData();
		data.animationWindow = m_AnimationWindow;
		data.animationSelection = this;
		data.states = tree.states;
		return data;
	}
	
	// Add this foldout tree to animation selection
	public void AddTree (FoldoutTree tree)
	{
		// TODO Find more efficient approach than resizing array
		System.Array.Resize<FoldoutTree>(ref m_Trees, m_Trees.Length+1);
		trees[trees.Length-1] = tree;
	}
	
	// Make sure a clip is selected, and make sure it gets added if not.
	// Returns false if clip is not present and could not be added.
	public bool EnsureClipPresence ()
	{
		if (clip == null)
		{
			if (m_ClipCanceled)
				return false;

			// Somehow EditorGUIUtility.s_EditorScreenPointOffset gets screwed by SaveFilePanelInProject dialogs but it's not obvious where and why
			// TODO: Fix this problem where it should be fixed
			Vector2 oldOffset = EditorGUIUtility.s_EditorScreenPointOffset;
			AnimationClip newClip = CreateNewClip();
			EditorGUIUtility.s_EditorScreenPointOffset = oldOffset;

			if (newClip != null)
			{
				ChooseClip(newClip);
				Refresh();
			}
			else
			{
				GUIUtility.keyboardControl = 0;
				GUIUtility.hotControl = 0;
				m_ClipCanceled = true;
				return false;
			}
		}
		return true;
	}
	
	internal static AnimationClip AllocateAndSetupClip (bool useAnimator)
	{
		// At this point we know that we can create a clip
		AnimationClip newClip = new AnimationClip();
		if (useAnimator)
		{
			AnimationClipSettings info = AnimationUtility.GetAnimationClipSettings (newClip);
			info.loopTime = true;
			AnimationUtility.SetAnimationClipSettingsNoDirty (newClip, info);

			AnimationUtility.SetAnimationType(newClip, ModelImporterAnimationType.Generic);
		}
		return newClip;
	}


	// Make a new clip, making sure that an Animation component is added if needed.
	// Will return null if clip could not be made.
	private AnimationClip CreateNewClip ()
	{
		bool useAnimator = animatedObject.GetComponent<Animator> () != null || animatedObject.GetComponent<Animation> () == null;
			
		// Go forward with presenting user a save clip dialog
		string message = string.Format("Create a new animation for the game object '{0}':", animatedObject.name);
		string newClipPath = EditorUtility.SaveFilePanelInProject ("Create New Animation", "New Animation", "anim", message, ProjectWindowUtil.GetActiveFolderPath ());
		
		// If user canceled or save path is invalid, we can't create a clip
		if (newClipPath == "") return null;
		
		// At this point we know that we can create a clip
		AnimationClip newClip = AllocateAndSetupClip (useAnimator);
		AssetDatabase.CreateAsset(newClip, newClipPath);
		
		// End animation mode before adding or changing animation component to object
		m_AnimationWindow.EndAnimationMode();
		
		// By default add it the animation to the Animator component.
		if (useAnimator)
			return AddClipToAnimatorComponent(animatedObject, newClip);
		// Add to the animation component if there is only an animation component
		else
			return AddClipToAnimationComponent(newClip);
		
	}

	public static AnimationClip AddClipToAnimatorComponent (GameObject animatedObject, AnimationClip newClip)
	{
		Animator animator = animatedObject.GetComponent<Animator> ();
		if (animator == null)
			animator = animatedObject.AddComponent<Animator> ();

        AnimatorController controller = AnimatorController.GetEffectiveAnimatorController(animator);
		if (controller == null)
		{
            controller = AnimatorController.CreateAnimatorControllerForClip(newClip, animatedObject);
            AnimatorController.SetAnimatorController(animator, controller);
			
			if (controller != null)
				return newClip;
			else
				return null;
		}
		else
		{
			AnimatorController.AddAnimationClipToController (controller, newClip);
			return newClip;
		}
	}

	private AnimationClip AddClipToAnimationComponent (AnimationClip newClip)
	{
		// Create the animation component if not already there
		if (animatedObject.GetComponent<Animation> () == null)
		{
			Animation animComp = animatedObject.AddComponent(typeof(Animation)) as Animation;
			animComp.clip = newClip;
		}

		// Get the list of clips in this animation component
		AnimationClip[] clips = AnimationUtility.GetAnimationClips(animatedObject);
		
		// Expand list if necessary
		System.Array.Resize<AnimationClip>(ref clips, clips.Length+1);

		// Add new clip to list
		clips[clips.Length-1] = newClip;
		AnimationUtility.SetAnimationClips(animatedObject.animation, clips);
		
		return newClip;
	}

	private void ChooseClip (AnimationClip newClip)
	{
		ChooseClip (newClip, GetIndexOfClip(newClip));
	}

	
	private void ChooseClip (AnimationClip newClip, int newClipIndex)
	{
		m_Clip = newClip;
		m_AnimationWindow.chosenClip.Set(GetStringArrayHashCode(GetClipNames()), newClipIndex);
		m_AnimationWindow.state.m_ActiveAnimationClip = m_Clip;
	}
	
	public void DrawRightIcon (Texture image, Color color, float width)
	{
		// Draw key icon
		Color tempColor = GUI.color;
		GUI.color = color;
		Rect r = m_AnimationWindow.GetIconRect(width);
		r.width = image.width;
		GUI.DrawTexture (r, image, ScaleMode.ScaleToFit, true, 1);
		
		GUI.color = tempColor;
	}
	
	public static string GetPath (Transform t)
	{
		return AnimationUtility.CalculateTransformPath(t, t.root);
	}
	
}

[System.Serializable]
internal class FoldoutTree
{
	private bool m_Locked;
	public bool locked { get { return m_Locked; } set { m_Locked = value; } }
	
	private FoldoutObjectState[] m_States;
	public FoldoutObjectState[] states { get { return m_States; } }
	
	public FoldoutObjectState root { get { return states[0]; } }
	public GameObject rootGameObject { get { return root.obj; } }
	public Transform rootTransform { get { return root.obj.transform; } }
	
	public FoldoutTree (Transform tr, SerializedStringTable expandedFoldouts)
	{
		List<FoldoutObjectState> stateList = new List<FoldoutObjectState>();
		new FoldoutObjectState(tr.gameObject, stateList, expandedFoldouts, 0); // recursively builds a tree
		m_States = stateList.ToArray();
	}
	
	public void Refresh (AnimationHierarchyData data)
	{
	//	root.Show(data, 0);
	}
}

[System.Serializable]
internal class FoldoutObjectState
{
	private GameObject m_Object;
	public GameObject obj { get { return m_Object; } }
	
	private bool m_Expanded = false;
	public bool expanded { get { return m_Expanded; } }
	
	private bool m_Animated = false;
	public bool animated { get { return m_Animated; } }
	
	public FoldoutComponentState[] m_Components;
	public FoldoutComponentState[] components { get { return m_Components; } }
	
	private int[] m_Children;
	public int[] children { get { return m_Children; } }
	
	public FoldoutObjectState (GameObject obj, List<FoldoutObjectState> states, SerializedStringTable expandedFoldouts, int level)
	{
		m_Object = obj;
		states.Add(this);
		
		if (level == 0)
			m_Expanded = true;
		else if (expandedFoldouts.Contains(AnimationSelection.GetPath(obj.transform)))
			m_Expanded = true;
		
		// Iterate through child objects
		List<int> children = new List<int>();
		foreach (Transform child in obj.transform)
		{
			children.Add(states.Count);
			new FoldoutObjectState(child.gameObject, states, expandedFoldouts, level+1);
		}
		m_Children = children.ToArray();
	}
	
	public void Expand (AnimationHierarchyData data, int level)
	{
		m_Expanded = true;
		//Show(data, level);
		if (level > 0)
			data.expandedFoldouts.Set(AnimationSelection.GetPath(obj.transform));
		data.animationWindow.SetDirtyCurves();
	}
	
	public void Collapse (AnimationHierarchyData data, int level)
	{
		m_Expanded = false;
		Hide(data, level);
		if (level > 0)
			data.expandedFoldouts.Remove(AnimationSelection.GetPath(obj.transform));
		data.animationWindow.SetDirtyCurves();
	}
	
	public void Hide (AnimationHierarchyData data, int level)
	{
		if (m_Components != null)
		{
			foreach (FoldoutComponentState componentState in m_Components)
				if (componentState.expanded) componentState.Hide(data, obj.transform, level);
		}
		
		foreach (int childIndex in children)
			if (data.states[childIndex].expanded) data.states[childIndex].Hide(data, level+1);
	}
	
	public void RefreshAnimatedState (AnimationHierarchyData data)
	{
		// Check if this object (or any children) has animated properties
		m_Animated = false;
		string relPath = AnimationUtility.CalculateTransformPath(obj.transform, data.animated.transform);
		foreach (int hash in data.animatedPaths.Keys)
		{
			if (hash == relPath.GetHashCode())
				m_Animated = true;
		}
	}
	
	public void AddChildCurvesToList (List<CurveState> curves, AnimationHierarchyData data)
	{
		if (!expanded)
			return;
		foreach (FoldoutComponentState comp in components)
			comp.AddChildCurvesToList(curves, data);
		foreach (int childIndex in children)
			data.states[childIndex].AddChildCurvesToList(curves, data);
			
	}
}

[System.Serializable]
internal class FoldoutComponentState
{
	private Object m_Object;
	public Object obj { get { return m_Object; } set { m_Object = value; } }
	
	private bool m_Expanded = false;
	public bool expanded { get { return m_Expanded; } }
	
	private bool m_Animated = false;
	public bool animated { get { return m_Animated; } }
	
	private CurveState[] m_CurveStates;
	public CurveState[] curveStates { get { return m_CurveStates; } }
	
	public FoldoutComponentState (Object obj, SerializedStringTable expandedFoldouts, Transform tr, int level)
	{
		m_Object = obj;
		if (level == 0 && m_Object.GetType() == typeof(Transform))
			m_Expanded = true;
		else if (expandedFoldouts.Contains(GetString(tr)))
			m_Expanded = true;
	}
	
	public string GetString (Transform tr)
	{
		return AnimationSelection.GetPath(tr) + "/:" + ObjectNames.GetInspectorTitle(obj);
	}
	
	public void Expand (AnimationHierarchyData data, Transform tr, int level)
	{
		m_Expanded = true;
		//Show(data, tr, level);
		if (level > 0 || m_Object.GetType() != typeof(Transform))
			data.expandedFoldouts.Set(GetString(tr));
		data.animationWindow.SetDirtyCurves();
	}
	
	public void Collapse (AnimationHierarchyData data, Transform tr, int level)
	{
		m_Expanded = false;
		Hide(data, tr, level);
		if (level > 0 || m_Object.GetType() != typeof(Transform))
			data.expandedFoldouts.Remove(GetString(tr));
		data.animationWindow.SetDirtyCurves();
	}
	
	public void Hide (AnimationHierarchyData data, Transform tr, int level)
	{
		m_CurveStates = null;
	}
	
	public void RefreshAnimatedState (AnimationHierarchyData data, Transform tr)
	{
		// Check if this object (or any children) has animated properties
		m_Animated = false;
		string relPath = AnimationUtility.CalculateTransformPath(tr, data.animated.transform);
		foreach (int hash in data.animatedPaths.Keys)
		{
			if (hash == CurveUtility.GetPathAndTypeID(relPath, obj.GetType()))
			{
				m_Animated = true;
				break;
			}
		}
	}
	
	public void AddChildCurvesToList (List<CurveState> curves, AnimationHierarchyData data)
	{
		if (!expanded)
			return;
		foreach (CurveState curve in curveStates)
			if (curve.animated || data.animationWindow.showAllProperties)
				curves.Add(curve);
	}
}

internal class CurveState
{
	private EditorCurveBinding m_CurveBinding;

	public Type type { get { return m_CurveBinding.type; } set { m_CurveBinding.type = value; } }
	public string propertyName { get { return m_CurveBinding.propertyName; } set { m_CurveBinding.propertyName = value; } }
	public string path { get { return m_CurveBinding.path; } set { m_CurveBinding.path = value; } }
	public EditorCurveBinding curveBinding { get { return m_CurveBinding; } set { m_CurveBinding = value; } }

	private bool m_Animated = false;
	public bool animated { get { return m_Animated; } set { m_Animated = value; } }
	

	public AnimationCurve curve
	{
		get
		{
			CurveRenderer renderer = CurveRendererCache.GetCurveRenderer (clip, m_CurveBinding);
			return renderer.GetCurve();
		}
	}
	
	public void SaveCurve (AnimationCurve animationCurve)
	{
		Undo.RegisterCompleteObjectUndo(clip, "Edit Curve");
		
		QuaternionCurveTangentCalculation.UpdateTangentsFromMode(animationCurve, clip, m_CurveBinding);
		AnimationUtility.SetEditorCurve (clip, m_CurveBinding, animationCurve);
	}
	
	public AnimationSelection animationSelection;
	public AnimationClip clip { get { return animationSelection.clip; } }
	public Color color;
	public Vector2 guiPosition;
	public bool even;

	public CurveState (EditorCurveBinding binding)
	{
		m_CurveBinding = binding;
	}
	
	public int GetID ()
	{
		return CurveUtility.GetCurveID (clip, m_CurveBinding);
	}
	
	public int GetGroupID ()
	{
		return CurveUtility.GetCurveGroupID (clip, m_CurveBinding);
	}
	
	public float GetSampledOrCurveValue (float time)
	{
		
		if (animated)
		{
			CurveRenderer renderer = CurveRendererCache.GetCurveRenderer (clip, m_CurveBinding);
			if (renderer==null)
				Debug.LogError("The renderer is null!");
			return renderer.EvaluateCurveSlow(time);
		}
		else
		{
			float val;
			if (!AnimationUtility.GetFloatValue (this.animationSelection.animatedObject, m_CurveBinding, out val))
			{
				//@TODO: Mecanim muscle values should support GetFloatValue
				// Debug.LogError("Object property value could not be sampled for "+curveData.path+"/"+curveData.propertyName);
				val = Mathf.Infinity;
			}
			return val;
		}
	}
}

} // namespace
