#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "GfxDeviceD3D9.h"
#include "D3D9Context.h"
#include "Runtime/Math/FloatConversion.h"
#include "D3D9VBO.h"
#include "CombinerD3D.h"
#include "External/shaderlab/Library/program.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "External/shaderlab/Library/pass.h"
#include "Runtime/GfxDevice/BuiltinShaderParams.h"
#include "Runtime/GfxDevice/GpuProgramParamsApply.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "PlatformDependent/Win/SmartComPointer.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Misc/Plugins.h"
#include "D3D9Utils.h"
#include "D3D9Window.h"
#include "RenderTextureD3D.h"
#include "GpuProgramsD3D.h"
#include "TimerQueryD3D9.h"
#include "GfxDeviceD3D9.h"


// --------------------------------------------------------------------------

bool IsActiveRenderTargetWithColorD3D9();

typedef std::list<IDirect3DQuery9*> D3D9QueryList;
static D3D9QueryList s_EventQueries;

static void PushEventQuery (int maxBuffer);
static void CleanupEventQueries ();



static const D3DBLEND kBlendModeD3D9[] = {
	D3DBLEND_ZERO, D3DBLEND_ONE, D3DBLEND_DESTCOLOR, D3DBLEND_SRCCOLOR, D3DBLEND_INVDESTCOLOR, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCCOLOR,
	D3DBLEND_DESTALPHA, D3DBLEND_INVDESTALPHA, D3DBLEND_SRCALPHASAT, D3DBLEND_INVSRCALPHA,
};

static const D3DBLENDOP kBlendOpD3D9[] = {
	D3DBLENDOP_ADD, D3DBLENDOP_SUBTRACT, D3DBLENDOP_REVSUBTRACT, D3DBLENDOP_MIN, D3DBLENDOP_MAX,
};

static const D3DCMPFUNC kCmpFuncD3D9[] = {
	D3DCMP_ALWAYS, D3DCMP_NEVER, D3DCMP_LESS, D3DCMP_EQUAL, D3DCMP_LESSEQUAL, D3DCMP_GREATER, D3DCMP_NOTEQUAL, D3DCMP_GREATEREQUAL, D3DCMP_ALWAYS
};

static const D3DSTENCILOP kStencilOpD3D9[] = {
	D3DSTENCILOP_KEEP, D3DSTENCILOP_ZERO, D3DSTENCILOP_REPLACE, D3DSTENCILOP_INCRSAT,
	D3DSTENCILOP_DECRSAT, D3DSTENCILOP_INVERT, D3DSTENCILOP_INCR, D3DSTENCILOP_DECR
};

static D3DCULL kCullModeD3D9[] = {
	D3DCULL_NONE, D3DCULL_CW, D3DCULL_CCW
};

// --------------------------------------------------------------------------


static inline D3DCOLOR ColorToD3D( const float color[4] )
{
	return D3DCOLOR_RGBA( NormalizedToByte(color[0]), NormalizedToByte(color[1]), NormalizedToByte(color[2]), NormalizedToByte(color[3]) );
}



// --------------------------------------------------------------------------

enum {
	kNeedsSoftwareVPVertexShader = (1<<0),
	kNeedsSoftwareVPTexGen = (1<<1),
};

class GfxDeviceD3D9;

static void ApplyBackfaceMode( DeviceStateD3D& state );
static void ApplyStencilFuncAndOp( DeviceStateD3D& state );





void DeviceStateD3D::Invalidate( GfxDeviceD3D9& device )
{
	int i;

	depthFunc = kFuncUnknown;
	depthWrite = -1;

	blending = -1; // unknown
	srcBlend = destBlend = srcBlendAlpha = destBlendAlpha = -1; // won't match any D3D mode
	blendOp = blendOpAlpha = -1; // won't match any D3D mode
	alphaFunc = kFuncUnknown;
	alphaValue = -1.0f;

	culling = kCullUnknown;
	d3dculling = D3DCULL_FORCE_DWORD;
	scissor = -1;

	offsetFactor = offsetUnits = -1000.0f;
	for( i = 0; i < kShaderTypeCount; ++i )
	{
		activeGpuProgramParams[i] = NULL;
		activeGpuProgram[i] = NULL;
		activeShader[i] = NULL;
	}
	fixedFunctionPS = 0;

	colorWriteMask = -1; // TBD ?
	m_StencilRef = -1;

	for (i = 0; i < ARRAY_SIZE(texturesPS); ++i)
		texturesPS[i].Invalidate();
	for (i = 0; i < ARRAY_SIZE(texturesVS); ++i)
		texturesVS[i].Invalidate();

	m_SoftwareVP = false;
	m_NeedsSofwareVPFlags = 0;

	IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
	if( dev && !m_DeviceLost )
	{
		D3D9_CALL(dev->SetVertexShader( NULL ));
		D3D9_CALL(dev->SetPixelShader( NULL ));

		ApplyBackfaceMode( *this );

		if( g_D3DUsesMixedVP )
			D3D9_CALL(dev->SetSoftwareVertexProcessing( FALSE ));

		// misc. state
		D3D9_CALL(dev->SetRenderState( D3DRS_LOCALVIEWER, TRUE ));

		#if UNITY_EDITOR
		D3D9_CALL(dev->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID ));
		#endif
	}
}


void UpdateChannelBindingsD3D( const ChannelAssigns& channels )
{
	// Texture coordinate index bindings
	GfxDeviceD3D9& device = (GfxDeviceD3D9&)GetRealGfxDevice();
	if( device.IsShaderActive(kShaderVertex) )
		return;
	DeviceStateD3D& state = device.GetState();
	IDirect3DDevice9* dev = GetD3DDevice();

	const int maxTexCoords = gGraphicsCaps.maxTexCoords; // fetch here once

	VertexPipeConfig& config = device.GetVertexPipeConfig();
	UInt32 textureSources = config.textureSources;
	for( int i = 0; i < maxTexCoords; ++i )
	{
		UInt32 source = (textureSources >> (i*3)) & 0x7;
		if( source > kTexSourceUV1 )
			continue;
		ShaderChannel texCoordChannel = channels.GetSourceForTarget( (VertexComponent)(kVertexCompTexCoord0 + i) );
		if( texCoordChannel == kShaderChannelTexCoord0 )
			textureSources = textureSources & ~(7<<i*3) | (kTexSourceUV0<<i*3);
		else if( texCoordChannel == kShaderChannelTexCoord1 )
			textureSources = textureSources & ~(7<<i*3) | (kTexSourceUV1<<i*3);
		else if( texCoordChannel != kShaderChannelNone ) {
			AssertString( "Bad texcoord index" );
		}
	}
	config.textureSources = textureSources;

	config.hasVertexColor = (channels.GetTargetMap() & (1<<kVertexCompColor)) ? 1 : 0;
}


struct SetValuesFunctorD3D9
{
	SetValuesFunctorD3D9(GfxDevice& device, VertexShaderConstantCache& vs, PixelShaderConstantCache& ps) : m_Device(device), vscache(vs), pscache(ps) { }
	GfxDevice& m_Device;
	VertexShaderConstantCache& vscache;
	PixelShaderConstantCache& pscache;
	void SetVectorVal (ShaderType shaderType, ShaderParamType type, int index, const float* ptr, int cols, const GpuProgramParameters& params, int cbIndex)
	{
		if (shaderType == kShaderVertex)
			vscache.SetValues(index, ptr, 1);
		else
			pscache.SetValues(index, ptr, 1);
	}
	void SetMatrixVal (ShaderType shaderType, int index, const Matrix4x4f* ptr, int rows, const GpuProgramParameters& params, int cbIndex)
	{
		DebugAssert(rows == 4);
		Matrix4x4f mat;
		TransposeMatrix4x4 (ptr, &mat);
		if (shaderType == kShaderVertex)
			vscache.SetValues(index, mat.GetPtr(), 4);
		else
			pscache.SetValues(index, mat.GetPtr(), 4);
	}
	void SetTextureVal (ShaderType shaderType, int index, int samplerIndex, TextureDimension dim, TextureID texID)
	{
		m_Device.SetTexture (shaderType, index, samplerIndex, texID, dim, std::numeric_limits<float>::infinity());
	}
};


// Compute/Update any deferred state before each draw call
void GfxDeviceD3D9::BeforeDrawCall( bool immediateMode )
{
	VertexShaderConstantCache& vscache = GetVertexShaderConstantCache();
	PixelShaderConstantCache& pscache = GetPixelShaderConstantCache();
	DeviceStateD3D& state = m_State;
	IDirect3DDevice9* dev = GetD3DDevice();
	bool usesVertexShader = (state.activeShader[kShaderVertex] != NULL);

	//@TODO: remove TESTING CODE
	static bool oldTnL = false;
	if( oldTnL != (!immediateMode) )
	{
		m_VertexPrevious.config.Reset ();
		m_VertexPrevious.ambient.set(-1,-1,-1,-1);
		oldTnL = !immediateMode;
	}

	m_TransformState.UpdateWorldViewMatrix (m_BuiltinParamValues);

	// Deferred setup of fixed function stuff
	if (!immediateMode)
		SetupVertexShaderD3D9( dev, m_TransformState, m_BuiltinParamValues, m_VertexConfig, m_VertexData, m_VertexPrevious, vscache, usesVertexShader, immediateMode );
	else
		SetupFixedFunctionD3D9( dev, m_TransformState, m_BuiltinParamValues, m_VertexConfig, m_VertexData, m_VertexPrevious, usesVertexShader, immediateMode );


	// update GL equivalents of built-in shader state

	const BuiltinShaderParamIndices& paramsVS = *m_BuiltinParamIndices[kShaderVertex];
	const BuiltinShaderParamIndices& paramsPS = *m_BuiltinParamIndices[kShaderFragment];
	int gpuIndexVS, gpuIndexPS;

#define SET_BUILTIN_MATRIX_BEGIN(idx) \
	gpuIndexVS = paramsVS.mat[idx].gpuIndex; gpuIndexPS = paramsPS.mat[idx].gpuIndex; if (gpuIndexVS >= 0 || gpuIndexPS >= 0)

#define SET_BUILTIN_MATRIX_END(name) \
	if (gpuIndexVS >= 0) vscache.SetValues(gpuIndexVS, name.GetPtr(), 4); \
	if (gpuIndexPS >= 0) pscache.SetValues(gpuIndexPS, name.GetPtr(), 4)

	// MVP matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatMVP)
	{
		Matrix4x4f matMul;
		MultiplyMatrices4x4 (&m_BuiltinParamValues.GetMatrixParam(kShaderMatProj), &m_TransformState.worldViewMatrix, &matMul);
		Matrix4x4f mat;
		TransposeMatrix4x4 (&matMul, &mat);
		SET_BUILTIN_MATRIX_END(mat);
	}
	// MV matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatMV)
	{
		Matrix4x4f mat;
		TransposeMatrix4x4 (&m_TransformState.worldViewMatrix, &mat);
		SET_BUILTIN_MATRIX_END(mat);
	}
	// Transpose MV matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatTransMV)
	{
		const Matrix4x4f& mat = m_TransformState.worldViewMatrix;
		SET_BUILTIN_MATRIX_END(mat);
	}
	// Inverse transpose of MV matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatInvTransMV)
	{
		Matrix4x4f mat;
		Matrix4x4f::Invert_Full (m_TransformState.worldViewMatrix, mat);
		if (m_VertexData.normalization == kNormalizationScale)
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
		SET_BUILTIN_MATRIX_END(mat);
	}
	// M matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatM)
	{
		Matrix4x4f mat;
		TransposeMatrix4x4 (&m_TransformState.worldMatrix, &mat);
		SET_BUILTIN_MATRIX_END(mat);
	}
	// Inverse M matrix
	SET_BUILTIN_MATRIX_BEGIN(kShaderInstanceMatInvM)
	{
		Matrix4x4f mat = m_TransformState.worldMatrix;
		if (m_VertexData.normalization == kNormalizationScale)
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
		TransposeMatrix4x4 (&inverseMat, &mat);
		SET_BUILTIN_MATRIX_END(mat);
	}

	// Set instance vector parameters
	for (int i = 0; i < kShaderInstanceVecCount; ++i)
	{
		gpuIndexVS = paramsVS.vec[i].gpuIndex;
		if (gpuIndexVS >= 0)
			vscache.SetValues(gpuIndexVS, m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i).GetPtr(), 1);
		gpuIndexPS = paramsPS.vec[i].gpuIndex;
		if (gpuIndexPS >= 0)
			pscache.SetValues(gpuIndexPS, m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i).GetPtr(), 1);
	}

	// Texture matrices for vertex shader
	for( int i = 0; i < 8; ++i )
	{
		if( paramsVS.mat[kShaderInstanceMatTexture0 + i].gpuIndex >= 0 )
		{
			Matrix4x4f mat;
			TransposeMatrix4x4 (&m_TransformState.texMatrices[i], &mat);
			const int index = paramsVS.mat[kShaderInstanceMatTexture0 + i].gpuIndex;
			vscache.SetValues( index, mat.GetPtr(), 4 );
		}
	}

	// Software VP flags
	if( g_D3DUsesMixedVP )
	{
		if( state.m_NeedsSofwareVPFlags )
		{
			if( state.m_SoftwareVP == false )
			{
				D3D9_CALL(dev->SetSoftwareVertexProcessing( TRUE ));
				state.m_SoftwareVP = true;
			}
		}
		else
		{
			if( state.m_SoftwareVP == true )
			{
				D3D9_CALL(dev->SetSoftwareVertexProcessing( FALSE ));
				state.m_SoftwareVP = false;
			}
		}
	}

	SetValuesFunctorD3D9 setValuesFunc(*this, vscache, pscache);
	ApplyMaterialPropertyBlockValues(m_MaterialProperties, m_State.activeGpuProgram ,m_State.activeGpuProgramParams, setValuesFunc);

	vscache.CommitVertexConstants();
	pscache.CommitPixelConstants();
}


