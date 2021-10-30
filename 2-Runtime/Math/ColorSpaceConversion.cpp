#include "UnityPrefix.h"
#include "ColorSpaceConversion.h"
#include "Runtime/Misc/PlayerSettings.h"


ColorSpace GetActiveColorSpace ()
{
	if (GetPlayerSettingsPtr())
		return GetPlayerSettings().GetValidatedColorSpace();
	else
		return kUninitializedColorSpace;
}

/*
 TODO:
 * Fog colors in fixed function pipeline are not adjusted. Ask aras how the fog color gets put into the shader. ApplyFog does something with builtin shader params but it's never passed to shaderstate??? WTF.
*/