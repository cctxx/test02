#pragma once

#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/TransformState.h"
#include "D3D11Includes.h"
#include "VertexDeclarationsD3D11.h"
#include "TexturesD3D11.h"
#include "FixedFunctionStateD3D11.h"
#include "ConstantBuffersD3D11.h"
#include <map>

class FixedFunctionProgramD3D11;
class DynamicD3D11VBO;


// TODO: optimize this. Right now we just send off whole 8 float3 UVs with each
// immediate mode vertex. We could at least detect the number of them used from
// ImmediateTexCoord calls.
struct ImmediateVertexD3D11 {
	Vector3f	vertex;
	Vector3f	normal;
	ColorRGBA32	color;
	Vector3f	texCoords[8];
};

struct ImmediateModeD3D11 {
	std::vector<ImmediateVertexD3D11>	m_Vertices;
	ImmediateVertexD3D11				m_Current;
	GfxPrimitiveType					m_Mode;
	ID3D11Buffer*						m_VB;
	int									m_VBUsedBytes;
	int									m_VBStartVertex;
	bool								m_HadColor;

	ImmediateModeD3D11();
	~ImmediateModeD3D11();
	void Cleanup();
	void Invalidate();
};

struct TextureUnitState11
{
	Matrix4x4f	matrix;
};



typedef std::map<FixedFunctionStateD3D11, FixedFunctionProgramD3D11*, FixedFuncStateCompareD3D11> FFProgramCacheD3D11;

struct ResolveTexturePool
{
	enum { kResolvePoolSize = 8 };
	struct Entry
	{
		// key
		int width;
		int height;
		RenderTextureFormat format;
		bool sRGB;

		// data
		ID3D11Texture2D* texture;
		ID3D11ShaderResourceView* srv;
		int	lastUse;
	};

	ResolveTexturePool();
	void Clear();

	Entry* GetResolveTexture (int width, int height, RenderTextureFormat fmt, bool sRGB);

	Entry	m_Entries[kResolvePoolSize];
	int		m_UseCounter;
};

class GfxDeviceD3D11 : public GfxThreadableDevice
{
public:
	struct DeviceBlendStateD3D11 : public DeviceBlendState
	{
		ID3D11BlendState* deviceState;
	};

	typedef std::map< GfxBlendState, DeviceBlendStateD3D11,  memcmp_less<GfxBlendState> > CachedBlendStates;
	typedef std::map< GfxDepthState, DeviceDepthState,  memcmp_less<GfxDepthState> > CachedDepthStates;
	typedef std::map< GfxStencilState, DeviceStencilState,  memcmp_less<GfxStencilState> > CachedStencilStates;
	typedef std::map< GfxRasterState, DeviceRasterState,  memcmp_less<GfxRasterState> > CachedRasterStates;

	struct DepthStencilState {
		DeviceDepthState d;
		DeviceStencilState s;
	};
	typedef std::map< DepthStencilState, ID3D11DepthStencilState*, memcmp_less<DepthStencilState> > CachedDepthStencilStates;

	struct FinalRasterState11 {
		DeviceRasterState raster;
		bool	backface;
		bool	wireframe;
		bool	scissor;
	};
	typedef std::map< FinalRasterState11, ID3D11RasterizerState*,  memcmp_less<FinalRasterState11> > CachedFinalRasterStates;

public:
	GfxDeviceD3D11();
	GFX_API ~GfxDeviceD3D11();

	GFX_API void	InvalidateState();
	#if GFX_DEVICE_VERIFY_ENABLE
	GFX_API void	VerifyState();
	#endif

	GFX_API void	Clear (UInt32 clearFlags, const float color[4], float depth, int stencil);
	GFX_API void	SetUserBackfaceMode( bool enable );
	GFX_API void SetWireframe (bool wire);
	GFX_API bool GetWireframe() const;
	GFX_API void	SetInvertProjectionMatrix( bool enable );
	GFX_API bool	GetInvertProjectionMatrix() const;

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

