using UnityEngine;
using UnityEditor.VersionControl;

namespace UnityEditorInternal.VersionControl
{
	// Display hooks for the main project window.  Icons are overlayed to show the version control state.
	internal class ProjectHooks 
	{
		// GUI callback for each item visible in the project window
		public static void OnProjectWindowItem (string guid, Rect drawRect)
		{
			if (!Provider.isActive)
				return;

			Overlay.DrawOverlay (Provider.GetAssetByGUID (guid), drawRect);
		}

		public static Rect GetOverlayRect(Rect drawRect)
		{
			return Overlay.GetOverlayRect (drawRect);
		}
	}
}
