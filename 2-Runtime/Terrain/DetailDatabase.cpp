#include "UnityPrefix.h"
#include "DetailDatabase.h"

#if ENABLE_TERRAIN

#include "Runtime/Filters/Renderer.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Geometry/TextureAtlas.h"
#include "PerlinNoise.h"
#include "TerrainData.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Scripting/ScriptingUtility.h"


const int kClampedVertexCount = 50000;
enum { kResolutionPerPatch = 8 };

static void UpdateAtlasTextureColorSpace(Texture2D* atlasTexture, Texture2D** sourceTextures, int sourceTextureCount)
{
	for (int i = 0; i < sourceTextureCount; ++i)
	{
		if (sourceTextures[i] != NULL && sourceTextures[i]->GetStoredColorSpace() != kTexColorSpaceLinear)
		{
			atlasTexture->SetStoredColorSpaceNoDirtyNoApply (kTexColorSpaceSRGB);
			return;
		}
	}
	atlasTexture->SetStoredColorSpaceNoDirtyNoApply(kTexColorSpaceLinear);
}

// 8x8 dither table from http://en.wikipedia.org/wiki/Ordered_dithering
const float kDitherTable[] = {
	1, 49, 13, 61, 4, 52, 16, 64,
	33, 17, 45, 29, 36, 20, 48, 32,
	9, 57, 5, 53, 12, 60, 8, 56,
	41, 25, 37, 21, 44, 28, 40, 24,
	3, 51, 15, 63, 2, 50, 14, 62,
	35, 19, 47, 31, 34, 18, 46, 30,
	11, 59, 7, 55, 10, 58, 6, 54,
	43, 27, 39, 23, 42, 26, 38, 22,
};

DetailDatabase::DetailDatabase (TerrainData* terrainData, TreeDatabase* database)
{
	m_TerrainData = terrainData;
	m_TreeDatabase = database;
	m_WavingGrassTint = ColorRGBAf (0.7f, 0.6f, 0.5f, 0.0f);
	m_WavingGrassStrength = .5F;
	m_WavingGrassAmount = .5F;
	m_WavingGrassSpeed = .5F;
	m_PatchCount = 0;
	m_PatchSamples = kResolutionPerPatch;
	m_IsPrototypesDirty = true;
	m_AtlasTexture = NULL;
}

bool DetailDatabase::IsPatchEmpty (int x, int y) const
{
	return GetPatch(x,y).numberOfObjects.empty ();
}
bool DetailDatabase::IsPatchDirty (int x, int y) const
{
	return GetPatch(x,y).dirty;
}

void DetailDatabase::SetDetailResolution (int resolution, int resolutionPerPatch)
{
	m_PatchCount = clamp<int>(resolution / resolutionPerPatch, 0, 10000);
	m_PatchSamples = clamp<int>(resolutionPerPatch, 8, 1000);
	
	m_Patches.clear();
	m_Patches.resize (m_PatchCount * m_PatchCount);

	SetDirty();
} 

void DetailDatabase::SetDetailPrototypesDirty ()
{
	m_IsPrototypesDirty = true;
}

void DetailDatabase::ResetDirtyDetails ()
{
	for (int i=0;i<m_Patches.size();i++)
		m_Patches[i].dirty = false;
}

int DetailDatabase::AddLayerIndex (int detailIndex, DetailPatch &patch)
{
	for (int i=0;i<patch.layerIndices.size();i++)
	{
		if (patch.layerIndices[i] == detailIndex)
			return i;
	}
	patch.layerIndices.push_back (detailIndex);
	patch.numberOfObjects.resize (patch.numberOfObjects.size() + m_PatchSamples * m_PatchSamples);
	return patch.layerIndices.size() - 1;
}

void DetailDatabase::RemoveLocalLayerIndex (int detailIndex, DetailPatch& patch)
{
	Assert(detailIndex >= 0 || detailIndex < patch.layerIndices.size());
	
	// Remove detail index out of numberofObjectsArray
	int begin = detailIndex * m_PatchSamples * m_PatchSamples;
	patch.numberOfObjects.erase (patch.numberOfObjects.begin() + begin, patch.numberOfObjects.begin() + begin + m_PatchSamples * m_PatchSamples);
	
	patch.layerIndices.erase(patch.layerIndices.begin() + detailIndex);
}

int DetailDatabase::GetSupportedLayers (int xBase, int yBase, int totalWidth, int totalHeight, int *buffer) const
{
	if( m_PatchCount <= 0 )
	{
		ErrorString ("Terrain has zero detail resolution");
		return 0;
	}
	
	int* enabledLayers;
	int prototypeCount = m_DetailPrototypes.size ();
	ALLOC_TEMP(enabledLayers, int, prototypeCount);
	memset(enabledLayers,0,sizeof (int) * prototypeCount);
	
	int minPatchX = clamp(xBase / m_PatchSamples, 0, m_PatchCount - 1);
	int minPatchY = clamp(yBase / m_PatchSamples, 0, m_PatchCount - 1);
	int maxPatchX = clamp((xBase+totalWidth) / m_PatchSamples, 0, m_PatchCount - 1);
	int maxPatchY = clamp((yBase+totalHeight) / m_PatchSamples, 0, m_PatchCount - 1);
	
	for (int patchY=minPatchY;patchY<=maxPatchY;patchY++)
	{
		for (int patchX=minPatchX;patchX<=maxPatchX;patchX++)
		{
			int minX = clamp (xBase - patchX * m_PatchSamples, 0, m_PatchSamples - 1);
			int minY = clamp (yBase - patchY * m_PatchSamples, 0, m_PatchSamples - 1);
	
			int maxX = clamp (xBase + totalWidth - patchX * m_PatchSamples, 0, m_PatchSamples);
			int maxY = clamp (yBase + totalHeight - patchY * m_PatchSamples, 0, m_PatchSamples);
	
			int width = maxX - minX;
			int height = maxY - minY;
			if (width == 0 || height == 0)
				continue;
	
			const DetailPatch& patch = GetPatch(patchX, patchY);
			for (int l=0;l<patch.layerIndices.size();l++)
			{
				int layer = patch.layerIndices[l];
				enabledLayers[layer] = 1;
			}
		}	
	}
	
	int enabledCount = 0;
	for (int i=0;i<prototypeCount;i++)
	{
		if (enabledLayers[i])
		{
			if (buffer) 
				buffer[enabledCount] = i;
			enabledCount++;
		}
	}
	
	return enabledCount;
}


