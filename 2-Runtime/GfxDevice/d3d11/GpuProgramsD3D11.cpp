#include "UnityPrefix.h"
#include "GpuProgramsD3D11.h"
#include "ConstantBuffersD3D11.h"
#include "D3D11Context.h"
#include "D3D11Utils.h"
#include "GfxDeviceD3D11.h"
#include "ShaderGeneratorD3D11.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/ComputeShader.h"
#include "ShaderGeneratorD3D11.h"
#include "ShaderPatchingD3D11.h"
#include "FixedFunctionStateD3D11.h"
#include "External/shaderlab/Library/properties.h"



ConstantBuffersD3D11& GetD3D11ConstantBuffers (GfxDevice& device);
const InputSignatureD3D11* GetD3D11InputSignature (void* code, unsigned length);

const InputSignatureD3D11* g_CurrentVSInputD3D11;


static GpuProgramLevel DecodeShader (const std::string& source, dynamic_array<UInt8>& output)
{
	GpuProgramLevel level = kGpuProgramNone;

	// decode shader
	int startSkip = 0;
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0)
	{
		if (strncmp(source.c_str()+1, "s_4_0_level_9_", strlen("s_4_0_level_9_")) == 0)
		{
			startSkip = strlen("vs_4_0_level_9_x") + 1;
			level = kGpuProgramSM3;
		}
		Assert ("Unsupported shader found!");
	}
	else if (strncmp(source.c_str()+1, "s_dx11", 6) == 0)
	{
		startSkip = 7;
		level = kGpuProgramSM4;
	}
	else if (strncmp(source.c_str()+1, "s_4_0", 5) == 0)
	{
		startSkip = 6;
		level = kGpuProgramSM4;
	}
	else if (strncmp(source.c_str()+1, "s_5_0", 5) == 0)
	{
		startSkip = 6;
		level = kGpuProgramSM5;
	}
	else
	{
		Assert ("Unknown shader prefix");
	}
	int sourceSize = source.size() - startSkip;
	const char* sourcePtr = source.c_str() + startSkip;

	output.reserve (sourceSize / 2);
	int i = 0;
	while (i < sourceSize)
	{
		char c1 = sourcePtr[i];
		if (c1 >= 'a')
		{
			AssertIf (i+1 == sourceSize);
			char c2 = sourcePtr[i+1];
			output.push_back ((c1-'a') * 16 + (c2-'a'));
			i += 2;
		}
		else
		{
			++i;
		}
	}

	// debug check: does our shader hashing code match D3Ds?
	#if !UNITY_RELEASE
	if (output.size() > 20)
	{
		void D3DHash (const unsigned char* data, unsigned size, unsigned char res[16]);
		UInt8 hsh[16];
		D3DHash (&output[20], output.size()-20, hsh);
		DebugAssert (0 == memcmp(hsh,&output[4],16));
	}
	#endif

	// patch shader code to do driver workarounds
	if (level < kGpuProgramSM4 && gGraphicsCaps.d3d11.buggyPartialPrecision10Level9)
	{
		PatchRemovePartialPrecisionD3D11 (output);
	}

	// patch shader code to do driver workarounds
	if (level < kGpuProgramSM4 && gGraphicsCaps.d3d11.buggyPartialPrecision10Level9)
	{
		PatchRemovePartialPrecisionD3D11 (output);
	}

	return level;
}

