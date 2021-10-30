#include "UnityPrefix.h"
#include "GpuProgramsD3D.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/Vector4.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/DirectX/builds/dx9include/d3dx9.h"
#include "D3D9Context.h"
#include "Runtime/GfxDevice/ShaderConstantCache.h"
#include "D3D9Utils.h"
#include "ShaderPatchingD3D9.h"

#define ENABLE_GPU_PROGRAM_STATS 0


#if ENABLE_GPU_PROGRAM_STATS
typedef std::map<ShaderLab::FastPropertyName, int> PropertyCount;
PropertyCount s_StatCounts[kShaderTypeCount];
void PrintDebugGpuProgramStats ()
{
	typedef std::pair<std::string, int> NameIntPair;
	struct Sorter {
		bool operator() (const NameIntPair& a, const NameIntPair& b) const {
			return a.second > b.second;
		}
	};
	for (int i = kShaderVertex; i < kShaderTypeCount; ++i)
	{
		std::vector<NameIntPair> sorted;
		sorted.reserve (s_StatCounts[i].size());
		int totalCount = 0;
		for (PropertyCount::const_iterator it = s_StatCounts[i].begin(); it != s_StatCounts[i].end(); ++it)
		{
			sorted.push_back (std::make_pair(it->first.GetName(), it->second));
			totalCount += it->second;
		}
		std::sort (sorted.begin(), sorted.end(), Sorter());
		printf_console ("%i Shader Stats: %i props, %i requests\n", i, sorted.size(), totalCount);
		for (size_t j = 0; j < sorted.size(); ++j)
		{
			printf_console ("  %-25s %6i %5.1f%%\n", sorted[j].first.c_str(), sorted[j].second, sorted[j].second*100.0/totalCount);
		}
		s_StatCounts[i].clear();
	}
}
#define ADD_TO_VS_STATS(name) ++s_StatCounts[kShaderVertex][name]
#define ADD_TO_PS_STATS(name) ++s_StatCounts[kShaderFragment][name]
#else
#define ADD_TO_VS_STATS(name)
#define ADD_TO_PS_STATS(name)
#endif


VertexShaderConstantCache& GetD3D9VertexShaderConstantCache(); // GfxDeviceD3D9.cpp
PixelShaderConstantCache& GetD3D9PixelShaderConstantCache(); // GfxDeviceD3D9.cpp


// non static; used by CombinerD3D.cpp and VertexPipeD3D9.cpp
ID3DXBuffer* AssembleD3DShader (const std::string& source)
{
	ID3DXBuffer *compiledShader, *compileErrors;

	// Skip validation of shaders at assembly time when in release mode. Saves
	// some time when loading them.
	DWORD flags = D3DXSHADER_SKIPVALIDATION;
	#if DEBUGMODE
	flags = 0;
	#endif

	HRESULT hr = D3DXAssembleShader( source.c_str(), source.size(), NULL, NULL, flags, &compiledShader, &compileErrors );
	if( FAILED(hr) )
	{
		if (compileErrors && compileErrors->GetBufferSize() > 0)
		{
			std::string error = Format ("Shader error in '%s': D3D shader assembly failed with: %s\nShader Assembly: %s", g_LastParsedShaderName.c_str(), (const char*)compileErrors->GetBufferPointer(), source.c_str());
			compileErrors->Release();
			ErrorString (error);
		}
		if( compiledShader )
			compiledShader->Release();
		return NULL;
	}

	return compiledShader;
}

// --------------------------------------------------------------------------

