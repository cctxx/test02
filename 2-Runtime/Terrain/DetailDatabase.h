#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/Geometry/AABB.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Math/Rect.h"
#include <vector>
#include "TreeDatabase.h"

class TerrainData;
class Heightmap;
class Mesh;

using std::vector;

struct DetailPatch 
{
  public:
	DECLARE_SERIALIZE (DetailPatch)
	AABB bounds;

	bool    dirty;

	vector<UInt8> layerIndices;
	vector<UInt8> numberOfObjects;
	
	DetailPatch () { dirty = false; }
};

template<class TransferFunc>
void DetailPatch::Transfer (TransferFunc& transfer)
{
	TRANSFER (bounds);
	TRANSFER (layerIndices);
	TRANSFER (numberOfObjects);
}

enum DetailRenderMode
{ 
	kDetailBillboard = 0,	// billboard
	kDetailMeshLit,			// just a mesh, lit like everything else
	kDetailMeshGrass,		// mesh (user supplied or generated grass crosses), waves in the wind
	kDetailRenderModeCount
};

struct DetailPrototype
{
	DECLARE_SERIALIZE (DetailPrototype)
	PPtr<GameObject> prototype;
	PPtr<Texture2D>    prototypeTexture;

	float      minWidth, maxWidth;		///< Width of the grass billboards (if renderMode is grassBillboard)
	float      minHeight, maxHeight;		///< Height of the grass billboards (if renderMode is grassBillboard)	
	float      noiseSpread;
	float      bendFactor;
	ColorRGBAf healthyColor;
	ColorRGBAf dryColor;
	float	   lightmapFactor;
	int        renderMode;
	int        usePrototypeMesh;

	vector<Vector3f> vertices;
	vector<Vector3f> normals;
	vector<Vector2f> uvs;
	vector<ColorRGBA32> colors;
	vector<UInt16>     triangles;
	
	DetailPrototype () :
		healthyColor (67/255.0F, 249/255.0F, 42/255.0F, 1 ),
		dryColor(205/255.0F, 188/255.0F, 26/255.0F, 1.0F ) 
	{
		minWidth = 1.0F;
		maxWidth = 2.0F;
		minHeight = 1.0F;
		maxHeight = 2.0F;
		noiseSpread = 10.0F;
		bendFactor = 1.0F;
		lightmapFactor = 1.0F;
		renderMode = kDetailMeshGrass;
		usePrototypeMesh = false;
	}
};

template<class TransferFunc>
void DetailPrototype::Transfer (TransferFunc& transfer)
{
	transfer.SetVersion(2);
	
	TRANSFER (prototype);
	TRANSFER (prototypeTexture);
	TRANSFER (minWidth);
	TRANSFER (maxWidth);
	TRANSFER (minHeight);
	TRANSFER (maxHeight);
	TRANSFER (noiseSpread);
	TRANSFER (bendFactor);
	TRANSFER (healthyColor);
	TRANSFER (dryColor);
	TRANSFER (lightmapFactor);
	TRANSFER (renderMode);
	TRANSFER (usePrototypeMesh);

	if (transfer.IsOldVersion(1))
	{
		if (prototype)
			usePrototypeMesh = 1;
		else
			usePrototypeMesh = 0;
	}
}


class DetailDatabase
{
  public:
	DetailDatabase (TerrainData* terrainData, TreeDatabase* treeDatabase);
	~DetailDatabase ();
  
	DECLARE_SERIALIZE (DetailDatabase)

	Texture2D* GetAtlasTexture () { return m_AtlasTexture; }
	GET_SET (ColorRGBAf, WavingGrassTint, m_WavingGrassTint);
	GET_SET (float, WavingGrassStrength, m_WavingGrassStrength);
	GET_SET (float, WavingGrassAmount, m_WavingGrassAmount);
	GET_SET (float, WavingGrassSpeed, m_WavingGrassSpeed);

	int GetPatchCount () const { return m_PatchCount; }
	
	const vector<DetailPrototype> &GetDetailPrototypes () const { return m_DetailPrototypes; }
	void SetDetailPrototypes (const vector<DetailPrototype> &treePrototypes );
	void RemoveDetailPrototype (int index);

	void SetDetailResolution (int resolution, int resolutionPerPatch);

	void ResetDirtyDetails ();
	void SetDetailPrototypesDirty ();
	void UpdateDetailPrototypesIfDirty ();
	void GenerateTextureAtlasThreaded ();

	void SetDirty ();

	int GetWidth () const  { return GetResolution (); }
	int GetHeight () const  { return GetResolution (); }
	int GetResolution () const { return m_PatchSamples * m_PatchCount; }
	int GetResolutionPerPatch () const { return m_PatchSamples; }