void DetailDatabase::GetLayer (int xBase, int yBase, int totalWidth, int totalHeight, int detailIndex, int *buffer) const
{
	if( m_PatchCount <= 0 )
	{
		ErrorString ("Terrain has zero detail resolution");
		return;
	}
	
	int minPatchX = clamp(xBase / m_PatchSamples, 0, m_PatchCount - 1);
	int minPatchY = clamp(yBase / m_PatchSamples, 0, m_PatchCount - 1);
	int maxPatchX = clamp((xBase+totalWidth) / m_PatchSamples, 0, m_PatchCount - 1);
	int maxPatchY = clamp((yBase+totalHeight) / m_PatchSamples, 0, m_PatchCount - 1);
	
	for (int patchY=minPatchY;patchY<=maxPatchY;patchY++)
	{
		for (int patchX=minPatchX;patchX<=maxPatchX;patchX++)
		{
			int minX = clamp(xBase - patchX * m_PatchSamples, 0, m_PatchSamples - 1);
			int minY = clamp(yBase - patchY * m_PatchSamples, 0, m_PatchSamples - 1);
	
			int maxX = clamp(xBase + totalWidth - patchX * m_PatchSamples, 0, m_PatchSamples);
			int maxY = clamp(yBase + totalHeight - patchY * m_PatchSamples, 0, m_PatchSamples);
	
			int width = maxX - minX;
			int height = maxY - minY;
			if (width == 0 || height == 0)
				continue;
	
			int xOffset = minX + patchX * m_PatchSamples - xBase;
			int yOffset = minY + patchY * m_PatchSamples - yBase;
			
			const DetailPatch &patch = GetPatch(patchX, patchY);
			
			const UInt8 *numberOfObjects = &patch.numberOfObjects[0];
			for (int l=0;l<patch.layerIndices.size();l++)
			{
				int layer = patch.layerIndices[l];
				if (layer != detailIndex)
					continue;
					
				for (int y=0;y<height;y++)
				{
					for (int x=0;x<width;x++)
					{
						int nbOfObjects = numberOfObjects[GetIndex(minX + x, minY + y, l)];
						buffer[x + xOffset + (y + yOffset) * totalWidth] = nbOfObjects;
					}
				}	
			}
		}	
	}
}

void DetailDatabase::SetLayer (int xBase, int yBase, int totalWidth, int totalHeight, int detailIndex, const int *buffer)
{
	if (detailIndex >= m_DetailPrototypes.size())
	{
		ErrorString ("Detail index out of bounds in DetailDatabase.SetLayers");
		return;
	}
	if (m_PatchCount <= 0)
	{
		ErrorString ("Terrain has zero detail resolution");
		return;
	}
	int minPatchX = clamp(xBase / m_PatchSamples, 0, m_PatchCount - 1);
	int minPatchY = clamp(yBase / m_PatchSamples, 0, m_PatchCount - 1);
	int maxPatchX = clamp((xBase+totalWidth) / m_PatchSamples, 0, m_PatchCount - 1);
	int maxPatchY = clamp((yBase+totalHeight) / m_PatchSamples, 0, m_PatchCount - 1);
	
	for (int patchY=minPatchY;patchY<=maxPatchY;patchY++)
	{
		for (int patchX=minPatchX;patchX<=maxPatchX;patchX++)
		{
			int minX = clamp(xBase - patchX * m_PatchSamples, 0, m_PatchSamples - 1);
			int minY = clamp(yBase - patchY * m_PatchSamples, 0, m_PatchSamples - 1);
	
			int maxX = clamp(xBase + totalWidth - patchX * m_PatchSamples, 0, m_PatchSamples);
			int maxY = clamp(yBase + totalHeight - patchY * m_PatchSamples, 0, m_PatchSamples);
	
			int width = maxX - minX;
			int height = maxY - minY;
			if (width == 0 || height == 0)
				continue;
	
			int xOffset = minX + patchX * m_PatchSamples - xBase;
			int yOffset = minY + patchY * m_PatchSamples - yBase;
			
			DetailPatch& patch = GetPatch(patchX, patchY);
			
			int localLayerIndex = AddLayerIndex(detailIndex, patch);
			UInt8* numberOfObjects = &patch.numberOfObjects[0];

			for (int y=0;y<height;y++)
			{
				for (int x=0;x<width;x++)
				{
					// TODO: Is this the right order?
					int nb = clamp(buffer[x + xOffset + (y + yOffset) * totalWidth], 0, 255);
					int nbIndex = GetIndex(minX + x, minY + y, localLayerIndex);
					if (nb != numberOfObjects[nbIndex])
					{
						numberOfObjects[nbIndex] = nb;
						patch.dirty = true;
					}
				}	
			}
			
			// Detect if this patch has zero details on this layer
			// In that case delete the layer completely to save space
			unsigned hasSomething = 0;
			int oneLayerSampleCount = m_PatchSamples * m_PatchSamples;
			for (int i=0;i<oneLayerSampleCount;i++)
				hasSomething += numberOfObjects[localLayerIndex * oneLayerSampleCount + i];
			
			if (hasSomething == 0)
				RemoveLocalLayerIndex(localLayerIndex, patch);
		}	
	}
	SetDirty ();
	
	// All detail renderers will reload details that have patch.dirty set
	// Then reset the patch.dirty = false on all patches.
	m_TerrainData->UpdateUsers (TerrainData::kRemoveDirtyDetailsImmediately);
	ResetDirtyDetails();
}

