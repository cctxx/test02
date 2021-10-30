#include "UnityPrefix.h"
#include "GfxDevice.h"
#include "Runtime/Utilities/HashFunctions.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Misc/Plugins.h"
#include "Runtime/Profiler/Profiler.h"
#include "ChannelAssigns.h"
#include "BatchRendering.h"
#include "GpuProgram.h"
#include "Runtime/Geometry/BoundingUtils.h"

#if ENABLE_TEXTUREID_MAP
	#include "TextureIdMap.h"
#endif

#if ENABLE_SPRITES
	#include "Runtime/Graphics/SpriteFrame.h"
#endif

class VBOList
{
public:
	List<VBO> m_List;
};

static GfxDevice* gfxDevice = NULL;

#if ENABLE_MULTITHREADED_CODE
static GfxDevice* realGfxDevice = NULL;
static Thread::ThreadID realGfxDeviceThreadId;
static GfxThreadingMode gfxThreadingMode = kGfxThreadingModeDirect;
#endif

void ApplyTexEnvData (unsigned int texUnit, unsigned int samplerUnit, const TexEnvData& data)
{
	GfxDevice& device = GetRealGfxDevice();

	device.SetTexture (kShaderFragment, texUnit, samplerUnit, data.textureID, static_cast<TextureDimension>(data.texDim), data.mipBias);
	// Only setup texture matrix & transform for texture units that fit into supported
	// coordinate count. Shaders can use more textures,
	// but then they can't have T&L matrices nor fixed function texgen.
	if (texUnit < kMaxSupportedTextureCoords)
	{
		device.SetTextureTransform (texUnit, static_cast<TextureDimension>(data.texDim), static_cast<TexGenMode>(data.texGen), data.identityMatrix, data.matrix.GetPtr());
	}
}

void ClearStaticBatchIndices();

bool IsGfxDevice()
{
	return gfxDevice != NULL;
}

GfxDevice& GetGfxDevice()
{
	Assert( gfxDevice );
#if ENABLE_MULTITHREADED_CODE
	DebugAssert(realGfxDevice == NULL || Thread::CurrentThreadIsMainThread());
#endif
	return *gfxDevice;
}

GfxDevice& GetUncheckedGfxDevice()
{
	return *gfxDevice;
}

void SetGfxDevice(GfxDevice* device)
{
	gfxDevice = device;
}

void DestroyGfxDevice()
{
	if (gfxDevice)
	{
		UNITY_DELETE(gfxDevice, kMemGfxDevice);
		gfxDevice = NULL;
	}
}

GfxDevice& GetRealGfxDevice()
{
#if ENABLE_MULTITHREADED_CODE
	if (realGfxDevice)
	{
		DebugAssert(Thread::EqualsCurrentThreadIDForAssert(realGfxDeviceThreadId));
		return *realGfxDevice;
	}
#endif
	return *gfxDevice;
}

bool IsRealGfxDeviceThreadOwner()
{
#if ENABLE_MULTITHREADED_CODE
	if (realGfxDevice)
		return Thread::EqualsCurrentThreadIDForAssert(realGfxDeviceThreadId);
#endif
	return true;
}

#if ENABLE_MULTITHREADED_CODE
void SetRealGfxDevice(GfxDevice* device)
{
	Assert( !realGfxDevice );
	realGfxDevice = device;
	SetRealGfxDeviceThreadOwnership();
}

void SetRealGfxDeviceThreadOwnership()
{
	realGfxDeviceThreadId = Thread::GetCurrentThreadID();
}

void DestroyRealGfxDevice()
{
	if (realGfxDevice)
	{
		UNITY_DELETE(realGfxDevice, kMemGfxThread);
		realGfxDevice = NULL;
	}
}

void SetGfxThreadingMode(GfxThreadingMode mode)
{
	gfxThreadingMode = mode;
}

GfxThreadingMode GetGfxThreadingMode()
{
	return gfxThreadingMode;
}
#endif

#if GFX_DEVICE_VIRTUAL
GfxDevice::GfxDevice()
{
	OnCreate();
}

GfxDevice::~GfxDevice()
{
	OnDelete();
}
#endif

