
#include "UnityPrefix.h"
#include "MeshPartitioner.h"
#include "Runtime/Filters/Mesh/LodMesh.h"

#if UNITY_EDITOR

static const UInt32 ComponentStride[] = { 12, 12, 4, 8, 8, 16, sizeof(BoneInfluence) };

static int CalcDMABatchSize(int totalVerts, int stride, const int sizeRestriction, bool padded) 
{
	const int alignmentRestriction = 16; // DMA transfers address must be a multiple of 16
	int a = alignmentRestriction;

	if(a>stride) 
	{
		if(a % stride == 0)
			return sizeRestriction;
		while(a % stride) { a+=alignmentRestriction; }
	}
	else
	{
		if(stride % a == 0)
			return sizeRestriction;
		while(stride % a) { a+=alignmentRestriction; }
	}

	int batchMultiple = a / stride;
	totalVerts = (totalVerts < sizeRestriction) ? totalVerts : sizeRestriction;
	if(padded)
		totalVerts += batchMultiple - 1;
	totalVerts /= batchMultiple;
	totalVerts *= batchMultiple;
	return totalVerts;
};

static int CalcBestFitBatchSize(const UInt32 availableChannels, int vertexCount, int maxVerts, bool padded = false) 
{
	int bestFit = INT_MAX;
	for(int i=0;i<=kShaderChannelCount;i++) 
	{
		if (availableChannels & (1<<i)) 
		{
			int maxVCount = CalcDMABatchSize(vertexCount, ComponentStride[i], maxVerts, padded);
			bestFit = (bestFit > maxVCount) ? maxVCount : bestFit;
		}
	}
	return bestFit;
}

template<typename T>
struct TempPartition 
{
	dynamic_array<Vector3f>		m_Vertices;
	dynamic_array<Vector2f>		m_UV;
	dynamic_array<Vector2f>		m_UV1;
	dynamic_array<ColorRGBA32>	m_Colors;
	dynamic_array<Vector3f>		m_Normals;
	dynamic_array<Vector4f>		m_Tangents;
	dynamic_array<BoneInfluence> m_Skin;
	dynamic_array<T>			indexBuffer;
	dynamic_array<T>			newToOld; 
	int							vertexCount;
	//
	void InitRemapping(int numVertices) 
	{
		newToOld.resize_uninitialized(numVertices);
		memset(&newToOld[0],(T)-1,numVertices*sizeof(T));
	}
	void RemapVertices(Mesh& mesh, int actualVertexCount)
	{
		m_Vertices.resize_uninitialized(vertexCount);
		const UInt32 channels = mesh.GetAvailableChannels();
		if(channels&(1<<kShaderChannelNormal))
			m_Normals.resize_uninitialized(vertexCount);
		if(channels&(1<<kShaderChannelTexCoord0))
			m_UV.resize_uninitialized(vertexCount);
		if(channels&(1<<kShaderChannelTexCoord1))
			m_UV1.resize_uninitialized(vertexCount);
		if(channels&(1<<kShaderChannelTangent))
			m_Tangents.resize_uninitialized(vertexCount);
		if(channels&(1<<kShaderChannelColor))
			m_Colors.resize_uninitialized(vertexCount);
		if(!mesh.GetSkin().empty())
			m_Skin.resize_uninitialized(vertexCount);

		T remapNew = 0;
		for(int vertex=0; vertex<vertexCount; vertex++) 
		{
			if((T)-1 != newToOld[vertex])
				remapNew = newToOld[vertex];
			m_Vertices[vertex]=mesh.GetVertexBegin()[remapNew];
			if(channels&(1<<kShaderChannelNormal))
				m_Normals[vertex]=mesh.GetNormalBegin()[remapNew];
			if(channels&(1<<kShaderChannelTexCoord0))
				m_UV[vertex]=mesh.GetUvBegin(0)[remapNew];
			if(channels&(1<<kShaderChannelTexCoord1))
				m_UV1[vertex]=mesh.GetUvBegin(1)[remapNew];
			if(channels&(1<<kShaderChannelTangent))
				m_Tangents[vertex]=mesh.GetTangentBegin()[remapNew];
			if(channels&(1<<kShaderChannelColor))
				m_Colors[vertex]=mesh.GetColorBegin()[remapNew];
			if(!mesh.GetSkin().empty())
				m_Skin[vertex]=mesh.GetSkin()[remapNew];
		}
	}
};

template<typename T>
struct SegmentedMesh
{
	std::vector<TempPartition<T> >	m_Partitions;
	void Clear() { m_Partitions.clear(); }
};

