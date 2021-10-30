using UnityEditorInternal;
using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

namespace UnityEditorInternal
{
	
/**
 *  Utilities for the Asset Store upload tool
 */
public partial class AssetStoreToolUtils {
	

	public static bool PreviewAssetStoreAssetBundleInInspector(AssetBundle bundle, AssetStoreAsset info)
	{
			//AssetBundle bundle = AssetBundle.CreateFromFile("testing.unity3d");
			
			info.id = 0; // make sure the id is zero when previewing 
			info.previewAsset = bundle.mainAsset;
			AssetStoreAssetSelection.Clear();
			AssetStoreAssetSelection.AddAssetInternal(info);
			
			// Make the inspector show the asset
			Selection.activeObject = AssetStoreAssetInspector.Instance;
			AssetStoreAssetInspector.Instance.Repaint();
			return true;
	}


	public static void UpdatePreloadingInternal()	
	{
			AssetStoreUtils.UpdatePreloading();
	}
	
}
} // UnityEditor namespace