void GfxDevice::OnCreate()
{
	m_Stats.ResetFrame();
	m_SavedStats.ResetFrame();
	m_ActiveRenderTexture = NULL;
	m_InsideFrame = false;
	m_IsRecording = false;
	m_IsThreadable = false;
	m_FramebufferDepthFormat = kDepthFormatNone;
	for (int i = 0; i < kShaderTypeCount; ++i)
		m_BuiltinParamIndices[i] = &m_NullParamIndices;
	m_VBOList = new VBOList;

#if ENABLE_TEXTUREID_MAP
	TextureIdMap::Initialize();
#endif
}

void GfxDevice::OnDelete()
{
	delete m_VBOList;

#if ENABLE_TEXTUREID_MAP
	TextureIdMap::Uninitialize();
#endif

	ClearStaticBatchIndices();
}

void GfxDevice::OnCreateVBO(VBO* vbo)
{
	SET_ALLOC_OWNER(this);
	m_VBOList->m_List.push_back(*vbo);
}

void GfxDevice::OnDeleteVBO(VBO* vbo)
{
	m_VBOList->m_List.erase(vbo);
}

int GfxDevice::GetTotalVBOCount() const
{
	int count = 0;
	List<VBO>::iterator itr, end = m_VBOList->m_List.end();
	for (itr = m_VBOList->m_List.begin(); itr != end; ++itr)
	{
		if (!itr->GetHideFromRuntimeStats())
			++count;
	}
	return count;
}

int GfxDevice::GetTotalVBOBytes() const
{
	int size = 0;
	List<VBO>::iterator itr, end = m_VBOList->m_List.end();
	for (itr = m_VBOList->m_List.begin(); itr != end; ++itr)
	{
		if (!itr->GetHideFromRuntimeStats())
			size += itr->GetRuntimeMemorySize();
	}
	return size;
}

void GfxDevice::RecreateAllVBOs()
{
	List<VBO>::iterator itr, end = m_VBOList->m_List.end();
	for (itr = m_VBOList->m_List.begin() ; itr != end; ++itr)
	{
		itr->Recreate();
	}
	GetDynamicVBO().Recreate();
}

#if GFX_SUPPORTS_D3D9
void GfxDevice::ResetDynamicVBs()
{
	List<VBO>::iterator itr, end = m_VBOList->m_List.end();
	for (itr = m_VBOList->m_List.begin() ; itr != end; ++itr)
	{
		itr->ResetDynamicVB();
	}
}
#endif

#if GFX_SUPPORTS_OPENGLES20
void GfxDevice::MarkAllVBOsLost()
{
	for(List<VBO>::iterator itr = m_VBOList->m_List.begin(), end = m_VBOList->m_List.end() ; itr != end; ++itr)
		itr->MarkBuffersLost();
}
#endif

void GfxDevice::SetWorldMatrixAndType( const float matrix[16], TransformType type )
{
	SetWorldMatrix(matrix);
	bool backface = (type & kOddNegativeScaleTransform) != 0;
	int normalization = (type & kUniformScaleTransform) ? kNormalizationScale : 0;
	normalization |= (type & kNonUniformScaleTransform) ? kNormalizationFull : 0;
	DebugAssert(normalization != (kNormalizationScale|kNormalizationFull));
	SetNormalizationBackface(NormalizationMode(normalization), backface);
}

void GfxDevice::SetInverseScale (float invScale)
{
	m_BuiltinParamValues.SetInstanceVectorParam(kShaderInstanceVecScale, Vector4f(0,0,0, invScale));
}

GpuProgram* GfxDevice::CreateGpuProgram( const std::string& source, CreateGpuProgramOutput& output )
{
	return ::CreateGpuProgram( source, output );
}

void GfxDevice::RecordSetBlendState(const DeviceBlendState* state, const ShaderLab::FloatVal& alphaRef, const ShaderLab::PropertySheet* props )
{
	ErrorString("GfxDevice does not support recording");
}

void GfxDevice::RecordSetMaterial( const ShaderLab::VectorVal& ambient, const ShaderLab::VectorVal& diffuse, const ShaderLab::VectorVal& specular, const ShaderLab::VectorVal& emissive, const ShaderLab::FloatVal& shininess, const ShaderLab::PropertySheet* props )
{
	ErrorString("GfxDevice does not support recording");
}

void GfxDevice::RecordSetColor( const ShaderLab::VectorVal& color, const ShaderLab::PropertySheet* props )
{
	ErrorString("GfxDevice does not support recording");
}