void DetailDatabase::CleanupPrototype (DetailPrototype &proto, string const& error)
{
	proto.vertices.clear();
	proto.uvs.clear();
	proto.colors.clear();
	proto.triangles.clear();
}


DetailDatabase::~DetailDatabase ()
{
	DestroySingleObject(m_AtlasTexture);
}

namespace DetailDatabase_Static
{
static SHADERPROP(MainTex);
} // namespace DetailDatabase_Static
 
#if UNITY_EDITOR
// For thread loading we need to know the textures. Going through the meshes then textures is not thread safe.
void DetailDatabase::SetupPreloadTextureAtlasData ()
{
	m_PreloadTextureAtlasData.resize(m_DetailPrototypes.size());

	Texture2D** sourceTextures;
	ALLOC_TEMP(sourceTextures, Texture2D*, m_DetailPrototypes.size());
	
	RefreshPrototypesStep1(sourceTextures);

	for (int i=0;i<m_DetailPrototypes.size();i++)
	{
		m_PreloadTextureAtlasData[i] = sourceTextures[i];
		if (sourceTextures[i] == NULL)
		{
			WarningString("Missing detail texture in Terrain, degraded loading performance");
			m_PreloadTextureAtlasData.clear();
			break;
		}
	}
	
	SetDetailPrototypesDirty();
}

#endif

void DetailDatabase::GenerateTextureAtlasThreaded ()
{
	if (!m_PreloadTextureAtlasData.empty())
	{
		AssertIf(m_PreloadTextureAtlasData.size() != m_DetailPrototypes.size());
		
		Texture2D** sourceTextures;
		ALLOC_TEMP(sourceTextures, Texture2D*, m_PreloadTextureAtlasData.size());
		
		int i;
		for (i=0;i<m_PreloadTextureAtlasData.size();i++)
		{
			Texture2D* tex = dynamic_pptr_cast<Texture2D*> (InstanceIDToObjectThreadSafe(m_PreloadTextureAtlasData[i].GetInstanceID()));
			if (tex == NULL)
				break;
			sourceTextures[i] = tex;
		}
		
		if (i == m_PreloadTextureAtlasData.size())
		{
			AssertIf (m_AtlasTexture != NULL);

			m_AtlasTexture = NEW_OBJECT_FULL(Texture2D, kCreateObjectFromNonMainThread);
			m_AtlasTexture->Reset();
			m_AtlasTexture->AwakeFromLoadThreaded();
			
			// ok, just from performance standpoint we don't want to upload texture here
			// or get an assert from uninited texture, so, let's cheat
			m_AtlasTexture->HackSetAwakeDidLoadThreadedWasCalled();

			m_PreloadTextureAtlasUVLayout.resize(m_PreloadTextureAtlasData.size());
			
			UpdateAtlasTextureColorSpace(m_AtlasTexture, sourceTextures, m_PreloadTextureAtlasData.size());
			PackTextureAtlasSimple (m_AtlasTexture, 2048, m_PreloadTextureAtlasData.size(), sourceTextures, &m_PreloadTextureAtlasUVLayout[0], 0, false, false);
		}
	}
}