DeviceBlendState* GfxDeviceD3D9::CreateBlendState(const GfxBlendState& state)
{
	std::pair<CachedBlendStates::iterator, bool> result = m_CachedBlendStates.insert(std::make_pair(state, DeviceBlendStateD3D9()));
	if (!result.second)
		return &result.first->second;

	DeviceBlendStateD3D9& d3dstate = result.first->second;
	memcpy(&d3dstate.sourceState, &state, sizeof(GfxBlendState));
	DWORD d3dmask = 0;
	const UInt8 mask = state.renderTargetWriteMask;
	if( mask & kColorWriteR ) d3dmask |= D3DCOLORWRITEENABLE_RED;
	if( mask & kColorWriteG ) d3dmask |= D3DCOLORWRITEENABLE_GREEN;
	if( mask & kColorWriteB ) d3dmask |= D3DCOLORWRITEENABLE_BLUE;
	if( mask & kColorWriteA ) d3dmask |= D3DCOLORWRITEENABLE_ALPHA;
	d3dstate.renderTargetWriteMask = d3dmask;

	DebugAssertIf(kFuncUnknown==state.alphaTest);
	d3dstate.alphaFunc = kCmpFuncD3D9[state.alphaTest];
	return &result.first->second;
}


DeviceDepthState* GfxDeviceD3D9::CreateDepthState(const GfxDepthState& state)
{
	std::pair<CachedDepthStates::iterator, bool> result = m_CachedDepthStates.insert(std::make_pair(state, DeviceDepthStateD3D9()));
	if (!result.second)
		return &result.first->second;

	DeviceDepthStateD3D9& d3dstate = result.first->second;
	memcpy(&d3dstate.sourceState, &state, sizeof(GfxDepthState));
	d3dstate.depthFunc = kCmpFuncD3D9[state.depthFunc];
	return &result.first->second;
}

DeviceStencilState* GfxDeviceD3D9::CreateStencilState(const GfxStencilState& state)
{
	std::pair<CachedStencilStates::iterator, bool> result = m_CachedStencilStates.insert(std::make_pair(state, DeviceStencilStateD3D9()));
	if (!result.second)
		return &result.first->second;

	DeviceStencilStateD3D9& st = result.first->second;
	memcpy(&st.sourceState, &state, sizeof(state));
	st.stencilFuncFront = kCmpFuncD3D9[state.stencilFuncFront];
	st.stencilFailOpFront = kStencilOpD3D9[state.stencilFailOpFront];
	st.depthFailOpFront = kStencilOpD3D9[state.stencilZFailOpFront];
	st.depthPassOpFront = kStencilOpD3D9[state.stencilPassOpFront];
	st.stencilFuncBack = kCmpFuncD3D9[state.stencilFuncBack];
	st.stencilFailOpBack = kStencilOpD3D9[state.stencilFailOpBack];
	st.depthFailOpBack = kStencilOpD3D9[state.stencilZFailOpBack];
	st.depthPassOpBack = kStencilOpD3D9[state.stencilPassOpBack];
	return &result.first->second;
}



DeviceRasterState* GfxDeviceD3D9::CreateRasterState(const GfxRasterState& state)
{
	std::pair<CachedRasterStates::iterator, bool> result = m_CachedRasterStates.insert(std::make_pair(state, DeviceRasterState()));
	if (!result.second)
		return &result.first->second;

	DeviceRasterState& d3dstate = result.first->second;
	memcpy(&d3dstate.sourceState, &state, sizeof(DeviceRasterState));

	return &result.first->second;
}


void GfxDeviceD3D9::SetBlendState(const DeviceBlendState* state, float alphaRef)
{
	DeviceBlendStateD3D9* devstate = (DeviceBlendStateD3D9*)state;

	if (m_CurrBlendState == devstate && alphaRef == m_State.alphaValue)
		return;

	m_CurrBlendState = devstate;
	if (!m_CurrBlendState)
		return;

	UInt32 colMask = devstate->renderTargetWriteMask;
	if (!IsActiveRenderTargetWithColorD3D9())
		colMask = 0;

	if(colMask != m_State.colorWriteMask)
	{
		IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
		D3D9_CALL(dev->SetRenderState(D3DRS_COLORWRITEENABLE, colMask));
		m_State.colorWriteMask = colMask;
	}

	const GfxBlendState& desc = state->sourceState;
	const CompareFunction mode = state->sourceState.alphaTest;
	const D3DBLEND d3dsrc = kBlendModeD3D9[desc.srcBlend];
	const D3DBLEND d3ddst = kBlendModeD3D9[desc.dstBlend];
	const D3DBLEND d3dsrca = kBlendModeD3D9[desc.srcBlendAlpha];
	const D3DBLEND d3ddsta = kBlendModeD3D9[desc.dstBlendAlpha];
	const D3DBLENDOP d3dop = kBlendOpD3D9[desc.blendOp];
	const D3DBLENDOP d3dopa = kBlendOpD3D9[desc.blendOpAlpha];

	const bool blendDisabled = (d3dsrc == D3DBLEND_ONE && d3ddst == D3DBLEND_ZERO && d3dsrca == D3DBLEND_ONE && d3ddsta == D3DBLEND_ZERO);

	IDirect3DDevice9* dev = GetD3DDevice();
	if(blendDisabled)
	{
		if( m_State.blending != 0 )
		{
			D3D9_CALL(dev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ));
			m_State.blending = 0;
		}
	}
	else
	{
		if( d3dsrc != m_State.srcBlend || d3ddst != m_State.destBlend )
		{
			D3D9_CALL(dev->SetRenderState( D3DRS_SRCBLEND, d3dsrc ));
			D3D9_CALL(dev->SetRenderState( D3DRS_DESTBLEND, d3ddst ));
			m_State.srcBlend = d3dsrc;
			m_State.destBlend = d3ddst;
		}

		if (d3dop != m_State.blendOp)
		{
			bool supports = true;
			if( (d3dop == D3DBLENDOP_SUBTRACT || d3dop == D3DBLENDOP_REVSUBTRACT) && !gGraphicsCaps.hasBlendSub )
				supports = false;
			if( (d3dop == D3DBLENDOP_MIN || d3dop == D3DBLENDOP_MAX) && !gGraphicsCaps.hasBlendMinMax )
				supports = false;

			if(supports)
			{
				D3D9_CALL(dev->SetRenderState(D3DRS_BLENDOP, d3dop));
				m_State.blendOp = d3dop;
			}
		}
		if (gGraphicsCaps.hasSeparateAlphaBlend)
		{
			if( d3dsrca != m_State.srcBlendAlpha || d3ddsta != m_State.destBlendAlpha || d3dopa != m_State.blendOpAlpha )
			{
				D3D9_CALL(dev->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, d3dsrc != d3dsrca || d3ddst != d3ddsta || d3dopa != d3dop));
				D3D9_CALL(dev->SetRenderState( D3DRS_SRCBLENDALPHA, d3dsrca ));
				D3D9_CALL(dev->SetRenderState( D3DRS_DESTBLENDALPHA, d3ddsta ));
				m_State.srcBlendAlpha = d3dsrca;
				m_State.destBlendAlpha = d3ddsta;

				bool supports = true;
				if( (d3dopa == D3DBLENDOP_SUBTRACT || d3dopa == D3DBLENDOP_REVSUBTRACT) && !gGraphicsCaps.hasBlendSub )
					supports = false;
				if( (d3dopa == D3DBLENDOP_MIN || d3dopa == D3DBLENDOP_MAX) && !gGraphicsCaps.hasBlendMinMax )
					supports = false;

				if (supports)
				{
					D3D9_CALL(dev->SetRenderState(D3DRS_BLENDOPALPHA, d3dopa));
					m_State.blendOpAlpha = d3dopa;
				}
			}
		}
		if( m_State.blending != 1 )
		{
			D3D9_CALL(dev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ));
			m_State.blending = 1;
		}
	}

	DebugAssertIf(mode==kFuncUnknown);
#if UNITY_EDITOR // gles2.0 doesn't have FF alpha testing(only discard/clip on shader side), so disable on editor while emulating
	bool skipAlphaTestFF = (gGraphicsCaps.IsEmulatingGLES20() && IsShaderActive(kShaderFragment));
	// possible that vertex shader will be used with FF "frag shader" (like Transparent/vertexlit.shader),
	// which will change alphatesting. So later on when real frag shaders come, we need to force disable alpha
	// testing or enjoy nasty artefacts (like active alpha testing messing up the whole scene).
	if ( skipAlphaTestFF && m_State.alphaFunc!=kFuncDisabled )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE ));
		m_State.alphaFunc = kFuncDisabled;
	}

	if ( !skipAlphaTestFF )
	{
#endif
		if( mode != m_State.alphaFunc || alphaRef != m_State.alphaValue )
		{
			if( mode != kFuncDisabled )
			{
				D3D9_CALL(dev->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE ));
				D3D9_CALL(dev->SetRenderState( D3DRS_ALPHAFUNC, kCmpFuncD3D9[mode] ));
				D3D9_CALL(dev->SetRenderState( D3DRS_ALPHAREF, alphaRef * 255.0f ));
			}
			else
			{
				D3D9_CALL(dev->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE ));
			}

			m_State.alphaFunc = mode;
			m_State.alphaValue = alphaRef;
		}
#if UNITY_EDITOR
	}
#endif
	// TODO: ATI/NVIDIA hacks
}


void GfxDeviceD3D9::SetRasterState(const DeviceRasterState* state)
{
	DeviceRasterState* devstate = (DeviceRasterState*)state;
	if(!devstate)
	{
		m_CurrRasterState = NULL;
		return;
	}

	m_CurrRasterState = devstate;

	IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
	CullMode cull = devstate->sourceState.cullMode;
	D3DCULL d3dcull = kCullModeD3D9[cull];
	if( d3dcull != m_State.d3dculling )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_CULLMODE, d3dcull ));
		m_State.culling = cull;
		m_State.d3dculling = d3dcull;
	}

	float zFactor = devstate->sourceState.slopeScaledDepthBias;
	float zUnits  = devstate->sourceState.depthBias;
	if( zFactor != m_State.offsetFactor || zUnits != m_State.offsetUnits )
	{
		m_State.offsetFactor = zFactor;
		m_State.offsetUnits = zUnits;

		// In D3D9 the values are in floating point, with 1 meaning "full depth range".
		// In theory the offset should depend on depth buffer bit count, and on 24 bit depth buffer a value close to 4.8e-7 should be used
		// (see Lengyel's GDC2007 "projection matrix tricks").
		// However, it looks like even on 16 bit depth buffer, a value as-if-24-bit should be used (tested on Radeon HD 3850, GeForce 8600, Intel 945).
		const double kOneBit = 4.8e-7;

		// It looks like generally we need twice the one bit (PolygonOff2 unit test, on Radeon 3850 and GeForce 8600).
		// To be somewhat more safer, we make it trhee times the one bit. Still looks quite okay.
		const float kBiasMultiplier = 3.0 * kOneBit;

		if( gGraphicsCaps.d3d.d3dcaps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS )
		{
			zUnits *= kBiasMultiplier;
			D3D9_CALL(dev->SetRenderState( D3DRS_DEPTHBIAS, *(DWORD*)&zUnits ));
		}
		if( gGraphicsCaps.d3d.d3dcaps.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS )
		{
			D3D9_CALL(dev->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD*)&zFactor ));
		}
	}
}


void GfxDeviceD3D9::SetDepthState(const DeviceDepthState* state)
{
	IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
	DeviceDepthStateD3D9* devstate = (DeviceDepthStateD3D9*)state;
	if (m_CurrDepthState == devstate)
		return;

	m_CurrDepthState = devstate;

	if (!m_CurrDepthState)
		return;

	if( devstate->sourceState.depthFunc != m_State.depthFunc )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_ZFUNC, devstate->depthFunc ));
		m_State.depthFunc = devstate->sourceState.depthFunc;
	}

	int d3dDepthWriteMode = devstate->sourceState.depthWrite ? TRUE : FALSE;
	if( d3dDepthWriteMode != m_State.depthWrite )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_ZWRITEENABLE, d3dDepthWriteMode ));
		m_State.depthWrite = d3dDepthWriteMode;
	}
}

void GfxDeviceD3D9::SetStencilState(const DeviceStencilState* state, int stencilRef)
{
	if (m_CurrStencilState == state && m_State.m_StencilRef == stencilRef)
		return;
	const DeviceStencilStateD3D9* st = static_cast<const DeviceStencilStateD3D9*>(state);
	m_CurrStencilState = st;
	if (!m_CurrStencilState)
		return;

	IDirect3DDevice9* dev = GetD3DDevice();
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILENABLE, st->sourceState.stencilEnable));
	D3D9_CALL (dev->SetRenderState (D3DRS_TWOSIDEDSTENCILMODE, TRUE));
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILMASK, st->sourceState.readMask));
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILWRITEMASK, st->sourceState.writeMask));
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILREF, stencilRef));

	m_State.stencilFunc[0] = st->stencilFuncFront;
	m_State.stencilFailOp[0] = st->stencilFailOpFront;
	m_State.depthFailOp[0] = st->depthFailOpFront;
	m_State.depthPassOp[0] = st->depthPassOpFront;
	m_State.stencilFunc[1] = st->stencilFuncBack;
	m_State.stencilFailOp[1] = st->stencilFailOpBack;
	m_State.depthFailOp[1] = st->depthFailOpBack;
	m_State.depthPassOp[1] = st->depthPassOpBack;
	ApplyStencilFuncAndOp(m_State);

	m_State.m_StencilRef = stencilRef;
}

