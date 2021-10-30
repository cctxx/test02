using UnityEditor;

using UnityEditor.VersionControl;

namespace UnityEditorInternal.VersionControl
{
	// Menu popup for the main unity project window.  Items are greyed out when not available to help with usability.
	public class ProjectContextMenu
	{
		//[MenuItem ("Assets/Version Control/Get Latest", true)]
		static bool GetLatestTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.GetLatestIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Get Latest", false, 100)]
		static void GetLatest (MenuCommand cmd)
		{
			AssetList list = Provider.GetAssetListFromSelection();
			Provider.GetLatest (list).SetCompletionAction (CompletionAction.UpdatePendingWindow);
		}

		//[MenuItem ("Assets/Version Control/Submit...", true)]
		static bool SubmitTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.SubmitIsValid (null, selected);
		}

		//[MenuItem ("Assets/Version Control/Submit...", false, 200)]
		static void Submit (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			WindowChange.Open (selected, true);
		}

		//[MenuItem ("Assets/Version Control/Check Out", true)]
		static bool CheckOutTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.CheckoutIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Check Out", false, 201)]
		static void CheckOut (MenuCommand cmd)
		{
			AssetList list = Provider.GetAssetListFromSelection();
			Provider.Checkout (list, CheckoutMode.Both).SetCompletionAction (CompletionAction.UpdatePendingWindow);
		}

		//[MenuItem ("Assets/Version Control/Mark Add", true)]
		static bool MarkAddTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.AddIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Mark Add", false, 202)]
		static void MarkAdd (MenuCommand cmd)
		{
			AssetList list = Provider.GetAssetListFromSelection();
			Provider.Add (list, true).SetCompletionAction (CompletionAction.UpdatePendingWindow);

		}

		//[MenuItem ("Assets/Version Control/Revert...", true)]
		static bool RevertTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.RevertIsValid (selected, RevertMode.Normal);
		}

		//[MenuItem ("Assets/Version Control/Revert...", false, 300)]
		static void Revert (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			WindowRevert.Open (selected);
		}

		//[MenuItem ("Assets/Version Control/Revert Unchanged", true)]
		static bool RevertUnchangedTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.RevertIsValid (selected, RevertMode.Normal);
		}

		//[MenuItem ("Assets/Version Control/Revert Unchanged", false, 301)]
		static void RevertUnchanged (MenuCommand cmd)
		{
			AssetList list = Provider.GetAssetListFromSelection();
			Provider.Revert (list, RevertMode.Unchanged).SetCompletionAction (CompletionAction.UpdatePendingWindow);
		}

		//[MenuItem ("Assets/Version Control/Diff Against Head...", true)]
		static bool ResolveTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.ResolveIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Diff Against Head...", false, 500)]
		static void Resolve (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			WindowResolve.Open (selected);
		}
	
		//[MenuItem ("Assets/Version Control/Lock", true)]
		static bool LockTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.LockIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Lock", false, 400)]
		static void Lock (MenuCommand cmd)
		{
			AssetList list = Provider.GetAssetListFromSelection();
			Provider.Lock (list, true).SetCompletionAction (CompletionAction.UpdatePendingWindow);
		}

		//[MenuItem ("Assets/Version Control/Unlock", true)]
		static bool UnlockTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.UnlockIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Unlock", false, 401)]
		static void Unlock (MenuCommand cmd)
		{
			AssetList list = Provider.GetAssetListFromSelection();
			Provider.Lock (list, false).SetCompletionAction (CompletionAction.UpdatePendingWindow);
		}

		//[MenuItem ("Assets/Version Control/Diff/Against Head...", true)]
		static bool DiffHeadTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.DiffIsValid (selected);
		}

		//[MenuItem ("Assets/Version Control/Diff/Against Head...", false, 500)]
		static void DiffHead (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			Provider.DiffHead (selected, false);
		}

		static bool DiffHeadWithMetaTest (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			return Provider.enabled && Provider.DiffIsValid (selected);
		}

		static void DiffHeadWithMeta (MenuCommand cmd)
		{
			AssetList selected = Provider.GetAssetListFromSelection();
			Provider.DiffHead (selected, true);
		}
	}
}
