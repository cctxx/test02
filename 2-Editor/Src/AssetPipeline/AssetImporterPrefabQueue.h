#pragma once

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/BaseClasses/BaseObject.h"

class Prefab;

class AssetImportPrefabQueue
{
	public:
	
	void QueuePrefab (PPtr<Prefab> prefab);
	void ApplyMergePrefabQueue ();
	
	static AssetImportPrefabQueue& Get ();

	static void InitializeClass();
	static void CleanupClass();
	
private:
	
	dynamic_array<PPtr<Prefab> > m_Prefabs;
};