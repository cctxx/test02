#ifndef INSPECTOREXPANDEDSTATE_H
#define INSPECTOREXPANDEDSTATE_H


#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Editor/Src/InspectorState.h"
#include <list>

class InspectorExpandedState : public Object
{
public:
	REGISTER_DERIVED_CLASS (InspectorExpandedState, Object)
	DECLARE_OBJECT_SERIALIZE (InspectorExpandedState)
	InspectorExpandedState (MemLabelId label, ObjectCreationMode mode);

	struct ExpandedData
	{
		DECLARE_SERIALIZE(ExpandedData)

		bool m_InspectorExpanded;
		int m_ClassID;
		UnityStr m_ScriptClass;
		std::vector<UnityStr> m_ExpandedProperties;
	};

	ExpandedData* GetExpandedData (Object* obj);
	ExpandedData* GetExpandedData (int classID, const std::string& scriptClass);
	void SetInspectorExpanded (Object* obj, bool inspectorExpanded);
	void SetInspectorExpanded (int classID, const std::string& scriptClass, bool inspectorExpanded);
	bool IsInspectorExpanded (Object* obj);
	bool IsInspectorExpanded (int classID, const std::string& scriptClass);
	void SetExpandedData (Object* obj, const std::vector<UnityStr>& properties);

	InspectorState& GetInspectorState () {return m_InspectorState;}

private:
	ExpandedData* CreateExpandedData (Object* obj);
	ExpandedData* CreateExpandedData (int classID, const std::string& scriptClass);

	typedef std::list<ExpandedData> ExpandedDatas;
	ExpandedDatas m_ExpandedData;
	
	// General cache for inspector data
	// - Can be used to hold gui state across play/stop, assembly reloads. 
	// - Cached data here only lives during a session of Unity -> not persistant
	InspectorState m_InspectorState;

};

InspectorExpandedState& GetInspectorExpandedState ();

#endif

