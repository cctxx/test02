#pragma once
#if ENABLE_MULTITHREADED_CODE

#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/TransformState.h"
#include "Runtime/GfxDevice/threaded/ClientIDMapper.h"
#include "Runtime/GfxDevice/threaded/ThreadedDeviceStates.h"
#include "Runtime/Utilities/dynamic_array.h"

class ThreadedStreamBuffer;
class GfxDeviceWorker;
class GfxDeviceWindow;
class ThreadedWindow;
class ThreadedDynamicVBO;
struct DisplayListContext;

enum
{
	kClientDeviceThreaded = 1 << 0,
	kClientDeviceForceRef = 1 << 1,
	kClientDeviceUseRealDevice = 1 << 2,
	kClientDeviceClientProcess = 1 << 3,
	kClientDeviceWorkerProcess = 1 << 4,
};

GfxDevice* CreateClientGfxDevice(GfxDeviceRenderer renderer, UInt32 flags, size_t bufferSize = 0, void *buffer=0);
bool GfxDeviceWorkerProcessRunCommand();

#define GFX_DEVICE_CLIENT_TRACK_TEXGEN (GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30)


class GfxDeviceClient : public GfxDevice
{
public:
	GfxDeviceClient(bool threaded, bool clientProcess, size_t bufferSize = 0, void *buffer=0);
	GFX_API ~GfxDeviceClient();

	GFX_API void	InvalidateState();
	#if GFX_DEVICE_VERIFY_ENABLE
	GFX_API void	VerifyState();
	#endif

	GFX_API void	SetMaxBufferedFrames (int bufferSize);

	GFX_API void	Clear (UInt32 clearFlags, const float color[4], float depth, int stencil);
	GFX_API void	SetUserBackfaceMode( bool enable );
	GFX_API void SetWireframe(bool wire);
	GFX_API bool GetWireframe() const;

	GFX_API void	SetInvertProjectionMatrix( bool enable );
	GFX_API bool	GetInvertProjectionMatrix() const;

	#if GFX_USES_VIEWPORT_OFFSET
	GFX_API void	SetViewportOffset( float x, float y );
	GFX_API void	GetViewportOffset( float &x, float &y ) const;
	#endif

