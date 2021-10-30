using UnityEngine;

namespace UnityEditor {

internal class AssetPreviewUpdater
{
	// Generate a preview texture for an asset
	public static Texture2D CreatePreviewForAsset (Object obj, Object[] subAssets, string assetPath)
	{
		if (obj == null)
			return null;

		System.Type type = CustomEditorAttributes.FindCustomEditorType (obj, false);
		if (type == null)
			return null;
		
		System.Reflection.MethodInfo info = type.GetMethod ("RenderStaticPreview");
		if (info == null)
		{
			Debug.LogError("Fail to find RenderStaticPreview base method");
			return null;
		}
		
		if (info.DeclaringType == typeof(Editor))
			return null;

			
		Editor editor = Editor.CreateEditor(obj);
		
		if (editor == null)
			return null;
			
		Texture2D tex = editor.RenderStaticPreview (assetPath, subAssets, 128, 128);	
		
		Object.DestroyImmediate(editor);
		
		return tex;
	}
}
}
