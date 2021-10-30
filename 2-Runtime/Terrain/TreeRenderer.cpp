#include "UnityPrefix.h"
#include "TreeRenderer.h"

#if ENABLE_TERRAIN

#include "TerrainData.h"
#include "PerlinNoise.h"
#include "Wind.h"

#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/IntermediateRenderer.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Camera/Shadows.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Allocator/MemoryMacros.h"

static ShaderLab::FastPropertyName kTerrainTreeLightDirections[4] = {
	ShaderLab::Property("_TerrainTreeLightDirections0"),
	ShaderLab::Property("_TerrainTreeLightDirections1"),
	ShaderLab::Property("_TerrainTreeLightDirections2"),
	ShaderLab::Property("_TerrainTreeLightDirections3"),
};
static ShaderLab::FastPropertyName kTerrainTreeLightColors[4] = {
	ShaderLab::Property("_TerrainTreeLightColors0"),
	ShaderLab::Property("_TerrainTreeLightColors1"),
	ShaderLab::Property("_TerrainTreeLightColors2"),
	ShaderLab::Property("_TerrainTreeLightColors3"),
};


// when we have shadow we need to emit all renderers to let them casting shadows
#define EMIT_ALL_TREE_RENDERERS		ENABLE_SHADOWS


// --------------------------------------------------------------------------


struct TreeBinaryTree : public NonCopyable
{
	TreeBinaryTree(TreeDatabase* database) 
	:	database(database), mesh(0), sortIndex(0), targetSortIndex(0), visible(0) 
	{
		Assert(database != NULL);
	}

	~TreeBinaryTree()
	{
		DestroySingleObject(mesh);
	}

	TreeDatabase* database;
	std::vector<int> instances;
	AABB bounds;
	Mesh* mesh;

	int sortIndex;
	int targetSortIndex;
	int visible;

	Plane splittingPlane;
	std::vector<int> usedPrototypes;
	
	std::auto_ptr<TreeBinaryTree> left;
	std::auto_ptr<TreeBinaryTree> right;
};

class TreeBinaryTreeBuilder
{
public:
	static bool AddLastTree (TreeBinaryTree& binTree, const Vector3f& position, const Vector3f& scale, int minimumInstances)
	{
		const std::vector<TreeInstance>& instances = binTree.database->GetInstances();
		Assert(!instances.empty());
		const TreeInstance& instance = instances.back();

		const std::vector<TreeDatabase::Prototype>& prototypes = binTree.database->GetPrototypes();

		EncapsulateBounds(binTree.bounds, instance, prototypes, position, scale);

		// Leaf node
		if (!binTree.left.get())
		{
			binTree.sortIndex = -1;
			if (binTree.visible != 0)
			{
				binTree.visible = 0;
				DestroySingleObject(binTree.mesh);
				binTree.mesh = 0;
			}

			if (binTree.instances.empty())
			{
				binTree.instances.resize(1);
				binTree.instances[0] = instances.size() - 1;
				binTree.bounds = CalculateBounds(instances, binTree.instances, prototypes, position, scale);
				binTree.usedPrototypes = CalculateSupportedInstances(instances, binTree.instances, prototypes);

				return true;
			}
			else if (binTree.instances.size() < minimumInstances * 2)
			{
				binTree.instances.push_back(instances.size() - 1);
				EncapsulateBounds(binTree.bounds, instance, prototypes, position, scale);
				binTree.usedPrototypes = CalculateSupportedInstances(instances, binTree.instances, prototypes);
				return true;
			}
			else
				// Adding will actually fail once all leaves are filled up to minimumInstances * 2.
				// Is that what we want?
				return false;
		}
		else
		{
			const Vector3f pos = Scale(instance.position, scale);
			if (binTree.splittingPlane.GetSide (pos))
				return AddLastTree(*binTree.left, position, scale, minimumInstances);
			else
				return AddLastTree(*binTree.right, position, scale, minimumInstances);
		}
	}

	static std::auto_ptr<TreeBinaryTree> Build(TreeDatabase& database, const Vector3f& position, const Vector3f& scale, int minimumInstances)
	{
		std::auto_ptr<TreeBinaryTree> tree(new TreeBinaryTree(&database));
		const std::vector<TreeInstance>& instances = database.GetInstances();
		if (instances.empty())
			return tree;

		tree->instances.resize(instances.size());
		for (int i = 0; i < instances.size(); ++i)
		{
			tree->instances[i] = i;
		}
		
		Assert(Magnitude(scale) > 0);
		Split(*tree, position, scale, minimumInstances);
		return tree;
	}

private:
	static void Split(TreeBinaryTree& bintree, const Vector3f& position, const Vector3f& scale, int minimumInstances)
	{
		const std::vector<TreeInstance>& allInstances = bintree.database->GetInstances();
		const std::vector<TreeDatabase::Prototype>& prototypes = bintree.database->GetPrototypes();
		bintree.bounds = CalculateBounds(allInstances, bintree.instances, prototypes, position, scale);
		bintree.usedPrototypes = CalculateSupportedInstances(allInstances, bintree.instances, prototypes);

		// Stop splitting when we reach a certain amount of trees
		if (bintree.instances.size() <= minimumInstances)
			return;
		
		{
			// We need to calculate bounds of position of all instances and then use median position (i.e. center) 
			// as a splitting point. We can't just use center of bintree.bounds, because it might be shifted sideways
			// if bounds of each prototype is shifted and that would result in left or right bintree being empty 
			// (see case 345094)
			AABB posBounds = CalculatePosBounds (allInstances, bintree.instances, position, scale);
			
			// The extent might be 0,0,0 if all instances are at the same position. In that case just stop subdiving this subtree.
			// That will cause this subtree to have more instances than minimumInstances. If that's not we want, then we should
			// just split the instances in half by the index.
			if (CompareApproximately (posBounds.GetExtent().x, 0.0f) && CompareApproximately (posBounds.GetExtent().z, 0.0f))
				return;
			
			// Splitting plane is the largest axis
			bintree.splittingPlane.SetNormalAndPosition(
				posBounds.GetExtent().x > posBounds.GetExtent().z ? Vector3f::xAxis : Vector3f::zAxis, 
				posBounds.GetCenter()
			);
		}

		// Now just queue up the binary tree
		std::vector<int> left, right;		
		for (std::vector<int>::const_iterator it = bintree.instances.begin(), end = bintree.instances.end(); it != end; ++it)
		{
			if (bintree.splittingPlane.GetSide (Scale(allInstances[*it].position, scale) + position))
				left.push_back(*it);
			else
				right.push_back(*it);
		}
		
		// Due to numerical errors we might still end up with one of the lists empty (not if the the epsilon for CompareApprox was big enough,
		// but that's another story). In that case just don't split this subtree.
		if (left.empty() || right.empty())
			return;

		bintree.instances.clear();
		bintree.usedPrototypes.clear();
		bintree.left.reset(new TreeBinaryTree(bintree.database));
		bintree.right.reset(new TreeBinaryTree(bintree.database));
		bintree.left->instances.swap(left);
		bintree.right->instances.swap(right);

		Split (*bintree.left, position, scale, minimumInstances);
		Split (*bintree.right, position, scale, minimumInstances);
	}
	
