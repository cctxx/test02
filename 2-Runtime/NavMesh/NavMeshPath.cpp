#include "UnityPrefix.h"
#include "NavMeshPath.h"
#include "NavMeshSettings.h"
#include "OffMeshLink.h"
#include "NavMesh.h"


NavMeshPath::NavMeshPath ()
: m_polygonCount (0)
, m_status (kPathInvalid)
, m_timeStamp (0)
{
}

NavMeshPath::~NavMeshPath ()
{
}