void DetailDatabase::RefreshPrototypesStep1 (Texture2D** sourceTextures)
{
	using namespace DetailDatabase_Static;

	for (int i=0;i<m_DetailPrototypes.size();i++)
	{
		DetailPrototype& proto = m_DetailPrototypes[i];
		sourceTextures[i] = NULL;
		
		GameObject *prototype = proto.prototype;
		if (proto.usePrototypeMesh && prototype)
		{
			Renderer* renderer = prototype->QueryComponent (Renderer);
			if (renderer == NULL)
			{
				CleanupPrototype(proto, Append("Missing renderer ", prototype->GetName()));
				continue;
			}
			
			if (renderer->GetMaterialCount() != 1)
			{
				CleanupPrototype(proto, Append(proto.prototype->GetName(), " must have exactly one material."));
				continue;
			}
			
			Material *sharedMaterial = renderer->GetMaterial (0);
			if (sharedMaterial == NULL)
			{
				CleanupPrototype(proto, Append("Missing material ", proto.prototype->GetName()));
				continue;
			}
			
			MeshFilter *filter = prototype->QueryComponent(MeshFilter);	
			if (filter == NULL)
			{
				CleanupPrototype(proto, Append("Missing mesh filter ", proto.prototype->GetName()));
				continue;
			}

			Mesh* mesh = filter->GetSharedMesh();
			if (mesh == NULL)
			{
				CleanupPrototype(proto, Append ("Missing mesh ", proto.prototype->GetName()));
				continue;
			}
			
			proto.vertices.assign (mesh->GetVertexBegin(), mesh->GetVertexEnd());
			if (proto.vertices.empty())
			{
				CleanupPrototype(proto, Append ("No vertices available ", prototype->GetName()));
				continue;
			}

			// Colors and normals are not optional here. Default to something
			if (mesh->IsAvailable (kShaderChannelColor))
			{
				proto.colors.assign (mesh->GetColorBegin (), mesh->GetColorEnd () );
			} 
			else
			{
				proto.colors.clear ();				
				proto.colors.resize (mesh->GetVertexCount(), ColorRGBA32(0xFFFFFFFF));
			} 

			if (mesh->IsAvailable (kShaderChannelNormal))
			{	
				proto.normals.assign(mesh->GetNormalBegin (), mesh->GetNormalEnd ());
			}
			else
			{
				proto.normals.clear ();				
				proto.normals.resize (mesh->GetVertexCount(), Vector3f(0,1,0));
			}

			if (mesh->IsAvailable (kShaderChannelTexCoord0))
			{	
				proto.uvs.assign (mesh->GetUvBegin(0), mesh->GetUvEnd(0));
			}
			else
			{
				CleanupPrototype(proto, Append("No uvs available ", proto.prototype->GetName()));
				continue;
			}

			Mesh::TemporaryIndexContainer tempBuffer;
			mesh->GetTriangles (tempBuffer);
			proto.triangles.assign (tempBuffer.begin(), tempBuffer.end());

			if (proto.triangles.empty())
			{
				CleanupPrototype(proto, Append("No triangles available ", proto.prototype->GetName()));
				continue;
			}

			if (sharedMaterial)
				sourceTextures[i] = dynamic_pptr_cast <Texture2D*>(sharedMaterial->GetTexture (kSLPropMainTex));
		}
		// We don't have a mesh, but we have a texture: it's grass quads
		else if( !proto.usePrototypeMesh && proto.prototypeTexture.IsValid() )
		{
			float halfWidth = 0.5F;
			float height = 1.0F;
			// color modifier at the top of the grass.
			// billboard top vertex color = topColor * perlinNoise * 2
			// Was 1.5f before we doublemultiplied
			ColorRGBA32 topColor = GfxDevice::ConvertToDeviceVertexColor( ColorRGBA32 (255, 255, 255, 255) );
			ColorRGBA32 bottomColor = GfxDevice::ConvertToDeviceVertexColor( ColorRGBA32 (160,160,160, 0) );

			Vector3f vertices[] = { 
				Vector3f (-halfWidth, 0, 0),
				Vector3f (-halfWidth, height, 0),
				Vector3f (halfWidth, height, 0),
				Vector3f (halfWidth, 0, 0),
			};

			ColorRGBA32 colors[] = {
				bottomColor, topColor, topColor, bottomColor,
			};
			Vector2f uvs[] = {
				Vector2f (0, 0), Vector2f (0, 1), Vector2f (1, 1), Vector2f (1, 0),
			};
			UInt16 triangles[] = {
				0, 1, 2, 2, 3, 0,
			};

			const int actualVertexCount = 4;
			const int actualIndexCount = 6;

			// skip normals creation, since they will be taken from the terrain in GenerateMesh()

			proto.vertices.assign (vertices, vertices + actualVertexCount);
			proto.colors.assign (colors, colors + actualVertexCount);
			proto.uvs.assign (uvs, uvs + actualVertexCount);
			proto.triangles.assign (triangles, triangles + actualIndexCount);
			sourceTextures[i] = proto.prototypeTexture;
		}
		else
		{
			if (proto.prototype)
				CleanupPrototype(proto, Append("Missing prototype ", proto.prototype->GetName()));
			else
				CleanupPrototype(proto, "Missing prototype");
			continue;
		}
	}
}


void DetailDatabase::RefreshPrototypes ()
{
	Texture2D** sourceTextures;
	ALLOC_TEMP(sourceTextures, Texture2D*, m_DetailPrototypes.size());

	RefreshPrototypesStep1(sourceTextures);

	// Normal non-threaded creation mode
	if (m_AtlasTexture == NULL || m_AtlasTexture->IsInstanceIDCreated())
	{
		// Not created yet 
		if (m_AtlasTexture == NULL)
		{
			m_AtlasTexture = CreateObjectFromCode<Texture2D>();
			m_AtlasTexture->SetHideFlags (Object::kHideAndDontSave);
			m_AtlasTexture->InitTexture(2, 2, kTexFormatARGB32, Texture2D::kMipmapMask, 1);
			m_AtlasTexture->SetWrapMode(kTexWrapClamp);
		}
		
		// TODO: Make 4096 a property & clamp to GFX card, detail settings
		Rectf* rects;
		ALLOC_TEMP(rects, Rectf, m_DetailPrototypes.size());
		
		UpdateAtlasTextureColorSpace(m_AtlasTexture, sourceTextures, m_DetailPrototypes.size());
		PackTextureAtlasSimple (m_AtlasTexture, 2048, m_DetailPrototypes.size(), sourceTextures, rects, 0, true, false);
		
		for (int i=0;i<m_DetailPrototypes.size();i++)
		{
			DetailPrototype &proto = m_DetailPrototypes[i];
			Rectf r = rects[i];
			float w = r.Width();
			float h = r.Height();
			for (int v=0;v<proto.uvs.size();v++)
			{
				proto.uvs[v].x = proto.uvs[v].x * w + r.x;
				proto.uvs[v].y = proto.uvs[v].y * h + r.y;
			}
		}
	}
	// Generated in loading thread - Just upload
	else
	{
		Object::AllocateAndAssignInstanceID(m_AtlasTexture);
		m_AtlasTexture->SetHideFlags (Object::kHideAndDontSave);
		m_AtlasTexture->SetWrapMode( kTexWrapClamp );
		
		AssertIf (m_PreloadTextureAtlasUVLayout.size() != m_DetailPrototypes.size());
		for (int i=0;i<m_DetailPrototypes.size();i++)
		{
			DetailPrototype &proto = m_DetailPrototypes[i];
			Rectf r = m_PreloadTextureAtlasUVLayout[i];
			float w = r.Width();
			float h = r.Height();
			for (int v=0;v<proto.uvs.size();v++)
			{
				proto.uvs[v].x = proto.uvs[v].x * w + r.x;
				proto.uvs[v].y = proto.uvs[v].y * h + r.y;
			}
		}
		
		m_AtlasTexture->AwakeFromLoad(kDefaultAwakeFromLoad);
	}
	
	m_IsPrototypesDirty = false;
}