static void ApplyStencilFuncAndOp (DeviceStateD3D& state)
{
	IDirect3DDevice9* dev = GetD3DDevice();
	// Normally [0] is front and [1] back stencil state, but when rendering
	// upside-down, the winding order flips, so flip the state as well.
	const int cw = state.invertProjMatrix ? 1 : 0;
	const int ccw = (cw + 1)%2;
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILFUNC, state.stencilFunc[cw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILFAIL, state.stencilFailOp[cw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILZFAIL, state.depthFailOp[cw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_STENCILPASS, state.depthPassOp[cw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_CCW_STENCILFUNC, state.stencilFunc[ccw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_CCW_STENCILFAIL, state.stencilFailOp[ccw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_CCW_STENCILZFAIL, state.depthFailOp[ccw]));
	D3D9_CALL (dev->SetRenderState (D3DRS_CCW_STENCILPASS, state.depthPassOp[ccw]));
}

void GfxDeviceD3D9::SetSRGBWrite (bool enable)
{
	IDirect3DDevice9* dev = GetD3DDevice();
	D3D9_CALL (dev->SetRenderState (D3DRS_SRGBWRITEENABLE, enable));
}

bool GfxDeviceD3D9::GetSRGBWrite ()
{
	IDirect3DDevice9* dev = GetD3DDevice();
	DWORD v;
	D3D9_CALL (dev->GetRenderState (D3DRS_SRGBWRITEENABLE, &v));
	return (v==TRUE);
}

GfxThreadableDevice* CreateD3D9GfxDevice(bool forceREF)
{
	if( !InitializeD3D(forceREF ? D3DDEVTYPE_REF : D3DDEVTYPE_HAL) )
		return NULL;

	#if UNITY_EDITOR
	if (!CreateHiddenWindowD3D())
		return NULL;
	#endif

	gGraphicsCaps.InitD3D9();

	GfxDeviceD3D9* device = UNITY_NEW_AS_ROOT(GfxDeviceD3D9(), kMemGfxDevice, "D3D9GfxDevice", "");

#if UNITY_EDITOR
	EditorInitializeD3D(device);
#else
	ScreenManagerWin& screenMgr = GetScreenManager();
	HWND window = screenMgr.GetWindow();
	int width = screenMgr.GetWidth();
	int height = screenMgr.GetHeight();
	int dummy;
	if (!InitializeOrResetD3DDevice(device, window, width, height, 0, false, 0, 0, dummy, dummy, dummy, dummy))
	{
		UNITY_DELETE(device, kMemGfxDevice);
		device = NULL;
	}
#endif

	return device;
}

GfxDeviceD3D9& GetD3D9GfxDevice()
{
	GfxDevice& device = GetRealGfxDevice();
	Assert( device.GetRenderer() == kGfxRendererD3D9 );
	return static_cast<GfxDeviceD3D9&>(device);
}

bool IsD3D9DeviceLost()
{
	GfxDeviceD3D9& device = static_cast<GfxDeviceD3D9&>( GetRealGfxDevice() );
	AssertIf( device.GetRenderer() != kGfxRendererD3D9 );
	return device.GetState().m_DeviceLost;
}

void SetD3D9DeviceLost( bool lost )
{
	GfxDeviceD3D9& device = static_cast<GfxDeviceD3D9&>( GetRealGfxDevice() );
	AssertIf( device.GetRenderer() != kGfxRendererD3D9 );
	device.GetState().m_DeviceLost = lost;
}


GfxDeviceD3D9::GfxDeviceD3D9()
{
	m_State.m_DeviceLost = false;
	m_DynamicVBO = NULL;

	m_State.appBackfaceMode = false;
	m_State.userBackfaceMode = false;
	m_State.invertProjMatrix = false;
	m_State.wireframe = false;

	InvalidateState();
	ResetFrameStats();

	m_Renderer = kGfxRendererD3D9;
	m_UsesOpenGLTextureCoords = false;
	m_UsesHalfTexelOffset = true;
	m_IsThreadable = true;

	m_MaxBufferedFrames = 1; // -1 means no limiting, default is 1

	m_State.viewport[0] = m_State.viewport[1] = m_State.viewport[2] = m_State.viewport[3] = 0;
	m_State.scissorRect[0] = m_State.scissorRect[1] = m_State.scissorRect[2] = m_State.scissorRect[3] = 0;

	m_CurrBlendState = 0;
	m_CurrDepthState = 0;
	m_CurrStencilState = 0;
	m_CurrRasterState = 0;
	m_CurrTargetWidth = 0;
	m_CurrTargetHeight = 0;
	m_CurrWindowWidth = 0;
	m_CurrWindowHeight = 0;

	m_AllWhiteVertexStream = NULL;

	extern RenderSurfaceBase* DummyColorBackBuferD3D9();
	SetBackBufferColorSurface(DummyColorBackBuferD3D9());

	extern RenderSurfaceBase* DummyDepthBackBuferD3D9();
	SetBackBufferDepthSurface(DummyDepthBackBuferD3D9());
}

GfxDeviceD3D9::~GfxDeviceD3D9()
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER
	PluginsSetGraphicsDevice (GetD3DDevice(), kGfxRendererD3D9, kGfxDeviceEventShutdown);
#endif

	D3D9VBO::CleanupSharedIndexBuffer();

	CleanupEventQueries ();
#if ENABLE_PROFILER
	m_TimerQueriesD3D9.ReleaseAllQueries();
#endif

	if( m_DynamicVBO )
		delete m_DynamicVBO;

	SAFE_RELEASE(m_AllWhiteVertexStream);
	SAFE_RELEASE(m_Imm.m_ImmVertexDecl);
	m_VertexDecls.Clear();
	TextureCombinersD3D::CleanupCombinerCache();
	CleanupVertexShadersD3D9 ();
	DestroyD3DDevice();

	#if UNITY_EDITOR
	DestroyHiddenWindowD3D();
	#endif

	CleanupD3D();
}

void GfxDeviceD3D9::InvalidateState()
{
	IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
	if( m_State.m_DeviceLost )
		dev = NULL;

	ResetVertexPipeStateD3D9 (dev, m_TransformState, m_BuiltinParamValues, m_VertexConfig, m_VertexData, m_VertexPrevious);
	m_FogParams.Invalidate();
	m_State.Invalidate(*this);
	m_Imm.Invalidate();
	m_VSConstantCache.Invalidate();
	m_PSConstantCache.Invalidate();

	m_CurrBlendState = NULL;
	m_CurrDepthState = NULL;
	m_CurrStencilState = NULL;
	m_CurrRasterState = NULL;
}


void GfxDeviceD3D9::Clear(UInt32 clearFlags, const float color[4], float depth, int stencil)
{
	if( !g_D3DHasDepthStencil )
		clearFlags &= ~kGfxClearDepthStencil;
	if (!IsActiveRenderTargetWithColorD3D9())
		clearFlags &= ~kGfxClearColor;

	DWORD flags = 0;
	if (clearFlags & kGfxClearColor) flags |= D3DCLEAR_TARGET;
	if (clearFlags & kGfxClearDepth) flags |= D3DCLEAR_ZBUFFER;
	if (clearFlags & kGfxClearStencil && GetStencilBitsFromD3DFormat (g_D3DDepthStencilFormat) > 0) {
		flags |= D3DCLEAR_STENCIL;
	}
	GetD3DDevice()->Clear (0, NULL, flags, ColorToD3D(color), depth, stencil);
}


static void ApplyBackfaceMode( DeviceStateD3D& state )
{
	if( (state.appBackfaceMode == state.userBackfaceMode) == state.invertProjMatrix )
	{
		kCullModeD3D9[kCullFront] = D3DCULL_CCW;
		kCullModeD3D9[kCullBack] = D3DCULL_CW;
	}
	else
	{
		kCullModeD3D9[kCullFront] = D3DCULL_CW;
		kCullModeD3D9[kCullBack] = D3DCULL_CCW;
	}

	if( state.culling != kCullUnknown )
	{
		IDirect3DDevice9* dev = GetD3DDevice();
		D3DCULL d3dcull = kCullModeD3D9[state.culling];
		if( d3dcull != state.d3dculling )
		{
			D3D9_CALL(dev->SetRenderState( D3DRS_CULLMODE, d3dcull ));
			state.d3dculling = d3dcull;
		}
	}
}

void GfxDeviceD3D9::SetUserBackfaceMode( bool enable )
{
	if( m_State.userBackfaceMode == enable )
		return;
	m_State.userBackfaceMode = enable;
	ApplyBackfaceMode( m_State );
}


void GfxDeviceD3D9::SetWireframe( bool wire )
{
	IDirect3DDevice9* dev = GetD3DDevice();
	D3D9_CALL(dev->SetRenderState( D3DRS_FILLMODE, wire ? D3DFILL_WIREFRAME : D3DFILL_SOLID ));
	m_State.wireframe = wire;
}

bool GfxDeviceD3D9::GetWireframe() const
{
	return m_State.wireframe;
}



// Even with programmable shaders, some things need fixed function D3DTS_PROJECTION to be set up;
// most notably fixed function fog (shader model 2.0).
static void SetFFProjectionMatrixD3D9 (const Matrix4x4f& m)
{
	IDirect3DDevice9* dev = GetD3DDevice();
	Matrix4x4f projFlip;
	projFlip.m_Data[ 0] =  m.m_Data[ 0];
	projFlip.m_Data[ 1] =  m.m_Data[ 1];
	projFlip.m_Data[ 2] =  m.m_Data[ 2];
	projFlip.m_Data[ 3] =  m.m_Data[ 3];
	projFlip.m_Data[ 4] =  m.m_Data[ 4];
	projFlip.m_Data[ 5] =  m.m_Data[ 5];
	projFlip.m_Data[ 6] =  m.m_Data[ 6];
	projFlip.m_Data[ 7] =  m.m_Data[ 7];
	projFlip.m_Data[ 8] = -m.m_Data[ 8];
	projFlip.m_Data[ 9] = -m.m_Data[ 9];
	projFlip.m_Data[10] = -m.m_Data[10];
	projFlip.m_Data[11] = -m.m_Data[11];
	projFlip.m_Data[12] =  m.m_Data[12];
	projFlip.m_Data[13] =  m.m_Data[13];
	projFlip.m_Data[14] =  m.m_Data[14];
	projFlip.m_Data[15] =  m.m_Data[15];
	D3D9_CALL(dev->SetTransform (D3DTS_PROJECTION, (const D3DMATRIX*)projFlip.GetPtr()));
}


void GfxDeviceD3D9::SetInvertProjectionMatrix( bool enable )
{
	if( m_State.invertProjMatrix == enable )
		return;

	m_State.invertProjMatrix = enable;
	ApplyBackfaceMode( m_State );
	ApplyStencilFuncAndOp( m_State );

	// When setting up "invert" flag, invert the matrix as well.
	Matrix4x4f& m = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj);
	m.Get(1,1) = -m.Get(1,1);
	m.Get(1,3) = -m.Get(1,3);
	m_TransformState.dirtyFlags |= TransformState::kProjDirty;
	SetFFProjectionMatrixD3D9 (m);
}

bool GfxDeviceD3D9::GetInvertProjectionMatrix() const
{
	return m_State.invertProjMatrix;
}

void GfxDeviceD3D9::SetWorldMatrix( const float matrix[16] )
{
	CopyMatrix (matrix, m_TransformState.worldMatrix.GetPtr());
	m_TransformState.dirtyFlags |= TransformState::kWorldDirty;
}

void GfxDeviceD3D9::SetViewMatrix( const float matrix[16] )
{
	m_TransformState.SetViewMatrix (matrix, m_BuiltinParamValues);
}

void GfxDeviceD3D9::SetProjectionMatrix(const Matrix4x4f& matrix)
{
	Matrix4x4f& m = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj);
	CopyMatrix (matrix.GetPtr(), m.GetPtr());
	CopyMatrix (matrix.GetPtr(), m_TransformState.projectionMatrixOriginal.GetPtr());

	CalculateDeviceProjectionMatrix (m, m_UsesOpenGLTextureCoords, m_State.invertProjMatrix);
	SetFFProjectionMatrixD3D9 (m);

	m_TransformState.dirtyFlags |= TransformState::kProjDirty;
}


void GfxDeviceD3D9::GetMatrix(float outMatrix[16]) const
{
	m_TransformState.UpdateWorldViewMatrix (m_BuiltinParamValues);
	CopyMatrix (m_TransformState.worldViewMatrix.GetPtr(), outMatrix);
}

const float* GfxDeviceD3D9::GetWorldMatrix() const
{
	return m_TransformState.worldMatrix.GetPtr();
}

const float* GfxDeviceD3D9::GetViewMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatView).GetPtr();
}

const float* GfxDeviceD3D9::GetProjectionMatrix() const
{
	return m_TransformState.projectionMatrixOriginal.GetPtr();
}

const float* GfxDeviceD3D9::GetDeviceProjectionMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatProj).GetPtr();
}

void GfxDeviceD3D9::SetNormalizationBackface( NormalizationMode mode, bool backface )
{
	IDirect3DDevice9* dev = GetD3DDevice();
	if( mode != m_VertexData.normalization )
	{
		m_VertexData.normalization = mode;
		m_VertexConfig.hasNormalization = (mode == kNormalizationFull);
	}
	if( m_State.appBackfaceMode != backface )
	{
		m_State.appBackfaceMode = backface;
		ApplyBackfaceMode( m_State );
	}
}

void GfxDeviceD3D9::SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial )
{
	m_VertexConfig.hasLighting = on ? 1 : 0;
	m_VertexConfig.hasSpecular = separateSpecular ? 1 : 0;
	DebugAssertIf(colorMaterial==kColorMatUnknown);
	m_VertexConfig.colorMaterial = colorMaterial;
}