	// Build bounds of tree positions for a bunch of trees
	static AABB CalculatePosBounds (const std::vector<TreeInstance>& allInstances, const std::vector<int>& instances, const Vector3f& position, const Vector3f& scale)
	{
		AssertIf(instances.empty());
		AABB bounds(Scale(allInstances[instances[0]].position, scale) + position, Vector3f::zero);
		for (std::vector<int>::const_iterator it = instances.begin(), end = instances.end(); it != end; ++it)
			bounds.Encapsulate (Scale(allInstances[*it].position, scale) + position);	
		
		return bounds;
	}
	
	// Build bounds for a bunch of trees
	static AABB CalculateBounds (const std::vector<TreeInstance>& allInstances, const std::vector<int>& instances, const TreeDatabase::PrototypeVector& prototypes, const Vector3f& position, const Vector3f& scale)
	{
		AssertIf(instances.empty());
		AABB bounds(Scale(allInstances[instances[0]].position, scale) + position, Vector3f::zero);
		for (std::vector<int>::const_iterator it = instances.begin(), end = instances.end(); it != end; ++it)
			EncapsulateBounds(bounds, allInstances[*it], prototypes, position, scale);
		
		return bounds;
	}

	// Encapsulate a single tree in the bounds
	static void EncapsulateBounds (AABB& bounds, const TreeInstance& instance, const TreeDatabase::PrototypeVector& prototypes, const Vector3f& position, const Vector3f& scale)
	{
		Vector3f pos = Scale(instance.position, scale) + position;
		Vector3f treeScale(instance.widthScale, instance.heightScale, instance.widthScale);
		const AABB& pb = prototypes[instance.index].bounds;
		bounds.Encapsulate (pos + Scale(treeScale, pb.GetMin()));		
		bounds.Encapsulate (pos + Scale(treeScale, pb.GetMax()));
	}


	// Calculates an array of indices, for which prototypes are being used in the batch	
	static std::vector<int> CalculateSupportedInstances (const std::vector<TreeInstance>& allInstances, const std::vector<int>& instances, const TreeDatabase::PrototypeVector& prototypes)
	{
		std::vector<char> supported(prototypes.size(), 0);
		for (std::vector<int>::const_iterator it = instances.begin(), end = instances.end(); it != end; ++it)
			supported[allInstances[*it].index] = 1;

		std::vector<int> supportedIndices;
		supportedIndices.reserve(prototypes.size());
		for (int i = 0; i < supported.size(); ++i)
		{
			if (supported[i])
				supportedIndices.push_back(i);
		}

		// we do explicit copy because we want to reduce reserved size
		return std::vector<int>(supportedIndices.begin(), supportedIndices.end());
	}
};

struct TreeInstanceIndexSorter : public std::binary_function<int, int, bool>
{
	const std::vector<TreeInstance>* m_AllInstances;

	TreeInstanceIndexSorter(const std::vector<TreeInstance>& allInstances)
		: m_AllInstances(&allInstances)
	{ }

	bool operator ()(int lhs, int rhs) const
	{
		return (*m_AllInstances)[lhs].temporaryDistance > (*m_AllInstances)[rhs].temporaryDistance;
	}
};

class TreeMeshIntermediateRenderer : public MeshIntermediateRenderer
{
public:
	static ForwardLinearAllocator* s_Allocator;

	inline void* operator new( size_t size)
	{
		Assert(s_Allocator != NULL);
		return s_Allocator->allocate(size);
	}

	inline void	operator delete( void* p)
	{
		Assert(s_Allocator != NULL);
		s_Allocator->deallocate(p);
	}

	void Update(int layer, bool shadows, int lightmapIndex, const MaterialPropertyBlock& properties)
	{
		m_Layer = layer;
		m_CastShadows = m_ReceiveShadows = shadows;
		SetLightmapIndexIntNoDirty(lightmapIndex);
		SetPropertyBlock(properties);
	}
};

ForwardLinearAllocator* TreeMeshIntermediateRenderer::s_Allocator = NULL;


// --------------------------------------------------------------------------



const int kTreesPerBatch = 500;

TreeRenderer::TreeRenderer(TreeDatabase& database, const Vector3f& position, int lightmapIndex)
:	m_Database(0), m_BillboardMaterial(0), m_TreeBinaryTree(0), m_CloseBillboardMesh(0), m_LightmapIndex(lightmapIndex)
,	m_RendererAllocator(8 * 1024, kMemTerrain)
,	m_LegacyTreeSceneNodes(kMemTerrain), m_LegacyTreeBoundingBoxes(kMemTerrain), m_TreeIndexToSceneNode(kMemTerrain)
{
	const TreeDatabase::PrototypeVector& prototypes = database.GetPrototypes();
	bool anyValidTrees = false;
	for (TreeDatabase::PrototypeVector::const_iterator it = prototypes.begin(), end = prototypes.end(); it != end; ++it)
	{
		if (!it->materials.empty())
			anyValidTrees = true;
	}

	if (!anyValidTrees)
		return;

	m_Database = &database;

	m_TerrainSize = database.GetTerrainData().GetHeightmap().GetSize();
	m_Position = position;
	m_ImposterRenderTexture.reset(new ImposterRenderTexture(database));
	
	// SetupMaterials
	Shader* s = GetScriptMapper().FindShader("Hidden/TerrainEngine/BillboardTree");	
	if (s == NULL)
	{
		ErrorString("Unable to find shaders used for the terrain engine. Please include Nature/Terrain/Diffuse shader in Graphics settings.");
		s = GetScriptMapper().FindShader("Diffuse");	
	}
	m_BillboardMaterial = Material::CreateMaterial(*s, Object::kHideAndDontSave);
	if (m_BillboardMaterial->HasProperty(ShaderLab::Property("_MainTex")))
		m_BillboardMaterial->SetTexture(ShaderLab::Property("_MainTex"), m_ImposterRenderTexture->GetTexture());

	if (database.GetInstances().empty() || database.GetPrototypes().empty())
		return;

	
	// mircea@ for some reason it doesn't compile on the PS3 if assigned directly...
	std::auto_ptr<TreeBinaryTree> TreeBinaryTree = TreeBinaryTreeBuilder::Build(database, m_Position, m_TerrainSize, kTreesPerBatch);
	m_TreeBinaryTree = TreeBinaryTree;

#if ENABLE_THREAD_CHECK_IN_ALLOCS
	m_RendererAllocator.SetThreadIDs(Thread::mainThreadId, Thread::mainThreadId);
#endif

	for (int i = 0; i < database.GetInstances().size(); ++i)
	{
		CreateTreeRenderer(i);
	}
}

