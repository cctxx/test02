#pragma once

#include "D3D9Includes.h"
#include "VertexDeclarations.h"
#include "TexturesD3D9.h"
#include "Runtime/GfxDevice/ShaderConstantCache.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "VertexPipeD3D9.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "D3D9Context.h"
#include "Runtime/Math/FloatConversion.h"
#include "D3D9VBO.h"
#include "CombinerD3D.h"
#include "External/shaderlab/Library/program.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/GfxDevice/BuiltinShaderParams.h"
#include "Runtime/Graphics/Image.h"
#include "PlatformDependent/Win/SmartComPointer.h"
#include "Runtime/Utilities/Utility.h"
#include "D3D9Utils.h"
#include "D3D9Window.h"
#include "GpuProgramsD3D.h"
#include "TimerQueryD3D9.h"

typedef SmartComPointer<IDirect3DSurface9> SurfacePointer;

struct TextureUnitStateD3D
{
	TextureID	texID;
	float		bias;

	void Invalidate()
	{
		texID.m_ID = -1;
		bias = 1.0e6f;
	}
};

class GfxDeviceD3D9;

struct DeviceStateD3D
{
	int				viewport[4];
	int				scissorRect[4];

	CompareFunction	depthFunc;
	int				depthWrite; // 0/1 or -1

	int				blending;
	int				srcBlend, destBlend, srcBlendAlpha, destBlendAlpha; // D3D modes
	int				blendOp, blendOpAlpha; // D3D modes
	CompareFunction alphaFunc;
	float			alphaValue;

	CullMode		culling;
	D3DCULL			d3dculling;
	bool			appBackfaceMode, userBackfaceMode, invertProjMatrix;
	bool			wireframe;
	int				scissor;

	// [0] is front, [1] is back, unless invertProjMatrix is true
	D3DCMPFUNC		stencilFunc[2];
	D3DSTENCILOP	stencilFailOp[2], depthFailOp[2], depthPassOp[2];

	float offsetFactor, offsetUnits;

	GpuProgram* activeGpuProgram[kShaderTypeCount];
	const GpuProgramParameters* activeGpuProgramParams[kShaderTypeCount];
	IUnknown* activeShader[kShaderTypeCount];

	int				colorWriteMask; // ColorWriteMask combinations

	int		m_StencilRef;

	TextureUnitStateD3D	texturesPS[kMaxSupportedTextureUnits];
	TextureUnitStateD3D	texturesVS[4];

	int			fixedFunctionPS;

	bool		m_DeviceLost;

	bool	m_SoftwareVP;
	UInt32	m_NeedsSofwareVPFlags;

	void	Invalidate( GfxDeviceD3D9& device );
	void	Verify();
};

// TODO: optimize this. Right now we just send off whole 8 float3 UVs with each
// immediate mode vertex. We could at least detect the number of them used from
// ImmediateTexCoord calls.
struct ImmediateVertexD3D {
	D3DVECTOR	vertex;
	D3DVECTOR	normal;
	D3DCOLOR	color;
	D3DVECTOR	texCoords[8];
};

struct ImmediateModeD3D {
	std::vector<ImmediateVertexD3D>	m_Vertices;
	ImmediateVertexD3D				m_Current;
	GfxPrimitiveType				m_Mode;
	IDirect3DVertexDeclaration9*	m_ImmVertexDecl;
	UInt16*							m_QuadsIB;

	ImmediateModeD3D();
	~ImmediateModeD3D();
	void Invalidate();
};

class GfxDeviceD3D9 : public GfxThreadableDevice
{
public:
	struct DeviceBlendStateD3D9 : public DeviceBlendState
	{
		UInt8		renderTargetWriteMask;
		D3DCMPFUNC	alphaFunc;
	};

	struct DeviceDepthStateD3D9 : public DeviceDepthState
	{
		D3DCMPFUNC depthFunc;
	};

	struct DeviceStencilStateD3D9 : public DeviceStencilState
	{
		D3DCMPFUNC		stencilFuncFront;
		D3DSTENCILOP	stencilFailOpFront;
		D3DSTENCILOP	depthFailOpFront;
		D3DSTENCILOP	depthPassOpFront;
		D3DCMPFUNC		stencilFuncBack;
		D3DSTENCILOP	stencilFailOpBack;
		D3DSTENCILOP	depthFailOpBack;
		D3DSTENCILOP	depthPassOpBack;
	};


	typedef std::map< GfxBlendState, DeviceBlendStateD3D9,  memcmp_less<GfxBlendState> > CachedBlendStates;
	typedef std::map< GfxDepthState, DeviceDepthStateD3D9,  memcmp_less<GfxDepthState> > CachedDepthStates;
	typedef std::map< GfxStencilState, DeviceStencilStateD3D9,  memcmp_less<GfxStencilState> > CachedStencilStates;
	typedef std::map< GfxRasterState, DeviceRasterState,  memcmp_less<GfxRasterState> > CachedRasterStates;


public:
	GfxDeviceD3D9();
	GFX_API ~GfxDeviceD3D9();