	GFX_API GPUSkinningInfo *CreateGPUSkinningInfo();
	GFX_API void	DeleteGPUSkinningInfo(GPUSkinningInfo *info);
	GFX_API void	SkinOnGPU( GPUSkinningInfo * info, bool lastThisFrame );
	GFX_API void	UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty);
	GFX_API void	UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses);

	GFX_API DeviceBlendState* CreateBlendState(const GfxBlendState& state);
	GFX_API DeviceDepthState* CreateDepthState(const GfxDepthState& state);
	GFX_API DeviceStencilState* CreateStencilState(const GfxStencilState& state);
	GFX_API DeviceRasterState* CreateRasterState(const GfxRasterState& state);

	GFX_API void	RecordSetBlendState(const DeviceBlendState* state, const ShaderLab::FloatVal& alphaRef, const ShaderLab::PropertySheet* props);
	GFX_API void	SetBlendState(const DeviceBlendState* state, float alphaRef);
	GFX_API void	SetDepthState(const DeviceDepthState* state);
	GFX_API void	SetStencilState(const DeviceStencilState* state, int stencilRef);
	GFX_API void	SetRasterState(const DeviceRasterState* state);
	GFX_API void	SetSRGBWrite (const bool);
	GFX_API bool	GetSRGBWrite ();

	#if UNITY_XENON
	GFX_API void SetNullPixelShader();
	GFX_API void SetHiZEnable( const HiZstate hiz_enable );
	GFX_API void SetHiStencilState( const bool hiStencilEnable, const bool hiStencilWriteEnable, const int hiStencilRef, const CompareFunction cmpfunc );
	GFX_API void HiStencilFlush( const HiSflush flushtype );
	#endif

	GFX_API void	SetWorldMatrix( const float matrix[16] );
	GFX_API void	SetViewMatrix( const float matrix[16] );
	GFX_API void	SetProjectionMatrix(const Matrix4x4f& matrix);
	GFX_API void	GetMatrix( float outMatrix[16] ) const;

	GFX_API const float* GetWorldMatrix() const;
	GFX_API const float* GetViewMatrix() const;
	GFX_API const float* GetProjectionMatrix() const;
	GFX_API const float* GetDeviceProjectionMatrix() const;

	GFX_API void	SetInverseScale( float invScale );

	GFX_API void	SetNormalizationBackface( NormalizationMode mode, bool backface );

	GFX_API void	SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial );
	GFX_API void	RecordSetMaterial( const ShaderLab::VectorVal& ambient, const ShaderLab::VectorVal& diffuse, const ShaderLab::VectorVal& specular, const ShaderLab::VectorVal& emissive, const ShaderLab::FloatVal& shininess, const ShaderLab::PropertySheet* props );
	GFX_API void	SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess );
	GFX_API void	RecordSetColor( const ShaderLab::VectorVal& color, const ShaderLab::PropertySheet* props );
	GFX_API void	SetColor( const float color[4] );
	GFX_API void	SetViewport( int x, int y, int width, int height );
	GFX_API void	GetViewport( int* values ) const;

	GFX_API void	SetScissorRect( int x, int y, int width, int height );
	GFX_API void	DisableScissor();
	GFX_API bool	IsScissorEnabled() const;
	GFX_API void	GetScissorRect( int values[4] ) const;

	GFX_API TextureCombinersHandle CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular );
	GFX_API void	DeleteTextureCombiners( TextureCombinersHandle& textureCombiners );
	GFX_API void	SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props );

	GFX_API void	SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias);
	GFX_API void	SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace );
	GFX_API void	SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16]);
	GFX_API void	SetTextureName( TextureID texture, char const* name );

	GFX_API void	SetMaterialProperties(const MaterialPropertyBlock& block);

	GFX_API GpuProgram* CreateGpuProgram( const std::string& source, CreateGpuProgramOutput& output );
	GFX_API void	SetShadersMainThread( ShaderLab::SubProgram* programs[kShaderTypeCount], const ShaderLab::PropertySheet* props );
	GFX_API bool	IsShaderActive( ShaderType type ) const;
	GFX_API void	DestroySubProgram( ShaderLab::SubProgram* subprogram );
	GFX_API void	SetConstantBufferInfo (int id, int size);

	GFX_API void	DisableLights( int startLight );
	GFX_API void	SetLight( int light, const GfxVertexLight& data);
	GFX_API void	SetAmbient( const float ambient[4] );

	GFX_API void	RecordEnableFog( FogMode fogMode, const ShaderLab::FloatVal& fogStart, const ShaderLab::FloatVal& fogEnd, const ShaderLab::FloatVal& fogDensity, const ShaderLab::VectorVal& fogColor, const ShaderLab::PropertySheet* props );
	GFX_API void	EnableFog( const GfxFogParams& fog );
	GFX_API void	DisableFog();

	GFX_API VBO*	CreateVBO();
	GFX_API void	DeleteVBO( VBO* vbo );
	GFX_API DynamicVBO&	GetDynamicVBO();

	GFX_API void	BeginSkinning( int maxSkinCount );
	GFX_API bool	SkinMesh( const SkinMeshInfo& skin, VBO* vbo );
	GFX_API void	EndSkinning();

#if GFX_ENABLE_DRAW_CALL_BATCHING
	GFX_API	void	BeginStaticBatching(const ChannelAssigns& channels, GfxPrimitiveType topology);
	GFX_API	void	StaticBatchMesh( UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount );
	GFX_API	void	EndStaticBatching( VBO& vbo, const Matrix4x4f& matrix, TransformType transformType, int sourceChannels );

	GFX_API	void	BeginDynamicBatching( const ChannelAssigns& shaderChannel, UInt32 channelsInVBO, size_t maxVertices, size_t maxIndices, GfxPrimitiveType topology);
	GFX_API	void	DynamicBatchMesh( const Matrix4x4f& matrix, const VertexBufferData& vertices, UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount );
#if ENABLE_SPRITES
	GFX_API	void    DynamicBatchSprite(const Matrix4x4f* matrix, const SpriteRenderData* rd, ColorRGBA32 color);
#endif
	GFX_API	void	EndDynamicBatching( TransformType transformType );
#endif

	GFX_API	void	AddBatchingStats( int batchedTris, int batchedVerts, int batchedCalls );

#if UNITY_XENON
	GFX_API RawVBO*	CreateRawVBO( UInt32 size, UInt32 flags );
	GFX_API void	DeleteRawVBO( RawVBO* vbo );

	GFX_API void    EnablePersistDisplayOnQuit( bool enabled );
	GFX_API void    OnLastFrameCallback();

	GFX_API void    RegisterTexture2D( TextureID tid, IDirect3DBaseTexture9* texture );
	GFX_API void    PatchTexture2D( TextureID tid, IDirect3DBaseTexture9* texture );
	GFX_API void    DeleteTextureEntryOnly( TextureID textureID );
	GFX_API void    UnbindAndDelayReleaseTexture( IDirect3DBaseTexture9* texture );
	GFX_API void    SetTextureWrapModes( TextureID textureID, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW );

	GFX_API xenon::IVideoPlayer* CreateVideoPlayer(bool fullscreen);
	GFX_API void                 DeleteVideoPlayer(xenon::IVideoPlayer* player);
#endif

	GFX_API RenderSurfaceHandle CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags);
	GFX_API RenderSurfaceHandle CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags);
	GFX_API void DestroyRenderSurface (RenderSurfaceHandle& rs);
	GFX_API void DiscardContents (RenderSurfaceHandle& rs);
	GFX_API void IgnoreNextUnresolveOnCurrentRenderTarget();
	GFX_API void IgnoreNextUnresolveOnRS(RenderSurfaceHandle rs);
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face = kCubeFaceUnknown);
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, UInt32 flags);
	GFX_API void ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle);
	GFX_API void ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle);

	GFX_API RenderSurfaceHandle GetActiveRenderColorSurface (int index);
	GFX_API RenderSurfaceHandle GetActiveRenderDepthSurface ();

	GFX_API bool IsRenderTargetConfigValid(UInt32 width, UInt32 height, RenderTextureFormat colorFormat, DepthBufferFormat depthFormat);

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
	GFX_API void	ResetDynamicResources();
	GFX_API bool	IsReadyToBeginFrame();
	GFX_API void	FinishRendering();
	GFX_API UInt32	InsertCPUFence();
	GFX_API UInt32	GetNextCPUFence();
	GFX_API void	WaitOnCPUFence(UInt32 fence);

	GFX_API void	AcquireThreadOwnership();
	GFX_API void	ReleaseThreadOwnership();

	GFX_API void	ImmediateVertex( float x, float y, float z );
	GFX_API void	ImmediateNormal( float x, float y, float z );
	GFX_API void	ImmediateColor( float r, float g, float b, float a );
	GFX_API void	ImmediateTexCoordAll( float x, float y, float z );
	GFX_API void	ImmediateTexCoord( int unit, float x, float y, float z );
	GFX_API void	ImmediateBegin( GfxPrimitiveType type );
	GFX_API	void	ImmediateEnd();

	// Recording display lists
	GFX_API bool	BeginRecording();
	GFX_API bool	EndRecording( GfxDisplayList** outDisplayList );

	// Capturing screen shots / blits
	GFX_API bool	CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 );
	GFX_API bool	ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY );
	GFX_API void	GrabIntoRenderTexture (RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height);

	// Any housekeeping around draw calls
	GFX_API void	BeforeDrawCall( bool immediateMode );

	GFX_API bool	IsPositionRequiredForTexGen (int texStageIndex) const;
	GFX_API bool	IsNormalRequiredForTexGen (int texStageIndex) const;
	GFX_API bool	IsPositionRequiredForTexGen() const;
	GFX_API bool	IsNormalRequiredForTexGen() const;

	GFX_API void	SetActiveContext (void* ctx);

	GFX_API void	ResetFrameStats();
	GFX_API void	BeginFrameStats();
	GFX_API void	EndFrameStats();
	GFX_API void	SaveDrawStats();
	GFX_API void	RestoreDrawStats();
	GFX_API void	SynchronizeStats();

	GFX_API void* GetNativeGfxDevice();
	GFX_API void* GetNativeTexturePointer(TextureID id);
	GFX_API UInt32 GetNativeTextureID(TextureID id);
