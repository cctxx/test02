#pragma once

#include "Runtime/GfxDevice/GpuProgram.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"

class ShaderErrors;

class GlslGpuProgram : public GpuProgramGL {
public:
	GlslGpuProgram( const std::string& source, CreateGpuProgramOutput& output );
	virtual ~GlslGpuProgram();
	
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);

	GLShaderID	GetGLProgram (FogMode fog, GpuProgramParameters& outParams);
	
private:
	bool		Create( const std::string& source, ShaderErrors& outErrors );
	static void FillParams (GLShaderID programID, GpuProgramParameters& params, PropertyNamesSet* outNames);
	void		FillChannels (ChannelAssigns& channels);

private:
	GLShaderID	m_GLSLVertexShader[kFogModeCount];
	GLShaderID	m_GLSLFragmentShader[kFogModeCount];
	int			m_FogColorIndex[kFogModeCount];
	int			m_FogParamsIndex[kFogModeCount];
	bool		m_FogFailed[kFogModeCount];
};
