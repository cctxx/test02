#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/NavMesh/NavMeshTypes.h"
#if UNITY_EDITOR
#include "Editor/Src/NavMesh/NavMeshBuildSettings.h"
#endif

class NavMesh;
class dtNavMesh;

class NavMeshSettings : public LevelGameManager
{
public:

	REGISTER_DERIVED_CLASS (NavMeshSettings, LevelGameManager);
	DECLARE_OBJECT_SERIALIZE (NavMeshSettings);

	NavMeshSettings (MemLabelId& label, ObjectCreationMode mode);

	virtual void		AwakeFromLoad (AwakeFromLoadMode mode);
	virtual void		Reset ();

	inline void			SetNavMesh (NavMesh* navMesh);
	inline NavMesh*		GetNavMesh ();

	bool				SetOffMeshPolyInstanceID (dtPolyRef ref, int instanceID);
	void                SetOffMeshPolyCostOverride (dtPolyRef ref, float costOverride);
	void				SetOffMeshPolyAccess (dtPolyRef ref, bool access);


	#if UNITY_EDITOR
	inline NavMeshBuildSettings& GetNavMeshBuildSettings ();
	#endif

	static void InitializeClass ();
	static void CleanupClass ();

	dtNavMesh* GetInternalNavMesh ();
private:

#if UNITY_EDITOR
	NavMeshBuildSettings m_BuildSettings;
#endif

	PPtr<NavMesh> m_NavMesh;
};

inline void NavMeshSettings::SetNavMesh (NavMesh* navMesh)
{
	m_NavMesh = navMesh;
	SetDirty ();
}

inline NavMesh* NavMeshSettings::GetNavMesh ()
{
	return m_NavMesh;
}

#if UNITY_EDITOR
inline NavMeshBuildSettings& NavMeshSettings::GetNavMeshBuildSettings ()
{
	return m_BuildSettings;
}
#endif

NavMeshSettings& GetNavMeshSettings ();

