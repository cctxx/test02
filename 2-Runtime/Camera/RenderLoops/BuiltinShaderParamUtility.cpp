#include "UnityPrefix.h"
#include "BuiltinShaderParamUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/ShaderKeywords.h"

static ShaderKeyword gSupportedLODFadeKeyword = keywords::Create("ENABLE_LOD_FADE");

void SetObjectScale (GfxDevice& device, float lodFade, float invScale)
{
	device.SetInverseScale(invScale);

	/////@TODO: Figure out why inverse scale is implemented in gfxdevice, and decide if we should do the same for lodFade?
	device.GetBuiltinParamValues().SetInstanceVectorParam(kShaderInstanceVecScale, Vector4f(0,0,lodFade, invScale));

	if (lodFade == LOD_FADE_DISABLED)
		g_ShaderKeywords.Disable(gSupportedLODFadeKeyword);
	else
		g_ShaderKeywords.Enable(gSupportedLODFadeKeyword);
}