void DetailDatabase::SetDirty ()
{
	m_TerrainData->SetDirty ();
}

void DetailDatabase::SetDetailPrototypes (const vector<DetailPrototype> & detailPrototypes)
{
	m_DetailPrototypes = detailPrototypes;
	RefreshPrototypes ();
	SetDirty ();
	m_TerrainData->UpdateUsers (TerrainData::kFlushEverythingImmediately);
}

void DetailDatabase::RemoveDetailPrototype (int index)
{
	#if UNITY_EDITOR
	
	if( index < 0 || index >= m_DetailPrototypes.size() )
	{
		ErrorString("invalid detail prototype index");
		return;
	}
	
	// erase detail prototype
	m_DetailPrototypes.erase( m_DetailPrototypes.begin() + index );
	
	// update detail patches
	for( size_t i = 0; i < m_Patches.size(); ++i )
	{
		DetailPatch& patch = m_Patches[i];
		int localIndex = -1;
		for( size_t j = 0; j < patch.layerIndices.size(); ++j )
		{
			if( patch.layerIndices[j] == index )
				localIndex = j;
			else if( patch.layerIndices[j] > index )
				--patch.layerIndices[j];
		}
		if( localIndex == -1 )
			continue;
		
		AssertIf( patch.numberOfObjects.size() != patch.layerIndices.size() * m_PatchSamples * m_PatchSamples );
		
		patch.layerIndices.erase( patch.layerIndices.begin() + localIndex );
		patch.numberOfObjects.erase(
			patch.numberOfObjects.begin() + localIndex * m_PatchSamples * m_PatchSamples,
			patch.numberOfObjects.begin() + (localIndex+1) * m_PatchSamples * m_PatchSamples );
	}
	
	RefreshPrototypes ();
	SetDirty();
	m_TerrainData->UpdateUsers (TerrainData::kFlushEverythingImmediately);
	
	#else
	ErrorString("only implemented in editor");
	#endif
}

static void CopyVertex (Vector3f *src, Vector3f *dst, const Matrix4x4f &transform, int offset, int count)
{
	for (int i=0;i<count;i++)
		dst[i+offset] = transform.MultiplyPoint3(src[i]);
}

static void CopyVertex (Vector3f pos, Vector3f *dst, int offset, int count)
{
	for (int i=0;i<count;i++)
		dst[i+offset] = pos;
}

static void CopyNormal (Vector3f* src, Vector3f* dst, Quaternionf rot, int offset, int count)
{
	for (int i=0;i<count;i++)
		dst[i+offset] = RotateVectorByQuat (rot, src[i]);
}

static void CopyNormalFromTerrain (const Heightmap& heightmap, float normalizedX, float normalizedZ, Vector3f* dst, int offset, int count)
{
	Vector3f terrainNormal = heightmap.GetInterpolatedNormal(normalizedX, normalizedZ);
	
	for (int i = 0; i < count; i++)
		dst[i+offset] = terrainNormal;
}

static void CopyUV (Vector2f* src, Vector2f *dst, int offset, int count)
{
	for (int i=0;i<count;i++)
		dst[i+offset] = src[i];
}

static void CopyTangents (Vector2f* src, Vector4f *dst, int offset, int count)
{
	for (int i = 0; i < count; i++)
	{
		Vector2f srcVector = src[i];
		Vector4f& dstVector = dst[i+offset];

		dstVector.x = srcVector.x;
		dstVector.y = srcVector.y;
	}
}

static void CopyUVFromTerrain (float normalizedX, float normalizedZ, Vector2f *dst, int offset, int count)
{
	Vector2f lightmapUV(normalizedX, normalizedZ);
	for (int i = 0; i < count; i++)
		dst[i+offset] = lightmapUV;
}

inline ColorRGBA32 MultiplyDouble (const ColorRGBA32 &inC0, const ColorRGBA32 &inC1) 
{
	return ColorRGBA32 (
		std::min (((int)inC0.r * (int)inC1.r) / 128, 255), 
		std::min (((int)inC0.g * (int)inC1.g) / 128, 255), 
		std::min (((int)inC0.b * (int)inC1.b) / 128, 255), 
		std::min (((int)inC0.a * (int)inC1.a) / 128, 255)
	);
}

static void CopyColor (ColorRGBA32* src, ColorRGBA32* dst, ColorRGBA32 scale, int offset, int count)
{
	for (int i=0;i<count;i++)
		dst[i+offset] = src[i] * scale;
}