void GfxDevice::RecordEnableFog( FogMode fogMode, const ShaderLab::FloatVal& fogStart, const ShaderLab::FloatVal& fogEnd, const ShaderLab::FloatVal& fogDensity, const ShaderLab::VectorVal& fogColor, const ShaderLab::PropertySheet* props )
{
	ErrorString("GfxDevice does not support recording");
}

void GfxDevice::SetMaterialProperties(const MaterialPropertyBlock& block)
{
	m_MaterialProperties = block;
}



struct SkinMeshTask
{
	SkinMeshInfo info;
	VBO* vbo;
	void* vboMemory;
};

#if ENABLE_MULTITHREADED_SKINNING
static JobScheduler::JobGroupID s_SkinJobGroup;
#endif
static dynamic_array<SkinMeshTask> s_ActiveSkins;
static bool s_InsideSkinning = false;

void EndSkinTask(SkinMeshTask& task)
{
	if (task.vbo)
		task.vbo->UnmapVertexStream(0);
	task.info.Release();
}

void GfxDevice::BeginSkinning( int maxSkinCount )
{
	Assert(!s_InsideSkinning);
#if ENABLE_MULTITHREADED_SKINNING
	s_SkinJobGroup = GetJobScheduler().BeginGroup(maxSkinCount);
#endif
	s_ActiveSkins.reserve(maxSkinCount);
	s_InsideSkinning = true;
}

bool GfxDevice::SkinMesh( const SkinMeshInfo& skin, VBO* vbo )
{
	Assert(s_InsideSkinning);
	Assert((vbo == NULL) != (skin.outVertices == NULL));
	VertexStreamData mappedVSD;
	if (vbo && !vbo->MapVertexStream(mappedVSD, 0))
	{
		// Bail out before we push to active skins
		skin.Release();
		return false;
	}

	// Array must be preallocated to at least the right size
	Assert(s_ActiveSkins.size() < s_ActiveSkins.capacity());
	int skinIndex = s_ActiveSkins.size();
	s_ActiveSkins.resize_uninitialized(skinIndex + 1);
	SkinMeshTask& task = s_ActiveSkins[skinIndex];
	task.info = skin;
	task.vbo = vbo;

	// Caller passes in a buffer if it wants to read it back
	// Otherwise skin directly to VBO memory
	if (vbo)
		task.info.outVertices = mappedVSD.buffer;

#if ENABLE_MULTITHREADED_SKINNING
	GetJobScheduler().SubmitJob(s_SkinJobGroup, DeformSkinnedMeshJob, &task.info, NULL);
#else
	DeformSkinnedMesh(task.info);
	EndSkinTask(task);
#endif
	return true;
}

void GfxDevice::EndSkinning()
{
	Assert(s_InsideSkinning);
#if ENABLE_MULTITHREADED_SKINNING
	GetJobScheduler().WaitForGroup(s_SkinJobGroup);
	for (int i = 0; i < s_ActiveSkins.size(); i++)
		EndSkinTask(s_ActiveSkins[i]);
#endif
	s_ActiveSkins.resize_uninitialized(0);
	s_InsideSkinning = false;
}

#if GFX_ENABLE_DRAW_CALL_BATCHING
struct StaticBatch
{
#if GFX_SUPPORTS_OPENGLES20 && GFX_OPENGLESxx_ONLY
	enum { kMaxIndexCount = 16384 * 3 };
#else // everything else
	// Anything over 32k causes a slowdown on MBPs with AMD cards (Case 394520)
	enum { kMaxIndexCount = 32000 };
#endif
	~StaticBatch() { UNITY_FREE( kMemBatchedGeometry, indices); }
	bool isActive;
	ABSOLUTE_TIME startTime;
	ChannelAssigns channels;
	size_t indexCount;
	size_t vertexCount;
	size_t meshCount;
	GfxPrimitiveType topology;
	size_t vertexRangeBegin;
	size_t vertexRangeEnd;
	UInt16* indices;
} s_StaticBatch;

void ClearStaticBatchIndices()
{
	UNITY_FREE(kMemBatchedGeometry,s_StaticBatch.indices);
	s_StaticBatch.indices = NULL;
}

int GfxDevice::GetMaxStaticBatchIndices()
{
	return StaticBatch::kMaxIndexCount;
}

