#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/Math/Vector2.h"
#include "Heightmap.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/BaseClasses/BaseObject.h"

using std::vector;
class Texture2D;
class ColorRGBAf;
class TerrainData;

struct SplatPrototype
{
	DECLARE_SERIALIZE (SplatPrototype)
	PPtr<Texture2D> texture;
	PPtr<Texture2D> normalMap;
	Vector2f   tileSize;
	Vector2f   tileOffset;
	
	SplatPrototype ();
};

template<class TransferFunc>
void SplatPrototype::Transfer (TransferFunc& transfer)
{
	TRANSFER (texture);
	TRANSFER (normalMap);
	TRANSFER (tileSize);
	TRANSFER (tileOffset);
}

class SplatDatabase {
public:
	DECLARE_SERIALIZE (SplatDatabase)

	SplatDatabase (TerrainData *owner);
	~SplatDatabase();
	
	void AwakeFromLoad (AwakeFromLoadMode mode);

	void Init (int splatResolution, int basemapResolution);
	void GetSplatTextures (vector<Texture2D*> *dest);
	
	int GetAlphamapResolution () { return m_AlphamapResolution; }
	int GetBaseMapResolution () { return m_BaseMapResolution; }
	int GetDepth () const {return m_Splats.size (); }
	void SetAlphamapResolution (int res);
	void SetBaseMapResolution (int res);

	// Extract a copy of the alpha map in the given area
	void GetAlphamaps (int xBase, int yBase, int width, int height, float* buffer);
	// Set alpha map in the given area
	void SetAlphamaps (int xBase, int yBase, int width, int height, float* buffer);
	// Extract whole alpha map, as byte weights
	void GetAlphamaps (dynamic_array<UInt8>& buffer);

	// NOT IMPLEMENTED void SetResolution (int width, int height);
	

	Texture2D *GetAlphaTexture (int index);
	int GetAlphaTextureCount () { return m_AlphaTextures.size(); }
		
	const vector<SplatPrototype> &GetSplatPrototypes () const { return m_Splats; }
	void SetSplatPrototypes (const vector<SplatPrototype> &splats );
	
	Texture2D* GetBasemap();
	void UploadBasemap();
	bool RecalculateBasemapIfDirty();
	void RecalculateBasemap(bool allowUpload);
	void SetBasemapDirty(bool dirty) { m_BaseMapDirty = dirty; }
	
private:
	
	Texture2D *AllocateAlphamap (const ColorRGBAf &color);

	vector<SplatPrototype>		m_Splats;
	vector<PPtr<Texture2D> >	m_AlphaTextures;
	Texture2D*					m_BaseMap;
	TerrainData*				m_TerrainData;
	int							m_AlphamapResolution;
	int							m_BaseMapResolution;	
	bool                        m_BaseMapDirty;
};

template<class TransferFunc>
void SplatDatabase::Transfer (TransferFunc& transfer)
{
	TRANSFER (m_Splats);
	TRANSFER (m_AlphaTextures);
	TRANSFER (m_AlphamapResolution);
	TRANSFER (m_BaseMapResolution);
}


struct MonoSplatPrototype
{
	ScriptingObjectPtr texture;
	ScriptingObjectPtr normalMap;
	Vector2f tileSize;
	Vector2f tileOffset;
};


void SplatPrototypeToMono (const SplatPrototype &src, MonoSplatPrototype &dest);
void SplatPrototypeToCpp (MonoSplatPrototype &src, SplatPrototype &dest);

#endif // ENABLE_TERRAIN
