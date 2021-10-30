#include "UnityPrefix.h"
#include "Heightmap.h"

#if ENABLE_TERRAIN

#include "Runtime/Terrain/TerrainRenderer.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/Utility.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxHeightField.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxHeightFieldDesc.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxHeightFieldShape.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxHeightFieldShapeDesc.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxHeightFieldSample.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxScene.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxActor.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxActorDesc.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxCapsuleShape.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxCapsuleShapeDesc.h"
#include "Runtime/Terrain/DetailDatabase.h"
#include "Runtime/Dynamics/NxWrapperUtility.h"
#include "math.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Terrain/TerrainIndexGenerator.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Dynamics/TerrainCollider.h"
#include "Runtime/Interfaces/IPhysics.h"

enum { kMaxHeight =  32766 };

using namespace std;

inline UInt32 GetLowMaterialIndex (UInt32 index)
{
	return index & 0x7F;
}

inline UInt32 GetHighMaterialIndex (UInt32 index)
{
	return index >> 7;
}

static void UpdatePatchMeshInternal (
	const Heightmap& heightmap,
	const StrideIterator<Vector3f>& vertices,
	const StrideIterator<Vector3f>& normals,
	const StrideIterator<Vector2f>& uvs,
	int xPatch, int yPatch, int mipLevel, int edgeMask, TerrainRenderer *renderer)
{
	Vector3f hmScale = heightmap.GetScale();

	float skipScale = 1 << mipLevel;
	float scale = hmScale.y / (float)(kMaxHeight);

	Vector2f uvscale;
	uvscale.x = 1.0F / (heightmap.GetWidth() - 1) * skipScale;
	uvscale.y = 1.0F / (heightmap.GetHeight() - 1) * skipScale;

	Vector2f uvoffset;
	uvoffset.x = xPatch * (1 << mipLevel) * (kPatchSize -1);
	uvoffset.x /= (heightmap.GetWidth() - 1);
	uvoffset.y = yPatch * (1 << mipLevel) * (kPatchSize -1);
	uvoffset.y /= (heightmap.GetHeight() - 1);

	int skip = 1 << mipLevel;
	int xBase = xPatch * (kPatchSize -1);
	int yBase = yPatch * (kPatchSize -1);

	StrideIterator<Vector3f> itVertices = vertices;
	StrideIterator<Vector3f> itNormals = normals;
	StrideIterator<Vector2f> itUVs = uvs;
		
	for (int x=0;x<kPatchSize;x++)
	{
		for (int y=0;y<kPatchSize;y++)
		{
			int sampleIndex = (y + yBase) + (x + xBase) * heightmap.GetHeight();
			sampleIndex *=  skip;
			float height = heightmap.GetRawHeight(sampleIndex);
			height *= scale;

			int index = y + x * kPatchSize;

			// Vertex
			itVertices[index].x = (x + xBase) * hmScale.x * skipScale;
			itVertices[index].y = height;
			itVertices[index].z = (y + yBase) * hmScale.z * skipScale;

			// UV				
			itUVs[index].x = (float)x * uvscale.x + uvoffset.x;
			itUVs[index].y = (float)y * uvscale.y + uvoffset.y;

			// Normal
			Vector3f normal = heightmap.CalculateNormalSobelRespectingNeighbors ((x + xBase) * skip, (y + yBase) * skip, renderer);
			itNormals[index] = normal;
		}
	}
}

Heightmap::Heightmap (TerrainData *owner)
:	m_Scale(1.0f,1.0f,1.0f)
{
	m_TerrainData = owner;
#if ENABLE_PHYSICS
	m_NxHeightField = NULL;
#endif
} 

Heightmap::~Heightmap  ()
{
#if ENABLE_PHYSICS
	CleanupNx ();
#endif
}

/// Calculates the index of the patch given it's level and x, y index
int Heightmap::GetPatchIndex (int x, int y, int level) const
{
	int index = 0;
	for (int i=0;i<level;i++)
	{
		int size = 1 << (m_Levels - i);
		index += size * size;
	}
	
	int width = 1 << (m_Levels - level);
	index += width * y;
	index += x;
	return index;
}