void GfxDeviceD3D9::SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess )
{
	D3DMATERIAL9& mat = m_VertexData.material;
	mat.Ambient = *(D3DCOLORVALUE*)ambient;
	mat.Diffuse = *(D3DCOLORVALUE*)diffuse;
	mat.Specular = *(D3DCOLORVALUE*)specular;
	mat.Emissive = *(D3DCOLORVALUE*)emissive;
	mat.Power = std::max<float>( std::min<float>(shininess,1.0f), 0.0f) * 128.0f;
}


void GfxDeviceD3D9::SetColor( const float color[4] )
{
	// If we have pixel shader set up, do nothing; fixed function
	// constant color can't be possibly used there
	if (m_State.activeShader[kShaderFragment] != 0) // inlined IsShaderActive(kShaderFragment)
		return;

	// There's no really good place to make a glColor equivalent, put it into
	// TFACTOR... Additionally put that into c4 register for ps_1_1 combiner emulation
	IDirect3DDevice9* dev = GetD3DDevice();
	D3D9_CALL(dev->SetRenderState( D3DRS_TEXTUREFACTOR, ColorToD3D(color) ));
	m_PSConstantCache.SetValues( kMaxD3DTextureStagesForPS, color, 1 );
}


void GfxDeviceD3D9::SetViewport( int x, int y, int width, int height )
{
	m_State.viewport[0] = x;
	m_State.viewport[1] = y;
	m_State.viewport[2] = width;
	m_State.viewport[3] = height;

	IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
	if( !dev ) // happens on startup, when deleting all render textures
		return;
	D3DVIEWPORT9 view;
	view.X = x;
	view.Y = y;
	view.Width = width;
	view.Height = height;
	view.MinZ = 0.0f;
	view.MaxZ = 1.0f;
	dev->SetViewport( &view );
}

void GfxDeviceD3D9::GetViewport( int* port ) const
{
	port[0] = m_State.viewport[0];
	port[1] = m_State.viewport[1];
	port[2] = m_State.viewport[2];
	port[3] = m_State.viewport[3];
}


void GfxDeviceD3D9::SetScissorRect( int x, int y, int width, int height )
{
	if (m_State.scissor != 1)
	{
		if (gGraphicsCaps.d3d.d3dcaps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST )
		{
			GetD3DDevice()->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
		}
		m_State.scissor = 1;
	}


	m_State.scissorRect[0] = x;
	m_State.scissorRect[1] = y;
	m_State.scissorRect[2] = width;
	m_State.scissorRect[3] = height;

	RECT rc;
	rc.left = x;
	rc.top = y;
	rc.right = x + width;
	rc.bottom = y + height;
	GetD3DDevice()->SetScissorRect( &rc );

}
void GfxDeviceD3D9::DisableScissor()
{
	if (m_State.scissor != 0)
	{
		if( gGraphicsCaps.d3d.d3dcaps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST )
		{
			GetD3DDevice()->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE);
		}
		m_State.scissor = 0;
	}
}
bool GfxDeviceD3D9::IsScissorEnabled() const
{
	return m_State.scissor == 1;
}

void GfxDeviceD3D9::GetScissorRect( int scissor[4] ) const
{
	scissor[0] = m_State.scissorRect[0];
	scissor[1] = m_State.scissorRect[1];
	scissor[2] = m_State.scissorRect[2];
	scissor[3] = m_State.scissorRect[3];
}

bool GfxDeviceD3D9::IsCombineModeSupported( unsigned int combiner )
{
	return true;
}

TextureCombinersHandle GfxDeviceD3D9::CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	TextureCombinersD3D* implD3D = TextureCombinersD3D::Create( count, texEnvs, props, hasVertexColorOrLighting, usesAddSpecular );
	return TextureCombinersHandle( implD3D );
}

void GfxDeviceD3D9::DeleteTextureCombiners( TextureCombinersHandle& textureCombiners )
{
	TextureCombinersD3D* implD3D = OBJECT_FROM_HANDLE(textureCombiners, TextureCombinersD3D);
	delete implD3D;
	textureCombiners.Reset();
}

void GfxDeviceD3D9::SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors )
{
	TextureCombinersD3D* implD3D = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersD3D);
	AssertIf( !implD3D );
	IDirect3DDevice9* dev = GetD3DDevice();

	AssertIf (IsShaderActive( kShaderFragment ));

	const int maxTexUnits = gGraphicsCaps.maxTexUnits; // fetch here once

	// set textures
	int i = 0;
	for( ; i < maxTexUnits && i < implD3D->envCount; ++i )
	{
		ApplyTexEnvData (i, i, texEnvData[i]);
	}

	// clear unused textures
	for (; i < maxTexUnits; ++i)
	{
		if (i < kMaxSupportedTextureCoords)
			m_VertexConfig.ClearTextureUnit(i);

		TextureUnitStateD3D& currTex = m_State.texturesPS[i];
		if (currTex.texID.m_ID != 0)
		{
			D3D9_CALL(dev->SetTexture( GetD3D9SamplerIndex(kShaderFragment,i), NULL ));
			currTex.texID.m_ID = 0;
		}
	}

	// setup texture stages
	if( implD3D->pixelShader )
	{
		for( i = 0; i < implD3D->stageCount; ++i )
		{
			const ShaderLab::TextureBinding& binding = implD3D->texEnvs[i];
			const Vector4f& texcolorVal = texColors[i];
			m_PSConstantCache.SetValues( i, texcolorVal.GetPtr(), 1 );
		}
		if( m_State.fixedFunctionPS != implD3D->uniqueID )
		{
			D3D9_CALL(dev->SetPixelShader( implD3D->pixelShader ));
			m_State.fixedFunctionPS = implD3D->uniqueID;
		}
	}
	else
	{
		if( implD3D->textureFactorIndex != -1 )
		{
			const Vector4f& color = texColors[implD3D->textureFactorIndex];
			D3D9_CALL(dev->SetRenderState( D3DRS_TEXTUREFACTOR, ColorToD3D( color.GetPtr() ) ));
		}
		for( i = 0; i < implD3D->stageCount; ++i )
		{
			// TODO: cache!
			const D3DTextureStage& stage = implD3D->stages[i];
			AssertIf( stage.colorOp == D3DTOP_DISABLE || stage.alphaOp == D3DTOP_DISABLE );
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_COLOROP, stage.colorOp ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_COLORARG1, stage.colorArgs[0] ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_COLORARG2, stage.colorArgs[1] ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_COLORARG0, stage.colorArgs[2] ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_ALPHAOP, stage.alphaOp ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_ALPHAARG1, stage.alphaArgs[0] ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_ALPHAARG2, stage.alphaArgs[1] ));
			D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_ALPHAARG0, stage.alphaArgs[2] ));
		}
		D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_COLOROP, D3DTOP_DISABLE ));
		D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_ALPHAOP, D3DTOP_DISABLE ));
		D3D9_CALL(dev->SetPixelShader( NULL ));
		m_State.fixedFunctionPS = 0;
	}
}


void GfxDeviceD3D9::SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props )
{
	TextureCombinersD3D* implD3D = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersD3D);
	AssertIf( !implD3D );

	int count = std::min(implD3D->envCount, gGraphicsCaps.maxTexUnits);

	// Fill in arrays
	TexEnvData* texEnvData;
	ALLOC_TEMP (texEnvData, TexEnvData, count);
	for( int i = 0; i < count; ++i )
	{
		ShaderLab::TexEnv *te = ShaderLab::GetTexEnvForBinding( implD3D->texEnvs[i], props );
		Assert( te != NULL );
		te->PrepareData (implD3D->texEnvs[i].m_TextureName.index, implD3D->texEnvs[i].m_MatrixName, props, &texEnvData[i]);
	}

	Vector4f* texColors;
	ALLOC_TEMP (texColors, Vector4f, implD3D->envCount);
	for( int i = 0; i < implD3D->envCount; ++i )
	{
		const ShaderLab::TextureBinding& binding = implD3D->texEnvs[i];
		texColors[i] = binding.GetTexColor().Get (props);
	}
	GfxDeviceD3D9::SetTextureCombinersThreadable(textureCombiners, texEnvData, texColors);
}


void GfxDeviceD3D9::SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias)
{
	DebugAssertIf( dim < kTexDim2D || dim > kTexDimCUBE );
	DebugAssertIf (unit < 0 || unit >= kMaxSupportedTextureUnits);

	if (unit < kMaxSupportedTextureCoords)
		m_VertexConfig.SetTextureUnit(unit);

	TextureUnitStateD3D* currTex = NULL;
	if (shaderType == kShaderFragment)
		currTex = &m_State.texturesPS[unit];
	else if (shaderType == kShaderVertex)
		currTex = &m_State.texturesVS[unit];
	else
	{
		AssertString ("Unsupported shader type for SetTexture");
		return;
	}

	if (texture != currTex->texID)
	{
		if (m_Textures.SetTexture (shaderType, unit, texture))
			currTex->texID = texture;
	}
	m_Stats.AddUsedTexture(texture);
	if (gGraphicsCaps.hasMipLevelBias && bias != currTex->bias && shaderType == kShaderFragment)
	{
		D3D9_CALL(GetD3DDevice()->SetSamplerState( unit, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&bias ));
		currTex->bias = bias;
	}
}



void GfxDeviceD3D9::SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16] )
{
	Assert (unit >= 0 && unit < kMaxSupportedTextureCoords);

	m_State.m_NeedsSofwareVPFlags &= ~kNeedsSoftwareVPTexGen;

	// -------- texture matrix

	float* mat = m_TransformState.texMatrices[unit].GetPtr();
	CopyMatrix( matrix, mat );

	// In OpenGL all texture reads are projective, and matrices are always 4x4, and z/w defaults to 0/1.
	// In D3D everything is different. So here we try to figure out how many components need to be transformed,
	// munge the matrix and enable projective texturing if needed.

	TextureMatrixMode matrixMode;
	int projectedTexture = 0;
	if( identity )
	{
		// matrix guaranteed to be identity: disable transformation
		matrixMode = kTexMatrixNone;
	}
	else if( dim == kTexDimCUBE || dim == kTexDim3D )
	{
		// for cube/volume texture: count3
		matrixMode = kTexMatrix3;
	}
	else
	{
		// detect projected matrix
		projectedTexture = (mat[3] != 0.0f || mat[7] != 0.0f || mat[11] != 0.0f || mat[15] != 1.0f) ? 1 : 0;
		// Cards that do support projected textures or cubemaps seem to want
		// Count3 flags for object/eyelinear transforms. Cards that don't support
		// projection nor cubemaps will have to use Count2 - fixes GUI text rendering!
		bool is3DTexGen = (texGen != kTexGenDisabled && texGen != kTexGenSphereMap);

		if( projectedTexture )
		{
			matrixMode = kTexMatrix4;
		}
		else if( is3DTexGen )
		{
			matrixMode = kTexMatrix3;
		}
		else
		{
			// regular texture: count2, and move matrix' 4th row into 3rd one
			matrixMode = kTexMatrix2;
			mat[ 8] = mat[12];
			mat[ 9] = mat[13];
			mat[10] = mat[14];
			mat[11] = mat[15];
		}
	}

	m_VertexConfig.textureMatrixModes = m_VertexConfig.textureMatrixModes & ~(3<<(unit*2)) | (matrixMode<<(unit*2));
	m_VertexData.projectedTextures = m_VertexData.projectedTextures & ~(1<<unit) | (projectedTexture<<unit);

	// -------- texture coordinate generation

	TextureSourceMode texSource = texGen == kTexGenDisabled ? kTexSourceUV0 : static_cast<TextureSourceMode>(texGen + 1);
	m_VertexConfig.textureSources = m_VertexConfig.textureSources & ~(7<<(unit*3)) | (texSource<<(unit*3));

	if( texGen == kTexGenSphereMap && !IsShaderActive(kShaderVertex) )
	{
		if( g_D3DUsesMixedVP && !(gGraphicsCaps.d3d.d3dcaps.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP) )
			m_State.m_NeedsSofwareVPFlags |= kNeedsSoftwareVPTexGen;
	}
}

void GfxDeviceD3D9::SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	m_Textures.SetTextureParams( texture, texDim, filter, wrap, anisoLevel, hasMipMap, colorSpace );

	// we'll need to set texture sampler states, so invalidate current texture cache
	// invalidate texture unit states that used this texture
	for (int i = 0; i < ARRAY_SIZE(m_State.texturesPS); ++i)
	{
		TextureUnitStateD3D& currTex = m_State.texturesPS[i];
		if( currTex.texID == texture )
			currTex.Invalidate();
	}
	for (int i = 0; i < ARRAY_SIZE(m_State.texturesVS); ++i)
	{
		TextureUnitStateD3D& currTex = m_State.texturesVS[i];
		if (currTex.texID == texture)
			currTex.Invalidate();
	}
}