TreeRenderer::~TreeRenderer()
{
	CleanupBillboardMeshes();

	DestroySingleObject(m_BillboardMaterial);
	m_BillboardMaterial = 0;

	DeleteTreeRenderers();
}


/// Injects a single tree into the renderer, without requiring a full rebuild of the binary tree.
/// Requires that there is at least one tree in the renderer
void TreeRenderer::InjectTree(const TreeInstance& newTree)
{
	Assert(&newTree == &m_Database->GetInstances().back());
	if (!m_TreeBinaryTree.get() || !TreeBinaryTreeBuilder::AddLastTree(*m_TreeBinaryTree, m_Position, m_TerrainSize, kTreesPerBatch))
	{
		// Failed adding the tree (Too many trees in one batch)
		
		// mircea@ for some reason it doesn't compile on the PS3 if assigned directly...
		std::auto_ptr<TreeBinaryTree> TreeBinaryTree = TreeBinaryTreeBuilder::Build(*m_Database, m_Position, m_TerrainSize, kTreesPerBatch);
		m_TreeBinaryTree = TreeBinaryTree;
	}
	CreateTreeRenderer(m_Database->GetInstances().size() - 1);
	//@TODO: Cleanup meshes
}

void TreeRenderer::RemoveTrees(const Vector3f& pos, float radius, int prototypeIndex)
{
	// hack
	ReloadTrees();
}

void TreeRenderer::ReloadTrees() 
{
	ReloadTrees(kTreesPerBatch);		
}

void TreeRenderer::ReloadTrees(int treesPerBatch)
{
	if (m_Database)
	{
		if (m_Database->GetInstances().empty())
			m_TreeBinaryTree.reset();
		else
		{
			// mircea@ for some reason it doesn't compile on the PS3 if assigned directly...
			std::auto_ptr<TreeBinaryTree> TreeBinaryTree = TreeBinaryTreeBuilder::Build(*m_Database, m_Position, m_TerrainSize, treesPerBatch);
			m_TreeBinaryTree = TreeBinaryTree;
		}
		m_RenderedBatches.clear();

		DeleteTreeRenderers();
		m_LegacyTreeSceneNodes.resize_uninitialized(0);
		m_LegacyTreeBoundingBoxes.resize_uninitialized(0);
		m_TreeIndexToSceneNode.resize_uninitialized(0);
		for (int i = 0; i < m_Database->GetInstances().size(); ++i)
		{
			CreateTreeRenderer(i);
		}
	}
}

namespace {
	// Encapsulate a single tree in the bounds
	static void GetBounds (AABB& bounds, const TreeInstance& instance, const TreeDatabase::PrototypeVector& prototypes, const Vector3f& position, const Vector3f& scale)
	{
		Vector3f pos = Scale(instance.position, scale) + position;
		Vector3f treeScale(instance.widthScale, instance.heightScale, instance.widthScale);
		const AABB& pb = prototypes[instance.index].bounds;
		bounds.SetCenterAndExtent(pos, Scale(treeScale, pb.GetExtent()));
	}
}

static void CalculateTreeBend (const AABB& bounds, float bendFactor, float time, Matrix4x4f& matrix, Vector4f& outWind)
{
	Vector4f force = WindManager::GetInstance().ComputeWindForce(bounds);

	const Vector3f& pos = bounds.GetCenter();

	Vector3f quaternionAxis(force.z, 0, -force.x);
	float magnitude = Magnitude(quaternionAxis);
	if (magnitude > 0.00001f)
		quaternionAxis = Normalize(quaternionAxis);
	else
	{
		magnitude = 0;
		quaternionAxis.z = 1;
	}

	Vector2f additionBend(
		PerlinNoise::NoiseNormalized(pos.x * 0.22F - time * 4.7F, pos.x * 9.005F) * 0.7f,
		PerlinNoise::NoiseNormalized(pos.z * 0.22F - time * 4.3F, pos.z * 9.005F) * 0.5f
	);

	Quaternionf q = EulerToQuaternion(Vector3f(additionBend.x, 0, additionBend.y) * (bendFactor * kDeg2Rad * force.w));;

	float bend = 7 * bendFactor * kDeg2Rad * magnitude;
	matrix.SetTRS(Vector3f::zero, AxisAngleToQuaternion(quaternionAxis, bend) * q, Vector3f::one);
	outWind = force;
}

static void RenderMeshIdentityMatrix (Mesh& mesh, Material& material, int layer, Camera& camera, const MaterialPropertyBlock& properties) 
{
	Matrix4x4f matrix;
	matrix.SetIdentity();
	
	IntermediateRenderer* r = AddMeshIntermediateRenderer (matrix, &mesh, &material, layer, true, true, 0, &camera);
	r->SetPropertyBlock (properties);
}

const int kFrustumPlaneCount = 6;