float Heightmap::InterpolatePatchHeight (float* data, float fx, float fy) const
{
	int lx = (int)(fx * kPatchSize);
	int ly = (int)(fy * kPatchSize);
	
	int hx = lx + 1;
	if (hx >= kPatchSize)
		hx = kPatchSize - 1;
	int hy = ly + 1;
	if (hy >= kPatchSize)
		hy = kPatchSize - 1;
	
	float s00 = GetPatchHeight (data, lx, ly);
	float s01 = GetPatchHeight (data, lx, hy);
	float s10 = GetPatchHeight (data, hx, ly);
	float s11 = GetPatchHeight (data, hx, hy);
	
	float dx = fx * kPatchSize - lx;
	float dy = fy * kPatchSize - ly;
	
	float x = Lerp(s00, s10, dx);
	float y = Lerp(s01, s11, dx);
	float value = Lerp(x, y, dy);
	
	return value;
}


Vector3f Heightmap::CalculateNormalSobelRespectingNeighbors (int x, int y, const TerrainRenderer *renderer) const
{		
	Vector3f normal;
	float dY, dX;
	// Do X sobel filter
	dX  = GetHeightRespectingNeighbors (x-1, y-1, renderer) * -1.0F;
	dX += GetHeightRespectingNeighbors (x-1, y  , renderer) * -2.0F;
	dX += GetHeightRespectingNeighbors (x-1, y+1, renderer) * -1.0F;
	dX += GetHeightRespectingNeighbors (x+1, y-1, renderer) *  1.0F;
	dX += GetHeightRespectingNeighbors (x+1, y  , renderer) *  2.0F;
	dX += GetHeightRespectingNeighbors (x+1, y+1, renderer) *  1.0F;
	
	dX /= m_Scale.x;
	
	// Do Y sobel filter
	dY  = GetHeightRespectingNeighbors (x-1, y-1, renderer) * -1.0F;
	dY += GetHeightRespectingNeighbors (x  , y-1, renderer) * -2.0F;
	dY += GetHeightRespectingNeighbors (x+1, y-1, renderer) * -1.0F;
	dY += GetHeightRespectingNeighbors (x-1, y+1, renderer) *  1.0F;
	dY += GetHeightRespectingNeighbors (x  , y+1, renderer) *  2.0F;
	dY += GetHeightRespectingNeighbors (x+1, y+1, renderer) *  1.0F;
	dY /= m_Scale.z;
	
	// Cross Product of components of gradient reduces to
	normal.x = -dX;
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_3_a1))
		normal.y = 8;
	else
		// 5 here is just wrong!
		normal.y = 5;
	normal.z = -dY;
	normal = NormalizeFast (normal);	  
		
	return normal;
}

Vector3f Heightmap::CalculateNormalSobel (int x, int y) const
{
	Vector3f normal;
	float dY, dX;
	// Do X sobel filter
	dX  = GetHeight (x-1, y-1) * -1.0F;
	dX += GetHeight (x-1, y  ) * -2.0F;
	dX += GetHeight (x-1, y+1) * -1.0F;
	dX += GetHeight (x+1, y-1) *  1.0F;
	dX += GetHeight (x+1, y  ) *  2.0F;
	dX += GetHeight (x+1, y+1) *  1.0F;
	
	dX /= m_Scale.x;
	
	// Do Y sobel filter
	dY  = GetHeight (x-1, y-1) * -1.0F;
	dY += GetHeight (x  , y-1) * -2.0F;
	dY += GetHeight (x+1, y-1) * -1.0F;
	dY += GetHeight (x-1, y+1) *  1.0F;
	dY += GetHeight (x  , y+1) *  2.0F;
	dY += GetHeight (x+1, y+1) *  1.0F;
	dY /= m_Scale.z;
	
	// Cross Product of components of gradient reduces to
	normal.x = -dX;
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_3_a1))
		normal.y = 8;
	else
		// 5 here is just wrong!
		normal.y = 5;
	normal.z = -dY;
	normal = NormalizeFast (normal);	  
		
	return normal;
}