void GfxDeviceD3D9::SetShadersThreadable( GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount])
{
	GpuProgram* vertexProgram = programs[kShaderVertex];
	GpuProgram* fragmentProgram = programs[kShaderFragment];

	IDirect3DDevice9* dev = GetD3DDevice();

	// vertex shader
	if( vertexProgram && vertexProgram->GetImplType() == kShaderImplVertex )
	{
		// set the shader
		bool resetToNoFog = false;
		IDirect3DVertexShader9* shader = static_cast<D3D9VertexShader&>(*vertexProgram).GetShader(m_FogParams.mode, resetToNoFog);
		// Note: get pixel shader to match actually used fog mode from VS. If VS was too complex
		// to patch for fog, for example, then we want PS to not have fog as well.
		if (resetToNoFog)
			m_FogParams.mode = kFogDisabled;
		DebugAssert (shader);

		if( m_State.activeShader[kShaderVertex] != shader )
		{
			D3D9_CALL(dev->SetVertexShader( shader ));
			if (m_State.activeShader[kShaderVertex] == NULL)
			{
				for( int i = 0; i < kMaxSupportedTextureCoords; ++i )
				{
					D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i ));
					D3D9_CALL(dev->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTSS_TCI_PASSTHRU ));
				}
			}

			m_VertexPrevious.vertexShader = NULL;
			m_VertexPrevious.ambient.set(-1,-1,-1,-1);

			m_State.activeShader[kShaderVertex] = shader;
		}

		if( g_D3DUsesMixedVP )
			m_State.m_NeedsSofwareVPFlags |= kNeedsSoftwareVPVertexShader;

		m_BuiltinParamIndices[kShaderVertex] = &params[kShaderVertex]->GetBuiltinParams();
	}
	else
	{
		// clear the shader
		DebugAssertIf( vertexProgram != 0 );
		if( m_State.activeShader[kShaderVertex] != 0 )
		{
			D3D9_CALL(dev->SetVertexShader( NULL ));
			m_State.activeShader[kShaderVertex] = 0;
		}

		if( g_D3DUsesMixedVP )
			m_State.m_NeedsSofwareVPFlags &= ~kNeedsSoftwareVPVertexShader;

		m_BuiltinParamIndices[kShaderVertex] = &m_NullParamIndices;
	}

	// pixel shader
	if( fragmentProgram && fragmentProgram->GetImplType() == kShaderImplFragment )
	{
		// set the shader
		IDirect3DPixelShader9* shader = static_cast<D3D9PixelShader&>(*fragmentProgram).GetShader(m_FogParams.mode, *params[kShaderFragment]);
		DebugAssert (shader);

		if( m_State.activeShader[kShaderFragment] != shader )
		{
			D3D9_CALL(dev->SetPixelShader( shader ));
			m_State.activeShader[kShaderFragment] = shader;
			m_State.fixedFunctionPS = 0;
		}

		m_BuiltinParamIndices[kShaderFragment] = &params[kShaderFragment]->GetBuiltinParams();
	}
	else
	{
		// clear the shader
		DebugAssertIf( fragmentProgram != 0 );
		if( m_State.activeShader[kShaderFragment] != 0 )
		{
			D3D9_CALL(dev->SetPixelShader( NULL ));
			m_State.activeShader[kShaderFragment] = 0;
			m_State.fixedFunctionPS = 0;
		}

		m_BuiltinParamIndices[kShaderFragment] = &m_NullParamIndices;
	}

	for (int pt = 0; pt < kShaderTypeCount; ++pt)
	{
		if (programs[pt])
		{
			m_State.activeGpuProgramParams[pt] = params[pt];
			m_State.activeGpuProgram[pt] = programs[pt];
			programs[pt]->ApplyGpuProgram (*params[pt], paramsBuffer[pt]);
		}
		else
		{
			m_State.activeGpuProgramParams[pt] = NULL;
			m_State.activeGpuProgram[pt] = NULL;
		}
	}
}


bool GfxDeviceD3D9::IsShaderActive( ShaderType type ) const
{
	return m_State.activeShader[type] != 0;
}

void GfxDeviceD3D9::DestroySubProgram( ShaderLab::SubProgram* subprogram )
{
	GpuProgram* program = &subprogram->GetGpuProgram();
	if (program->GetImplType() == kShaderImplVertex)
	{
		for (int i = 0; i < kFogModeCount; ++i)
		{
			IUnknown* shader = static_cast<D3D9VertexShader*>(program)->GetShaderAtFogIndex(static_cast<FogMode>(i));
			if (m_State.activeShader[kShaderVertex] == shader)
				m_State.activeShader[kShaderVertex] = NULL;
		}
	}
	else if (program->GetImplType() == kShaderImplFragment)
	{
		for (int i = 0; i < kFogModeCount; ++i)
		{
			IUnknown* shader = static_cast<D3D9PixelShader*>(program)->GetShaderAtFogIndex(static_cast<FogMode>(i));
			if (m_State.activeShader[kShaderFragment] == shader)
				m_State.activeShader[kShaderFragment] = NULL;
		}
	}
	delete subprogram;
}

void GfxDeviceD3D9::DisableLights( int startLight )
{
	m_VertexData.vertexLightCount = startLight;

	const Vector4f black(0.0F, 0.0F, 0.0F, 0.0F);
	for (int i = startLight; i < gGraphicsCaps.maxLights; ++i)
	{
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + i), black);
	}
}

void GfxDeviceD3D9::SetLight( int light, const GfxVertexLight& data)
{
	IDirect3DDevice9* dev = GetD3DDevice();
	DebugAssert(light >= 0 && light < kMaxSupportedVertexLights);

	DebugAssertIf( (data.position.w == 0.0f) != (data.type == kLightDirectional) ); // directional lights should have 0 in position.w
	DebugAssertIf( (data.spotAngle != -1.0f) != (data.type == kLightSpot) ); // non-spot lights should have -1 in spot angle

	GfxVertexLight& dest = m_VertexData.lights[light];
	dest = data;

	const Matrix4x4f& viewMat = m_BuiltinParamValues.GetMatrixParam(kShaderMatView);

	if (data.type == kLightDirectional)
	{
		dest.position.Set(0.0f,0.0f,0.0f,0.0f);
		Vector3f v = viewMat.MultiplyVector3((const Vector3f&)data.position);
		dest.spotDirection.Set( v.x, v.y, v.z, 0.0f );
	}
	else
	{
		Vector3f v = viewMat.MultiplyPoint3((const Vector3f&)data.position);
		dest.position.Set( v.x, v.y, v.z, 1.0f );
		Vector3f d = viewMat.MultiplyVector3((const Vector3f&)data.spotDirection);
		dest.spotDirection.Set( d.x, d.y, d.z, 0.0f );
	}

	SetupVertexLightParams (light, data);
}

void GfxDeviceD3D9::SetAmbient( const float ambient[4] )
{
	if( m_VertexData.ambient != ambient )
	{
		m_VertexData.ambient.set( ambient );
		m_VertexData.ambientClamped.set( clamp01(ambient[0]), clamp01(ambient[1]), clamp01(ambient[2]), clamp01(ambient[3]) );
		m_BuiltinParamValues.SetVectorParam(kShaderVecLightModelAmbient, Vector4f(ambient));
	}
}


static D3DFOGMODE s_D3DFogModes[kFogModeCount] = { D3DFOG_NONE, D3DFOG_LINEAR, D3DFOG_EXP, D3DFOG_EXP2 };

void GfxDeviceD3D9::EnableFog(const GfxFogParams& fog)
{
	IDirect3DDevice9* dev = GetD3DDevice();
	DebugAssertIf( fog.mode <= kFogDisabled );
	if( m_FogParams.mode != fog.mode )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGTABLEMODE, s_D3DFogModes[fog.mode] )); // TODO: or maybe vertex fog?
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGENABLE, TRUE ));
		m_FogParams.mode = fog.mode;
	}
	if( m_FogParams.start != fog.start )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGSTART, *(DWORD*)&fog.start ));
		m_FogParams.start = fog.start;
	}
	if( m_FogParams.end != fog.end )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGEND, *(DWORD*)&fog.end ));
		m_FogParams.end = fog.end;
	}
	if( m_FogParams.density != fog.density )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGDENSITY, *(DWORD*)&fog.density ));
		m_FogParams.density = fog.density;
	}
	if( m_FogParams.color != fog.color )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGCOLOR, ColorToD3D(fog.color.GetPtr()) ));
		m_FogParams.color = fog.color;
	}
}

void GfxDeviceD3D9::DisableFog()
{
	IDirect3DDevice9* dev = GetD3DDevice();
	if( m_FogParams.mode != kFogDisabled )
	{
		D3D9_CALL(dev->SetRenderState( D3DRS_FOGENABLE, FALSE ));
		m_FogParams.mode = kFogDisabled;
	}
}

VBO* GfxDeviceD3D9::CreateVBO()
{
	VBO* vbo = new D3D9VBO();
	OnCreateVBO(vbo);
	return vbo;
}

void GfxDeviceD3D9::DeleteVBO( VBO* vbo )
{
	OnDeleteVBO(vbo);
	delete vbo;
}

DynamicVBO&	GfxDeviceD3D9::GetDynamicVBO()
{
	if( !m_DynamicVBO ) {
		m_DynamicVBO = new DynamicD3D9VBO( 1024 * 1024, 65536 ); // initial 1 MiB VB, 64 KiB IB
	}
	return *m_DynamicVBO;
}

IDirect3DVertexBuffer9* GfxDeviceD3D9::GetAllWhiteVertexStream()
{
	if( !m_AllWhiteVertexStream )
	{
		int maxVerts = 0x10000;
		int size = maxVerts * sizeof(D3DCOLOR);
		HRESULT hr = GetD3DDevice()->CreateVertexBuffer( size, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &m_AllWhiteVertexStream, NULL );
		if( !SUCCEEDED(hr) )
			return NULL;
		void* buffer;
		hr = m_AllWhiteVertexStream->Lock( 0 , 0, &buffer, 0 );
		if( !SUCCEEDED(hr) )
		{
			SAFE_RELEASE( m_AllWhiteVertexStream );
			return NULL;
		}
		D3DCOLOR* dest = (D3DCOLOR*)buffer;
		for( int i = 0; i < maxVerts; i++ )
			dest[i] = D3DCOLOR_ARGB(255, 255, 255, 255);
		m_AllWhiteVertexStream->Unlock();
	}
	return m_AllWhiteVertexStream;
}

void GfxDeviceD3D9::ResetDynamicResources()
{
	delete m_DynamicVBO;
	m_DynamicVBO = NULL;

	CleanupEventQueries ();
	ResetDynamicVBs ();

	#if ENABLE_PROFILER
		m_TimerQueriesD3D9.ReleaseAllQueries();
	#endif

	D3D9VBO::CleanupSharedIndexBuffer();
}


void ResetDynamicResourcesD3D9()
{
	AutoGfxDeviceAcquireThreadOwnership autoOwner;
	GetD3D9GfxDevice().ResetDynamicResources();
}

IDirect3DVertexDeclaration9* GetD3DVertexDeclaration( UInt32 shaderChannelsMap )
{
	ChannelInfoArray channels;
	int offset = 0;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		ChannelInfo& info = channels[i];
		if (shaderChannelsMap & (1 << i))
		{
			info.stream = 0;
			info.offset = offset;
			info.format = VBO::GetDefaultChannelFormat( i );
			info.dimension = VBO::GetDefaultChannelDimension( i );
			offset += VBO::GetDefaultChannelByteSize( i );
}
		else
			info.Reset();
	}
	return GetD3D9GfxDevice().GetVertexDecls().GetVertexDecl( channels );
}

VertexShaderConstantCache& GetD3D9VertexShaderConstantCache()
{
	return GetD3D9GfxDevice().GetVertexShaderConstantCache();
}

PixelShaderConstantCache& GetD3D9PixelShaderConstantCache()
{
	return GetD3D9GfxDevice().GetPixelShaderConstantCache();
}


// ---------- render textures

RenderSurfaceHandle GfxDeviceD3D9::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	return CreateRenderColorSurfaceD3D9 (textureID, width, height, samples, dim, createFlags, format, m_Textures);
}
RenderSurfaceHandle GfxDeviceD3D9::CreateRenderDepthSurface(TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	return CreateRenderDepthSurfaceD3D9 (textureID, width, height, samples, depthFormat, createFlags, m_Textures);
}
void GfxDeviceD3D9::DestroyRenderSurface(RenderSurfaceHandle& rs)
{
	DestroyRenderSurfaceD3D9( rs, m_Textures );
}
void GfxDeviceD3D9::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face)
{
	bool isBackBuffer;
	m_CurrTargetWidth = m_CurrWindowWidth;
	m_CurrTargetHeight = m_CurrWindowHeight;
	if (SetRenderTargetD3D9 (count, colorHandles, depthHandle, mipLevel, face, m_CurrTargetWidth, m_CurrTargetHeight, isBackBuffer))
	{
		// changing render target might mean different color clear flags; so reset current state
		m_CurrBlendState = NULL;
	}
}
void GfxDeviceD3D9::ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle)
{
	Assert (gGraphicsCaps.d3d.hasDepthResolveRESZ);

	RenderSurfaceD3D9* depthSurf = reinterpret_cast<RenderSurfaceD3D9*>(depthHandle.object);

	IDirect3DDevice9* dev = GetD3DDevice();
	// Important: change point size render state to something else than RESZ
	// before the dummy draw call; otherwise RESZ state set will be filtered out
	// by non-PURE D3D device.
	dev->SetRenderState (D3DRS_POINTSIZE, 0);

	// Bind destination as texture
	SetTexture (kShaderFragment, 0, 0, depthSurf->textureID, kTexDim2D, 0.0f);

	// Dummy draw call
	float dummy[3] = {0,0,0};
	dev->DrawPrimitiveUP (D3DPT_POINTLIST, 1, dummy, 12);

	// RESZ to trigger depth buffer copy
	dev->SetRenderState (D3DRS_POINTSIZE, 0x7fa05000);
}