	GFX_API void	InvalidateState();
	#if GFX_DEVICE_VERIFY_ENABLE
	GFX_API void	VerifyState();
	#endif

	GFX_API void	Clear(UInt32 clearFlags, const float color[4], float depth, int stencil);
	GFX_API void	SetUserBackfaceMode( bool enable );
	GFX_API void SetWireframe(bool wire);
	GFX_API bool GetWireframe() const;
	GFX_API void	SetInvertProjectionMatrix( bool enable );
	GFX_API bool	GetInvertProjectionMatrix() const;

	GFX_API GPUSkinningInfo *CreateGPUSkinningInfo() { return NULL; }
	GFX_API void	DeleteGPUSkinningInfo(GPUSkinningInfo *info) { AssertBreak(false); }
	GFX_API void	SkinOnGPU( GPUSkinningInfo * info, bool lastThisFrame ) { AssertBreak(false); }
	GFX_API void	UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty) { AssertBreak(false); }
	GFX_API void	UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses) { AssertBreak(false); }

	GFX_API DeviceBlendState* CreateBlendState(const GfxBlendState& state);
	GFX_API DeviceDepthState* CreateDepthState(const GfxDepthState& state);
	GFX_API DeviceStencilState* CreateStencilState(const GfxStencilState& state);
	GFX_API DeviceRasterState* CreateRasterState(const GfxRasterState& state);

	GFX_API void	SetBlendState(const DeviceBlendState* state, float alphaRef);
	GFX_API void	SetRasterState(const DeviceRasterState* state);
	GFX_API void	SetDepthState(const DeviceDepthState* state);
	GFX_API void	SetStencilState(const DeviceStencilState* state, int stencilRef);
	GFX_API void	SetSRGBWrite (const bool);
	GFX_API bool	GetSRGBWrite ();

	GFX_API void	SetWorldMatrix( const float matrix[16] );
	GFX_API void	SetViewMatrix( const float matrix[16] );
	GFX_API void	SetProjectionMatrix(const Matrix4x4f& matrix);
	GFX_API void	GetMatrix( float outMatrix[16] ) const;

	GFX_API	const float* GetWorldMatrix() const ;
	GFX_API	const float* GetViewMatrix() const ;
	GFX_API	const float* GetProjectionMatrix() const ;
	GFX_API const float* GetDeviceProjectionMatrix() const;

	GFX_API void	SetNormalizationBackface( NormalizationMode mode, bool backface );
	GFX_API void	SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial );
	GFX_API void	SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess );
	GFX_API void	SetColor( const float color[4] );
	GFX_API void	SetViewport( int x, int y, int width, int height );
	GFX_API void	GetViewport( int* port ) const;

	GFX_API void	SetScissorRect( int x, int y, int width, int height );
	GFX_API void	DisableScissor();
	GFX_API bool	IsScissorEnabled() const;
	GFX_API void	GetScissorRect( int values[4] ) const;

	GFX_API bool	IsCombineModeSupported( unsigned int combiner );
	GFX_API TextureCombinersHandle CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular );
	GFX_API void	DeleteTextureCombiners( TextureCombinersHandle& textureCombiners );
	GFX_API void	SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors );
	GFX_API void	SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props );

	GFX_API void	SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias);
	GFX_API void	SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace );
	GFX_API void	SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16]);
	GFX_API void	SetTextureName ( TextureID texture, const char* name ) { }

	GFX_API void	SetShadersThreadable (GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount]);
	GFX_API bool	IsShaderActive( ShaderType type ) const;
	GFX_API void	DestroySubProgram( ShaderLab::SubProgram* subprogram );

	GFX_API void	DisableLights( int startLight );
	GFX_API void	SetLight( int light, const GfxVertexLight& data);
	GFX_API void	SetAmbient( const float ambient[4] );

	GFX_API void	EnableFog(const GfxFogParams& fog);
	GFX_API void	DisableFog();

	GFX_API VBO*	CreateVBO();
	GFX_API void	DeleteVBO( VBO* vbo );
	GFX_API DynamicVBO&	GetDynamicVBO();

	GFX_API RenderSurfaceHandle CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags);
	GFX_API RenderSurfaceHandle CreateRenderDepthSurface(TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags);
	GFX_API void DestroyRenderSurface(RenderSurfaceHandle& rs);
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face = kCubeFaceUnknown);
	GFX_API void ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle);
	GFX_API void ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle);
	GFX_API RenderSurfaceHandle GetActiveRenderColorSurface(int index);
	GFX_API RenderSurfaceHandle GetActiveRenderDepthSurface();
	GFX_API void SetSurfaceFlags(RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags);

	GFX_API void UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace );
	GFX_API void UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace );
	GFX_API void UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace );
	GFX_API void UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags );
	GFX_API void DeleteTexture( TextureID texture );

	GFX_API PresentMode	GetPresentMode();

	GFX_API void	BeginFrame();
	GFX_API void	EndFrame();
	GFX_API void	PresentFrame();
	GFX_API bool	IsValidState();
	GFX_API bool	HandleInvalidState();
	GFX_API void	FinishRendering();

	// Immediate mode rendering
	GFX_API void	ImmediateVertex( float x, float y, float z );
	GFX_API void	ImmediateNormal( float x, float y, float z );
	GFX_API void	ImmediateColor( float r, float g, float b, float a );
	GFX_API void	ImmediateTexCoordAll( float x, float y, float z );
	GFX_API void	ImmediateTexCoord( int unit, float x, float y, float z );
	GFX_API void	ImmediateBegin( GfxPrimitiveType type );
	GFX_API	void	ImmediateEnd();

	GFX_API bool	CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 );
	GFX_API bool	ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY );
	GFX_API void	GrabIntoRenderTexture(RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height);

	GFX_API void	BeforeDrawCall( bool immediateMode );

	GFX_API bool	IsPositionRequiredForTexGen(int texStageIndex) const { return false; }
	GFX_API bool	IsNormalRequiredForTexGen(int texStageIndex) const { return false; }
	GFX_API bool	IsPositionRequiredForTexGen() const { return false; }
	GFX_API bool	IsNormalRequiredForTexGen() const { return false; }

	GFX_API void	DiscardContents (RenderSurfaceHandle& rs) {}

