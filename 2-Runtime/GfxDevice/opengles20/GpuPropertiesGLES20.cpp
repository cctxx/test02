#include "UnityPrefix.h"
#include "GpuPropertiesGLES20.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/GfxDevice/BuiltinShaderParams.h"
#include "Runtime/GfxDevice/BuiltinShaderParamsNames.h"

#if GFX_SUPPORTS_OPENGLES20

struct GLSLESProperty
{
	GLSLESProperty(const char* _glName, const char* _glesName) : glName(_glName), unityName(_glesName) { }
	const char* glName;
	const char* unityName;
};

#define DEF_MAT_INTERNAL(name, builtin) GLSLESProperty(name, GetShaderInstanceMatrixParamName(builtin))
#define DEF_MAT_BUILTIN(name, builtin)  GLSLESProperty(name, GetBuiltinMatrixParamName(builtin))
#define BIND_VEC_BUILTIN(name, builtin)	GLSLESProperty(name, GetBuiltinVectorParamName(builtin))


static const GLSLESProperty kglslesProperties[] = 
{
	DEF_MAT_BUILTIN("gl_ProjectionMatrix", kShaderMatProj),
	DEF_MAT_INTERNAL("gl_NormalMatrix", kShaderInstanceMatNormalMatrix),
	DEF_MAT_INTERNAL("gl_ModelViewProjectionMatrix", kShaderInstanceMatMVP),
	DEF_MAT_INTERNAL("gl_ModelViewMatrixTranspose", kShaderInstanceMatTransMV),
	DEF_MAT_INTERNAL("gl_ModelViewMatrixInverseTranspose", kShaderInstanceMatInvTransMV),
	DEF_MAT_INTERNAL("gl_ModelViewMatrix", kShaderInstanceMatMV),
	
	DEF_MAT_INTERNAL("gl_TextureMatrix0", kShaderInstanceMatTexture0),
	DEF_MAT_INTERNAL("gl_TextureMatrix1", kShaderInstanceMatTexture1),
	DEF_MAT_INTERNAL("gl_TextureMatrix2", kShaderInstanceMatTexture2),
	DEF_MAT_INTERNAL("gl_TextureMatrix3", kShaderInstanceMatTexture3),
	DEF_MAT_INTERNAL("gl_TextureMatrix4", kShaderInstanceMatTexture4),
	DEF_MAT_INTERNAL("gl_TextureMatrix5", kShaderInstanceMatTexture5),
	DEF_MAT_INTERNAL("gl_TextureMatrix6", kShaderInstanceMatTexture6),
	DEF_MAT_INTERNAL("gl_TextureMatrix7", kShaderInstanceMatTexture7),
	
	BIND_VEC_BUILTIN("_glesLightSource[0].diffuse", kShaderVecLight0Diffuse),
	BIND_VEC_BUILTIN("_glesLightSource[1].diffuse", kShaderVecLight1Diffuse),
	BIND_VEC_BUILTIN("_glesLightSource[2].diffuse", kShaderVecLight2Diffuse),
	BIND_VEC_BUILTIN("_glesLightSource[3].diffuse", kShaderVecLight3Diffuse),
	BIND_VEC_BUILTIN("_glesLightSource[0].position", kShaderVecLight0Position),
	BIND_VEC_BUILTIN("_glesLightSource[1].position", kShaderVecLight1Position),
	BIND_VEC_BUILTIN("_glesLightSource[2].position", kShaderVecLight2Position),
	BIND_VEC_BUILTIN("_glesLightSource[3].position", kShaderVecLight3Position),
	BIND_VEC_BUILTIN("_glesLightSource[0].spotDirection", kShaderVecLight0SpotDirection),
	BIND_VEC_BUILTIN("_glesLightSource[1].spotDirection", kShaderVecLight1SpotDirection),
	BIND_VEC_BUILTIN("_glesLightSource[2].spotDirection", kShaderVecLight2SpotDirection),
	BIND_VEC_BUILTIN("_glesLightSource[3].spotDirection", kShaderVecLight3SpotDirection),	
	BIND_VEC_BUILTIN("_glesLightSource[0].atten", kShaderVecLight0Atten),
	BIND_VEC_BUILTIN("_glesLightSource[1].atten", kShaderVecLight1Atten),
	BIND_VEC_BUILTIN("_glesLightSource[2].atten", kShaderVecLight2Atten),
	BIND_VEC_BUILTIN("_glesLightSource[3].atten", kShaderVecLight3Atten),
	BIND_VEC_BUILTIN("_glesLightModel.ambient", kShaderVecLightModelAmbient),
};


const char* GetGLSLESPropertyNameRemap (const char* name)
{
	for (int i = 0; i < ARRAY_SIZE(kglslesProperties); i++)
	{
		const GLSLESProperty& prop = kglslesProperties[i];
		if (strcmp(name, prop.glName) == 0)
			return prop.unityName;
	}
	return NULL;
}

#endif // GFX_SUPPORTS_OPENGLES20
