#ifndef GLSLUTILITIES_H
#define GLSLUTILITIES_H

#include "Runtime/GfxDevice/GfxDeviceTypes.h"

class ShaderErrors;

enum GLSLErrorType
{
	kErrorCompileVertexShader,
	kErrorCompileFragShader,
	kErrorLinkProgram,
	kErrorCount
};
void OutputGLSLShaderError (const char* log, GLSLErrorType errorType, ShaderErrors& outErrors);

bool CanUseOptimizedFogCodeGLES(const std::string& srcVS);

bool PatchShaderFogGLSL (std::string& src, FogMode fog);
bool PatchShaderFogGLES (std::string& srcVS, std::string& srcPS, FogMode fog, bool useOptimizedFogCode);

// TODO: might be better to share whole parm filling procedure, but good enough for now
// TODO: as for now all gles impls we know returns first elem name
//       in case of using array[0] only - arraySize would be 1
//       in case of using array[1] only - arraySize would be 2 and uniform name would be array[0]
//       but this behaviour might change in future
bool IsShaderParameterArray(const char* name, unsigned nameLen, int arraySize, bool* isZeroElem=0);


#endif