template <typename CACHE>
static const UInt8* ApplyValueParametersD3D9 (CACHE& constantCache, const UInt8* buffer, const GpuProgramParameters::ValueParameterArray& valueParams)
{
	GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
	for (GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd; ++i)
	{
		if (i->m_RowCount == 1 && i->m_ArraySize == 1)
		{
			// Apply vector parameters
			const Vector4f* val = reinterpret_cast<const Vector4f*>(buffer);
			constantCache.SetValues(i->m_Index, val->GetPtr(), 1);
			buffer += sizeof(Vector4f);
		}
		else
		{
			// matrix/array
			int size = *reinterpret_cast<const int*>(buffer); buffer += sizeof(int);
			Assert (i->m_RowCount == 4 && size == 16);
			const Matrix4x4f* val = reinterpret_cast<const Matrix4x4f*>(buffer);
			Matrix4x4f transposed;
			TransposeMatrix4x4 (val, &transposed);
			const float *ptr = transposed.GetPtr();
			constantCache.SetValues (i->m_Index, ptr, 4);
			buffer += size * sizeof(float);
		}
	}
	return buffer;
}



// --------------------------------------------------------------------------

D3D9VertexShader::D3D9VertexShader( const std::string& source )
:	m_FogFailed(0)
{
	for (int i = 0; i < kFogModeCount; ++i)
	{
		m_Shaders[i] = NULL;
	}
	m_ImplType = kShaderImplVertex;
	if( !Create(source) )
		m_NotSupported = true;
}

D3D9VertexShader::~D3D9VertexShader ()
{
	for (int i = 0; i < kFogModeCount; ++i)
	{
		if( m_Shaders[i] )
		{
			ULONG refCount = m_Shaders[i]->Release();
			AssertIf( refCount != 0 );
		}
	}
}


bool D3D9VertexShader::Create( const std::string& source )
{
	// fast skip 3.0 shaders on unsupporting hardware
	bool isShaderModel3 = !strncmp(source.c_str(), "vs_3_0", 6);
	if( gGraphicsCaps.shaderCaps < kShaderLevel3 && isShaderModel3 )
		return false;

	if (isShaderModel3)
		m_GpuProgramLevel = kGpuProgramSM3;
	else
	{
		bool isShaderModel1 = !strncmp(source.c_str(), "vs_1_1", 6);
		m_GpuProgramLevel = isShaderModel1 ? kGpuProgramSM1 : kGpuProgramSM2;
	}

	HRESULT hr;
	IDirect3DDevice9* dev = GetD3DDevice();

	// assemble shader
	ID3DXBuffer *compiledShader = AssembleD3DShader( source );
	if( !compiledShader )
	{
		return false;
	}

	// create shader
	hr = dev->CreateVertexShader( (const DWORD*)compiledShader->GetBufferPointer(), &m_Shaders[0] );
	compiledShader->Release();
	if( FAILED(hr) )
	{
		printf_console( "D3D shader create error for shader %s\n", source.c_str() );
		return false;
	}

	if (isShaderModel3)
	{
		m_SourceForFog = source;
	}

	return true;
}

void D3D9VertexShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GfxDevice& device = GetRealGfxDevice();
	IDirect3DDevice9* dev = GetD3DDevice();
	VertexShaderConstantCache& constantCache = GetD3D9VertexShaderConstantCache();

	const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
	buffer = ApplyValueParametersD3D9<VertexShaderConstantCache>(constantCache, buffer, valueParams);

	// Apply textures
	if (gGraphicsCaps.hasVertexTextures)
	{
		const GpuProgramParameters::TextureParameterList& textureParams = params.GetTextureParams();
		const GpuProgramParameters::TextureParameterList::const_iterator textureParamsEnd = textureParams.end();
		for( GpuProgramParameters::TextureParameterList::const_iterator i = textureParams.begin(); i != textureParamsEnd; ++i )
		{
			const GpuProgramParameters::TextureParameter& t = *i;
			const TexEnvData* texdata = reinterpret_cast<const TexEnvData*>(buffer);
			device.SetTexture (kShaderVertex, t.m_Index, 0, texdata->textureID, static_cast<TextureDimension>(texdata->texDim), 0);
			buffer += sizeof(*texdata);
		}
	}
}