void GfxDevice::BeginStaticBatching (const ChannelAssigns& channels, GfxPrimitiveType topology)
{
	Assert(!s_StaticBatch.isActive);
	Assert(topology==kPrimitiveTriangles || topology==kPrimitiveTriangleStripDeprecated);

	StaticBatch& batch = s_StaticBatch;
	if (!batch.indices)
	{
		const size_t ibSize = StaticBatch::kMaxIndexCount * kVBOIndexSize;
		batch.indices = reinterpret_cast<UInt16*>(UNITY_MALLOC_ALIGNED( kMemBatchedGeometry, ibSize, 32));
	}
	batch.startTime = START_TIME;
	batch.channels = channels;
	batch.indexCount = 0;
	batch.vertexCount = 0;
	batch.meshCount = 0;
	batch.topology = topology;
	batch.vertexRangeBegin = std::numeric_limits<size_t>::max();
	batch.vertexRangeEnd = 0;
	batch.isActive = true;
}

void GfxDevice::StaticBatchMesh( UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount )
{
	Assert(s_StaticBatch.isActive);

	StaticBatch& batch = s_StaticBatch;
	batch.vertexCount += vertexCount;
	batch.vertexRangeBegin = std::min<size_t>(batch.vertexRangeBegin, firstVertex);
	batch.vertexRangeEnd = std::max<size_t>(batch.vertexRangeEnd, firstVertex + vertexCount);
	const UInt16* srcIndices = reinterpret_cast<const UInt16*>(static_cast<const UInt8*>(indices.indices) + firstIndexByte);
	AppendMeshIndices(batch.indices, batch.indexCount, srcIndices, indexCount, batch.topology==kPrimitiveTriangleStripDeprecated);
	batch.meshCount++;
}

void GfxDevice::EndStaticBatching( VBO& vbo, const Matrix4x4f& matrix, TransformType transformType, int sourceChannels )
{
	Assert(s_StaticBatch.isActive);

	SetWorldMatrixAndType(matrix.GetPtr(), transformType);
	const StaticBatch& batch = s_StaticBatch;
	vbo.DrawCustomIndexed(batch.channels, batch.indices, batch.indexCount, batch.topology,
	batch.vertexRangeBegin, batch.vertexRangeEnd, batch.vertexCount);

	ABSOLUTE_TIME elapsedTime = ELAPSED_TIME(batch.startTime);
	int primCount = GetPrimitiveCount(batch.indexCount, batch.topology, false);
	GetFrameStats().AddBatch(primCount, batch.vertexCount, batch.meshCount, elapsedTime);
	s_StaticBatch.isActive = false;
}

struct DynamicBatch
{
	bool isActive;
	ABSOLUTE_TIME startTime;
	ChannelAssigns shaderChannels;
	UInt32 availableChannels;
	size_t maxVertices;
	size_t maxIndices;
	size_t vertexCount;
	size_t indexCount;
	size_t meshCount;
	GfxPrimitiveType topology;
	size_t destStride;
	UInt8* outVertices;
	UInt16* outIndices;
} s_DynamicBatch;

void GfxDevice::BeginDynamicBatching( const ChannelAssigns& shaderChannels, UInt32 availableChannels, size_t maxVertices, size_t maxIndices, GfxPrimitiveType topology)
{
	Assert(!s_DynamicBatch.isActive);
	Assert(topology != kPrimitiveLineStrip);

	DynamicBatch& batch = s_DynamicBatch;
	batch.startTime = START_TIME;
	batch.shaderChannels = shaderChannels;
	batch.availableChannels = availableChannels;
	batch.maxVertices = maxVertices;
	batch.maxIndices = (topology == kPrimitiveQuads) ? maxIndices/4*6 : maxIndices;
	batch.vertexCount = 0;
	batch.indexCount = 0;
	batch.meshCount = 0;
	batch.topology = topology;

	batch.destStride = 0;
	for( int i = 0; i < kShaderChannelCount; ++i )
		if( availableChannels & (1<<i) )
			batch.destStride += VBO::GetDefaultChannelByteSize(i);

	DynamicVBO::RenderMode renderMode;
	switch (topology)
	{
		case kPrimitiveTriangleStripDeprecated:
			renderMode =  DynamicVBO::kDrawIndexedTriangleStrip;
		break;
		case kPrimitiveLines:
			renderMode = DynamicVBO::kDrawIndexedLines;
		break;
		case kPrimitiveQuads:
			renderMode = DynamicVBO::kDrawIndexedQuads;
		break;
		case kPrimitivePoints:
			renderMode = DynamicVBO::kDrawIndexedPoints;
		break;
		default:
			renderMode = DynamicVBO::kDrawIndexedTriangles;
		break;
	}

	// Get VBO chunk
	batch.isActive = GetDynamicVBO().GetChunk(
		availableChannels, maxVertices, batch.maxIndices, renderMode,
		(void**)&batch.outVertices, (void**)&batch.outIndices);
}

