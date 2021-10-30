using System.IO;
using UnityEditor.ProjectWindowCallback;
using UnityEngine;


namespace UnityEditor
{

[System.Serializable]
internal class CreateAssetUtility
{
	[SerializeField]
	EndNameEditAction m_EndAction;
	[SerializeField]
	int m_InstanceID;
	[SerializeField]
	string m_Path = "";
	[SerializeField]
	Texture2D m_Icon;
	[SerializeField]
	string m_ResourceFile;

	public void Clear()
	{
		m_EndAction = null;
		m_InstanceID = 0;
		m_Path = "";
		m_Icon = null;
		m_ResourceFile = "";	
	}

	public int instanceID 
	{
		get { return m_InstanceID; }
	}

	public Texture2D icon 
	{
		get { return m_Icon; }
	}

	public string folder 
	{
		get { return Path.GetDirectoryName(m_Path); }
	}

	public string extension
	{
		get { return Path.GetExtension(m_Path); }
	}

	public string originalName
	{
		get { return Path.GetFileNameWithoutExtension(m_Path); }
	}

	public EndNameEditAction endAction
	{
		get { return m_EndAction; }
	}

	public void BeginNewAssetCreation (int instanceID, EndNameEditAction newAssetEndAction, string pathName, Texture2D icon, string newAssetResourceFile)
	{
		if (!pathName.StartsWith("assets/", System.StringComparison.CurrentCultureIgnoreCase))
		{
			pathName = AssetDatabase.GetUniquePathNameAtSelectedPath(pathName);
		}
		else
		{
			pathName = AssetDatabase.GenerateUniqueAssetPath(pathName);
		}

		m_InstanceID = instanceID;
		m_Path = pathName;
		m_Icon = icon;
		m_EndAction = newAssetEndAction;
		m_ResourceFile = newAssetResourceFile;

		// Change selection to none or instanceID
		Selection.activeObject = EditorUtility.InstanceIDToObject (instanceID);
	}

	// The asset is created here
	public void EndNewAssetCreation (string name)
	{
		string path = folder + "/" + name + extension;
		EndNameEditAction endAction = m_EndAction;
		int instanceID = m_InstanceID;
		string resourceFile = m_ResourceFile;
		Clear (); // Ensure clear if anything goes bad in EndNameEditAction and gui is exited.

		ProjectWindowUtil.EndNameEditAction (endAction, instanceID, path, resourceFile);
	}

	public bool IsCreatingNewAsset ()
	{
		return !string.IsNullOrEmpty (m_Path);
	}
}


} // end namespace UnityEditor