IDirect3DVertexShader9* D3D9VertexShader::GetShader (FogMode fog, bool& outResetToNoFog)
{
	int index = 0;
	outResetToNoFog = false;
	if (fog > kFogDisabled && !m_SourceForFog.empty())
	{
		Assert (fog >= 0 && fog < kFogModeCount);

		if (m_Shaders[fog])
		{
			// already have patched fog shader
			index = fog;
		}
		else if (!(m_FogFailed & (1<<fog)))
		{
			// patch fog shader on demand
			std::string src = m_SourceForFog;

			if (PatchVertexShaderFogD3D9 (src))
			{
				// assemble & create the shader
				ID3DXBuffer *compiledShader = AssembleD3DShader (src);
				if (compiledShader)
				{
					HRESULT hr = GetD3DDevice()->CreateVertexShader ((const DWORD*)compiledShader->GetBufferPointer(), &m_Shaders[fog]);
					compiledShader->Release();
					if (SUCCEEDED(hr))
					{
						index = fog;
					}
					else
					{
						printf_console ("D3D vertex shader create error for patched fog mode %d shader %s\n", (int)fog, src.c_str());
					}
				}
			}
		}
		if (index == 0)
		{
			outResetToNoFog = true;
			m_FogFailed |= (1<<fog);
		}
	}
	return m_Shaders[index];
}

// --------------------------------------------------------------------------

D3D9PixelShader::D3D9PixelShader( const std::string& source )
:	m_FogFailed(0)
{
	for (int i = 0; i < kFogModeCount; ++i)
	{
		m_Shaders[i] = NULL;
		m_FogRegisters[i] = NULL;
	}
	m_ImplType = kShaderImplFragment;
	if( !Create(source) )
		m_NotSupported = true;
}

D3D9PixelShader::~D3D9PixelShader ()
{
	for (int i = 0; i < kFogModeCount; ++i)
	{
		if( m_Shaders[i] )
		{
			ULONG refCount = m_Shaders[i]->Release();
			AssertIf( refCount != 0 );
		}
	}
}

bool D3D9PixelShader::Create( const std::string& source )
{
	// fast skip 3.0 shaders on unsupporting hardware
	bool isShaderModel3 = !strncmp(source.c_str(), "ps_3_0", 6);
	if( gGraphicsCaps.shaderCaps < kShaderLevel3 && isShaderModel3 )
		return false;

	m_GpuProgramLevel = isShaderModel3 ? kGpuProgramSM3 : kGpuProgramSM2;
	
	HRESULT hr;
	IDirect3DDevice9* dev = GetD3DDevice();

	// assemble shader
	ID3DXBuffer *compiledShader = AssembleD3DShader( source );
	if( !compiledShader )
	{
		return false;
	}

	// create shader
	hr = dev->CreatePixelShader( (const DWORD*)compiledShader->GetBufferPointer(), &m_Shaders[0] );
	compiledShader->Release();
	if( FAILED(hr) )
	{
		printf_console( "D3D shader create error for shader %s\n", source.c_str() );
		return false;
	}

	if (isShaderModel3)
	{
		m_SourceForFog = source;
	}

	return true;
}

void D3D9PixelShader::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GfxDevice& device = GetRealGfxDevice();
	IDirect3DDevice9* dev = GetD3DDevice();
	PixelShaderConstantCache& constantCache = GetD3D9PixelShaderConstantCache();

	const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
	buffer = ApplyValueParametersD3D9<PixelShaderConstantCache>(constantCache, buffer, valueParams);

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

	// Apply fog parameters if needed
	if (!m_SourceForFog.empty())
	{
		const GfxFogParams& fog = device.GetFogParams();
		if (fog.mode > kFogDisabled && !(m_FogFailed & (1<<fog.mode)))
		{
			int reg = m_FogRegisters[fog.mode];
			constantCache.SetValues (reg, fog.color.GetPtr(), 1);
			float params[4];
			params[0] = fog.density * 1.2011224087f ; // density / sqrt(ln(2))
			params[1] = fog.density * 1.4426950408f; // density / ln(2)
			if (fog.mode == kFogLinear)
			{
				float diff = fog.end - fog.start;
				float invDiff = Abs(diff) > 0.0001f ? 1.0f/diff : 0.0f;
				params[2] = -invDiff;
				params[3] = fog.end * invDiff;
			}
			else
			{
				params[2] = 0.0f;
				params[3] = 0.0f;
			}
			constantCache.SetValues (reg+1, params, 1);
		}
	}
}