float Heightmap::ComputeMaximumHeightError (int xPatch, int yPatch, int level) const
{
	// Lod zero never has error
	if (level == 0)
		return 0.0F;
	
	float* data = new float[kPatchSize * kPatchSize];
	GetPatchData (xPatch, yPatch, level, data);
	float deltaMax = 0.0F;
	
	int skip = 1 << level;
	int xBase = xPatch * (kPatchSize - 1) *  skip;
	int yBase = yPatch * (kPatchSize - 1) *  skip;
	
	int size = (kPatchSize - 1) * skip + 1;
	float normalizeScale = 1.0F / (kPatchSize * skip);
	
	for (int x=0;x<size;x++)
	{
		for (int y=0;y<size;y++)
		{
			float fx = (float)x * normalizeScale;
			float fy = (float)y * normalizeScale;
			float interpolatedHeight = InterpolatePatchHeight (data, fx, fy);
			float realHeight = GetHeight (x + xBase, y + yBase);
			float delta = Abs(realHeight - interpolatedHeight);
			deltaMax = max(deltaMax, delta);
		}
	}

	delete[] data;
	return deltaMax;
}


Vector3f Heightmap::GetSize () const
{
	return Vector3f (m_Scale.x * (m_Width - 1), m_Scale.y, m_Scale.z * (m_Height - 1));
}

void Heightmap::SetSize (const Vector3f& size)
{
	m_Scale.x = size.x / (m_Width - 1);
	m_Scale.y = size.y;
	m_Scale.z = size.z / (m_Height - 1);
	
	PrecomputeError(0, 0, m_Width, m_Height, false);

#if ENABLE_PHYSICS
	UpdateNx ();
	RecreateShapes();
#endif
	
	m_TerrainData->SetDirty();
	m_TerrainData->UpdateUsers (TerrainData::kHeightmap);
}

void Heightmap::AwakeFromLoad ()
{
#if ENABLE_PHYSICS
	CreateNx ();
	RecreateShapes ();
#endif
}

#if ENABLE_PHYSICS
void Heightmap::RecreateShapes ()
{
	for (TerrainColliderList::iterator i=m_TerrainColliders.begin();i != m_TerrainColliders.end();)
	{
		TerrainCollider& col = **i;
		i++;
		col.Create(NULL);
	}
}
#endif

// Precompute error only on a part of the heightmap
// if forceHighestLod is enabled we simply set the error to infinity
// This casues the heightmap to be rendered at full res (Used while editing)
void Heightmap::PrecomputeError (int minX, int minY, int width, int height, bool forceHighestLod)
{
	for (int level=0;level <= m_Levels;level++)
	{
		for (int y=0;y<GetPatchCountY(level);y++)
		{
			for (int x=0;x<GetPatchCountX(level);x++)
			{
				int skip = 1 << level;
				int curXBase = x * (kPatchSize - 1) *  skip;
				int curYBase = y * (kPatchSize - 1) *  skip;
				int curPatchSize = kPatchSize * skip;
				
				// Are we in the bounds horizontally
				if (curXBase + curPatchSize < minX)
					continue;
				if (curXBase > minX + width)	
					continue;
					
				// Are we in the bounds vertically
				if (curYBase + curPatchSize < minY)
					continue;
				if (curYBase > minY + height)	
					continue;
			
				if (forceHighestLod)
				{
					m_PrecomputedError[GetPatchIndex(x, y, level)] = std::numeric_limits<float>::infinity();
				}
				else
				{
					float error = ComputeMaximumHeightError(x, y, level);	
					m_PrecomputedError[GetPatchIndex(x, y, level)] = error;
				}
				RecalculateMinMaxHeight(x, y, level);
			}
		}
	}
	m_TerrainData->SetDirty();
}

