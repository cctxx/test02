using UnityEngine;
using UnityEditor;
using System.IO;

namespace UnityEditorInternal
{
    public sealed partial class AnimatorController : RuntimeAnimatorController
{	
	public System.Action OnAnimatorControllerDirty;

	const string kControllerExtension = "controller";

	internal string GetDefaultBlendTreeParameter()
	{
		for (int i = 0; i < parameterCount; i++)
		{
            if (GetParameterType(i) == AnimatorControllerParameterType.Float)
                return GetParameterName(i);
		}
		
		return "Blend";
	}

    internal static void OnInvalidateAnimatorController(AnimatorController controller)
	{
		if(controller.OnAnimatorControllerDirty != null)
			controller.OnAnimatorControllerDirty();		
	}

    public static AnimatorController CreateAnimatorControllerAtPath(string path)
	{
        AnimatorController controller = new AnimatorController();
		controller.name = Path.GetFileName (path);
				
		AssetDatabase.CreateAsset(controller, path);

		controller.AddLayer("Base Layer");		
		
		
		///@TODO: Write the asset again, otherwise 
		
		return controller;
	}

    public static AnimatorController CreateAnimatorControllerForClip(AnimationClip clip, GameObject animatedObject)
	{
		string path = AssetDatabase.GetAssetPath(clip);

		if (string.IsNullOrEmpty (path))
			return null;
	
        path = Path.Combine(FileUtil.DeleteLastPathNameComponent (path), animatedObject.name + "." + kControllerExtension);
        path = AssetDatabase.GenerateUniqueAssetPath(path);
     
		if (string.IsNullOrEmpty (path))
			return null;
			
			
		return CreateAnimatorControllerAtPathWithClip(path, clip);
	}

    public static void AddAnimationClipToController(AnimatorController controller, AnimationClip clip)
	{        
        StateMachine sm = controller.GetLayerStateMachine(0);
        Vector3 statePosition = sm.stateCount > 0
                                    ? sm.GetState(sm.stateCount - 1).position + new Vector3(35,65)
                                    : new Vector3(200, 0, 0);

		State state = sm.AddState(clip.name);
        state.position = statePosition;
	    
        state.SetAnimationClip(clip);
	}

    public static AnimatorController CreateAnimatorControllerAtPathWithClip(string path, AnimationClip clip)
	{
        AnimatorController controller = CreateAnimatorControllerAtPath(path);
		
		State state = controller.GetLayerStateMachine(0).AddState(clip.name);
		state.SetAnimationClip(clip);
			
		return controller;
	}
}
}