static int FindUnusedConstantRegister (const std::string& src, const GpuProgramParameters& params)
{
	int maxRegisterUsed = -1;

	const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
	for (GpuProgramParameters::ValueParameterArray::const_iterator it = valueParams.begin(), itEnd = valueParams.end(); it != itEnd; ++it)
	{
		int idx = it->m_Index + it->m_RowCount - 1;
		if (idx > maxRegisterUsed)
			maxRegisterUsed = idx;
	}

	// Built-ins
	const BuiltinShaderParamIndices& builtins = params.GetBuiltinParams();
	for (int i = 0; i < kShaderInstanceMatCount; ++i)
	{
		int index = builtins.mat[i].gpuIndex;
		if (index >= 0 && index + 3 > maxRegisterUsed)
			maxRegisterUsed = index + 3;
	}

	// Explicit constants in the shader ("def c*")
	size_t pos = 0;
	const size_t n = src.size();
	while ((pos = src.find("def c", pos)) != std::string::npos)
	{
		pos += 5; // skip "def c"
		int reg = -1;
		sscanf(src.c_str() + pos, "%d", &reg);
		if (reg > maxRegisterUsed)
			maxRegisterUsed = reg;
	}

	return maxRegisterUsed + 1;
}

IDirect3DPixelShader9* D3D9PixelShader::GetShader(FogMode fog, const GpuProgramParameters& params)
{
	int index = 0;
	if (fog > kFogDisabled && !m_SourceForFog.empty())
	{
		Assert (fog >= 0 && fog < kFogModeCount);

		if (m_Shaders[fog])
		{
			// already have patched fog shader
			index = fog;
		}
		else if (!(m_FogFailed & (1<<fog)))
		{
			// patch fog shader on demand
			std::string src = m_SourceForFog;

			// find constant register that we'll use to store fog params
			int reg = FindUnusedConstantRegister (src, params);
			m_FogRegisters[fog] = reg;

			if (PatchPixelShaderFogD3D9 (src, fog, reg, reg+1))
			{
				// assemble & create the shader
				ID3DXBuffer *compiledShader = AssembleD3DShader (src);
				if (compiledShader)
				{
					HRESULT hr = GetD3DDevice()->CreatePixelShader ((const DWORD*)compiledShader->GetBufferPointer(), &m_Shaders[fog]);
					compiledShader->Release();
					if (SUCCEEDED(hr))
					{
						index = fog;
					}
					else
					{
						printf_console ("D3D pixel shader create error for patched fog mode %d shader %s\n", (int)fog, src.c_str());
					}
				}
			}

			if (index == 0)
				m_FogFailed |= (1<<fog);
		}
	}
	return m_Shaders[index];
}



// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
SUITE (GpuProgramsD3DTests)
{

TEST(FindUnusedConstantRegisterCanHandleUnsortedParams)
{
	GpuProgramParameters pp;
	pp.AddVectorParam(1,kShaderParamFloat,4,"A",-1,NULL);
	pp.AddVectorParam(0,kShaderParamFloat,4,"B",-1,NULL);
	pp.MakeReady(); // this does sort, but sorts by name; NOT the GPU index!
	CHECK_EQUAL(2,FindUnusedConstantRegister("", pp));
}

} // SUITE
#endif // ENABLE_UNIT_TESTS
