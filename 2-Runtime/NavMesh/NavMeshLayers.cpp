#include "UnityPrefix.h"
#include "NavMeshLayers.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "NavMeshManager.h"


const char* NavMeshLayers::s_WarningCostLessThanOne = "Setting a NavMeshLayer cost less than one can give unexpected results.";

NavMeshLayers::NavMeshLayers (MemLabelId& label, ObjectCreationMode mode)
: Super (label, mode)
{

}

NavMeshLayers::~NavMeshLayers ()
{

}

void NavMeshLayers::Reset ()
{
	Super::Reset ();

	m_Layers[kNotWalkable].name = "Not Walkable";
	m_Layers[kNotWalkable].cost = 1.0f;
	m_Layers[kNotWalkable].editType = NavMeshLayerData::kEditNone;

	m_Layers[kDefaultLayer].name = "Default";
	m_Layers[kDefaultLayer].cost = 1.0f;
	m_Layers[kDefaultLayer].editType  = NavMeshLayerData::kEditCost;

	m_Layers[kJumpLayer].name = "Jump";
	m_Layers[kJumpLayer].cost = 2.0f;
	m_Layers[kJumpLayer].editType = NavMeshLayerData::kEditCost;

	for (int i = kBuiltinLayerCount; i < kLayerCount; ++i)
	{
		m_Layers[i].cost = 1.0F;
		m_Layers[i].editType = NavMeshLayerData::kEditCost | NavMeshLayerData::kEditName;
	}
}


template<class TransferFunction>
void NavMeshLayers::NavMeshLayerData::Transfer (TransferFunction& transfer)
{
	TransferMetaFlags nameFlag = (editType & kEditName) ? kNoTransferFlags : kNotEditableMask;
	TransferMetaFlags costFlag = (editType & kEditCost) ? kNoTransferFlags : kNotEditableMask;
	transfer.Transfer (name, "name", nameFlag);
	transfer.Transfer (cost, "cost", costFlag);
	transfer.Transfer (editType, "editType", kNotEditableMask|kHideInEditorMask);
}

template<class TransferFunction>
void NavMeshLayers::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	for (int i = 0; i < kLayerCount; ++i)
	{
		char name[64];
		if (i < kBuiltinLayerCount)
			sprintf (name, "Built-in Layer %d", i);
		else
			sprintf (name, "User Layer %d", i - kBuiltinLayerCount);

		transfer.Transfer (m_Layers[i], name);
	}
}

void NavMeshLayers::SetLayerCost (unsigned int index, float cost)
{
	if (index >= kLayerCount)
	{
		ErrorString ("Index out of bounds");
		return;
	}
#if UNITY_EDITOR
	if (cost < 1.0f)
	{
		WarningString(s_WarningCostLessThanOne);
	}
#endif
	m_Layers[index].cost = cost;
	GetNavMeshManager ().UpdateAllNavMeshAgentCosts (index, cost);

	SetDirty ();
}

float NavMeshLayers::GetLayerCost (unsigned int index) const
{
	if (index >= kLayerCount)
	{
		ErrorString ("Index out of bounds");
		return 0.0F;
	}
	return m_Layers[index].cost;
}

void NavMeshLayers::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	// When the user changes the cost in the inspector
	if (UNITY_EDITOR && (awakeMode & kDidLoadFromDisk) == 0)
	{
		for (int i = 0; i < kLayerCount; ++i)
			GetNavMeshManager ().UpdateAllNavMeshAgentCosts (i, m_Layers[i].cost);
	}
}

int NavMeshLayers::GetNavMeshLayerFromName (const UnityStr& layerName) const
{
	for (int i = 0; i < kLayerCount; ++i)
	{
		if (m_Layers[i].name.compare (layerName) == 0)
		{
			return i;
		}
	}
	return -1;
}

std::vector<std::string> NavMeshLayers::NavMeshLayerNames () const
{
	std::vector<std::string> layers;
	for (int i = 0; i < kLayerCount; ++i)
	{
		if (m_Layers[i].name.length () != 0)
		{
			layers.push_back (m_Layers[i].name);
		}
	}
	return layers;
}

void NavMeshLayers::CheckConsistency ()
{
#if UNITY_EDITOR
	for (int i = 0; i < kLayerCount; ++i)
	{
		if (m_Layers[i].cost < 1.0f)
		{
			WarningString (s_WarningCostLessThanOne);
			return;
		}
	}
#endif
}


IMPLEMENT_CLASS (NavMeshLayers)
IMPLEMENT_OBJECT_SERIALIZE (NavMeshLayers)
GET_MANAGER (NavMeshLayers)
