#pragma once

#include "Configuration/UnityConfigure.h"
#include "GfxDeviceConfigure.h"
#include "GfxDeviceTypes.h"
#include "GfxDeviceObjects.h"
#include "GfxDeviceStats.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Threads/Thread.h"
#include "BuiltinShaderParams.h"
#include "Runtime/Modules/ExportModules.h"

#if UNITY_EDITOR && UNITY_WIN
#include "GfxDeviceWindow.h"
#endif

#if ENABLE_TEXTUREID_MAP
	#include "TextureIdMap.h"
#endif


// On some platforms we choose renderer at runtime; on others there's always a single
// renderer. On those that have only one, GFX_DEVICE_VIRTUAL is defined to zero, and
// the actual implementation is named GfxDevice, and uses no virtual functions.

#if UNITY_PS3
//#	define CELL_GCM_DEBUG
#	include <cell/gcm.h>
#endif

#if UNITY_WIN || UNITY_OSX || UNITY_LINUX || UNITY_PS3 || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_XENON || UNITY_BB10 || UNITY_WEBGL || UNITY_TIZEN
#define GFX_DEVICE_VIRTUAL 1
#else
#define GFX_DEVICE_VIRTUAL 0
#endif

#if (UNITY_OSX && WEBPLUG) || UNITY_LINUX && !UNITY_PEPPER
#define GFX_USES_VIEWPORT_OFFSET 1
#else
#define GFX_USES_VIEWPORT_OFFSET 0
#endif



#if GFX_DEVICE_VIRTUAL

#define GFX_API virtual
#define GFX_PURE = 0
#define GFX_GL_IMPL GfxDeviceGL

#else

#define GFX_API
#define GFX_PURE
#define GFX_GL_IMPL GfxDevice
struct GfxDeviceImpl;

#endif

#define GFX_DEVICE_VERIFY_ENABLE (!UNITY_RELEASE)

class VBO;
class RawVBO;
class VBOList;
class DynamicVBO;
class RenderTexture;
class ImageReference;
class Matrix4x4f;
class GpuProgram;
class GpuProgramParameters;
class GfxTimerQuery;
class GfxDisplayList;
class MaterialPropertyBlock;
class ShaderErrors;
class ChannelAssigns;
class CreateGpuProgramOutput;
struct SkinMeshInfo;
struct MemExportInfo;
struct VertexBufferData;
struct IndexBufferData;
struct PropertyNamesSet;
#if ENABLE_SPRITES
struct SpriteRenderData;
#endif
struct BoneInfluence;
namespace ShaderLab {
	class IntShader;
	struct ParserShader;
	struct TextureBinding;
	class PropertySheet;
	class SubProgram;
}
namespace xenon {
	class IVideoPlayer;
}

class GPUSkinningInfo;

class GfxDevice {
public:

	enum PresentMode
	{
		kPresentBeforeUpdate,
		kPresentAfterDraw
	};

	enum SurfaceFlags
	{
		kSurfaceDefault = 0,

		// Bits 0 and 1 are used to control render target restores. There flags are mutually exclusive.
		kSurfaceNeverRestore  = (1<<0), // Xbox 360: SetRenderTarget will never restore contents to EDRAM.
		kSurfaceAlwaysRestore = (1<<1), // Xbox 360: SetRenderTarget will always restore contents to EDRAM.
		kSurfaceRestoreMask   = kSurfaceNeverRestore | kSurfaceAlwaysRestore,
		// Xbox 360: SetRenderTarget by default will only restore contents to EDRAM if render target was previously used that frame.

		// next flag (1<<2)
	};

	enum ReloadResourcesFlags {
		kReleaseRenderTextures = (1<<0),
		kReloadShaders = (1<<1),
		kReloadTextures = (1<<2),
	};

	enum RenderTargetFlags {
		kFlagDontRestoreColor = (1<<0),	// Xbox 360 specific: do not restore old contents to EDRAM
		kFlagDontRestoreDepth = (1<<1),	// Xbox 360 specific: do not restore old contents to EDRAM
		kFlagDontRestore      = kFlagDontRestoreColor | kFlagDontRestoreDepth,
		kFlagForceResolve     = (1<<3), // Xbox 360 specific: force a resolve to system RAM
	};

	enum ImmediateShapeType {
		kShapeCube = 0,			// Quads
		kShapeDodecahedron		// Triangles
	};

	enum GfxProfileControl {
		kGfxProfBeginFrame = 0,
		kGfxProfEndFrame,
		kGfxProfDisableSampling,
		kGfxProfSetActive,
	};

	GfxDevice();
	GFX_API ~GfxDevice();

	GFX_API void	InvalidateState() GFX_PURE;
	#if GFX_DEVICE_VERIFY_ENABLE
	GFX_API void	VerifyState() GFX_PURE;
	#endif