void TreeRenderer::Render(Camera& camera, const UNITY_VECTOR(kMemRenderer, Light*)& lights, float meshTreeDistance, float billboardTreeDistance, float crossFadeLength, int maximumMeshTrees, int layer)
{
	if (!m_TreeBinaryTree.get() || !m_Database)
		return;

	RenderTexture::SetTemporarilyAllowIndieRenderTexture (true);

	// Graphics emulation might have changed, so we check for a change here
	bool supportedNow = (RenderTexture::IsEnabled());
	if (supportedNow != m_ImposterRenderTexture->GetSupported())
	{	
		m_ImposterRenderTexture.reset(new ImposterRenderTexture(*m_Database));
		m_BillboardMaterial->SetTexture(ShaderLab::Property("_MainTex"), m_ImposterRenderTexture->GetTexture());		
	}

	// If we don't support render textures or vertex shaders, there will be no
	// billboards.
	if (!m_ImposterRenderTexture->GetSupported())
	{
		billboardTreeDistance = meshTreeDistance;
		crossFadeLength = 0.0f;
	}

	meshTreeDistance = std::min(billboardTreeDistance, meshTreeDistance);
	crossFadeLength = clamp(crossFadeLength, 0.0F, billboardTreeDistance - meshTreeDistance);
	m_CrossFadeLength = crossFadeLength;
	m_SqrMeshTreeDistance = meshTreeDistance * meshTreeDistance;
	m_SqrBillboardTreeDistance = billboardTreeDistance * billboardTreeDistance;
	m_SqrCrossFadeEndDistance = (meshTreeDistance + m_CrossFadeLength) * (meshTreeDistance + m_CrossFadeLength);

	Transform& cameraTransform = camera.GetComponent(Transform);
	const Vector3f& cameraPos = cameraTransform.GetPosition();
	Vector3f cameraDir = cameraTransform.TransformDirection(Vector3f::zAxis);

	Plane frustum[kFrustumPlaneCount];
	ExtractProjectionPlanes(camera.GetWorldToClipMatrix(), frustum);

	// Mark as becoming invisible
	std::vector<TreeBinaryTree*> oldRenderedBatches = m_RenderedBatches;
	for (std::vector<TreeBinaryTree*>::iterator it = oldRenderedBatches.begin(), end = oldRenderedBatches.end(); it != end; ++it)
	{
		TreeBinaryTree* binTree = *it;

		// We need to check if the tree is actually visible here, since it might have been 
		// pulled away from us while rebuilding the bintree or adding a single tree in the editor
		if (binTree->visible == 1)
			binTree->visible = -1;
	}
	m_RenderedBatches.clear();

	m_FullTrees.clear();
	std::vector<int> billboardsList;

	// Build billboard meshes and stuff

#if EMIT_ALL_TREE_RENDERERS
	// calculate distance to camera: do this when we emit all tree renderers
	// because renderer's skewFade will be using it
	for (int i = 0; i < m_Database->GetInstances().size(); ++i)
	{
		TreeInstance& instance = m_Database->GetInstances()[i];
		Vector3f offset = GetPosition(instance) - cameraPos;
		offset.y = 0.0F;
		instance.temporaryDistance = SqrMagnitude(offset);
	}
#endif
	
	// Collect all batches, billboards and full trees
	// batches will be rendered in place
	RenderRecurse(m_TreeBinaryTree.get(), frustum, billboardsList, cameraPos);

	// Cleanup the batches that have become invisible
	for (std::vector<TreeBinaryTree*>::iterator it = oldRenderedBatches.begin(), end = oldRenderedBatches.end(); it != end; ++it)
	{
		TreeBinaryTree* binTree = *it;

		if (binTree->visible == -1)
		{
			DestroySingleObject(binTree->mesh);
			binTree->mesh = 0;
			binTree->visible = 0;
		}
	}

#if !EMIT_ALL_TREE_RENDERERS
	// Sort single trees back to front
	std::sort(m_FullTrees.begin(), m_FullTrees.end(), TreeInstanceIndexSorter(m_Database->GetInstances()));

	// We are exceeding the mesh tree limit
	// Move some trees into billboard list
	if (m_FullTrees.size() > maximumMeshTrees)
	{
		int moveToBillboardCount = m_FullTrees.size() - maximumMeshTrees;
		billboardsList.insert(billboardsList.end(), m_FullTrees.begin(), m_FullTrees.begin() + moveToBillboardCount);
		m_FullTrees.erase(m_FullTrees.begin(), m_FullTrees.begin() + moveToBillboardCount);
	}
#endif

	// Render the close by billboard mesh (Use immediate mode interface instead)
	
	// Sort billboards back to front
	std::sort(billboardsList.begin(), billboardsList.end(), TreeInstanceIndexSorter(m_Database->GetInstances()));

	const int lightCount = std::min<int>(lights.size(), 4);
	for (int i = 0; i < lightCount; ++i)
	{
		const Light* light = lights[i];
		const Transform& lightTransform = light->GetComponent(Transform);
		Vector3f forward = RotateVectorByQuat (-lightTransform.GetRotation(), Vector3f::zAxis);
		ShaderLab::g_GlobalProperties->SetVector(kTerrainTreeLightDirections[i], -forward.x, -forward.y, -forward.z, 0.0f);
		ColorRGBAf color = light->GetColor() * light->GetIntensity();
		color = GammaToActiveColorSpace (color);
		ShaderLab::g_GlobalProperties->SetVector(kTerrainTreeLightColors[i], color.GetPtr());
	}
	for (int i = lightCount; i < 4; ++i)
		ShaderLab::g_GlobalProperties->SetVector(kTerrainTreeLightColors[i], 0.0f, 0.0f, 0.0f, 0.0f);

	GetRenderSettings().SetupAmbient();

	if (m_ImposterRenderTexture->GetSupported())
		m_ImposterRenderTexture->UpdateImposters (camera);

	// Setup billboard shader properties
	UpdateShaderProps(camera);

	// Draw far away trees (only billboards)
	for (std::vector<TreeBinaryTree*>::const_iterator it = m_RenderedBatches.begin(), end = m_RenderedBatches.end(); it != end; ++it)
	{
		RenderMeshIdentityMatrix (*(*it)->mesh, *m_BillboardMaterial, layer, camera, m_PropertyBlock);
	}

	// Draw close by trees (billboard part for cross fade)
	if (m_ImposterRenderTexture->GetSupported() && !billboardsList.empty())
	{
		if (!m_CloseBillboardMesh)
		{
			m_CloseBillboardMesh = CreateObjectFromCode<Mesh>(kInstantiateOrCreateFromCodeAwakeFromLoad);
			m_CloseBillboardMesh->SetHideFlags(Object::kDontSave);
			m_CloseBillboardMesh->MarkDynamic(); // will be modified each frame, better use dynamic VBOs
		}
		GenerateBillboardMesh(*m_CloseBillboardMesh, billboardsList, true);
		RenderMeshIdentityMatrix (*m_CloseBillboardMesh, *m_BillboardMaterial, layer, camera, m_PropertyBlock);
	}

#if EMIT_ALL_TREE_RENDERERS
	// Draw all remaining trees (mesh part)
#else
	// Draw close by trees (mesh part)
#endif
	const float currentTime = GetTimeManager ().GetTimeSinceLevelLoad ();

	const ShaderLab::FastPropertyName propertyTerrainEngineBendTree = ShaderLab::Property("_TerrainEngineBendTree");
	const ShaderLab::FastPropertyName propertyColor = ShaderLab::Property("_Color");
	const ShaderLab::FastPropertyName propertyCutoff = ShaderLab::Property("_Cutoff");
	const ShaderLab::FastPropertyName propertyScale = ShaderLab::Property("_Scale");
	const ShaderLab::FastPropertyName propertySquashPlaneNormal = ShaderLab::Property("_SquashPlaneNormal");
	const ShaderLab::FastPropertyName propertySquashAmount = ShaderLab::Property("_SquashAmount");
	const ShaderLab::FastPropertyName propertyWind = ShaderLab::Property("_Wind");

	float billboardOffsetFactor = 1;
	if (m_ImposterRenderTexture->GetSupported())
	{
		float tempBillboardAngleFactor;
		m_ImposterRenderTexture->GetBillboardParams(tempBillboardAngleFactor, billboardOffsetFactor);
	}

	for (std::vector<int>::iterator it = m_FullTrees.begin(), end = m_FullTrees.end(); it != end; ++it)			 
	{
		const TreeInstance& instance = m_Database->GetInstances()[*it];

		const TreeDatabase::Prototype& prototype = m_Database->GetPrototypes()[instance.index];
		Mesh* mesh = prototype.mesh;
		if (!mesh)
			continue;

		const std::pair<int, int>& indexPair = m_TreeIndexToSceneNode[*it];
		if (indexPair.first == -1 || indexPair.second == 0)
		{
			continue;
		}

		const std::vector<Material*>& materials = prototype.materials;

		float distance = sqrt(instance.temporaryDistance);
		float skewFade = 1.0f;
		// Skewfade is 1 when it looks like a real full mesh tree
		// Skewfade is 0 where we switch to billboard
		if (!CompareApproximately(crossFadeLength, 0.0F))
			skewFade = SmoothStep(0, 1, (meshTreeDistance + crossFadeLength - distance) / crossFadeLength);
		float squashAmount = Lerp(0.04f, 1.0f, skewFade);

		// Only cast shadows from trees that are at full mesh appearance.
		// When they start squashing, just stop casting shadows.
		bool shadows = (skewFade >= 1.0f);

		AABB bounds;
		GetBounds(bounds, instance, m_Database->GetPrototypes(), m_Position, m_TerrainSize);

		// Tree bending
		Matrix4x4f rotate;
		Vector4f wind;
		CalculateTreeBend(bounds, skewFade * prototype.bendFactor, currentTime, rotate, wind);

		// we need to offset tree squash plane by the same amount as billboard is offsetted
		const float centerOffset = prototype.treeWidth * instance.widthScale * 0.5F;

		// that will work as long as the trees are not rotated
		// since the forward direction has to be in model space
		Vector3f forward = cameraDir;

		ColorRGBAf treeColor = instance.color * instance.lightmapColor;

		int materialCount = std::min<int> (mesh->GetSubMeshCount(), materials.size());
		materialCount = std::min(materialCount, indexPair.second);

		for (int m = 0; m < materialCount; ++m)
		{
			m_PropertyBlock.Clear();
			m_PropertyBlock.AddPropertyMatrix(propertyTerrainEngineBendTree, rotate);

			ColorRGBAf color = prototype.originalMaterialColors[m] * treeColor;

			// Use the stored inverseAlphaCutoff value, since we're modifying the cutoff value
			// in the material by using the property block below
			float cutoff = 0.5f / prototype.inverseAlphaCutoff[m];

			m_PropertyBlock.AddPropertyColor(propertyColor, color);
			m_PropertyBlock.AddPropertyFloat(propertyCutoff, cutoff);
			m_PropertyBlock.AddPropertyVector(propertyScale, Vector4f(instance.widthScale, instance.heightScale, instance.widthScale, 1));

			// squash properties
			// Position of squash plane has to match billboard plane - we use dual mode for billboard planes (see TerrainBillboardTree in TerrainEngine.cginc)
			m_PropertyBlock.AddPropertyVector(propertySquashPlaneNormal, Vector4f(forward.x, forward.y, forward.z, centerOffset * billboardOffsetFactor));
			m_PropertyBlock.AddPropertyFloat(propertySquashAmount, squashAmount);
			
			m_PropertyBlock.AddPropertyVector(propertyWind, wind);

			SceneNode& sceneNode = m_LegacyTreeSceneNodes[indexPair.first + m];
			TreeMeshIntermediateRenderer* r = static_cast<TreeMeshIntermediateRenderer*>(sceneNode.renderer);
			r->Update(layer, shadows, m_LightmapIndex != -1 ? 0xFE : -1, m_PropertyBlock);
			sceneNode.layer = layer;
		}
	}
	RenderTexture::SetTemporarilyAllowIndieRenderTexture (false);
}