static const UInt8* ApplyValueParameters11 (ConstantBuffersD3D11& cbs, const UInt8* buffer, const GpuProgramParameters::ValueParameterArray& valueParams, int cbIndex)
{
	GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
	for (GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd; ++i)
	{
		if (i->m_RowCount == 1)
		{
			// Vector
			const Vector4f* val = reinterpret_cast<const Vector4f*>(buffer);
			if (i->m_Type != kShaderParamInt)
			{
				cbs.SetCBConstant (cbIndex, i->m_Index, val->GetPtr(), i->m_ColCount*4);
			}
			else
			{
				int vali[4] = {val->x, val->y, val->z, val->w};
				cbs.SetCBConstant (cbIndex, i->m_Index, vali, i->m_ColCount*4);
			}
			buffer += sizeof(Vector4f);
		}
		else
		{
			// matrix/array
			int size = *reinterpret_cast<const int*>(buffer); buffer += sizeof(int);
			Assert (i->m_RowCount == 4 && size == 16);
			const Matrix4x4f* val = reinterpret_cast<const Matrix4x4f*>(buffer);
			cbs.SetCBConstant (cbIndex, i->m_Index, val->GetPtr(), 64);
			buffer += size * sizeof(float);
		}
	}
	return buffer;
}

static const UInt8* ApplyBufferParameters11 (GfxDevice& device, ShaderType shaderType, const UInt8* buffer, const GpuProgramParameters::BufferParameterArray& bufferParams)
{
	GfxDeviceD3D11& device11 = static_cast<GfxDeviceD3D11&>(device);

	GpuProgramParameters::BufferParameterArray::const_iterator bufferParamsEnd = bufferParams.end();
	for (GpuProgramParameters::BufferParameterArray::const_iterator i = bufferParams.begin(); i != bufferParamsEnd; ++i)
	{
		ComputeBufferID buf = *reinterpret_cast<const ComputeBufferID*>(buffer);
		device11.SetComputeBuffer11 (shaderType, i->m_Index, buf);
		buffer += sizeof(ComputeBufferID);
	}
	return buffer;
}


// --------------------------------------------------------------------------


D3D11CommonShader::~D3D11CommonShader ()
{
	for (int i = 0; i < kFogModeCount; ++i)
	{
		SAFE_RELEASE(m_Shaders[i]);
	}
}

IUnknown* D3D11CommonShader::GetShader(FogMode fog, bool haveDomainShader, bool& outResetToNoFog)
{
	outResetToNoFog = false;
	// no fog?
	if (fog <= kFogDisabled)
		return m_Shaders[0];

	// already have shader for this fog mode?
	Assert (fog >= 0 && fog < kFogModeCount);
	if (m_Shaders[fog])
		return m_Shaders[fog];

	// can't do fog for this mode?
	unsigned fogBit = (1<<fog);
	if (m_FogFailed & fogBit)
	{
		outResetToNoFog = true;
		return m_Shaders[0];
	}

	// have domain shader and we're vertex - nothing to do; fog delegated to domain one
	if (haveDomainShader && m_ImplType == kShaderImplVertex)
		return m_Shaders[0];

	// patch shader to handle fog
	bool ok = PatchShaderForFog (fog);
	if (!ok)
	{
		m_FogFailed |= fogBit;
		return m_Shaders[0];
	}

	Assert(m_Shaders[fog]);
	return m_Shaders[fog];
}

bool D3D11CommonShader::PatchShaderForFog (FogMode fog)
{
	Assert (fog > kFogDisabled && fog < kFogModeCount);
	Assert (!m_Shaders[fog]);
	IUnknown* s = m_Shaders[0];
	m_Shaders[fog] = s;
	if (s)
		s->AddRef();
	return true;
}


const UInt8* D3D11CommonShader::ApplyTextures (GfxDevice& device, ShaderType shaderType, const GpuProgramParameters& params, const UInt8* buffer)
{
	const GpuProgramParameters::TextureParameterList& textureParams = params.GetTextureParams();
	const GpuProgramParameters::TextureParameterList::const_iterator textureParamsEnd = textureParams.end();
	for (GpuProgramParameters::TextureParameterList::const_iterator i = textureParams.begin(); i != textureParamsEnd; ++i)
	{
		const GpuProgramParameters::TextureParameter& t = *i;
		const TexEnvData* texdata = reinterpret_cast<const TexEnvData*>(buffer);
		device.SetTexture (shaderType, t.m_Index, t.m_SamplerIndex, texdata->textureID, static_cast<TextureDimension>(texdata->texDim), 0);
		buffer += sizeof(*texdata);
	}
	return buffer;
}


