#include "UnityPrefix.h"
#include "OcclusionPortal.h"
#include "UnityScene.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

OcclusionPortal::OcclusionPortal (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
	m_Open = true;
	m_PortalIndex = -1;
	m_Center = Vector3f(0,0,0);
	m_Size = Vector3f(1,1,1);
}

OcclusionPortal::~OcclusionPortal ()
{
}

bool OcclusionPortal::CalculatePortalEnabled ()
{
	if (!IsActive())
		return true;

	return m_Open;
}

void OcclusionPortal::SetIsOpen (bool open)
{
	m_Open = open;
	
	if (m_PortalIndex != -1)
		GetScene().SetOcclusionPortalEnabled (m_PortalIndex, CalculatePortalEnabled());
}

void OcclusionPortal::Deactivate (DeactivateOperation operation)
{
	if (m_PortalIndex != -1)
		GetScene().SetOcclusionPortalEnabled (m_PortalIndex, true);
}

void OcclusionPortal::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	
	if (m_PortalIndex != -1)
		GetScene().SetOcclusionPortalEnabled (m_PortalIndex, CalculatePortalEnabled());
}

template<class TransferFunction>
void OcclusionPortal::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_Open);
	transfer.Align();
	TRANSFER (m_Center);
	TRANSFER (m_Size);
}

IMPLEMENT_OBJECT_SERIALIZE (OcclusionPortal)
IMPLEMENT_CLASS (OcclusionPortal)