	GfxDeviceRenderer GetRenderer() const { return m_Renderer; }
	// OpenGL: texture V coordinate is 0 at the bottom; 1 at the top
	// otherwise: texture V coordinate is 0 at the top; 1 at the bottom
	bool UsesOpenGLTextureCoords() const { return m_UsesOpenGLTextureCoords; }
	// Should half-texel offset be applied for pixel-correct rendering (true on D3D9)?
	bool UsesHalfTexelOffset() const { return m_UsesHalfTexelOffset; }

	GFX_API void SetMaxBufferedFrames (int bufferSize) { m_MaxBufferedFrames = bufferSize; }
	int GetMaxBufferedFrames () const { return m_MaxBufferedFrames; }

	const GfxDeviceStats& GetFrameStats() const { return m_Stats; }
	GfxDeviceStats& GetFrameStats() { return m_Stats; }

	RenderTexture* GetActiveRenderTexture() const
	{
#if !UNITY_EDITOR // TODO: this needs fixing in the editor
		ASSERT_RUNNING_ON_MAIN_THREAD;
#endif
		return m_ActiveRenderTexture;
	}
	void SetActiveRenderTexture(RenderTexture* rt)
	{
#if !UNITY_EDITOR // TODO: this needs fixing in the editor
		ASSERT_RUNNING_ON_MAIN_THREAD;
#endif
		m_ActiveRenderTexture = rt;
	}

	const BuiltinShaderParamValues& GetBuiltinParamValues() const { return m_BuiltinParamValues; }
	BuiltinShaderParamValues& GetBuiltinParamValues() { return m_BuiltinParamValues; }
	const GfxFogParams& GetFogParams() const { return m_FogParams; }

	static inline ColorRGBA32 ConvertToDeviceVertexColor(const ColorRGBA32& color)
	{
		#if GFX_OPENGLESxx_ONLY || UNITY_PS3 || UNITY_WII
			// Optimization: we know that we never have to swizzle vertex color here
			DebugAssert(gGraphicsCaps.needsToSwizzleVertexColors == false);
			return color;
		#elif UNITY_XENON
			// Optimization: we know that we always have to swizzle vertex color on Xbox360 platform
			DebugAssert(gGraphicsCaps.needsToSwizzleVertexColors == true);
			return color.SwizzleToARGB();
		#else
			return gGraphicsCaps.needsToSwizzleVertexColors ? color.SwizzleToBGRA() : color;
		#endif
	};

	int			GetTotalVBOCount() const;
	int			GetTotalVBOBytes() const;

	void		RecreateAllVBOs();

	#if GFX_SUPPORTS_D3D9
		void		ResetDynamicVBs();
	#endif
	#if GFX_SUPPORTS_OPENGLES20
		void		MarkAllVBOsLost();
	#endif


	GFX_API void	Clear (UInt32 clearFlags, const float color[4], float depth, int stencil) GFX_PURE;
	GFX_API void	SetInvertProjectionMatrix( bool enable ) GFX_PURE;
	GFX_API bool	GetInvertProjectionMatrix() const GFX_PURE;
	#if GFX_USES_VIEWPORT_OFFSET
	GFX_API void	SetViewportOffset( float x, float y ) GFX_PURE;
	GFX_API void	GetViewportOffset( float &x, float &y ) const GFX_PURE;
	#endif

	GFX_API DeviceBlendState* CreateBlendState(const GfxBlendState& state) GFX_PURE;
	GFX_API DeviceDepthState* CreateDepthState(const GfxDepthState& state) GFX_PURE;
	GFX_API DeviceStencilState* CreateStencilState(const GfxStencilState& state) GFX_PURE;
	GFX_API DeviceRasterState* CreateRasterState(const GfxRasterState& state) GFX_PURE;

	GFX_API void RecordSetBlendState(const DeviceBlendState* state, const ShaderLab::FloatVal& alphaRef, const ShaderLab::PropertySheet* props );
	GFX_API void SetBlendState(const DeviceBlendState* state, float alphaRef) GFX_PURE;
	GFX_API void SetRasterState(const DeviceRasterState* state) GFX_PURE;
	GFX_API void SetDepthState(const DeviceDepthState* state) GFX_PURE;
	GFX_API void SetStencilState(const DeviceStencilState* state, int stencilRef) GFX_PURE;
	GFX_API void SetSRGBWrite (const bool) GFX_PURE;
	GFX_API bool GetSRGBWrite () GFX_PURE;

	GFX_API void SetUserBackfaceMode(bool enable) GFX_PURE;
	GFX_API void SetWireframe(bool wire) GFX_PURE;
	GFX_API bool GetWireframe() const GFX_PURE;

	GFX_API void	SetWorldMatrixAndType( const float matrix[16], TransformType type );
	GFX_API void	SetWorldMatrix( const float matrix[16] ) GFX_PURE;
	GFX_API void	SetViewMatrix( const float matrix[16] ) GFX_PURE;
	GFX_API void	SetProjectionMatrix (const Matrix4x4f& matrix) GFX_PURE;

	GFX_API void	GetMatrix( float outMatrix[16] ) const GFX_PURE;


