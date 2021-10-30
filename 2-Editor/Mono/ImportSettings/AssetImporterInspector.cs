using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor
{

internal abstract class AssetImporterInspector : Editor
{
	ulong m_AssetTimeStamp = 0;
	bool m_MightHaveModified = false;
	
	internal Editor m_AssetEditor;
	internal Editor assetEditor { get { return m_AssetEditor; } }
	
	internal override string targetTitle { get {
		return assetEditor.targetTitle + " Import Settings";
	} }

	internal override int referenceTargetIndex
	{
		get { return base.referenceTargetIndex; }
		set 
		{
			base.referenceTargetIndex = value;
			assetEditor.referenceTargetIndex = value;
		}
	}

	// Make the Importer use the preview of the asset itself
	// The importer never has the main preview but the preview functions might be used for header or similar.
	public override string GetInfoString () { return assetEditor.GetInfoString (); }
	public override bool HasPreviewGUI () 
	{ 		
		return assetEditor != null && assetEditor.HasPreviewGUI(); 
	}
	public override GUIContent GetPreviewTitle () { return assetEditor.GetPreviewTitle (); }
	public override void OnPreviewSettings () { assetEditor.OnPreviewSettings (); }
	public override void OnPreviewGUI (Rect r, GUIStyle background) { assetEditor.OnPreviewGUI (r, background); }
	
	//We usually want to redirect the DrawPreview to the assetEditor, but there are few cases we don't want that.
	//If you want to use the Importer DrawPreview, then override useAssetDrawPreview to false.
	protected virtual bool useAssetDrawPreview { get { return true; } }
	internal override void DrawPreview (Rect previewPosition)
	{
		if (useAssetDrawPreview)
		{
			assetEditor.DrawPreview (previewPosition);
		} 
		else
		{
			base.DrawPreview (previewPosition);
		}
	}

	// Make the Importer use the icon of the asset
	internal override void OnHeaderIconGUI (Rect iconRect) { assetEditor.OnHeaderIconGUI(iconRect); }
	
	// Let asset importers decide if the imported object should be shown as a separate editor or not
	internal virtual bool showImportedObject { get { return true; } }
	
	internal override SerializedObject GetSerializedObjectInternal ()
	{
		if (m_SerializedObject == null)
			m_SerializedObject = SerializedObject.LoadFromCache (GetInstanceID());
		if (m_SerializedObject == null)
			m_SerializedObject = new SerializedObject (targets);
		return m_SerializedObject;
	}

	public void OnDisable ()
	{
		// When destroying the inspector check if we have any unapplied modifications
		// and apply them.
		AssetImporter importer = target as AssetImporter;
		if (Unsupported.IsDestroyScriptableObject(this) && m_MightHaveModified && importer != null && !InternalEditorUtility.ignoreInspectorChanges && HasModified () && !AssetWasUpdated() )
		{
			if (EditorUtility.DisplayDialog ("Unapplied import settings", "Unapplied import settings for \'" + importer.assetPath + "\'", "Apply", "Revert"))
			{
				Apply();
				m_MightHaveModified = false;
				AssetImporterInspector.ImportAssets(GetAssetPaths());
			}
		}
		
		// Only cache SerializedObject if it has modified properties.
		// If we have multiple editors (e.g. a tabbed editor and its editor for the active tab) we don't
		// want the one that doesn't do anything with the SerializedObject to overwrite the cache.
		if (m_SerializedObject != null && m_SerializedObject.hasModifiedProperties)
		{
			m_SerializedObject.Cache (GetInstanceID ());
			m_SerializedObject = null;
		}
	}

	internal virtual void Awake ()
	{
		ResetTimeStamp();
		ResetValues();
	}
	
	string[] GetAssetPaths ()
	{
		Object[] allTargets = targets;
		string[] paths = new string[allTargets.Length];
		for (int i=0;i<allTargets.Length;i++)
		{
			AssetImporter importer = allTargets[i] as AssetImporter;
			paths[i] = importer.assetPath;
		}
		return paths;
	}
	
	internal virtual void ResetValues ()
	{
		serializedObject.SetIsDifferentCacheDirty ();
		serializedObject.Update ();
	}

	internal virtual bool HasModified ()
	{
		return serializedObject.hasModifiedProperties;
	}

	internal virtual void Apply ()
	{
		serializedObject.ApplyModifiedPropertiesWithoutUndo ();
	}
	
	internal bool AssetWasUpdated ()
	{
		AssetImporter importer = target as AssetImporter;
		if (m_AssetTimeStamp == 0)
			ResetTimeStamp ();
		return importer != null && m_AssetTimeStamp != importer.assetTimeStamp;
	}
	
	internal void ResetTimeStamp ()
	{
		AssetImporter importer = target as AssetImporter;
		if (importer != null)
			m_AssetTimeStamp = importer.assetTimeStamp;
	}

	internal void ApplyAndImport ()
	{
		Apply();
		m_MightHaveModified = false;
		ImportAssets(GetAssetPaths());
		ResetValues();
	}
	
	static void ImportAssets (string[] paths)
	{
		// When using the cache server we have to write all import settings to disk first.
		// Then perform the import (Otherwise the cache server will not be used for the import)
		foreach (string path in paths)
			AssetDatabase.WriteImportSettingsIfDirty(path);

		try
		{
			AssetDatabase.StartAssetEditing ();
			foreach (string path in paths)
				AssetDatabase.ImportAsset(path);
		}
		finally
		{
			AssetDatabase.StopAssetEditing ();	
		}
	}
	
	protected void ApplyRevertGUI ()
	{
		m_MightHaveModified = true;
		EditorGUILayout.Space ();
		bool wasEnabled = GUI.enabled;
		GUI.enabled = HasModified() && wasEnabled;

		GUILayout.BeginHorizontal ();
		GUILayout.FlexibleSpace ();

		if (GUILayout.Button("Revert")) 
		{
			m_MightHaveModified = false;
			ResetTimeStamp();
			ResetValues ();
			if (HasModified ())
				Debug.LogError ("Importer reports modified values after reset.");
		}

		if (GUILayout.Button("Apply"))
		{
			ApplyAndImport ();
		}
		
		// If the .meta file was modified on disk, reload UI
		if (AssetWasUpdated ())
		{
			ResetTimeStamp ();
			ResetValues();
		}

		GUILayout.EndHorizontal();
		
		GUI.enabled = wasEnabled;
	}
}
}
