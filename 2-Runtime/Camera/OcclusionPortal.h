#ifndef OCCLUSION_PORTAL_H
#define OCCLUSION_PORTAL_H

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Vector3.h"

class OcclusionPortal : public Unity::Component
{
public:
	REGISTER_DERIVED_CLASS (OcclusionPortal, Component)
	DECLARE_OBJECT_SERIALIZE (OcclusionPortal)    
	
	OcclusionPortal (MemLabelId label, ObjectCreationMode mode);
	
	void SetPortalIndex (int portalIndex) { m_PortalIndex = portalIndex; }
	
	void SetIsOpen (bool opened);
	bool GetIsOpen ()                   { return m_Open; }

	
	Vector3f GetCenter () { return m_Center; }
	Vector3f GetSize () { return m_Size; }
	
	bool CalculatePortalEnabled ();

	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	virtual void Deactivate (DeactivateOperation operation);

	private:
	
	Vector3f m_Center;
	Vector3f m_Size;
	int      m_PortalIndex;
	bool     m_Open;
};

#endif