	GFX_API	const float* GetWorldMatrix() const GFX_PURE;
	GFX_API	const float* GetViewMatrix() const GFX_PURE;
	GFX_API	const float* GetProjectionMatrix() const GFX_PURE; // get projection matrix as passed from Unity (OpenGL projection conventions)
	GFX_API const float* GetDeviceProjectionMatrix() const GFX_PURE; // get projection matrix that will be actually used

	GFX_API void	SetInverseScale (float invScale);

	GFX_API void	SetNormalizationBackface( NormalizationMode mode, bool backface ) GFX_PURE;

	GFX_API void	SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial ) GFX_PURE;
	GFX_API void	RecordSetMaterial( const ShaderLab::VectorVal& ambient, const ShaderLab::VectorVal& diffuse, const ShaderLab::VectorVal& specular, const ShaderLab::VectorVal& emissive, const ShaderLab::FloatVal& shininess, const ShaderLab::PropertySheet* props );
	GFX_API void	SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess ) GFX_PURE;
	GFX_API void	RecordSetColor( const ShaderLab::VectorVal& color, const ShaderLab::PropertySheet* props );
	GFX_API void	SetColor( const float color[4] ) GFX_PURE;
	GFX_API void	SetViewport( int x, int y, int width, int height ) GFX_PURE;
	GFX_API void	GetViewport( int* values ) const GFX_PURE;

	GFX_API void	SetScissorRect( int x, int y, int width, int height ) GFX_PURE;
	GFX_API void	DisableScissor() GFX_PURE;
	GFX_API bool	IsScissorEnabled() const GFX_PURE;
	GFX_API void	GetScissorRect( int values[4] ) const GFX_PURE;

	GFX_API TextureCombinersHandle CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular ) GFX_PURE;
	GFX_API void	DeleteTextureCombiners( TextureCombinersHandle& textureCombiners ) GFX_PURE;
	GFX_API void	SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props ) GFX_PURE;

	GFX_API void	SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias) GFX_PURE;
	GFX_API void	SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace ) GFX_PURE;
	GFX_API void	SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16]) GFX_PURE;
	GFX_API void	SetTextureName( TextureID texture, char const* name ) GFX_PURE;

	GFX_API void	SetMaterialProperties(const MaterialPropertyBlock& block);

	GFX_API GpuProgram* CreateGpuProgram( const std::string& source, CreateGpuProgramOutput& output );
	GFX_API void	SetShadersMainThread( ShaderLab::SubProgram* programs[kShaderTypeCount], const ShaderLab::PropertySheet* props ) GFX_PURE;

	GFX_API bool	IsShaderActive( ShaderType type ) const GFX_PURE;
	GFX_API void	DestroySubProgram( ShaderLab::SubProgram* subprogram ) GFX_PURE;
	GFX_API void	SetConstantBufferInfo (int /*id*/, int /*size*/) { }

	GFX_API void	DisableLights( int startLight ) GFX_PURE;
	GFX_API void	SetLight( int light, const GfxVertexLight& data) GFX_PURE;
	GFX_API void	SetAmbient( const float ambient[4] ) GFX_PURE;

	GFX_API void	RecordEnableFog( FogMode fogMode, const ShaderLab::FloatVal& fogStart, const ShaderLab::FloatVal& fogEnd, const ShaderLab::FloatVal& fogDensity, const ShaderLab::VectorVal& fogColor, const ShaderLab::PropertySheet* props );
	GFX_API void	EnableFog( const GfxFogParams& fog ) GFX_PURE;
	GFX_API void	DisableFog() GFX_PURE;

	GFX_API VBO*	CreateVBO() GFX_PURE;
	GFX_API void	DeleteVBO( VBO* vbo ) GFX_PURE;
	GFX_API DynamicVBO&	GetDynamicVBO() GFX_PURE;

	GFX_API void	BeginSkinning( int maxSkinCount );
	GFX_API bool	SkinMesh( const SkinMeshInfo& skin, VBO* vbo );
	GFX_API void	EndSkinning();

#if GFX_ENABLE_DRAW_CALL_BATCHING
	static	int		GetMaxStaticBatchIndices();
	GFX_API	void	BeginStaticBatching(const ChannelAssigns& channels, GfxPrimitiveType topology);
	GFX_API	void	StaticBatchMesh( UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount );
	GFX_API	void	EndStaticBatching( VBO& vbo, const Matrix4x4f& matrix, TransformType transformType, int sourceChannels );

	GFX_API	void	BeginDynamicBatching( const ChannelAssigns& shaderChannels, UInt32 availableChannels, size_t maxVertices, size_t maxIndices, GfxPrimitiveType topology);
	GFX_API	void	DynamicBatchMesh( const Matrix4x4f& matrix, const VertexBufferData& vertices, UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount );
	GFX_API	void	EndDynamicBatching( TransformType transformType );
#if ENABLE_SPRITES
	GFX_API	void    DynamicBatchSprite(const Matrix4x4f* matrix, const SpriteRenderData* rd, ColorRGBA32 color);
