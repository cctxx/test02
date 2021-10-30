#include "UnityPrefix.h"
#include "TreeDatabase.h"

#if ENABLE_TERRAIN

#include "TerrainData.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Scripting/Scripting.h"

TreeDatabase::Prototype::Prototype()
:	treeHeight (1)
,	treeVisibleHeight (1)
,	treeWidth (1)
,	treeAspectRatio (1)
,	bounds (Vector3f::zero, Vector3f::zero)
,	bendFactor (0)
{
	prefab = NULL;
	mesh = NULL;
}

TreeDatabase::Prototype::~Prototype()
{
	for (std::vector<Material*>::iterator it = imposterMaterials.begin(), end = imposterMaterials.end(); it != end; ++it)
		DestroySingleObject(*it);

	for (std::vector<Material*>::iterator it = materials.begin(), end = materials.end(); it != end; ++it)
		DestroySingleObject(*it);
}

void TreeDatabase::Prototype::Set(const PPtr<Unity::GameObject>& source, float inBendFactor, const TerrainData& terrainData)
{
	prefab = source;
	if (!source)
	{
		ErrorStringObject(
			Format(
				"A tree couldn't be loaded because the prefab is missing.\nPlease select any instance of the %s asset in a scene and make it reference only tree prefabs that exist.",
				terrainData.GetName ()),
			&terrainData);
		return;
	}
	
	MeshFilter* filter = prefab->QueryComponent(MeshFilter);
	Renderer* renderer = prefab->QueryComponent(Renderer);
	
	if (!filter || !filter->GetSharedMesh() || !renderer)
	{
		WarningStringObject(std::string() + "The tree " + source->GetName() + " couldn't be instanced because the prefab contains no valid mesh renderer", source);
		return;
	}

	mesh = filter->GetSharedMesh();
	const Renderer::MaterialArray& originalMaterials = renderer->GetMaterialArray();
	for (int m = 0; m < originalMaterials.size(); ++m) {
		if (!originalMaterials[m])
		{
			WarningStringObject(std::string() + "The tree " + source->GetName() + " couldn't be instanced because one of the materials is missing", source);
			return;
		}
	}
	
	// * Create cloned imposter materials using special billboard shader
	// * Create cloned normal materials so we can modify the color in place
	// * Setup orginal colors array			
	//materials = originalMaterials;
	imposterMaterials.resize(originalMaterials.size());
	materials.resize(originalMaterials.size());
	originalMaterialColors.resize(originalMaterials.size());
	inverseAlphaCutoff.resize(originalMaterials.size());

	for (int m = 0; m < originalMaterials.size(); m++)
	{
		if (!SetMaterial (m, originalMaterials[m]))
		{
			WarningStringObject(std::string() + "The tree " + source->GetName() + " must use the Nature/Soft Occlusion shader. Otherwise billboarding/lighting will not work correctly.", source);
		}
	}
	
	
	bounds = mesh->GetBounds();
	MinMaxAABB minMaxBounds(bounds);
	treeVisibleHeight = minMaxBounds.GetMax().y;
	treeHeight  = bounds.GetExtent().y * 2;
	
	float x = std::max(std::abs(minMaxBounds.GetMin().x), std::abs(minMaxBounds.GetMax().x));
	float z = std::max(std::abs(minMaxBounds.GetMin().z), std::abs(minMaxBounds.GetMax().z));
	
	treeWidth = std::max(x, z) * 1.9F;

	treeAspectRatio = treeWidth / treeHeight;
	bendFactor = inBendFactor;
}


bool TreeDatabase::Prototype::SetMaterial (int index, Material* material)
{
	if (index < 0 || index >= materials.size())
		return true;

	const ShaderLab::FastPropertyName colorProperty = ShaderLab::Property("_Color");
	const ShaderLab::FastPropertyName cutoffProperty = ShaderLab::Property("_Cutoff");

	if (material->HasProperty(colorProperty))
		originalMaterialColors[index] = material->GetColor(colorProperty);
	else
		originalMaterialColors[index].Set(1, 1, 1, 1); 

	inverseAlphaCutoff[index] = 1.0F;
	if (material->HasProperty(cutoffProperty))
		inverseAlphaCutoff[index] = 0.5F / material->GetFloat(cutoffProperty);

	// Instantiate normal material.				
	if (materials[index])
		DestroySingleObject (materials[index]);
	materials[index] = Material::CreateMaterial (*material, Object::kHideAndDontSave);

	// Instantiate a special billboarding material.
	// Eg. leaves shader needs specialized premultiplied alpha rendering into render tex.
	if (imposterMaterials[index])
		DestroySingleObject (imposterMaterials[index]);
	imposterMaterials[index] = Material::CreateMaterial (*material, Object::kHideAndDontSave);
	Shader* imposterShader = imposterMaterials[index]->GetShader()->GetDependency ("BillboardShader");
	if (!imposterShader)
		return false;

	imposterMaterials[index]->SetShader(imposterShader);
	return true;
}


