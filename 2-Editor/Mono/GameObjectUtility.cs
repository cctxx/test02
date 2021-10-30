using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace UnityEditor
{
public sealed partial class GameObjectUtility
{
	internal static bool ContainsStatic (GameObject[] objects)
	{
		if (objects == null || objects.Length == 0)
			return false;
		for (int i = 0; i < objects.Length; i++)
		{
			if (objects[i] != null && objects[i].isStatic)
				return true;
		}
		return false;
	}

	internal static bool HasChildren (IEnumerable<GameObject> gameObjects)
	{
		return gameObjects.Any (go => go.transform.childCount > 0);
	}

	internal enum ShouldIncludeChildren
	{
		HasNoChildren = -1,
		IncludeChildren = 0,
		DontIncludeChildren = 1,
		Cancel = 2
	}

	internal static ShouldIncludeChildren DisplayUpdateChildrenDialogIfNeeded (IEnumerable<GameObject> gameObjects, string title, string message)
	{
		if (!HasChildren (gameObjects))
			return ShouldIncludeChildren.HasNoChildren;

		return (ShouldIncludeChildren)EditorUtility.DisplayDialogComplex (title, message, "Yes, change children", "No, this object only", "Cancel");
	}
}
}
