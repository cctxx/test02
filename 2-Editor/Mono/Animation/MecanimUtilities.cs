using UnityEngine;
using System.Collections;
using UnityEditor;
using System.Collections.Generic;
using System.Linq;

using UnityEditorInternal;

namespace UnityEditor
{
	internal class MecanimUtilities
	{		
		
        static public bool HasChildMotion(Motion parent, Motion motion)
        {
            if (parent == motion)
            {
                return true;
            }
            else if (parent is BlendTree)
            {
                BlendTree tree = parent as BlendTree;
            	int childCount = tree.childCount;
                for(int i = 0; i < childCount; i++) 
                {
                    if (HasChildMotion(tree.GetMotion(i), motion))
                        return true;
                }
            }
            
            return false;
        }
    
		static public bool StateMachineRelativePath(StateMachine parent, StateMachine toFind, ref List<StateMachine> hierarchy)
		{
			hierarchy.Add(parent);
			if (parent == toFind)
				return true;

			for (int i = 0; i < parent.stateMachineCount; i++)
			{
				if (StateMachineRelativePath(parent.GetStateMachine(i), toFind, ref hierarchy))
					return true;				
			}
			hierarchy.Remove(parent);
			return false;
		}

		

		
		internal static bool AreSameAsset(Object obj1, Object obj2)
		{
			return AssetDatabase.GetAssetPath(obj1) == AssetDatabase.GetAssetPath(obj2);
		}

		internal static void DestroyStateMachineRecursive(StateMachine stateMachine)
		{
			for (int i = 0; i < stateMachine.stateMachineCount; i++)
			{
				StateMachine subStateMachine = stateMachine.GetStateMachine(i);
				if (AreSameAsset(stateMachine, subStateMachine))
					DestroyStateMachineRecursive(subStateMachine);
			}

			for (int i = 0; i < stateMachine.stateCount; i++)
			{
				for (int j = 0; j < stateMachine.motionSetCount; j++)
				{
					BlendTree blendTree = stateMachine.GetState(i).GetMotionInternal(j) as BlendTree;
					if (blendTree != null && AreSameAsset(stateMachine, blendTree))
						DestroyBlendTreeRecursive(blendTree);
				}
			}

			Object.DestroyImmediate(stateMachine, true);
		}

		internal static void DestroyBlendTreeRecursive(BlendTree blendTree)
		{
			for (int i = 0; i < blendTree.childCount; i++)
			{
				BlendTree childBlendTree = blendTree.GetMotion(i) as BlendTree;
				if (childBlendTree != null && AreSameAsset(blendTree, childBlendTree))
					DestroyBlendTreeRecursive(childBlendTree);
			}

			Object.DestroyImmediate(blendTree, true);
		}
	}
}
