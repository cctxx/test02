#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "D3D11Context.h"
#include "D3D11VBO.h"
#include "External/shaderlab/Library/program.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/GfxDevice/GpuProgramParamsApply.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "External/shaderlab/Library/properties.h"
#include "D3D11Utils.h"
#include "GpuProgramsD3D11.h"
#include "ShaderPatchingD3D11.h"
#include "TimerQueryD3D11.h"
#include "PlatformDependent/Win/SmartComPointer.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Misc/Plugins.h"
#if UNITY_EDITOR
#include "D3D11Window.h"
#endif
#include "Runtime/GfxDevice/d3d11/StreamOutSkinnedMesh.h"

class GfxDeviceD3D11;

namespace ShaderLab {
	TexEnv* GetTexEnvForBinding( const TextureBinding& binding, const PropertySheet* props ); // pass.cpp
}


#include "GfxDeviceD3D11.h"

extern const InputSignatureD3D11* g_CurrentVSInputD3D11;
extern ID3D11InputLayout* g_ActiveInputLayoutD3D11;
extern D3D11_PRIMITIVE_TOPOLOGY g_ActiveTopologyD3D11;


static ShaderLab::FastPropertyName kSLPropFogCB = ShaderLab::Property ("UnityFogPatchCB");



static const D3D11_COMPARISON_FUNC kCmpFuncD3D11[] = {
	D3D11_COMPARISON_ALWAYS, D3D11_COMPARISON_NEVER, D3D11_COMPARISON_LESS, D3D11_COMPARISON_EQUAL, D3D11_COMPARISON_LESS_EQUAL, D3D11_COMPARISON_GREATER, D3D11_COMPARISON_NOT_EQUAL, D3D11_COMPARISON_GREATER_EQUAL, D3D11_COMPARISON_ALWAYS
};

static const D3D11_STENCIL_OP kStencilOpD3D11[] = {
	D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_ZERO, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_INCR_SAT,
	D3D11_STENCIL_OP_DECR_SAT, D3D11_STENCIL_OP_INVERT, D3D11_STENCIL_OP_INCR, D3D11_STENCIL_OP_DECR
};

// Graphics device requires access to reset render textures
bool RebindActiveRenderTargets (TexturesD3D11* textures);

DXGI_FORMAT GetRenderTextureFormat (RenderTextureFormat format, bool sRGB);
DXGI_FORMAT GetShaderResourceViewFormat (RenderTextureFormat format, bool sRGB);
extern DXGI_FORMAT kD3D11RenderResourceFormats[kRTFormatCount];
extern DXGI_FORMAT kD3D11RenderTextureFormatsNorm[kRTFormatCount];


bool SetTopologyD3D11 (GfxPrimitiveType topology, GfxDevice& device, ID3D11DeviceContext* ctx);
void SetInputLayoutD3D11 (ID3D11DeviceContext* ctx, ID3D11InputLayout* layout);


// --------------------------------------------------------------------------


ResolveTexturePool::ResolveTexturePool()
:	m_UseCounter(0)
{
	memset (m_Entries, 0, sizeof(m_Entries));
}

void ResolveTexturePool::Clear()
{
	for (int i = 0; i < ARRAY_SIZE(m_Entries); ++i)
	{
		SAFE_RELEASE(m_Entries[i].texture);
		SAFE_RELEASE(m_Entries[i].srv);
	}
}

ResolveTexturePool::Entry* ResolveTexturePool::GetResolveTexture (int width, int height, RenderTextureFormat fmt, bool sRGB)
{
	++m_UseCounter;

	// check if we have a suitable temporary resolve texture already?
	int newIndex = -1;
	int lruIndex = 0;
	int lruScore = 0;
	for (int i = 0; i < ARRAY_SIZE(m_Entries); ++i)
	{
		Entry& e = m_Entries[i];
		if (e.width == width && e.height == height && e.format == fmt && e.sRGB == sRGB)
		{
			Assert (e.texture);
			Assert (e.srv);
			e.lastUse = m_UseCounter;
			return &e;
		}

		if (e.width == 0)
		{
			// unused slot
			Assert (e.height == 0 && !e.texture && !e.srv);
			if (newIndex == -1)
				newIndex = i;
		}
		else
		{
			// used slot
			if (m_UseCounter - e.lastUse > lruScore)
			{
				lruIndex = i;
				lruScore = m_UseCounter - e.lastUse;
			}
		}
	}

	// if all slots are used; release least recently used
	if (newIndex == -1)
	{
		Entry& e = m_Entries[lruIndex];
		Assert (e.texture);
		e.width = e.height = 0;
		SAFE_RELEASE(e.texture);
		SAFE_RELEASE(e.srv);
		newIndex = lruIndex;
	}

	Entry& ee = m_Entries[newIndex];

	// create texture & SRV in this slot

	ID3D11Device* dev = GetD3D11Device();
	D3D11_TEXTURE2D_DESC tDesc;
	tDesc.Width = width;
	tDesc.Height = height;
	tDesc.MipLevels = 1;
	tDesc.ArraySize = 1;
	tDesc.Format = (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 ? kD3D11RenderResourceFormats[fmt] : kD3D11RenderTextureFormatsNorm[fmt]);

	tDesc.SampleDesc.Count = 1;
	tDesc.SampleDesc.Quality = 0;
	tDesc.Usage = D3D11_USAGE_DEFAULT;
	tDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	// 9.x feature levels require the resolved texture to also have a render target flag, otherwise
	// CopySubresourceRegion will silently corrupt runtime/driver state.
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0)
		tDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;

	tDesc.CPUAccessFlags = 0;
	tDesc.MiscFlags = 0;

	HRESULT hr = dev->CreateTexture2D (&tDesc, NULL, &ee.texture);
	if (FAILED(hr))
		return NULL;
	SetDebugNameD3D11 (ee.texture, Format("ResolveTexture2D-%dx%d", width, height));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = GetShaderResourceViewFormat (fmt, (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0) ? sRGB : false);
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	hr = dev->CreateShaderResourceView (ee.texture, &srvDesc, &ee.srv);
	if (FAILED(hr))
		return NULL;
	SetDebugNameD3D11 (ee.srv, Format("ResolveTexture2D-SRV-%dx%d", width, height));

	ee.width = width;
	ee.height = height;
	ee.format = fmt;
	ee.sRGB = sRGB;
	ee.lastUse = m_UseCounter;

	return &ee;
}


// --------------------------------------------------------------------------


static FixedFunctionProgramD3D11* GetFixedFunctionProgram11 (FFProgramCacheD3D11& cache, const FixedFunctionStateD3D11& state)
{
	// Do we have one for this state already?
	FFProgramCacheD3D11::iterator cachedProgIt = cache.find (state);
	if (cachedProgIt != cache.end())
		return cachedProgIt->second;

	// Don't have one yet, create it
	FixedFunctionProgramD3D11* ffProg = new FixedFunctionProgramD3D11 (state);
	cache.insert (std::make_pair(state, ffProg));
	return ffProg;
}



// --------------------------------------------------------------------------



void GfxDeviceD3D11::SetupDeferredDepthStencilState ()
{
	ID3D11DepthStencilState* dss = m_CurrDSState;
	if (!dss)
	{
		DepthStencilState state;
		memset (&state, 0, sizeof(state));
		if (m_CurrDepthState)
			state.d = *m_CurrDepthState;
		if (m_CurrStencilState)
			state.s = *m_CurrStencilState;

		CachedDepthStencilStates::iterator it = m_CachedDepthStencilStates.find(state);
		if (it == m_CachedDepthStencilStates.end())
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			memset (&desc, 0, sizeof(desc));
			if (m_CurrDepthState)
			{
				desc.DepthEnable = TRUE;
				desc.DepthWriteMask = (m_CurrDepthState->sourceState.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO);
				desc.DepthFunc = kCmpFuncD3D11[m_CurrDepthState->sourceState.depthFunc];
			}
			if (m_CurrStencilState)
			{
				desc.StencilEnable = m_CurrStencilState->sourceState.stencilEnable;
				desc.StencilReadMask = m_CurrStencilState->sourceState.readMask;
				desc.StencilWriteMask = m_CurrStencilState->sourceState.writeMask;
				desc.FrontFace.StencilFunc = kCmpFuncD3D11[m_CurrStencilState->sourceState.stencilFuncFront];
				desc.FrontFace.StencilFailOp = kStencilOpD3D11[m_CurrStencilState->sourceState.stencilFailOpFront];
				desc.FrontFace.StencilDepthFailOp = kStencilOpD3D11[m_CurrStencilState->sourceState.stencilZFailOpFront];
				desc.FrontFace.StencilPassOp = kStencilOpD3D11[m_CurrStencilState->sourceState.stencilPassOpFront];
				desc.BackFace.StencilFunc = kCmpFuncD3D11[m_CurrStencilState->sourceState.stencilFuncBack];
				desc.BackFace.StencilFailOp = kStencilOpD3D11[m_CurrStencilState->sourceState.stencilFailOpBack];
				desc.BackFace.StencilDepthFailOp = kStencilOpD3D11[m_CurrStencilState->sourceState.stencilZFailOpBack];
				desc.BackFace.StencilPassOp = kStencilOpD3D11[m_CurrStencilState->sourceState.stencilPassOpBack];
			}

			ID3D11DepthStencilState* d3dstate = NULL;
			HRESULT hr = GetD3D11Device()->CreateDepthStencilState (&desc, &d3dstate);
			Assert(SUCCEEDED(hr));
			SetDebugNameD3D11 (d3dstate, Format("DepthStencilState-%d-%d", desc.DepthWriteMask, desc.DepthFunc));
			it = m_CachedDepthStencilStates.insert (std::make_pair(state, d3dstate)).first;
		}
		dss = it->second;
	}
	if (dss != m_CurrDSState || m_StencilRef != m_CurrStencilRef)
	{
		GetD3D11Context()->OMSetDepthStencilState (dss, m_StencilRef);
		m_CurrDSState = dss;
		m_CurrStencilRef = m_StencilRef;
	}
}

void GfxDeviceD3D11::SetupDeferredRasterState ()
{
	// raster state; needs to be deferred due to cull winding / scissor / wireframe
	// not known at creation time
	if (!m_CurrRasterState)
		return;

	ID3D11RasterizerState* rss = m_CurrRSState;
	if (!rss)
	{
		FinalRasterState11 rsKey;
		memcpy (&rsKey.raster, &m_CurrRasterState->sourceState, sizeof(rsKey.raster));
		rsKey.backface = (m_CurrRasterState->sourceState.cullMode != kCullOff) && ((m_AppBackfaceMode==m_UserBackfaceMode) == m_InvertProjMatrix);
		rsKey.wireframe = m_Wireframe;
		rsKey.scissor = m_Scissor;

		CachedFinalRasterStates::iterator it = m_CachedFinalRasterStates.find(rsKey);
		if (it == m_CachedFinalRasterStates.end())
		{
			D3D11_RASTERIZER_DESC desc;
			memset (&desc, 0, sizeof(desc));

			desc.FrontCounterClockwise = rsKey.backface ? TRUE : FALSE;
			//TODO: wtf??? DepthBias doesn't work at 9.1
			if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0)
				desc.DepthBias = rsKey.raster.sourceState.depthBias;
			desc.SlopeScaledDepthBias = rsKey.raster.sourceState.slopeScaledDepthBias;
			desc.ScissorEnable = m_Scissor;
			desc.MultisampleEnable = TRUE; // only applies to line drawing in MSAA; if set to FALSE lines will be aliased even when MSAA is used
			desc.DepthClipEnable = TRUE;
			desc.FillMode = rsKey.wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
			switch (rsKey.raster.sourceState.cullMode)
			{
			case kCullOff: desc.CullMode = D3D11_CULL_NONE; break;
			case kCullFront: desc.CullMode = D3D11_CULL_FRONT; break;
			case kCullBack: desc.CullMode = D3D11_CULL_BACK; break;
			default: AssertIf("Unsupported cull mode!");
			}
			ID3D11RasterizerState* d3dstate = NULL;
			HRESULT hr = GetD3D11Device()->CreateRasterizerState (&desc, &d3dstate);
			Assert(SUCCEEDED(hr));
			SetDebugNameD3D11 (d3dstate, Format("RasterizerState-%d-%d", desc.FrontCounterClockwise, desc.FillMode));
			it = m_CachedFinalRasterStates.insert (std::make_pair(rsKey, d3dstate)).first;
		}
		rss = it->second;
	}
	if (rss != m_CurrRSState)
	{
		GetD3D11Context()->RSSetState (rss);
		m_CurrRSState = rss;
	}
}


void UpdateChannelBindingsD3D11 (const ChannelAssigns& channels)
{
	DX11_LOG_ENTER_FUNCTION("UpdateChannelBindingsD3D11");
	GfxDeviceD3D11& device = static_cast<GfxDeviceD3D11&>(GetRealGfxDevice());
	if (!device.IsShaderActive(kShaderVertex))
	{
		const int maxTexCoords = gGraphicsCaps.maxTexCoords; // fetch here once
		UInt64 textureSources = device.m_FFState.texUnitSources;
		for (int i = 0; i < maxTexCoords; ++i)
		{
			UInt32 source = (textureSources >> (i*4)) & 0xF;
			if (source > kTexSourceUV7)
				continue;
			ShaderChannel texCoordChannel = channels.GetSourceForTarget ((VertexComponent)(kVertexCompTexCoord0 + i));
			if (texCoordChannel == kShaderChannelTexCoord0)
				textureSources = textureSources & ~(0xFUL<<i*4) | (UInt64(kTexSourceUV0)<<i*4);
			else if (texCoordChannel == kShaderChannelTexCoord1)
				textureSources = textureSources & ~(0xFUL<<i*4) | (UInt64(kTexSourceUV1)<<i*4);
			else if (texCoordChannel != kShaderChannelNone) {
				AssertString( "Bad texcoord index" );
			}
		}
		device.m_FFState.texUnitSources = textureSources;
	}

	device.m_FFState.useUniformInsteadOfVertexColor = !(channels.GetTargetMap() & (1<<kVertexCompColor));
}


