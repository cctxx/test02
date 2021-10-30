#pragma once

#include "D3D9Includes.h"
#include "Runtime/GfxDevice/GpuProgram.h"


class D3D9VertexShader : public GpuProgram {
public:
	D3D9VertexShader( const std::string& source );
	virtual ~D3D9VertexShader();
	
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);
	IDirect3DVertexShader9* GetShader(FogMode fog, bool& outResetToNoFog);
	IDirect3DVertexShader9* GetShaderAtFogIndex(FogMode fog) { return m_Shaders[fog]; }
	
private:
	bool Create( const std::string& source );

	std::string		m_SourceForFog; // original source, used for fog patching if needed
	IDirect3DVertexShader9*	m_Shaders[kFogModeCount];
	unsigned		m_FogFailed; // bit per fog mode
};

class D3D9PixelShader : public GpuProgram {
public:
	D3D9PixelShader( const std::string& source );
	virtual ~D3D9PixelShader();

	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);
	IDirect3DPixelShader9* GetShader(FogMode fog, const GpuProgramParameters& params);
	IDirect3DPixelShader9* GetShaderAtFogIndex(FogMode fog) { return m_Shaders[fog]; }

private:
	bool Create( const std::string& source );

	std::string		m_SourceForFog; // original source, used for fog patching if needed
	IDirect3DPixelShader9*	m_Shaders[kFogModeCount];
	int m_FogRegisters[kFogModeCount];
	unsigned		m_FogFailed; // bit per fog mode
};
