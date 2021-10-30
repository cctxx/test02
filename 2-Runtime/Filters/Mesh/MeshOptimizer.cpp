#include "UnityPrefix.h"
#include "MeshOptimizer.h"
#include <vector>

//@TODO: 

// Step 1

//* bool ExtractCollisionData (Mesh& mesh, UNITY_TEMP_VECTOR(kMemGeometry, Vector3f)& vertices, UNITY_TEMP_VECTOR(kMemGeometry, UInt32)& triangles);
//   -> make it return welded vertices and triangle array
//* Enable Deformablemesh code and make it work with welding code and check that cloth works visually...

// Testing:
//* Check mesh collision detection code to work visually correct.
// * run functional test suite
// * run lightmapper tests in the integration test suite. They have a complete test for the lightmap uv coordinates picking up lightmap values...


// Step 2:
//* Verify vertex cache performance on iPad1 / Wii / intel integrated graphics
//* Switch to default gpu optimized mode and update all model importer templates



template<typename T, const int CACHE_SIZE>
class VertexCacheOptimizer 
{
	UInt32* m_cacheEntries;
	UInt32 m_cacheSize;

	mutable UInt32 m_cacheMisses;
	mutable UInt32 m_cacheHits;

	UInt32 GetInCache(UInt32 lIndex, const char* vertexInCache) const 
	{ 
		return vertexInCache[lIndex] ? 1 : 0; 
	}
	
	void AddToCache(UInt32 lIndex, char* vertexInCache) 
	{
		if(m_cacheEntries[0]!=-1) 
			vertexInCache[m_cacheEntries[0]]=0;

		for(UInt32 i=0; i<m_cacheSize-1; i++) 
			m_cacheEntries[i]=m_cacheEntries[i+1];

		m_cacheEntries[m_cacheSize-1]=lIndex;
		vertexInCache[lIndex]=1;
	}

public:
	
	VertexCacheOptimizer () : m_cacheSize(CACHE_SIZE)
	{
		m_cacheEntries=new UInt32 [m_cacheSize];

		m_cacheHits = m_cacheMisses = 0;
		for(UInt32 i=0; i<m_cacheSize; i++) 
			m_cacheEntries[i]=(UInt32)-1;
	}

	~VertexCacheOptimizer() { delete m_cacheEntries; }

	UInt32 GetCacheMisses() { return m_cacheMisses; }
	UInt32 GetCacheHits() { return m_cacheHits; }

	void OptimizeTriangles(T* pdstTris, UInt32 numVertices, const T* srcTris, UInt32 numTriangles) 
	{
		UInt32 cachedVerts=0;
		char* triangleUsed=new char [numTriangles];
		char* vertexInCache=new char [numVertices];
		memset(triangleUsed,0,numTriangles);
		memset(vertexInCache,0,numVertices);

		bool foundTriangle=true;
		while (foundTriangle) 
		{
			foundTriangle=false;
			UInt32 bestCandidate=0;
			UInt32 bestCacheValue=0;
			for (UInt32 i = 0; i < numTriangles; i++) 
			{
				if (triangleUsed[i]) 
					continue;

				foundTriangle=true;
				UInt32 i1=srcTris[i*3+0];
				UInt32 i2=srcTris[i*3+1];
				UInt32 i3=srcTris[i*3+2];

				UInt32 lCacheValue=GetInCache(i1,vertexInCache)+GetInCache(i2,vertexInCache)+GetInCache(i3,vertexInCache)+1;
				if (lCacheValue > bestCacheValue) 
				{
					bestCandidate=i;
					bestCacheValue=lCacheValue;
					if (bestCacheValue == 4) 
						break;
				}
			}
			if(foundTriangle) 
			{
				triangleUsed[bestCandidate]=1;
				UInt32 i1=srcTris[bestCandidate*3+0];
				UInt32 i2=srcTris[bestCandidate*3+1];
				UInt32 i3=srcTris[bestCandidate*3+2];
				*pdstTris++=(T)i1;
				*pdstTris++=(T)i2;
				*pdstTris++=(T)i3;
				if (!GetInCache(i1,vertexInCache)) { AddToCache(i1,vertexInCache); cachedVerts++; m_cacheMisses++; } else m_cacheHits++;
				if (!GetInCache(i2,vertexInCache)) { AddToCache(i2,vertexInCache); cachedVerts++; m_cacheMisses++; } else m_cacheHits++;
				if (!GetInCache(i3,vertexInCache)) { AddToCache(i3,vertexInCache); cachedVerts++; m_cacheMisses++; } else m_cacheHits++;
			}
		}
		delete[] triangleUsed;
		delete[] vertexInCache;
	}
};