// --------------------------------------------------------------------------

D3D11VertexShader::D3D11VertexShader (const std::string& compiledSource)
:	m_InputSignature(NULL)
{
	m_ImplType = kShaderImplVertex;
	if (!Create(compiledSource))
		m_NotSupported = true;
}

D3D11VertexShader::~D3D11VertexShader ()
{
}


bool D3D11VertexShader::Create (const std::string& compiledSource)
{
	m_GpuProgramLevel = DecodeShader (compiledSource, m_ByteCode);

	m_InputSignature = GetD3D11InputSignature (&m_ByteCode[0], m_ByteCode.size());

	HRESULT hr = GetD3D11Device()->CreateVertexShader (&m_ByteCode[0], m_ByteCode.size(), NULL, (ID3D11VertexShader**)&m_Shaders[0]);
	if( FAILED(hr) )
	{
		printf_console ("D3D shader create error for shader %s\n", compiledSource.c_str());
		return false;
	}

	std::string debugName = Format("VS-%d", compiledSource.size());
	hr = ((ID3D11DeviceChild*)m_Shaders[0])->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());

	return true;
}

bool D3D11VertexShader::PatchShaderForFog (FogMode fog)
{
	// no fog patching for 9.x level yet
	if (m_GpuProgramLevel < kGpuProgramSM4)
		return false;

	dynamic_array<UInt8> bc = m_ByteCode;
	bool ok = PatchVertexOrDomainShaderFogD3D11 (bc);
	if (!ok)
	{
		printf_console("DX11: failed to patch vertex shader for fog mode %d\n", fog);
		return false;
	}

	Assert (!m_Shaders[fog]);
	HRESULT hr = GetD3D11Device()->CreateVertexShader (&bc[0], bc.size(), NULL, (ID3D11VertexShader**)&m_Shaders[fog]);
	if (FAILED(hr))
	{
		printf_console ("D3D11 shader create error for VS with fog mode %i\n", fog);
		return false;
	}

	SetDebugNameD3D11 ((ID3D11DeviceChild*)m_Shaders[fog], Format("VS-%d-fog-%d", (int)bc.size(), fog));
	return true;
}


void D3D11VertexShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	g_CurrentVSInputD3D11 = m_InputSignature;

	GfxDevice& device = GetRealGfxDevice();
	ConstantBuffersD3D11& cbs = GetD3D11ConstantBuffers(device);
	cbs.ResetBinds (kShaderVertex);

	for (GpuProgramParameters::ConstantBufferList::const_iterator cbi = params.GetConstantBuffers().begin(); cbi != params.GetConstantBuffers().end(); ++cbi)
	{
		const int cbIndex = cbs.FindAndBindCB (cbi->m_Name.index, kShaderVertex, cbi->m_BindIndex, cbi->m_Size);
		buffer = ApplyValueParameters11 (cbs, buffer, cbi->m_ValueParams, cbIndex);
	}
	buffer = ApplyTextures (device, kShaderVertex, params, buffer);
	buffer = ApplyBufferParameters11 (device, kShaderVertex, buffer, params.GetBufferParams());
}


// --------------------------------------------------------------------------


D3D11PixelShader::D3D11PixelShader (const std::string& compiledSource)
{
	m_ImplType = kShaderImplFragment;
	if (!Create(compiledSource))
		m_NotSupported = true;
}


