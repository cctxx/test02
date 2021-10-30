#include "UnityPrefix.h"
#include "SplatMaterials.h"

#if ENABLE_TERRAIN

#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Utilities/ArrayUtility.h"

namespace SplatMaterials_Static
{
static SHADERPROP(MainTex);
static SHADERPROP(Control);
} // namespace SplatMaterials_Static

static const char* kSplatNameStrings[4] = {"_Splat0","_Splat1","_Splat2","_Splat3"};
static const char* kSplatNormalNameStrings[4] = {"_Normal0","_Normal1","_Normal2","_Normal3"};


SplatMaterials::SplatMaterials (PPtr<TerrainData> terrain)
:	m_CurrentTemplateMaterial(NULL)
{
	m_TerrainData = terrain;
	for(int i=0;i<kTerrainShaderCount;i++)
		m_Shaders[i] = NULL;
	m_BaseMapMaterial = NULL;
	for(int i = 0; i < 32; i++)
		m_AllocatedMaterials[i] = NULL;
}

SplatMaterials::~SplatMaterials () 
{
	Cleanup ();
}

void SplatMaterials::Cleanup ()
{
	for (int s = 0; s < ARRAY_SIZE(m_Shaders); ++s)
	{
		m_Shaders[s] = NULL;
	}
	for (int x = 0; x < ARRAY_SIZE(m_AllocatedMaterials); ++x)
	{
		DestroySingleObject (m_AllocatedMaterials[x]);
		m_AllocatedMaterials[x] = NULL;
	}
	DestroySingleObject (m_BaseMapMaterial);
	m_BaseMapMaterial = NULL;
	m_CurrentTemplateMaterial = NULL;
}



void SplatMaterials::LoadSplatShaders (Material* templateMat)
{
	// If material is already created, and our template material is still the same:
	// nothing to do
	if ((m_Shaders[0] != NULL) && (templateMat == m_CurrentTemplateMaterial))
		return;
	
	// Material template has changed; recreate internal materials
	if (templateMat != m_CurrentTemplateMaterial)
	{
		Cleanup();
		m_CurrentTemplateMaterial = templateMat;
	}
	
	Shader* templateShader = NULL;
	if (templateMat)
		templateShader = templateMat->GetShader();
	if (templateShader)
	{
		// Shader in the template is for the first pass;
		// additive pass and basemap are queried from dependencies
		m_Shaders[kTerrainShaderFirst] = templateShader;
		m_Shaders[kTerrainShaderAdd] = templateShader->GetDependency("AddPassShader");
		m_Shaders[kTerrainShaderBaseMap] = templateShader->GetDependency("BaseMapShader");
	}
	
	ScriptMapper& sm = GetScriptMapper ();
	
	// Note: No good reason to keep "Lightmap-" in the built-in terrain shader names, except
	// to be able to run Unity 2.x content on regression rig with 3.0 player. The shader
	// names just have to match _some_ existing built-in shader in Unity 2.x.
	
	if (m_Shaders[kTerrainShaderBaseMap] == NULL)
		m_Shaders[kTerrainShaderBaseMap] = sm.FindShader("Diffuse");
	if (m_Shaders[kTerrainShaderFirst] == NULL)
		m_Shaders[kTerrainShaderFirst]   = sm.FindShader("Hidden/TerrainEngine/Splatmap/Lightmap-FirstPass");
	if (m_Shaders[kTerrainShaderAdd] == NULL)
		m_Shaders[kTerrainShaderAdd]     = sm.FindShader("Hidden/TerrainEngine/Splatmap/Lightmap-AddPass");

	bool shaderNotFound = false;
	for (int i = kTerrainShaderFirst; i <= kTerrainShaderAdd; ++i)
	{
		if (m_Shaders[i] == NULL)
		{
			shaderNotFound = true;
			m_Shaders[i] = sm.FindShader("Diffuse");
		}
	}

	if (shaderNotFound)
	{
		ErrorString("Unable to find shaders used for the terrain engine. Please include Nature/Terrain/Diffuse shader in Graphics settings.");
	}
}




