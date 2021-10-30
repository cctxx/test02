#include "UnityPrefix.h"
#include "HierarchyState.h"
#include "Selection.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

using namespace std;

IMPLEMENT_CLASS_HAS_INIT (HierarchyState)
IMPLEMENT_OBJECT_SERIALIZE (HierarchyState)

void HierarchyState::InitializeClass ()
{
	RegisterAllowNameConversion(HierarchyState::GetClassStringStatic(), "scrollposition.x", "scrollposition_x");
	RegisterAllowNameConversion(HierarchyState::GetClassStringStatic(), "scrollposition.y", "scrollposition_y");
}

template<class TransferFunction>
void HierarchyState::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	AssertIf ((transfer.GetFlags() & kPerformUnloadDependencyTracking));
	transfer.Transfer (expanded, "expanded");
	transfer.Transfer (selection, "selection");
	transfer.Transfer (scrollposition.x, "scrollposition_x");
	transfer.Transfer (scrollposition.y, "scrollposition_y");
}

HierarchyState::HierarchyState (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	scrollposition.x = 0;
	scrollposition.y = 0;
	m_AllowSetDirty = true;
}

HierarchyState::~HierarchyState ()
{ }

bool HierarchyState::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}

vector<int> HierarchyState::GetExpandedArray ()
{
	std::vector<int> expandedArray;
	for (std::set<PPtr<Object> >::iterator i=expanded.begin();i != expanded.end();i++)
	{
		expandedArray.push_back(i->GetInstanceID());
	}
	return expandedArray;
}

void HierarchyState::SetExpandedArray (const int* objs, int count)
{
	expanded.clear();
	for (int i=0;i<count;i++)
		expanded.insert(PPtr<Object> (objs[i]));
	
	if (!IsPersistentDirty() && m_AllowSetDirty)
	{
		SetDirty();
	}
}