#endif
#endif

	GFX_API	void	AddBatchingStats( int batchedTris, int batchedVerts, int batchedCalls );

	/**
	* CreateGPUSkinningInfo - Create a GPU-assisted skinning object.
	*
	* GPU-assisted skinning is limited to platforms that support GPU
	* writing the skinned output mesh into a VBO (StreamOut, MemExport, Transform Feedback).
	*
	* The returned object should be used against a single Skinned Mesh instance, and deleted once no longer needed.
	*
	* @return GFX_API GPUSkinningInfo* New GPU skinning object, or NULL if GPU skinning is not supported on this GfxDevice.
	*/
	GFX_API GPUSkinningInfo* CreateGPUSkinningInfo() GFX_PURE;

	/**
	* DeleteGPUSkinningInfo - Release a GPUSkinningInfo object.
	*
	* @param info GPUSkinningInfo object to delete
	*/
	GFX_API void DeleteGPUSkinningInfo(GPUSkinningInfo *info) GFX_PURE;

	/**
	* SkinOnGPU - Perform GPU-assisted skinning into the destination VBO.
	*
	* @param info GPUSkinningInfo object filled with valid data and set up using UpdateSkinData and UpdateSkinBones
	* @param lastThisFrame True if this is the last GPU-skinned mesh in this frame, false otherwise.
	*/
	GFX_API void	SkinOnGPU( GPUSkinningInfo* info, bool lastThisFrame ) GFX_PURE;

	/**
	* UpdateSkinSourceData - Pass the Vertex and skin data to the GPU.
	*
	* Assumes channel map, vertex count and stride has been previously set.
	* If dirty is true, the vertex or skin data has changed since the last call, and the implementation should refresh the content.
	* Otherwise, the implementation should just check that it has set up its internal buffers correctly and return.
	*
	* @param info GPUSkinningInfo object, expected to be properly set up with calls to setVertexCount, setChannelMap and setStride
	* @param vertData Vertex data, array size defined by previous calls to setVertexCount(), setChannelMap() and setStride
	* @param skinData Bone influence data, array size defined by setVertexCount
	* @param dirty Dirty flag, see above
	*/
	GFX_API void UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty) GFX_PURE;

	/**
	* UpdateSkinBonePoses - Update bone pose matrices for a GPU-assisted skin.
	*
	* Note that the array is not guaranteed to stay alive after the call, so unless
	* the data can be uploaded immediately, a local copy is required.
	*
	* @param boneCount Number of bones
	* @param poses Array of bone matrices
	*/
	GFX_API void UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses) GFX_PURE;


#if UNITY_XENON
	GFX_API RawVBO*	CreateRawVBO( UInt32 size, UInt32 flags ) GFX_PURE;
	GFX_API void	DeleteRawVBO( RawVBO* vbo ) GFX_PURE;
	GFX_API void    EnablePersistDisplayOnQuit( bool enabled ) GFX_PURE;

	GFX_API void    RegisterTexture2D( TextureID tid, IDirect3DBaseTexture9* texture ) GFX_PURE;
	GFX_API void    PatchTexture2D( TextureID tid, IDirect3DBaseTexture9* texture ) GFX_PURE;
	GFX_API void    DeleteTextureEntryOnly( TextureID textureID ) GFX_PURE;
	GFX_API void    UnbindAndDelayReleaseTexture( IDirect3DBaseTexture9* texture ) GFX_PURE;
	GFX_API void    SetTextureWrapModes( TextureID textureID, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW ) GFX_PURE;

	GFX_API void    OnLastFrameCallback() GFX_PURE;

	GFX_API xenon::IVideoPlayer* CreateVideoPlayer( bool fullscreen ) GFX_PURE;
	GFX_API void                 DeleteVideoPlayer( xenon::IVideoPlayer* player ) GFX_PURE;
	GFX_API void SetNullPixelShader() GFX_PURE;
	GFX_API void SetHiZEnable( const HiZstate hiz_enable ) GFX_PURE;
	GFX_API void SetHiStencilState( const bool hiStencilEnable, const bool hiStencilWriteEnable, const int hiStencilRef, const CompareFunction cmpFunc ) GFX_PURE;
	GFX_API void HiStencilFlush( const HiSflush flushtype ) GFX_PURE;
#endif

	GFX_API RenderSurfaceHandle CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags) GFX_PURE;
	GFX_API RenderSurfaceHandle CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags) GFX_PURE;
	GFX_API void DestroyRenderSurface (RenderSurfaceHandle& rs) GFX_PURE;
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel = 0, CubemapFace face = kCubeFaceUnknown) GFX_PURE;
	GFX_API void SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, UInt32 flags)
#if !UNITY_XENON
	{ SetRenderTargets(count, colorHandles, depthHandle, mipLevel, face); }
#else
	GFX_PURE