/// After editing is complete we need to recompute the error for the modified patches
/// We also update the min&max height of each patch
void Heightmap::RecomputeInvalidPatches(UNITY_TEMP_VECTOR(int)& recomputedPatches)
{
	recomputedPatches.clear();
	for (int level=0;level <= m_Levels;level++)
	{
		for (int y=0;y<GetPatchCountY(level);y++)
		{
			for (int x=0;x<GetPatchCountX(level);x++)
			{
				int patchIndex = GetPatchIndex(x, y, level);
				if (m_PrecomputedError[patchIndex] == std::numeric_limits<float>::infinity())
				{
					float error = ComputeMaximumHeightError(x, y, level);
					m_PrecomputedError[patchIndex] = error;
					RecalculateMinMaxHeight(x, y, level);

					recomputedPatches.push_back(patchIndex);
				}
			}
		}
	}
	if (!recomputedPatches.empty())
		m_TerrainData->SetDirty();
}

float Heightmap::GetMaximumHeightError (int x, int y, int level) const
{
	return m_PrecomputedError[GetPatchIndex(x, y, level)];
}


float Heightmap::Bilerp (const float* corners, float u, float v)
{
	// Corners are laid out like this
	/// 0 1
	/// 2 3
	if (u > v)
	{
		float z00 = corners[0];
		float z01 = corners[1];
		float z11 = corners[3];
		return z00 + (z01-z00) * u + (z11 - z01) * v;
	}
	else
	{
		float z00 = corners[0];
		float z10 = corners[2];
		float z11 = corners[3];
		return z00 + (z11-z10) * u + (z10 - z00) * v;
	}
}

/// The scaled interpolated height at the normalized coordinate x, y [0...1]
/// Out of bounds x and y will be clamped
float Heightmap::GetInterpolatedHeight (float x, float y) const
{
	float fx = x * (m_Width - 1);
	float fy = y * (m_Height - 1);
	int lx = (int)fx;
	int ly = (int)fy;
	
	float u = fx - lx;
	float v = fy - ly;
	
	if (u > v)
	{
		float z00 = GetHeight (lx+0, ly+0);
		float z01 = GetHeight (lx+1, ly+0);
		float z11 = GetHeight (lx+1, ly+1);
		return z00 + (z01-z00) * u + (z11 - z01) * v;
	}
	else
	{
		float z00 = GetHeight (lx+0, ly+0);
		float z10 = GetHeight (lx+0, ly+1);
		float z11 = GetHeight (lx+1, ly+1);
		return z00 + (z11-z10) * u + (z10 - z00) * v;
	}
}

// Gets the interpolated normal of the terrain at a 
float Heightmap::GetSteepness (float x, float y) const
{
	float steepness = Dot(GetInterpolatedNormal(x, y), Vector3f(0.0F, 1.0F, 0.0F));
	steepness = Rad2Deg(acos (steepness));
	return steepness;
}

// Gets the interpolated normal of the terrain at a 
Vector3f Heightmap::GetInterpolatedNormal (float x, float y) const
{
	float fx = x * (m_Width - 1);
	float fy = y * (m_Height - 1);
	int lx = (int)fx;
	int ly = (int)fy;
	
	Vector3f n00 = CalculateNormalSobel (lx+0, ly+0);
	Vector3f n10 = CalculateNormalSobel (lx+1, ly+0);
	Vector3f n01 = CalculateNormalSobel (lx+0, ly+1);
	Vector3f n11 = CalculateNormalSobel (lx+1, ly+1);
	
	float u = fx - lx;
	float v = fy - ly;
	
	Vector3f s = Lerp(n00, n10, u);
	Vector3f t = Lerp(n01, n11, u);
	Vector3f value = Lerp(s, t, v);
	value = NormalizeFast(value);
	
	return value;
}

void Heightmap::GetHeights (int xBase, int yBase, int width, int height, float* heights) const
{
	float toNormalize = 1.0F / kMaxHeight;
	for (int x=0;x<width;x++)
	{
		for (int y=0;y<height;y++)
		{
			NxHeightFieldSample sample = reinterpret_cast<const NxHeightFieldSample&> (m_Heights[(y + yBase) + (x + xBase) * m_Height]);
			float height = sample.height;
			height *= toNormalize;
			heights[y * width + x] = height;
		}
	}
}

