#include "UnityPrefix.h"
#include "CombinerGLES20.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "VBOGLES20.h"

#if GFX_SUPPORTS_OPENGLES20

TextureCombinersGLES2* TextureCombinersGLES2::Create (int count, const ShaderLab::TextureBinding* texEnvs)
{
	// check if we have enough vertex attributes to emulate this combiner
	if (count + GL_TEXTURE_ARRAY0 >= gGraphicsCaps.gles20.maxAttributes)
		return NULL;
	
	// create struct that holds texture combiner info object
	TextureCombinersGLES2* combiners = new TextureCombinersGLES2();
	combiners->count = count;
	combiners->texEnvs = texEnvs;
	return combiners;
}

#endif // GFX_SUPPORTS_OPENGLES20