#endif
	;

	GFX_API void ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle) GFX_PURE;

	GFX_API void DiscardContents (RenderSurfaceHandle& rs) GFX_PURE;
	// Do not produce a warning for next unresolve of current RT; it is expected and there's nothing we can do about it
	GFX_API void IgnoreNextUnresolveOnCurrentRenderTarget() { }
	GFX_API void IgnoreNextUnresolveOnRS(RenderSurfaceHandle rs) { }

	GFX_API void ResolveDepthIntoTexture (RenderSurfaceHandle /*colorHandle*/, RenderSurfaceHandle /*depthHandle*/) { }

	GFX_API RenderSurfaceHandle GetActiveRenderColorSurface (int index) GFX_PURE;
	GFX_API RenderSurfaceHandle GetActiveRenderDepthSurface () GFX_PURE;

	// TODO: we might need to extend it in the future, e.g. for multi-display
	GFX_API RenderSurfaceHandle GetBackBufferColorSurface () 	{ return m_BackBufferColor; }
	GFX_API RenderSurfaceHandle GetBackBufferDepthSurface () 	{ return m_BackBufferDepth; }

	GFX_API void SetBackBufferColorSurface(RenderSurfaceBase* color)	{ m_BackBufferColor=RenderSurfaceHandle(color); }
	GFX_API void SetBackBufferDepthSurface(RenderSurfaceBase* depth)	{ m_BackBufferDepth=RenderSurfaceHandle(depth); }

	GFX_API bool IsRenderTargetConfigValid(UInt32 width, UInt32 height, RenderTextureFormat /*colorFormat*/, DepthBufferFormat /*depthFormat*/)
#if !UNITY_XENON
	{ return width <= gGraphicsCaps.maxRenderTextureSize && height <= gGraphicsCaps.maxRenderTextureSize; }
#else
	GFX_PURE
#endif
	;

	GFX_API void SetSurfaceFlags(RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags = 0) GFX_PURE;

	GFX_API TextureID	CreateTextureID();
	GFX_API void		FreeTextureID( TextureID texture );

#if ENABLE_TEXTUREID_MAP
	GFX_API intptr_t	CreateExternalTextureFromNative(intptr_t nativeTex) 				{ return nativeTex; }
	GFX_API void		UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex)	{ TextureIdMap::UpdateTexture(tex, nativeTex); }
#endif

	enum kUploadTextureFlags
	{
		kUploadTextureDefault = 0,
		kUploadTextureDontUseSubImage = 1<<0, // texture might not be created yet, or is being resized
		kUploadTextureOSDrawingCompatible = 1<<1, // create an OS-drawing compatible one (e.g. for GDI on Windows)
		// NOTE: Richard S added these on another Xbox360 branch, uncomment/merge when that branch comes back
		//kUploadTextureTiled = 1<<2,
		//kUploadTextureMemoryReady = 1<<3,
	};

	GFX_API void UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode,  TextureColorSpace colorSpace ) GFX_PURE;
	GFX_API void UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace ) GFX_PURE;
	GFX_API void UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace ) GFX_PURE;
	GFX_API void UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags ) GFX_PURE;
	GFX_API void DeleteTexture( TextureID texture ) GFX_PURE;

	GFX_API PresentMode	GetPresentMode() GFX_PURE;

	GFX_API void	BeginFrame() GFX_PURE;
	GFX_API void	EndFrame() GFX_PURE;
	inline bool		IsInsideFrame() const { return m_InsideFrame; }
	inline void		SetInsideFrame(bool v) { m_InsideFrame = v; }
	GFX_API void	PresentFrame() GFX_PURE;
	// Check if device is in valid state. E.g. lost device on D3D9; in this case all rendering
	// should be skipped.
	GFX_API bool	IsValidState() GFX_PURE;
	GFX_API bool	HandleInvalidState() { return true; }
	GFX_API void	ResetDynamicResources() {}
	GFX_API bool	IsReadyToBeginFrame() { return true; }

	// Fully finish any queued-up rendering (including on the GPU)
	GFX_API void	FinishRendering() GFX_PURE;
	// Insert CPU fence into command queue (if threaded)
	GFX_API UInt32	InsertCPUFence() { return 0; }
	// Get next CPU fence that will be inserted
	GFX_API UInt32	GetNextCPUFence() { return 0; }
	// Finish any threaded commands before CPU fence
	GFX_API void	WaitOnCPUFence(UInt32 /*fence*/) {}

	// Are we recording graphics commands?
	inline bool		IsRecording() const { return m_IsRecording; }

	// Does this device derive from GfxThreadableDevice?
	inline bool		IsThreadable() const { return m_IsThreadable; }

	// Acquire thread ownership on the calling thread. Worker releases ownership.
	GFX_API void	AcquireThreadOwnership() {}
	// Release thread ownership on the calling thread. Worker acquires ownership.
	GFX_API void	ReleaseThreadOwnership() {}

	// Immediate mode rendering
	GFX_API void	ImmediateShape( float x, float y, float z, float scale, ImmediateShapeType shape );
	GFX_API void	ImmediateVertex( float x, float y, float z ) GFX_PURE;
	GFX_API void	ImmediateNormal( float x, float y, float z ) GFX_PURE;
	GFX_API void	ImmediateColor( float r, float g, float b, float a ) GFX_PURE;
	GFX_API void	ImmediateTexCoordAll( float x, float y, float z ) GFX_PURE;
	GFX_API void	ImmediateTexCoord( int unit, float x, float y, float z ) GFX_PURE;
	GFX_API void	ImmediateBegin( GfxPrimitiveType type ) GFX_PURE;
	GFX_API	void	ImmediateEnd() GFX_PURE;

	// Recording display lists