/*Rectf DetailDatabase::GetNormalizedArea (int x, int y)
{
	float fx = (float)x / m_PatchCount;
	float fy = (float)y / m_PatchCount;
	float size = 1.0F / m_PatchCount;
	return Rectf (fx, fy, fx+size, fy+size);
}
*/
/*
void DetailDatabase::GenerateBounds (DetailPatch &patch, int patchX, int patchY)
{
	if (patch.numberOfObjects.size() != 0)
	{
		Mesh mesh = new Mesh ();
		
		GenerateMesh (mesh, patch, patchX, patchY, m_Heightmap.size, false, DetailRenderMode.Grass);
		patch.bounds = mesh.bounds;
//			Debug.Log(patch.bounds.min);
//			Debug.Log(patch.bounds.max);
		DestroyImmediate (mesh);
	}
}
*/

PROFILER_INFORMATION(gBuildDetailMesh, "Terrain.Details.BuildPatchMesh", kProfilerRender)
PROFILER_INFORMATION(gExtractLightmap, "DetailMesh.ExtractLightmap", kProfilerRender);
PROFILER_INFORMATION(gSetup, "DetailMesh.Setup", kProfilerRender);
PROFILER_INFORMATION(gBuildData, "DetailMesh.BuildData", kProfilerRender);
PROFILER_INFORMATION(gAssignToMesh, "DetailMesh.AssignToMesh", kProfilerRender);

Mesh* DetailDatabase::BuildMesh (int patchX, int patchY, Vector3f size, int lightmapIndex, DetailRenderMode renderMode, float density)
{
	int totalTriangleCount, totalVertexCount;
	
	PROFILER_AUTO(gBuildDetailMesh, NULL)

	DetailPatch &patch = GetPatch (patchX, patchY);
	ComputeVertexAndTriangleCount(patch, renderMode, density, &totalVertexCount, &totalTriangleCount);
	if (totalTriangleCount == 0 || totalVertexCount == 0)
		return NULL;
	else
	{
		Mesh* mesh = NEW_OBJECT(Mesh);
		mesh->Reset();
		mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

		mesh->SetHideFlags(Object::kHideAndDontSave);
		GenerateMesh (*mesh, patchX, patchY, size, lightmapIndex, renderMode, density, totalVertexCount, totalTriangleCount);
		return mesh;
	}
} 

// Fixes bounds of the patch of billboards. Since each quad making a billboard
// has its vertices collapsed in one point, the bounding box does not take
// the height nor the width of the billboard into account.
inline void ExpandDetailBillboardBounds(Mesh& mesh, float detailMaxHalfWidth, float detailMaxHeight)
{
	// The origin of the billboard is in the middle of the bottom edge.
	AABB aabb = mesh.GetBounds();

	// The billboard always faces the camera, so when looking from the top
	// it's the height of the billboard that extends in the XZ plane.
	float maxHalfWidth = std::max(detailMaxHalfWidth, detailMaxHeight);

	aabb.m_Extent += Vector3f(maxHalfWidth, 0.5f * detailMaxHeight, maxHalfWidth);
	aabb.m_Center += Vector3f(0, 0.5f * detailMaxHeight, 0);
	mesh.SetBounds(aabb);
}

