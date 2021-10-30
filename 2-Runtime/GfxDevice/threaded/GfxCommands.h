#pragma once

#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/GfxDevice/threaded/ThreadedDeviceStates.h"
#include "Runtime/GfxDevice/threaded/ClientIDMapper.h"

struct ClientDeviceVBO;


enum GfxCommand
{
	kGfxCmd_Unused = 10000,
	kGfxCmd_InvalidateState,
	kGfxCmd_VerifyState,
	kGfxCmd_SetMaxBufferedFrames,
	kGfxCmd_Clear,
	kGfxCmd_SetUserBackfaceMode,
	kGfxCmd_SetInvertProjectionMatrix,
	kGfxCmd_SetViewportOffset,
	kGfxCmd_CreateBlendState,
	kGfxCmd_CreateDepthState,
	kGfxCmd_CreateStencilState,
	kGfxCmd_CreateRasterState,
	kGfxCmd_SetBlendState,
	kGfxCmd_SetDepthState,
	kGfxCmd_SetStencilState,
	kGfxCmd_SetRasterState,
	kGfxCmd_SetSRGBState,
	kGfxCmd_SetWorldMatrix,
	kGfxCmd_SetViewMatrix,
	kGfxCmd_SetProjectionMatrix,
	kGfxCmd_SetInverseScale,
	kGfxCmd_SetNormalizationBackface,
	kGfxCmd_SetFFLighting,
	kGfxCmd_SetMaterial,
	kGfxCmd_SetColor,
	kGfxCmd_SetViewport,
	kGfxCmd_SetScissorRect,
	kGfxCmd_DisableScissor,
	kGfxCmd_CreateTextureCombiners,
	kGfxCmd_DeleteTextureCombiners,
	kGfxCmd_SetTextureCombiners,
	kGfxCmd_SetTexture,
	kGfxCmd_SetTextureParams,
	kGfxCmd_SetTextureTransform,
	kGfxCmd_SetTextureName,
	kGfxCmd_SetMaterialProperties,
	kGfxCmd_CreateGpuProgram,
	kGfxCmd_SetShaders,
	kGfxCmd_CreateShaderParameters,
	kGfxCmd_DestroySubProgram,
	kGfxCmd_SetConstantBufferInfo,
	kGfxCmd_DisableLights,
	kGfxCmd_SetLight,
	kGfxCmd_SetAmbient,
	kGfxCmd_EnableFog,
	kGfxCmd_DisableFog,
	kGfxCmd_BeginSkinning,
	kGfxCmd_SkinMesh,
	kGfxCmd_EndSkinning,
	kGfxCmd_BeginStaticBatching,
	kGfxCmd_StaticBatchMesh,
	kGfxCmd_EndStaticBatching,
	kGfxCmd_BeginDynamicBatching,
	kGfxCmd_DynamicBatchMesh,
#if ENABLE_SPRITES
	kGfxCmd_DynamicBatchSprite,
#endif
	kGfxCmd_EndDynamicBatching,
	kGfxCmd_AddBatchingStats,
	kGfxCmd_CreateRenderColorSurface,
	kGfxCmd_CreateRenderDepthSurface,
	kGfxCmd_DestroyRenderSurface,
	kGfxCmd_DiscardContents,
	kGfxCmd_SetRenderTarget,
	kGfxCmd_SetRenderTargetWithFlags,
	kGfxCmd_ResolveColorSurface,
	kGfxCmd_ResolveDepthIntoTexture,
	kGfxCmd_SetSurfaceFlags,
	kGfxCmd_UploadTexture2D,
	kGfxCmd_UploadTextureSubData2D,
	kGfxCmd_UploadTextureCube,
	kGfxCmd_UploadTexture3D,
	kGfxCmd_DeleteTexture,
	kGfxCmd_BeginFrame,
	kGfxCmd_EndFrame,
	kGfxCmd_PresentFrame,
	kGfxCmd_HandleInvalidState,
	kGfxCmd_FinishRendering,
	kGfxCmd_InsertCPUFence,
	kGfxCmd_ImmediateVertex,
	kGfxCmd_ImmediateNormal,
	kGfxCmd_ImmediateColor,
	kGfxCmd_ImmediateTexCoordAll,
	kGfxCmd_ImmediateTexCoord,
	kGfxCmd_ImmediateBegin,
	kGfxCmd_ImmediateEnd,
	kGfxCmd_CaptureScreenshot,
	kGfxCmd_ReadbackImage,
	kGfxCmd_GrabIntoRenderTexture,
	kGfxCmd_SetActiveContext,
	kGfxCmd_ResetFrameStats,
	kGfxCmd_BeginFrameStats,
	kGfxCmd_EndFrameStats,
	kGfxCmd_SaveDrawStats,
	kGfxCmd_RestoreDrawStats,
	kGfxCmd_SynchronizeStats,
	kGfxCmd_SetAntiAliasFlag,
	kGfxCmd_SetWireframe,
	kGfxCmd_DrawUserPrimitives,
	kGfxCmd_Quit,