#if GFX_SUPPORTS_DISPLAY_LISTS
	GFX_API bool	BeginRecording() { return false; }
	GFX_API bool	EndRecording( GfxDisplayList** outDisplayList ) { return false; }
#endif

	// Capturing screen shots / blits
	GFX_API bool	CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 ) GFX_PURE;
	GFX_API bool	ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY ) GFX_PURE;
	GFX_API void	GrabIntoRenderTexture (RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height) GFX_PURE;

	// Any housekeeping around draw calls
	GFX_API void	BeforeDrawCall( bool immediateMode ) GFX_PURE;
	GFX_API void	AfterDrawCall() {};

	GFX_API bool	IsPositionRequiredForTexGen (int texStageIndex) const GFX_PURE;
	GFX_API bool	IsNormalRequiredForTexGen (int texStageIndex) const GFX_PURE;
	GFX_API bool	IsPositionRequiredForTexGen() const GFX_PURE;
	GFX_API bool	IsNormalRequiredForTexGen() const GFX_PURE;

	GFX_API void	SetActiveContext (void* /*ctx*/) {};

	GFX_API void	ResetFrameStats();
	GFX_API void	BeginFrameStats();
	GFX_API void	EndFrameStats();
	GFX_API void	SaveDrawStats();
	GFX_API void	RestoreDrawStats();
	GFX_API void	SynchronizeStats();

	#if ENABLE_PROFILER
	GFX_API void	BeginProfileEvent (const char* /*name*/) {}
	GFX_API void	EndProfileEvent () {}
	GFX_API void	ProfileControl (GfxProfileControl /*ctrl*/, unsigned /*param*/) {}

	GFX_API GfxTimerQuery*		CreateTimerQuery() GFX_PURE;
	GFX_API void				DeleteTimerQuery(GfxTimerQuery* query) GFX_PURE;
	GFX_API void				BeginTimerQueries() GFX_PURE;
	GFX_API void				EndTimerQueries() GFX_PURE;
	GFX_API bool				TimerQueriesIsActive() { return false; }
	#endif

	// Editor-only stuff
	#if UNITY_EDITOR
	GFX_API void	SetColorBytes (const UInt8 color[4]);
	GFX_API void				SetAntiAliasFlag( bool aa ) GFX_PURE;
	GFX_API void				DrawUserPrimitives( GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride ) GFX_PURE;
	GFX_API int					GetCurrentTargetAA() const GFX_PURE;

	#if UNITY_WIN
		//ToDo: This is windows specific code, we should replace HWND window with something more abstract
		GFX_API GfxDeviceWindow*	CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias ) GFX_PURE;

	#endif
	#endif

	#if UNITY_WIN
		GFX_API int	GetCurrentTargetWidth() const { return 0; }
		GFX_API int	GetCurrentTargetHeight() const { return 0; }
		GFX_API void SetCurrentTargetSize(int /*width*/, int /*height*/) { }
		GFX_API void SetCurrentWindowSize(int /*width*/, int /*height*/) { }
	#endif

	static void CommonReloadResources( UInt32 flags );

	#if GFX_SUPPORTS_OPENGL
	GFX_API void UnbindObjects () {}
	#endif

	#if GFX_OPENGLESxx_ONLY || GFX_SUPPORTS_MOLEHILL
	GFX_API void ReloadResources() GFX_PURE;
	#endif

	#if !GFX_DEVICE_VIRTUAL
	GfxDeviceImpl*	GetImpl() { return impl; }
	#endif

#if GFX_DEVICE_VIRTUAL
	GFX_API RenderTextureFormat	GetDefaultRTFormat() const	{ return kRTFormatARGB32; }
#else
	GFX_API RenderTextureFormat	GetDefaultRTFormat() const;
#endif

#if GFX_DEVICE_VIRTUAL
	GFX_API RenderTextureFormat GetDefaultHDRRTFormat() const	{ return kRTFormatARGBHalf; }
#else
	GFX_API RenderTextureFormat	GetDefaultHDRRTFormat() const;