inline bool CompareBlendShapeVertexIndex (const BlendShapeVertex& lhs, const BlendShapeVertex& rhs)
{
	return lhs.index < rhs.index;
}

void OptimizeReorderVertexBuffer (Mesh& mesh)
{
	const int submeshCount = mesh.GetSubMeshCount();
	const int vertexCount = mesh.GetVertexCount();

	// backup required data
	VertexData backupVertexData(mesh.m_VertexData, mesh.GetAvailableChannels(), mesh.GetVertexData().GetStreamsLayout(), mesh.GetVertexData().GetChannelsLayout());

	Mesh::BoneInfluenceContainer backupSkin;	
	if (!mesh.m_Skin.empty())		
		backupSkin.swap(mesh.m_Skin);

	// reorder the vertices so they come in increasing order
	dynamic_array<UInt32> oldToNew;
	dynamic_array<UInt32> newToOld;
	newToOld.resize_initialized(vertexCount, 0xFFFFFFFF);
	oldToNew.resize_initialized(vertexCount, 0xFFFFFFFF);
	
	Mesh::TemporaryIndexContainer dstIndices;
	int newVertexCount = 0;
	for (int submesh = 0; submesh < submeshCount; submesh++) 
	{
		Mesh::TemporaryIndexContainer indices;
		mesh.GetTriangles (indices, submesh);

		const int indexCount = indices.size();
		dstIndices.resize(indexCount);
		for (int index=0; index < indexCount; index++) 
		{
			int vertex = indices[index];				
			AssertBreak(vertex >= 0);
			AssertBreak(vertex < vertexCount);
			
			if (oldToNew[vertex] == 0xFFFFFFFF)
			{
				oldToNew[vertex]=newVertexCount;
				newToOld[newVertexCount]=vertex;
				newVertexCount++;
			}
			dstIndices[index] = oldToNew[vertex];
		}
		
		mesh.SetIndices (&dstIndices[0], dstIndices.size(), submesh, kPrimitiveTriangles);
	}
	
	mesh.ResizeVertices(newVertexCount, backupVertexData.GetChannelMask());

	if (!backupSkin.empty())
		mesh.m_Skin.resize_initialized(newVertexCount);

	for (int vertex=0; vertex < newVertexCount; vertex++) 
	{
		UInt32 remapNew = newToOld[vertex];
		Assert(remapNew != 0xFFFFFFFF);
		
		if (!backupSkin.empty())
			mesh.m_Skin[vertex] = backupSkin[remapNew];

		mesh.GetVertexBegin()[vertex] = backupVertexData.MakeStrideIterator<Vector3f> (kShaderChannelVertex)[remapNew];

		if (backupVertexData.HasChannel(kShaderChannelNormal))
			mesh.GetNormalBegin()[vertex] = backupVertexData.MakeStrideIterator<Vector3f> (kShaderChannelNormal)[remapNew];

		if (backupVertexData.HasChannel(kShaderChannelColor))
			mesh.GetColorBegin()[vertex] = backupVertexData.MakeStrideIterator<ColorRGBA32> (kShaderChannelColor)[remapNew];

		if (backupVertexData.HasChannel(kShaderChannelTexCoord0))
			mesh.GetUvBegin(0)[vertex] = backupVertexData.MakeStrideIterator<Vector2f> (kShaderChannelTexCoord0)[remapNew];

		if (backupVertexData.HasChannel(kShaderChannelTexCoord1))
			mesh.GetUvBegin(1)[vertex] = backupVertexData.MakeStrideIterator<Vector2f> (kShaderChannelTexCoord1)[remapNew];

		if (backupVertexData.HasChannel(kShaderChannelTangent))
			mesh.GetTangentBegin()[vertex] = backupVertexData.MakeStrideIterator<Vector4f> (kShaderChannelTangent)[remapNew];
	}
	
	// Remap vertex indices stored in blend shapes
	BlendShapeData& blendShapeData = mesh.GetWriteBlendShapeDataInternal();
	BlendShapeVertices& blendShapeVertices = blendShapeData.vertices;
	for (BlendShapeVertices::iterator itv = blendShapeVertices.begin(), endv = blendShapeVertices.end(); itv != endv; ++itv)
	{
		BlendShapeVertex& bsv = *itv;
		bsv.index = oldToNew[bsv.index];
	}

	// Sort each shape's vertices by index so the blending writes to memory as linearly as possible
	for (int shapeIndex = 0; shapeIndex < blendShapeData.shapes.size(); shapeIndex++)
	{
		const BlendShape& shape = blendShapeData.shapes[shapeIndex];
		BlendShapeVertex* vertices = &blendShapeVertices[shape.firstVertex];
		std::sort(vertices, vertices + shape.vertexCount, CompareBlendShapeVertexIndex);
	}

	mesh.SetChannelsDirty(mesh.GetAvailableChannels(), true);
}