void TreeRenderer::CollectTreeRenderers(dynamic_array<SceneNode>& sceneNodes, dynamic_array<AABB>& boundingBoxes)
{
	for (int i = 0; i < m_FullTrees.size(); ++i)
	{
		const std::pair<int, int>& indexPair = m_TreeIndexToSceneNode[m_FullTrees[i]];
		if (indexPair.first == -1 || indexPair.second == 0)
		{
			continue;
		}

		for (int j = 0; j < indexPair.second; ++j)
		{
			sceneNodes.push_back(m_LegacyTreeSceneNodes[indexPair.first + j]);
			boundingBoxes.push_back(m_LegacyTreeBoundingBoxes[indexPair.first + j]);
		}
	}
}


// --------------------------------------------------------------------------



namespace
{
	template <class T, class A>
	const T* GetData(const std::vector<T,A>& data) 
	{
		return data.empty() ? 0 : &data.front();
	}

	float Calculate2DSqrDistance (const AABB& rkBox, const Vector3f& rkPoint)
	{
		// compute coordinates of point in box coordinate system
		Vector3f kClosest = rkPoint - rkBox.GetCenter();
		kClosest.y = kClosest.z;
		Vector3f extent = rkBox.GetExtent();
		extent.y = extent.z;

		// project test point onto box
		float fSqrDistance = 0.0f;
		float fDelta;

		for (int i = 0; i < 2; ++i)
		{
			if ( kClosest[i] < -extent[i] )
			{
				fDelta = kClosest[i] + extent[i];
				fSqrDistance += fDelta * fDelta;
				kClosest[i] = -extent[i];
			}
			else if ( kClosest[i] > extent[i] )
			{
				fDelta = kClosest[i] - extent[i];
				fSqrDistance += fDelta * fDelta;
				kClosest[i] = extent[i];
			}
		}

		return fSqrDistance;
	}