	kGfxCmd_VBO_UpdateVertexData,
	kGfxCmd_VBO_UpdateIndexData,
	kGfxCmd_VBO_Draw,
	kGfxCmd_VBO_DrawCustomIndexed,
	kGfxCmd_VBO_Recreate,
	kGfxCmd_VBO_MapVertexStream,
	kGfxCmd_VBO_IsVertexBufferLost,
	kGfxCmd_VBO_SetVertexStreamMode,
	kGfxCmd_VBO_SetIndicesDynamic,
	kGfxCmd_VBO_GetRuntimeMemorySize,
	kGfxCmd_VBO_GetVertexSize,
	kGfxCmd_VBO_AddExtraUvChannels,
	kGfxCmd_VBO_CopyExtraUvChannels,
	kGfxCmd_VBO_Constructor,
	kGfxCmd_VBO_Destructor,
	kGfxCmd_VBO_UseAsStreamOutput,

	kGfxCmd_DynVBO_Chunk,
	kGfxCmd_DynVBO_DrawChunk,

	kGfxCmd_DisplayList_Call,
	kGfxCmd_DisplayList_End,

	kGfxCmd_CreateWindow,
	kGfxCmd_SetActiveWindow,
	kGfxCmd_WindowReshape,
	kGfxCmd_WindowDestroy,
	kGfxCmd_BeginRendering,
	kGfxCmd_EndRendering,

	kGfxCmd_AcquireThreadOwnership,
	kGfxCmd_ReleaseThreadOwnership,

	kGfxCmd_BeginProfileEvent,
	kGfxCmd_EndProfileEvent,
	kGfxCmd_ProfileControl,
	kGfxCmd_BeginTimerQueries,
	kGfxCmd_EndTimerQueries,

	kGfxCmd_TimerQuery_Constructor,
	kGfxCmd_TimerQuery_Destructor,
	kGfxCmd_TimerQuery_Measure,
	kGfxCmd_TimerQuery_GetElapsed,

	kGfxCmd_InsertCustomMarker,

	kGfxCmd_SetComputeBufferData,
	kGfxCmd_GetComputeBufferData,
	kGfxCmd_CopyComputeBufferCount,
	kGfxCmd_SetRandomWriteTargetTexture,
	kGfxCmd_SetRandomWriteTargetBuffer,
	kGfxCmd_ClearRandomWriteTargets,
	kGfxCmd_CreateComputeProgram,
	kGfxCmd_DestroyComputeProgram,
	kGfxCmd_CreateComputeConstantBuffers,
	kGfxCmd_DestroyComputeConstantBuffers,
	kGfxCmd_CreateComputeBuffer,
	kGfxCmd_DestroyComputeBuffer,
	kGfxCmd_UpdateComputeConstantBuffers,
	kGfxCmd_UpdateComputeResources,
	kGfxCmd_DispatchComputeProgram,
	kGfxCmd_DrawNullGeometry,
	kGfxCmd_DrawNullGeometryIndirect,
	kGfxCmd_QueryGraphicsCaps,
	kGfxCmd_SetGpuProgramParameters,
	kGfxCmd_DeleteGPUSkinningInfo,
	kGfxCmd_SkinOnGPU,
	kGfxCmd_UpdateSkinSourceData,
	kGfxCmd_UpdateSkinBonePoses,

	// Keep platform specific flags last
#if UNITY_XENON
	kGfxCmd_RawVBO_Constructor,
	kGfxCmd_RawVBO_Destructor,
	kGfxCmd_RawVBO_Next,
	kGfxCmd_RawVBO_Write,
	kGfxCmd_RawVBO_InvalidateGpuCache,

	kGfxCmd_EnablePersistDisplayOnQuit,
	kGfxCmd_OnLastFrameCallback,

	kGfxCmd_RegisterTexture2D,
	kGfxCmd_PatchTexture2D,
	kGfxCmd_DeleteTextureEntryOnly,
	kGfxCmd_UnbindAndDelayReleaseTexture,
	kGfxCmd_SetTextureWrapModes,

