using UnityEditor;
using UnityEngine;

internal class PatchImportSettingRecycleID
{
	const int kMaxObjectsPerClassID = 100000;
	
	static public void Patch (SerializedObject serializedObject, int classID, string oldName, string newName)
	{
		SerializedProperty recycleMap = serializedObject.FindProperty("m_FileIDToRecycleName");
		foreach (SerializedProperty element in recycleMap)
		{
			SerializedProperty first = element.FindPropertyRelative("first");
			if (first.intValue >= kMaxObjectsPerClassID * classID && first.intValue < kMaxObjectsPerClassID * (classID+1))
			{
				SerializedProperty second = element.FindPropertyRelative("second");
				if (second.stringValue == oldName)
				{
					second.stringValue = newName;
					return;
				}
			}
		}
	}
}