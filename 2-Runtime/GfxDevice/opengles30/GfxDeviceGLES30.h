#pragma once

#if GFX_DEVICE_VIRTUAL

#ifdef GFX_GL_IMPL
#undef GFX_GL_IMPL
#endif
#define GFX_GL_IMPL GfxDeviceGLES30

class GFX_GL_IMPL : public GfxThreadableDevice {
public:
	GFX_GL_IMPL();
	GFX_API ~GFX_GL_IMPL();

	GFX_API void	InvalidateState();
	#if GFX_DEVICE_VERIFY_ENABLE
	GFX_API void	VerifyState();
	#endif

	GFX_API void	Clear (UInt32 clearFlags, const float color[4], float depth, int stencil);
	GFX_API void	SetUserBackfaceMode( bool enable );
	GFX_API void SetWireframe(bool wire) { } // not possible in GLES
	GFX_API bool GetWireframe() const { return false; } // not possible in GLES
	GFX_API void	SetInvertProjectionMatrix( bool enable );
	GFX_API bool	GetInvertProjectionMatrix() const;

	GFX_API void	SetWorldMatrix( const float matrix[16] );
	GFX_API void	SetViewMatrix( const float matrix[16] );

	GFX_API void	SetProjectionMatrix (const Matrix4x4f& matrix);
	GFX_API void	GetMatrix( float outMatrix[16] ) const;

	GFX_API	const float* GetWorldMatrix() const ;
	GFX_API	const float* GetViewMatrix() const;
	GFX_API	const float* GetProjectionMatrix() const;
	GFX_API const float* GetDeviceProjectionMatrix() const;

	GFX_API void	SetNormalizationBackface( NormalizationMode mode, bool backface );
	GFX_API void	SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial );
	GFX_API void	SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess );
	GFX_API void	SetColor( const float color[4] );
	GFX_API void	SetViewport( int x, int y, int width, int height );
	GFX_API void	GetViewport( int* values ) const;

	GFX_API void	SetScissorRect( int x, int y, int width, int height );
	GFX_API void	DisableScissor();
	GFX_API bool	IsScissorEnabled() const;
	GFX_API void	GetScissorRect( int values[4] ) const;
	GFX_API void	SetSRGBWrite (const bool);
	GFX_API bool	GetSRGBWrite ();

	GFX_API void ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle);

	GFX_API TextureCombinersHandle CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular );
	GFX_API void	DeleteTextureCombiners( TextureCombinersHandle& textureCombiners );
	GFX_API void	SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props );

	GFX_API void	SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias);
	GFX_API void	SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace );
	GFX_API void	SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16]);
	GFX_API void	SetTextureName ( TextureID texture, const char* name ) { }

	GFX_API void	SetMaterialProperties( const MaterialPropertyBlock& block );

	GFX_API void	SetShadersThreadable (GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount]);
	GFX_API void	CreateShaderParameters( ShaderLab::SubProgram* program, FogMode fogMode );

	GFX_API bool	IsCombineModeSupported( unsigned int combiner );
	GFX_API void	SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors );

	GFX_API bool	IsShaderActive( ShaderType type ) const;
	GFX_API void	DestroySubProgram( ShaderLab::SubProgram* subprogram );
	GFX_API void	SetConstantBufferInfo( int id, int size );

	GFX_API void	DisableLights( int startLight );
	GFX_API void	SetLight( int light, const GfxVertexLight& data);
	GFX_API void	SetAmbient( const float ambient[4] );

	GFX_API void	EnableFog (const GfxFogParams& fog);
	GFX_API void	DisableFog();

	GFX_API VBO*	CreateVBO();
	GFX_API void	DeleteVBO( VBO* vbo );
	GFX_API DynamicVBO&	GetDynamicVBO();

	GFX_API RenderSurfaceHandle CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags);
	GFX_API RenderSurfaceHandle CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags);
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face = kCubeFaceUnknown);
	GFX_API void DestroyRenderSurface (RenderSurfaceHandle& rs);
	GFX_API void ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle);
	GFX_API RenderSurfaceHandle GetActiveRenderColorSurface (int index);
	GFX_API RenderSurfaceHandle GetActiveRenderDepthSurface ();
	GFX_API void SetSurfaceFlags(RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags);
	GFX_API void DiscardContents (RenderSurfaceHandle& rs);

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

	// Acquire thread ownership on the calling thread. Worker releases ownership.
	GFX_API void	AcquireThreadOwnership();
	// Release thread ownership on the calling thread. Worker acquires ownership.
	GFX_API void	ReleaseThreadOwnership();

	protected:

	GFX_API bool	CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 );
	GFX_API bool	ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY );
	GFX_API void	GrabIntoRenderTexture( RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height );

	GFX_API void	BeforeDrawCall( bool immediateMode);

	GFX_API void	SetBlendState(const DeviceBlendState* state, float alphaRef);
	GFX_API void	SetRasterState(const DeviceRasterState* state);
	GFX_API void	SetDepthState(const DeviceDepthState* state);
	GFX_API void	SetStencilState(const DeviceStencilState* state, int stencilRef);

	GFX_API DeviceBlendState* CreateBlendState(const GfxBlendState& state);
	GFX_API DeviceDepthState* CreateDepthState(const GfxDepthState& state);
	GFX_API DeviceStencilState* CreateStencilState(const GfxStencilState& state);
	GFX_API DeviceRasterState* CreateRasterState(const GfxRasterState& state);

	GFX_API GPUSkinningInfo *CreateGPUSkinningInfo();
	GFX_API void	DeleteGPUSkinningInfo(GPUSkinningInfo *info);
	GFX_API void	SkinOnGPU( GPUSkinningInfo * info, bool lastThisFrame );
	GFX_API void	UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty);
	GFX_API void	UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses);

#if ENABLE_PROFILER
	GFX_API void				BeginProfileEvent (const char* name);
	GFX_API void				EndProfileEvent ();

	GFX_API GfxTimerQuery*		CreateTimerQuery();
	GFX_API void				DeleteTimerQuery(GfxTimerQuery* query);
	GFX_API void				BeginTimerQueries();
	GFX_API void				EndTimerQueries();
#endif

public:
	// OpenGLES specific
	GFX_API void	ReloadResources();
	GFX_API bool	IsPositionRequiredForTexGen(int texStageIndex) const;
	GFX_API bool	IsNormalRequiredForTexGen(int texStageIndex) const;

	GFX_API bool	IsPositionRequiredForTexGen() const;
	GFX_API bool	IsNormalRequiredForTexGen() const;

	GFX_API void* GetNativeTexturePointer(TextureID id);

	DeviceStateGLES30&  GetState() { return state; }

private:
	DeviceStateGLES30				state;
};

#define STATE this->state
#define GetGLES30DeviceState(device) device.GetState()

#else // GFX_DEVICE_VIRTUAL


struct GfxDeviceImpl {
	DeviceStateGLES30	state;
};

#define STATE impl->state
#define GetGLES30DeviceState(device) device.GetImpl()->state

#endif // GFX_DEVICE_VIRTUAL
