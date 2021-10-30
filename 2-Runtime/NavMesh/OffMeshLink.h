#ifndef RUNTIME_OFFMESHLINK
#define RUNTIME_OFFMESHLINK

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Graphics/Transform.h"
#include "NavMeshTypes.h"

class dtNavMesh;



class OffMeshLink : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS (OffMeshLink, Behaviour)
	DECLARE_OBJECT_SERIALIZE (OffMeshLink)

	OffMeshLink (MemLabelId& label, ObjectCreationMode mode);
	// ~OffMeshLink (); declared by a macro

	inline void SetManagerHandle (int handle);
	inline float GetCostOverride () const;
	void SetCostOverride (float costOverride);

	inline bool GetBiDirectional () const;
	void SetBiDirectional (bool bidirectional);

	inline bool GetActivated () const;
	void SetActivated (bool activated);

	bool GetOccupied () const;

	inline UInt32 GetNavMeshLayer () const;
	void SetNavMeshLayer (UInt32 layer);

	inline void ClearDynamicPolyRef ();
	inline bool ClearStaticPolyRef ();

	inline Transform* GetStartTransform () const;
	inline void SetStartTransform (Transform* t);

	inline Transform* GetEndTransform () const;
	inline void SetEndTransform (Transform* t);

	void OnNavMeshCleanup ();
	void OnNavMeshChanged ();
	void UpdatePositions ();
	void UpdateMovedPositions ();

	inline bool GetAutoUpdatePositions () const;
	inline void SetAutoUpdatePositions (bool autoUpdate);

protected:
	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void Reset ();
	virtual void SmartReset ();

private:
	void EnableDynamic ();
	void DisableDynamic (bool unregister = true);
	void DisableStatic ();
	void CheckDynamicPositions ();
	inline dtPolyRef GetStaticOrDynamicPolyRef () const;

	PPtr<Transform> m_Start;
	PPtr<Transform> m_End;

	UInt32 m_NavMeshLayer;
	bool m_AutoUpdatePositions;
	bool m_ShouldUpdateDynamic;

	dtPolyRef m_StaticPolyRef;
	dtPolyRef m_DynamicPolyRef;
	int m_ManagerHandle;
	float m_CostOverride;
	bool m_BiDirectional;
	bool m_Activated;
};


inline void OffMeshLink::SetManagerHandle (int handle)
{
	m_ManagerHandle = handle;
}

inline float OffMeshLink::GetCostOverride () const
{
	return m_CostOverride;
}

inline bool OffMeshLink::GetBiDirectional () const
{
	return m_BiDirectional;
}

inline bool OffMeshLink::GetActivated () const
{
	return m_Activated;
}

inline UInt32 OffMeshLink::GetNavMeshLayer () const
{
	return m_NavMeshLayer;
}

inline void OffMeshLink::ClearDynamicPolyRef ()
{
	m_DynamicPolyRef = 0;
}

inline bool OffMeshLink::ClearStaticPolyRef ()
{
	if (m_StaticPolyRef == 0)
	{
		return false;
	}
	m_StaticPolyRef = 0;
	SetDirty ();
	return true;
}

inline Transform* OffMeshLink::GetStartTransform () const
{
	return m_Start;
}

inline void OffMeshLink::SetStartTransform (Transform* t)
{
	if (t == m_Start)
	{
		return;
	}
	m_Start = t;
	SetDirty ();
}

inline Transform* OffMeshLink::GetEndTransform () const
{
	return m_End;
}

inline void OffMeshLink::SetEndTransform (Transform* t)
{
	if (t == m_End)
	{
		return;
	}
	m_End = t;
	SetDirty ();
}

inline bool OffMeshLink::GetAutoUpdatePositions () const
{
	return m_AutoUpdatePositions;
}

inline void OffMeshLink::SetAutoUpdatePositions (bool autoUpdate)
{
	m_AutoUpdatePositions = autoUpdate;
	SetDirty ();
}

inline dtPolyRef OffMeshLink::GetStaticOrDynamicPolyRef () const
{
	return m_StaticPolyRef ? m_StaticPolyRef : m_DynamicPolyRef;
}

#endif
