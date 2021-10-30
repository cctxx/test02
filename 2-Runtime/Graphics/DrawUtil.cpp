#include "UnityPrefix.h"
#include "DrawUtil.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/RenderManager.h"
#include "Transform.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceStats.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Shaders/ComputeShader.h"
#include "Runtime/Graphics/SpriteFrame.h"
#if UNITY_PS3
#	include "Runtime/GfxDevice/ps3/GfxGCMVBO.h"
#endif

PROFILER_INFORMATION(gDrawMeshVBOProfile, "Mesh.DrawVBO", kProfilerRender)
PROFILER_INFORMATION(gDrawMeshNullProfile, "Graphics.DrawProcedural", kProfilerRender)

static void DrawMeshInternal (const ChannelAssigns& channels, Mesh &mesh, const Matrix4x4f &matrix, int subsetIndex, TransformType transformType);

void DrawUtil::DrawVBOMeshRaw (VBO& vbo, Mesh& mesh, const ChannelAssigns& channels, int subsetIndex, UInt32 channelsInVBO)
{
	PROFILER_AUTO(gDrawMeshVBOProfile, &mesh)

	subsetIndex = std::min<unsigned int>(subsetIndex, mesh.GetSubMeshCount()? mesh.GetSubMeshCount()-1:0);
	SubMesh& submesh = mesh.GetSubMeshFast(subsetIndex);
	
	int firstVertex, vertexCount;

	firstVertex = submesh.firstVertex;
	vertexCount = submesh.vertexCount;
#if UNITY_PS3
	GfxGCMVBO& gcmVBO = (GfxGCMVBO&)vbo;
	gcmVBO.DrawSubmesh(channels, subsetIndex, &submesh);
#else
	vbo.DrawVBO (channels, submesh.firstByte, submesh.indexCount, submesh.topology, firstVertex, vertexCount);
#endif
	GPU_TIMESTAMP();
}

void DrawUtil::DrawMeshRaw (const ChannelAssigns& channels, Mesh& mesh, int subsetIndex)
{
	VBO* vbo = mesh.GetSharedVBO (channels.GetSourceMap());
	if( vbo != NULL )
	{
		DrawVBOMeshRaw (*vbo, mesh, channels, subsetIndex);
	}
}

void DrawUtil::DrawMesh (const ChannelAssigns& channels, Mesh &mesh, const Vector3f &position, const Quaternionf &rotation, int subsetIndex)
{
	Matrix4x4f matrix;
	QuaternionToMatrix (rotation, matrix);
	matrix.SetPosition( position );

	TransformType transformType = kNoScaleTransform;
	DrawMeshInternal (channels, mesh, matrix, subsetIndex, transformType);
}

void DrawUtil::DrawMesh (const ChannelAssigns& channels, Mesh &mesh, const Matrix4x4f &matrix, int subsetIndex)
	{
	// just assume it is a scaled transform, but don't handle negative scales... oh well! (says @aras_p)
	TransformType transformType = kUniformScaleTransform;
	DrawMeshInternal (channels, mesh, matrix, subsetIndex, transformType);
}

static void DrawMeshInternal (const ChannelAssigns& channels, Mesh &mesh, const Matrix4x4f &matrix, int subsetIndex, TransformType transformType)
{
	const Camera* camera = GetCurrentCameraPtr();

	GfxDevice& device = GetGfxDevice();
	float matWorld[16], matView[16];

	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	if (camera)
		device.SetViewMatrix( camera->GetWorldToCameraMatrix().GetPtr() );

	SetupObjectMatrix (matrix, transformType);

	if (subsetIndex != -1)
	{
		DrawUtil::DrawMeshRaw (channels, mesh, subsetIndex);
	}
	else
	{
		int submeshCount = mesh.GetSubMeshCount();
		for (int i=0;i<submeshCount;i++)
			DrawUtil::DrawMeshRaw (channels, mesh, i);
	}

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

void DrawUtil::DrawProcedural (GfxPrimitiveType topology, int vertexCount, int instanceCount)
{
	if (instanceCount > 1 && !gGraphicsCaps.hasInstancing)
	{
		ErrorString ("Can't do instanced Graphics.DrawProcedural");
		return;
	}

	PROFILER_AUTO(gDrawMeshNullProfile, NULL)

	GfxDevice& device = GetGfxDevice();
	device.DrawNullGeometry (topology, vertexCount, instanceCount);
	device.GetFrameStats().AddDrawCall (vertexCount*instanceCount, vertexCount*instanceCount);

	GPU_TIMESTAMP();
}

void DrawUtil::DrawProceduralIndirect (GfxPrimitiveType topology, ComputeBuffer* bufferWithArgs, UInt32 argsOffset)
{
	if (!gGraphicsCaps.hasInstancing || !gGraphicsCaps.hasComputeShader)
	{
		ErrorString ("Can't do indirect Graphics.DrawProcedural");
		return;
	}
	if (!bufferWithArgs)
	{
		ErrorString ("Graphics.DrawProcedural with invalid buffer");
		return;
	}
	ComputeBufferID bufferHandle = bufferWithArgs->GetBufferHandle();
	if (!bufferHandle.IsValid())
	{
		ErrorString ("Graphics.DrawProcedural with invalid buffer");
		return;
	}

	PROFILER_AUTO(gDrawMeshNullProfile, NULL)

	GfxDevice& device = GetGfxDevice();
	device.DrawNullGeometryIndirect (topology, bufferHandle, argsOffset);
	device.GetFrameStats().AddDrawCall (1,1); // unknown primitive count

	GPU_TIMESTAMP();
}

void DrawUtil::DrawSpriteRaw (const ChannelAssigns& channels, Sprite& sprite, const ColorRGBAf& color)
{
	GfxDevice& device = GetGfxDevice();

	const SpriteRenderData& rd = sprite.GetRenderData(false); // Use non-atlased RenderData as input.
	Assert(rd.texture.IsValid());

	// Get VBO chunk for a rectangle or mesh
	UInt32 numIndices = rd.indices.size();
	UInt32 numVertices = rd.vertices.size();
	if (!numIndices)
		return;

	const UInt32 channelMask = (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor);

	DynamicVBO& vbo = device.GetDynamicVBO();
	UInt8*  __restrict vbPtr;
	UInt16* __restrict ibPtr;
	if ( !vbo.GetChunk(channelMask, numVertices, numIndices, DynamicVBO::kDrawIndexedTriangles, (void**)&vbPtr, (void**)&ibPtr) )
		return;

	TransformSprite (vbPtr, ibPtr, NULL, &rd, device.ConvertToDeviceVertexColor(color), 0);
	vbo.ReleaseChunk(numVertices, numIndices);
	vbo.DrawChunk(channels);
}