	namespace SortUtility
	{
		static Vector3f sortDirections[] = {
			Vector3f(-1,  0,  1), Vector3f(0,  0,  1), Vector3f(1,  0,  1), 
			Vector3f(-1,  0,  0), Vector3f(0,  0,  0), Vector3f(1,  0,  0),
			Vector3f(-1,  0, -1), Vector3f(0,  0, -1), Vector3f(1,  0, -1)
		};	

		const int insideBoundsDirection = 4;

		/* Because we want to do funky alpha blending instead of alpha cutoff (It looks SO MUCH BETTER)
		We need to sort all patches. Now that would be really expensive, so we do a trick.

		We classify each patch by it's direction relative to the camera.
		There are 8 potential directions. 
		0  1   2
		3  4   5
		6  7   8

		So every frame we check if the direction of the patch has changed and most of the time it doesn't.
		Only when it changes do we resort. This lets us skip a lot of sorting.
		Patches inside of the bounding volume are constantly being resorted, which is expensive but we can't do much about it!
		*/
		int CalculateTargetSortIndex (const AABB& bounds, const Vector3f& cameraPos)
		{
			MinMaxAABB aabb(bounds);

			int sortIndex = 0;
			if (cameraPos.x > aabb.GetMax().x)
				sortIndex += 2;
			else if (cameraPos.x > aabb.GetMin().x)
				sortIndex += 1;

			if (cameraPos.z > aabb.GetMax().z)
				sortIndex += 0;
			else if (cameraPos.z > aabb.GetMin().z)
				sortIndex += 3;
			else
				sortIndex += 6;

			return sortIndex;
		}
	}

	struct BatchItem
	{
		int index;
		float distance;
	};

	bool operator<(const BatchItem& soi1, const BatchItem& soi2)
	{
		return soi1.distance < soi2.distance;
	}

	// Sorts a single batch by sort index (See SortUtility for more information)
	void SortBatch (const TreeBinaryTree& tree, int sortIndex)
	{
		// Create sort order indices which are later used to build the sorted triangle indices
		const std::vector<TreeInstance>& allInstances = tree.database->GetInstances();
		const std::vector<int>& instances = tree.instances;
		const int size = instances.size();
		std::vector<BatchItem> sortOrder(size);		
		for (int i = 0; i < size; ++i)
			sortOrder[i].index = i;

		if (sortIndex != SortUtility::insideBoundsDirection)
		{
			// Build array with distances
			const Vector3f& direction = SortUtility::sortDirections[sortIndex];
			for (int i = 0; i < size; ++i)
				sortOrder[i].distance = Dot(allInstances[instances[i]].position, direction);

			// Generate the sort order by sorting the sortDistances
			std::sort(sortOrder.begin(), sortOrder.end());
		}

		// Build a triangle list from the sort order
		UNITY_TEMP_VECTOR(UInt16) triangles(size * 6);
		for (int i=0; i < size; ++i)
		{
			int triIndex = i * 6; 
			int vertexIndex = sortOrder[i].index * 4; 
			triangles[triIndex+0] = vertexIndex + 0;
			triangles[triIndex+1] = vertexIndex + 1;
			triangles[triIndex+2] = vertexIndex + 2;
			triangles[triIndex+3] = vertexIndex + 2;
			triangles[triIndex+4] = vertexIndex + 1;
			triangles[triIndex+5] = vertexIndex + 3;
		}
		// Apply to mesh
		tree.mesh->SetIndicesComplex (GetData(triangles), triangles.size(), 0, kPrimitiveTriangles, Mesh::k16BitIndices | Mesh::kDontSupportSubMeshVertexRanges);
	}
}


void TreeRenderer::RenderRecurse(TreeBinaryTree* binTree, const Plane* planes, std::vector<int>& closeupBillboards, const Vector3f& cameraPos) 
{
	// if we have exactly zero trees, we get null nodes a bit down - not sure why...
	if (!binTree)
		return;

	float sqrDistance = CalculateSqrDistance(cameraPos, binTree->bounds);
	float sqr2DDistance = Calculate2DSqrDistance(binTree->bounds, cameraPos);
	if (sqr2DDistance > m_SqrBillboardTreeDistance)
		return;

	if (!IntersectAABBFrustumFull (binTree->bounds, planes))
	{
#if EMIT_ALL_TREE_RENDERERS
		// we need to queue them as renderers since they might be visible in some shadow map
		m_FullTrees.insert(m_FullTrees.end(), binTree->instances.begin(), binTree->instances.end());
#endif
		return;
	}

	// Recurse children
	if (binTree->instances.empty())
	{
		if (binTree->splittingPlane.GetSide(cameraPos))
		{
			RenderRecurse(binTree->right.get(), planes, closeupBillboards, cameraPos);
			RenderRecurse(binTree->left.get(), planes, closeupBillboards, cameraPos);
		}
		else
		{
			RenderRecurse(binTree->left.get(), planes, closeupBillboards, cameraPos);
			RenderRecurse(binTree->right.get(), planes, closeupBillboards, cameraPos);
		}
	}
	// Render leaf node
	else
	{
		// Render the trees as one big batch
		if (sqr2DDistance > m_SqrCrossFadeEndDistance)
		{
			binTree->targetSortIndex = SortUtility::CalculateTargetSortIndex(binTree->bounds, cameraPos);
			RenderBatch (*binTree, sqrDistance);

			if (binTree->targetSortIndex != binTree->sortIndex)
			{
				binTree->sortIndex = binTree->targetSortIndex;
				SortBatch(*binTree, binTree->sortIndex);
			}
		}
		// Render the trees individually
		else
		{
			for (std::vector<int>::iterator it = binTree->instances.begin(), end = binTree->instances.end(); it != end; ++it)
			{
				TreeInstance& instance = m_Database->GetInstances()[*it];
				Vector3f position = GetPosition(instance);
				Vector3f scale(instance.widthScale, instance.heightScale, instance.widthScale);
				AABB treeBounds = m_Database->GetPrototypes()[instance.index].bounds;
				treeBounds.SetCenterAndExtent(Scale(treeBounds.GetCenter(), scale) + position, Scale(treeBounds.GetExtent(), scale));

#if EMIT_ALL_TREE_RENDERERS
				float indivdual2DSqrDistance = instance.temporaryDistance;
				if (indivdual2DSqrDistance < m_SqrBillboardTreeDistance)
				{
					if (indivdual2DSqrDistance > m_SqrCrossFadeEndDistance
						&& IntersectAABBFrustumFull (treeBounds, planes))
					{
						closeupBillboards.push_back(*it);
					}
					else
					{
						m_FullTrees.push_back(*it);
					}
				}
#else
				if (!IntersectAABBFrustumFull (treeBounds, planes))
					continue;

				Vector3f offset = position - cameraPos;
				offset.y = 0.0F;
				float indivdual2DSqrDistance = SqrMagnitude(offset);
				instance.temporaryDistance = indivdual2DSqrDistance;

				// Tree is close, render it as a mesh
				if (indivdual2DSqrDistance < m_SqrCrossFadeEndDistance)
				{
					m_FullTrees.push_back(*it);
				}
				// Tree is still too far away, so render it as a billboard
				else if (indivdual2DSqrDistance < m_SqrBillboardTreeDistance)
				{
					closeupBillboards.push_back(*it);
				}
#endif
			}
		}
	}
}

