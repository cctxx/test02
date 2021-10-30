#ifndef MESHRENDERER_H
#define MESHRENDERER_H

#include "Runtime/Filters/Renderer.h"

class Mesh;



class MeshRenderer : public Renderer {
  public:
	MeshRenderer (MemLabelId label, ObjectCreationMode mode);
	// ~MeshRenderer ();	 declared-by-macro
	REGISTER_DERIVED_CLASS (MeshRenderer, Renderer)
	static void InitializeClass ();
	
	// Tag class as sealed, this makes QueryComponent faster.
	static bool IsSealedClass ()				{ return true; }
	
	static void RenderMultiple (const BatchInstanceData* instances, size_t count, const ChannelAssigns& channels);
	virtual void Render (int materialIndex, const ChannelAssigns& channels);
	
	virtual void UpdateLocalAABB();

	virtual void SetSubsetIndex(int subsetIndex, int index);

	virtual int GetStaticBatchIndex() const;
	virtual UInt32 GetMeshIDSmall() const;
	int GetMeshStaticBatchIndex() const;
	 
	void TransformChanged (int changeMask);
	void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Deactivate (DeactivateOperation operation);
	
	void SetSharedMesh (PPtr<Mesh> mesh);
	PPtr<Mesh> GetSharedMesh ();
	
	Mesh& GetInstantiatedMesh ();
	void SetInstantiatedMesh (Mesh* mesh);

	Mesh* GetMeshUsedForRendering();

	void DidModifyMeshBounds ();
	void DidModifyMeshValidity ();
	void DidModifyMesh ();
	void DidDeleteMesh ();
	#if UNITY_EDITOR
	float GetCachedSurfaceArea ();
	virtual void GetRenderStats (RenderStats& renderStats);
	#endif

	static bool CanUseDynamicBatching(const Mesh& mesh, UInt32 wantedChannels, int vertexCount);

  private:

	Mesh* GetCachedMesh ();

  	ListNode<Object> m_MeshNode;
  	void UpdateCachedMesh ();

	void FreeScaledMesh ();
  	
  	Mesh*           m_CachedMesh;
  	PPtr<Mesh>  m_Mesh;
	
	struct ScaledMesh
	{
		Matrix4x4f matrix;
		Mesh* mesh;
	};
	
	ScaledMesh*       m_ScaledMesh;
	
	// as we have padding anyway, we can add more flags here
  	UInt8			m_ScaledMeshDirty;
	// setted on responce to event to properly handle vertices changing on non-uniform scale
	UInt8			m_MeshWasModified; 
	// for future
	UInt16			m_Padding16;	

	#if UNITY_EDITOR
	float m_CachedSurfaceArea;
	#endif

};

#endif