	/* GPU Skinning functions */
	GFX_API GPUSkinningInfo *CreateGPUSkinningInfo();
	GFX_API void	DeleteGPUSkinningInfo(GPUSkinningInfo *info);
	GFX_API void	SkinOnGPU( GPUSkinningInfo * info, bool lastThisFrame );
	GFX_API void	UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty);
	GFX_API void	UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses);

	GFX_API void	SetWorldMatrix( const float matrix[16] );
	GFX_API void	SetViewMatrix( const float matrix[16] );
	GFX_API void	SetProjectionMatrix (const Matrix4x4f& matrix);
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
	GFX_API void	SetConstantBufferInfo (int id, int size);

	GFX_API void	DisableLights( int startLight );
	GFX_API void	SetLight( int light, const GfxVertexLight& data);
	GFX_API void	SetAmbient( const float ambient[4] );

	GFX_API void	EnableFog (const GfxFogParams& fog);
	GFX_API void	DisableFog();

	GFX_API VBO*	CreateVBO();
	GFX_API void	DeleteVBO( VBO* vbo );
	GFX_API DynamicVBO&	GetDynamicVBO();

	GFX_API void DiscardContents (RenderSurfaceHandle& rs);

	GFX_API RenderSurfaceHandle CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags);
	GFX_API RenderSurfaceHandle CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags);
	GFX_API void DestroyRenderSurface (RenderSurfaceHandle& rs);
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face = kCubeFaceUnknown);
	GFX_API void ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle);
	GFX_API void ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle);
	GFX_API RenderSurfaceHandle GetActiveRenderColorSurface (int index);
	GFX_API RenderSurfaceHandle GetActiveRenderDepthSurface ();
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
	GFX_API void	GrabIntoRenderTexture( RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height );

	GFX_API void	BeforeDrawCall( bool immediateMode );

	GFX_API bool	IsPositionRequiredForTexGen (int texStageIndex) const { return false; }
	GFX_API bool	IsNormalRequiredForTexGen (int texStageIndex) const { return false; }
	GFX_API bool	IsPositionRequiredForTexGen() const { return false; }
	GFX_API bool	IsNormalRequiredForTexGen() const { return false; }

	#if ENABLE_PROFILER
	GFX_API void	BeginProfileEvent (const char* name);
	GFX_API void	EndProfileEvent ();

	GFX_API GfxTimerQuery*		CreateTimerQuery();
	GFX_API void				DeleteTimerQuery(GfxTimerQuery* query);
	GFX_API void				BeginTimerQueries();
	GFX_API void				EndTimerQueries();
	#endif // ENABLE_PROFILER

	#if UNITY_EDITOR
	GFX_API void				SetAntiAliasFlag (bool aa);
	GFX_API void				DrawUserPrimitives (GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride);
	GFX_API int					GetCurrentTargetAA() const;
	GFX_API GfxDeviceWindow*	CreateGfxWindow (HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias);
	#endif

	GFX_API int	GetCurrentTargetWidth() const;
	GFX_API int	GetCurrentTargetHeight() const;
	GFX_API void SetCurrentTargetSize(int width, int height);
	GFX_API void SetCurrentWindowSize(int width, int height);

	GFX_API void* GetNativeGfxDevice();
	GFX_API void* GetNativeTexturePointer(TextureID id);
	GFX_API intptr_t CreateExternalTextureFromNative(intptr_t nativeTex);
	GFX_API void UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex);

	GFX_API void SetComputeBufferData (ComputeBufferID bufferHandle, const void* data, size_t size);
	GFX_API void GetComputeBufferData (ComputeBufferID bufferHandle, void* dest, size_t destSize);
	GFX_API void CopyComputeBufferCount (ComputeBufferID srcBuffer, ComputeBufferID dstBuffer, UInt32 dstOffset);

	GFX_API void SetRandomWriteTargetTexture (int index, TextureID tid);
	GFX_API void SetRandomWriteTargetBuffer (int index, ComputeBufferID bufferHandle);
	GFX_API void ClearRandomWriteTargets ();

	GFX_API ComputeProgramHandle CreateComputeProgram (const UInt8* code, size_t codeSize);
	GFX_API void DestroyComputeProgram (ComputeProgramHandle& cpHandle);
	GFX_API void CreateComputeConstantBuffers (unsigned count, const UInt32* sizes, ConstantBufferHandle* outCBs);
	GFX_API void DestroyComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs);
	GFX_API void CreateComputeBuffer (ComputeBufferID id, size_t count, size_t stride, UInt32 flags);
	GFX_API void DestroyComputeBuffer (ComputeBufferID handle);
	GFX_API void UpdateComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs,  UInt32 cbDirty, size_t dataSize, const UInt8* data, const UInt32* cbSizes, const UInt32* cbOffsets, const int* bindPoints);
	GFX_API void UpdateComputeResources (
		unsigned texCount, const TextureID* textures, const int* texBindPoints,
		unsigned samplerCount, const unsigned* samplers,
		unsigned inBufferCount, const ComputeBufferID* inBuffers, const int* inBufferBindPoints,
		unsigned outBufferCount, const ComputeBufferID* outBuffers, const TextureID* outTextures, const UInt32* outBufferBindPoints);
	GFX_API void DispatchComputeProgram (ComputeProgramHandle cpHandle, unsigned threadsX, unsigned threadsY, unsigned threadsZ);

	GFX_API void DrawNullGeometry (GfxPrimitiveType topology, int vertexCount, int instanceCount);
	GFX_API void DrawNullGeometryIndirect (GfxPrimitiveType topology, ComputeBufferID bufferHandle, UInt32 bufferOffset);

	ID3D11Buffer* GetAllWhiteVertexStream();
	VertexDeclarationsD3D11& GetVertexDecls() { return m_VertexDecls; }
	ConstantBuffersD3D11& GetConstantBuffers() { return m_CBs; }
	TexturesD3D11& GetTextures() { return m_Textures; }
	void SetComputeBuffer11 (ShaderType shaderType, int unit, ComputeBufferID bufferHandle);