void GfxDevice::DynamicBatchMesh( const Matrix4x4f& matrix, const VertexBufferData& vertices, UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount )
{
	Assert(s_DynamicBatch.isActive);
	DynamicBatch& batch = s_DynamicBatch;
	size_t outIndexCount;

	// convert quad indices to triangle indices
	if (batch.topology == kPrimitiveQuads)
	{
		int quadIndexCount = indexCount/4*6;
		UInt16*	quadIB = ALLOC_TEMP_MANUAL (UInt16, quadIndexCount);
		UInt16* src = (UInt16*)((UInt8*)indices.indices + firstIndexByte);
		Prefetch(src, indexCount * kVBOIndexSize);
		for (int i = 0; i < indexCount/4; ++i)
		{
			quadIB[6*i+0] = src[0];
			quadIB[6*i+1] = src[1];
			quadIB[6*i+2] = src[2];
			quadIB[6*i+3] = src[0];
			quadIB[6*i+4] = src[2];
			quadIB[6*i+5] = src[3];
			src += 4;
		}
		outIndexCount = TransformIndices(batch.outIndices, quadIB, 0, quadIndexCount, firstVertex, batch.vertexCount, false);
		FREE_TEMP_MANUAL(quadIB);
	}
	else
		outIndexCount = TransformIndices(batch.outIndices, indices.indices, firstIndexByte, indexCount, firstVertex, batch.vertexCount, batch.topology==kPrimitiveTriangleStripDeprecated);

	size_t outVertexCount = TransformVertices(batch.outVertices, matrix, vertices, firstVertex, vertexCount, batch.availableChannels);

	batch.outIndices += outIndexCount;
	batch.outVertices += outVertexCount * batch.destStride;
	batch.indexCount += outIndexCount;
	batch.vertexCount += outVertexCount;
	batch.meshCount++;
}

void GfxDevice::EndDynamicBatching( TransformType transformType )
{
	Assert(s_DynamicBatch.isActive);

	const DynamicBatch& batch = s_DynamicBatch;
	Assert(batch.vertexCount <= batch.maxVertices);
	Assert(batch.indexCount <= batch.maxIndices);

	// Release VBO chunk
	GetDynamicVBO().ReleaseChunk(batch.vertexCount, batch.indexCount);

	SetWorldMatrixAndType(Matrix4x4f::identity.GetPtr(), transformType);

	GetDynamicVBO().DrawChunk(batch.shaderChannels);
	ABSOLUTE_TIME elapsedTime = ELAPSED_TIME(batch.startTime);
	int primCount = GetPrimitiveCount(batch.indexCount, batch.topology, false);
	GetFrameStats().AddBatch(primCount, batch.vertexCount, batch.meshCount, elapsedTime);
	s_DynamicBatch.isActive = false;
}
#if ENABLE_SPRITES
void GfxDevice::DynamicBatchSprite(const Matrix4x4f* matrix, const SpriteRenderData* rd, ColorRGBA32 color)
{
	Assert(s_DynamicBatch.isActive);
	DynamicBatch& batch = s_DynamicBatch;

	TransformSprite (batch.outVertices, batch.outIndices, matrix, rd, color, batch.vertexCount);
	int outIndexCount = (int)rd->indices.size();
	int outVertexCount = (int)rd->vertices.size();
	batch.outIndices += outIndexCount;
	batch.outVertices += outVertexCount * batch.destStride;
	batch.indexCount += outIndexCount;
	batch.vertexCount += outVertexCount;
	batch.meshCount++;
}
#endif
#else
void ClearStaticBatchIndices(){}
#endif //GFX_ENABLE_DRAW_CALL_BATCHING