	int GetSupportedLayers (int xBase, int yBase, int totalWidth, int totalHeight, int *buffer) const;
	void GetLayer (int xBase, int yBase, int totalWidth, int totalHeight, int detailIndex, int *buffer) const;
	void SetLayer (int xBase, int yBase, int totalWidth, int totalHeight, int detailIndex, const int *buffer);
	
	int GetIndex (int x, int y, int l) const
	{
		int res = m_PatchSamples;
		int nbIndex = y * res + x + l * res * res;
		return nbIndex;
	}

	void RefreshPrototypes ();
	
	Rectf GetNormalizedArea (int x, int y) const
	{
		float fx = (float)x / m_PatchCount;
		float fy = (float)y / m_PatchCount;
		float size = 1.0F / m_PatchCount;
		return Rectf (fx, fy, size, size);
	}
	
//	void GenerateBounds (DetailPatch &patch, int patchX, int patchY);

	Mesh* BuildMesh (int patchX, int patchY, Vector3f size, int lightmapIndex, DetailRenderMode renderMode, float density);
	bool IsPatchEmpty (int x, int y) const;
	bool IsPatchDirty (int x, int y) const;


	
private:
	void GenerateMesh (Mesh& mesh, int patchX, int patchY, Vector3f size, int lightmapIndex, DetailRenderMode renderMode, float density, int totalVertexCount, int totalTriangleCount); 

	#if UNITY_EDITOR
	void SetupPreloadTextureAtlasData ();
	#endif
	void RefreshPrototypesStep1 (Texture2D** sourceTextures);

	void CleanupPrototype (DetailPrototype &proto, string const& error);
	void ComputeVertexAndTriangleCount(DetailPatch &patch, DetailRenderMode renderMode, float density, int* vertexCount, int* triangleCount);
	static int GetActualResolution (int resolution, int heightmapResolution);
	int AddLayerIndex (int detailIndex, DetailPatch &patch);
	void RemoveLocalLayerIndex (int detailIndex, DetailPatch& patch);

	const DetailPatch& GetPatch (int x, int y) const { return m_Patches[y * m_PatchCount + x]; }
	DetailPatch& GetPatch (int x, int y) { return m_Patches[y * m_PatchCount + x]; }

	bool m_IsPrototypesDirty;
	vector<DetailPatch>		m_Patches;
	vector<DetailPrototype>	m_DetailPrototypes;
	TerrainData*			m_TerrainData;
	TreeDatabase*			m_TreeDatabase;
	int					m_PatchCount;
	int					m_PatchSamples;
	vector<Vector3f>		m_RandomRotations;
	Texture2D*		m_AtlasTexture;

	ColorRGBAf			m_WavingGrassTint;
	float					m_WavingGrassStrength;
	float					m_WavingGrassAmount;
	float					m_WavingGrassSpeed;

	Rand				m_Random;
	
	vector<PPtr<Texture2D> > m_PreloadTextureAtlasData;
	vector<Rectf>            m_PreloadTextureAtlasUVLayout;
};


template<class TransferFunc>
void DetailDatabase::Transfer (TransferFunc& transfer)
{
	TRANSFER (m_Patches);
	TRANSFER (m_DetailPrototypes);
	TRANSFER (m_PatchCount);
	TRANSFER (m_PatchSamples);
	TRANSFER (m_RandomRotations);
	transfer.Transfer( m_WavingGrassTint, "WavingGrassTint" );
	TRANSFER (m_WavingGrassStrength);
	TRANSFER (m_WavingGrassAmount);
	TRANSFER (m_WavingGrassSpeed);
	transfer.Transfer (m_TreeDatabase->GetInstances(), "m_TreeInstances");
	transfer.Transfer (m_TreeDatabase->GetTreePrototypes(), "m_TreePrototypes");
	
	#if UNITY_EDITOR
	if (transfer.IsBuildingTargetPlatform(kBuildAnyPlayerData))
	{
		SetupPreloadTextureAtlasData();
		TRANSFER (m_PreloadTextureAtlasData);
		m_PreloadTextureAtlasData.clear();
	}
	else
	#endif
	{
		TRANSFER (m_PreloadTextureAtlasData);
	}
}


struct MonoDetailPrototype {
	ScriptingObjectPtr prototype;			
	ScriptingObjectPtr prototypeTexture;

	ColorRGBAf      healthyColor;
	ColorRGBAf      dryColor;

	float      minWidth, maxWidth;
	float      minHeight, maxHeight;
	float      noiseSpread;
	float      bendFactor;
	int renderMode;
	int usePrototypeMesh;
};


void DetailPrototypeToMono (const DetailPrototype &src, MonoDetailPrototype &dest);
void DetailPrototypeToCpp (MonoDetailPrototype &src, DetailPrototype &dest) ;


#endif // ENABLE_TERRAIN
