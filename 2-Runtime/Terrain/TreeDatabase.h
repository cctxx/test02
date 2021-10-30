#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Filters/Mesh/LodMesh.h"

#include <vector>

class Vector2f;
class ColorRGBAf;
class TerrainData;
struct TreeInstance;

namespace Unity
{
	class GameObject;
	class Material;
}

struct TreeInstance
{
	DECLARE_SERIALIZE (TreeInstance)
	
	Vector3f		position;
	float			widthScale;
	float			heightScale;
	ColorRGBA32		color;
	ColorRGBA32		lightmapColor;
	int				index;
	float			temporaryDistance;
};

template<class TransferFunc>
void TreeInstance::Transfer (TransferFunc& transfer)
{
	TRANSFER (position);
	TRANSFER (widthScale);
	TRANSFER (heightScale);
	TRANSFER (color);
	TRANSFER (lightmapColor);
	TRANSFER (index);
}

struct TreePrototype
{
	DECLARE_SERIALIZE (TreePrototype)
	
	PPtr<GameObject> prefab;
	float      bendFactor;
	
	TreePrototype ()
	{
		bendFactor = 1.0F;
	}
};


struct MonoTreePrototype {
	ScriptingObjectPtr prefab;
	float bendFactor;
};
void TreePrototypeToMono (const TreePrototype &src, MonoTreePrototype &dest) ;
void TreePrototypeToCpp (MonoTreePrototype &src, TreePrototype &dest);

template<class TransferFunc>
void TreePrototype::Transfer (TransferFunc& transfer)
{
	TRANSFER (prefab);
	TRANSFER (bendFactor);
}

class TreeDatabase
{
public:
	class Prototype 
	{
	public:
		Prototype();
		~Prototype();

		void Set (const PPtr<Unity::GameObject>& source, float inBendFactor, const TerrainData& terrainData);
		bool SetMaterial (int index, Material* material);

		// if a tree is more tall than wide then we want to use square billboards, 
		// because tree has to fit into billboard when looking from above (or bellow)
		float getBillboardAspect() const { return std::min<float>(1.f, treeAspectRatio); }
		float getBillboardHeight() const { return std::max<float>(treeHeight, treeWidth); }

		// offset of center point of the tree from the ground (i.e. from pivot point/root of the tree)
		float getCenterOffset() const { return treeVisibleHeight - treeHeight / 2; }

	public:
		PPtr<Unity::GameObject> prefab;
		PPtr<Mesh> mesh;

		std::vector<float> inverseAlphaCutoff;
		std::vector<Material*> materials;
		std::vector<ColorRGBAf> originalMaterialColors;
		std::vector<Material*> imposterMaterials;

		// actual tree height from the bottom to the top
		float treeHeight;
		// visible tree height, i.e. the part which is above terrain (i.e. from pivot point/root of the tree to the top)
		float treeVisibleHeight;
		// The width of the tree
		float treeWidth;
		// How wide is the tree relative to the height. (Usually less than 1)
		float treeAspectRatio;
		AABB bounds;
		float bendFactor;
	};

	typedef std::vector<Prototype> PrototypeVector;

public:
	TreeDatabase (TerrainData& source);
		
	void AddTree (const TreeInstance& tree);
	int RemoveTrees (const Vector2f& position, float radius, int prototypeIndex);
	
	TerrainData& GetTerrainData() const { return m_SourceData; }

	PrototypeVector& GetPrototypes() { return m_Prototypes; }
	const std::vector<Prototype>& GetPrototypes() const { return m_Prototypes; }	
	
	
	std::vector<TreeInstance> &GetInstances () { return m_Instances; }
	std::vector<TreePrototype> &GetTreePrototypes () { return m_TreePrototypes; }
	
	void SetTreePrototypes (const std::vector<TreePrototype> &treePrototypes );
	void RefreshPrototypes ();
	void RemoveTreePrototype (int index);
	
	void UpdateTreeInstances ();
	void RecalculateTreePositions ();
	void ValidateTrees ();

private:
	TerrainData& m_SourceData;
	std::vector<TreePrototype>	m_TreePrototypes;
	std::vector<TreeInstance> m_Instances;

	PrototypeVector m_Prototypes;
};

#endif // ENABLE_TERRAIN