bool D3D11PixelShader::Create (const std::string& compiledSource)
{
	m_GpuProgramLevel = DecodeShader (compiledSource, m_ByteCode);

	HRESULT hr = GetD3D11Device()->CreatePixelShader (&m_ByteCode[0], m_ByteCode.size(), NULL, (ID3D11PixelShader**)&m_Shaders[0]);
	if( FAILED(hr) )
	{
		printf_console ("D3D shader create error for shader %s\n", compiledSource.c_str());
		return false;
	}

	std::string debugName = Format("PS-%d", compiledSource.size());
	hr = ((ID3D11DeviceChild*)m_Shaders[0])->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());

	return true;
}


bool D3D11PixelShader::PatchShaderForFog (FogMode fog)
{
	// no fog patching for 9.x level yet
	if (m_GpuProgramLevel < kGpuProgramSM4)
		return false;

	dynamic_array<UInt8> bc = m_ByteCode;
	bool ok = PatchPixelShaderFogD3D11 (bc, fog);
	if (!ok)
	{
		printf_console("DX11: failed to patch pixel shader for fog mode %d\n", fog);
		return false;
	}

	Assert (!m_Shaders[fog]);
	HRESULT hr = GetD3D11Device()->CreatePixelShader (&bc[0], bc.size(), NULL, (ID3D11PixelShader**)&m_Shaders[fog]);
	if (FAILED(hr))
	{
		printf_console ("D3D11 shader create error for PS with fog mode %i\n", fog);
		return false;
	}

	SetDebugNameD3D11 ((ID3D11DeviceChild*)m_Shaders[fog], Format("PS-%d-fog-%d", (int)bc.size(), fog));

	return true;
}


void D3D11PixelShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GfxDevice& device = GetRealGfxDevice();
	ConstantBuffersD3D11& cbs = GetD3D11ConstantBuffers(device);
	cbs.ResetBinds (kShaderFragment);

	for (GpuProgramParameters::ConstantBufferList::const_iterator cbi = params.GetConstantBuffers().begin(); cbi != params.GetConstantBuffers().end(); ++cbi)
	{
		const int cbIndex = cbs.FindAndBindCB (cbi->m_Name.index, kShaderFragment, cbi->m_BindIndex, cbi->m_Size);
		buffer = ApplyValueParameters11 (cbs, buffer, cbi->m_ValueParams, cbIndex);
	}

	// Apply textures
	const GpuProgramParameters::TextureParameterList& textureParams = params.GetTextureParams();
	GpuProgramParameters::TextureParameterList::const_iterator textureParamsEnd = textureParams.end();
	for( GpuProgramParameters::TextureParameterList::const_iterator i = textureParams.begin(); i != textureParamsEnd; ++i )
	{
		const GpuProgramParameters::TextureParameter& t = *i;
		const TexEnvData* texdata = reinterpret_cast<const TexEnvData*>(buffer);
		ApplyTexEnvData (t.m_Index, t.m_SamplerIndex, *texdata);
		buffer += sizeof(*texdata);
	}

	buffer = ApplyBufferParameters11 (device, kShaderFragment, buffer, params.GetBufferParams());
}


// --------------------------------------------------------------------------

D3D11GeometryShader::D3D11GeometryShader (const std::string& compiledSource)
{
	m_ImplType = kShaderImplGeometry;
	if (!Create(compiledSource))
		m_NotSupported = true;
}


bool D3D11GeometryShader::Create (const std::string& compiledSource)
{
	m_GpuProgramLevel = DecodeShader (compiledSource, m_ByteCode);

	HRESULT hr = GetD3D11Device()->CreateGeometryShader (&m_ByteCode[0], m_ByteCode.size(), NULL, (ID3D11GeometryShader**)&m_Shaders[0]);
	if( FAILED(hr) )
	{
		printf_console ("D3D shader create error for shader %s\n", compiledSource.c_str());
		return false;
	}

	std::string debugName = Format("GS-%d", compiledSource.size());
	hr = ((ID3D11DeviceChild*)m_Shaders[0])->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());

	return true;
}

