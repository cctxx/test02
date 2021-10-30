#include "UnityPrefix.h"
#include "AssetPipeline/AssetImporterUtil.h"

#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/ProceduralMaterial.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Math/ColorSpaceConversion.h"

#include "Editor/Platform/Interface/EditorWindows.h"

void ColorSpaceLiveSwitch ()
{
	CheckTextureImporterLinearRenderingMode ();
	
	// AwakeFromLoad materials (Property sheet is storing linear light values) 
	// AwakeFromLoad lights (Precomputed color value is storing linear light values)
	std::vector<Object*> reloadObjects;
	Object::FindObjectsOfType(ClassID(Light), &reloadObjects);
	Object::FindObjectsOfType(ClassID(Material), &reloadObjects);

	for (int i=0;i<reloadObjects.size();i++)
		reloadObjects[i]->AwakeFromLoad(kDefaultAwakeFromLoad);
	
	// Recreate all OpenGL / Direct-X contexts / textures etc to enable / disable sRGB write
	Texture::ReloadAll();
	RenderTexture::ReleaseAll();

	InvalidateGraphicsStateInEditorWindows ();
	RequestRepaintOnAllViews();
}