void GfxDevice::AddBatchingStats( int batchedTris, int batchedVerts, int batchedCalls )
{
	ABSOLUTE_TIME unusedTime;
	ABSOLUTE_TIME_INIT(unusedTime);
	GetFrameStats().AddBatch(batchedTris, batchedVerts, batchedCalls, unusedTime);
}

// on gl/gles we create textures at the very beginning with glGenTextures
// so start generate ids from smth not 0
volatile int GfxDevice::ms_TextureIDGenerator = 10;
volatile int GfxDevice::ms_ComputeBufferIDGenerator = 0;

#if !UNITY_WII // Wii also needs to register the ID, so separate impl
TextureID GfxDevice::CreateTextureID ()
{
	return TextureID(AtomicIncrement(&ms_TextureIDGenerator));
}
#endif

void GfxDevice::FreeTextureID( TextureID texture )
{
}

ComputeBufferID GfxDevice::CreateComputeBufferID()
{
	return ComputeBufferID(AtomicIncrement (&ms_ComputeBufferIDGenerator));
}

void GfxDevice::FreeComputeBufferID(ComputeBufferID id)
{
	// Do nothing yet
}


void GfxDevice::ResetFrameStats()
{
	m_Stats.ResetFrame();
}

void GfxDevice::BeginFrameStats()
{
	m_Stats.BeginFrameStats();
}

void GfxDevice::EndFrameStats()
{
	m_Stats.EndClientFrameStats();
}

void GfxDevice::SaveDrawStats()
{
	m_SavedStats.CopyAllDrawStats(m_Stats);
	m_SavedStats.CopyClientStats(m_Stats);
}

void GfxDevice::RestoreDrawStats()
{
	m_Stats.CopyAllDrawStats(m_SavedStats);
	m_Stats.CopyClientStats(m_SavedStats);
}

void GfxDevice::SynchronizeStats()
{
}

#if UNITY_EDITOR
void GfxDevice::SetColorBytes (const UInt8 color[4])
{
	float colorFloat[4];
	colorFloat[0] = ByteToNormalized(color[0]);
	colorFloat[1] = ByteToNormalized(color[1]);
	colorFloat[2] = ByteToNormalized(color[2]);
	colorFloat[3] = ByteToNormalized(color[3]);
	SetColor (colorFloat);
}
#endif


void GfxThreadableDevice::SetShadersMainThread (ShaderLab::SubProgram* programs[kShaderTypeCount], const ShaderLab::PropertySheet* props)
{
	ErrorString("Don't call SetShadersMainThread on threadable device! Use GraphicsHelper instead");
}



void GfxDevice::CommonReloadResources(UInt32 flags)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER
	if (flags & kReloadTextures)
		Texture::ReloadAll();

	if (flags & kReloadShaders)
		Shader::ReloadAllShaders();

	if (flags & kReleaseRenderTextures)
		RenderTexture::ReleaseAll();
#else
	//todo.
#endif
}


void CalculateDeviceProjectionMatrix (Matrix4x4f& m, bool usesOpenGLTextureCoords, bool invertY)
{
	if (usesOpenGLTextureCoords)
		return; // nothing to do on OpenGL-like devices

	// Otherwise, the matrix is OpenGL style, and we have to convert it to
	// D3D-like projection matrix

	if (invertY)
	{
		m.Get(1,0) = -m.Get(1,0);
		m.Get(1,1) = -m.Get(1,1);
		m.Get(1,2) = -m.Get(1,2);
		m.Get(1,3) = -m.Get(1,3);
	}


	// Now scale&bias to get Z range from -1..1 to 0..1:
	// matrix = scaleBias * matrix
	//	1   0   0   0
	//	0   1   0   0
	//	0   0 0.5 0.5
	//	0   0   0   1
	m.Get(2,0) = m.Get(2,0) * 0.5f + m.Get(3,0) * 0.5f;
	m.Get(2,1) = m.Get(2,1) * 0.5f + m.Get(3,1) * 0.5f;
	m.Get(2,2) = m.Get(2,2) * 0.5f + m.Get(3,2) * 0.5f;
	m.Get(2,3) = m.Get(2,3) * 0.5f + m.Get(3,3) * 0.5f;
}