#define SUPPORT_PHYSX_UPDATE_BLOCKS 1

void Heightmap::SetHeights (int xBase, int yBase, int width, int height, const float* heights, bool delayLodComputation)
{
	UInt32 min = 0;
	float normalizedTo16 = kMaxHeight;
	
	#if ENABLE_PHYSICS && SUPPORT_PHYSX_UPDATE_BLOCKS
	NxHeightFieldSample* nxSamples = new NxHeightFieldSample[width * height];
	int materialIndex = GetLowMaterialIndex(GetMaterialIndex());
	#endif

	for (int x=0;x<width;x++)
	{
		for (int y=0;y<height;y++)
		{
			float nHeight = heights[y * width + x] * normalizedTo16;
			SInt32 iheight = RoundfToInt(nHeight);
			iheight = clamp<SInt32> (iheight, min, kMaxHeight);

			// Update height value			
			m_Heights[(y + yBase) + (x + xBase) * m_Height] = iheight;
				
			#if ENABLE_PHYSICS && SUPPORT_PHYSX_UPDATE_BLOCKS
			// Build update buffer for novodex
			NxHeightFieldSample sample;
			sample.height = iheight;
			sample.materialIndex0 = materialIndex;
			sample.materialIndex1 = materialIndex;
			sample.tessFlag = 1;
			sample.unused = 0;
			nxSamples[height * x + y] = sample;
			#endif
		}
	}
	
	#if ENABLE_PHYSICS
	#if SUPPORT_PHYSX_UPDATE_BLOCKS
	if (m_NxHeightField)
	{
		m_NxHeightField->updateBlock(xBase, yBase, width, height, height * sizeof(NxHeightFieldSample), nxSamples);
		RecreateShapes();
	}
	
	delete[] nxSamples;
	#else
	UpdateNx ();
	RecreateShapes();
	#endif
	#endif
	
	PrecomputeError(xBase, yBase, width, height, delayLodComputation);
	
	m_TerrainData->SetDirty();
	
	m_TerrainData->UpdateUsers (delayLodComputation ? TerrainData::kDelayedHeightmapUpdate : TerrainData::kHeightmap);
}

int Heightmap::GetAdjustedSize (int size) const
{
	int levels = HighestBit( NextPowerOfTwo( size / kPatchSize ) );
	levels = max<int>(1, levels);
	return (1 << levels) * (kPatchSize - 1) + 1;
}

#if ENABLE_PHYSICS
void Heightmap::SetPhysicMaterial(PPtr<PhysicMaterial> mat)
{
	m_DefaultPhysicMaterial = mat;
	UpdateNx ();
	RecreateShapes();
}
#endif

void Heightmap::SetResolution (int resolution)
{
	m_Levels = HighestBit( NextPowerOfTwo( resolution / kPatchSize ) );
	m_Levels = max<int>(1, m_Levels);
	m_Width = (1 << m_Levels) * (kPatchSize - 1) + 1;
	m_Height = (1 << m_Levels) * (kPatchSize - 1) + 1;
	
	UInt32 materialIndex = GetLowMaterialIndex(GetMaterialIndex());//
	
	NxHeightFieldSample sample;
	sample.height = 0;
	sample.materialIndex0 = materialIndex;
	sample.materialIndex1 = materialIndex;
	sample.tessFlag = 1;
	sample.unused = 0;

	m_Heights.clear();
	m_Heights.resize(m_Width * m_Height, reinterpret_cast<UInt32&> (sample));

	m_PrecomputedError.clear();
	m_PrecomputedError.resize(GetTotalPatchCount());

	m_MinMaxPatchHeights.clear();
	m_MinMaxPatchHeights.resize(GetTotalPatchCount() * 2);
	
#if ENABLE_PHYSICS
	UpdateNx ();
	RecreateShapes();
#endif
	
	m_TerrainData->SetDirty();
	m_TerrainData->UpdateUsers (TerrainData::kHeightmap);
}