Material *SplatMaterials::GetSplatBaseMaterial (Material* templateMat)
{
	using namespace SplatMaterials_Static;

	LoadSplatShaders (templateMat);

	Material *mat = m_BaseMapMaterial;
	if (mat && mat->GetTexture(kSLPropMainTex) == NULL)
	{
		DestroySingleObject( mat );
		mat = NULL;
	}
	if (!mat) {
		mat = Material::CreateMaterial (*m_Shaders[kTerrainShaderBaseMap], Object::kHideAndDontSave);
		mat->SetTexture( kSLPropMainTex, m_TerrainData->GetSplatDatabase().GetBasemap() );
		m_BaseMapMaterial = mat;
	}
	if (templateMat)
	{
		mat->CopyPropertiesFromMaterial (*templateMat);	
		mat->SetTexture (kSLPropMainTex, m_TerrainData->GetSplatDatabase().GetBasemap());
	}
	return mat;		
}

Material **SplatMaterials::GetMaterials (Material* templateMat, int &materialCount)
{
	using namespace SplatMaterials_Static;

	TerrainData *terrainData = m_TerrainData;
	LoadSplatShaders (templateMat);
	
	const bool setNormalMaps = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1);
		
	int materialIndex = -1;
	Material *splatMaterial = NULL;		
	int actualNbSplats = terrainData->GetSplatDatabase().GetDepth();
	int nbSplats = actualNbSplats>1?actualNbSplats: 1;
	for( int i = 0; i < nbSplats; ++i )
	{
		if (i / 4 != materialIndex)
		{
			materialIndex = i / 4;
			
			splatMaterial = m_AllocatedMaterials[materialIndex];
			if (splatMaterial == NULL)
			{
				const int shaderIndex = (materialIndex != 0) ? kTerrainShaderAdd : kTerrainShaderFirst;
				Shader* shader = m_Shaders[shaderIndex];
				splatMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
				splatMaterial->SetCustomRenderQueue(splatMaterial->GetActualRenderQueue() + materialIndex);
				m_AllocatedMaterials[materialIndex] = splatMaterial;
			}

			if (templateMat)
				splatMaterial->CopyPropertiesFromMaterial (*templateMat);
			
			if( splatMaterial->HasProperty(kSLPropMainTex) )
				splatMaterial->SetTexture(kSLPropMainTex, terrainData->GetSplatDatabase().GetBasemap() );
			
			// As soon as our shader does not support 4 splats per pass,
			// that means (at least currently) it's a single pass base map.
			// So stop adding materials.
			if( splatMaterial->GetTag("SplatCount",false,"") != "4" )
				break;
		}
		
		int localSplatIndex = i - (materialIndex * 4);
		if (materialIndex < actualNbSplats)
		{
			splatMaterial->SetTexture( kSLPropControl,  terrainData->GetSplatDatabase().GetAlphaTexture(materialIndex));
		}
		else
			splatMaterial->SetTexture( kSLPropControl, NULL);
		SetupSplat(*splatMaterial, localSplatIndex, i, setNormalMaps);
	}
	
	materialCount = materialIndex + 1;
	
	return m_AllocatedMaterials;
}


void SplatMaterials::SetupSplat (Material &m, int splatIndex, int index, bool setNormalMap)
{
	ShaderLab::FastPropertyName slName = ShaderLab::Property(kSplatNameStrings[splatIndex]);
	ShaderLab::FastPropertyName slNormalName = ShaderLab::Property(kSplatNormalNameStrings[splatIndex]);
	const bool hasTex = m.HasProperty(slName);
	const bool hasNormalMap = setNormalMap && m.HasProperty(slNormalName);
	
	if (index < m_TerrainData->GetSplatDatabase().GetDepth())	
	{
		const SplatPrototype& splat = m_TerrainData->GetSplatDatabase().GetSplatPrototypes()[index];
				
		Vector3f heightmapSize = m_TerrainData->GetHeightmap().GetSize();
		Vector2f splatScale(heightmapSize.x / splat.tileSize.x, heightmapSize.z / splat.tileSize.y);
		Vector2f splatOffset(splat.tileOffset.x / heightmapSize.x * splatScale.x, splat.tileOffset.y / heightmapSize.z * splatScale.y);
		
		if (hasTex)
		{
			m.SetTexture (slName, splat.texture);
			m.SetTextureScale (slName, splatScale);
			m.SetTextureOffset (slName, splatOffset);
		}
		if (hasNormalMap)
			m.SetTexture (slNormalName, splat.normalMap);
	}
	else
	{
		if (hasTex)
			m.SetTexture (slName, NULL);
		if (hasNormalMap)
			m.SetTexture (slNormalName, NULL);
	}
}

#endif // ENABLE_TERRAIN