template<typename T>
static void CreateFromSubMesh(std::vector< SegmentedMesh<T> >& segments, Mesh& mesh, int submesh)
{
	SubMesh& sm = mesh.GetSubMeshFast(submesh);

	T vertexCount = 0;
	const int numIndices = sm.indexCount;
	const int numTriangles = numIndices / 3;

	AssertBreak((numTriangles * 3) == numIndices);

	UInt32 maxComponentStride = 0;
	const UInt32 availableChannels = mesh.GetAvailableChannels() | (mesh.GetSkin().empty() ? 0 : (1<<kShaderChannelCount));
	for(int i=0;i<=kShaderChannelCount;i++) 
	{ 
		if(availableChannels & (1<<i)) 
		{
			if(maxComponentStride < ComponentStride[i]) 
				maxComponentStride = ComponentStride[i];
		}
	}

	const UInt32 maxDMATransferSize = 16 * 1024;
	const UInt32 numVerts = (numIndices + 15) & (~15);
	const UInt32 maxVerts = std::min(numVerts, maxDMATransferSize / maxComponentStride);
	const UInt32 batchSize = CalcBestFitBatchSize(availableChannels, numVerts, maxVerts);

	const int maxPartitions = (numIndices + batchSize-1) / batchSize;
	const int numVertices = (sm.indexCount + 2*maxPartitions);

	const T* srcIndices = reinterpret_cast<const T*> (&mesh.GetIndexBuffer()[sm.firstByte]);

	int startTriangle = 0;
	int startVertex = 0;
	std::vector<T> oldToNew; 
	oldToNew.resize(mesh.GetVertexCount());
	std::vector<TempPartition<T> > & partitions = segments[submesh].m_Partitions;
	while(startTriangle != numTriangles) 
	{
		TempPartition<T> p;
		p.indexBuffer.clear();
		p.vertexCount = 0;
		p.InitRemapping(batchSize+3);
		dynamic_array<T>& dstIndices = p.indexBuffer;
		memset(&oldToNew[0],(T)-1,oldToNew.size()*sizeof(T));
		for(int i=startTriangle; i<numTriangles; i++) 
		{
			startTriangle = numTriangles;
			T lastVertexCount = vertexCount; // undo stack
			for(int j=0;j<3;j++) 
			{
				int index = i*3+j;
				int vertex = srcIndices[index];				
				AssertBreak(vertex >= 0);
				AssertBreak(vertex < mesh.GetVertexCount());
				AssertBreak(lastVertexCount-startVertex+j < p.newToOld.size());
				AssertBreak(p.newToOld[lastVertexCount-startVertex+j] == (T)-1);
				if(oldToNew[vertex]==(T)-1) 
				{
					AssertBreak(vertexCount < numVertices);
					oldToNew[vertex]=vertexCount-startVertex;
					p.newToOld[vertexCount-startVertex]=vertex;
					vertexCount++;
				}
				dstIndices.push_back(oldToNew[vertex]);
			}
			if((vertexCount-startVertex) > batchSize) 
			{
				//undo the last one in the partition
				for(int j=0;j<3;j++)
				{
					p.newToOld[lastVertexCount-startVertex+j] = -1;;
					dstIndices.pop_back();
				}
				startTriangle = i;
				vertexCount = lastVertexCount;
				break;
			}
		}
		const int actualVertexCount = vertexCount - startVertex;
		p.vertexCount = maxVerts;//CalcBestFitBatchSize(availableChannels, actualVertexCount, maxVerts, true);	// FIXME!!! This needs to find the next "best fit" that will still keep alignment restrictions..
		p.RemapVertices(mesh, actualVertexCount);
		partitions.push_back(p);
		startVertex = vertexCount;
	}
	oldToNew.clear();
}

// mircea: todo: this would be awesome!!!
//	spuInOut:
//		m_Vertices
//		m_Normals
//		m_Tangents
//	spuIn:
//		m_Skin

//	rsxDirect
//		m_UV
//		m_UV1
//		m_Colors
//		m_IndexBuffer

