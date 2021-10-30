#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "TerrainData.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"

enum{
	kTerrainShaderBaseMap,
	kTerrainShaderFirst,
	kTerrainShaderAdd,
	kTerrainShaderCount
};


class SplatMaterials 
{
public:
	SplatMaterials (PPtr<TerrainData> terrain);
	~SplatMaterials ();
	
	Material** GetMaterials (Material* templateMat, int &materialCount);
	Material* GetSplatBaseMaterial (Material* templateMat);
	void Cleanup ();	

private:
	void LoadSplatShaders (Material* templateMat);
	void SetupSplat (Material &m, int splatIndex, int index, bool setNormalMap);
	
private:
	PPtr<TerrainData> m_TerrainData;
	Shader *m_Shaders[kTerrainShaderCount];
	
	Material* m_AllocatedMaterials[32];	
	Material* m_BaseMapMaterial;	
	Material* m_CurrentTemplateMaterial;
};

#endif // ENABLE_TERRAIN