struct SetValuesFunctorD3D11
{
	SetValuesFunctorD3D11(GfxDevice& device, ConstantBuffersD3D11& cbs) : m_Device(device), m_CBs(cbs) { }
	GfxDevice& m_Device;
	ConstantBuffersD3D11& m_CBs;
	void SetVectorVal (ShaderType shaderType, ShaderParamType type, int index, const float* ptr, int cols, const GpuProgramParameters& params, int cbIndex)
	{
		const GpuProgramParameters::ConstantBuffer& cb = params.GetConstantBuffers()[cbIndex];
		int idx = m_CBs.FindAndBindCB (cb.m_Name.index, shaderType, cb.m_BindIndex, cb.m_Size);
		if (type != kShaderParamInt)
			m_CBs.SetCBConstant (idx, index, ptr, cols*4);
		else
		{
			int vali[4] = {ptr[0], ptr[1], ptr[2], ptr[3]};
			m_CBs.SetCBConstant (idx, index, vali, cols*4);
		}
	}
	void SetMatrixVal (ShaderType shaderType, int index, const Matrix4x4f* ptr, int rows, const GpuProgramParameters& params, int cbIndex)
	{
		DebugAssert(rows == 4);
		const GpuProgramParameters::ConstantBuffer& cb = params.GetConstantBuffers()[cbIndex];
		int idx = m_CBs.FindAndBindCB (cb.m_Name.index, shaderType, cb.m_BindIndex, cb.m_Size);
		m_CBs.SetCBConstant (idx, index, ptr, 64);
	}
	void SetTextureVal (ShaderType shaderType, int index, int samplerIndex, TextureDimension dim, TextureID texID)
	{
		m_Device.SetTexture (shaderType, index, samplerIndex, texID, dim, std::numeric_limits<float>::infinity());
	}
};


void GfxDeviceD3D11::BeforeDrawCall( bool immediateMode )
{
	DX11_LOG_ENTER_FUNCTION("GfxDeviceD3D11::BeforeDrawCall");
	ID3D11DeviceContext* ctx = GetD3D11Context();
	HRESULT hr;

	SetupDeferredSRGBWrite ();
	SetupDeferredDepthStencilState ();
	SetupDeferredRasterState ();

	m_TransformState.UpdateWorldViewMatrix (m_BuiltinParamValues);

	if (m_FogParams.mode != kFogDisabled)
	{
		float diff    = m_FogParams.mode == kFogLinear ? m_FogParams.end - m_FogParams.start : 0.0f;
		float invDiff = Abs(diff) > 0.0001f ? 1.0f/diff : 0.0f;
		Vector4f fogParams(m_FogParams.density * 1.2011224087f,
			m_FogParams.density * 1.4426950408f,
			m_FogParams.mode == kFogLinear ? -invDiff : 0.0f,
			m_FogParams.mode == kFogLinear ? m_FogParams.end * invDiff : 0.0f
		);
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFFogParams, fogParams);
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFFogColor, m_FogParams.color);
	}

	void* shader[kShaderTypeCount];
	for (int pt = 0; pt < kShaderTypeCount; ++pt)
	{
		shader[pt] = NULL;
		m_BuiltinParamIndices[pt] = &m_NullParamIndices;
	}

	if (m_ActiveGpuProgram[kShaderVertex] && m_ActiveGpuProgram[kShaderFragment])
	{
		// Programmable shaders
		const bool haveDomainShader = m_ActiveGpuProgram[kShaderDomain];
		bool resetToNoFog = false;
		for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
		{
			if (m_ActiveGpuProgram[pt])
			{
				DebugAssert (!m_ActiveGpuProgram[pt] || m_ActiveGpuProgram[pt]->GetImplType() == pt);
				m_BuiltinParamIndices[pt] = &m_ActiveGpuProgramParams[pt]->GetBuiltinParams();
				D3D11CommonShader* prog = static_cast<D3D11CommonShader*>(m_ActiveGpuProgram[pt]);
				shader[pt] = prog->GetShader(m_FogParams.mode, haveDomainShader, resetToNoFog);
				if (resetToNoFog)
					m_FogParams.mode = kFogDisabled;
			}
		}

		// Apply fog parameters if needed
		if (m_FogParams.mode > kFogDisabled)
		{
			const int cbIndex = m_CBs.FindAndBindCB (kSLPropFogCB.index, kShaderFragment, k11FogConstantBufferBind, k11FogSize*16);

			m_CBs.SetCBConstant (cbIndex, k11FogColor*16, m_FogParams.color.GetPtr(), 16);

			float params[4];
			params[0] = m_FogParams.density * 1.2011224087f ; // density / sqrt(ln(2))
			params[1] = m_FogParams.density * 1.4426950408f; // density / ln(2)
			if (m_FogParams.mode == kFogLinear)
			{
				float diff = m_FogParams.end - m_FogParams.start;
				float invDiff = Abs(diff) > 0.0001f ? 1.0f/diff : 0.0f;
				params[2] = -invDiff;
				params[3] = m_FogParams.end * invDiff;
			}
			else
			{
				params[2] = 0.0f;
				params[3] = 0.0f;
			}
			m_CBs.SetCBConstant (cbIndex, k11FogParams*16, params, 16);
		}
	}
	else
	{
		// Emulate fixed function
		m_FFState.fogMode = m_FogParams.mode;
		FixedFunctionProgramD3D11* program = GetFixedFunctionProgram11 (m_FFPrograms, m_FFState);

		shader[kShaderVertex] = program->GetVertexShader();
		shader[kShaderFragment] = program->GetPixelShader();

		program->ApplyFFGpuProgram (m_BuiltinParamValues, m_CBs);

		m_BuiltinParamIndices[kShaderVertex] = &program->GetVPMatrices();
	}

	// Set D3D shaders
	for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
	{
		if (m_ActiveShaders[pt] == shader[pt])
			continue;
		switch (pt) {
		case kShaderVertex: D3D11_CALL(ctx->VSSetShader ((ID3D11VertexShader*)shader[pt], NULL, 0)); break;
		case kShaderFragment: D3D11_CALL(ctx->PSSetShader ((ID3D11PixelShader*)shader[pt], NULL, 0)); break;
		case kShaderGeometry: D3D11_CALL(ctx->GSSetShader ((ID3D11GeometryShader*)shader[pt], NULL, 0)); break;
		case kShaderHull: D3D11_CALL(ctx->HSSetShader ((ID3D11HullShader*)shader[pt], NULL, 0)); break;
		case kShaderDomain: D3D11_CALL(ctx->DSSetShader ((ID3D11DomainShader*)shader[pt], NULL, 0)); break;
		}
		m_ActiveShaders[pt] = shader[pt];
	}

	// Set Unity built-in parameters
	bool anyGpuIndexValid;
	int gpuIndex[kShaderTypeCount];

#define SET_BUILTIN_MATRIX_BEGIN(idx) \
	anyGpuIndexValid = false; \
	for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt) { \
		int gi = m_BuiltinParamIndices[pt]->mat[idx].gpuIndex; \
		if (gi >= 0) anyGpuIndexValid = true; \
		gpuIndex[pt] = gi; \
	} \
	if (anyGpuIndexValid)

#define SET_BUILTIN_MATRIX_END(idx,mtx) \
	for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt) { \
		int gi = gpuIndex[pt]; \
		if (gi >= 0) m_CBs.SetBuiltinCBConstant (m_BuiltinParamIndices[pt]->mat[idx].cbID, gi, mtx.GetPtr(), sizeof(mtx)); \
	}


	// MVP matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatMVP)
	{
		Matrix4x4f mat;
		MultiplyMatrices4x4 (&m_BuiltinParamValues.GetMatrixParam(kShaderMatProj), &m_TransformState.worldViewMatrix, &mat);
		SET_BUILTIN_MATRIX_END(kShaderInstanceMatMVP,mat);
	}
	// MV matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatMV)
	{
		Matrix4x4f& mat = m_TransformState.worldViewMatrix;
		SET_BUILTIN_MATRIX_END(kShaderInstanceMatMV,mat);
	}
	// Transpose MV matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatTransMV)
	{
		Matrix4x4f mat;
		TransposeMatrix4x4 (&m_TransformState.worldViewMatrix, &mat);
		SET_BUILTIN_MATRIX_END(kShaderInstanceMatTransMV,mat);
	}
	// Inverse transpose of MV matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatInvTransMV)
	{
		Matrix4x4f mat;
		Matrix4x4f::Invert_Full (m_TransformState.worldViewMatrix, mat);
		if (true) //@TODO m_VertexData.normalization == kNormalizationScale)
		{
			// Inverse transpose of modelview should be scaled by uniform
			// normal scale (this will match state.matrix.invtrans.modelview
			// and gl_NormalMatrix in OpenGL)
			float scale = Magnitude (m_TransformState.worldMatrix.GetAxisX());
			mat.Get (0, 0) *= scale;
			mat.Get (1, 0) *= scale;
			mat.Get (2, 0) *= scale;
			mat.Get (0, 1) *= scale;
			mat.Get (1, 1) *= scale;
			mat.Get (2, 1) *= scale;
			mat.Get (0, 2) *= scale;
			mat.Get (1, 2) *= scale;
			mat.Get (2, 2) *= scale;
		}
		Matrix4x4f transposed;
		TransposeMatrix4x4 (&mat, &transposed);
		SET_BUILTIN_MATRIX_END(kShaderInstanceMatInvTransMV,transposed);
	}
	// M matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatM)
	{
		Matrix4x4f& mat = m_TransformState.worldMatrix;
		SET_BUILTIN_MATRIX_END(kShaderInstanceMatM,mat);
	}
	// Inverse M matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatInvM)
	{
		Matrix4x4f mat = m_TransformState.worldMatrix;
		if (true) //@TODO m_VertexData.normalization == kNormalizationScale)
		{
			// Kill scale in the world matrix before inverse
			float invScale = m_BuiltinParamValues.GetInstanceVectorParam(kShaderInstanceVecScale).w;
			mat.Get (0, 0) *= invScale;
			mat.Get (1, 0) *= invScale;
			mat.Get (2, 0) *= invScale;
			mat.Get (0, 1) *= invScale;
			mat.Get (1, 1) *= invScale;
			mat.Get (2, 1) *= invScale;
			mat.Get (0, 2) *= invScale;
			mat.Get (1, 2) *= invScale;
			mat.Get (2, 2) *= invScale;
		}
		Matrix4x4f inverseMat;
		Matrix4x4f::Invert_General3D (mat, inverseMat);
		SET_BUILTIN_MATRIX_END(kShaderInstanceMatInvM,inverseMat);
	}

	// Set instance vector parameters
	for (int i = 0; i < kShaderInstanceVecCount; ++i)
	{
		for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
		{
			int gi = m_BuiltinParamIndices[pt]->vec[i].gpuIndex;
			if (gi >= 0)
			{
				m_CBs.SetBuiltinCBConstant (m_BuiltinParamIndices[pt]->vec[i].cbID, gi, m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i).GetPtr(), m_BuiltinParamIndices[pt]->vec[i].dim*4);
			}
		}
	}

	// Texture matrices for vertex shader
	for( int i = 0; i < 8; ++i )
	{
		const BuiltinShaderParamIndices::MatrixParamData* matParam = &m_BuiltinParamIndices[kShaderVertex]->mat[kShaderInstanceMatTexture0 + i];
		if (matParam->gpuIndex >= 0)
		{
			const Matrix4x4f& mat = m_TextureUnits[i].matrix;
			m_CBs.SetBuiltinCBConstant (matParam->cbID, matParam->gpuIndex, mat.GetPtr(), sizeof(mat));
		}
	}

	// Material properties
	SetValuesFunctorD3D11 setValuesFunc(*this, m_CBs);
	ApplyMaterialPropertyBlockValues(m_MaterialProperties, m_ActiveGpuProgram, m_ActiveGpuProgramParams, setValuesFunc);

	///@TODO the rest

	m_CBs.UpdateBuffers ();
}

static const D3D11_BLEND kBlendModeD3D11[] = {
	D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_SRC_ALPHA_SAT, D3D11_BLEND_INV_SRC_ALPHA,
};
static const D3D11_BLEND kBlendModeAlphaD3D11[] = {
	D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_SRC_ALPHA_SAT, D3D11_BLEND_INV_SRC_ALPHA,
};
static const D3D11_BLEND_OP kBlendOpD3D11[] = {
	D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT, D3D11_BLEND_OP_MIN, D3D11_BLEND_OP_MAX,
	/* ADD for all the logic op modes, used for fallback.*/
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_ADD,

};

static const D3D11_LOGIC_OP kLogicOpD3D11[] = {
	/* Zeroes for the blend modes */
	D3D11_LOGIC_OP_CLEAR,
	D3D11_LOGIC_OP_CLEAR,
	D3D11_LOGIC_OP_CLEAR,
	D3D11_LOGIC_OP_CLEAR,
	D3D11_LOGIC_OP_CLEAR,
	/* Actual logic ops */
	D3D11_LOGIC_OP_CLEAR,
	D3D11_LOGIC_OP_SET,
	D3D11_LOGIC_OP_COPY,
	D3D11_LOGIC_OP_COPY_INVERTED,
	D3D11_LOGIC_OP_NOOP,
	D3D11_LOGIC_OP_INVERT,
	D3D11_LOGIC_OP_AND,
	D3D11_LOGIC_OP_NAND,
	D3D11_LOGIC_OP_OR,
	D3D11_LOGIC_OP_NOR,
	D3D11_LOGIC_OP_XOR,
	D3D11_LOGIC_OP_EQUIV,
	D3D11_LOGIC_OP_AND_REVERSE,
	D3D11_LOGIC_OP_AND_INVERTED,
	D3D11_LOGIC_OP_OR_REVERSE,
	D3D11_LOGIC_OP_OR_INVERTED
};