void PartitionSubmeshes(Mesh& m) 
{
	typedef UInt16 T;

	const int submeshCount = m.m_SubMeshes.size();

	m.m_PartitionInfos.clear();
	m.m_Partitions.clear();

	// skinned meshes cannot be partitioned if the optimization flag is not set because partitioning changes the vertex/index buffers
	if (!m.GetMeshOptimized() || m.GetSkin().empty())
		return;

	// destripify if needed
	m.DestripifyIndices ();

	// need to fixup the indices first so they are not relative to the partition start anymore.
	Mesh::MeshPartitionInfoContainer& partInfos = m.m_PartitionInfos;
	for(int pi=0; pi<partInfos.size(); pi++)
	{
		const MeshPartitionInfo& partInfo = m.m_PartitionInfos[pi];

		for(int s=0; s<partInfo.partitionCount; s++)
		{
			const MeshPartition& p = m.m_Partitions[partInfo.submeshStart + s];
			IndexBufferData indexBufferData;
			m.GetIndexBufferData(indexBufferData);
			UInt16* indices = (UInt16*)(&m.m_IndexBuffer[0] + p.indexByteOffset);
			for(int i=0;i<p.indexCount;i++) 
				indices[i] += p.vertexOffset;
		}
	}

	// make a segment for each submesh
	std::vector< SegmentedMesh<T> > segments;
	segments.resize(submeshCount);
	for(int submesh=0;submesh<submeshCount;submesh++) 
		CreateFromSubMesh<T>(segments, m, submesh);

	///////////////////////////////////////////////////////////////////////////////
	// combine the segments to get the script accessible buffers

	UInt32 availableChannels = m.GetAvailableChannels();

	m.Clear(false);
	m.SetMeshOptimized(true);		//mircea@ m.Clear will set the optimized mesh to false. Being here means we are partitioning an optimized mesh so restore the flag.
	m.SetSubMeshCount(submeshCount);

	UInt32 vertexOffset = 0;
	UInt32 indexOffset = 0;

	for(int submesh=0;submesh<submeshCount;submesh++) 
	{
		int indexCount = 0;
		SegmentedMesh<T>& seg = segments[submesh];

		MeshPartitionInfo partInfo;
		partInfo.submeshStart = m.m_Partitions.size();
		partInfo.partitionCount = seg.m_Partitions.size();
		m.m_PartitionInfos.push_back(partInfo);

		// create partitions & build the mesh buffers
		for(int s=0;s<seg.m_Partitions.size();s++)
		{
			MeshPartition part;
			TempPartition<T>& p = seg.m_Partitions[s];
			part.vertexCount = p.vertexCount;
			part.vertexOffset = vertexOffset;
			part.indexCount = p.indexBuffer.size();
			part.indexByteOffset = indexOffset;
			AssertBreak(0 == (part.vertexOffset & 15));
			m.m_Partitions.push_back(part);;
			indexCount += part.indexCount;
			indexOffset += p.indexBuffer.size() * sizeof(T);
			vertexOffset += p.vertexCount;
		}
	}		

	// fill in the partitioned data back into the mesh.
	m.ResizeVertices(vertexOffset, availableChannels);

	for(int submesh=0;submesh<submeshCount;submesh++) 
	{
		const SegmentedMesh<T>& seg = segments[submesh];
		const MeshPartitionInfo& partInfo = m.m_PartitionInfos[submesh];
		for(int s=0;s<seg.m_Partitions.size();s++)
		{
			const TempPartition<T>& p = seg.m_Partitions[s];
			const MeshPartition& part = m.m_Partitions[partInfo.submeshStart + s];
			strided_copy (p.m_Vertices.begin (), p.m_Vertices.end(), m.GetVertexBegin () + part.vertexOffset);
			if(!p.m_Normals.empty())
				strided_copy (p.m_Normals.begin (), p.m_Normals.end(), m.GetNormalBegin () + part.vertexOffset);
			if(!p.m_UV.empty())
				strided_copy (p.m_UV.begin (), p.m_UV.end (), m.GetUvBegin (0) + part.vertexOffset);
			if(!p.m_UV1.empty())
				strided_copy (p.m_UV1.begin (), p.m_UV1.end (), m.GetUvBegin (1) + part.vertexOffset);
			if(!p.m_Tangents.empty())
				strided_copy (p.m_Tangents.begin (), p.m_Tangents.end (), m.GetTangentBegin () + part.vertexOffset);
			if(!p.m_Colors.empty())
				strided_copy (p.m_Colors.begin (), p.m_Colors.end (), m.GetColorBegin() + part.vertexOffset);
			if(!p.m_Skin.empty())
				m.GetSkin().insert(m.GetSkin().end(), p.m_Skin.begin(), p.m_Skin.end());
		}

		std::vector<T> indices;
		for(int s=0;s<partInfo.partitionCount;s++)
		{
			const MeshPartition& p = m.m_Partitions[partInfo.submeshStart+s];
			const TempPartition<T>& tp = seg.m_Partitions[s];
			for(int i=0;i<p.indexCount;i++) 
			{
				int index = tp.indexBuffer[i];
				AssertBreak( (index>=0) && (index < (p.vertexCount)));
					#if DEBUG_PARTITIONING
						index += p.vertexOffset;
					#endif
				indices.push_back(index);
			}
		}
		m.SetIndices (&indices[0], indices.size(), submesh, kPrimitiveTriangles);
	}
}

void PartitionMesh(Mesh* m) 
{
	PartitionSubmeshes(*m);
}

#endif	//UNITY_EDITOR