#if ENABLE_TEXTUREID_MAP
	GFX_API intptr_t CreateExternalTextureFromNative(intptr_t nativeTex);
	GFX_API void UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex);
#endif

	GFX_API void InsertCustomMarker (int marker);

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
	GFX_API void UpdateComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs, UInt32 cbDirty, size_t dataSize, const UInt8* data, const UInt32* cbSizes, const UInt32* cbOffsets, const int* bindPoints);
	GFX_API void UpdateComputeResources (
		unsigned texCount, const TextureID* textures, const int* texBindPoints,
		unsigned samplerCount, const unsigned* samplers,
		unsigned inBufferCount, const ComputeBufferID* inBuffers, const int* inBufferBindPoints,
		unsigned outBufferCount, const ComputeBufferID* outBuffers, const TextureID* outTextures, const UInt32* outBufferBindPoints);
	GFX_API void DispatchComputeProgram (ComputeProgramHandle cpHandle, unsigned threadsX, unsigned threadsY, unsigned threadsZ);

	GFX_API void DrawNullGeometry (GfxPrimitiveType topology, int vertexCount, int instanceCount);
	GFX_API void DrawNullGeometryIndirect (GfxPrimitiveType topology, ComputeBufferID bufferHandle, UInt32 bufferOffset);


#if ENABLE_PROFILER
	GFX_API void	BeginProfileEvent (const char* name);
	GFX_API void	EndProfileEvent ();
	GFX_API void	ProfileControl (GfxProfileControl ctrl, unsigned param);
	GFX_API GfxTimerQuery* CreateTimerQuery();
	GFX_API void	DeleteTimerQuery(GfxTimerQuery* query);
	GFX_API void	BeginTimerQueries();
	GFX_API void	EndTimerQueries();
#endif

	// Editor-only stuff
#if UNITY_EDITOR
	GFX_API void	SetAntiAliasFlag( bool aa );
	GFX_API void	DrawUserPrimitives( GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride );
	GFX_API int		GetCurrentTargetAA() const;
#endif

#if UNITY_EDITOR && UNITY_WIN
	//ToDo: This is windows specific code, we should replace HWND window with something more abstract
	GFX_API GfxDeviceWindow* CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias );

	void SetActiveWindow(ClientDeviceWindow* handle);
	void WindowReshape(ClientDeviceWindow* handle, int width, int height, DepthBufferFormat depthFormat, int antiAlias);
	void WindowDestroy(ClientDeviceWindow* handle);
	void BeginRendering(ClientDeviceWindow* handle);
	void EndRendering(ClientDeviceWindow* handle, bool presentContent);
#endif

#if UNITY_WIN
	GFX_API int		GetCurrentTargetWidth() const;
	GFX_API int		GetCurrentTargetHeight() const;
	GFX_API void	SetCurrentTargetSize(int width, int height);
	GFX_API void	SetCurrentWindowSize(int width, int height);
#endif

#if GFX_OPENGLESxx_ONLY || GFX_SUPPORTS_MOLEHILL
	GFX_API void ReloadResources() {};