void GfxDeviceD3D9::ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle)
{
	Assert (srcHandle.IsValid());
	Assert (dstHandle.IsValid());
	RenderColorSurfaceD3D9* src = reinterpret_cast<RenderColorSurfaceD3D9*>(srcHandle.object);
	RenderColorSurfaceD3D9* dst = reinterpret_cast<RenderColorSurfaceD3D9*>(dstHandle.object);
	if (!src->colorSurface || !dst->colorSurface)
	{
		WarningString("RenderTexture: Resolving non-color surfaces.");
		return;
	}
	if (!src->m_Surface || !dst->m_Surface)
	{
		WarningString("RenderTexture: Resolving NULL surfaces.");
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

	IDirect3DDevice9* dev = GetD3DDevice();
	dev->StretchRect (src->m_Surface, NULL, dst->m_Surface, NULL, D3DTEXF_NONE);
}

RenderSurfaceHandle GfxDeviceD3D9::GetActiveRenderColorSurface (int index)
{
	return GetActiveRenderColorSurfaceD3D9(index);
}
RenderSurfaceHandle GfxDeviceD3D9::GetActiveRenderDepthSurface()
{
	return GetActiveRenderDepthSurfaceD3D9();
}
void GfxDeviceD3D9::SetSurfaceFlags (RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags)
{
}


// ---------- uploading textures

void GfxDeviceD3D9::UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	m_Textures.UploadTexture2D( texture, dimension, srcData, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode, colorSpace );
}
void GfxDeviceD3D9::UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	m_Textures.UploadTextureSubData2D( texture, srcData, mipLevel, x, y, width, height, format, colorSpace );
}
void GfxDeviceD3D9::UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	m_Textures.UploadTextureCube( texture, srcData, faceDataSize, size, format, mipCount, uploadFlags, colorSpace );
}
void GfxDeviceD3D9::UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	m_Textures.UploadTexture3D( texture, srcData, width, height, depth, format, mipCount, uploadFlags );
}

void GfxDeviceD3D9::DeleteTexture( TextureID texture )
{
	m_Textures.DeleteTexture( texture );

	// invalidate texture unit states that used this texture
	for (int i = 0; i < ARRAY_SIZE(m_State.texturesPS); ++i)
	{
		TextureUnitStateD3D& currTex = m_State.texturesPS[i];
		if( currTex.texID == texture )
			currTex.Invalidate();
	}
	for (int i = 0; i < ARRAY_SIZE(m_State.texturesVS); ++i)
	{
		TextureUnitStateD3D& currTex = m_State.texturesVS[i];
		if (currTex.texID == texture)
			currTex.Invalidate();
	}
}

void UnbindTextureD3D9( TextureID texture )
{
	GfxDeviceD3D9& device = static_cast<GfxDeviceD3D9&>( GetRealGfxDevice() );
	IDirect3DDevice9* dev = GetD3DDevice();

	// invalidate texture unit states that used this texture
	for (int i = 0; i < ARRAY_SIZE(device.GetState().texturesPS); ++i)
	{
		TextureUnitStateD3D& currTex = device.GetState().texturesPS[i];
		if( currTex.texID == texture )
		{
			D3D9_CALL(dev->SetTexture(GetD3D9SamplerIndex(kShaderFragment,i), NULL));
			currTex.Invalidate();
		}
	}
	for (int i = 0; i < ARRAY_SIZE(device.GetState().texturesVS); ++i)
	{
		TextureUnitStateD3D& currTex = device.GetState().texturesVS[i];
		if (currTex.texID == texture)
		{
			D3D9_CALL(dev->SetTexture(GetD3D9SamplerIndex(kShaderVertex,i), NULL));
			currTex.Invalidate();
		}
	}
}


// ---------- context

GfxDevice::PresentMode GfxDeviceD3D9::GetPresentMode()
{
	return kPresentBeforeUpdate;
}

void GfxDeviceD3D9::BeginFrame()
{
	if( m_State.m_DeviceLost )
		return;

	// begin scene
	Assert( !m_InsideFrame );
	GetD3DDevice()->BeginScene();
	m_InsideFrame = true;

}

void GfxDeviceD3D9::EndFrame()
{
	// Check if we're inside scene in case BeginFrame() failed
	if( !m_InsideFrame )
		return;

	GetD3DDevice()->EndScene();
	m_InsideFrame = false;
}

bool GfxDeviceD3D9::IsValidState()
{
	return !m_State.m_DeviceLost;
}

bool GfxDeviceD3D9::HandleInvalidState()
{
#if ENABLE_MULTITHREADED_CODE
	// Reset render textures owned by the main thread
	if (Thread::CurrentThreadIsMainThread())
		CommonReloadResources(kReleaseRenderTextures);
#endif

	ResetDynamicResourcesD3D9();

	bool success = HandleD3DDeviceLost();

#if ENABLE_PROFILER
	if (success)
		m_TimerQueriesD3D9.RecreateAllQueries();
#endif

	InvalidateState();
	return success;
}

static void CleanupEventQueries ()
{
	D3D9QueryList::iterator itEnd = s_EventQueries.end();
	for (D3D9QueryList::iterator it = s_EventQueries.begin(); it != itEnd; ++it)
	{
		IDirect3DQuery9* query = *it;
		if (query != NULL)
		{
			query->Release();
		}
	}
	s_EventQueries.clear();
}

static void PopEventQuery ()
{
	AssertIf (s_EventQueries.empty());

	IDirect3DQuery9* query = s_EventQueries.front();
	AssertIf (query == NULL);

	while (S_FALSE == query->GetData (NULL, 0, D3DGETDATA_FLUSH))
	{
		Sleep (1);
	}
	query->Release();

	s_EventQueries.pop_front();
}

void GfxDeviceD3D9::PushEventQuery ()
{
	if (m_MaxBufferedFrames < 0)
		return;

	IDirect3DQuery9* query = NULL;
	HRESULT hr = GetD3DDevice()->CreateQuery (D3DQUERYTYPE_EVENT, &query);
	if (query != NULL)
	{
		if (SUCCEEDED(query->Issue(D3DISSUE_END)))
			s_EventQueries.push_back (query);
		else
			query->Release();
	}

	// don't exceed maximum lag...  instead we'll deterministically block here until the GPU has done enough work
	while (!s_EventQueries.empty() && s_EventQueries.size() > m_MaxBufferedFrames)
	{
		PopEventQuery();
	}
}

void GfxDeviceD3D9::PresentFrame()
{
	if( m_State.m_DeviceLost )
		return;

	HRESULT hr = GetD3DDevice()->Present( NULL, NULL, NULL, NULL );
	PushEventQuery();
	// When D3DERR_DRIVERINTERNALERROR is returned from Present(),
	// the application can do one of the following, try recovering just as
	// from the lost device.
	if( hr == D3DERR_DEVICELOST || hr == D3DERR_DRIVERINTERNALERROR )
	{
		m_State.m_DeviceLost = true;
	}
}

void GfxDeviceD3D9::FinishRendering()
{
	// not needed on D3D
}



// ---------- immediate mode rendering

// we break very large immediate mode submissions into multiple batches internally
const int kMaxImmediateVerticesPerDraw = 8192;


ImmediateModeD3D::ImmediateModeD3D()
:	m_ImmVertexDecl(NULL)
{
	m_QuadsIB = new UInt16[kMaxImmediateVerticesPerDraw*6];
	UInt32 baseIndex = 0;
	UInt16* ibPtr = m_QuadsIB;
	for( int i = 0; i < kMaxImmediateVerticesPerDraw; ++i )
	{
		ibPtr[0] = baseIndex + 1;
		ibPtr[1] = baseIndex + 2;
		ibPtr[2] = baseIndex;
		ibPtr[3] = baseIndex + 2;
		ibPtr[4] = baseIndex + 3;
		ibPtr[5] = baseIndex;
		baseIndex += 4;
		ibPtr += 6;
	}
}

ImmediateModeD3D::~ImmediateModeD3D()
{
	delete[] m_QuadsIB;
}


void ImmediateModeD3D::Invalidate()
{
	m_Vertices.clear();
	memset( &m_Current, 0, sizeof(m_Current) );
}

void GfxDeviceD3D9::ImmediateVertex( float x, float y, float z )
{
	// If the current batch is becoming too large, internally end it and begin it again.
	size_t currentSize = m_Imm.m_Vertices.size();
	if( currentSize >= kMaxImmediateVerticesPerDraw - 4 )
	{
		GfxPrimitiveType mode = m_Imm.m_Mode;
		// For triangles, break batch when multiple of 3's is reached.
		if( mode == kPrimitiveTriangles && currentSize % 3 == 0 )
		{
			ImmediateEnd();
			ImmediateBegin( mode );
		}
		// For other primitives, break on multiple of 4's.
		// NOTE: This won't quite work for triangle strips, but we'll just pretend
		// that will never happen.
		else if( mode != kPrimitiveTriangles && currentSize % 4 == 0 )
		{
			ImmediateEnd();
			ImmediateBegin( mode );
		}
	}
	D3DVECTOR& vert = m_Imm.m_Current.vertex;
	vert.x = x;
	vert.y = y;
	vert.z = z;
	m_Imm.m_Vertices.push_back( m_Imm.m_Current );
}

void GfxDeviceD3D9::ImmediateNormal( float x, float y, float z )
{
	m_Imm.m_Current.normal.x = x;
	m_Imm.m_Current.normal.y = y;
	m_Imm.m_Current.normal.z = z;
}

void GfxDeviceD3D9::ImmediateColor( float r, float g, float b, float a )
{
	float color[4] = { r, g, b, a };
	m_Imm.m_Current.color = ColorToD3D( color );
}

void GfxDeviceD3D9::ImmediateTexCoordAll( float x, float y, float z )
{
	for( int i = 0; i < 8; ++i )
	{
		D3DVECTOR& uv = m_Imm.m_Current.texCoords[i];
		uv.x = x;
		uv.y = y;
		uv.z = z;
	}
}

void GfxDeviceD3D9::ImmediateTexCoord( int unit, float x, float y, float z )
{
	if( unit < 0 || unit >= 8 )
	{
		ErrorString( "Invalid unit for texcoord" );
		return;
	}
	D3DVECTOR& uv = m_Imm.m_Current.texCoords[unit];
	uv.x = x;
	uv.y = y;
	uv.z = z;
}

void GfxDeviceD3D9::ImmediateBegin( GfxPrimitiveType type )
{
	m_Imm.m_Mode = type;
	m_Imm.m_Vertices.clear();
}

void GfxDeviceD3D9::ImmediateEnd()
{
	if( m_Imm.m_Vertices.empty() )
		return;

	// lazily create vertex declaration
	IDirect3DDevice9* dev = GetD3DDevice();
	HRESULT hr = S_OK;
	if( !m_Imm.m_ImmVertexDecl )
	{
		static const D3DVERTEXELEMENT9 elements[] = {
			// stream, offset, data type, processing, semantics, index
			{ 0,   0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 }, // position
			{ 0,  12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 }, // normal
			{ 0,  24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 }, // color
			{ 0,  28, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 }, // UVs
			{ 0,  40, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
			{ 0,  52, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
			{ 0,  64, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },
			{ 0,  76, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4 },
			{ 0,  88, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 5 },
			{ 0, 100, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 6 },
			{ 0, 112, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 7 },
			D3DDECL_END()
		};
		hr = dev->CreateVertexDeclaration( elements, &m_Imm.m_ImmVertexDecl );
		if( FAILED(hr) ) {
			// TODO: error
		}
	}

	// draw
	D3D9_CALL(dev->SetVertexDeclaration( m_Imm.m_ImmVertexDecl ));

	BeforeDrawCall( true );

	int vertexCount = m_Imm.m_Vertices.size();
	const ImmediateVertexD3D* vb = &m_Imm.m_Vertices[0];
	switch( m_Imm.m_Mode )
	{
	case kPrimitiveTriangles:
		hr = D3D9_CALL_HR(dev->DrawPrimitiveUP( D3DPT_TRIANGLELIST, vertexCount / 3, vb, sizeof(ImmediateVertexD3D) ));
		m_Stats.AddDrawCall( vertexCount / 3, vertexCount );
		break;
	case kPrimitiveTriangleStripDeprecated:
		hr = D3D9_CALL_HR(dev->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, vertexCount - 2, vb, sizeof(ImmediateVertexD3D) ));
		m_Stats.AddDrawCall( vertexCount - 2, vertexCount );
		break;
	case kPrimitiveQuads:
		hr = D3D9_CALL_HR(dev->DrawIndexedPrimitiveUP( D3DPT_TRIANGLELIST, 0, vertexCount, vertexCount / 4 * 2, m_Imm.m_QuadsIB, D3DFMT_INDEX16, vb, sizeof(ImmediateVertexD3D) ));
		m_Stats.AddDrawCall( vertexCount / 4 * 2, vertexCount );
		break;
	case kPrimitiveLines:
		hr = D3D9_CALL_HR(dev->DrawPrimitiveUP( D3DPT_LINELIST, vertexCount / 2, vb, sizeof(ImmediateVertexD3D) ));
		m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
		break;
	default:
		AssertString("ImmediateEnd: unknown draw mode");
	}
	AssertIf( FAILED(hr) );
	// TODO: stats

	// clear vertices
	m_Imm.m_Vertices.clear();
}



bool GfxDeviceD3D9::CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 )
{
	HRESULT hr;
	IDirect3DDevice9* dev = GetD3DDevice();

	SurfacePointer renderTarget;
	hr = dev->GetRenderTarget( 0, &renderTarget );
	if( !renderTarget || FAILED(hr) )
		return false;

	D3DSURFACE_DESC rtDesc;
	renderTarget->GetDesc( &rtDesc );

	SurfacePointer resolvedSurface;
	if( rtDesc.MultiSampleType != D3DMULTISAMPLE_NONE )
	{
		hr = dev->CreateRenderTarget( rtDesc.Width, rtDesc.Height, rtDesc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &resolvedSurface, NULL );
		if( FAILED(hr) )
			return false;
		hr = dev->StretchRect( renderTarget, NULL, resolvedSurface, NULL, D3DTEXF_NONE );
		if( FAILED(hr) )
			return false;
		renderTarget = resolvedSurface;
	}

	SurfacePointer offscreenSurface;
	hr = dev->CreateOffscreenPlainSurface( rtDesc.Width, rtDesc.Height, rtDesc.Format, D3DPOOL_SYSTEMMEM, &offscreenSurface, NULL );
	if( FAILED(hr) )
		return false;

	hr = dev->GetRenderTargetData( renderTarget, offscreenSurface );
	bool ok = SUCCEEDED(hr);
	if( ok )
	{
		rgba32 += (height-1) * width * sizeof(UInt32);
		if( rtDesc.Format == D3DFMT_A8R8G8B8 || rtDesc.Format == D3DFMT_X8R8G8B8 )
		{
			// Backbuffer is 32 bit
			D3DLOCKED_RECT lr;
			RECT rect;
			rect.left = left;
			rect.right = left + width;
			rect.top = rtDesc.Height - bottom - height;
			rect.bottom = rtDesc.Height - bottom;
			hr = offscreenSurface->LockRect( &lr, &rect, D3DLOCK_READONLY );
			if( SUCCEEDED(hr) )
			{
				const UInt8* src = (const UInt8*)lr.pBits;
				for( int y = 0; y < height; ++y )
				{
					const UInt32* srcPtr = (const UInt32*)src;
					UInt32* dstPtr = (UInt32*)rgba32;
					for( int x = 0; x < width; ++x )
					{
						UInt32 argbCol = *srcPtr;
						UInt32 abgrCol = (argbCol&0xFF00FF00) | ((argbCol&0x00FF0000)>>16) | ((argbCol&0x000000FF)<<16);
						*dstPtr = abgrCol;
						++srcPtr;
						++dstPtr;
					}
					rgba32 -= width * sizeof(UInt32);
					src += lr.Pitch;
				}
			}
			else
			{
				ok = false;
			}
			offscreenSurface->UnlockRect();
		}
		else if( rtDesc.Format == D3DFMT_R5G6B5 )
		{
			// Backbuffer is 16 bit 565
			D3DLOCKED_RECT lr;
			RECT rect;
			rect.left = left;
			rect.right = left + width;
			rect.top = rtDesc.Height - bottom - height;
			rect.bottom = rtDesc.Height - bottom;
			hr = offscreenSurface->LockRect( &lr, &rect, D3DLOCK_READONLY );
			if( SUCCEEDED(hr) )
			{
				const UInt8* src = (const UInt8*)lr.pBits;
				for( int y = 0; y < height; ++y )
				{
					const UInt16* srcPtr = (const UInt16*)src;
					UInt32* dstPtr = (UInt32*)rgba32;
					for( int x = 0; x < width; ++x )
					{
						UInt16 rgbCol = *srcPtr;
						UInt32 abgrCol = 0xFF000000 | ((rgbCol&0xF800)>>8) | ((rgbCol&0x07E0)<<5) | ((rgbCol&0x001F)<<19);
						*dstPtr = abgrCol;
						++srcPtr;
						++dstPtr;
					}
					rgba32 -= width * sizeof(UInt32);
					src += lr.Pitch;
				}
			}
			else
			{
				ok = false;
			}
			offscreenSurface->UnlockRect();
		}
		else if( rtDesc.Format == D3DFMT_X1R5G5B5 || rtDesc.Format == D3DFMT_A1R5G5B5 )
		{
			// Backbuffer is 15 bit 555
			D3DLOCKED_RECT lr;
			RECT rect;
			rect.left = left;
			rect.right = left + width;
			rect.top = rtDesc.Height - bottom - height;
			rect.bottom = rtDesc.Height - bottom;
			hr = offscreenSurface->LockRect( &lr, &rect, D3DLOCK_READONLY );
			if( SUCCEEDED(hr) )
			{
				const UInt8* src = (const UInt8*)lr.pBits;
				for( int y = 0; y < height; ++y )
				{
					const UInt16* srcPtr = (const UInt16*)src;
					UInt32* dstPtr = (UInt32*)rgba32;
					for( int x = 0; x < width; ++x )
					{
						UInt16 rgbCol = *srcPtr;
						UInt32 abgrCol = ((rgbCol&0x8000)<<16) | ((rgbCol&0x7C00)>>7) | ((rgbCol&0x03E0)<<6) | ((rgbCol&0x001F)<<19);
						*dstPtr = abgrCol;
						++srcPtr;
						++dstPtr;
					}
					rgba32 -= width * sizeof(UInt32);
					src += lr.Pitch;
				}
			}
			else
			{
				ok = false;
			}
			offscreenSurface->UnlockRect();
		}
		else
		{
			// TODO: handle more conversions!
			ok = false;
		}
	}

	return ok;
}