float Heightmap::GetHeight (int x, int y) const
{
	x = clamp<int>(x, 0, m_Width-1);
	y = clamp<int>(y, 0, m_Height-1);
	float scale = m_Scale.y / (float)(kMaxHeight);
	
	NxHeightFieldSample sample = reinterpret_cast<const NxHeightFieldSample&> (m_Heights[y + x * m_Height]);
	return sample.height * scale;
}

float Heightmap::GetHeightRespectingNeighbors (int x, int y, const TerrainRenderer *renderer) const
{
	const Heightmap *lookup = this;
	if(x<0)
	{
		if(renderer && renderer->m_LeftNeighbor && renderer->m_LeftNeighbor->GetTerrainData())
		{
			renderer = renderer->m_LeftNeighbor;
			lookup = &(renderer->GetTerrainData()->GetHeightmap());
			x += lookup->m_Width - 1;
		}
	}
	if(x >= lookup->m_Width)
	{
		if(renderer && renderer->m_RightNeighbor && renderer->m_RightNeighbor->GetTerrainData())
		{
			x -= lookup->m_Width - 1;
			renderer = renderer->m_RightNeighbor;
			lookup = &(renderer->GetTerrainData()->GetHeightmap());
		}
	}
	if(y<0)
	{
		if(renderer && renderer->m_BottomNeighbor && renderer->m_BottomNeighbor->GetTerrainData())
		{
			renderer = renderer->m_BottomNeighbor;
			lookup = &(renderer->GetTerrainData()->GetHeightmap());
			y += lookup->m_Height - 1;
		}
	}
	if(y >= lookup->m_Height)
	{
		if(renderer && renderer->m_TopNeighbor && renderer->m_TopNeighbor->GetTerrainData())
		{
			y -= lookup->m_Height - 1;
			renderer = renderer->m_TopNeighbor;
			lookup = &(renderer->GetTerrainData()->GetHeightmap());
		}
	}
	return lookup->GetHeight(x,y);
}

void Heightmap::UpdatePatchIndices (Mesh& mesh, int xPatch, int yPatch, int level, int edgeMask)
{	
	unsigned int count;

	unsigned short *tris = TerrainIndexGenerator::GetOptimizedIndexStrip(edgeMask, count);	
	mesh.SetIndicesComplex (tris, count, 0, kPrimitiveTriangleStripDeprecated, Mesh::k16BitIndices|Mesh::kDontSupportSubMeshVertexRanges);
}

void Heightmap::UpdatePatchMesh (Mesh& mesh, int xPatch, int yPatch, int mipLevel, int edgeMask, TerrainRenderer *renderer)
{
	int vertexCount = kPatchSize * kPatchSize;
	mesh.ResizeVertices (vertexCount, mesh.GetAvailableChannels() | VERTEX_FORMAT3(Vertex, TexCoord0, Normal));

	UpdatePatchMeshInternal(*this,
		mesh.GetVertexBegin(),
		mesh.GetNormalBegin(),
		mesh.GetUvBegin(),
		xPatch, yPatch, mipLevel, edgeMask, renderer);
	
	mesh.SetBounds(GetBounds(xPatch, yPatch, mipLevel));
	mesh.SetChannelsDirty (mesh.GetAvailableChannels(), false);
}

void Heightmap::UpdatePatchIndices (VBO& vbo, SubMesh& subMesh, int xPatch, int yPatch, int level, int edgeMask)
{	
	unsigned int count;

	unsigned short *tris = TerrainIndexGenerator::GetOptimizedIndexStrip(edgeMask, count);	
	IndexBufferData ibd = {
		tris,
		count,
		1 << kPrimitiveTriangleStripDeprecated
	};
	vbo.UpdateIndexData(ibd);
	subMesh.indexCount = count;
	subMesh.topology = kPrimitiveTriangleStripDeprecated;
}