void GfxDevice::SetupVertexLightParams(int light, const GfxVertexLight& data)
{
	DebugAssert(light >= 0 && light < kMaxSupportedVertexLights);

	const Matrix4x4f& viewMat = m_BuiltinParamValues.GetMatrixParam(kShaderMatView);

	Vector4f& position = m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Position + light));
	Vector4f& spotDirection = m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0SpotDirection + light));
	Vector4f& atten = m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Atten + light));

	// color
	m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + light), data.color);

	// position
	if (data.type == kLightDirectional)
	{
		Vector3f p = viewMat.MultiplyVector3 ((const Vector3f&)data.position);
		position.Set(-p.x, -p.y, -p.z, 0.0f);
	}
	else
	{
		Vector3f p = viewMat.MultiplyPoint3 ((const Vector3f&)data.position);
		position.Set(p.x, p.y, p.z, 1.0f);
	}

	// attenuation set in a way where distance attenuation can be computed:
	//	float lengthSq = dot(toLight, toLight);
	//	float atten = 1.0 / (1.0 + lengthSq * unity_LightAtten[i].z);
	// and spot cone attenuation:
	//	float rho = max (0, dot(normalize(toLight), unity_SpotDirection[i].xyz));
	//	float spotAtt = (rho - unity_LightAtten[i].x) * unity_LightAtten[i].y;
	//	spotAtt = saturate(spotAtt);
	// and the above works for all light types, i.e. spot light code works out
	// to correct math for point & directional lights as well.

	const float rangeSq = data.range * data.range;

	// spot direction & attenuation
	if (data.spotAngle > 0.0f)
	{
		// spot light
		Vector3f d = viewMat.MultiplyVector3((const Vector3f&)data.spotDirection);
		spotDirection.Set(-d.x, -d.y, -d.z, 0.0f);

		const float radAngle = Deg2Rad(data.spotAngle);
		const float cosTheta = cosf(radAngle*0.25f);
		const float cosPhi = cosf(radAngle*0.5f);
		const float cosDiff = cosTheta - cosPhi;
		atten.Set(cosPhi, (cosDiff != 0.0f) ? 1.0f / cosDiff : 1.0f, data.quadAtten, rangeSq);
	}
	else
	{
		// non-spot light
		spotDirection.Set(0.0f, 0.0f, 1.0f, 0.0f);
		atten.Set(-1.0f, 1.0f, data.quadAtten, rangeSq);
	}
}



#if UNITY_EDITOR
VertexComponent kSuitableVertexComponentForChannel[kShaderChannelCount] = {
	kVertexCompVertex,
	kVertexCompNormal,
	kVertexCompColor,
	kVertexCompTexCoord0,
	kVertexCompTexCoord1,
	kVertexCompTexCoord2,
};
#endif

static const float kDodecahedron[20][3] = {
	{  0.607f,  0.000f,  0.795f },
	{  0.188f,  0.577f,  0.795f },
	{ -0.491f,  0.357f,  0.795f },
	{ -0.491f, -0.357f,  0.795f },
	{  0.188f, -0.577f,  0.795f },
	{  0.982f,  0.000f,  0.188f },
	{  0.304f,  0.934f,  0.188f },
	{ -0.795f,  0.577f,  0.188f },
	{ -0.795f, -0.577f,  0.188f },
	{  0.304f, -0.934f,  0.188f },
	{  0.795f,  0.577f, -0.188f },
	{ -0.304f,  0.934f, -0.188f },
	{ -0.982f,  0.000f, -0.188f },
	{ -0.304f, -0.934f, -0.188f },
	{  0.795f, -0.577f, -0.188f },
	{  0.491f,  0.357f, -0.795f },
	{ -0.188f,  0.577f, -0.795f },
	{ -0.607f,  0.000f, -0.795f },
	{ -0.188f, -0.577f, -0.795f },
	{  0.491f, -0.357f, -0.795f },
};

#define DODECAHEDRON_TRIANGLE(x,y,z,a,b,c,s) \
	ImmediateVertex(x + kDodecahedron[a][0] * s, y + kDodecahedron[a][1] * s, z + kDodecahedron[a][2] * s); \
	ImmediateVertex(x + kDodecahedron[b][0] * s, y + kDodecahedron[b][1] * s, z + kDodecahedron[b][2] * s); \
	ImmediateVertex(x + kDodecahedron[c][0] * s, y + kDodecahedron[c][1] * s, z + kDodecahedron[c][2] * s);

