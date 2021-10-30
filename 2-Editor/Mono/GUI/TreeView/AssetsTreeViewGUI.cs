using UnityEditor.ProjectWindowCallback;
using UnityEngine;
using UnityEditorInternal.VersionControl;
using UnityEditor.VersionControl;

namespace UnityEditor
{

internal class AssetsTreeViewGUI : TreeViewGUI
{
	static bool s_VCEnabled;
	const float k_IconOverlayPadding = 7f;

	public AssetsTreeViewGUI (TreeView treeView)
		: base (treeView)
	{
		iconOverlayGUI += OnIconOverlayGUI;
	}


	// ---------------------
	// OnGUI section

	override public void BeginRowGUI ()
	{
		s_VCEnabled = Provider.isActive;
		iconLeftPadding = iconRightPadding = s_VCEnabled ? k_IconOverlayPadding : 0f;
		base.BeginRowGUI();
	}


	//-------------------
	// Create asset and Rename asset section

	protected CreateAssetUtility GetCreateAssetUtility ()
	{
		return m_TreeView.state.createAssetUtility;
	}

	virtual protected bool IsCreatingNewAsset (int instanceID)
	{
		return GetCreateAssetUtility ().IsCreatingNewAsset () && IsRenaming (instanceID);
	}

	override protected void ClearRenameAndNewNodeState()
	{
		GetCreateAssetUtility().Clear();
		base.ClearRenameAndNewNodeState();
	}

	override protected void RenameEnded ()
	{
		string name = string.IsNullOrEmpty (GetRenameOverlay ().name) ? GetRenameOverlay ().originalName : GetRenameOverlay ().name;
		int instanceID = GetRenameOverlay ().userData;
		bool isCreating = GetCreateAssetUtility ().IsCreatingNewAsset ();
		bool userAccepted = GetRenameOverlay ().userAcceptedRename;

		if (userAccepted)
		{
			if (isCreating)
			{
				// Create a new asset
				GetCreateAssetUtility().EndNewAssetCreation(name);
			}
			else
			{
				// Rename an existing asset
				ObjectNames.SetNameSmartWithInstanceID(instanceID, name);
			}
		}
	}
	
	override protected void SyncFakeItem ()
	{
		if (!m_TreeView.data.HasFakeItem () && GetCreateAssetUtility ().IsCreatingNewAsset ())
		{
			int parentInstanceID = AssetDatabase.LoadAssetAtPath (GetCreateAssetUtility().folder, typeof(Object)).GetInstanceID ();
			m_TreeView.data.InsertFakeItem (GetCreateAssetUtility ().instanceID, parentInstanceID, GetCreateAssetUtility ().originalName, GetCreateAssetUtility ().icon);
		}

		if (m_TreeView.data.HasFakeItem () && !GetCreateAssetUtility ().IsCreatingNewAsset ())
		{
			m_TreeView.data.RemoveFakeItem ();
		}
	}

	// Not part of interface because it is very specific to creating assets
	virtual public void BeginCreateNewAsset (int instanceID, EndNameEditAction endAction, string pathName, Texture2D icon, string resourceFile)
	{
		ClearRenameAndNewNodeState();

		// Selection changes when calling BeginNewAsset
		GetCreateAssetUtility ().BeginNewAssetCreation (instanceID, endAction, pathName, icon, resourceFile);
		SyncFakeItem ();

		// Start nameing the asset
		bool renameStarted = GetRenameOverlay ().BeginRename (GetCreateAssetUtility ().originalName, instanceID, 0f);
		if (!renameStarted)
			Debug.LogError ("Rename not started (when creating new asset)");
	}

	// Handles fetching rename icon or cached asset database icon
	protected override Texture GetIconForNode (TreeViewItem item)
	{
		if (item == null)
			return null;

		Texture icon = null;
		if (IsCreatingNewAsset(item.id))
			icon = GetCreateAssetUtility().icon;

		if (icon == null)
			icon = item.icon;

		if (icon == null && item.id != 0)
		{
			string path = AssetDatabase.GetAssetPath(item.id);
			icon = AssetDatabase.GetCachedIcon(path);
		}
		return icon;
	}

	private void OnIconOverlayGUI (TreeViewItem item, Rect overlayRect)
	{
		// Draw vcs icons
		if (s_VCEnabled && AssetDatabase.IsMainAsset(item.id)) 
		{
			string path = AssetDatabase.GetAssetPath (item.id);
			string guid = AssetDatabase.AssetPathToGUID (path);
			ProjectHooks.OnProjectWindowItem (guid, overlayRect);
		}
	}
}


}