private:
	void SetupDeferredDepthStencilState();
	void SetupDeferredRasterState();
	void SetupDeferredSRGBWrite();

	void DrawQuad (float u0, float v0, float u1, float v1, float z, ID3D11ShaderResourceView* texture);
	bool ImmediateEndSetup();
	void ImmediateEndDraw();

public:
	ImmediateModeD3D11		m_Imm;
	TransformState			m_TransformState;

	ConstantBuffersD3D11	m_CBs;
	VertexDeclarationsD3D11	m_VertexDecls;
	TexturesD3D11			m_Textures;
	DynamicD3D11VBO*		m_DynamicVBO;

	FFProgramCacheD3D11		m_FFPrograms;
	FixedFunctionStateD3D11	m_FFState;

	CachedBlendStates		m_CachedBlendStates;
	CachedDepthStates		m_CachedDepthStates;
	CachedStencilStates		m_CachedStencilStates;
	CachedRasterStates			m_CachedRasterStates;
	CachedFinalRasterStates		m_CachedFinalRasterStates;
	CachedDepthStencilStates	m_CachedDepthStencilStates;

	TextureUnitState11	m_TextureUnits[kMaxSupportedTextureUnits];
	TextureID			m_ActiveTextures[kShaderTypeCount][kMaxSupportedTextureUnits];
	TextureID			m_ActiveSamplers[kShaderTypeCount][kMaxSupportedTextureUnits];

	GpuProgram* m_ActiveGpuProgram[kShaderTypeCount];
	const GpuProgramParameters* m_ActiveGpuProgramParams[kShaderTypeCount];

	void*	m_ActiveShaders[kShaderTypeCount];

	const DeviceBlendState*		m_CurrBlendState;
	const DeviceRasterState*	m_CurrRasterState;
	const DeviceDepthState*		m_CurrDepthState;
	const DeviceStencilState*	m_CurrStencilState;
	ID3D11RasterizerState*		m_CurrRSState;
	ID3D11DepthStencilState*	m_CurrDSState;
	int		m_StencilRef;
	int		m_CurrStencilRef;

	bool	m_InvertProjMatrix;
	bool	m_AppBackfaceMode;
	bool	m_UserBackfaceMode;
	bool	m_Wireframe;
	bool	m_Scissor;
	bool	m_SRGBWrite;
	bool	m_ActualSRGBWrite;

	int		m_Viewport[4];
	int		m_ScissorRect[4];
	int		m_CurrTargetWidth;
	int		m_CurrTargetHeight;
	int		m_CurrWindowWidth;
	int		m_CurrWindowHeight;

	ResolveTexturePool	m_Resolves;
};

GfxDeviceD3D11& GetD3D11GfxDevice();
