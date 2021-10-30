#include "UnityPrefix.h"
#include "OffMeshLink.h"
#include "NavMesh.h"
#include "NavMeshSettings.h"
#include "NavMeshManager.h"
#include "DetourNavMesh.h"
#include "NavMeshLayers.h"
#include "DetourCrowd.h"
#include "Runtime/Utilities/ValidateArgs.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

OffMeshLink::OffMeshLink (MemLabelId& label, ObjectCreationMode mode)
:	Super (label, mode)
{
	m_NavMeshLayer = 0;
	m_StaticPolyRef = 0;
	m_DynamicPolyRef = 0;
	m_ManagerHandle = -1;
	m_CostOverride = -1.0f;
	m_BiDirectional = true;
	m_AutoUpdatePositions = false;
	m_ShouldUpdateDynamic = false;
	m_Activated = true;
}

void OffMeshLink::Reset ()
{
	Super::Reset ();
}

void OffMeshLink::SmartReset ()
{
	Super::SmartReset ();
#if UNITY_EDITOR
	m_NavMeshLayer = GetGameObject ().GetNavMeshLayer ();
#endif
}

OffMeshLink::~OffMeshLink ()
{
	DisableDynamic ();
}

void OffMeshLink::AddToManager ()
{
	if (m_StaticPolyRef)
	{
		GetNavMeshSettings ().SetOffMeshPolyAccess (m_StaticPolyRef, m_Activated);

		// TODO: We really should be uging users to update here - but this warning breaks runtime tests because of unexpected log.
		//WarningStringObject ("This OffMeshLink is static. To make it dynamic please re-bake the navmesh or reset this OffMeshLink component", this);
	}
	else
	{
		UpdatePositions ();
	}
}

void OffMeshLink::RemoveFromManager ()
{
	if (m_StaticPolyRef)
		GetNavMeshSettings ().SetOffMeshPolyAccess (m_StaticPolyRef, m_Activated);

	DisableDynamic ();
}

template<class TransferFunc>
void OffMeshLink::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);

	transfer.SetVersion (2);

	TRANSFER (m_NavMeshLayer); //< Bake time only

	#if UNITY_EDITOR
	// NavMeshLayer has been moved from the game object to OffMeshLink in Unity 4.0
	// This ensures that the value is pulled from the game object in AwakeFromLoad
	if (transfer.IsOldVersion (1))
	{
		m_NavMeshLayer = 0xFFFFFFFF;
	}
	#endif

	TRANSFER (m_Start); //< Bake time only
	TRANSFER (m_End); //< Bake time only

	///@TODO: Get rid of the static polygon reference when breaking backwards compatibility
	/// all component-based offmeshlinks should be dynamic.
	transfer.Transfer (m_StaticPolyRef, "m_DtPolyRef", kHideInEditorMask);
																					// from being copied when duplicating an OML
	TRANSFER (m_CostOverride); //< Changes propagated to navmesh at runtime

	transfer.Align ();
	TRANSFER (m_BiDirectional); //< Bake time only
	TRANSFER (m_Activated); //< Changes propagated to navmesh at runtime
	TRANSFER (m_AutoUpdatePositions); //< Changes propagated to navmesh at runtime

}

void OffMeshLink::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);

	#if UNITY_EDITOR
	// NavMeshLayer has been moved from the game object to OffMeshLink in Unity 4.0
	// This ensures that the value is pulled from the game object in AwakeFromLoad
	if (m_NavMeshLayer == 0xFFFFFFFF && GetGameObjectPtr ())
	{
		m_NavMeshLayer = GetGameObject ().GetNavMeshLayer ();
	}
	#endif

	NavMeshSettings& settings = GetNavMeshSettings ();

	// Prioritize static polyref (baked) over dynamic
	if (m_StaticPolyRef && !m_AutoUpdatePositions)
	{
		DisableDynamic ();
		// The object instanceIDs may change from load to load.
		// So storing the instanceID (of OffMeshLink) in baked data is not an option.
		//
		// Instead we attempt to set it given the polyref we got from the bake process.
		//
		// However: storing references (polyRef) to baked data in an object (OffMeshLink) is unsafe!
		// eg: a user does not save the scene after baking navmesh - and later opens scene.
		// Or: user disables an OffMeshLink, bakes navmesh, and re-enables offmeshlink.

		if (settings.SetOffMeshPolyInstanceID (m_StaticPolyRef, GetInstanceID ()))
		{
			settings.SetOffMeshPolyCostOverride (m_StaticPolyRef, m_CostOverride);
			settings.SetOffMeshPolyAccess (m_StaticPolyRef, m_Activated);
		}
	}
	else
	{
		m_ShouldUpdateDynamic = true;
	}
}

void OffMeshLink::UpdatePositions ()
{
	if (!IsActive ())
		return;

	DisableStatic ();
	DisableDynamic (false);
	EnableDynamic ();
	m_ShouldUpdateDynamic = false;
}