void DetailDatabase::GenerateMesh (Mesh& mesh, int patchX, int patchY, Vector3f size, int lightmapIndex, DetailRenderMode renderMode, float density, int totalVertexCount, int totalTriangleCount)
{
	PROFILER_BEGIN_INTERNAL(gSetup, NULL);
	DetailPatch &patch = GetPatch (patchX, patchY);
	Vector3f* vertices;
	ALLOC_TEMP(vertices, Vector3f, totalVertexCount);

	Vector2f* uvs;
	ALLOC_TEMP(uvs, Vector2f, totalVertexCount);

	int uv2Count = totalVertexCount;

	Vector2f* uvs2 = NULL;
	ALLOC_TEMP(uvs2, Vector2f, uv2Count);

	int tangentCount = 0;
	if (renderMode == kDetailBillboard)
		tangentCount = totalVertexCount;

	Vector4f* tangents = NULL;
	ALLOC_TEMP(tangents, Vector4f, tangentCount);

	ColorRGBA32* colors;
	ALLOC_TEMP(colors, ColorRGBA32, totalVertexCount);
	
	int normalCount = totalVertexCount;
	Vector3f* normals = NULL;
	ALLOC_TEMP (normals, Vector3f, normalCount);
		
	UInt16* triangles;
	ALLOC_TEMP (triangles, UInt16, totalTriangleCount);

	int triangleCount = 0;
	int vertexCount = 0;
	float randomResolutionSize = 1.0F / GetResolution();
	int res = m_PatchSamples;

	Heightmap* heightmap = &m_TerrainData->GetHeightmap();
///	int samplesPerHeixel = m_SamplesPerHeixel;
///	int heixels = m_SamplesPerHeixel;
		
//	int xBaseHeightmap = patchX * m_PatchSamples / m_SamplesPerHeixel;
//	int yBaseHeightmap = patchY * m_PatchSamples / m_SamplesPerHeixel;

	PROFILER_END_INTERNAL;
		
	PROFILER_BEGIN_INTERNAL(gBuildData, NULL);

	float detailMaxHalfWidth = 0.0f;
	float detailMaxHeight = 0.0f;

	for (int i=0;i<patch.layerIndices.size();i++)
	{
		DetailPrototype& prototype = m_DetailPrototypes[patch.layerIndices[i]];
		
		if (prototype.renderMode != renderMode)
			continue;
		
		Vector3f* prototypeVertices = prototype.vertices.size() > 0 ? &prototype.vertices[0] : NULL;
		Vector3f* prototypeNormals = prototype.normals.size() > 0 ? &prototype.normals[0] : NULL;
		Vector2f* prototypeUvs = prototype.uvs.size() > 0 ? &prototype.uvs[0] : NULL;
		ColorRGBA32* prototypeColors = prototype.colors.size() > 0 ? &prototype.colors[0] : NULL;
		UInt16* prototypeTris = prototype.triangles.size() > 0 ? &prototype.triangles[0] : NULL;
		float noiseSpread = prototype.noiseSpread;
		ColorRGBAf dry = prototype.dryColor; 
		ColorRGBAf healthy = prototype.healthyColor;
		
		float halfGrassWidth = prototype.minWidth * 0.5F;
		float halfGrassWidthDelta = (prototype.maxWidth - prototype.minWidth) * .5F;
		float grassHeight = prototype.minHeight;
		float grassHeightDelta = prototype.maxHeight - prototype.minHeight;
		int prototypeTrisSize = prototype.triangles.size();
		int prototypeVerticesSize = prototype.vertices.size();
		
		if (prototypeVerticesSize == 0)
			continue;
		
		for (int y=0;y<res;y++)
		{
			for (int x=0;x<res;x++)
			{
				int nbIndex = y * res + x + i * res * res;
				int origCount = patch.numberOfObjects[nbIndex];
				if (origCount == 0)
					continue;
				
				float nx = (float)patchX / m_PatchCount + (float)x / (res * m_PatchCount);
				float ny = (float)patchY / m_PatchCount + (float)y / (res * m_PatchCount);
				m_Random.SetSeed (nbIndex + (patchX * m_PatchCount + patchY) * 1013);
				
				// Clamp the number of genrated details to not generate more than kClampedVertex vertices
				int maxCount = (kClampedVertexCount - vertexCount) / prototypeVerticesSize;
				origCount = std::min(maxCount, origCount);

				int newCount = (int)(origCount * density + (kDitherTable[(x&7)*8+(y&7)] - 0.5f) / 64.0f);
				for (int k=0;k<newCount;k++)
				{
					// Generate position & rotation
					
					float normalizedX = nx + m_Random.GetFloat() * randomResolutionSize;
					float normalizedZ = ny + m_Random.GetFloat() * randomResolutionSize;
					
					// normalizedX = nx + 0.5F * randomSize;
					// normalzedZ = ny + 0.5F * randomSize;
					Vector3f pos;
					pos.y = heightmap->GetInterpolatedHeight (normalizedX, normalizedZ);
					pos.x = normalizedX * size.x;
					pos.z = normalizedZ * size.z;
					
					float noise = PerlinNoise::NoiseNormalized(pos.x * noiseSpread, pos.z * noiseSpread);
					ColorRGBA32 healthyDryColor = Lerp (dry, healthy, noise);
					healthyDryColor = GfxDevice::ConvertToDeviceVertexColor (healthyDryColor);

					// set second UVs to point to the fragment of the terrain lightmap underneath the detail mesh
					CopyUVFromTerrain (normalizedX, normalizedZ, uvs2, vertexCount, prototypeVerticesSize);
					
					if (renderMode == kDetailBillboard)
					{
						DebugAssertIf (prototypeVerticesSize != 4);
						DebugAssertIf (prototypeTrisSize != 6);

						float grassX = halfGrassWidth + halfGrassWidthDelta * noise;
						float grassY = grassHeight + grassHeightDelta * noise;

						detailMaxHalfWidth = std::max(detailMaxHalfWidth, grassX);
						detailMaxHeight = std::max(detailMaxHeight, grassY);
						
						Vector2f billboardSize[] = 
						{ 
							Vector2f (-grassX, 0),
							Vector2f (-grassX, grassY),
							Vector2f (grassX, grassY),
							Vector2f (grassX, 0) 
						};
						
						CopyVertex (pos, vertices, vertexCount, prototypeVerticesSize);
						CopyUV (prototypeUvs, uvs, vertexCount, prototypeVerticesSize);

						// used for offsetting vertices in the vertex shader
						CopyTangents (billboardSize, tangents, vertexCount, prototypeVerticesSize);

						CopyColor (prototypeColors, colors, healthyDryColor, vertexCount, prototypeVerticesSize);

						CopyNormalFromTerrain(*heightmap, normalizedX, normalizedZ, normals, vertexCount, prototypeVerticesSize);
						
						for (int t=0;t<prototypeTrisSize;t++)
							triangles[t+triangleCount] = prototypeTris[t] + vertexCount;
												
						triangleCount += prototypeTrisSize;
						vertexCount += prototypeVerticesSize;
					}
					else
					{
						Quaternionf rot = AxisAngleToQuaternion (Vector3f (0,1,0), m_Random.GetFloat() * 6.2831852f);
						float scaleX = Lerp (prototype.minWidth, prototype.maxWidth, noise);
						float scaleZ = Lerp (prototype.minHeight, prototype.maxHeight, noise);
						Vector3f scale = Vector3f (scaleX, scaleZ, scaleX);
						Matrix4x4f transform;
						transform.SetTRS (pos, rot, scale);
						
						CopyVertex (prototypeVertices, vertices, transform, vertexCount, prototypeVerticesSize);
						CopyUV (prototypeUvs, uvs, vertexCount, prototypeVerticesSize);
						CopyColor (prototypeColors, colors, healthyDryColor, vertexCount, prototypeVerticesSize);

						if (renderMode == kDetailMeshGrass)
						{
							CopyNormalFromTerrain(*heightmap, normalizedX, normalizedZ, normals, vertexCount, prototypeVerticesSize);
						}
						else if (normals != NULL)
						{
							CopyNormal (prototypeNormals, normals, rot, vertexCount, prototypeVerticesSize);
						}
						
						for (int t=0;t<prototypeTrisSize;t++)
							triangles[t+triangleCount] = prototypeTris[t] + vertexCount;
												
						triangleCount += prototypeTrisSize;
						vertexCount += prototypeVerticesSize;
					}
				}
			}
		}
	}
	PROFILER_END_INTERNAL;
	
	PROFILER_BEGIN_INTERNAL(gAssignToMesh, NULL);

	AssertIf(triangleCount != totalTriangleCount);
	AssertIf(vertexCount != totalVertexCount);
	
	// Assign the mesh
	mesh.Clear(true);
	unsigned meshFormat = VERTEX_FORMAT5 (Vertex, Normal, Color, TexCoord0, TexCoord1);
	if (renderMode == kDetailBillboard)
		meshFormat |= VERTEX_FORMAT1 (Tangent);
	
	mesh.ResizeVertices (vertexCount, meshFormat);
	strided_copy (vertices, vertices + vertexCount, mesh.GetVertexBegin ());
	strided_copy (colors, colors + vertexCount, mesh.GetColorBegin ());
	strided_copy (normals, normals + vertexCount, mesh.GetNormalBegin ());
	strided_copy (uvs, uvs + vertexCount, mesh.GetUvBegin (0));
	strided_copy (uvs2, uvs2 + vertexCount, mesh.GetUvBegin (1));
	if (renderMode == kDetailBillboard)
		strided_copy (tangents, tangents + vertexCount, mesh.GetTangentBegin ());

	mesh.SetIndicesComplex (triangles, triangleCount, 0, kPrimitiveTriangles, Mesh::k16BitIndices);
	mesh.SetChannelsDirty (mesh.GetAvailableChannels (), true);
	mesh.RecalculateBounds ();
	
	if (renderMode == kDetailBillboard)
		ExpandDetailBillboardBounds(mesh, detailMaxHalfWidth, detailMaxHeight);

	PROFILER_END_INTERNAL;
}
	
