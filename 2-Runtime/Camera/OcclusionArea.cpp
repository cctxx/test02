#include "UnityPrefix.h"
#include "OcclusionArea.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

OcclusionArea::OcclusionArea (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

OcclusionArea::~OcclusionArea ()
{
}

void OcclusionArea::Reset ()
{
	Super::Reset();	
	
	m_Center = Vector3f::zero;
	m_Size = Vector3f::one;
	m_IsViewVolume = true;
//	m_OverrideResolution = false;
//	m_SmallestVoxelSize = 1.0F;
//	m_SmallestHoleSize = 0.1F;
//	m_BackfaceCulling = 0.0F;
}

Vector3f OcclusionArea::GetGlobalExtents () const
{
	Vector3f extents = GetComponent (Transform).GetWorldScaleLossy ();
	extents.Scale (m_Size);
	extents *= 0.5F;
	extents = Abs (extents);
	return extents;
}

Vector3f OcclusionArea::GetGlobalCenter () const
{
	return GetComponent (Transform).TransformPoint (m_Center);
}

using namespace Unity;

template<class TransferFunction>
void OcclusionArea::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	
	TRANSFER(m_Size);
	TRANSFER(m_Center);
	TRANSFER(m_IsViewVolume);
//	TRANSFER(m_OverrideResolution);
	transfer.Align();
	
//	TRANSFER(m_SmallestVoxelSize);
//	TRANSFER(m_SmallestHoleSize);
//	TRANSFER(m_BackfaceCulling);
}

IMPLEMENT_OBJECT_SERIALIZE (OcclusionArea)
IMPLEMENT_CLASS_HAS_INIT (OcclusionArea)

void OcclusionArea::InitializeClass ()
{
}