#endif

	bool IsThreaded() const { return m_Threaded; }
	bool IsSerializing() const { return m_Serialize; }

	ThreadedStreamBuffer* GetCommandQueue() const { return m_CommandQueue; }
	GfxDeviceWorker* GetGfxDeviceWorker() const { return m_DeviceWorker; }

	void SetRealGfxDevice(GfxThreadableDevice* realDevice);
	void WriteBufferData(const void* data, int size);
	void ReadbackData(dynamic_array<UInt8>& data, const SInt32 chunkSize = 16384);
	void SubmitCommands();
	void DoLockstep();

	void QueryGraphicsCaps ();

	GFX_API RenderTextureFormat	GetDefaultRTFormat() const;
	GFX_API RenderTextureFormat	GetDefaultHDRRTFormat() const;

private:
	friend class ThreadedDisplayList;
	void UpdateFogDisabled();
	void UpdateFogEnabled(const GfxFogParams& fogParams);
	void UpdateShadersActive(bool shadersActive[kShaderTypeCount]);

private:
	void CreateShaderParameters( ShaderLab::SubProgram* program, FogMode fogMode );

	void CheckMainThread() const;
	void BeforeRenderTargetChange(int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle);
	void AfterRenderTargetChange();
	void WaitForPendingPresent();
	void WaitForSignal();

	typedef std::map< GfxBlendState, ClientDeviceBlendState, memcmp_less<GfxBlendState> > CachedBlendStates;
	typedef std::map< GfxDepthState, ClientDeviceDepthState, memcmp_less<GfxDepthState> > CachedDepthStates;
	typedef std::map< GfxStencilState, ClientDeviceStencilState, memcmp_less<GfxStencilState> > CachedStencilStates;
	typedef std::map< GfxRasterState, ClientDeviceRasterState, memcmp_less<GfxRasterState> > CachedRasterStates;

	GfxDeviceWorker*    m_DeviceWorker;
	GfxThreadableDevice* m_RealDevice;
	bool				m_Threaded;
	bool				m_Serialize;
	int					m_RecordDepth;
	int					m_MaxCallDepth;
	ThreadedStreamBuffer* m_CommandQueue;
	DisplayListContext*	m_DisplayListStack;
	DisplayListContext*	m_CurrentContext;
	DynamicVBO*			m_DynamicVBO;
	bool				m_InvertProjectionMatrix;
	#if GFX_USES_VIEWPORT_OFFSET
	Vector2f			m_ViewportOffset;
	#endif
	CachedBlendStates	m_CachedBlendStates;
	CachedDepthStates	m_CachedDepthStates;
	CachedStencilStates	m_CachedStencilStates;
	CachedRasterStates	m_CachedRasterStates;
	TransformState		m_TransformState;
	ClientDeviceRect	m_Viewport;
	ClientDeviceRect	m_ScissorRect;
	int					m_ScissorEnabled;
	RenderSurfaceHandle m_ActiveRenderColorSurfaces[kMaxSupportedRenderTargets];
	RenderSurfaceHandle m_ActiveRenderDepthSurface;
	int					m_CurrentTargetWidth;
	int					m_CurrentTargetHeight;
	int					m_CurrentWindowWidth;
	int					m_CurrentWindowHeight;
	int					m_ThreadOwnershipCount;
	UInt32				m_CurrentCPUFence;
	UInt32				m_PresentFrameID;
	bool				m_PresentPending;
	bool				m_Wireframe;
	bool				m_sRGBWrite;
	dynamic_array<UInt8> m_ReadbackData;

#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	// TEMP: we can actually grab this info from shader, but for now lets just workaround it with minimal impact
	// anyway, we should kill ffp (pretty please)
	// TODO: it might be needed on more platforms, but for now i care only about gles
	UInt8				posForTexGen;
	UInt8				nrmForTexGen;

	void				SetTexGen(int unit, TexGenMode mode);
	void				DropTexGen(int unit);
#endif

public:
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientIDMapper		m_BlendStateMapper;
	ClientIDMapper		m_DepthStateMapper;
	ClientIDMapper		m_StencilStateMapper;
	ClientIDMapper		m_RasterStateMapper;
	ClientIDMapper		m_TextureCombinerMapper;
	ClientIDMapper		m_VBOMapper;
	ClientIDMapper		m_RenderSurfaceMapper;
	ClientIDMapper		m_GpuProgramParametersMapper;
#endif
};

#endif