void OptimizeIndexBuffers (Mesh& mesh)
{
	const int submeshCount = mesh.GetSubMeshCount();
	const int vertexCount = mesh.GetVertexCount();

	// first optimize the indices for each submesh
	for (int submesh = 0; submesh < submeshCount; submesh++)
	{
		Mesh::TemporaryIndexContainer unoptimizedIndices;
		mesh.GetTriangles (unoptimizedIndices, submesh);
		
		Mesh::TemporaryIndexContainer optimizedIndices;
		optimizedIndices.resize(unoptimizedIndices.size());
		
		VertexCacheOptimizer<UInt32, 16> vertexCacheOptimizer;
		vertexCacheOptimizer.OptimizeTriangles(&optimizedIndices[0], vertexCount, &unoptimizedIndices[0], unoptimizedIndices.size() / 3);
		// LogString(Format("[Optimize] mesh: %s: submesh: %d hits: %d misses: %d\n", mesh.GetName(), submesh, vertexCacheOptimizer.GetCacheHits(), vertexCacheOptimizer.GetCacheMisses()));

		mesh.SetIndices (&optimizedIndices[0], optimizedIndices.size(), submesh, kPrimitiveTriangles);
	}
}


template<typename T, const int CACHE_SIZE>
class VertexCacheDeOptimizer 
{
	UInt32* m_cacheEntries;
	UInt32 m_cacheSize;
    
	mutable UInt32 m_cacheMisses;
	mutable UInt32 m_cacheHits;
    
	UInt32 GetInCache(UInt32 lIndex, const char* vertexInCache) const 
	{ 
		return vertexInCache[lIndex] ? 1 : 0; 
	}
    
	void AddToCache(UInt32 lIndex, char* vertexInCache) 
	{
		if(m_cacheEntries[0]!=-1) 
			vertexInCache[m_cacheEntries[0]]=0;
        
		for(UInt32 i=0; i<m_cacheSize-1; i++) 
			m_cacheEntries[i]=m_cacheEntries[i+1];
        
		m_cacheEntries[m_cacheSize-1]=lIndex;
		vertexInCache[lIndex]=1;
	}
    
public:
    
