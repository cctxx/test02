#pragma once

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Math/Vector3.h"

class LODGroup : public Unity::Component
{
public:
	
	struct LODRenderer
	{
		PPtr<Renderer> renderer;
		
		DECLARE_SERIALIZE (LODRenderer)
	};
	
	typedef dynamic_array<LODRenderer> LODRenderers;
	struct LOD
	{
		float        screenRelativeHeight;
		LODRenderers renderers;
		
		LOD ()
			: screenRelativeHeight (0.0F)
		{ }
		
		
		DECLARE_SERIALIZE (LOD)
	};
	typedef std::vector<LOD> LODArray;

	REGISTER_DERIVED_CLASS (LODGroup, Component)
	DECLARE_OBJECT_SERIALIZE(LODGroup)

	LODGroup (MemLabelId label, ObjectCreationMode mode);
	// virtual ~LODGroup (); declared-by-macro
	
	virtual void Reset ();
	virtual void SmartReset ();
	virtual void CheckConsistency ();
	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	virtual void Deactivate (DeactivateOperation operation);

	// Property get / set
	Vector3f GetLocalReferencePoint ()                    { return m_LocalReferencePoint; }
	void     SetLocalReferencePoint (const Vector3f& ref);
	Vector3f GetWorldReferencePoint ();
	
	float GetWorldSpaceSize ();

	
	void UpdateEnabledState (bool active);
	bool GetEnabled();
	void SetEnabled(bool enabled);

	float    GetSize () const                             { return m_Size; }
	void     SetSize (float size);
	
	int        GetLODCount () const                       { return m_LODs.size(); } 
	const LOD& GetLOD (int index);
	void       SetLODArray (const LODArray& lodArray);
	void       GetLODArray (LODArray& lodArray) const     { lodArray = m_LODs; }
	int        GetLODGroup () const                       { return m_LODGroup; }
	
	// Interface for Renderer / Scene
	void ClearCachedRenderers ();
	void RegisterCachedRenderers ();
	void RemoveFromCachedRenderers (Renderer* renderer);
	void NotifyLODGroupManagerIndexChange (int newIndex);
	void GetLODGroupIndexAndMask (Renderer* renderer, UInt32* outLODGroupIndex, UInt32* outActiveLODMask);
	
	static void InitializeClass();
	static void CleanupClass();
	
	// Supported messages
	void OnTransformChanged (int options);
	float GetWorldSpaceScale ();

private:
	void Create();
	void Cleanup();
	
	void SyncLODGroupManager ();
	
	Vector3f            m_LocalReferencePoint;
	float               m_Size;
	LODArray            m_LODs;
	int                 m_LODGroup;
	bool				m_Enabled;
	float               m_ScreenRelativeTransitionHeight;
	
	
	typedef dynamic_array<Renderer*> CachedRenderers;
	CachedRenderers     m_CachedRenderers;
	
	friend class LODGroupManager;
};


struct MonoLOD
{
	float screenRelativeTransitionHeight;
	ScriptingArrayPtr renderers;
};