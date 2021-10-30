#ifndef HIERARCHYSTATE_H
#define HIERARCHYSTATE_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/GUID.h"

class HierarchyState : public Object
{
	public:
	REGISTER_DERIVED_CLASS (HierarchyState, Object)
	DECLARE_OBJECT_SERIALIZE (HierarchyState)
	
	HierarchyState (MemLabelId label, ObjectCreationMode mode);
	
	std::vector<int> GetExpandedArray ();
	void SetExpandedArray (const int* objs, int count);
	virtual bool ShouldIgnoreInGarbageDependencyTracking ();
	
	static void InitializeClass ();
	static void CleanupClass () {  }
	
	std::set<PPtr<Object> > expanded;
	std::set<PPtr<Object> > selection;
	Vector2f scrollposition;
	bool m_AllowSetDirty;
};

#endif