void OffMeshLink::EnableDynamic ()
{
#if DT_DYNAMIC_OFFMESHLINK
	if (m_ManagerHandle == -1)
		GetNavMeshManager ().RegisterOffMeshLink (*this, m_ManagerHandle);

	if (m_DynamicPolyRef || !m_Activated || !m_End || !m_Start || m_NavMeshLayer == NavMeshLayers::kNotWalkable)
		return;

	NavMeshSettings& settings = GetNavMeshSettings ();
	dtNavMesh* navmesh = settings.GetInternalNavMesh ();
	if (!navmesh)
		return;

	const int instanceID = GetInstanceID ();
	const Vector3f startPos = m_Start->GetPosition ();
	const Vector3f endPos = m_End->GetPosition ();

	m_DynamicPolyRef = navmesh->AddDynamicOffMeshLink (startPos.GetPtr (), endPos.GetPtr (), instanceID, m_BiDirectional, (unsigned char)m_NavMeshLayer);
	if (m_DynamicPolyRef)
	{
		settings.SetOffMeshPolyCostOverride (m_DynamicPolyRef, m_CostOverride);
	}
#endif
}

void OffMeshLink::CheckDynamicPositions ()
{
#if DT_DYNAMIC_OFFMESHLINK
	if (m_DynamicPolyRef == 0 || m_ShouldUpdateDynamic)
		return;

	NavMeshSettings& settings = GetNavMeshSettings ();
	dtNavMesh* navmesh = settings.GetInternalNavMesh ();
	if (!navmesh)
		return;

	if (!m_Start || !m_End)
		return;

	const Vector3f objectStartPos = m_Start->GetPosition ();
	const Vector3f objectEndPos = m_End->GetPosition ();
	const dtOffMeshConnection* con = navmesh->GetDynamicOffMeshLink (m_DynamicPolyRef);
	if (con)
	{
		const float connectionRadius = con->rad;
		const Vector3f startPos = Vector3f (&con->pos[0]);
		const Vector3f endPos = Vector3f (&con->pos[3]);
		m_ShouldUpdateDynamic = !CompareApproximately (startPos, objectStartPos, connectionRadius) || !CompareApproximately (endPos, objectEndPos, connectionRadius);
	}
#endif
}

void OffMeshLink::OnNavMeshCleanup ()
{
	ClearDynamicPolyRef ();
}

void OffMeshLink::OnNavMeshChanged ()
{
	ClearDynamicPolyRef ();
	m_ShouldUpdateDynamic = true;
}

void OffMeshLink::UpdateMovedPositions ()
{
	if (m_AutoUpdatePositions)
		CheckDynamicPositions ();

	if (m_ShouldUpdateDynamic || m_DynamicPolyRef == 0)
	{
		UpdatePositions ();
	}
}

void OffMeshLink::DisableStatic ()
{
	GetNavMeshSettings ().SetOffMeshPolyAccess (m_StaticPolyRef, false);
}

void OffMeshLink::DisableDynamic (bool unregister)
{
#if DT_DYNAMIC_OFFMESHLINK
	if (unregister && m_ManagerHandle != -1)
		GetNavMeshManager ().UnregisterOffMeshLink (m_ManagerHandle);

	if (m_DynamicPolyRef == 0)
		return;

	if (dtNavMesh* navmesh = GetNavMeshSettings ().GetInternalNavMesh ())
		navmesh->RemoveDynamicOffMeshLink (m_DynamicPolyRef);

	m_DynamicPolyRef = 0;
#endif
}

void OffMeshLink::SetCostOverride (float costOverride)
{
	ABORT_INVALID_FLOAT (costOverride, costOverride, OffMeshLink);
	if (m_CostOverride == costOverride)
		return;

	dtPolyRef ref = GetStaticOrDynamicPolyRef ();
	GetNavMeshSettings ().SetOffMeshPolyCostOverride (ref, costOverride);
	m_CostOverride = costOverride;
	SetDirty ();
}

void OffMeshLink::SetBiDirectional (bool bidirectional)
{
	if (m_BiDirectional == bidirectional)
		return;
	m_BiDirectional = bidirectional;
	SetDirty ();
}

void OffMeshLink::SetActivated (bool activated)
{
	if (m_Activated == activated)
		return;

	m_Activated = activated;

	if (m_StaticPolyRef)
	{
		GetNavMeshSettings ().SetOffMeshPolyAccess (m_StaticPolyRef, activated);
	}
	else
	{
		if (activated && !m_DynamicPolyRef)
		{
			EnableDynamic ();
		}
		else if (!activated && m_DynamicPolyRef)
		{
			DisableDynamic ();
		}
	}
	SetDirty ();
}

void OffMeshLink::SetNavMeshLayer (UInt32 layer)
{
	if (m_NavMeshLayer == layer)
		return;

	m_NavMeshLayer = layer;
	UpdatePositions ();
	SetDirty ();
}


bool OffMeshLink::GetOccupied () const
{
	if (const dtCrowd* crowd = GetNavMeshManager ().GetCrowdSystem ())
	{
		const dtPolyRef ref = GetStaticOrDynamicPolyRef ();
		return crowd->IsRefOccupied (ref);
	}
	return false;
}

IMPLEMENT_CLASS (OffMeshLink)
IMPLEMENT_OBJECT_SERIALIZE (OffMeshLink)