	kGfxCmd_VideoPlayer_Constructor,
	kGfxCmd_VideoPlayer_Destructor,
	kGfxCmd_VideoPlayer_Render,
	kGfxCmd_VideoPlayer_Pause,
	kGfxCmd_VideoPlayer_Resume,
	kGfxCmd_VideoPlayer_Stop,
	kGfxCmd_VideoPlayer_SetPlaySpeed,
	kGfxCmd_VideoPlayer_Play,
	kGfxCmd_SetHiStencilState,
	kGfxCmd_HiStencilFlush,
	kGfxCmd_SetNullPixelShader,
	kGfxCmd_EnableHiZ,
#endif

	kGfxCmd_Count
};

typedef int GfxCmdBool;


struct GfxCmdClear
{
	UInt32 clearFlags;
	Vector4f color;
	float depth;
	int stencil;
};

struct GfxCmdSetNormalizationBackface
{
	NormalizationMode mode;
	bool backface;
};

struct GfxCmdSetFFLighting
{
	bool on;
	bool separateSpecular;
	ColorMaterialMode colorMaterial;
};

struct GfxCmdCreateTextureCombiners
{
	int count;
	bool hasVertexColorOrLighting;
	bool usesAddSpecular;
};

struct GfxCmdSetTexture
{
	ShaderType shaderType;
	int unit;
	int samplerUnit;
	TextureID texture;
	TextureDimension dim;
	float bias;
};

struct GfxCmdSetTextureParams
{
	TextureID texture;
	TextureDimension texDim;
	TextureFilterMode filter;
	TextureWrapMode wrap;
	int anisoLevel;
	bool hasMipMap;
	TextureColorSpace colorSpace;
};

struct GfxCmdSetTextureName
{
	TextureID texture;
	int nameLength;
};

struct GfxCmdSetTextureBias
{
	int unit;
	float bias;
};

struct GfxCmdSetTextureTransform
{
	int unit;
	TextureDimension dim;
	TexGenMode texGen;
	bool identity;
	Matrix4x4f matrix;
};

struct GfxCmdSetMaterialProperties
{
	int propertyCount;
	int bufferSize;
};

#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
struct GfxCmdCreateGpuProgram
{
	const char *source;
	CreateGpuProgramOutput* output;
	GpuProgram** result;
};
#endif

struct GfxCmdSetShaders
{
	ClientIDWrapper(GpuProgram) programs[kShaderTypeCount];
	ClientIDWrapper(const GpuProgramParameters) params[kShaderTypeCount];
	int paramsBufferSize[kShaderTypeCount];
};

struct GfxCmdCreateShaderParameters
{
	ShaderLab::SubProgram* program;
	FogMode fogMode;
};

struct GfxCmdBeginStaticBatching
{
	ChannelAssigns channels;
	GfxPrimitiveType topology;
};

struct GfxCmdStaticBatchMesh
{
	UInt32 firstVertex;
	UInt32 vertexCount;
	IndexBufferData indices;
	UInt32 firstIndexByte;
	UInt32 indexCount;
};

struct GfxCmdEndStaticBatching
{
	ClientDeviceVBO* vbo;
	Matrix4x4f matrix;
	TransformType transformType;
	int sourceChannels;
};

struct GfxCmdBeginDynamicBatching
{
	ChannelAssigns shaderChannels;
	UInt32 channelsInVBO;
	size_t maxVertices;
	size_t maxIndices;
	GfxPrimitiveType topology;
};

struct GfxCmdDynamicBatchMesh
{
	Matrix4x4f matrix;
	VertexBufferData vertices;
	UInt32 firstVertex;
	UInt32 vertexCount;
	IndexBufferData indices;
	UInt32 firstIndexByte;
	UInt32 indexCount;
};

#if ENABLE_SPRITES
struct GfxCmdDynamicBatchSprite
{
	Matrix4x4f matrix;
	const SpriteRenderData *sprite;
	ColorRGBA32 color;
};
#endif
struct GfxCmdEndDynamicBatching
{
	TransformType transformType;
};

struct GfxCmdAddBatchingStats
{
	int batchedTris;
	int batchedVerts;
	int batchedCalls;
};

struct GfxCmdCreateRenderColorSurface
{
	TextureID textureID;
	int width;
	int height;
	int samples;
	int depth;
	TextureDimension dim;
	RenderTextureFormat format;
	UInt32 createFlags;
};

struct GfxCmdCreateRenderDepthSurface
{
	TextureID textureID;
	int width;
	int height;
	int samples;
	TextureDimension dim;
	DepthBufferFormat depthFormat;
	UInt32 createFlags;
};

struct GfxCmdSetRenderTarget
{
	ClientIDWrapperHandle(RenderSurfaceHandle) colorHandles[kMaxSupportedRenderTargets];
	ClientIDWrapperHandle(RenderSurfaceHandle) depthHandle;
	int			colorCount;
	int			mipLevel;
	CubemapFace face;
};

