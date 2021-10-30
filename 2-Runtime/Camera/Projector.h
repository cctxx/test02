#ifndef PROJECTOR_H
#define PROJECTOR_H

#include "Runtime/GameCode/Behaviour.h"
#include "Renderable.h"
#include "Runtime/Math/Matrix4x4.h"

namespace Unity { class Material; }
struct CullResults;
struct ProjectorRenderSettings;

class Projector : public Behaviour, Renderable {
public:
	Projector ();
	
	REGISTER_DERIVED_CLASS (Projector, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Projector)
	
	Projector (MemLabelId label, ObjectCreationMode mode);
	
	// Renderable
	virtual void RenderRenderable (const CullResults& cullResults);
	
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	
	virtual void Reset();
	virtual void CheckConsistency ();
	
	void SetNearClipPlane (float inNear) { m_NearClipPlane = inNear; SetDirty(); }
	float GetNearClipPlane () const { return m_NearClipPlane; }
	
	void SetFarClipPlane (float farPlane) { m_FarClipPlane = farPlane; SetDirty(); }
	float GetFarClipPlane () const { return m_FarClipPlane; }

	void SetFieldOfView (float angle) { m_FieldOfView = angle; SetDirty(); }
	float GetFieldOfView () const { return m_FieldOfView; }

	void SetAspectRatio (float aspect) { m_AspectRatio = aspect; SetDirty(); }
	float GetAspectRatio () const { return m_AspectRatio; }	

	void SetOrthographic (bool isOrtho) { m_Orthographic = isOrtho; SetDirty(); }
	bool GetOrthographic () const { return m_Orthographic; }	

	void SetOrthographicSize (float size) { m_OrthographicSize = size; SetDirty(); }
	float GetOrthographicSize () const { return m_OrthographicSize; }	

	PPtr<Material> GetMaterial () const { return m_Material; }
	void SetMaterial (PPtr<Material> material) { m_Material = material; }
	
	int GetIgnoreLayers () { return m_IgnoreLayers.m_Bits; }
	void SetIgnoreLayers(int layers) { m_IgnoreLayers.m_Bits = layers; SetDirty(); }
		
	Matrix4x4f GetProjectorToPerspectiveMatrix() const;
	static void InitializeClass();
	static void CleanupClass() { }

private:
		
	void SetupProjectorSettings (Material* material, ProjectorRenderSettings& settings);
	Matrix4x4f CalculateProjectionMatrix() const;
	
	float           m_NearClipPlane;
	float           m_FarClipPlane;
	float           m_FieldOfView;
	float           m_AspectRatio;
	bool            m_Orthographic;
	float           m_OrthographicSize;

	BitField 		m_IgnoreLayers; 
	PPtr<Material>  m_Material;		///< Custom material to apply. If set it overrides, texture & blend mode settings
};

#endif
