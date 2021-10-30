#ifndef UNITYSCENE_H
#define UNITYSCENE_H

#include "Runtime/Geometry/AABB.h"
#include "IntermediateRenderer.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "SceneNode.h"
#include "Runtime/Utilities/LinkedList.h"
#include "UmbraTomeData.h"

struct SceneCullingParameters;
struct CullingParameters;
struct CullingOutput;
class BaseRenderer;
class OcclusionPortal;
class Renderer;
class IntermediateRenderer;
class LODGroupManager;
class MinMaxAABB;
class Texture2D;

namespace Unity { class Component; class Culler; }
namespace Umbra { class QueryExt; class Tome; class CameraTransform; class Visibility; }

typedef int UmbraInt32;

// Function called when a node is determined to be visible.
//typedef bool CullFunction( void* user, const SceneNode& node, const AABB& aabb, float lodFade );

namespace Unity
{

// Simple dummy for the scene class, used by the cullers.
class Scene
{
public:

	Scene ();
	~Scene ();

	void NotifyVisible(const CullingOutput& visibleObjects);
	void NotifyInvisible();

	void BeginCameraRender();
	void EndCameraRender();

	// Adds/removes from the scene
	SceneHandle AddRenderer (Renderer *renderer);
	BaseRenderer* RemoveRenderer (SceneHandle handle);

	// Functions to set information about renderers
	void SetDirtyAABB (SceneHandle handle)									{ SceneNode& node = m_RendererNodes[handle]; if (!node.dirtyAABB) { m_DirtyAABBList.push_back(handle); node.dirtyAABB = true; } }
	void SetRendererAABB (SceneHandle handle, const AABB& aabb)				{ m_BoundingBoxes[handle] = aabb; m_RendererNodes[handle].dirtyAABB = false; }
	void SetRendererLayer (SceneHandle handle, UInt32 layer)				{ m_RendererNodes[handle].layer = layer; }
	void SetRendererLODGroup (SceneHandle handle, int group)				{ m_RendererNodes[handle].lodGroup = group; }
	void SetRendererLODIndexMask (SceneHandle handle, UInt32 mask)			{ m_RendererNodes[handle].lodIndexMask = mask; }
	void SetRendererNeedsCullCallback (SceneHandle handle, bool flag)		{ m_RendererNodes[handle].needsCullCallback = flag; }

	// Const access only to SceneNode and AABB!
	// Scene needs to be notified about changes and some data is private
	const SceneNode&	GetRendererNode (SceneHandle handle)				{ return m_RendererNodes[handle]; }
	const AABB&			GetRendererAABB (SceneHandle handle)				{ return m_BoundingBoxes[handle]; }

	const SceneNode* GetStaticSceneNodes () const;
	const SceneNode* GetDynamicSceneNodes () const;

	const AABB* GetStaticBoundingBoxes () const;
	const AABB* GetDynamicBoundingBoxes () const;
	
	size_t GetRendererNodeCount () const { return m_RendererNodes.size(); }
	size_t GetStaticObjectCount () const;
	size_t GetDynamicObjectCount () const;
	size_t GetIntermediateObjectCount () const;
	
	void RecalculateDirtyBounds();

	// Intermediate nodes
	void ClearIntermediateRenderers();
	IntermediateRenderers& GetIntermediateRenderers () { return m_IntermediateNodes; }

	void SetOcclusionPortalEnabled (unsigned int portalIndex, bool enabled);
	
	
	//@TODO: REview this function
	void SetPreventAddRemoveRenderer(bool enable);

	
	#if DEBUGMODE
	bool HasNodeForRenderer( const BaseRenderer* r );
	#endif	
	
	#if UNITY_EDITOR
	void GetUmbraDebugLines (const CullingParameters& cullingParameters, dynamic_array<Vector3f>& lines, bool targetCells);

	unsigned GetUmbraDataSize ();

	bool IsPositionInPVSVolume (const Vector3f& pos);
	
	#endif

	Umbra::QueryExt*    GetUmbraQuery () { return m_UmbraQuery; }
	int					GetNumRenderers() { return m_RendererNodes.size(); }

	const UmbraTomeData& GetUmbraTome ()  { return m_UmbraTome; }

	void CleanupPVSAndRequestRebuild ();
	
	static void InitializeClass ();
	static void CleanupClass ();
	
private:
	void InitializeUmbra();
	void CleanupUmbra ();
	void CleanupUmbraNodesAndQuery ();

	SceneHandle AddRendererInternal (Renderer *renderer, int layer, const AABB& aabb);

	bool IsDirtyAABB (SceneHandle handle) const { return m_RendererNodes[handle].dirtyAABB; }
	
	IntermediateRenderers	       m_IntermediateNodes;
	dynamic_array<SceneHandle>     m_PendingRemoval;
	
	enum
	{
		kVisibleCurrentFrame = 1 << 0,
		kVisiblePreviousFrame = 1 << 1,
		kBecameVisibleCalled = 1 << 2
	};
	
	// These arrays are always kept in sync
	dynamic_array<SceneNode> m_RendererNodes;
	dynamic_array<AABB>      m_BoundingBoxes;
	dynamic_array<UInt8>     m_VisibilityBits;

	// Other data	
	dynamic_array<SceneHandle> m_DirtyAABBList;
	
	Umbra::QueryExt*	   m_UmbraQuery;
	UInt8*                 m_GateState;
	
	
	UmbraTomeData          m_UmbraTome;
	
	int                    m_PreventAddRemoveRenderer;
	bool                   m_RequestStaticPVSRebuild;
};

Scene& GetScene ();

	
} // namespace Unity


#endif