#define DODECAHEDRON_FACE(x,y,z,a,b,c,d,e,s) \
	DODECAHEDRON_TRIANGLE(x,y,z,a,b,c,s) \
	DODECAHEDRON_TRIANGLE(x,y,z,a,c,d,s) \
	DODECAHEDRON_TRIANGLE(x,y,z,a,d,e,s)

void GfxDevice::ImmediateShape( float x, float y, float z, float scale, ImmediateShapeType shape )
{
	switch (shape)
	{
	case kShapeCube:
		ImmediateBegin(kPrimitiveQuads);
		ImmediateNormal(0, 0, 0);
		// -z
		ImmediateVertex (x+scale, y-scale, z-scale);
		ImmediateVertex (x-scale, y-scale, z-scale);
		ImmediateVertex (x-scale, y+scale, z-scale);
		ImmediateVertex (x+scale, y+scale, z-scale);
		// +z
		ImmediateVertex (x-scale, y-scale, z+scale);
		ImmediateVertex (x+scale, y-scale, z+scale);
		ImmediateVertex (x+scale, y+scale, z+scale);
		ImmediateVertex (x-scale, y+scale, z+scale);
		// -x
		ImmediateVertex (x-scale, y+scale, z-scale);
		ImmediateVertex (x-scale, y-scale, z-scale);
		ImmediateVertex (x-scale, y-scale, z+scale);
		ImmediateVertex (x-scale, y+scale, z+scale);
		// +x
		ImmediateVertex (x+scale, y-scale, z-scale);
		ImmediateVertex (x+scale, y+scale, z-scale);
		ImmediateVertex (x+scale, y+scale, z+scale);
		ImmediateVertex (x+scale, y-scale, z+scale);
		// -y
		ImmediateVertex (x-scale, y-scale, z-scale);
		ImmediateVertex (x+scale, y-scale, z-scale);
		ImmediateVertex (x+scale, y-scale, z+scale);
		ImmediateVertex (x-scale, y-scale, z+scale);
		// +y
		ImmediateVertex (x+scale, y+scale, z-scale);
		ImmediateVertex (x-scale, y+scale, z-scale);
		ImmediateVertex (x-scale, y+scale, z+scale);
		ImmediateVertex (x+scale, y+scale, z+scale);

		ImmediateEnd();
		break;

	case kShapeDodecahedron:
		// template edge length
		// a = 0.713644
		// radius of sphere containing the dodecahedron
		// r = a / 20 * sqrtf(250 + 110*sqrtf(5))
		// scale our radius to fit the template
		// TODO: is this correct? :)
		scale = scale * 1.258408f;

		ImmediateBegin(kPrimitiveTriangles);
		ImmediateNormal(0, 0, 0);

		DODECAHEDRON_FACE(x,y,z, 0,1,2,3,4, scale);
		DODECAHEDRON_FACE(x,y,z, 0,5,10,6,1, scale);
		DODECAHEDRON_FACE(x,y,z, 1,6,11,7,2, scale);
		DODECAHEDRON_FACE(x,y,z, 2,7,12,8,3, scale);
		DODECAHEDRON_FACE(x,y,z, 3,8,13,9,4, scale);
		DODECAHEDRON_FACE(x,y,z, 4,9,14,5,0, scale);
		DODECAHEDRON_FACE(x,y,z, 15,16,11,6,10, scale);
		DODECAHEDRON_FACE(x,y,z, 16,17,12,7,11, scale);
		DODECAHEDRON_FACE(x,y,z, 17,18,13,8,12, scale);
		DODECAHEDRON_FACE(x,y,z, 18,19,14,9,13, scale);
		DODECAHEDRON_FACE(x,y,z, 19,15,10,5,14, scale);
		DODECAHEDRON_FACE(x,y,z, 15,19,18,17,16, scale);

		ImmediateEnd();
		break;

	default:
		FatalErrorString("Unknown ImmediateShape");
		break;
	};
}

#undef DODECAHEDRON_FACE
#undef DODECAHEDRON_TRIANGLE

UInt32 GfxDevice::GetNativeTextureID(TextureID id)
{
#if ENABLE_TEXTUREID_MAP
	return TextureIdMap::QueryNativeTexture(id);
#else
	return id.m_ID;
#endif
}

void GfxDevice::InsertCustomMarker (int marker)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER
	PluginsRenderMarker (marker);
#endif
}