	VertexCacheDeOptimizer () : m_cacheSize(CACHE_SIZE)
	{
		m_cacheEntries=new UInt32 [m_cacheSize];
        
		m_cacheHits = m_cacheMisses = 0;
		for(UInt32 i=0; i<m_cacheSize; i++) 
			m_cacheEntries[i]=(UInt32)-1;
	}
    
	~VertexCacheDeOptimizer() { delete m_cacheEntries; }
	
	UInt32 GetCacheMisses() { return m_cacheMisses; }
	UInt32 GetCacheHits() { return m_cacheHits; }
    
	void DeOptimizeTriangles(T* pdstTris, UInt32 numVertices, const T* srcTris, UInt32 numTriangles) 
	{
		UInt32 cachedVerts=0;
		char* triangleUsed=new char [numTriangles];
		char* vertexInCache=new char [numVertices];
		memset(triangleUsed,0,numTriangles);
		memset(vertexInCache,0,numVertices);
        
		bool foundTriangle=true;
		while (foundTriangle) 
		{
			foundTriangle=false;
			UInt32 bestCandidate=0;
			UInt32 bestCacheValue=4;
			for (UInt32 i = 0; i < numTriangles; i++) 
			{
				if (triangleUsed[i]) 
					continue;
                
				foundTriangle=true;
				UInt32 i1=srcTris[i*3+0];
				UInt32 i2=srcTris[i*3+1];
				UInt32 i3=srcTris[i*3+2];
                
				UInt32 lCacheValue=GetInCache(i1,vertexInCache)+GetInCache(i2,vertexInCache)+GetInCache(i3,vertexInCache)+1;
				if (lCacheValue <= bestCacheValue) 
				{
					bestCandidate=i;
					bestCacheValue=lCacheValue;
					if (bestCacheValue == 1) 
						break;
				}
			}
			if(foundTriangle) 
			{
				triangleUsed[bestCandidate]=1;
				UInt32 i1=srcTris[bestCandidate*3+0];
				UInt32 i2=srcTris[bestCandidate*3+1];
				UInt32 i3=srcTris[bestCandidate*3+2];
				*pdstTris++=(T)i1;
				*pdstTris++=(T)i2;
				*pdstTris++=(T)i3;
				if (!GetInCache(i1,vertexInCache)) { AddToCache(i1,vertexInCache); cachedVerts++; m_cacheMisses++; } else m_cacheHits++;
				if (!GetInCache(i2,vertexInCache)) { AddToCache(i2,vertexInCache); cachedVerts++; m_cacheMisses++; } else m_cacheHits++;
				if (!GetInCache(i3,vertexInCache)) { AddToCache(i3,vertexInCache); cachedVerts++; m_cacheMisses++; } else m_cacheHits++;
			}
		}
		delete triangleUsed;
		delete vertexInCache;
	}
};

void DeOptimizeIndexBuffers (Mesh& mesh)
{
	const int submeshCount = mesh.GetSubMeshCount();
	const int vertexCount = mesh.GetVertexCount();
    
	// first optimize the indices for each submesh
	for (int submesh = 0; submesh < submeshCount; submesh++)
	{
		Mesh::TemporaryIndexContainer unoptimizedIndices;
		mesh.GetTriangles (unoptimizedIndices, submesh);
        
		Mesh::TemporaryIndexContainer deOptimizedIndices;
		deOptimizedIndices.resize(unoptimizedIndices.size());
        
		VertexCacheDeOptimizer<UInt32, 16> vertexCacheDeOptimizer;
		vertexCacheDeOptimizer.DeOptimizeTriangles(&deOptimizedIndices[0], vertexCount, &unoptimizedIndices[0], unoptimizedIndices.size() / 3);
        
		//LogString(Format("[Deoptimize] mesh: %s: submesh: %d hits: %d misses: %d\n", mesh.GetName(), submesh, vertexCacheDeOptimizer.GetCacheHits(), vertexCacheDeOptimizer.GetCacheMisses()));
        
		mesh.SetIndices (&deOptimizedIndices[0], deOptimizedIndices.size(), submesh, kPrimitiveTriangles);
	}
}