void Heightmap::UpdatePatchMesh (VBO& vbo, SubMesh& subMesh, int xPatch, int yPatch, int mipLevel, int edgeMask, TerrainRenderer *renderer)
{
	const VertexChannelsLayout& format = VertexDataInfo::kVertexChannelsDefault;
	UInt8 normalOffset = format.channels[kShaderChannelVertex].dimension * GetChannelFormatSize(format.channels[kShaderChannelVertex].format);
	UInt8 uvOffset = format.channels[kShaderChannelNormal].dimension
					 * GetChannelFormatSize(format.channels[kShaderChannelNormal].format)
					 + normalOffset;
	UInt8 stride = format.channels[kShaderChannelTexCoord0].dimension
				   * GetChannelFormatSize(format.channels[kShaderChannelTexCoord0].format)
				   + uvOffset;

	VertexBufferData vbd;

	vbd.channels[kShaderChannelVertex].format = format.channels[kShaderChannelVertex].format;
	vbd.channels[kShaderChannelVertex].dimension = format.channels[kShaderChannelVertex].dimension;
	vbd.channels[kShaderChannelNormal].format = format.channels[kShaderChannelNormal].format;
	vbd.channels[kShaderChannelNormal].dimension = format.channels[kShaderChannelNormal].dimension;
	vbd.channels[kShaderChannelNormal].offset = normalOffset;
	vbd.channels[kShaderChannelTexCoord0].format = format.channels[kShaderChannelTexCoord0].format;
	vbd.channels[kShaderChannelTexCoord0].dimension = format.channels[kShaderChannelTexCoord0].dimension;
	vbd.channels[kShaderChannelTexCoord0].offset = uvOffset;

	// UV1 is used for lightmapping and it shares with UV0
	vbd.channels[kShaderChannelTexCoord1] = vbd.channels[kShaderChannelTexCoord0];

	vbd.streams[0].channelMask = VERTEX_FORMAT4(Vertex, Normal, TexCoord0, TexCoord1);
	vbd.streams[0].stride = stride;

	vbd.vertexCount = kPatchSize * kPatchSize;

	vbd.bufferSize = VertexDataInfo::AlignStreamSize(vbd.vertexCount * stride);
	UInt8* buffer;
	ALLOC_TEMP_ALIGNED(buffer, UInt8, vbd.bufferSize, VertexDataInfo::kVertexDataAlign);
	vbd.buffer = buffer;

	UpdatePatchMeshInternal(*this,
		StrideIterator<Vector3f>(vbd.buffer, stride),
		StrideIterator<Vector3f>(vbd.buffer + normalOffset, stride),
		StrideIterator<Vector2f>(vbd.buffer + uvOffset, stride),
		xPatch, yPatch, mipLevel, edgeMask, renderer);

	vbo.UpdateVertexData(vbd);
	subMesh.localAABB = GetBounds(xPatch, yPatch, mipLevel);
	subMesh.vertexCount = vbd.vertexCount;
}

void Heightmap::GetPatchData (int xPatch, int yPatch, int mipLevel, float* heights) const
{
	int skip = 1 << mipLevel;
	int xBase = xPatch * (kPatchSize -1);
	int yBase = yPatch * (kPatchSize -1);

	float scale = m_Scale.y / (float)(kMaxHeight);

	for (int x=0;x<kPatchSize;x++)
	{
		for (int y=0;y<kPatchSize;y++)
		{
			int index = (y + yBase) + (x + xBase) * m_Height;
			index *=  skip;
			float height = m_Heights[index];
			height *= scale;
			heights[y + x * kPatchSize] = height;
		}
	}
}

void Heightmap::RecalculateMinMaxHeight (int xPatch, int yPatch, int mipLevel)
{
	int totalSamples = kPatchSize * kPatchSize;
	float* heights = new float[totalSamples];
	GetPatchData (xPatch, yPatch, mipLevel, heights);
	float minHeight = std::numeric_limits<float>::infinity();
	float maxHeight = -std::numeric_limits<float>::infinity();
	for (int i=0;i<totalSamples;i++)
	{
		minHeight = min(minHeight, heights[i]);	
		maxHeight = max(maxHeight, heights[i]);
	}
	
	int patchIndex = GetPatchIndex(xPatch, yPatch,  mipLevel);
	m_MinMaxPatchHeights[patchIndex * 2 + 0] = minHeight / m_Scale.y ;
	m_MinMaxPatchHeights[patchIndex * 2 + 1] = maxHeight / m_Scale.y;
	
	delete[] heights;
}

