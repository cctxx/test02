#ifndef GPUPROGRAMSGLES20_H
#define GPUPROGRAMSGLES20_H


#if !GFX_SUPPORTS_OPENGLES20
#error "Should not include GpuProgramsGLES20 on this platform"
#endif


#include "Runtime/GfxDevice/GpuProgram.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Utilities/GLSLUtilities.h"
#include "IncludesGLES20.h"
#include "GpuProgramsGLES20_UniformCache.h"

class GlslGpuProgramGLES20 : public GpuProgramGL
{
public:
	GlslGpuProgramGLES20 (const std::string& source, CreateGpuProgramOutput& output);
	~GlslGpuProgramGLES20();

	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer) { Assert(!"Should not be used"); }

	// Returns the permutation index used
	int ApplyGpuProgramES20 (const GpuProgramParameters& params, const UInt8 *buffer);
	int	GetGLProgram (FogMode fog, GpuProgramParameters& outParams, ChannelAssigns& channels);

	static bool	InitBinaryShadersSupport();


	UniformCacheGLES20	m_UniformCache[kFogModeCount];

private:
	static void FillParams (unsigned int programID, GpuProgramParameters& params, PropertyNamesSet* outNames);

	bool		Create (const std::string& source, ChannelAssigns& channels);

	bool		CompileProgram(unsigned index, const std::string& vprog, const std::string& fshader, ChannelAssigns& channels);

private:
	std::string m_VertexShaderSourceForFog;
	GLShaderID m_GLSLVertexShader[kFogModeCount];
	GLShaderID m_GLSLFragmentShader[kFogModeCount];
	int		m_FogColorIndex[kFogModeCount];
	int		m_FogParamsIndex[kFogModeCount];
	bool	m_FogFailed[kFogModeCount];
	
	static std::string					_CachePath;
	static glGetProgramBinaryOESFunc	_glGetProgramBinaryOES;
	static glProgramBinaryOESFunc		_glProgramBinaryOES;
};


class FixedFunctionProgramGLES20
{
public:
	FixedFunctionProgramGLES20(GLShaderID vertexShader, GLShaderID fragmentShader);
	~FixedFunctionProgramGLES20();

	void ApplyFFGpuProgram(const BuiltinShaderParamValues& values) const;
	const BuiltinShaderParamIndices& GetBuiltinParams() const { return m_Params.GetBuiltinParams(); }

	mutable UniformCacheGLES20 		m_UniformCache;

protected:
	GLShaderID Create(GLShaderID vertexShader, GLShaderID fragmentShader);

private:
	GLShaderID				m_GLSLProgram;
	GLShaderID				m_GLSLVertexShader, m_GLSLFragmentShader;

	GpuProgramParameters	m_Params;
};

bool CompileGlslShader(GLShaderID shader, GLSLErrorType type, const char* source);

#endif