struct GfxCmdResolveDepthIntoTexture
{
	ClientIDWrapperHandle(RenderSurfaceHandle) colorHandle;
	ClientIDWrapperHandle(RenderSurfaceHandle) depthHandle;
};

struct GfxCmdSetSurfaceFlags
{
	ClientIDWrapperHandle(RenderSurfaceHandle) surf;
	UInt32 flags;
	UInt32 keepFlags;
};

struct GfxCmdUploadTexture2D
{
	TextureID texture;
	TextureDimension dimension;
	int srcSize;
	int width;
	int height;
	TextureFormat format;
	int mipCount;
	UInt32 uploadFlags;
	int skipMipLevels;
	TextureUsageMode usageMode;
	TextureColorSpace colorSpace;
};

struct GfxCmdUploadTextureSubData2D
{
	TextureID texture;
	int srcSize;
	int mipLevel;
	int x;
	int y;
	int width;
	int height;
	TextureFormat format;
	TextureColorSpace colorSpace;
};

struct GfxCmdUploadTextureCube
{
	TextureID texture;
	int srcSize;
	int faceDataSize;
	int size;
	TextureFormat format;
	int mipCount;
	UInt32 uploadFlags;
	TextureColorSpace colorSpace;
};

struct GfxCmdUploadTexture3D
{
	TextureID texture;
	int srcSize;
	int width;
	int height;
	int depth;
	TextureFormat format;
	int mipCount;
	UInt32 uploadFlags;
};

struct GfxCmdDrawUserPrimitives
{
	GfxPrimitiveType type;
	int vertexCount;
	UInt32 vertexChannels;
	int stride;
};

struct GfxCmdVBODraw
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	ChannelAssigns channels;
	UInt32 firstIndexByte;
	UInt32 indexCount;
	GfxPrimitiveType topology;
	UInt32 firstVertex;
	UInt32 vertexCount;
};

struct GfxCmdVBODrawCustomIndexed
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	ChannelAssigns channels;
	UInt32 indexCount;
	GfxPrimitiveType topology;
	UInt32 vertexRangeBegin;
	UInt32 vertexRangeEnd;
	UInt32 drawVertexCount;
};

struct GfxCmdVBODrawStripWireframe
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	ChannelAssigns channels;
	UInt32 indexCount;
	UInt32 triCount;
	UInt32 vertexCount;
};

struct GfxCmdVBOMapVertexStream
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	unsigned stream;
	UInt32 size;
};

struct GfxCmdVBOSetVertexStreamMode
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	unsigned stream;
	int mode;
};

struct GfxCmdVBOSetSetIndicesDynamic
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	int dynamic;
};

struct GfxCmdVBOAddExtraUvChannels
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO vbo;
#else
	ClientDeviceVBO* vbo;
#endif
	UInt32 size;
	int extraUvCount;
};

struct GfxCmdVBOCopyExtraUvChannels
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientDeviceVBO dest;
	ClientDeviceVBO source;
#else
	ClientDeviceVBO* dest;
	ClientDeviceVBO* source;
#endif
	
};


struct GfxCmdVector3
{
	float x;
	float y;
	float z;
};

struct GfxCmdVector4
{
	float x;
	float y;
	float z;
	float w;
};

struct GfxCmdImmediateTexCoord
{
	int unit;
	float x;
	float y;
	float z;
};

struct GfxCmdCaptureScreenshot
{
	int left;
	int bottom;
	int width;
	int height;
	UInt8* rgba32;
	bool* success;
};

struct GfxCmdReadbackImage
{
	ImageReference& image;
	int left;
	int bottom;
	int width;
	int height;
	int destX;
	int destY;
	bool* success;
};

struct GfxCmdGrabIntoRenderTexture
{
	ClientIDWrapperHandle(RenderSurfaceHandle) rs;
	ClientIDWrapperHandle(RenderSurfaceHandle) rd;
	int x;
	int y;
	int width;
	int height;
};

struct GfxCmdDynVboChunk
{
	UInt32 channelMask;
	UInt32 vertexStride;
	UInt32 actualVertices;
	UInt32 actualIndices;
	UInt32 renderMode;
};

#if UNITY_WIN && UNITY_EDITOR

struct GfxCmdWindowReshape
{
	int width;
	int height;
	DepthBufferFormat depthFormat;
	int antiAlias;
};

struct GfxCmdCreateWindow
{
	HWND window;
	int width;
	int height;
	DepthBufferFormat depthFormat;
	int antiAlias;
};

#endif
