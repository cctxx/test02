#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Serialize/SerializeUtility.h"

#include "SplatDatabase.h"
#include "DetailDatabase.h"
#include "Heightmap.h"
#include "TreeDatabase.h"
#include "Runtime/Utilities/NonCopyable.h"


class TerrainData : public NamedObject, NonCopyable
{
  public:
	REGISTER_DERIVED_CLASS(TerrainData, NamedObject)
	DECLARE_OBJECT_SERIALIZE(TerrainData)

	TerrainData (MemLabelId label, ObjectCreationMode mode);

	SplatDatabase &GetSplatDatabase ();
	DetailDatabase &GetDetailDatabase ();
	Heightmap &GetHeightmap () { return m_Heightmap; }
	TreeDatabase& GetTreeDatabase () { return m_TreeDatabase; }
	
	bool HasUser (GameObject *user) const;
	void AddUser (GameObject *user);
	void RemoveUser (GameObject *user);

	static void InitializeClass ();	
	static void CleanupClass ();

	enum ChangedFlags
	{
		kNoChange = 0,
		kHeightmap = 1,
		kTreeInstances = 2, 
		kDelayedHeightmapUpdate = 4,
		kFlushEverythingImmediately = 8,
		kRemoveDirtyDetailsImmediately = 16,
		kWillBeDestroyed = 256,
	};

	// Sends a callback to any users of this terrainsData (typically C# Terrain objects) so they can update their renderers, etc.
	void UpdateUsers (ChangedFlags changedFlag);
	void SetLightmapIndexOnUsers(int lightmapIndex);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	void AwakeFromLoadThreaded();

	void ExtractPreloadShaders (vector<PPtr<Object> >& shaders);
	
  private:
	SplatDatabase m_SplatDatabase;
	TreeDatabase m_TreeDatabase;
	DetailDatabase m_DetailDatabase;
	Heightmap m_Heightmap;
	std::set<PPtr<GameObject> > m_Users;		// List of terrains for the client callbacks
};
ENUM_FLAGS(TerrainData::ChangedFlags);

#endif // ENABLE_TERRAIN
