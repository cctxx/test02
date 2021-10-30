#ifndef GPUPROGRAMSGLES30_H
#define GPUPROGRAMSGLES30_H


#if !GFX_SUPPORTS_OPENGLES30 && !GFX_SUPPORTS_OPENGLES
#error "Should not include GpuProgramsGLES30 on this platform"
#endif


#include "Runtime/GfxDevice/GpuProgram.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "IncludesGLES30.h"
#include "GpuProgramsGLES30_UniformCache.h"

class ConstantBuffersGLES30;

class GlslGpuProgramGLES30 : public GpuProgramGL
{
public:
	GlslGpuProgramGLES30 (const std::string& source, CreateGpuProgramOutput& output);

	~GlslGpuProgramGLES30();

	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer) { Assert(!"Should not be used"); }

	// Returns the permutation index used
	int ApplyGpuProgramES30 (const GpuProgramParameters& params, const UInt8 *buffer);

	static bool	InitBinaryShadersSupport();


	UniformCacheGLES30	m_UniformCache[kFogModeCount];

	static bool CompileGlslShader(GLShaderID shader, const char* source);
	int 		GetGLProgram (FogMode fog, GpuProgramParameters& outParams, ChannelAssigns& channels);

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


class FixedFunctionProgramGLES30
{
public:
	static const ShaderLab::FastPropertyName kSLPropTransformBlock;
	static const ShaderLab::FastPropertyName kSLPropUVTransformBlock;

public:
	FixedFunctionProgramGLES30(GLShaderID vertexShader, GLShaderID fragmentShader);
	~FixedFunctionProgramGLES30();

	void ApplyFFGpuProgram(const BuiltinShaderParamValues& values, ConstantBuffersGLES30& cbs) const;

	const BuiltinShaderParamIndices& GetBuiltinParams() const { return m_Params.GetBuiltinParams(); }
	const GpuProgramParameters::ConstantBufferList& GetConstantBuffers() const { return m_Params.GetConstantBuffers(); }

	mutable UniformCacheGLES30 		m_UniformCache;

protected:
	GLShaderID Create(GLShaderID vertexShader, GLShaderID fragmentShader);

private:
	GLShaderID				m_GLSLProgram;
	GLShaderID				m_GLSLVertexShader, m_GLSLFragmentShader;

	GpuProgramParameters	m_Params;
};


#endif
