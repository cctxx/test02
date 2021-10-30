#pragma once

#include "Runtime/GfxDevice/GpuProgram.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "D3D11Includes.h"

class GfxDevice;

class ConstantBuffersD3D11;
struct FixedFunctionStateD3D11;
struct InputSignatureD3D11;

class D3D11CommonShader : public GpuProgram {
public:
	virtual ~D3D11CommonShader();
	IUnknown* GetShader(FogMode fog, bool haveDomainShader, bool& outResetToNoFog);

protected:
	D3D11CommonShader ()
		: m_FogFailed(0)
	{
		for (int i = 0; i < kFogModeCount; ++i)
			m_Shaders[i] = NULL;
	}

	const UInt8* ApplyTextures (GfxDevice& device, ShaderType shaderType, const GpuProgramParameters& params, const UInt8* buffer);

	// default implementation just uses non-fog shader
	virtual bool PatchShaderForFog (FogMode fog);

protected:
	IUnknown*	m_Shaders[kFogModeCount];
	dynamic_array<UInt8> m_ByteCode; // stored for fog patching
	UInt32		m_FogFailed;
};

class D3D11VertexShader : public D3D11CommonShader {
public:
	D3D11VertexShader (const std::string& compiledSource);
	virtual ~D3D11VertexShader();
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);

protected:
	virtual bool PatchShaderForFog (FogMode fog);
private:
	bool Create (const std::string& compiledSource);
	const InputSignatureD3D11* m_InputSignature;
};

class D3D11PixelShader : public D3D11CommonShader {
public:
	D3D11PixelShader (const std::string& compiledSource);
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);
protected:
	virtual bool PatchShaderForFog (FogMode fog);
private:
	bool Create (const std::string& compiledSource);
};

class D3D11GeometryShader : public D3D11CommonShader {
public:
	D3D11GeometryShader (const std::string& compiledSource);
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);
private:
	bool Create (const std::string& compiledSource);
};

class D3D11HullShader : public D3D11CommonShader {
public:
	D3D11HullShader (const std::string& compiledSource);
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);
private:
	bool Create (const std::string& compiledSource);
};

class D3D11DomainShader : public D3D11CommonShader {
public:
	D3D11DomainShader (const std::string& compiledSource);
	virtual void ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer);
protected:
	virtual bool PatchShaderForFog (FogMode fog);
private:
	bool Create (const std::string& compiledSource);
};


class FixedFunctionProgramD3D11
{
public:
	struct ValueParameter {
		int	m_Name;
		int m_Index;
		int m_Bytes;
		ValueParameter (int n, int idx, int bytes) : m_Name(n), m_Index(idx), m_Bytes(bytes) {}
	};
	typedef dynamic_array<ValueParameter> ValueParameterArray;
	struct ValueParameters {
		ValueParameters() : m_CBID(0), m_CBSize(0) { }
		void AddVectorParam (int index, int dim, BuiltinShaderVectorParam name) { m_Params.push_back (ValueParameter (name, index, dim*4)); }
		bool HasVectorParams() const { return !m_Params.empty(); }
		void ApplyValues (const BuiltinShaderParamValues& values, ConstantBuffersD3D11& cbs, ShaderType shaderType) const;
		ValueParameterArray	m_Params;
		int	m_CBID;
		int	m_CBSize;
	};

public:
	FixedFunctionProgramD3D11 (const FixedFunctionStateD3D11& state);
	~FixedFunctionProgramD3D11 ();

	void ApplyFFGpuProgram (const BuiltinShaderParamValues& values, ConstantBuffersD3D11& cbs) const;

	const BuiltinShaderParamIndices& GetVPMatrices() const { return m_VPMatrices; }

	ID3D11VertexShader* GetVertexShader() { return m_VS; }
	ID3D11PixelShader* GetPixelShader() { return m_PS; }

private:
	ValueParameters		m_VPParams;
	ValueParameters		m_FPParams;
	BuiltinShaderParamIndices	m_VPMatrices;
	ID3D11VertexShader*		m_VS;
	ID3D11PixelShader*		m_PS;
	const InputSignatureD3D11* m_InputSig;
};