void TreeRenderer::CleanupBillboardMeshes ()
{
	for (std::vector<TreeBinaryTree*>::iterator it = m_RenderedBatches.begin(), end = m_RenderedBatches.end(); it != end; ++it)
	{
		TreeBinaryTree& binTree = **it;
		if (binTree.visible != 0)
		{
			DestroySingleObject(binTree.mesh);
			binTree.mesh = 0;
			binTree.visible = 0;
		}
	}

	m_RenderedBatches.clear();

	DestroySingleObject(m_CloseBillboardMesh);
	m_CloseBillboardMesh = 0;
}

void TreeRenderer::RenderBatch (TreeBinaryTree& binTree, float sqrDistance)
{
	if (binTree.visible == 0)
	{
		DestroySingleObject(binTree.mesh);
		binTree.mesh = NULL;

		binTree.mesh = CreateObjectFromCode<Mesh>(kInstantiateOrCreateFromCodeAwakeFromLoad);

		binTree.mesh->SetHideFlags(Object::kDontSave);
		binTree.mesh->SetName("tree billboard");
		GenerateBillboardMesh (*binTree.mesh, binTree.instances, false);
		binTree.sortIndex = -1;
	}

	binTree.visible = 1;

	m_RenderedBatches.push_back(&binTree);
}

void TreeRenderer::UpdateShaderProps(const Camera& cam)
{
	const Transform& cameraTransform = cam.GetComponent(Transform);
	const Vector3f& pos = cameraTransform.GetPosition();

	// we want to get camera orientation from imposter in order to avoid any imprecisions
	const Matrix4x4f& cameraMatrix = m_ImposterRenderTexture->getCameraOrientation();
	const Vector3f& right	= cameraMatrix.GetAxisX();
	const Vector3f& up		= cameraMatrix.GetAxisY();
	const Vector3f& front	= cameraMatrix.GetAxisZ();

	if (m_ImposterRenderTexture->GetSupported())
	{
		float billboardAngleFactor, billboardOffsetFactor;
		m_ImposterRenderTexture->GetBillboardParams(billboardAngleFactor, billboardOffsetFactor);
		
		m_PropertyBlock.Clear();
		m_PropertyBlock.AddPropertyVector(ShaderLab::Property("_TreeBillboardCameraRight"), Vector4f(right.x, right.y, right.z, 0.0F));
		m_PropertyBlock.AddPropertyVector(ShaderLab::Property("_TreeBillboardCameraUp"), Vector4f(up.x, up.y, up.z, billboardOffsetFactor));
		m_PropertyBlock.AddPropertyVector(ShaderLab::Property("_TreeBillboardCameraFront"), Vector4f(front.x, front.y, front.z, 0));
		m_PropertyBlock.AddPropertyVector(ShaderLab::Property("_TreeBillboardCameraPos"), Vector4f(pos.x, pos.y, pos.z, billboardAngleFactor));	
		m_PropertyBlock.AddPropertyVector(ShaderLab::Property("_TreeBillboardDistances"), Vector4f(m_SqrBillboardTreeDistance,0,0,0));
	}
}


Vector3f TreeRenderer::GetPosition (const TreeInstance& instance) const
{
	return Scale(instance.position, m_TerrainSize) + m_Position;
}

struct TreeBillboardVertex
{
	static const unsigned kFormat = VERTEX_FORMAT4(Vertex, TexCoord0, TexCoord1, Color);
	
	Vector3f p;
	ColorRGBA32 color;
	Vector2f uv, size;
};

