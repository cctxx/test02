#include "UnityPrefix.h"
#include "LightProbeGroup.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

using namespace std;

#if UNITY_EDITOR
static LightProbeGroupList gAllLightProbeGroups;
#endif

LightProbeGroup::LightProbeGroup (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
#if UNITY_EDITOR
	, m_LightProbeGroupNode (this)
#endif
{
}

LightProbeGroup::~LightProbeGroup ()
{
}

IMPLEMENT_CLASS (LightProbeGroup)
IMPLEMENT_OBJECT_SERIALIZE (LightProbeGroup)

template<class T> inline
void LightProbeGroup::Transfer (T& transfer)
{
	Super::Transfer (transfer);
	
	TRANSFER_EDITOR_ONLY (m_SourcePositions);
}

#if UNITY_EDITOR
void LightProbeGroup::AddToManager ()
{
	gAllLightProbeGroups.push_back (m_LightProbeGroupNode);
}

void LightProbeGroup::RemoveFromManager ()
{
	m_LightProbeGroupNode.RemoveFromList ();
}

LightProbeGroupList& GetLightProbeGroups ()
{
	return gAllLightProbeGroups;
}
#endif