void DetailDatabase::ComputeVertexAndTriangleCount(DetailPatch &patch, DetailRenderMode renderMode, float density, int* vertexCount, int* triangleCount)
{
	*triangleCount = 0;
	*vertexCount = 0;
	int res = m_PatchSamples;

	for (int i=0;i<patch.layerIndices.size();i++)
	{
		DetailPrototype &prototype = m_DetailPrototypes[patch.layerIndices[i]];
		if (prototype.renderMode != renderMode)
			continue;
			
		if (prototype.vertices.empty()) 
			continue;

		int count = 0;
		for (int y=0;y<res;y++)
		{
			for (int x=0;x<res;x++)
			{
				int nbIndex = y * res + x + i * res * res;
				int origCount = patch.numberOfObjects[nbIndex];
				if (!origCount)
					continue;
				int newCount = (int)(origCount * density + (kDitherTable[(x&7)*8+(y&7)] - 0.5f) / 64.0f);
				count += newCount;
			}
		}

		
		
		// Clamp the number of genrated details to not generate more than kClampedVertex vertices
		int maxCount = (kClampedVertexCount - *vertexCount) / prototype.vertices.size();
		count = std::min(maxCount, count);
		
		*triangleCount += prototype.triangles.size() * count;
		*vertexCount += prototype.vertices.size() * count;
	}
}


void DetailDatabase::UpdateDetailPrototypesIfDirty ()
{
	if (m_IsPrototypesDirty)
		RefreshPrototypes();
}

#if UNITY_EDITOR
/*
AssetDatabase::RegisterPostprocessCallback (Postprocess);
static void Postprocess (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& removed, const std::set<UnityGUID>& moved);
{
	for (std::set<UnityGUID>::iterator i=refresh.begin();i != refreshed.end();i++)
	{
		PPtr<Texture> tex = dynamic_pptr_cast<Texture*> (GetMainAsset());
		if (tex)
		{
			vector<TerrainData*> data;
			FindObjectsOfType(data);
			data.GetDetailDatabase().RefreshPrototypes();
		}
	}
	
}
*/
#endif



void DetailPrototypeToMono (const DetailPrototype &src, MonoDetailPrototype &dest) {
	dest.prototype = Scripting::ScriptingWrapperFor (src.prototype);
	dest.prototypeTexture = Scripting::ScriptingWrapperFor (src.prototypeTexture);
	dest.healthyColor = src.healthyColor;
	dest.dryColor = src.dryColor;
	dest.minWidth = src.minWidth;
	dest.maxWidth = src.maxWidth;
	dest.minHeight = src.minHeight;
	dest.maxHeight = src.maxHeight;
	dest.noiseSpread = src.noiseSpread;
	dest.bendFactor = src.bendFactor;
	dest.renderMode = src.renderMode;
	dest.usePrototypeMesh = src.usePrototypeMesh;
	
}
void DetailPrototypeToCpp (MonoDetailPrototype &src, DetailPrototype &dest) {
	dest.prototype = ScriptingObjectToObject<GameObject> (src.prototype);
	dest.prototypeTexture = ScriptingObjectToObject<Texture2D> (src.prototypeTexture);
	dest.healthyColor = src.healthyColor;
	dest.dryColor = src.dryColor;
	dest.minWidth = src.minWidth;
	dest.maxWidth = src.maxWidth;
	dest.minHeight = src.minHeight;
	dest.maxHeight = src.maxHeight;
	dest.noiseSpread = src.noiseSpread;
	dest.bendFactor = src.bendFactor;
	dest.renderMode = src.renderMode;
	dest.usePrototypeMesh = src.usePrototypeMesh;
}


#endif // ENABLE_TERRAIN
