#include "UnityPrefix.h"
#include "BatchApplyPropertyModification.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Filters/Renderer.h"

using namespace std;

void BatchApplyPropertyModification::Apply (const PropertyModification& value, bool hasPrefabOverride)
{
	Object* target = value.target;
	if (target == NULL)
		return;

	// Revert material property blocks for renderers when we are no longer animating it.
	Renderer* renderer = dynamic_pptr_cast<Renderer*> (target);
	if (renderer && renderer->GetClassID() != ClassID(SpriteRenderer) )
		renderer->ClearPropertyBlock();

	// Revert prefab override
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(target);
	if (!hasPrefabOverride && prefab != NULL)
	{
		RemovePropertyModifications (*prefab, value);
	}
	else
	{
		if (prefab != NULL)
		{
			AddPropertyModification(*prefab, value);
		}
		else
		{
			// Revert value
			const TypeTree& typeTree = GenerateCachedTypeTree(*target, kSerializeForPrefabSystem);

			dynamic_array<UInt8> buffer(kMemTempAlloc);
			WriteObjectToVector(*target, &buffer, kSerializeForPrefabSystem);

			ApplyPropertyModification(typeTree, buffer, value);

			ReadObjectFromVector(target, buffer, kSerializeForPrefabSystem);
		}

		// Add both instance objects and prefabs to the awake from load queue
		// Objects simply need a simple awake, while prefabs are merged first
		if (!m_AwakeQueue.IsInQueue(*target))
			m_AwakeQueue.Add(*target);
	}
}

void BatchApplyPropertyModification::RemovePropertyModifications (Prefab& inputPrefab, const PropertyModification& value)
{
	map<PPtr<Prefab>, PropertyModifications>::iterator i = m_PrefabPropertyModifications.find(PPtr<Prefab>(&inputPrefab));
	if (i == m_PrefabPropertyModifications.end())
		i = m_PrefabPropertyModifications.insert(make_pair(PPtr<Prefab>(&inputPrefab), inputPrefab.GetPropertyModifications())).first;

	RemovePropertyModification(i->second, value.target, value.propertyPath);
}

void BatchApplyPropertyModification::AddPropertyModification (Prefab& inputPrefab, const PropertyModification& value)
{
	map<PPtr<Prefab>, PropertyModifications>::iterator i = m_PrefabPropertyModifications.find(PPtr<Prefab>(&inputPrefab));
	if (i == m_PrefabPropertyModifications.end())
		i = m_PrefabPropertyModifications.insert(make_pair(PPtr<Prefab>(&inputPrefab), inputPrefab.GetPropertyModifications())).first;

	PropertyModification copy = value;
	copy.target = GetPrefabParentObject(value.target);
	InsertPropertyModification(copy, i->second);
}

void BatchApplyPropertyModification::Complete()
{
	map<PPtr<Prefab>, PropertyModifications>::iterator i = m_PrefabPropertyModifications.begin();
	for (i; i != m_PrefabPropertyModifications.end(); ++i)
	{
		SetPropertyModifications(*i->first, i->second);
	}

	// Make all objects that had property changes applied are awaken and marked dirty
	m_AwakeQueue.CheckConsistency();
	m_AwakeQueue.AwakeFromLoad(kDefaultAwakeFromLoad);

	dynamic_array<PPtr<Object> > objectsToDirty;
	m_AwakeQueue.ExtractAllObjects(objectsToDirty);
	for (int i = 0; i < objectsToDirty.size(); i++)
	{
		objectsToDirty[i]->SetDirty();
	}

	m_AwakeQueue.Clear();
	m_PrefabPropertyModifications.clear();
}

BatchApplyPropertyModification::~BatchApplyPropertyModification ()
{
}