bool GfxDeviceD3D9::ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	// TODO: make it work in all different situations

	AssertIf( image.GetFormat() != kTexFormatARGB32 && image.GetFormat() != kTexFormatRGB24 );

	HRESULT hr;
	IDirect3DDevice9* dev = GetD3DDevice();
	SurfacePointer renderTarget;
	hr = dev->GetRenderTarget( 0, &renderTarget );
	if( !renderTarget || FAILED(hr) )
		return false;

	D3DSURFACE_DESC rtDesc;
	renderTarget->GetDesc( &rtDesc );

	SurfacePointer resolvedSurface;
	if( rtDesc.MultiSampleType != D3DMULTISAMPLE_NONE )
	{
		hr = dev->CreateRenderTarget( rtDesc.Width, rtDesc.Height, rtDesc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &resolvedSurface, NULL );
		if( FAILED(hr) )
			return false;
		hr = dev->StretchRect( renderTarget, NULL, resolvedSurface, NULL, D3DTEXF_NONE );
		if( FAILED(hr) )
			return false;
		renderTarget = resolvedSurface;
	}

	SurfacePointer offscreenSurface;
	hr = dev->CreateOffscreenPlainSurface( rtDesc.Width, rtDesc.Height, rtDesc.Format, D3DPOOL_SYSTEMMEM, &offscreenSurface, NULL );
	if( FAILED(hr) )
		return false;
	if (width <= 0 || left < 0 || left + width > rtDesc.Width)
	{
		ErrorString("Trying to read pixel out of bounds");
		return false;
	}
	if (height <= 0 || bottom < 0 || bottom + height > rtDesc.Height)
	{
		ErrorString("Trying to read pixel out of bounds");
		return false;
	}

	hr = dev->GetRenderTargetData( renderTarget, offscreenSurface );
	bool ok = SUCCEEDED(hr);
	if( ok )
	{
		if( rtDesc.Format == D3DFMT_A8R8G8B8 || rtDesc.Format == D3DFMT_X8R8G8B8 )
		{
			// Render target is 32 bit
			D3DLOCKED_RECT lr;
			RECT rect;
			rect.left = left;
			rect.right = left + width;
			rect.top = rtDesc.Height - bottom - height;
			rect.bottom = rtDesc.Height - bottom;
			hr = offscreenSurface->LockRect( &lr, &rect, D3DLOCK_READONLY );
			if( SUCCEEDED(hr) )
			{
				const UInt8* src = (const UInt8*)lr.pBits;
				if( image.GetFormat() == kTexFormatARGB32 )
				{
					for( int y = height-1; y >= 0; --y )
					{
						const UInt32* srcPtr = (const UInt32*)src;
						UInt32* dstPtr = (UInt32*)(image.GetRowPtr(destY+y) + destX * 4);
						for( int x = 0; x < width; ++x )
						{
							UInt32 argbCol = *srcPtr;
							UInt32 bgraCol = ((argbCol&0xFF000000)>>24) | ((argbCol&0x00FF0000)>>8) | ((argbCol&0x0000FF00)<<8) | ((argbCol&0x000000FF)<<24);
							*dstPtr = bgraCol;
							++srcPtr;
							++dstPtr;
						}
						src += lr.Pitch;
					}
				}
				else if( image.GetFormat() == kTexFormatRGB24 )
				{
					for( int y = height-1; y >= 0; --y )
					{
						const UInt32* srcPtr = (const UInt32*)src;
						UInt8* dstPtr = image.GetRowPtr(destY+y) + destX * 3;
						for( int x = 0; x < width; ++x )
						{
							UInt32 argbCol = *srcPtr;
							dstPtr[0] = (argbCol & 0x00FF0000) >> 16;
							dstPtr[1] = (argbCol & 0x0000FF00) >> 8;
							dstPtr[2] = (argbCol & 0x000000FF);
							++srcPtr;
							dstPtr += 3;
						}
						src += lr.Pitch;
					}
				}
				else
				{
					AssertString( "Invalid image format" );
				}
			}
			else
			{
				ok = false;
			}
			offscreenSurface->UnlockRect();
		}
		else if( rtDesc.Format == D3DFMT_R5G6B5 )
		{
			// Render target is 16 bit 565
			D3DLOCKED_RECT lr;
			RECT rect;
			rect.left = left;
			rect.right = left + width;
			rect.top = rtDesc.Height - bottom - height;
			rect.bottom = rtDesc.Height - bottom;
			hr = offscreenSurface->LockRect( &lr, &rect, D3DLOCK_READONLY );
			if( SUCCEEDED(hr) )
			{
				const UInt8* src = (const UInt8*)lr.pBits;
				if( image.GetFormat() == kTexFormatARGB32 )
				{
					for( int y = height-1; y >= 0; --y )
					{
						const UInt16* srcPtr = (const UInt16*)src;
						UInt32* dstPtr = (UInt32*)(image.GetRowPtr(destY+y) + destX * 4);
						for( int x = 0; x < width; ++x )
						{
							UInt16 argbCol = *srcPtr;
							UInt32 bgraCol = 0x000000FF | (argbCol&0xF800) | ((argbCol&0x07E0)<<13) | ((argbCol&0x001F)<<27);
							*dstPtr = bgraCol;
							++srcPtr;
							++dstPtr;
						}
						src += lr.Pitch;
					}
				}
				else if( image.GetFormat() == kTexFormatRGB24 )
				{
					for( int y = height-1; y >= 0; --y )
					{
						const UInt16* srcPtr = (const UInt16*)src;
						UInt8* dstPtr = image.GetRowPtr(destY+y) + destX * 3;
						for( int x = 0; x < width; ++x )
						{
							UInt16 argbCol = *srcPtr;
							dstPtr[0] = (argbCol & 0xF800) >> 8;
							dstPtr[1] = (argbCol & 0x07E0) >> 3;
							dstPtr[2] = (argbCol & 0x001F) << 3;
							++srcPtr;
							dstPtr += 3;
						}
						src += lr.Pitch;
					}
				}
				else
				{
					AssertString( "Invalid image format" );
				}
			}
			else
			{
				ok = false;
			}
			offscreenSurface->UnlockRect();
		}
		else if( rtDesc.Format == D3DFMT_A1R5G5B5 || rtDesc.Format == D3DFMT_X1R5G5B5 )
		{
			// Render target is 15 bit 555
			D3DLOCKED_RECT lr;
			RECT rect;
			rect.left = left;
			rect.right = left + width;
			rect.top = rtDesc.Height - bottom - height;
			rect.bottom = rtDesc.Height - bottom;
			hr = offscreenSurface->LockRect( &lr, &rect, D3DLOCK_READONLY );
			if( SUCCEEDED(hr) )
			{
				const UInt8* src = (const UInt8*)lr.pBits;
				if( image.GetFormat() == kTexFormatARGB32 )
				{
					for( int y = height-1; y >= 0; --y )
					{
						const UInt16* srcPtr = (const UInt16*)src;
						UInt32* dstPtr = (UInt32*)(image.GetRowPtr(destY+y) + destX * 4);
						for( int x = 0; x < width; ++x )
						{
							UInt16 argbCol = *srcPtr;
							UInt32 bgraCol = ((argbCol&0x8000)>>8) | ((argbCol&0x7C00)<<1) | ((argbCol&0x03E0)<<14) | ((argbCol&0x001F)<<27);
							*dstPtr = bgraCol;
							++srcPtr;
							++dstPtr;
						}
						src += lr.Pitch;
					}
				}
				else if( image.GetFormat() == kTexFormatRGB24 )
				{
					for( int y = height-1; y >= 0; --y )
					{
						const UInt16* srcPtr = (const UInt16*)src;
						UInt8* dstPtr = image.GetRowPtr(destY+y) + destX * 3;
						for( int x = 0; x < width; ++x )
						{
							UInt16 argbCol = *srcPtr;
							dstPtr[0] = (argbCol & 0x7C00) >> 7;
							dstPtr[1] = (argbCol & 0x03E0) >> 2;
							dstPtr[2] = (argbCol & 0x001F) << 3;
							++srcPtr;
							dstPtr += 3;
						}
						src += lr.Pitch;
					}
				}
				else
				{
					AssertString( "Invalid image format" );
				}
			}
			else
			{
				ok = false;
			}
			offscreenSurface->UnlockRect();
		}
		else
		{
			// TODO: handle more conversions!
			ok = false;
		}
	}

	return ok;
}

void GfxDeviceD3D9::GrabIntoRenderTexture(RenderSurfaceHandle rtHandle, RenderSurfaceHandle rd, int x, int y, int width, int height )
{
	if( !rtHandle.IsValid() )
		return;

	RenderColorSurfaceD3D9* renderTexture = reinterpret_cast<RenderColorSurfaceD3D9*>( rtHandle.object );

	HRESULT hr;
	IDirect3DDevice9* dev = GetD3DDevice();
	SurfacePointer currentRenderTarget;
	hr = dev->GetRenderTarget( 0, &currentRenderTarget );
	if( !currentRenderTarget || FAILED(hr) )
		return;

	D3DSURFACE_DESC rtDesc;
	currentRenderTarget->GetDesc( &rtDesc );

	IDirect3DTexture9* texturePointer = static_cast<IDirect3DTexture9*>(m_Textures.GetTexture (renderTexture->textureID));
	if( !texturePointer )
		return;

	SurfacePointer textureSurface;
	hr = texturePointer->GetSurfaceLevel( 0, &textureSurface );
	if( !textureSurface || FAILED(hr) )
		return;

	RECT rc;
	rc.left = x;
	rc.top = rtDesc.Height - (y + height);
	rc.right = x + width;
	rc.bottom = rtDesc.Height - (y);
	hr = dev->StretchRect( currentRenderTarget, &rc, textureSurface, NULL, D3DTEXF_NONE );
}