TreeDatabase::TreeDatabase (TerrainData& source)
:	m_SourceData(source)
{
}

void TreeDatabase::RefreshPrototypes ()
{
	m_Prototypes.clear();

	const vector<TreePrototype>& sourcePrototypes = GetTreePrototypes();
	m_Prototypes.resize(sourcePrototypes.size());
	for (int i = 0; i < m_Prototypes.size(); ++i)
		m_Prototypes[i].Set(sourcePrototypes[i].prefab, sourcePrototypes[i].bendFactor, m_SourceData);
	
	m_SourceData.UpdateUsers (TerrainData::kFlushEverythingImmediately);
}


// TODO: make sure all trees are within bounds.
void TreeDatabase::ValidateTrees ()
{
	int prototypeCount = m_TreePrototypes.size();
	for (vector<TreeInstance>::iterator i = m_Instances.begin(); i != m_Instances.end(); i++)
	{
		i->position.x = clamp01(i->position.x);
		i->position.y = clamp01(i->position.y);
		i->position.z = clamp01(i->position.z);
		i->index = clamp(i->index,0,prototypeCount - 1);
	}
}

void TreeDatabase::RecalculateTreePositions () 
{
	Heightmap *heightmap = &m_SourceData.GetHeightmap();
	Vector3f terrainSize = heightmap->GetSize();
	for (int i = 0; i < m_Instances.size(); i++)
	{
		Vector3f pos = m_Instances[i].position;
		pos.y = heightmap->GetInterpolatedHeight(pos.x, pos.z) / terrainSize.y;
		m_Instances[i].position = pos;
	}
	
	UpdateTreeInstances();
}

void TreeDatabase::AddTree (const TreeInstance& tree)
{
	m_Instances.push_back(tree);
	
	const Heightmap& heightmap = m_SourceData.GetHeightmap();

	float height = heightmap.GetInterpolatedHeight(tree.position.x, tree.position.z);
	height /= heightmap.GetSize().y;
	m_Instances.back().position.y = height;
		
	UpdateTreeInstances();
}

int TreeDatabase::RemoveTrees (const Vector2f& position, float radius, int prototypeIndex)
{
	float sqrRadius = radius * radius;
	std::vector<TreeInstance> instances;
	instances.reserve(m_Instances.size());
	for (int i=0; i<m_Instances.size(); ++i)
	{
		const TreeInstance& instance = m_Instances[i];
		Vector2f offset(instance.position.x - position.x, instance.position.z - position.y);
		bool shouldRemovePrototypeIndex = prototypeIndex == instance.index || prototypeIndex == -1;
		if (!shouldRemovePrototypeIndex || SqrMagnitude(offset) > sqrRadius)
			instances.push_back(instance);
	}
	
	int removedTrees = 0;
	if (m_Instances.size() != instances.size())
	{
		removedTrees = m_Instances.size() - instances.size();
		m_Instances = instances;

		UpdateTreeInstances();
	}
	return removedTrees;
}

void TreeDatabase::UpdateTreeInstances ()
{
	ValidateTrees ();
	m_SourceData.SetDirty ();
	m_SourceData.UpdateUsers (TerrainData::kTreeInstances);
}

void TreeDatabase::SetTreePrototypes (const vector<TreePrototype> &treePrototypes)
{
	m_TreePrototypes = treePrototypes;
	ValidateTrees ();

	RefreshPrototypes();

	m_SourceData.SetDirty ();
}

void TreeDatabase::RemoveTreePrototype (int index)
{
#if UNITY_EDITOR
	
	if( index < 0 || index >= m_TreePrototypes.size() )
	{
		ErrorString("invalid tree prototype index");
		return;
	}
	
	// erase tree prototype
	m_TreePrototypes.erase( m_TreePrototypes.begin() + index );
	
	// update tree instance indices
	for( vector<TreeInstance>::iterator it = m_Instances.begin(); it != m_Instances.end(); /**/ )
	{
		if( it->index == index )
		{
			it = m_Instances.erase( it );
		}
		else
		{
			if( it->index > index )
				it->index--;
			++it;
		}
	}
	
	m_SourceData.SetDirty();
	
	RefreshPrototypes();
	
#else
	ErrorString("only implemented in editor");
#endif
}


void TreePrototypeToMono (const TreePrototype &src, MonoTreePrototype &dest) {
	dest.prefab = Scripting::ScriptingWrapperFor (src.prefab);
	dest.bendFactor = src.bendFactor;
}
void TreePrototypeToCpp (MonoTreePrototype &src, TreePrototype &dest) {
	dest.prefab = ScriptingObjectToObject<GameObject> (src.prefab);
	dest.bendFactor = src.bendFactor;
}


#endif // ENABLE_TERRAIN
