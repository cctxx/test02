#include "UnityPrefix.h"
#include "CombinerGLES30.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "VBOGLES30.h"

#if GFX_SUPPORTS_OPENGLES30

TextureCombinersGLES3* TextureCombinersGLES3::Create (int count, const ShaderLab::TextureBinding* texEnvs)
{
	// check if we have enough vertex attributes to emulate this combiner
	if (count + kGLES3AttribLocationTexCoord0 >= gGraphicsCaps.gles30.maxAttributes)
		return NULL;
	
	// create struct that holds texture combiner info object
	TextureCombinersGLES3* combiners = new TextureCombinersGLES3();
	combiners->count = count;
	combiners->texEnvs = texEnvs;
	return combiners;
}

#endif // GFX_SUPPORTS_OPENGLES30