void* GfxDeviceD3D9::GetNativeGfxDevice()
{
	return GetD3DDevice();
}

void* GfxDeviceD3D9::GetNativeTexturePointer(TextureID id)
{
	return m_Textures.GetTexture (id);
}

intptr_t GfxDeviceD3D9::CreateExternalTextureFromNative(intptr_t nativeTex)
{
	return m_Textures.RegisterNativeTexture((IDirect3DBaseTexture9*)nativeTex);
}

void GfxDeviceD3D9::UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex)
{
	m_Textures.UpdateNativeTexture(tex, (IDirect3DBaseTexture9*)nativeTex);
}


#if ENABLE_PROFILER

void GfxDeviceD3D9::BeginProfileEvent (const char* name)
{
	if (g_D3D9BeginEventFunc)
	{
		wchar_t wideName[100];
		UTF8ToWide (name, wideName, 100);
		g_D3D9BeginEventFunc (0, wideName);
	}
}

void GfxDeviceD3D9::EndProfileEvent ()
{
	if (g_D3D9EndEventFunc)
	{
		g_D3D9EndEventFunc ();
	}
}

GfxTimerQuery* GfxDeviceD3D9::CreateTimerQuery()
{
	Assert(gGraphicsCaps.hasTimerQuery);
	return m_TimerQueriesD3D9.CreateTimerQuery();
}

void GfxDeviceD3D9::DeleteTimerQuery(GfxTimerQuery* query)
{
	delete query;
}

void GfxDeviceD3D9::BeginTimerQueries()
{
	if(!gGraphicsCaps.hasTimerQuery)
		return;

	m_TimerQueriesD3D9.BeginTimerQueries();
}

void GfxDeviceD3D9::EndTimerQueries()
{
	if(!gGraphicsCaps.hasTimerQuery)
		return;

	m_TimerQueriesD3D9.EndTimerQueries();
}

/*
SInt32 GfxDeviceD3D9::GetTimerQueryIdentifier()
{
	if(!gGraphicsCaps.hasTimerQuery)
		return -1;
	// Allocate more queries
	if(m_QueryCount[m_CurrentQueryBuffer] >= m_GPUQueries[m_CurrentQueryBuffer].size())
	{
		int count = std::max (m_QueryCount[m_CurrentQueryBuffer], 100);
		IDirect3DQuery9* d3dQuery;
		for( int i = 0; i < count; i++)
		{
			GetD3DDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &d3dQuery);
			// initialze more Query objects
			m_GPUQueries[m_CurrentQueryBuffer].push_back(d3dQuery);
		}
	}
	int index = m_QueryCount[m_CurrentQueryBuffer]++;
	IDirect3DQuery9* currentQuery = m_GPUQueries[m_CurrentQueryBuffer][index];
	currentQuery ->Issue(D3DISSUE_END);
	return index;
}

ProfileTimeFormat GfxDeviceD3D9::GetTimerQueryData(SInt32 identifier, bool wait)
{
	if(!gGraphicsCaps.hasTimerQuery)
		return 0;

	if(m_GPUQueries[m_CurrentQueryBuffer].size()<=identifier)
		return 0;

	UINT64 time;
	while (S_OK != m_GPUQueries[m_CurrentQueryBuffer][identifier]->GetData(&time, sizeof(time), D3DGETDATA_FLUSH)) {}
	return (double)time * m_TimeMultiplier;
}

void GfxDeviceD3D9::CleanupTimerQueries ()
{
	if(!gGraphicsCaps.hasTimerQuery)
		return;

	for(int buffer = 0; buffer < 2; buffer++)
	{
		for(int i = 0; i < m_GPUQueries[buffer].size(); i++)
			m_GPUQueries[buffer][i]->Release();
		m_GPUQueries[buffer].clear();
		if(m_FrequencyQuery[buffer])
			m_FrequencyQuery[buffer]->Release();
		m_FrequencyQuery[buffer] = NULL;
		m_QueryCount[buffer] = 0;
	}
}
*/

#endif // ENABLE_PROFILER


// -------- editor only functions

#if UNITY_EDITOR
void GfxDeviceD3D9::SetAntiAliasFlag( bool aa )
{
	#pragma message("! implement SetAntiAliasFlag")
}


void GfxDeviceD3D9::DrawUserPrimitives( GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride )
{
	if( vertexCount == 0 )
		return;

	AssertIf(vertexCount > 60000); // TODO: handle this by multi-batching

	AssertIf( !data || vertexCount < 0 || vertexChannels == 0 );

	IDirect3DDevice9* dev = GetD3DDevice();

	IDirect3DVertexDeclaration9* vertexDecl = GetD3DVertexDeclaration( vertexChannels );

	ChannelAssigns channels;
	for( int i = 0; i < kShaderChannelCount; ++i )
	{
		if( !( vertexChannels & (1<<i) ) )
			continue;
		VertexComponent destComponent = kSuitableVertexComponentForChannel[i];
		channels.Bind( (ShaderChannel)i, destComponent );
	}
	D3D9_CALL(dev->SetVertexDeclaration( vertexDecl ));
	UpdateChannelBindingsD3D( channels );
	BeforeDrawCall(false);

	HRESULT hr;
	switch( type ) {
	case kPrimitiveTriangles:
		hr = D3D9_CALL_HR(dev->DrawPrimitiveUP( D3DPT_TRIANGLELIST, vertexCount/3, data, stride ));
		m_Stats.AddDrawCall( vertexCount / 3, vertexCount );
		break;
	case kPrimitiveQuads:
		while (vertexCount > 0)
		{
			int vcount = std::min(vertexCount,kMaxImmediateVerticesPerDraw);
			hr = D3D9_CALL_HR(dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vcount, vcount / 4 * 2, m_Imm.m_QuadsIB, D3DFMT_INDEX16, data, stride));
			m_Stats.AddDrawCall(vcount / 4 * 2, vcount);
			data = (const UInt8*)data + vcount * stride;
			vertexCount -= vcount;
		}
		break;
	case kPrimitiveLines:
		hr = D3D9_CALL_HR(dev->DrawPrimitiveUP( D3DPT_LINELIST, vertexCount/2, data, stride ));
		m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
		break;
	case kPrimitiveLineStrip:
		hr = D3D9_CALL_HR(dev->DrawPrimitiveUP( D3DPT_LINESTRIP, vertexCount-1, data, stride ));
		m_Stats.AddDrawCall( vertexCount-1, vertexCount );
		break;
	default:
		ErrorString("Primitive type not supported");
		return;
	}
	Assert(SUCCEEDED(hr));
}

int GfxDeviceD3D9::GetCurrentTargetAA() const
{
	return GetCurrentD3DFSAALevel();
}

GfxDeviceWindow*	GfxDeviceD3D9::CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias )
{
	return new D3D9Window( GetD3DDevice(), window, width, height, depthFormat, antiAlias);
}

#endif

int GfxDeviceD3D9::GetCurrentTargetWidth() const
{
	return m_CurrTargetWidth;
}

int GfxDeviceD3D9::GetCurrentTargetHeight() const
{
	return m_CurrTargetHeight;
}

void GfxDeviceD3D9::SetCurrentTargetSize(int width, int height)
{
	m_CurrTargetWidth = width;
	m_CurrTargetHeight = height;
}

void GfxDeviceD3D9::SetCurrentWindowSize(int width, int height)
{
	m_CurrWindowWidth = m_CurrTargetWidth = width;
	m_CurrWindowHeight = m_CurrTargetHeight = height;
}


#if UNITY_EDITOR

static IDirect3DTexture9* FindD3D9TextureByID (TextureID tid)
{
	GfxDevice& device = GetRealGfxDevice();
	if (device.GetRenderer() != kGfxRendererD3D9)
		return NULL;
	GfxDeviceD3D9& dev = static_cast<GfxDeviceD3D9&>(device);
	IDirect3DBaseTexture9* basetex = dev.GetTextures().GetTexture (tid);
	if (!basetex)
		return NULL;
	if (basetex->GetType() != D3DRTYPE_TEXTURE)
		return NULL;
	return static_cast<IDirect3DTexture9*>(basetex);
}

// In the editor, for drawing directly into HDC of D3D texture.
// Functions not defined in any header; declare prototypes manually:
//   HDC AcquireHDCForTextureD3D9 (TextureID tid, int& outWidth, int& outHeight);
//   void ReleaseHDCForTextureD3D9 (TextureID tid, HDC dc);
// AcquireHDCForTextureD3D9 _can_ return NULL if it can't get to DC (not D3D9, no
// texture, wrong texture format, ...).

HDC AcquireHDCForTextureD3D9 (TextureID tid, int& outWidth, int& outHeight)
{
	IDirect3DTexture9* tex = FindD3D9TextureByID (tid);
	if (!tex)
		return NULL;
	SurfacePointer surface;
	if (FAILED(tex->GetSurfaceLevel(0,&surface)))
		return NULL;
	D3DSURFACE_DESC desc;
	if (FAILED(surface->GetDesc (&desc)))
		return NULL;
	outWidth = desc.Width;
	outHeight = desc.Height;
	HDC dc = NULL;
	if (FAILED(surface->GetDC(&dc)))
		return NULL;
	return dc;
}

void ReleaseHDCForTextureD3D9 (TextureID tid, HDC dc)
{
	IDirect3DTexture9* tex = FindD3D9TextureByID (tid);
	if (!tex)
		return;
	SurfacePointer surface;
	if (FAILED(tex->GetSurfaceLevel(0,&surface)))
		return;
	surface->ReleaseDC (dc);
}

#endif


// ----------------------------------------------------------------------
//  verification of state

#if GFX_DEVICE_VERIFY_ENABLE

#include "Runtime/Utilities/Utility.h"

void VerifyStateF(D3DRENDERSTATETYPE rs, float val, const char *str);
#define VERIFYF(s,t) VerifyState (s, t, #s " (" #t ")")
void VerifyStateI(D3DRENDERSTATETYPE rs, int val, const char *str);
#define VERIFYI(s,t) VerifyStateI (s, t, #s " (" #t ")")
void VerifyEnabled(D3DRENDERSTATETYPE rs, bool val, const char *str);
#define VERIFYENAB(s,t) VerifyEnabled ( s, t, #s " (" #t ")")

static void VERIFY_PRINT( const char* format, ... )
{
	ErrorString( VFormat( format, va_list(&format + 1) ) );
}

const float kVerifyDelta = 0.0001f;

void VerifyStateF(D3DRENDERSTATETYPE rs, float val, const char *str)
{
	float temp = 0;
	GetD3DDevice()->GetRenderState(rs,(DWORD*)&temp);
	if( !CompareApproximately(temp,val,kVerifyDelta) ) {
		VERIFY_PRINT ("%s differs from cache (%f != %f)\n", str, val, temp);
	}
}

void VerifyStateI(D3DRENDERSTATETYPE rs, int val, const char *str)
{
	int temp;
	GetD3DDevice()->GetRenderState(rs,(DWORD*)&temp);
	if (temp != val) {
		VERIFY_PRINT ("%s differs from cache (%i != %i)\n", str, val, temp);
	}
}

void VerifyEnabled(D3DRENDERSTATETYPE rs, bool val, const char *str)
{
	DWORD v;
	GetD3DDevice()->GetRenderState(rs,&v);
	bool temp = v==TRUE ? true : false;
	if (temp != val) {
		VERIFY_PRINT ("%s differs from cache (%d != %d)\n", str, val, temp);
	}
}

void GfxDeviceD3D9::VerifyState()
{
	// check if current state blocks match internal state
	if (m_CurrBlendState != NULL) {
		if (m_State.blending == 0) {
			Assert (D3DBLEND_ONE == kBlendModeD3D9[m_CurrBlendState->sourceState.srcBlend]);
			Assert (D3DBLEND_ZERO == kBlendModeD3D9[m_CurrBlendState->sourceState.dstBlend]);
		} else {
			Assert (m_State.srcBlend == kBlendModeD3D9[m_CurrBlendState->sourceState.srcBlend]);
			Assert (m_State.destBlend == kBlendModeD3D9[m_CurrBlendState->sourceState.dstBlend]);
		}
		#if !UNITY_EDITOR // Editor does some funkiness when emulating alpha test, see SetBlendState
		Assert (kCmpFuncD3D9[m_State.alphaFunc] == m_CurrBlendState->alphaFunc);
		#endif
	}

	m_State.Verify();
}



void DeviceStateD3D::Verify()
{
	#ifdef DUMMY_D3D9_CALLS
	return;
	#endif
	if( !GetD3DDevice() ) {
		ErrorString("Verify: no D3D device");
		return;
	}

	if( depthFunc != kFuncUnknown ) {
		VERIFYI( D3DRS_ZFUNC, kCmpFuncD3D9[depthFunc] );
	}
	if( depthWrite != -1 ) {
		VERIFYI( D3DRS_ZWRITEENABLE, (depthWrite ? TRUE : FALSE) );
	}
	if( blending != -1 ) {
		VERIFYENAB( D3DRS_ALPHABLENDENABLE, blending != 0 );
		if( blending ) {
			VERIFYI( D3DRS_SRCBLEND, srcBlend );
			VERIFYI( D3DRS_DESTBLEND, destBlend );
		}
	}

	if( alphaFunc != kFuncUnknown ) {
		VERIFYENAB( D3DRS_ALPHATESTENABLE, alphaFunc != kFuncDisabled );
		if( alphaFunc != kFuncDisabled ) {
			VERIFYI( D3DRS_ALPHAFUNC, kCmpFuncD3D9[alphaFunc] );
			if( alphaValue != -1 )
				VERIFYI( D3DRS_ALPHAREF, alphaValue*255.0f );
		}
	}
}

#endif // GFX_DEVICE_VERIFY_ENABLE