DeviceBlendState* GfxDeviceD3D11::CreateBlendState (const GfxBlendState& state)
{
	std::pair<CachedBlendStates::iterator, bool> result = m_CachedBlendStates.insert(std::make_pair(state, DeviceBlendStateD3D11()));
	if (!result.second)
		return &result.first->second;

	DeviceBlendStateD3D11& d3dstate = result.first->second;
	memcpy (&d3dstate.sourceState, &state, sizeof(d3dstate.sourceState));

	// DX11.1 logic ops, falls through to ADD blendop if not supported
	if(state.blendOp >= kBlendOpLogicalClear && state.blendOp <= kBlendOpLogicalOrInverted
		&& gGraphicsCaps.hasBlendLogicOps)
	{
		D3D11_BLEND_DESC1 desc;
		memset (&desc, 0, sizeof(desc));
		if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0)
			desc.AlphaToCoverageEnable = state.alphaToMask;

		desc.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC1& dst = desc.RenderTarget[0];

		dst.BlendEnable = false;
		dst.LogicOpEnable = true;

		dst.LogicOp = kLogicOpD3D11[state.blendOp];

		DWORD d3dmask = 0;
		const UInt8 mask = state.renderTargetWriteMask;
		if( mask & kColorWriteR ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_RED;
		if( mask & kColorWriteG ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
		if( mask & kColorWriteB ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
		if( mask & kColorWriteA ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
		dst.RenderTargetWriteMask = d3dmask;
		// GetD3D11_1Device cannot return null if we're running on DX11 and have gGraphicsCaps.hasBlendLogicOps, so no need to check.

		ID3D11BlendState1 *blendObj = NULL;
		HRESULT hr = GetD3D11_1Device()->CreateBlendState1 (&desc, &blendObj);
		d3dstate.deviceState = blendObj;
		AssertIf(FAILED(hr));
		SetDebugNameD3D11 (d3dstate.deviceState, Format("BlendState-%d-%d", dst.SrcBlend, dst.DestBlend));

	}
	else
	{
		D3D11_BLEND_DESC desc;
		memset (&desc, 0, sizeof(desc));
		if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0)
			desc.AlphaToCoverageEnable = state.alphaToMask;
		desc.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC& dst = desc.RenderTarget[0];
		dst.BlendEnable = state.srcBlend != kBlendOne || state.dstBlend != kBlendZero || state.srcBlendAlpha != kBlendOne || state.dstBlendAlpha != kBlendZero;
		dst.SrcBlend = kBlendModeD3D11[state.srcBlend];
		dst.DestBlend = kBlendModeD3D11[state.dstBlend];
		dst.BlendOp = kBlendOpD3D11[state.blendOp];
		dst.SrcBlendAlpha = kBlendModeAlphaD3D11[state.srcBlendAlpha];
		dst.DestBlendAlpha = kBlendModeAlphaD3D11[state.dstBlendAlpha];
		dst.BlendOpAlpha = kBlendOpD3D11[state.blendOpAlpha];

		DWORD d3dmask = 0;
		const UInt8 mask = state.renderTargetWriteMask;
		if( mask & kColorWriteR ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_RED;
		if( mask & kColorWriteG ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
		if( mask & kColorWriteB ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
		if( mask & kColorWriteA ) d3dmask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
		dst.RenderTargetWriteMask = d3dmask;
		HRESULT hr = GetD3D11Device()->CreateBlendState (&desc, &d3dstate.deviceState);
		AssertIf(FAILED(hr));
		SetDebugNameD3D11 (d3dstate.deviceState, Format("BlendState-%d-%d", dst.SrcBlend, dst.DestBlend));

	}


	return &result.first->second;
}


DeviceDepthState* GfxDeviceD3D11::CreateDepthState(const GfxDepthState& state)
{
	std::pair<CachedDepthStates::iterator, bool> result = m_CachedDepthStates.insert(std::make_pair(state, DeviceDepthState()));
	if (!result.second)
		return &result.first->second;

	DeviceDepthState& st = result.first->second;
	memcpy(&st.sourceState, &state, sizeof(st.sourceState));
	return &result.first->second;
}

DeviceStencilState* GfxDeviceD3D11::CreateStencilState(const GfxStencilState& state)
{
	std::pair<CachedStencilStates::iterator, bool> result = m_CachedStencilStates.insert(std::make_pair(state, DeviceStencilState()));
	if (!result.second)
		return &result.first->second;

	DeviceStencilState& st = result.first->second;
	memcpy(&st.sourceState, &state, sizeof(state));
	return &result.first->second;
}


DeviceRasterState* GfxDeviceD3D11::CreateRasterState(const GfxRasterState& state)
{
	std::pair<CachedRasterStates::iterator, bool> result = m_CachedRasterStates.insert(std::make_pair(state, DeviceRasterState()));
	if (!result.second)
		return &result.first->second;

	DeviceRasterState& st = result.first->second;
	memcpy(&st.sourceState, &state, sizeof(state));
	return &result.first->second;
}


void GfxDeviceD3D11::SetBlendState(const DeviceBlendState* state, float alphaRef)
{
	if (state != m_CurrBlendState)
	{
		m_CurrBlendState = state;
		DeviceBlendStateD3D11* devstate = (DeviceBlendStateD3D11*)state;
		GetD3D11Context()->OMSetBlendState (devstate->deviceState, NULL, 0xFFFFFFFF);
	}

	// alpha test
	m_FFState.alphaTest = state->sourceState.alphaTest;
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFAlphaTestRef, Vector4f(alphaRef, alphaRef, alphaRef, alphaRef));
}


void GfxDeviceD3D11::SetRasterState(const DeviceRasterState* state)
{
	if (m_CurrRasterState != state)
	{
		m_CurrRasterState = state;
		m_CurrRSState = NULL;
	}
}


void GfxDeviceD3D11::SetDepthState (const DeviceDepthState* state)
{
	if (m_CurrDepthState != state)
	{
		m_CurrDepthState = state;
		m_CurrDSState = NULL;
	}
}

void GfxDeviceD3D11::SetStencilState(const DeviceStencilState* state, int stencilRef)
{
	if (m_CurrStencilState != state)
	{
		m_CurrStencilState = state;
		m_CurrDSState = NULL;
	}
	m_StencilRef = stencilRef;
}

void GfxDeviceD3D11::SetupDeferredSRGBWrite()
{
	if (m_SRGBWrite == m_ActualSRGBWrite)
		return;

	// sRGB write is not just a render state on DX11; we need to actually rebind the render target views with
	// a different format. Looks like some drivers do not optimize useless RT changes away, causing
	// a lot of performance being wasted. So only apply sRGB write change when actually needed.
	m_ActualSRGBWrite = m_SRGBWrite;
	RebindActiveRenderTargets (&m_Textures);
}

void GfxDeviceD3D11::SetSRGBWrite (bool enable)
{
	m_SRGBWrite = enable;
}

bool GfxDeviceD3D11::GetSRGBWrite ()
{
	return m_SRGBWrite;
}

void GfxDeviceD3D11::DiscardContents (RenderSurfaceHandle& rs)
{
#	if UNITY_WINRT // WSA/WP8 guaranteed to have DX11.1 runtime, needed for DiscardResource

	if(!rs.IsValid())
		return;

	RenderSurfaceD3D11 *surf = reinterpret_cast<RenderSurfaceD3D11*>( rs.object );
	if (surf->m_Texture)
	{
		ID3D11DeviceContext1 * ctx = (ID3D11DeviceContext1 *)GetD3D11Context();
		DX11_CHK(ctx->DiscardResource(surf->m_Texture));
	}

#	endif
}

GfxDevice* CreateD3D11GfxDevice()
{
	if( !InitializeD3D11() )
		return NULL;

	gGraphicsCaps.InitD3D11();

	GfxDeviceD3D11* device = UNITY_NEW_AS_ROOT(GfxDeviceD3D11(), kMemGfxDevice, "D3D11GfxDevice", "");
	return device;
}


GfxDeviceD3D11::GfxDeviceD3D11()
{
	m_DynamicVBO = NULL;
	InvalidateState();
	ResetFrameStats();

	m_Renderer = kGfxRendererD3D11;
	m_UsesOpenGLTextureCoords = false;
	m_UsesHalfTexelOffset = false;
	m_IsThreadable = true;

	m_MaxBufferedFrames = -1; // no limiting

	m_Viewport[0] = m_Viewport[1] = m_Viewport[2] = m_Viewport[3] = 0;
	m_ScissorRect[0] = m_ScissorRect[1] = m_ScissorRect[2] = m_ScissorRect[3] = 0;
	m_CurrTargetWidth = 0;
	m_CurrTargetHeight = 0;
	m_CurrWindowWidth = 0;
	m_CurrWindowHeight = 0;

	m_InvertProjMatrix = false;
	m_AppBackfaceMode = false;
	m_UserBackfaceMode = false;
	m_Wireframe = false;
	m_Scissor = false;
	m_SRGBWrite = false;
	m_ActualSRGBWrite = false;

	m_FramebufferDepthFormat = kDepthFormat24;

	// constant buffer for fog params
	m_CBs.SetCBInfo (kSLPropFogCB.index, k11FogSize*16);

	extern RenderSurfaceBase* DummyColorBackBuferD3D11();
	SetBackBufferColorSurface(DummyColorBackBuferD3D11());

	extern RenderSurfaceBase* DummyDepthBackBuferD3D11();
	SetBackBufferDepthSurface(DummyDepthBackBuferD3D11());
}

GfxDeviceD3D11::~GfxDeviceD3D11()
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER
	PluginsSetGraphicsDevice (GetD3D11Device(), kGfxRendererD3D11, kGfxDeviceEventShutdown);
#endif

	StreamOutSkinningInfo::CleanUp();

#if ENABLE_PROFILER
	g_TimerQueriesD3D11.ReleaseAllQueries();
#endif

	D3D11VBO::CleanupSharedBuffers();
	if( m_DynamicVBO )
		delete m_DynamicVBO;
	for (FFProgramCacheD3D11::iterator it = m_FFPrograms.begin(); it != m_FFPrograms.end(); ++it)
		delete it->second;
	for (CachedBlendStates::iterator it = m_CachedBlendStates.begin(); it != m_CachedBlendStates.end(); ++it)
		it->second.deviceState->Release();
	for (CachedDepthStencilStates::iterator it = m_CachedDepthStencilStates.begin(); it != m_CachedDepthStencilStates.end(); ++it)
		it->second->Release();
	for (CachedFinalRasterStates::iterator it = m_CachedFinalRasterStates.begin(); it != m_CachedFinalRasterStates.end(); ++it)
		it->second->Release();
	m_Imm.Cleanup();
	m_VertexDecls.Clear();
	m_CBs.Clear();
	m_Textures.ClearTextureResources();
	m_Resolves.Clear();
	DestroyD3D11Device();
	CleanupD3D11();
}


void GfxDeviceD3D11::InvalidateState()
{
	g_ActiveInputLayoutD3D11 = NULL;
	g_CurrentVSInputD3D11 = NULL;
	g_ActiveTopologyD3D11 = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	m_TransformState.Invalidate(m_BuiltinParamValues);

	m_FogParams.Invalidate();

	//m_State.Invalidate(*this);
	m_Imm.Invalidate();
	//m_VSConstantCache.Invalidate();
	//m_PSConstantCache.Invalidate();

	memset (&m_FFState, 0, sizeof(m_FFState));
	m_FFState.useUniformInsteadOfVertexColor = true;

	m_CurrBlendState = NULL;
	m_CurrRasterState = NULL;
	m_CurrDepthState = NULL;
	m_CurrStencilState = NULL;
	m_CurrRSState = NULL;
	m_CurrDSState = NULL;
	m_CurrStencilRef = -1;

	for (int pt = 0; pt < kShaderTypeCount; ++pt)
	{
		m_ActiveGpuProgram[pt] = NULL;
		m_ActiveGpuProgramParams[pt] = NULL;
		m_ActiveShaders[pt] = NULL;
		for (int i = 0; i < kMaxSupportedTextureUnits; ++i)
		{
			m_ActiveTextures[pt][i].m_ID = -1;
			m_ActiveSamplers[pt][i].m_ID = -1;
		}
	}

	m_Textures.InvalidateSamplers();
	m_CBs.InvalidateState();

	ID3D11DeviceContext* ctx = GetD3D11Context(true);
	if (ctx)
	{
		D3D11_CALL(ctx->VSSetShader (NULL, NULL, 0));
		D3D11_CALL(ctx->PSSetShader (NULL, NULL, 0));
		D3D11_CALL(ctx->GSSetShader (NULL, NULL, 0));
		D3D11_CALL(ctx->HSSetShader (NULL, NULL, 0));
		D3D11_CALL(ctx->DSSetShader (NULL, NULL, 0));
	}
}


void GfxDeviceD3D11::Clear (UInt32 clearFlags, const float color[4], float depth, int stencil)
{
	DX11_LOG_ENTER_FUNCTION("Clear(%d, (%.2f, %.2f, %.2f, %.2f), %.2f, %d)", clearFlags, color[0], color[1], color[2], color[3], depth, stencil);
	SetupDeferredSRGBWrite ();
	ID3D11DeviceContext* ctx = GetD3D11Context();

	if ((clearFlags & kGfxClearColor) && g_D3D11CurrRT)
		DX11_CHK(ctx->ClearRenderTargetView (g_D3D11CurrRT, color));
	if ((clearFlags & kGfxClearDepthStencil) && g_D3D11CurrDS)
	{
		UINT flags = 0;
		if (clearFlags & kGfxClearDepth)
			flags |= D3D11_CLEAR_DEPTH;
		if (clearFlags & kGfxClearStencil)
			flags |= D3D11_CLEAR_STENCIL;
		DX11_CHK(ctx->ClearDepthStencilView (g_D3D11CurrDS, flags, depth, stencil));
	}
}

void GfxDeviceD3D11::SetUserBackfaceMode( bool enable )
{
	if (m_UserBackfaceMode != enable)
	{
		m_UserBackfaceMode = enable;
		m_CurrRSState = NULL;
	}
}

void GfxDeviceD3D11::SetWireframe (bool wire)
{
	if (m_Wireframe != wire)
	{
		m_Wireframe = wire;
		m_CurrRSState = NULL;
	}
}

bool GfxDeviceD3D11::GetWireframe() const
{
	return m_Wireframe;
}



void GfxDeviceD3D11::SetInvertProjectionMatrix( bool enable )
{
	if (m_InvertProjMatrix == enable)
		return;

	m_InvertProjMatrix = enable;

	// When setting up "invert" flag, invert the matrix as well.
	Matrix4x4f& m = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj);
	m.Get(1,1) = -m.Get(1,1);
	m.Get(1,3) = -m.Get(1,3);
	m_TransformState.dirtyFlags |= TransformState::kProjDirty;

	m_CurrRSState = NULL;
}

bool GfxDeviceD3D11::GetInvertProjectionMatrix() const
{
	return m_InvertProjMatrix;
}

void GfxDeviceD3D11::SetWorldMatrix( const float matrix[16] )
{
	CopyMatrix( matrix, m_TransformState.worldMatrix.GetPtr() );
	m_TransformState.dirtyFlags |= TransformState::kWorldDirty;
}

void GfxDeviceD3D11::SetViewMatrix( const float matrix[16] )
{
	m_TransformState.SetViewMatrix (matrix, m_BuiltinParamValues);
}

void GfxDeviceD3D11::SetProjectionMatrix (const Matrix4x4f& matrix)
{
	Matrix4x4f& m = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj);
	CopyMatrix (matrix.GetPtr(), m.GetPtr());
	CopyMatrix (matrix.GetPtr(), m_TransformState.projectionMatrixOriginal.GetPtr());
	CalculateDeviceProjectionMatrix (m, m_UsesOpenGLTextureCoords, m_InvertProjMatrix);
	m_TransformState.dirtyFlags |= TransformState::kProjDirty;
}

void GfxDeviceD3D11::GetMatrix( float outMatrix[16] ) const
{
	m_TransformState.UpdateWorldViewMatrix (m_BuiltinParamValues);
	CopyMatrix (m_TransformState.worldViewMatrix.GetPtr(), outMatrix);
}

const float* GfxDeviceD3D11::GetWorldMatrix() const
{
	return m_TransformState.worldMatrix.GetPtr();
}

const float* GfxDeviceD3D11::GetViewMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatView).GetPtr();
}

const float* GfxDeviceD3D11::GetProjectionMatrix() const
{
	return m_TransformState.projectionMatrixOriginal.GetPtr();
}

const float* GfxDeviceD3D11::GetDeviceProjectionMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatProj).GetPtr();
}


