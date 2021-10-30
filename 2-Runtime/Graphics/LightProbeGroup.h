#ifndef LIGHTPROBEGROUP_H
#define LIGHTPROBEGROUP_H

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/dynamic_array.h"

class LightProbeGroup : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS (LightProbeGroup, Behaviour)
	DECLARE_OBJECT_SERIALIZE (LightProbeGroup)

	LightProbeGroup (MemLabelId label, ObjectCreationMode mode);
	
#if UNITY_EDITOR
	void SetPositions(Vector3f* data, int size) { m_SourcePositions.assign(data, data + size); SetDirty(); }
	Vector3f* GetPositions() { return m_SourcePositions.size() > 0 ? &m_SourcePositions[0] : NULL; }
	int GetPositionsSize() { return m_SourcePositions.size(); }

	virtual void AddToManager ();
	virtual void RemoveFromManager ();
#else
	virtual void AddToManager () {}
	virtual void RemoveFromManager () {}
#endif

private:
#if UNITY_EDITOR
	dynamic_array<Vector3f> m_SourcePositions;
	ListNode<LightProbeGroup> m_LightProbeGroupNode;
#endif
};

#if UNITY_EDITOR
typedef List< ListNode<LightProbeGroup> > LightProbeGroupList;
LightProbeGroupList& GetLightProbeGroups ();
#endif

#endif