AABB Heightmap::GetBounds () const
{
	return GetBounds(0, 0, m_Levels);
}


AABB Heightmap::GetBounds (int xPatch, int yPatch, int mipLevel) const
{
	int patchIndex = GetPatchIndex(xPatch, yPatch,  mipLevel);
	
	float minHeight = m_MinMaxPatchHeights[patchIndex * 2 + 0];
	float maxHeight = m_MinMaxPatchHeights[patchIndex * 2 + 1];
	
	Vector3f min;
	min.x = (xPatch) * (1 << mipLevel) * (kPatchSize -1) * m_Scale.x;
	min.z = (yPatch) * (1 << mipLevel) * (kPatchSize -1) * m_Scale.z;
	min.y = minHeight * m_Scale.y;

	Vector3f max;
	max.x = (xPatch + 1) * (1 << mipLevel) * (kPatchSize -1) * m_Scale.x;
	max.z = (yPatch + 1) * (1 << mipLevel) * (kPatchSize -1) * m_Scale.z;
	max.y = maxHeight * m_Scale.y;

	MinMaxAABB bounds = MinMaxAABB (min, max);
	return bounds;
}


int Heightmap::GetMaterialIndex () const
{
#if ENABLE_PHYSICS
	const PhysicMaterial* material = m_DefaultPhysicMaterial;
	if (material)
		return material->GetMaterialIndex ();
	else
#endif
		return 0;
}

#if ENABLE_PHYSICS
NxHeightField* Heightmap::GetNxHeightField ()
{
	if (m_NxHeightField == NULL)
		CreateNx();
	return m_NxHeightField;
}

void Heightmap::CreateNx ()
{
	IPhysics* physicsModule = GetIPhysics();

	if (!physicsModule)
		return;

	if (m_NxHeightField == NULL)
	{
		NxHeightFieldDesc desc;
		BuildDesc(desc);

		m_NxHeightField = physicsModule->CreateNxHeightField(desc);

		free(desc.samples);
	}
	else
		UpdateNx ();
}

// builds an nsdesc of the heightmap (you need to deallocate the samples array yourself)
void Heightmap::BuildDesc (NxHeightFieldDesc& desc)
{
	NxHeightFieldSample* nxSamples = (NxHeightFieldSample*)malloc (sizeof(NxHeightFieldSample) * m_Width * m_Height);
	AssertIf(!nxSamples);
	desc.nbRows = m_Width;
	desc.nbColumns = m_Height;
	desc.samples = nxSamples;
	desc.sampleStride = 4;
	// the threshold seems to be the difference between two samples in 16 height space
	// Thus a value of less than 1 doesn't make a difference
	desc.convexEdgeThreshold = 4;
	int materialIndex = GetLowMaterialIndex(GetMaterialIndex());
	
	for (int x=0;x<m_Width;x++)
	{
		for (int y=0;y<m_Height;y++)
		{
			// Build update buffer for novodex
			NxHeightFieldSample sample;
			sample.height = m_Heights[m_Height * x + y];
			sample.materialIndex0 = materialIndex;
			sample.materialIndex1 = materialIndex;
			sample.tessFlag = 1;
			sample.unused = 0;
			nxSamples[m_Height * x + y] = sample;
		}
	}
}

void Heightmap::UpdateNx ()
{
	if (!m_NxHeightField)
		return;
	
	NxHeightFieldDesc desc;
	BuildDesc(desc);
	

	m_NxHeightField->loadFromDesc(desc);
	free(desc.samples);
}

void Heightmap::CleanupNx ()
{
	if (m_NxHeightField)
	{
		GetIPhysics()->ReleaseHeightField(*m_NxHeightField);
		m_NxHeightField = NULL;
	}
}

#endif
#endif