void GfxDeviceD3D11::SetNormalizationBackface( NormalizationMode mode, bool backface )
{
	if (m_AppBackfaceMode != backface)
	{
		m_AppBackfaceMode = backface;
		m_CurrRSState = NULL;
	}
}

void GfxDeviceD3D11::SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial )
{
	DX11_LOG_ENTER_FUNCTION("SetFFLighting(%s, %s, %d)", GetDX11BoolString(on), GetDX11BoolString(separateSpecular), colorMaterial);
	DebugAssert(colorMaterial!=kColorMatUnknown);
	m_FFState.lightingEnabled = on;
	m_FFState.specularEnabled = on && separateSpecular;
	m_FFState.colorMaterial = colorMaterial;
}

void GfxDeviceD3D11::SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess )
{
	DX11_LOG_ENTER_FUNCTION("SetMaterial((%.2f, %.2f, %.2f, %.2f), (%.2f, %.2f, %.2f, %.2f), (%.2f, %.2f, %.2f, %.2f), (%.2f, %.2f, %.2f, %.2f), %.2f)",
		ambient[0], ambient[1], ambient[2], ambient[3],
		diffuse[0], diffuse[1], diffuse[2], diffuse[3],
		specular[0], specular[1], specular[2], specular[3],
		emissive[0], emissive[1], emissive[2], emissive[3],
		shininess);

	float glshine = clamp01 (shininess) * 128.0f;

	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatAmbient, Vector4f(ambient[0], ambient[1], ambient[2], 1.0F));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatDiffuse, Vector4f(diffuse));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatSpecular, Vector4f(specular[0], specular[1], specular[2], glshine));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatEmission, Vector4f(emissive[0], emissive[1], emissive[2], 1.0F));
}


void GfxDeviceD3D11::SetColor( const float color[4] )
{
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFColor, Vector4f(color));
}



void GfxDeviceD3D11::SetViewport( int x, int y, int width, int height )
{
	DX11_LOG_ENTER_FUNCTION("SetViewport(%d, %d, %d, %d)", x, y, width, height);
	m_Viewport[0] = x;
	m_Viewport[1] = y;
	m_Viewport[2] = width;
	m_Viewport[3] = height;

	D3D11_VIEWPORT view;
	view.TopLeftX = x;
	view.TopLeftY = y;
	view.Width = width;
	view.Height = height;
	view.MinDepth = 0.0f;
	view.MaxDepth = 1.0f;
	ID3D11DeviceContext* ctx = GetD3D11Context();
	//@TODO
	//if( !dev ) // happens on startup, when deleting all render textures
	//	return;
	ctx->RSSetViewports (1, &view);
}

void GfxDeviceD3D11::GetViewport( int* port ) const
{
	port[0] = m_Viewport[0];
	port[1] = m_Viewport[1];
	port[2] = m_Viewport[2];
	port[3] = m_Viewport[3];
}


void GfxDeviceD3D11::SetScissorRect (int x, int y, int width, int height)
{
	DX11_LOG_ENTER_FUNCTION("SetScissorRect(%d, %d, %d, %d)", x, y, width, height);
	if (!m_Scissor)
	{
		m_Scissor = true;
		m_CurrRSState = NULL;
	}

	m_ScissorRect[0] = x;
	m_ScissorRect[1] = y;
	m_ScissorRect[2] = width;
	m_ScissorRect[3] = height;

	D3D11_RECT rc;
	rc.left = x;
	rc.top = y;
	rc.right = x + width;
	rc.bottom = y + height;
	GetD3D11Context()->RSSetScissorRects (1, &rc);
}

void GfxDeviceD3D11::DisableScissor()
{
	if (m_Scissor)
	{
		m_Scissor = false;
		m_CurrRSState = NULL;
	}
}

bool GfxDeviceD3D11::IsScissorEnabled() const
{
	return m_Scissor;
}

void GfxDeviceD3D11::GetScissorRect (int scissor[4]) const
{
	scissor[0] = m_ScissorRect[0];
	scissor[1] = m_ScissorRect[1];
	scissor[2] = m_ScissorRect[2];
	scissor[3] = m_ScissorRect[3];
}


struct TextureCombiners11
{
	const ShaderLab::TextureBinding* texEnvs;
	int count;
};

bool GfxDeviceD3D11::IsCombineModeSupported( unsigned int combiner )
{
	return true;
}

TextureCombinersHandle GfxDeviceD3D11::CreateTextureCombiners (int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular)
{
	DX11_LOG_ENTER_FUNCTION("CreateTextureCombiners()");
	if (count > gGraphicsCaps.maxTexUnits)
		return TextureCombinersHandle(NULL);

	TextureCombiners11* combiners = new TextureCombiners11();
	combiners->texEnvs = texEnvs;
	combiners->count = count;
	return TextureCombinersHandle(combiners);
}

void GfxDeviceD3D11::DeleteTextureCombiners (TextureCombinersHandle& textureCombiners)
{
	DX11_LOG_ENTER_FUNCTION("DeleteTextureCombiners()");
	TextureCombiners11* combiners = OBJECT_FROM_HANDLE(textureCombiners,TextureCombiners11);
	delete combiners;
	textureCombiners.Reset();
}

void GfxDeviceD3D11::SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors )
{
	DX11_LOG_ENTER_FUNCTION("SetTextureCombinersThreadable()");
	TextureCombiners11* combiners = OBJECT_FROM_HANDLE(textureCombiners,TextureCombiners11);
	Assert (combiners);

	const int count = std::min(combiners->count, gGraphicsCaps.maxTexUnits);
	m_FFState.texUnitCount = count;
	for (int i = 0; i < count; ++i)
	{
		const ShaderLab::TextureBinding& binding = combiners->texEnvs[i];
		ApplyTexEnvData (i, i, texEnvData[i]);
		m_BuiltinParamValues.SetVectorParam ((BuiltinShaderVectorParam)(kShaderVecFFTextureEnvColor0 + i), texColors[i]);
		m_FFState.texUnitColorCombiner[i] = binding.m_CombColor;
		m_FFState.texUnitAlphaCombiner[i] = binding.m_CombAlpha;
	}

	// unused textures
	UInt32 mask = (1<<count)-1;
	m_FFState.texUnitCube &= mask;
	m_FFState.texUnit3D &= mask;
	m_FFState.texUnitProjected &= mask;
}

void GfxDeviceD3D11::SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props )
{
	DX11_LOG_ENTER_FUNCTION("SetTextureCombiners()");
	TextureCombiners11* combiners = OBJECT_FROM_HANDLE(textureCombiners,TextureCombiners11);
	Assert(combiners);

	const int count = std::min(combiners->count, gGraphicsCaps.maxTexUnits);

	// Fill in arrays
	TexEnvData* texEnvData;
	ALLOC_TEMP (texEnvData, TexEnvData, count);
	for (int i = 0; i < count; ++i)
	{
		const ShaderLab::TextureBinding& binding = combiners->texEnvs[i];
		ShaderLab::TexEnv *te = ShaderLab::GetTexEnvForBinding(binding, props);
		Assert(te != NULL);
		te->PrepareData (binding.m_TextureName.index, binding.m_MatrixName, props, &texEnvData[i]);
	}

	Vector4f* texColors;
	ALLOC_TEMP (texColors, Vector4f, count);
	for (int i = 0; i < count; ++i)
	{
		const ShaderLab::TextureBinding& binding = combiners->texEnvs[i];
		texColors[i] = binding.GetTexColor().Get (props);
	}
	GfxDeviceD3D11::SetTextureCombinersThreadable (textureCombiners, texEnvData, texColors);
}



void UnbindTextureD3D11 (TextureID texture)
{
	DX11_LOG_ENTER_FUNCTION("UnbindTextureD3D11(%d)", texture.m_ID);
	GfxDeviceD3D11& device = static_cast<GfxDeviceD3D11&>(GetRealGfxDevice());
	ID3D11DeviceContext* ctx = GetD3D11Context();

	for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
	{
		for (int i = 0; i < kMaxSupportedTextureUnits; ++i)
		{
			if (device.m_ActiveTextures[pt][i]==texture)
			{
				ID3D11ShaderResourceView* srv = NULL;
				switch (pt) {
				case kShaderVertex:   ctx->VSSetShaderResources (i, 1, &srv); break;
				case kShaderFragment: ctx->PSSetShaderResources (i, 1, &srv); break;
				case kShaderGeometry: ctx->GSSetShaderResources (i, 1, &srv); break;
				case kShaderHull:     ctx->HSSetShaderResources (i, 1, &srv); break;
				case kShaderDomain:   ctx->DSSetShaderResources (i, 1, &srv); break;
				default: AssertString("unknown shader type");
				}
				device.m_ActiveTextures[pt][i].m_ID = -1;
			}
			if (device.m_ActiveSamplers[pt][i]==texture)
			{
				device.m_ActiveSamplers[pt][i].m_ID = -1;
			}
		}
	}
}



void GfxDeviceD3D11::SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias)
{
	DebugAssertIf (dim < kTexDim2D || dim > kTexDimCUBE);
	DebugAssertIf (unit < 0 || unit >= kMaxSupportedTextureUnits);

	// WP8 seems to have a driver bug (?) with occasionally losing texture state; can't do redundant bind early out here.
	// Repros on Shadowgun flyby on Nokia Not For Sale.
	#if !UNITY_WP8
	if (m_ActiveTextures[shaderType][unit] == texture && (samplerUnit >= 0 && m_ActiveSamplers[shaderType][samplerUnit] == texture))
		return;
	#endif

	if (m_Textures.SetTexture (shaderType, unit, samplerUnit, texture, bias))
	{
		m_Stats.AddUsedTexture(texture);
		m_ActiveTextures[shaderType][unit] = texture;
		if (samplerUnit >= 0)
			m_ActiveSamplers[shaderType][samplerUnit] = texture;
	}

	if (shaderType == kShaderFragment && unit < kMaxSupportedTextureCoords)
	{
		if (m_FFState.texUnitCount <= unit)
			m_FFState.texUnitCount = unit+1;

		UInt32 mask = 1<<unit;
		if (dim==kTexDimCUBE)
			m_FFState.texUnitCube |= mask;
		else
			m_FFState.texUnitCube &= ~mask;
		if (dim==kTexDim3D)
			m_FFState.texUnit3D |= mask;
		else
			m_FFState.texUnit3D &= ~mask;
	}
}


void GfxDeviceD3D11::SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16])
{
	Assert (unit >= 0 && unit < kMaxSupportedTextureCoords);

	TextureSourceD3D11 texSource = texGen == kTexGenDisabled ? kTexSourceUV0 : static_cast<TextureSourceD3D11>(texGen + kTexSourceUV7);
	m_FFState.texUnitSources = m_FFState.texUnitSources & ~(15<<(unit*4)) | (UInt64(texSource)<<(unit*4));

	if (identity)
		m_TextureUnits[unit].matrix.SetIdentity();
	else
		CopyMatrix (matrix, m_TextureUnits[unit].matrix.GetPtr());

	// Detect if we have a projective texture matrix
	m_FFState.texUnitProjected &= ~(1<<unit);
	if (!identity && dim==kTexDim2D)
	{
		if (matrix[3] != 0.0f || matrix[7] != 0.0f || matrix[11] != 0.0f || matrix[15] != 1.0f)
			m_FFState.texUnitProjected |= (1<<unit);
	}
}

void GfxDeviceD3D11::SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	UnbindTextureD3D11 (texture);
	m_Textures.SetTextureParams (texture, texDim, filter, wrap, anisoLevel, hasMipMap, colorSpace);
}



void GfxDeviceD3D11::SetShadersThreadable (GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount])
{
	for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
	{
		m_ActiveGpuProgram[pt] = programs[pt];
		m_ActiveGpuProgramParams[pt] = params[pt];
	}

	// Apply programmable shader parameters
	if (m_ActiveGpuProgram[kShaderVertex] && m_ActiveGpuProgram[kShaderFragment])
	{
		for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
		{
			if (m_ActiveGpuProgram[pt])
			{
				DebugAssert (!m_ActiveGpuProgram[pt] || m_ActiveGpuProgram[pt]->GetImplType() == pt);
				D3D11CommonShader* prog = static_cast<D3D11CommonShader*>(m_ActiveGpuProgram[pt]);
				m_ActiveGpuProgram[pt]->ApplyGpuProgram (*params[pt], paramsBuffer[pt]);
			}
		}
	}
}


bool GfxDeviceD3D11::IsShaderActive( ShaderType type ) const
{
	return (m_ActiveGpuProgram[type] != 0);
}

void GfxDeviceD3D11::DestroySubProgram( ShaderLab::SubProgram* subprogram )
{
	delete subprogram;
}

void GfxDeviceD3D11::SetConstantBufferInfo (int id, int size)
{
	m_CBs.SetCBInfo (id, size);
}

void GfxDeviceD3D11::DisableLights( int startLight )
{
	startLight = std::min (startLight, gGraphicsCaps.maxLights);
	m_FFState.lightCount = startLight;
	
	const Vector4f black(0.0F, 0.0F, 0.0F, 0.0F);
	const Vector4f zpos(0.0F, 0.0F, 1.0F, 0.0F);
	for (int i = startLight; i < gGraphicsCaps.maxLights; ++i)
	{
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Position + i), zpos);
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + i), black);
	}
}

void GfxDeviceD3D11::SetLight( int light, const GfxVertexLight& data)
{
	if (light >= gGraphicsCaps.maxLights)
		return;
	SetupVertexLightParams (light, data);
}

void GfxDeviceD3D11::SetAmbient( const float ambient[4] )
{
	m_BuiltinParamValues.SetVectorParam(kShaderVecLightModelAmbient, Vector4f(ambient));
}


void GfxDeviceD3D11::EnableFog (const GfxFogParams& fog)
{
	DebugAssert (fog.mode > kFogDisabled);
	m_FogParams = fog;

	//@TODO: fog DXBC patching not implemented for 9.x level; and something still wrong with FF shaders in 9.x level as well
	// (e.g. crashes WARP). Just disable fog for now.
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0)
		m_FogParams.mode = kFogDisabled;
}

void GfxDeviceD3D11::DisableFog()
{
	m_FogParams.mode = kFogDisabled;
	m_FogParams.density = 0.0f;
}


VBO* GfxDeviceD3D11::CreateVBO()
{
	VBO* vbo = new D3D11VBO();
	OnCreateVBO(vbo);
	return vbo;
}

void GfxDeviceD3D11::DeleteVBO( VBO* vbo )
{
	OnDeleteVBO(vbo);
	delete vbo;
}

DynamicVBO&	GfxDeviceD3D11::GetDynamicVBO()
{
	if( !m_DynamicVBO ) {
		m_DynamicVBO = new DynamicD3D11VBO( 1024 * 1024, 65536 ); // initial 1 MiB VB, 64 KiB IB
	}
	return *m_DynamicVBO;
}

