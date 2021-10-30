#include "Material.h"

inline bool IsTransparentOrCutoutMaterial (Material& material)
{
	string tag = material.GetTag("RenderType", false, string());
	
	if (material.GetActualRenderQueue() >= kAlphaTestRenderQueue)
		return true;
	
	if (tag == "TreeTransparentCutout" || tag == "GrassBillboard" || tag == "Grass" || tag == "TreeLeaf" || tag == "TranparentCutout" || tag == "Tranparent")
		return true;
	
	return false;
}
