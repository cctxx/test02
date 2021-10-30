#pragma once

#include "PropertyModification.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include <map>

class Prefab;

class BatchApplyPropertyModification
{
public:
	BatchApplyPropertyModification() : m_AwakeQueue(kMemEditorUtility) {}

	void Apply (const PropertyModification& value, bool hasPrefabOverride);
	void Complete ();
	
	~BatchApplyPropertyModification ();

private:
	void RemovePropertyModifications (Prefab& inputPrefab, const PropertyModification& value);
	void AddPropertyModification (Prefab& inputPrefab, const PropertyModification& value);

	AwakeFromLoadQueue		m_AwakeQueue;

	std::map<PPtr<Prefab>, PropertyModifications> m_PrefabPropertyModifications;
};