/*
void GfxDeviceD3D11::ResetDynamicResources()
{
	delete m_DynamicVBO;
	m_DynamicVBO = NULL;

	CleanupEventQueries ();

	for( ListIterator<D3D11VBO*> i = m_DynamicVBOs.begin(); i != m_DynamicVBOs.end(); ++i )
	{
		D3D11VBO* vbo = *i;
		vbo->ResetDynamicVB();
	}
}
*/

GfxDeviceD3D11& GetD3D11GfxDevice()
{
	GfxDevice& device = GetRealGfxDevice();
	DebugAssert(device.GetRenderer() == kGfxRendererD3D11);
	return static_cast<GfxDeviceD3D11&>(device);
}


ID3D11InputLayout* GetD3D11VertexDeclaration (const ChannelInfoArray& channels)
{
	GfxDevice& device = GetRealGfxDevice();
	DebugAssert(device.GetRenderer() == kGfxRendererD3D11);
	GfxDeviceD3D11* deviceD3D = static_cast<GfxDeviceD3D11*>( &device );
	return deviceD3D->GetVertexDecls().GetVertexDecl (channels, g_CurrentVSInputD3D11);
}

const InputSignatureD3D11* GetD3D11InputSignature (void* code, unsigned length)
{
	GfxDevice& device = GetRealGfxDevice();
	DebugAssert(device.GetRenderer() == kGfxRendererD3D11);
	GfxDeviceD3D11* deviceD3D = static_cast<GfxDeviceD3D11*>( &device );
	return deviceD3D->GetVertexDecls().GetShaderInputSignature (code, length);
}

ConstantBuffersD3D11& GetD3D11ConstantBuffers (GfxDevice& device)
{
	Assert (device.GetRenderer() == kGfxRendererD3D11);
	GfxDeviceD3D11& deviceD3D = static_cast<GfxDeviceD3D11&>(device);
	return deviceD3D.GetConstantBuffers();
}

TexturesD3D11& GetD3D11Textures (GfxDevice& device)
{
	Assert (device.GetRenderer() == kGfxRendererD3D11);
	GfxDeviceD3D11& deviceD3D = static_cast<GfxDeviceD3D11&>(device);
	return deviceD3D.GetTextures();
}


// ---------- render textures


RenderSurfaceHandle CreateRenderColorSurfaceD3D11( TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, TexturesD3D11& textures);
RenderSurfaceHandle CreateRenderDepthSurfaceD3D11( TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags, TexturesD3D11& textures);
void DestroyRenderSurfaceD3D11 (RenderSurfaceHandle& rsHandle, TexturesD3D11& textures);
bool SetRenderTargetD3D11 (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, int* outTargetWidth, int* outTargetHeight, TexturesD3D11* textures);
RenderSurfaceHandle GetActiveRenderColorSurfaceD3D11(int index);
RenderSurfaceHandle GetActiveRenderDepthSurfaceD3D11();
RenderSurfaceHandle GetActiveRenderColorSurfaceBBD3D11();

RenderSurfaceHandle GfxDeviceD3D11::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	DX11_LOG_ENTER_FUNCTION("CreateRenderColorSurface(%d, %d, %d, %d, %d)", textureID.m_ID, width, height, format, createFlags);
	return CreateRenderColorSurfaceD3D11 (textureID, width, height, samples, depth, dim, createFlags, format, m_Textures);
}
RenderSurfaceHandle GfxDeviceD3D11::CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	DX11_LOG_ENTER_FUNCTION("CreateRenderDepthSurface(%d, %d, %d, %d, %d, %d)", textureID.m_ID, width, height, dim, depthFormat, createFlags);
	return CreateRenderDepthSurfaceD3D11 (textureID, width, height, samples, dim, depthFormat, createFlags, m_Textures);
}
void GfxDeviceD3D11::DestroyRenderSurface (RenderSurfaceHandle& rs)
{
	DX11_LOG_ENTER_FUNCTION("DestroyRenderSurface()");
	DestroyRenderSurfaceD3D11 (rs, m_Textures);
}
void GfxDeviceD3D11::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face)
{
	DX11_LOG_ENTER_FUNCTION("SetRenderTargets(%i, c0=%p, d=%p, mip=%i, f=%i)", count, colorHandles[0].object, depthHandle.object, mipLevel, face);
	SetupDeferredSRGBWrite ();
	m_CurrTargetWidth = m_CurrWindowWidth;
	m_CurrTargetHeight = m_CurrWindowHeight;
	if (SetRenderTargetD3D11 (count, colorHandles, depthHandle, mipLevel, face, &m_CurrTargetWidth, &m_CurrTargetHeight, &m_Textures))
	{
		// changing render target might mean different color clear flags; so reset current state
		m_CurrBlendState = NULL;
	}
}

void GfxDeviceD3D11::ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle)
{
	DX11_LOG_ENTER_FUNCTION("ResolveDepthIntoTexture(%p, %p)", colorHandle.object, depthHandle.object);
	RenderSurfaceD3D11* depthSurf = reinterpret_cast<RenderSurfaceD3D11*>(depthHandle.object);
	TexturesD3D11::D3D11Texture* destTexture = m_Textures.GetTexture (depthSurf->textureID);
	DebugAssert (destTexture);
	if (!destTexture)
		return;
	DebugAssert (g_D3D11CurrDepthRT);
	if (!g_D3D11CurrDepthRT || !g_D3D11CurrDepthRT->m_Texture)
		return;

	GetD3D11Context()->CopyResource (destTexture->m_Texture, g_D3D11CurrDepthRT->m_Texture);
}


void GfxDeviceD3D11::ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle)
{
	Assert (srcHandle.IsValid());
	Assert (dstHandle.IsValid());
	RenderColorSurfaceD3D11* src = reinterpret_cast<RenderColorSurfaceD3D11*>(srcHandle.object);
	RenderColorSurfaceD3D11* dst = reinterpret_cast<RenderColorSurfaceD3D11*>(dstHandle.object);
	if (!src->colorSurface || !dst->colorSurface)
	{
		WarningString("RenderTexture: Resolving non-color surfaces.");
		return;
	}
	if (src->dim != dst->dim)
	{
		WarningString("RenderTexture: Resolving surfaces of different types.");
		return;
	}
	if (src->format != dst->format)
	{
		WarningString("RenderTexture: Resolving surfaces of different formats.");
		return;
	}
	if (src->width != dst->width || src->height != dst->height)
	{
		WarningString("RenderTexture: Resolving surfaces of different sizes.");
		return;
	}

	ID3D11DeviceContext* ctx = GetD3D11Context();
	if (src->samples <= 1 && dst->samples <= 1)
	{
		ctx->CopyResource (dst->m_Texture, src->m_Texture);
	}
	else
	{
		extern DXGI_FORMAT kD3D11RenderTextureFormatsNorm[kRTFormatCount];
		ctx->ResolveSubresource (dst->m_Texture, 0, src->m_Texture, 0, kD3D11RenderTextureFormatsNorm[dst->format]);
		if ((dst->flags & kSurfaceCreateMipmap) &&
			(dst->flags & kSurfaceCreateAutoGenMips) &&
			dst->m_SRViewForMips)
		{
			ctx->GenerateMips (dst->m_SRViewForMips);
	}
	}
}


RenderSurfaceHandle GfxDeviceD3D11::GetActiveRenderColorSurface(int index)
{
	DX11_LOG_ENTER_FUNCTION("GetActiveRenderColorSurface(%d)", index);
	return GetActiveRenderColorSurfaceD3D11(index);
}
RenderSurfaceHandle GfxDeviceD3D11::GetActiveRenderDepthSurface()
{
	DX11_LOG_ENTER_FUNCTION("GetActiveRenderDepthSurface");
	return GetActiveRenderDepthSurfaceD3D11();
}
void GfxDeviceD3D11::SetSurfaceFlags (RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags)
{
}


// ---------- uploading textures

void GfxDeviceD3D11::UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	DX11_LOG_ENTER_FUNCTION("UploadTexture2D(%d, %d, <srcData>, %d, %d, %d, %d, %d, %d, %d)",
		texture.m_ID, dimension, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode);
	UnbindTextureD3D11 (texture);
	m_Textures.UploadTexture2D (texture, dimension, srcData, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode, colorSpace);
}
void GfxDeviceD3D11::UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	DX11_LOG_ENTER_FUNCTION("UploadTextureSubData2D(%d, <srcData>, ...)", texture.m_ID)
	m_Textures.UploadTextureSubData2D (texture, srcData, mipLevel, x, y, width, height, format, colorSpace);
}
void GfxDeviceD3D11::UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	DX11_LOG_ENTER_FUNCTION("UploadTextureCube(%d, <srcData>, ...)", texture.m_ID)
	UnbindTextureD3D11 (texture);
	m_Textures.UploadTextureCube (texture, srcData, faceDataSize, size, format, mipCount, uploadFlags, colorSpace);
}
void GfxDeviceD3D11::UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	DX11_LOG_ENTER_FUNCTION("UploadTexture3D(%d, <srcData>, ...)", texture.m_ID)
	UnbindTextureD3D11 (texture);
	m_Textures.UploadTexture3D (texture, srcData, width, height, depth, format, mipCount, uploadFlags);
}
void GfxDeviceD3D11::DeleteTexture( TextureID texture )
{
	DX11_LOG_ENTER_FUNCTION("DeleteTexture(%d)", texture.m_ID)
	UnbindTextureD3D11 (texture);
	m_Textures.DeleteTexture (texture);
}


// ---------- context

GfxDevice::PresentMode GfxDeviceD3D11::GetPresentMode()
{
	return kPresentBeforeUpdate;
}

void GfxDeviceD3D11::BeginFrame()
{
	DX11_LOG_OUTPUT("*****************************************");
	DX11_LOG_OUTPUT("*****************************************");
	DX11_LOG_OUTPUT("*****************************************");
	DX11_LOG_ENTER_FUNCTION("BeginFrame()");
	DX11_MARK_FRAME_BEGIN();
	m_InsideFrame = true;

	#if UNITY_WINRT
	ActivateD3D11BackBuffer(this);	// ?!-
	#endif
}



void GfxDeviceD3D11::EndFrame()
{
	DX11_LOG_ENTER_FUNCTION("EndFrame()");
	DX11_MARK_FRAME_END();
	m_InsideFrame = false;
}

bool GfxDeviceD3D11::IsValidState()
{
	return true;
}

void GfxDeviceD3D11::PresentFrame()
{
	#if !UNITY_WP8
	IDXGISwapChain* swapChain = GetD3D11SwapChain();
	if (swapChain)
		swapChain->Present (GetD3D11SyncInterval(), 0);
	#endif
	m_CBs.NewFrame();
}

void GfxDeviceD3D11::FinishRendering()
{
	// not needed on D3D
}



// ---------- immediate mode rendering

// we break very large immediate mode submissions into multiple batches internally
const int kMaxImmediateVerticesPerDraw = 8192;


ImmediateModeD3D11::ImmediateModeD3D11()
:	m_VB(NULL)
,	m_VBUsedBytes(0)
,	m_VBStartVertex(0)
{
}

ImmediateModeD3D11::~ImmediateModeD3D11()
{
	Assert (!m_VB);
}

void ImmediateModeD3D11::Cleanup()
{
	REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VB);
	SAFE_RELEASE(m_VB);
	m_VBUsedBytes = 0;
}


void ImmediateModeD3D11::Invalidate()
{
	m_Vertices.clear();
	memset( &m_Current, 0, sizeof(m_Current) );
	m_HadColor = false;
}

void GfxDeviceD3D11::ImmediateVertex( float x, float y, float z )
{
	// If the current batch is becoming too large, internally end it and begin it again.
	size_t currentSize = m_Imm.m_Vertices.size();
	if( currentSize >= kMaxImmediateVerticesPerDraw - 4 )
	{
		GfxPrimitiveType mode = m_Imm.m_Mode;
		// For triangles, break batch when multiple of 3's is reached.
		if( mode == kPrimitiveTriangles && currentSize % 3 == 0 )
		{
			bool hadColor = m_Imm.m_HadColor;
			ImmediateEnd();
			ImmediateBegin( mode );
			m_Imm.m_HadColor = hadColor;
		}
		// For other primitives, break on multiple of 4's.
		// NOTE: This won't quite work for triangle strips, but we'll just pretend
		// that will never happen.
		else if( mode != kPrimitiveTriangles && currentSize % 4 == 0 )
		{
			bool hadColor = m_Imm.m_HadColor;
			ImmediateEnd();
			ImmediateBegin( mode );
			m_Imm.m_HadColor = hadColor;
		}
	}
	Vector3f& vert = m_Imm.m_Current.vertex;
	vert.x = x;
	vert.y = y;
	vert.z = z;
	m_Imm.m_Vertices.push_back( m_Imm.m_Current );
}

void GfxDeviceD3D11::ImmediateNormal( float x, float y, float z )
{
	m_Imm.m_Current.normal.x = x;
	m_Imm.m_Current.normal.y = y;
	m_Imm.m_Current.normal.z = z;
}

void GfxDeviceD3D11::ImmediateColor( float r, float g, float b, float a )
{
	m_Imm.m_Current.color.Set (ColorRGBAf(r,g,b,a));
	m_Imm.m_HadColor = true;
}

void GfxDeviceD3D11::ImmediateTexCoordAll( float x, float y, float z )
{
	for( int i = 0; i < 8; ++i )
	{
		Vector3f& uv = m_Imm.m_Current.texCoords[i];
		uv.x = x;
		uv.y = y;
		uv.z = z;
	}
}

void GfxDeviceD3D11::ImmediateTexCoord( int unit, float x, float y, float z )
{
	if( unit < 0 || unit >= 8 )
	{
		ErrorString( "Invalid unit for texcoord" );
		return;
	}
	Vector3f& uv = m_Imm.m_Current.texCoords[unit];
	uv.x = x;
	uv.y = y;
	uv.z = z;
}

void GfxDeviceD3D11::ImmediateBegin( GfxPrimitiveType type )
{
	m_Imm.m_Mode = type;
	m_Imm.m_Vertices.clear();
	m_Imm.m_HadColor = false;
}

