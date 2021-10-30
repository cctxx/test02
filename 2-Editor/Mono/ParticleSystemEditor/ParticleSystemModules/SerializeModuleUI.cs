using UnityEngine;
using System.Collections.Generic;
using UnityEditor;
using System.Collections;

namespace UnityEditor
{

internal class SerializedModule
{
	protected string m_ModuleName;
	SerializedObject m_Object;
	
	public SerializedModule (SerializedObject o, string name)
	{
		m_Object = o;
		m_ModuleName = name;
	}

	public SerializedProperty GetProperty0 (string name)
	{
		SerializedProperty prop = m_Object.FindProperty (name);
		if (prop == null)
			Debug.LogError("GetProperty0: not found: " + name);
		return prop;

	}
	
	public SerializedProperty GetProperty (string name)
	{
		SerializedProperty prop = m_Object.FindProperty (Concat (m_ModuleName, name));
		if (prop == null)
			Debug.LogError("GetProperty: not found: " + Concat(m_ModuleName, name));
		return prop;
	}

	public SerializedProperty GetProperty (string structName, string propName)
	{
		SerializedProperty prop = m_Object.FindProperty (Concat (Concat (m_ModuleName, structName), propName));
		if (prop == null)
			Debug.LogError("GetProperty: not found: " + Concat(Concat(m_ModuleName, structName), propName));
		return prop;
	}
	
	public static string Concat (string a, string b)
	{
		return a + "." + b;
	}

	public string GetUniqueModuleName ()
	{
		return Concat(""+m_Object.targetObject.GetInstanceID(), m_ModuleName);
	}
}

} // namespace UnityEditor