#endif

	GFX_API void* GetNativeGfxDevice() { return NULL; }
	GFX_API void* GetNativeTexturePointer(TextureID /*id*/) { return NULL; }
	GFX_API UInt32 GetNativeTextureID(TextureID id);
	GFX_API void InsertCustomMarker (int marker);

	GFX_API ComputeBufferID CreateComputeBufferID();
	GFX_API void FreeComputeBufferID(ComputeBufferID id);

	GFX_API void SetComputeBufferData (ComputeBufferID /*bufferHandle*/, const void* /*data*/, size_t /*size*/) { }
	GFX_API void GetComputeBufferData (ComputeBufferID /*bufferHandle*/, void* /*dest*/, size_t /*destSize*/) { }
	GFX_API void CopyComputeBufferCount (ComputeBufferID /*srcBuffer*/, ComputeBufferID /*dstBuffer*/, UInt32 /*dstOffset*/) { }

	GFX_API void SetRandomWriteTargetTexture (int /*index*/, TextureID /*tid*/) { }
	GFX_API void SetRandomWriteTargetBuffer (int /*index*/, ComputeBufferID /*bufferHandle*/) { }
	GFX_API void ClearRandomWriteTargets () { }

	GFX_API ComputeProgramHandle CreateComputeProgram (const UInt8* /*code*/, size_t /*codeSize*/) { ComputeProgramHandle cp; return cp; }
	GFX_API void DestroyComputeProgram (ComputeProgramHandle& /*cpHandle*/) { }
	GFX_API void CreateComputeConstantBuffers (unsigned /*count*/, const UInt32* /*sizes*/, ConstantBufferHandle* /*outCBs*/) { }
	GFX_API void DestroyComputeConstantBuffers (unsigned /*count*/, ConstantBufferHandle* /*cbs*/) { }
	GFX_API void CreateComputeBuffer (ComputeBufferID /*id*/, size_t /*count*/, size_t /*stride*/, UInt32 /*flags*/) { }
	GFX_API void DestroyComputeBuffer (ComputeBufferID /*handle*/) { }
	GFX_API void UpdateComputeConstantBuffers (unsigned /*count*/, ConstantBufferHandle* /*cbs*/, UInt32 /*cbDirty*/, size_t /*dataSize*/, const UInt8* /*data*/, const UInt32* /*cbSizes*/, const UInt32* /*cbOffsets*/, const int* /*bindPoints*/) { }
	GFX_API void UpdateComputeResources (
		unsigned /*texCount*/, const TextureID* /*textures*/, const int* /*texBindPoints*/,
		unsigned /*samplerCount*/, const unsigned* /*samplers*/,
		unsigned /*inBufferCount*/, const ComputeBufferID* /*inBuffers*/, const int* /*inBufferBindPoints*/,
		unsigned /*outBufferCount*/, const ComputeBufferID* /*outBuffers*/, const TextureID* /*outTextures*/, const UInt32* /*outBufferBindPoints*/) { }
	GFX_API void DispatchComputeProgram (ComputeProgramHandle /*cpHandle*/, unsigned /*threadsX*/, unsigned /*threadsY*/, unsigned /*threadsZ*/) { }

	GFX_API void DrawNullGeometry (GfxPrimitiveType /*topology*/, int /*vertexCount*/, int /*instanceCount*/) { };
	GFX_API void DrawNullGeometryIndirect (GfxPrimitiveType /*topology*/, ComputeBufferID /*bufferHandle*/, UInt32 /*bufferOffset*/) { };
	DepthBufferFormat GetFramebufferDepthFormat() const { return m_FramebufferDepthFormat; }
	void SetFramebufferDepthFormat(DepthBufferFormat depthFormat) { m_FramebufferDepthFormat = depthFormat; }

protected:
	void SetupVertexLightParams(int light, const GfxVertexLight& data);

private:

	#if !GFX_DEVICE_VIRTUAL
	GfxDeviceImpl*	impl;
	#endif

protected:
	void OnCreate();
	void OnDelete();
	void OnCreateVBO(VBO* vbo);
	void OnDeleteVBO(VBO* vbo);

	// Mutable state
	BuiltinShaderParamValues	m_BuiltinParamValues;
	GfxFogParams		m_FogParams;
	GfxDeviceStats		m_Stats;
	GfxDeviceStats		m_SavedStats;
	bool				m_InsideFrame;
	bool				m_IsRecording;
	bool				m_IsThreadable;
	RenderTexture*		m_ActiveRenderTexture;
	const BuiltinShaderParamIndices*	m_BuiltinParamIndices[kShaderTypeCount];
	BuiltinShaderParamIndices	m_NullParamIndices;
	MaterialPropertyBlock	m_MaterialProperties;

	// Immutable data
	GfxDeviceRenderer	m_Renderer;
	bool				m_UsesOpenGLTextureCoords;
	bool				m_UsesHalfTexelOffset;
	int					m_MaxBufferedFrames;
	DepthBufferFormat	m_FramebufferDepthFormat;


	RenderSurfaceHandle	m_BackBufferColor;
	RenderSurfaceHandle	m_BackBufferDepth;