bool GfxDeviceD3D11::ImmediateEndSetup()
{
	if( m_Imm.m_Vertices.empty() )
		return false;

	HRESULT hr = S_OK;
	ID3D11DeviceContext* ctx = GetD3D11Context();

	// vertex buffer
	const int kImmediateVBSize = kMaxImmediateVerticesPerDraw * sizeof(ImmediateVertexD3D11);
	if (!m_Imm.m_VB)
	{
		ID3D11Device* dev = GetD3D11Device();
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = kImmediateVBSize;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		hr = dev->CreateBuffer (&desc, NULL, &m_Imm.m_VB);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_Imm.m_VB,kImmediateVBSize,this);
		SetDebugNameD3D11 (m_Imm.m_VB, "VertexBufferImmediate");
		m_Imm.m_VBUsedBytes = 0;
		m_Imm.m_VBStartVertex = 0;
	}

	const ImmediateVertexD3D11* vb = &m_Imm.m_Vertices[0];
	const int vertexCount = m_Imm.m_Vertices.size();
	const int vertexDataSize = vertexCount * sizeof(vb[0]);
	D3D11_MAPPED_SUBRESOURCE mapped;

	if (m_Imm.m_VBUsedBytes + vertexDataSize > kImmediateVBSize)
	{
		D3D11_CALL_HR(ctx->Map (m_Imm.m_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		m_Imm.m_VBUsedBytes = 0;
	}
	else
	{
		D3D11_CALL_HR(ctx->Map (m_Imm.m_VB, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped));
	}
	m_Imm.m_VBStartVertex = m_Imm.m_VBUsedBytes / sizeof(vb[0]);

	memcpy ((UInt8*)mapped.pData + m_Imm.m_VBUsedBytes, vb, vertexDataSize);
	D3D11_CALL_HR(ctx->Unmap (m_Imm.m_VB, 0));
	m_Imm.m_VBUsedBytes += vertexDataSize;

	UINT strides = sizeof(vb[0]);
	UINT offsets = 0;
	D3D11_CALL (ctx->IASetVertexBuffers(0, 1, &m_Imm.m_VB, &strides, &offsets));


	m_FFState.useUniformInsteadOfVertexColor = !m_Imm.m_HadColor;
	if (!IsShaderActive(kShaderVertex))
	{
		UInt64 textureSources = m_FFState.texUnitSources;
		for (int i = 0; i < gGraphicsCaps.maxTexCoords; ++i)
		{
			UInt32 source = (textureSources >> (i*4)) & 0xF;
			// In immediate mode, each texcoord binds to it's own stage, unless we have texgen
			// on some of them.
			if (source <= kTexSourceUV7)
			{
				textureSources = textureSources & ~(0xFUL<<i*4) | (UInt64(kTexSourceUV0+i) << i*4);
			}
		}
		m_FFState.texUnitSources = textureSources;
	}

	BeforeDrawCall (true);
	return true;
}


void GfxDeviceD3D11::ImmediateEndDraw()
{
	ID3D11DeviceContext* ctx = GetD3D11Context();
	int vertexCount = m_Imm.m_Vertices.size();

	// vertex layout
	ID3D11InputLayout* inputLayout = m_VertexDecls.GetImmVertexDecl (g_CurrentVSInputD3D11);
	if (inputLayout)
	{
		SetInputLayoutD3D11 (ctx, inputLayout);
		if (SetTopologyD3D11 (m_Imm.m_Mode, *this, ctx))
		{

			// draw
			switch (m_Imm.m_Mode)
			{
			case kPrimitiveTriangles:
				D3D11_CALL(ctx->Draw (vertexCount, m_Imm.m_VBStartVertex));
				m_Stats.AddDrawCall (vertexCount / 3, vertexCount);
				break;
			case kPrimitiveTriangleStripDeprecated:
				D3D11_CALL(ctx->Draw (vertexCount, m_Imm.m_VBStartVertex));
				m_Stats.AddDrawCall (vertexCount - 2, vertexCount);
				break;
			case kPrimitiveQuads:
				GetDynamicVBO(); // ensure it's created
				D3D11_CALL(ctx->IASetIndexBuffer (m_DynamicVBO->GetQuadsIB(), DXGI_FORMAT_R16_UINT, 0));
				D3D11_CALL(ctx->DrawIndexed (vertexCount/4*6, 0, m_Imm.m_VBStartVertex));
				m_Stats.AddDrawCall( vertexCount / 4 * 2, vertexCount );
				break;
			case kPrimitiveLines:
				D3D11_CALL(ctx->Draw (vertexCount, m_Imm.m_VBStartVertex));
				m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
				break;
			default:
				AssertString("ImmediateEnd: unknown draw mode");
			}
		}
	}

	// clear vertices
	m_Imm.m_Vertices.clear();
}


void GfxDeviceD3D11::ImmediateEnd()
{
	if (ImmediateEndSetup())
		ImmediateEndDraw();
}


typedef SmartComPointer<ID3D11RenderTargetView> RTVPointer;
typedef SmartComPointer<ID3D11Resource> ResourcePointer;
typedef SmartComPointer<ID3D11Texture2D> Texture2DPointer;


bool GfxDeviceD3D11::CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 )
{
	DX11_LOG_ENTER_FUNCTION("CaptureScreenshot(%d, %d, %dx%d)", left, bottom, width, height);
	HRESULT hr;
	ID3D11DeviceContext* ctx = GetD3D11Context();

	SetupDeferredSRGBWrite ();

	RenderSurfaceHandle currColorSurface = GetActiveRenderColorSurfaceBBD3D11();
	RenderColorSurfaceD3D11* colorSurf = reinterpret_cast<RenderColorSurfaceD3D11*>(currColorSurface.object);
	if (!colorSurf)
		return false;

	RTVPointer rtView;
	ctx->OMGetRenderTargets (1, &rtView, NULL);
	if (!rtView)
		return false;

	ResourcePointer rtRes;
	rtView->GetResource (&rtRes);
	if (!rtRes)
		return false;

	D3D11_RESOURCE_DIMENSION rtType;
	rtRes->GetType (&rtType);
	if (rtType != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return false;

	ID3D11Texture2D* rtTex = static_cast<ID3D11Texture2D*>((ID3D11Resource*)rtRes);
	D3D11_TEXTURE2D_DESC rtDesc;
	rtTex->GetDesc (&rtDesc);
	if (rtDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		rtDesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS &&
		rtDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
		rtDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
		return false;

	ID3D11Device* dev = GetD3D11Device();
	ResolveTexturePool::Entry* resolved = NULL;
	if (rtDesc.SampleDesc.Count != 1)
	{
		resolved = m_Resolves.GetResolveTexture (rtDesc.Width, rtDesc.Height, colorSurf->format, m_SRGBWrite);
		if (!resolved)
			return false;

		ctx->ResolveSubresource (resolved->texture, 0, rtTex, 0, rtDesc.Format);
		rtTex = resolved->texture;
	}

	Texture2DPointer stagingTex;
	D3D11_TEXTURE2D_DESC stagingDesc;
	stagingDesc.Width = width;
	stagingDesc.Height = height;
	stagingDesc.MipLevels = 1;
	stagingDesc.ArraySize = 1;

	bool useRGBA = rtDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
		rtDesc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
		rtDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	stagingDesc.Format = useRGBA ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
	stagingDesc.SampleDesc.Count = 1;
	stagingDesc.SampleDesc.Quality = 0;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;
	hr = dev->CreateTexture2D (&stagingDesc, NULL, &stagingTex);
	if (FAILED(hr))
		return false;
	SetDebugNameD3D11 (stagingTex, Format("CaptureScreenshot-Texture2D-%dx%d", width, height));

	D3D11_BOX srcBox;
	srcBox.left = left;
	srcBox.right = left + width; 
#if UNITY_WP8
	/* In WP8 m_CurrTargetHeight seems not to match 
	*  ID3D11DeviceContext height */
	srcBox.top = bottom;
	srcBox.bottom = bottom + height;
#else
	srcBox.top = m_CurrTargetHeight - (bottom + height);
	srcBox.bottom = m_CurrTargetHeight - (bottom);
#endif
	srcBox.front = 0;
	srcBox.back = 1;
	ctx->CopySubresourceRegion (stagingTex, 0, 0, 0, 0, rtTex, 0, &srcBox);

	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = ctx->Map (stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr))
		return false;

	rgba32 += (height-1) * width * sizeof(UInt32);
	const UInt8* src = (const UInt8*)mapped.pData;
	for (int y = 0; y < height; ++y)
	{
		if (useRGBA)
		{
			memcpy (rgba32, src, width*4);
		}
		else
		{
			for (int x = 0; x < width*4; x +=4)
			{
				rgba32[x] = src[x + 2];
				rgba32[x + 1] = src[x + 1];
				rgba32[x + 2] = src[x + 0];
				rgba32[x + 3] = src[x + 3];
			}
		}

		rgba32 -= width * sizeof(UInt32);
		src += mapped.RowPitch;
	}


	ctx->Unmap (stagingTex, 0);
	return true;
}



bool GfxDeviceD3D11::ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	Assert (image.GetFormat() == kTexFormatARGB32 || image.GetFormat() == kTexFormatRGB24);

	SetupDeferredSRGBWrite ();

	HRESULT hr;
	ID3D11DeviceContext* ctx = GetD3D11Context();

	RenderSurfaceHandle currColorSurface = GetActiveRenderColorSurfaceBBD3D11();
	RenderColorSurfaceD3D11* colorSurf = reinterpret_cast<RenderColorSurfaceD3D11*>(currColorSurface.object);
	if (!colorSurf)
		return false;

	RTVPointer rtView;
	ctx->OMGetRenderTargets (1, &rtView, NULL);
	if (!rtView)
		return false;

	ResourcePointer rtRes;
	rtView->GetResource (&rtRes);
	if (!rtRes)
		return false;

	D3D11_RESOURCE_DIMENSION rtType;
	rtRes->GetType (&rtType);
	if (rtType != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return false;

	ID3D11Texture2D* rtTex = static_cast<ID3D11Texture2D*>((ID3D11Resource*)rtRes);
	D3D11_TEXTURE2D_DESC rtDesc;
	rtTex->GetDesc (&rtDesc);
	if (rtDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && rtDesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS && rtDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
		return false;

	ID3D11Device* dev = GetD3D11Device();
	ResolveTexturePool::Entry* resolved = NULL;
	if (rtDesc.SampleDesc.Count != 1)
	{
		resolved = m_Resolves.GetResolveTexture (rtDesc.Width, rtDesc.Height, colorSurf->format, m_SRGBWrite);
		if (!resolved)
			return false;

		ctx->ResolveSubresource (resolved->texture, 0, rtTex, 0, rtDesc.Format);
		rtTex = resolved->texture;
	}

	Texture2DPointer stagingTex;
	D3D11_TEXTURE2D_DESC stagingDesc;
	stagingDesc.Width = width;
	stagingDesc.Height = height;
	stagingDesc.MipLevels = 1;
	stagingDesc.ArraySize = 1;
	stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	stagingDesc.SampleDesc.Count = 1;
	stagingDesc.SampleDesc.Quality = 0;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;
	hr = dev->CreateTexture2D (&stagingDesc, NULL, &stagingTex);
	if (FAILED(hr))
		return false;
	SetDebugNameD3D11 (stagingTex, Format("Readback-Texture2D-%dx%d", width, height));

	D3D11_BOX srcBox;
	srcBox.left = left;
	srcBox.right = left + width;
	srcBox.top = m_CurrTargetHeight - (bottom + height);
	srcBox.bottom = m_CurrTargetHeight - (bottom);
	srcBox.front = 0;
	srcBox.back = 1;
	ctx->CopySubresourceRegion (stagingTex, 0, 0, 0, 0, rtTex, 0, &srcBox);

	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = ctx->Map (stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr))
		return false;

	const UInt8* src = (const UInt8*)mapped.pData;

	if (image.GetFormat() == kTexFormatARGB32)
	{
		for (int y = height-1; y >= 0; --y)
		{
			const UInt32* srcPtr = (const UInt32*)src;
			UInt32* dstPtr = (UInt32*)(image.GetRowPtr(destY+y) + destX * 4);
			for (int x = 0; x < width; ++x)
			{
				UInt32 abgrCol = *srcPtr;
				UInt32 bgraCol = ((abgrCol & 0x00FFFFFF) << 8) | ((abgrCol&0xFF000000) >> 24);
				*dstPtr = bgraCol;
				++srcPtr;
				++dstPtr;
			}
			src += mapped.RowPitch;
		}
	}
	else if (image.GetFormat() == kTexFormatRGB24)
	{
		for (int y = height-1; y >= 0; --y)
		{
			const UInt32* srcPtr = (const UInt32*)src;
			UInt8* dstPtr = image.GetRowPtr(destY+y) + destX * 3;
			for (int x = 0; x < width; ++x)
			{
				UInt32 abgrCol = *srcPtr;
				dstPtr[0] = (abgrCol & 0x000000FF);
				dstPtr[1] = (abgrCol & 0x0000FF00) >> 8;
				dstPtr[2] = (abgrCol & 0x00FF0000) >> 16;
				++srcPtr;
				dstPtr += 3;
			}
			src += mapped.RowPitch;
		}
	}
	ctx->Unmap (stagingTex, 0);
	return true;
}

void GfxDeviceD3D11::GrabIntoRenderTexture(RenderSurfaceHandle rtHandle, RenderSurfaceHandle rd, int x, int y, int width, int height)
{
	DX11_LOG_ENTER_FUNCTION("GrabIntoRenderTexture(%p, %p, %d, %d, %dx%d)", rtHandle.object, rd.object, x, y, width, height);
	if (!rtHandle.IsValid())
		return;
	if (!g_D3D11CurrColorRT)
		return;
	RenderSurfaceHandle currColorSurface = GetActiveRenderColorSurfaceBBD3D11();
	RenderColorSurfaceD3D11* colorSurf = reinterpret_cast<RenderColorSurfaceD3D11*>(currColorSurface.object);
	if (!colorSurf)
		return;
	const bool sRGB = (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0) ? (colorSurf->flags & kSurfaceCreateSRGB) : false;

	RenderColorSurfaceD3D11* renderTexture = reinterpret_cast<RenderColorSurfaceD3D11*>(rtHandle.object);
	TexturesD3D11::D3D11Texture* texturePointer = m_Textures.GetTexture (renderTexture->textureID);
	if (!texturePointer)
		return;

	SetupDeferredSRGBWrite ();

	ID3D11Resource* srcResource = g_D3D11CurrColorRT->m_Texture;
	ID3D11Texture2D* srcTex = static_cast<ID3D11Texture2D*>(srcResource);
	D3D11_TEXTURE2D_DESC rtDesc;
	srcTex->GetDesc (&rtDesc);
	Assert (rtDesc.Width == colorSurf->width && rtDesc.Height == colorSurf->height);

	ID3D11DeviceContext* ctx = GetD3D11Context();
	ResolveTexturePool::Entry* resolved = NULL;
	if (rtDesc.SampleDesc.Count != 1)
	{
		resolved = m_Resolves.GetResolveTexture (rtDesc.Width, rtDesc.Height, colorSurf->format, sRGB);
		if (!resolved)
			return;

		ctx->ResolveSubresource (resolved->texture, 0, srcResource, 0, GetRenderTextureFormat(colorSurf->format, sRGB));
		srcResource = resolved->texture;
	}

	ID3D11Texture2D* dstTex = static_cast<ID3D11Texture2D*>(texturePointer->m_Texture);
	D3D11_TEXTURE2D_DESC dstDesc;
	dstTex->GetDesc (&dstDesc);

	if (GetBPPFromDXGIFormat(rtDesc.Format) == GetBPPFromDXGIFormat(dstDesc.Format))
	{
		D3D11_BOX srcBox;
		srcBox.left = x;
		srcBox.right = x + width;
		srcBox.top = m_CurrTargetHeight - (y + height);
		srcBox.bottom = m_CurrTargetHeight - (y);
		srcBox.front = 0;
		srcBox.back = 1;

		ctx->CopySubresourceRegion (texturePointer->m_Texture, 0, 0, 0, 0, srcResource, 0, &srcBox);
	}
	else
	{
		// formats not compatible; have to draw a quad into destination, sampling the source texture
		RenderColorSurfaceD3D11* currRT = g_D3D11CurrColorRT;
		int oldTargetHeight = m_CurrTargetHeight;
		int oldView[4];
		GetViewport (oldView);
		bool oldScissor = IsScissorEnabled();
		int oldScissorRect[4];
		GetScissorRect (oldScissorRect);

		SetViewport (0, 0, dstDesc.Width, dstDesc.Height);
		DisableScissor ();

		RenderSurfaceHandle currColor = GetActiveRenderColorSurface(0);
		RenderSurfaceHandle currDepth = GetActiveRenderDepthSurface();
		SetRenderTargets (1, &rtHandle, rd, 0, kCubeFaceUnknown);

		const float u0 = x / float(rtDesc.Width);
		const float u1 = (x+width) / float(rtDesc.Width);
		const float v0 = (rtDesc.Height - y) / float(rtDesc.Height);
		const float v1 = (rtDesc.Height - (y+height)) / float(rtDesc.Height);

		ID3D11ShaderResourceView* srv = currRT ? currRT->m_SRView : NULL;
		if (resolved)
			srv = resolved->srv;

		DrawQuad (u0, v0, u1, v1, 0.0f, srv);
		SetRenderTargets (1, &currColor, currDepth, 0, kCubeFaceUnknown);
		SetViewport (oldView[0], oldView[1], oldView[2], oldView[3]);
		if (oldScissor)
			SetScissorRect (oldScissorRect[0], oldScissorRect[1], oldScissorRect[2], oldScissorRect[3]);
	}
}

void GfxDeviceD3D11::DrawQuad (float u0, float v0, float u1, float v1, float z, ID3D11ShaderResourceView* texture)
{
	// Can't use DeviceMVPMatricesState since that tries to get potentially threaded device.
	// We need to access our own device directly.
	Matrix4x4f	m_World, m_View, m_Proj;
	CopyMatrix(GetViewMatrix(), m_View.GetPtr());
	CopyMatrix(GetWorldMatrix(), m_World.GetPtr());
	CopyMatrix(GetProjectionMatrix(), m_Proj.GetPtr());

	// Can't use LoadFullScreenOrthoMatrix for the same reason.
	Matrix4x4f matrix;
	matrix.SetOrtho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 100.0f);
	SetProjectionMatrix (matrix);
	SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity

	DisableFog ();

	SetFFLighting (false, false, kColorMatDisabled);
	DisableLights (0);

	ShaderLab::SubProgram* programs[kShaderTypeCount] = {0};
	GraphicsHelper::SetShaders (*this, programs, NULL);

	GfxBlendState blendDesc;
	DeviceBlendState* blendState = CreateBlendState(blendDesc);
	SetBlendState (blendState, 0.0f);

	GfxDepthState depthDesc; depthDesc.depthWrite = false; depthDesc.depthFunc = kFuncAlways;
	DeviceDepthState* depthState = CreateDepthState(depthDesc);
	SetDepthState (depthState);

	GfxRasterState rasterDesc; rasterDesc.cullMode = kCullOff;
	DeviceRasterState* rasterState = CreateRasterState(rasterDesc);
	SetRasterState (rasterState);

	ShaderLab::TextureBinding texEnv;
	texEnv.m_TextureName.index = kShaderTexEnvWhite | ShaderLab::FastPropertyName::kBuiltinTexEnvMask;
	TextureCombinersHandle combiners = CreateTextureCombiners (1, &texEnv, NULL, false, false);

	// Can't call SetTextureCombiners here since that expects to be called on main thread,
	// and we might be on the device thread here. So do the work manually and call
	// SetTextureCombinersThreadable.

	TexEnvData texEnvData;
	memset(&texEnvData, 0, sizeof(texEnvData));
	texEnvData.textureID = TextureID();
	texEnvData.texDim = kTexDim2D;
	texEnvData.texGen = kTexGenDisabled;
	texEnvData.identityMatrix = true;
	Vector4f texColors;
	texColors.Set(1,1,1,1);
	SetTextureCombinersThreadable (combiners, &texEnvData, &texColors);

	ImmediateBegin (kPrimitiveQuads);
	ImmediateTexCoord(0,u0,v0,0.0f); ImmediateVertex (0.0f, 0.0f, z);
	ImmediateTexCoord(0,u0,v1,0.0f); ImmediateVertex (0.0f, 1.0f, z);
	ImmediateTexCoord(0,u1,v1,0.0f); ImmediateVertex (1.0f, 1.0f, z);
	ImmediateTexCoord(0,u1,v0,0.0f); ImmediateVertex (1.0f, 0.0f, z);
	if (ImmediateEndSetup ())
	{
		ID3D11SamplerState* sampler = m_Textures.GetSampler (kSamplerPointClamp);
		Assert (sampler);
		ID3D11DeviceContext* ctx = GetD3D11Context();
		ctx->PSSetShaderResources (0, 1, &texture);
		ctx->PSSetSamplers (0, 1, &sampler);
		m_Textures.InvalidateSampler (kShaderFragment, 0);
		m_ActiveTextures[kShaderFragment][0].m_ID = -1;
		m_ActiveSamplers[kShaderFragment][0].m_ID = -1;

		ImmediateEndDraw ();

		ID3D11ShaderResourceView* nullTex = NULL;
		ctx->PSSetShaderResources (0, 1, &nullTex);
	}

	// restore matrices
	SetViewMatrix(m_View.GetPtr());
	SetWorldMatrix(m_World.GetPtr());
	SetProjectionMatrix(m_Proj);
}