void D3D11GeometryShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GfxDevice& device = GetRealGfxDevice();
	ConstantBuffersD3D11& cbs = GetD3D11ConstantBuffers(device);
	cbs.ResetBinds (kShaderGeometry);

	for (GpuProgramParameters::ConstantBufferList::const_iterator cbi = params.GetConstantBuffers().begin(); cbi != params.GetConstantBuffers().end(); ++cbi)
	{
		const int cbIndex = cbs.FindAndBindCB (cbi->m_Name.index, kShaderGeometry, cbi->m_BindIndex, cbi->m_Size);
		buffer = ApplyValueParameters11 (cbs, buffer, cbi->m_ValueParams, cbIndex);
	}
	buffer = ApplyTextures (device, kShaderGeometry, params, buffer);
	buffer = ApplyBufferParameters11 (device, kShaderGeometry, buffer, params.GetBufferParams());
}


// --------------------------------------------------------------------------

D3D11HullShader::D3D11HullShader (const std::string& compiledSource)
{
	m_ImplType = kShaderImplHull;
	if (!Create(compiledSource))
		m_NotSupported = true;
}


bool D3D11HullShader::Create (const std::string& compiledSource)
{
	m_GpuProgramLevel = DecodeShader (compiledSource, m_ByteCode);

	HRESULT hr = GetD3D11Device()->CreateHullShader (&m_ByteCode[0], m_ByteCode.size(), NULL, (ID3D11HullShader**)&m_Shaders[0]);
	if( FAILED(hr) )
	{
		printf_console ("D3D shader create error for shader %s\n", compiledSource.c_str());
		return false;
	}

	std::string debugName = Format("HS-%d", compiledSource.size());
	hr = ((ID3D11DeviceChild*)m_Shaders[0])->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());

	return true;
}

void D3D11HullShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GfxDevice& device = GetRealGfxDevice();
	ConstantBuffersD3D11& cbs = GetD3D11ConstantBuffers(device);
	cbs.ResetBinds (kShaderHull);

	for (GpuProgramParameters::ConstantBufferList::const_iterator cbi = params.GetConstantBuffers().begin(); cbi != params.GetConstantBuffers().end(); ++cbi)
	{
		const int cbIndex = cbs.FindAndBindCB (cbi->m_Name.index, kShaderHull, cbi->m_BindIndex, cbi->m_Size);
		buffer = ApplyValueParameters11 (cbs, buffer, cbi->m_ValueParams, cbIndex);
	}
	buffer = ApplyTextures (device, kShaderHull, params, buffer);
	buffer = ApplyBufferParameters11 (device, kShaderHull, buffer, params.GetBufferParams());
}


// --------------------------------------------------------------------------

D3D11DomainShader::D3D11DomainShader (const std::string& compiledSource)
{
	m_ImplType = kShaderImplDomain;
	if (!Create(compiledSource))
		m_NotSupported = true;
}

bool D3D11DomainShader::Create (const std::string& compiledSource)
{
	m_GpuProgramLevel = DecodeShader (compiledSource, m_ByteCode);

	HRESULT hr = GetD3D11Device()->CreateDomainShader (&m_ByteCode[0], m_ByteCode.size(), NULL, (ID3D11DomainShader**)&m_Shaders[0]);
	if( FAILED(hr) )
	{
		printf_console ("D3D shader create error for shader %s\n", compiledSource.c_str());
		return false;
	}

	std::string debugName = Format("DS-%d", compiledSource.size());
	hr = ((ID3D11DeviceChild*)m_Shaders[0])->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());

	return true;
}

