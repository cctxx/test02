#pragma once

#include "Runtime/GfxDevice/GpuProgram.h"

class ArbGpuProgram : public GpuProgramGL {
public:
	ArbGpuProgram( const std::string& source, ShaderType type, ShaderImplType implType );
	virtual ~ArbGpuProgram();
	
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);

	GLShaderID GetGLProgram(FogMode fog);
	
private:
	bool Create( const std::string& source, ShaderType type );	

private:
	bool		m_FogFailed[kFogModeCount];
};