void* GfxDeviceD3D11::GetNativeGfxDevice()
{
	return GetD3D11Device();
}

void* GfxDeviceD3D11::GetNativeTexturePointer(TextureID id)
{
	TexturesD3D11::D3D11Texture* tex = m_Textures.GetTexture(id);
	if (!tex)
		return NULL;
	return tex->m_Texture;
}

intptr_t GfxDeviceD3D11::CreateExternalTextureFromNative(intptr_t nativeTex)
{
	return m_Textures.RegisterNativeTexture((ID3D11ShaderResourceView*)nativeTex);
}

void GfxDeviceD3D11::UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex)
{
	m_Textures.UpdateNativeTexture(tex, (ID3D11ShaderResourceView*)nativeTex);
}


#if ENABLE_PROFILER

void GfxDeviceD3D11::BeginProfileEvent (const char* name)
{
	if (g_D3D11BeginEventFunc)
	{
		wchar_t wideName[100];
		UTF8ToWide (name, wideName, 100);
		g_D3D11BeginEventFunc (0, wideName);
	}
}

void GfxDeviceD3D11::EndProfileEvent ()
{
	if (g_D3D11EndEventFunc)
	{
		g_D3D11EndEventFunc ();
	}
}

GfxTimerQuery* GfxDeviceD3D11::CreateTimerQuery()
{
	Assert(gGraphicsCaps.hasTimerQuery);
	return g_TimerQueriesD3D11.CreateTimerQuery();
}

void GfxDeviceD3D11::DeleteTimerQuery(GfxTimerQuery* query)
{
	delete query;
}

void GfxDeviceD3D11::BeginTimerQueries()
{
	if(!gGraphicsCaps.hasTimerQuery)
		return;

	g_TimerQueriesD3D11.BeginTimerQueries();
	}

void GfxDeviceD3D11::EndTimerQueries()
{
	if(!gGraphicsCaps.hasTimerQuery)
		return;

	g_TimerQueriesD3D11.EndTimerQueries();
}

#endif // ENABLE_PROFILER


// -------- editor only functions

#if UNITY_EDITOR

void GfxDeviceD3D11::SetAntiAliasFlag (bool aa)
{
}


void GfxDeviceD3D11::DrawUserPrimitives (GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride)
{
	if (vertexCount == 0)
		return;

	Assert(vertexCount <= 60000); // TODO: handle this by multi-batching

	Assert(data && vertexCount >= 0 && vertexChannels != 0);

	DynamicD3D11VBO& vbo = static_cast<DynamicD3D11VBO&>(GetDynamicVBO());

	void* vbPtr;
	//@TODO: hack to pass kDrawTriangleStrip, but we only need that to determine if we need index buffer or not (we don't)
	if (!vbo.GetChunk(vertexChannels, vertexCount, 0, DynamicVBO::kDrawTriangleStrip, &vbPtr, NULL))
		return;
	memcpy (vbPtr, data, vertexCount * stride);
	vbo.ReleaseChunk (vertexCount, 0);

	vbo.DrawChunkUserPrimitives (type);
}

int GfxDeviceD3D11::GetCurrentTargetAA() const
{
	return 0; //@TODO
}

GfxDeviceWindow* GfxDeviceD3D11::CreateGfxWindow (HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias)
{
	return new D3D11Window(window, width, height, depthFormat, antiAlias);
}

static ID3D11Texture2D* FindD3D11TextureByID (TextureID tid)
{
	GfxDevice& device = GetRealGfxDevice();
	if (device.GetRenderer() != kGfxRendererD3D11)
		return NULL;
	GfxDeviceD3D11& dev = static_cast<GfxDeviceD3D11&>(device);
	TexturesD3D11::D3D11Texture* basetex = dev.GetTextures().GetTexture(tid);
	if (!basetex || !basetex->m_Texture)
		return NULL;
	D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	basetex->m_Texture->GetType(&dim);
	if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return NULL;
	return static_cast<ID3D11Texture2D*>(basetex->m_Texture);
}

HDC AcquireHDCForTextureD3D11 (TextureID tid, int& outWidth, int& outHeight)
{
	ID3D11Texture2D* tex = FindD3D11TextureByID (tid);
	if (!tex)
		return NULL;

	D3D11_TEXTURE2D_DESC desc;
	tex->GetDesc (&desc);
	outWidth = desc.Width;
	outHeight = desc.Height;

	IDXGISurface1* dxgiSurface = NULL;
	tex->QueryInterface(__uuidof(IDXGISurface1), (void**)(&dxgiSurface));
	HDC dc = NULL;
	if (dxgiSurface)
	{
		dxgiSurface->GetDC (false, &dc);
		dxgiSurface->Release();
	}
	return dc;
}

void ReleaseHDCForTextureD3D11 (TextureID tid, HDC dc)
{
	ID3D11Texture2D* tex = FindD3D11TextureByID (tid);
	if (!tex)
		return;

	IDXGISurface1* dxgiSurface = NULL;
	tex->QueryInterface(__uuidof(IDXGISurface1), (void**)(&dxgiSurface));
	if (dxgiSurface)
	{
		dxgiSurface->ReleaseDC(NULL);
		dxgiSurface->Release();
	}
}

#endif // UNITY_EDITOR


int GfxDeviceD3D11::GetCurrentTargetWidth() const
{
	return m_CurrTargetWidth;
}

int GfxDeviceD3D11::GetCurrentTargetHeight() const
{
	return m_CurrTargetHeight;
}

void GfxDeviceD3D11::SetCurrentTargetSize(int width, int height)
{
	m_CurrTargetWidth = width;
	m_CurrTargetHeight = height;
}

void GfxDeviceD3D11::SetCurrentWindowSize(int width, int height)
{
	m_CurrWindowWidth = m_CurrTargetWidth = width;
	m_CurrWindowHeight = m_CurrTargetHeight = height;
}


// ----------------------------------------------------------------------

void GfxDeviceD3D11::SetComputeBuffer11 (ShaderType shaderType, int unit, ComputeBufferID bufferHandle)
{
	ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(bufferHandle);
	ID3D11ShaderResourceView* srv = buffer ? buffer->srv : NULL;
	ID3D11DeviceContext* ctx = GetD3D11Context();
	switch (shaderType) {
	case kShaderVertex:   ctx->VSSetShaderResources (unit, 1, &srv); break;
	case kShaderFragment: ctx->PSSetShaderResources (unit, 1, &srv); break;
	case kShaderGeometry: ctx->GSSetShaderResources (unit, 1, &srv); break;
	case kShaderHull:     ctx->HSSetShaderResources (unit, 1, &srv); break;
	case kShaderDomain:   ctx->DSSetShaderResources (unit, 1, &srv); break;
	default: AssertString("unknown shader type");
	}
	m_ActiveTextures[shaderType][unit].m_ID = 0;
}


void GfxDeviceD3D11::SetComputeBufferData (ComputeBufferID bufferHandle, const void* data, size_t size)
{
	if (!data || !size)
		return;
	ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(bufferHandle);
	if (!buffer || !buffer->buffer)
		return;

	ID3D11DeviceContext* ctx = GetD3D11Context();
	D3D11_BOX box;
	box.left = 0;
	box.top = 0;
	box.front = 0;
	box.right = size;
	box.bottom = 1;
	box.back = 1;
	ctx->UpdateSubresource (buffer->buffer, 0, &box, data, 0, 0);
}


void GfxDeviceD3D11::GetComputeBufferData (ComputeBufferID bufferHandle, void* dest, size_t destSize)
{
	if (!dest || !destSize)
		return;
	ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(bufferHandle);
	if (!buffer || !buffer->buffer)
		return;

	ID3D11DeviceContext* ctx = GetD3D11Context();

	ID3D11Buffer* cpuBuffer = NULL;
	D3D11_BUFFER_DESC desc;
	ZeroMemory (&desc, sizeof(desc));
	buffer->buffer->GetDesc (&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	HRESULT hr = GetD3D11Device()->CreateBuffer(&desc, NULL, &cpuBuffer);
	if (FAILED(hr))
		return;
	SetDebugNameD3D11 (cpuBuffer, Format("CSGetData-Staging-%d", desc.ByteWidth));

	ctx->CopyResource (cpuBuffer, buffer->buffer);

	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = ctx->Map (cpuBuffer, 0, D3D11_MAP_READ, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy (dest, mapped.pData, destSize);
		ctx->Unmap (cpuBuffer, 0);
	}
	SAFE_RELEASE (cpuBuffer);
}



void GfxDeviceD3D11::CopyComputeBufferCount (ComputeBufferID srcBuffer, ComputeBufferID dstBuffer, UInt32 dstOffset)
{
	ComputeBuffer11* src = m_Textures.GetComputeBuffer(srcBuffer);
	if (!src || !src->uav)
		return;
	ComputeBuffer11* dst = m_Textures.GetComputeBuffer(dstBuffer);
	if (!dst || !dst->buffer)
		return;
	ID3D11DeviceContext* ctx = GetD3D11Context();
	ctx->CopyStructureCount (dst->buffer, dstOffset, src->uav);
}



void GfxDeviceD3D11::SetRandomWriteTargetTexture (int index, TextureID tid)
{
	void SetRandomWriteTargetTextureD3D11 (int index, TextureID tid);
	SetRandomWriteTargetTextureD3D11 (index, tid);
}

void GfxDeviceD3D11::SetRandomWriteTargetBuffer (int index, ComputeBufferID bufferHandle)
{
	void SetRandomWriteTargetBufferD3D11 (int index, ComputeBufferID bufferHandle);
	SetRandomWriteTargetBufferD3D11 (index, bufferHandle);
}

void GfxDeviceD3D11::ClearRandomWriteTargets ()
{
	void ClearRandomWriteTargetsD3D11 (TexturesD3D11* textures);
	ClearRandomWriteTargetsD3D11 (&m_Textures);
}


ComputeProgramHandle GfxDeviceD3D11::CreateComputeProgram (const UInt8* code, size_t codeSize)
{
	ComputeProgramHandle cpHandle;
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level11_0)
		return cpHandle;

	ID3D11Device* dev = GetD3D11Device();
	HRESULT hr;
	ID3D11ComputeShader* cs = NULL;
	hr = dev->CreateComputeShader (code, codeSize, NULL, &cs);
	if (FAILED(hr))
		return cpHandle;
	SetDebugNameD3D11 (cs, Format("ComputeShader-%d", (int)codeSize));

	cpHandle.object = cs;
	return cpHandle;
}

