using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using System.IO;

namespace UnityEditor
{

[CustomEditor(typeof(OcclusionPortal))]
internal class OcclusionPortalInspector : Editor
{
	private static readonly int s_BoxHash = "BoxColliderEditor".GetHashCode();
	private readonly BoxEditor m_BoxEditor = new BoxEditor(true, s_BoxHash);

	SerializedProperty m_Center;
	SerializedProperty m_Size;
	SerializedObject   m_Object;
	
	public void OnEnable()
	{
		m_Object = new SerializedObject(targets);

		m_Center = m_Object.FindProperty("m_Center");
		m_Size = m_Object.FindProperty("m_Size");

		m_BoxEditor.OnEnable();
		m_BoxEditor.SetAlwaysDisplayHandles (true);
	}

	void OnSceneGUI ()
	{
		OcclusionPortal portal = target as OcclusionPortal;
	
		Vector3 center = m_Center.vector3Value;
		Vector3 size = m_Size.vector3Value;

		Color color = Handles.s_ColliderHandleColor;
		if (m_BoxEditor.OnSceneGUI(portal.transform, color, ref center, ref size))
		{
			m_Center.vector3Value = center;
			m_Size.vector3Value = size;
			m_Object.ApplyModifiedProperties();
		}
	}
	
	
	
}

}	