bool D3D11DomainShader::PatchShaderForFog (FogMode fog)
{
	// no fog patching for 9.x level yet
	if (m_GpuProgramLevel < kGpuProgramSM4)
		return false;

	dynamic_array<UInt8> bc = m_ByteCode;
	bool ok = PatchVertexOrDomainShaderFogD3D11 (bc);
	if (!ok)
	{
		printf_console("DX11: failed to patch domain shader for fog mode %d\n", fog);
		return false;
	}

	Assert (!m_Shaders[fog]);
	HRESULT hr = GetD3D11Device()->CreateDomainShader (&bc[0], bc.size(), NULL, (ID3D11DomainShader**)&m_Shaders[fog]);
	if (FAILED(hr))
	{
		printf_console ("D3D11 shader create error for DS with fog mode %i\n", fog);
		return false;
	}

	SetDebugNameD3D11 ((ID3D11DeviceChild*)m_Shaders[fog], Format("DS-%d-fog-%d", (int)bc.size(), fog));
	return true;
}


void D3D11DomainShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GfxDevice& device = GetRealGfxDevice();
	ConstantBuffersD3D11& cbs = GetD3D11ConstantBuffers(device);
	cbs.ResetBinds (kShaderDomain);

	for (GpuProgramParameters::ConstantBufferList::const_iterator cbi = params.GetConstantBuffers().begin(); cbi != params.GetConstantBuffers().end(); ++cbi)
	{
		const int cbIndex = cbs.FindAndBindCB (cbi->m_Name.index, kShaderDomain, cbi->m_BindIndex, cbi->m_Size);
		buffer = ApplyValueParameters11 (cbs, buffer, cbi->m_ValueParams, cbIndex);
	}
	buffer = ApplyTextures (device, kShaderDomain, params, buffer);
	buffer = ApplyBufferParameters11 (device, kShaderDomain, buffer, params.GetBufferParams());
}


// --------------------------------------------------------------------------


void FixedFunctionProgramD3D11::ValueParameters::ApplyValues (const BuiltinShaderParamValues& values, ConstantBuffersD3D11& cbs, ShaderType shaderType) const
{
	int cbBindIndex = cbs.FindAndBindCB (m_CBID, shaderType, 0, m_CBSize);

	ValueParameterArray::const_iterator valueParamsEnd = m_Params.end();
	for (ValueParameterArray::const_iterator i = m_Params.begin(); i != valueParamsEnd; ++i)
	{
		const Vector4f& val = values.GetVectorParam((BuiltinShaderVectorParam)i->m_Name);
		cbs.SetCBConstant (cbBindIndex, i->m_Index, &val, i->m_Bytes);
	}
}


// --------------------------------------------------------------------------


static std::string GetFixedFunctionStateDesc (const FixedFunctionStateD3D11& state)
{
	std::string res;
	if (state.lightingEnabled)
	{
		res += Format("  lighting with %i lights\n", state.lightCount);
	}
	res += Format("  combiners: %i\n", state.texUnitCount);
	for (int i = 0; i < state.texUnitCount; ++i)
	{
		res += Format ("    #%i: %08x %08x uv=%i %s %s\n",
			i, state.texUnitColorCombiner[i], state.texUnitAlphaCombiner[i],
			unsigned((state.texUnitSources>>(i*4))&0xF),
			(state.texUnitCube&(1<<i))?"cube":"2d",
			(state.texUnitProjected&(1<<i))?"projected":"");
	}
	res += state.useUniformInsteadOfVertexColor ? "  color from uniform" : "  color from VBO\n";
	if (state.alphaTest != kFuncDisabled && state.alphaTest != kFuncAlways)
		res += Format("  alpha test: %d\n", state.alphaTest);
	return res;
}

#if UNITY_METRO_VS2013 || (UNITY_WIN && !UNITY_WINRT)
extern bool HasD3D11Linker();
extern void* BuildVertexShaderD3D11_Link(const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, BuiltinShaderParamIndices& matrices, size_t& outSize);
extern void* BuildFragmentShaderD3D11_Link(const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, size_t& outSize);
#endif

