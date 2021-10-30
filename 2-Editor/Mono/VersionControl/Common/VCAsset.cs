using System;

namespace UnityEditor.VersionControl
{
public partial class Asset
{

	internal static bool IsState (Asset.States isThisState, Asset.States partOfThisState)
	{
		return (isThisState & partOfThisState) != 0;
	}

	public bool IsState (Asset.States state)
	{
		return IsState(this.state, state);
	}

	public bool IsOneOfStates (Asset.States[] states)
	{
		foreach (Asset.States st in states)
		{
			if ((this.state & st) != 0) return true;
		}
		return false;
	}

	internal bool IsUnderVersionControl
	{
		get { return IsState(Asset.States.Synced) || IsState(Asset.States.OutOfSync) || IsState(Asset.States.AddedLocal); }
	}

	public void Edit ()
	{
		UnityEngine.Object load = Load ();

		if (load != null)
			AssetDatabase.OpenAsset (load);
	}

	public UnityEngine.Object Load ()
	{
		if (state == States.DeletedLocal || isMeta)
		{
			return null;
		}

		// Standard asset loading
		return AssetDatabase.LoadAssetAtPath (path, typeof (UnityEngine.Object));
	}

	internal static string StateToString(Asset.States state)
	{
		if (IsState (state, Asset.States.AddedLocal))
			return "Added Local";

		if (IsState (state, Asset.States.AddedRemote))
			return "Added Remote";

		if (IsState (state, Asset.States.CheckedOutLocal) && !IsState (state, Asset.States.LockedLocal))
			return "Checked Out Local";

		if (IsState (state, Asset.States.CheckedOutRemote) && !IsState (state, Asset.States.LockedRemote))
			return "Checked Out Remote";

		if (IsState (state, Asset.States.Conflicted))
			return "Conflicted";
	
		if (IsState (state, Asset.States.DeletedLocal))
			return "Deleted Local";

		if (IsState (state, Asset.States.DeletedRemote))
			return "Deleted Remote";

		if (IsState (state, Asset.States.Local))
			return "Local";

		if (IsState (state, Asset.States.LockedLocal))
			return "Locked Local";

		if (IsState (state, Asset.States.LockedRemote))
			return "Locked Remote";

		if (IsState (state, Asset.States.OutOfSync))
			return "Out Of Sync";

		return "";
	}

	internal string StateToString()
	{
		return StateToString(this.state);
	}

	public string prettyPath {
		get {
			return path;
		}
	}

}
}