void TreeRenderer::GenerateBillboardMesh(Mesh& mesh, const std::vector<int>& instances, bool buildTriangles)
{
	const std::vector<TreeInstance>& allInstances = m_Database->GetInstances();
	int treeCount = instances.size();
	
	mesh.ResizeVertices (4 * treeCount, TreeBillboardVertex::kFormat);
	TreeBillboardVertex* pv = (TreeBillboardVertex*)mesh.GetVertexDataPointer ();
	bool swizzleColors = mesh.GetVertexColorsSwizzled();

	AABB bounds;

	// Generate tree billboards
	for (int i = 0; i < treeCount; ++i)
	{
		const TreeInstance& instance = allInstances[instances[i]];
		const TreeDatabase::Prototype& prototype = m_Database->GetPrototypes()[instance.index];

		const Vector3f position = Scale(instance.position, m_TerrainSize) + m_Position;
		ColorRGBAf color = instance.color;
		color *= instance.lightmapColor;

		const float halfWidth = prototype.treeWidth * instance.widthScale * 0.5F;
		const float halfHeight = prototype.treeHeight * instance.heightScale * 0.5F;
		const float centerOffset = prototype.getCenterOffset() * instance.heightScale;
		
		//const float billboardTop  = prototype.treeVisibleHeight * instance.heightScale;
		//const float billboardBottom  = (prototype.treeHeight - prototype.treeVisibleHeight) * instance.heightScale;

		const float billboardHeight = prototype.getBillboardHeight();
		const float billboardHeightDiff = (billboardHeight - prototype.treeHeight) / 2;

		const float billboardTop  = (prototype.treeVisibleHeight + billboardHeightDiff) * instance.heightScale;
		const float billboardBottom  = (prototype.treeHeight - prototype.treeVisibleHeight + billboardHeightDiff) * instance.heightScale;

		const float billboardHalfHeight2 = prototype.getBillboardHeight() * instance.widthScale	 * 0.5F;
		
		// We position billboards at the root of the tree and offset billboard top and bottom differently (see billboardTop and billboardBottom)
		// We blend between two modes (for billboard y axis in camera space):
		// 1) vertical, which is based on billboardBottom and billboardTop 
		// 2) horizontal, which is based on billboardHalfHeight2 (the same for top and bottom)
		//
		// See TerrainBillboardTree in TerrainEngine.cginc for more detailed explanation
		int index = i * 4;
		pv[index + 0].p = position;
		pv[index + 1].p = position;
		pv[index + 2].p = position;
		pv[index + 3].p = position;

		const Rectf& area = m_ImposterRenderTexture->GetArea(instance.index);

		// we're packing billboardHalfHeight2 into first uvs, and then recompute uvs.y by using formula uvs.y = uvs.y > 0 ? 1 : 0;
		pv[index + 0].uv.Set(area.x,			-billboardHalfHeight2);
		pv[index + 1].uv.Set(area.GetRight(), -billboardHalfHeight2);
		pv[index + 2].uv.Set(area.x,			 billboardHalfHeight2);
		pv[index + 3].uv.Set(area.GetRight(),  billboardHalfHeight2);

		pv[index + 0].size.Set(-halfWidth, -billboardBottom);
		pv[index + 1].size.Set( halfWidth, -billboardBottom);
		pv[index + 2].size.Set(-halfWidth,  billboardTop);
		pv[index + 3].size.Set( halfWidth,  billboardTop);
		
		ColorRGBA32 color32 = (ColorRGBA32)color;
		if (swizzleColors)
			color32 = SwizzleColorForPlatform(color32);
		pv[index + 0].color = pv[index + 1].color = pv[index + 2].color = pv[index + 3].color = color32;
		
		const Vector3f treeBounds(halfWidth, halfHeight, halfWidth);
		const Vector3f treeCenter = position + Vector3f(0, centerOffset, 0);
		if (0 == i)
		{
			bounds.SetCenterAndExtent(treeCenter, treeBounds);
		}
		else
		{
			bounds.Encapsulate(treeCenter + treeBounds);
			bounds.Encapsulate(treeCenter - treeBounds);
		}
	}

	mesh.SetBounds(bounds);
	mesh.SetChannelsDirty (mesh.GetAvailableChannels (), false);

	if (buildTriangles)
	{
		UNITY_TEMP_VECTOR(UInt16) triangles(instances.size() * 6);
		for (int i = 0; i < instances.size(); ++i)
		{
			int triIndex = i * 6; 
			int vertexIndex = i * 4; 
			triangles[triIndex+0] = vertexIndex + 0;
			triangles[triIndex+1] = vertexIndex + 1;
			triangles[triIndex+2] = vertexIndex + 2;
			triangles[triIndex+3] = vertexIndex + 2;
			triangles[triIndex+4] = vertexIndex + 1;
			triangles[triIndex+5] = vertexIndex + 3;
		}
		// Apply to mesh
		mesh.SetIndicesComplex (GetData(triangles), triangles.size(), 0, kPrimitiveTriangles, Mesh::k16BitIndices | Mesh::kDontSupportSubMeshVertexRanges);
	}	
}

void TreeRenderer::CreateTreeRenderer(int instance)
{
	const TreeInstance& tree = m_Database->GetInstances()[instance];

	Assert(instance == m_TreeIndexToSceneNode.size());
	std::pair<int, int>& indexPair = m_TreeIndexToSceneNode.push_back();

	const TreeDatabase::Prototype& prototype = m_Database->GetPrototypes()[tree.index];
	Mesh* mesh = prototype.mesh;
	if (mesh == NULL)
	{
		indexPair.first = -1;
		indexPair.second = 0;
		return;
	}

	const std::vector<Material*>& materials = prototype.materials;

	Matrix4x4f modelview;
	modelview.SetTRS(GetPosition(tree), Quaternionf::identity(), Vector3f::one);

	const int materialCount = std::min<int> (mesh->GetSubMeshCount(), materials.size());

	indexPair.first = m_LegacyTreeSceneNodes.size();
	indexPair.second = materialCount;

	for (int m = 0; m < materialCount; ++m)
	{
		AABB aabb = mesh->GetBounds();
		aabb.GetCenter().y += aabb.GetExtent().y * tree.heightScale - aabb.GetExtent().y;
		aabb.SetCenterAndExtent (aabb.GetCenter(), Vector3f (aabb.GetExtent().x * tree.widthScale, aabb.GetExtent().y * tree.heightScale, aabb.GetExtent().z * tree.widthScale));

		Assert(TreeMeshIntermediateRenderer::s_Allocator == NULL);
		TreeMeshIntermediateRenderer::s_Allocator = &m_RendererAllocator;
		TreeMeshIntermediateRenderer* r = new TreeMeshIntermediateRenderer();
		TreeMeshIntermediateRenderer::s_Allocator = NULL;
		r->Initialize(modelview, mesh, aabb, materials[m], 0, false, false, m);

		m_LegacyTreeSceneNodes.push_back(SceneNode());
		SceneNode& node = m_LegacyTreeSceneNodes.back();
		node.renderer = r;
		node.layer = 0;
	
		r->GetWorldAABB(m_LegacyTreeBoundingBoxes.push_back());
	}
}

void TreeRenderer::DeleteTreeRenderers()
{
	Assert(TreeMeshIntermediateRenderer::s_Allocator == NULL);
	TreeMeshIntermediateRenderer::s_Allocator = &m_RendererAllocator;
	for (int i = 0; i < m_LegacyTreeSceneNodes.size(); ++i)
	{
		delete m_LegacyTreeSceneNodes[i].renderer;
	}
	TreeMeshIntermediateRenderer::s_Allocator = NULL;

#if ENABLE_THREAD_CHECK_IN_ALLOCS
	Assert(m_RendererAllocator.m_Allocated == 0);
#endif
	m_RendererAllocator.purge();
}

#endif // ENABLE_TERRAIN