#if ENABLE_PROFILER
	GFX_API void	BeginProfileEvent (const char* name);
	GFX_API void	EndProfileEvent ();

	TimerQueriesD3D9& GetTimerQueries() {return m_TimerQueriesD3D9;}
	GFX_API GfxTimerQuery*	CreateTimerQuery();
	GFX_API void			DeleteTimerQuery(GfxTimerQuery* query);
	GFX_API void			BeginTimerQueries();
	GFX_API void			EndTimerQueries();
	#endif

	#if UNITY_EDITOR
	GFX_API void				SetAntiAliasFlag( bool aa );
	GFX_API void				DrawUserPrimitives( GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride );
	GFX_API int					GetCurrentTargetAA() const;
	GFX_API GfxDeviceWindow*	CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias );
	#endif

	GFX_API int	GetCurrentTargetWidth() const;
	GFX_API int	GetCurrentTargetHeight() const;
	GFX_API void SetCurrentTargetSize(int width, int height);
	GFX_API void SetCurrentWindowSize(int width, int height);

	GFX_API void* GetNativeGfxDevice();
	GFX_API void* GetNativeTexturePointer(TextureID id);
	GFX_API intptr_t CreateExternalTextureFromNative(intptr_t nativeTex);
	GFX_API void UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex);

	GFX_API void ResetDynamicResources();

	IDirect3DVertexBuffer9*	GetAllWhiteVertexStream();

	VertexDeclarations& GetVertexDecls() { return m_VertexDecls; }

	const DeviceStateD3D& GetState() const { return m_State; }
	DeviceStateD3D& GetState() { return m_State; }
	VertexShaderConstantCache& GetVertexShaderConstantCache() { return m_VSConstantCache; }
	PixelShaderConstantCache& GetPixelShaderConstantCache() { return m_PSConstantCache; }

	const VertexPipeConfig& GetVertexPipeConfig() const { return m_VertexConfig; }
	VertexPipeConfig& GetVertexPipeConfig() { return m_VertexConfig; }
	const VertexPipeDataD3D9& GetVertexPipeData() const { return m_VertexData; }
	VertexPipeDataD3D9& GetVertexPipeData() { return m_VertexData; }
	TexturesD3D9& GetTextures() { return m_Textures; }

	void PushEventQuery();

private:

	DeviceStateD3D		m_State;
	ImmediateModeD3D	m_Imm;
	VertexPipeConfig	m_VertexConfig;
	TransformState		m_TransformState;
	VertexPipeDataD3D9		m_VertexData;
	VertexPipePrevious	m_VertexPrevious;

	DeviceBlendStateD3D9*	m_CurrBlendState;
	DeviceDepthStateD3D9*	m_CurrDepthState;
	const DeviceStencilStateD3D9*	m_CurrStencilState;
	DeviceRasterState*		m_CurrRasterState;
	int						m_CurrTargetWidth;
	int						m_CurrTargetHeight;
	int						m_CurrWindowWidth;
	int						m_CurrWindowHeight;

	IDirect3DVertexBuffer9*	m_AllWhiteVertexStream;

	VertexDeclarations	m_VertexDecls;
	TexturesD3D9		m_Textures;
	DynamicVBO*			m_DynamicVBO;

	CachedBlendStates	m_CachedBlendStates;
	CachedDepthStates	m_CachedDepthStates;
	CachedStencilStates	m_CachedStencilStates;
	CachedRasterStates	m_CachedRasterStates;

	VertexShaderConstantCache	m_VSConstantCache;
	PixelShaderConstantCache	m_PSConstantCache;

#if ENABLE_PROFILER
	TimerQueriesD3D9 m_TimerQueriesD3D9;
#endif
};

GfxDeviceD3D9& GetD3D9GfxDevice();
