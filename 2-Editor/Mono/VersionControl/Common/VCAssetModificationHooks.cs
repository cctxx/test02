using UnityEditor;
using UnityEngine;

using UnityEditor.VersionControl;

namespace UnityEditorInternal.VersionControl
{
	// Monitor the behavior of assets.  This is where general unity asset operations are handled
	public class AssetModificationHook
	{
		private static Asset GetStatusCachedIfPossible(string from)
		{
			Asset asset = Provider.CacheStatus(from);
			if (asset == null || asset.IsState(Asset.States.Updating))
			{
				// Fetch status
				Task statusTask = Provider.Status(from, false);
				statusTask.Wait();
				asset = Provider.CacheStatus(from);
			}
			return asset;
		}
		
		// Handle asset moving
		public static AssetMoveResult OnWillMoveAsset (string from, string to)
		{
			if (!Provider.enabled)
				return AssetMoveResult.DidNotMove;

			Asset asset = GetStatusCachedIfPossible(from);

			if (asset == null || !asset.IsUnderVersionControl)
					return AssetMoveResult.DidNotMove;

			// Perform the actual move
			Task task = Provider.Move (from, to);
			task.Wait ();

			return task.success ? (AssetMoveResult)task.resultCode : AssetMoveResult.FailedMove;
		}

		// Handle asset deletion
		public static AssetDeleteResult OnWillDeleteAsset (string assetPath, RemoveAssetOptions option)
		{
			if (!Provider.enabled)
				return AssetDeleteResult.DidNotDelete;

			Task task = Provider.Delete (assetPath);
			task.SetCompletionAction(CompletionAction.UpdatePendingWindow);
			task.Wait ();

			// Using DidNotDelete on success to force unity to clean up local files
			return task.success ? AssetDeleteResult.DidNotDelete : AssetDeleteResult.FailedDelete;
		}

		public static bool IsOpenForEdit (string assetPath, out string message)
		{
			message = "";

			if (!Provider.enabled)
				return true;

			if (string.IsNullOrEmpty (assetPath))
				return true;

			Asset asset = Provider.GetAssetByPath (assetPath);

			if (asset == null)
			{
				Task task = Provider.Status(assetPath, false);
				task.Wait ();
				asset = task.assetList.Count > 0 ? task.assetList[0] : null;
			}

			if (asset == null)
			{
				return false;
			}

			return Provider.IsOpenForEdit (asset);
		}
	}
}