private:

	VBOList*			m_VBOList;
	static volatile int	ms_TextureIDGenerator;
	static volatile int	ms_ComputeBufferIDGenerator;

	typedef std::map<TextureID, size_t> TextureIDToSizeMap;
	TextureIDToSizeMap m_TextureSizes;
};

class GfxThreadableDevice : public GfxDevice
{
public:
	//! Called by the worker thread on thread startup
	GFX_API void	OnDeviceCreated (bool /*callingFromRenderThread*/) { }

	//! IsCombineModeSupported() exists because we want CreateTextureCombiners() failure to happen
	//! on the main thread, not the render thread where we can't do anything about it.
	//! When it returns true then creating combiners *should* succeed
	GFX_API bool	IsCombineModeSupported( unsigned int combiner ) GFX_PURE;

	GFX_API void	SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors ) GFX_PURE;
	GFX_API void	SetShadersMainThread (ShaderLab::SubProgram* programs[kShaderTypeCount], const ShaderLab::PropertySheet* props);
	GFX_API void	SetShadersThreadable (GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount]) { };

	//! CreateShaderParameters() exists because the main thread needs to know which parameters a shader takes, and on
	//! GL-like platforms it can vary with different fog modes because we recompile shaders when the mode was changed.
	GFX_API void	CreateShaderParameters( ShaderLab::SubProgram* /*program*/, FogMode /*fogMode*/ ) { }
};

bool				IsGfxDevice();
EXPORT_COREMODULE GfxDevice& GetGfxDevice();
GfxDevice&			GetUncheckedGfxDevice();
void				SetGfxDevice(GfxDevice* device);
void				DestroyGfxDevice();

GfxDevice&			GetRealGfxDevice();
bool				IsRealGfxDeviceThreadOwner();
#if ENABLE_MULTITHREADED_CODE
void				SetRealGfxDevice(GfxDevice* device);
void				SetRealGfxDeviceThreadOwnership();
void				DestroyRealGfxDevice();
void				SetGfxThreadingMode(GfxThreadingMode mode);
GfxThreadingMode	GetGfxThreadingMode();
#endif

class AutoGfxDeviceAcquireThreadOwnership
{
public:
	AutoGfxDeviceAcquireThreadOwnership();
	~AutoGfxDeviceAcquireThreadOwnership();

private:
	bool m_WasOwner;
};

class AutoGfxDeviceBeginEndFrame
{
public:
	AutoGfxDeviceBeginEndFrame();
	~AutoGfxDeviceBeginEndFrame();

	void End();
	bool GetSuccess() const { return m_Success; }

private:
	bool m_Success;
	bool m_NeedsEndFrame;
};

void CalculateDeviceProjectionMatrix (Matrix4x4f& m, bool usesOpenGLTextureCoords, bool invertY);

inline AutoGfxDeviceAcquireThreadOwnership::AutoGfxDeviceAcquireThreadOwnership()
{
	m_WasOwner = IsRealGfxDeviceThreadOwner();
	if (!m_WasOwner)
		GetGfxDevice().AcquireThreadOwnership();
}

inline AutoGfxDeviceAcquireThreadOwnership::~AutoGfxDeviceAcquireThreadOwnership()
{
	if (!m_WasOwner)
		GetGfxDevice().ReleaseThreadOwnership();
}

inline AutoGfxDeviceBeginEndFrame::AutoGfxDeviceBeginEndFrame() :
m_Success(true), m_NeedsEndFrame(false)
{
	GfxDevice& device = GetGfxDevice();
	if (!device.IsInsideFrame())
	{
		device.BeginFrame();
		m_Success = device.IsValidState();
		m_NeedsEndFrame = true;
	}
}

inline AutoGfxDeviceBeginEndFrame::~AutoGfxDeviceBeginEndFrame()
{
	End();
}

inline void AutoGfxDeviceBeginEndFrame::End()
{
	if (m_NeedsEndFrame)
		GetGfxDevice().EndFrame();
	m_NeedsEndFrame = false;
}


inline DepthBufferFormat DepthBufferFormatFromBits(int bits)
{
	if( bits <= 0 )
		return kDepthFormatNone;
	else if( bits <= 16 )
		return kDepthFormat16;
	else
		return kDepthFormat24;
}

#if UNITY_EDITOR
extern VertexComponent kSuitableVertexComponentForChannel[];
#endif


class SetAndRestoreWireframeMode {
public:
	SetAndRestoreWireframeMode() {
		GfxDevice& device = GetGfxDevice();
		m_SavedWireframe = device.GetWireframe();
	}
	SetAndRestoreWireframeMode(bool wireframe) {
		GfxDevice& device = GetGfxDevice();
		m_SavedWireframe = device.GetWireframe();
		device.SetWireframe(wireframe);
	}
	~SetAndRestoreWireframeMode() {
		GfxDevice& device = GetGfxDevice();
		device.SetWireframe(m_SavedWireframe);
	}
private:
	bool m_SavedWireframe;
};

void ApplyTexEnvData (unsigned int texUnit, unsigned int samplerUnit, const TexEnvData& data);