void GfxDeviceD3D11::DestroyComputeProgram (ComputeProgramHandle& cpHandle)
{
	if (!cpHandle.IsValid())
		return;

	ID3D11ComputeShader* cs = reinterpret_cast<ID3D11ComputeShader*>(cpHandle.object);
	SAFE_RELEASE(cs);
	cpHandle.Reset();
}

void GfxDeviceD3D11::CreateComputeConstantBuffers (unsigned count, const UInt32* sizes, ConstantBufferHandle* outCBs)
{
	ID3D11Device* dev = GetD3D11Device();
	HRESULT hr;

	D3D11_BUFFER_DESC desc;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	for (unsigned i = 0; i < count; ++i)
	{
		desc.ByteWidth = sizes[i];
		ID3D11Buffer* cb = NULL;
		hr = dev->CreateBuffer (&desc, NULL, &cb);
		if (cb)
			REGISTER_EXTERNAL_GFX_ALLOCATION_REF(cb,sizes[i],this);
		Assert (SUCCEEDED(hr));
		outCBs[i].object = cb;

		SetDebugNameD3D11 (cb, Format("CSConstantBuffer-%d-%d", i, sizes[i]));
	}
}

void GfxDeviceD3D11::DestroyComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs)
{
	for (unsigned i = 0; i < count; ++i)
	{
		ID3D11Buffer* cb = reinterpret_cast<ID3D11Buffer*>(cbs[i].object);
		REGISTER_EXTERNAL_GFX_DEALLOCATION(cb);
		SAFE_RELEASE(cb);
		cbs[i].Reset();
	}
}


void GfxDeviceD3D11::CreateComputeBuffer (ComputeBufferID id, size_t count, size_t stride, UInt32 flags)
{
	ComputeBuffer11 buffer;
	buffer.buffer = NULL;
	buffer.srv = NULL;
	buffer.uav = NULL;
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0)
		return;

	ID3D11Device* dev = GetD3D11Device();
	HRESULT hr;

	// buffer
	D3D11_BUFFER_DESC bufferDesc;
	memset (&bufferDesc, 0, sizeof(bufferDesc));
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level11_0)
		bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	bufferDesc.ByteWidth = count * stride;
	if (flags & kCBFlagDrawIndirect)
		bufferDesc.MiscFlags = (gGraphicsCaps.d3d11.featureLevel >= kDX11Level11_0 ? D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS : 0);
	else if (flags & kCBFlagRaw)
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	else
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = stride;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	hr = dev->CreateBuffer (&bufferDesc, NULL, &buffer.buffer);
	if (buffer.buffer)
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(buffer.buffer,count * stride,this);
	Assert (SUCCEEDED(hr));
	SetDebugNameD3D11 (buffer.buffer, Format("ComputeBuffer-%dx%d", (int)count, (int)stride));

	// unordered access view, only on DX11+ HW
	if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level11_0 && !(flags & kCBFlagDrawIndirect))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc; 
		memset (&uavDesc, 0, sizeof(uavDesc));
		uavDesc.Format = (flags & kCBFlagRaw) ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = flags & kCBFlagTypeMask;
		hr = dev->CreateUnorderedAccessView (buffer.buffer, &uavDesc, &buffer.uav);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (buffer.uav, Format("ComputeBuffer-UAV-%dx%d", (int)count, (int)stride));

		// shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		memset (&srvDesc, 0, sizeof(srvDesc));
		if (flags & kCBFlagRaw)
		{
			srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srvDesc.BufferEx.FirstElement = 0;
			srvDesc.BufferEx.NumElements = count;
			srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		}
		else
		{
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = count;
		}
		hr = dev->CreateShaderResourceView (buffer.buffer, &srvDesc, &buffer.srv);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (buffer.uav, Format("ComputeBuffer-SRV-%dx%d", (int)count, (int)stride));
	}

	m_Textures.AddComputeBuffer (id, buffer);
}


void GfxDeviceD3D11::DestroyComputeBuffer (ComputeBufferID handle)
{
	if (!handle.IsValid())
		return;

	ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(handle);
	if (buffer)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(buffer->buffer);
		SAFE_RELEASE(buffer->buffer);
		SAFE_RELEASE(buffer->srv);
		SAFE_RELEASE(buffer->uav);
	}
	m_Textures.RemoveComputeBuffer (handle);
}


void GfxDeviceD3D11::UpdateComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs,  UInt32 cbDirty, size_t dataSize, const UInt8* data, const UInt32* cbSizes, const UInt32* cbOffsets, const int* bindPoints)
{
	ID3D11DeviceContext* ctx = GetD3D11Context();

	// go over constant buffers in use
	for (unsigned i = 0; i < count; ++i)
	{
		if (bindPoints[i] < 0)
			continue; // CB not going to be used, no point in updating it

		ID3D11Buffer* cb = reinterpret_cast<ID3D11Buffer*>(cbs[i].object);

		// update buffer if dirty
		UInt32 dirtyMask = (1<<i);
		if (cbDirty & dirtyMask)
		{
			D3D11_MAPPED_SUBRESOURCE mapped;
			HRESULT hr;
			hr = ctx->Map (cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			Assert (SUCCEEDED(hr));
			memcpy (mapped.pData, data + cbOffsets[i], cbSizes[i]);
			ctx->Unmap (cb, 0);
		}

		// bind it
		ctx->CSSetConstantBuffers (bindPoints[i], 1, &cb);
	}
}



void GfxDeviceD3D11::UpdateComputeResources (
	unsigned texCount, const TextureID* textures, const int* texBindPoints,
	unsigned samplerCount, const unsigned* samplers,
	unsigned inBufferCount, const ComputeBufferID* inBuffers, const int* inBufferBindPoints,
	unsigned outBufferCount, const ComputeBufferID* outBuffers, const TextureID* outTextures, const UInt32* outBufferBindPoints)
{
	ID3D11DeviceContext* ctx = GetD3D11Context();

	for (unsigned i = 0; i < texCount; ++i)
	{
		if (textures[i].m_ID == 0)
			continue;
		TexturesD3D11::D3D11Texture* tex = m_Textures.GetTexture (textures[i]);
		if (!tex)
			continue;

		// if texture is bound as render target: unbind it (set backbuffer as RT)
		if ((g_D3D11CurrColorRT && g_D3D11CurrColorRT->m_Texture == tex->m_Texture) ||
			(g_D3D11CurrDepthRT && g_D3D11CurrDepthRT->m_Texture == tex->m_Texture))
		{
			RenderSurfaceHandle defaultColor = GetBackBufferColorSurface();
			RenderSurfaceHandle defaultDepth = GetBackBufferDepthSurface();
			SetRenderTargets (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown);
		}
		ctx->CSSetShaderResources (texBindPoints[i] & 0xFFFF, 1, &tex->m_SRV);
		unsigned samplerBindPoint = (texBindPoints[i] >> 16) & 0xFFFF;
		if (samplerBindPoint != 0xFFFF)
		{
			ID3D11SamplerState* smp = m_Textures.GetSampler(tex->m_Sampler);
			ctx->CSSetSamplers (samplerBindPoint, 1, &smp);
		}
	}

	for (unsigned i = 0; i < samplerCount; ++i)
	{
		BuiltinSamplerState type = (BuiltinSamplerState)((samplers[i] & 0xFFFF0000) >> 16);
		unsigned bindPoint = samplers[i] & 0xFFFF;
		ID3D11SamplerState* smp = m_Textures.GetSampler (type);
		Assert (smp);
		ctx->CSSetSamplers (bindPoint, 1, &smp);
	}

	for (unsigned i = 0; i < inBufferCount; ++i)
	{
		ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(inBuffers[i]);
		if (!buffer)
			continue;
		ctx->CSSetShaderResources (inBufferBindPoints[i], 1, &buffer->srv);
	}

	for (unsigned i = 0; i < outBufferCount; ++i)
	{
		ID3D11UnorderedAccessView* uav = NULL;
		if (outBufferBindPoints[i] & 0x80000000)
		{
			// UAV comes from texture
			if (outTextures[i].m_ID == 0)
				continue;
			TexturesD3D11::D3D11Texture* tex = m_Textures.GetTexture (outTextures[i]);
			if (!tex || !tex->m_UAV)
				continue;
			uav = tex->m_UAV;
		}
		else
		{
			// UAV is raw buffer
			if (!outBuffers[i].IsValid())
				continue;
			ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(outBuffers[i]);
			if (!buffer)
				continue;
			uav = buffer->uav;
		}
		UINT uavInitialCounts[] = { -1 }; // keeps current offsets for Appendable/Consumeable UAVs
		ctx->CSSetUnorderedAccessViews (outBufferBindPoints[i] & 0x7FFFFFFF, 1, &uav, uavInitialCounts);
	}
}



void GfxDeviceD3D11::DispatchComputeProgram (ComputeProgramHandle cpHandle, unsigned threadsX, unsigned threadsY, unsigned threadsZ)
{
	if (!cpHandle.IsValid())
		return;

	ID3D11DeviceContext* ctx = GetD3D11Context();
	ID3D11ComputeShader* cs = reinterpret_cast<ID3D11ComputeShader*>(cpHandle.object);
	ctx->CSSetShader (cs, NULL, 0);
	ctx->Dispatch (threadsX, threadsY, threadsZ);


	// DEBUG: readback output UAV contents
	#if 0 && !UNITY_RELEASE
	ID3D11UnorderedAccessView* uav;
	ctx->CSGetUnorderedAccessViews (0, 1, &uav);
	ID3D11Buffer* res;
	uav->GetResource ((ID3D11Resource**)&res);

	ID3D11Buffer* debugbuf = NULL;
	D3D11_BUFFER_DESC desc;
	ZeroMemory (&desc, sizeof(desc));
	res->GetDesc (&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	GetD3D11Device()->CreateBuffer(&desc, NULL, &debugbuf);
	ctx->CopyResource (debugbuf, res);

	D3D11_MAPPED_SUBRESOURCE mapped;
	ctx->Map(debugbuf, 0, D3D11_MAP_READ, 0, &mapped);
	ctx->Unmap(debugbuf, 0);
	SAFE_RELEASE(debugbuf);
	#endif

	ID3D11UnorderedAccessView* nullUAVs[8] = {0};
	ctx->CSSetUnorderedAccessViews (0, 8, nullUAVs, NULL);
}


// ----------------------------------------------------------------------


void GfxDeviceD3D11::DrawNullGeometry (GfxPrimitiveType topology, int vertexCount, int instanceCount)
{
	ID3D11DeviceContext* ctx = GetD3D11Context();
	UINT strides = 0;
	UINT offsets = 0;
	ID3D11Buffer* vb = NULL;
	D3D11_CALL (ctx->IASetVertexBuffers(0, 1, &vb, &strides, &offsets));

	BeforeDrawCall (false);

	// vertex layout
	SetInputLayoutD3D11 (ctx, NULL);

	// draw
	if (!SetTopologyD3D11 (topology, *this, ctx))
		return;
	if (instanceCount > 1)
	{
		D3D11_CALL (ctx->DrawInstanced (vertexCount, instanceCount, 0, 0));
	}
	else
	{
		D3D11_CALL (ctx->Draw (vertexCount, 0));
	}
}

void GfxDeviceD3D11::DrawNullGeometryIndirect (GfxPrimitiveType topology, ComputeBufferID bufferHandle, UInt32 bufferOffset)
{
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level11_0)
		return;

	ID3D11DeviceContext* ctx = GetD3D11Context();
	UINT strides = 0;
	UINT offsets = 0;
	ID3D11Buffer* vb = NULL;
	D3D11_CALL (ctx->IASetVertexBuffers(0, 1, &vb, &strides, &offsets));

	BeforeDrawCall (false);

	// vertex layout
	SetInputLayoutD3D11 (ctx, NULL);

	// draw
	if (!SetTopologyD3D11 (topology, *this, ctx))
		return;
	ComputeBuffer11* buffer = m_Textures.GetComputeBuffer(bufferHandle);
	if (!buffer || !buffer->buffer)
		return;
	D3D11_CALL (ctx->DrawInstancedIndirect (buffer->buffer, bufferOffset));
}


// GPU skinning functionality
GPUSkinningInfo * GfxDeviceD3D11::CreateGPUSkinningInfo()
{
	// stream-out requires at least DX10.0
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0)
		return NULL;

	return new StreamOutSkinningInfo();
}

void	GfxDeviceD3D11::DeleteGPUSkinningInfo(GPUSkinningInfo *info)
{
	delete reinterpret_cast<StreamOutSkinningInfo *>(info);
}

// All actual functionality is performed in StreamOutSkinningInfo, just forward the calls
void GfxDeviceD3D11::SkinOnGPU( GPUSkinningInfo * info, bool lastThisFrame )
{
	reinterpret_cast<StreamOutSkinningInfo *>(info)->SkinMesh(lastThisFrame);
}

void GfxDeviceD3D11::UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty)
{
	reinterpret_cast<StreamOutSkinningInfo *>(info)->UpdateSourceData(vertData, skinData, dirty);
}

void GfxDeviceD3D11::UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses)
{
	reinterpret_cast<StreamOutSkinningInfo *>(info)->UpdateSourceBones(boneCount, poses);
}


// ----------------------------------------------------------------------
//  verification of state

#if GFX_DEVICE_VERIFY_ENABLE


void GfxDeviceD3D11::VerifyState()
{
}
#endif // GFX_DEVICE_VERIFY_ENABLE