FixedFunctionProgramD3D11::FixedFunctionProgramD3D11 (const FixedFunctionStateD3D11& state)
:	m_VS(NULL)
,	m_PS(NULL)
,	m_InputSig(NULL)
{
	size_t sizeVS, sizePS;

	void* codeVS = NULL;
	void* codePS = NULL;

	// Generate using DX11 shader linker if available
	#if UNITY_METRO_VS2013 || (UNITY_WIN && !UNITY_WINRT)
	if (HasD3D11Linker())
	{
		codeVS = BuildVertexShaderD3D11_Link(state, m_VPParams, m_VPMatrices, sizeVS);
		if (codeVS)
			codePS = BuildFragmentShaderD3D11_Link(state, m_FPParams, sizePS);
	}
	#endif

	// If linker failed or not available, generate raw hlsl bytecode
	if (!codeVS || !codePS)
	{
		if (codeVS)
			free(codeVS);
		if (codePS)
			free(codePS);
		codeVS = BuildVertexShaderD3D11(state, m_VPParams, m_VPMatrices, sizeVS);
		codePS = BuildFragmentShaderD3D11(state, m_FPParams, sizePS);
	}

	// Both generators failed, give up
	if (!codeVS || !codePS)
	{
		ErrorString ("Failed to create fixed function shader pair");
		if (codeVS)
			free(codeVS);
		if (codePS)
			free(codePS);
		return;
	}

	/*
	// debug check: does our shader hashing code match D3Ds?
	#if !UNITY_RELEASE
	if (sizeVS > 20 && sizePS > 20)
	{
		void D3DHash (const unsigned char* data, unsigned size, unsigned char res[16]);
		UInt8 hsh[16];
		D3DHash (&codeVS[20], sizeVS-20, hsh);
		DebugAssert (0 == memcmp(hsh,&codeVS[4],16));
		D3DHash (&codePS[20], sizePS-20, hsh);
		DebugAssert (0 == memcmp(hsh,&codePS[4],16));
	}
	// dump vertex shader code
	DXBCContainer* dxbc = dxbc_parse (codeVS, sizeVS);
	dxbc_print (dxbc);
	delete dxbc;
	#endif
	*/

#if _DEBUG && !UNITY_METRO && 0
	static int s_Num = 0;
	char sz[1024];

	sprintf(sz,"dump%04d.vs",s_Num);
	FILE* f = fopen(sz,"wb");
	fwrite(codeVS,sizeVS,1,f);
	fclose(f);
	sprintf(sz,"dump%04d.ps",s_Num);
	f = fopen(sz,"wb");
	fwrite(codePS,sizePS,1,f);
	fclose(f);
	s_Num++;
#endif


	ID3D11Device* dev = GetD3D11Device();
	HRESULT hr;
	hr = dev->CreateVertexShader (codeVS, sizeVS, NULL, &m_VS);
	Assert (SUCCEEDED(hr));
	hr = dev->CreatePixelShader (codePS, sizePS, NULL, &m_PS);
	Assert (SUCCEEDED(hr));

	std::string debugName = Format("FixedFunctionVS-%d", sizeVS);
	hr = m_VS->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());
	debugName = Format("FixedFunctionPS-%d", sizePS);
	hr = m_PS->SetPrivateData (WKPDID_D3DDebugObjectName, debugName.size(), debugName.c_str());

	m_InputSig = GetD3D11InputSignature (codeVS, sizeVS);

	free(codeVS);
	free(codePS);
}

FixedFunctionProgramD3D11::~FixedFunctionProgramD3D11 ()
{
	SAFE_RELEASE(m_VS);
	SAFE_RELEASE(m_PS);
}


void FixedFunctionProgramD3D11::ApplyFFGpuProgram (const BuiltinShaderParamValues& values, ConstantBuffersD3D11& cbs) const
{
	g_CurrentVSInputD3D11 = m_InputSig;

	cbs.ResetBinds (kShaderVertex);
	cbs.ResetBinds (kShaderFragment);

	m_VPParams.ApplyValues (values, cbs, kShaderVertex);
	m_FPParams.ApplyValues (values, cbs, kShaderFragment);
}
