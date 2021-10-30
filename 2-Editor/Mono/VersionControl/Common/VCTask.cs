namespace UnityEditor.VersionControl
{
public partial class Task
{
	public AssetList assetList
	{
		get
		{
			AssetList list = new AssetList ();
			Asset[] assets = Internal_GetAssetList ();
			foreach (Asset asset in assets)
			{
				list.Add (asset);
			}

			return list;
		}
	}

	public ChangeSets changeSets
	{
		get
		{
			ChangeSets list = new ChangeSets ();
			ChangeSet[] changes = Internal_GetChangeSets ();
			foreach (ChangeSet change in changes)
			{
				list.Add (change);
			}

			return list;
		}
	}

}
}
