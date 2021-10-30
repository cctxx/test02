#include "UnityPrefix.h"
#include "Tree.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "Wind.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"

IMPLEMENT_CLASS_HAS_INIT(Tree)
IMPLEMENT_OBJECT_SERIALIZE(Tree)

SHADERPROP(Wind);

Tree::Tree(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

Tree::~Tree()
{
}

void Tree::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (Tree, kOnWillRenderObject, OnWillRenderObject);
}

UInt32 Tree::CalculateSupportedMessages ()
{
	return kHasOnWillRenderObject;
}


template<class TransferFunc>
void Tree::Transfer (TransferFunc& transfer)
{
	Super::Transfer(transfer);
	TRANSFER_EDITOR_ONLY(m_TreeData);
}

void Tree::SetTreeData (PPtr<MonoBehaviour> tree)
{
	#if UNITY_EDITOR
	m_TreeData = tree;
	SetDirty();
	#endif
}

PPtr<MonoBehaviour> Tree::GetTreeData ()
{
	#if UNITY_EDITOR
	return m_TreeData;
	#else
	return NULL;
	#endif
}

void Tree::OnWillRenderObject()
{
	MeshRenderer* renderer = QueryComponent(MeshRenderer);
	if (renderer == NULL)
		return;
	
	AABB bounds;
	renderer->GetWorldAABB(bounds);
	
	// Compute wind factor from wind zones
	Vector4f wind = WindManager::GetInstance().ComputeWindForce(bounds);
	
	// Apply material property block
	MaterialPropertyBlock& block = renderer->GetPropertyBlockRememberToUpdateHash ();
	block.Clear();
	block.AddPropertyVector(kSLPropWind, wind);
	renderer->ComputeCustomPropertiesHash();
}
