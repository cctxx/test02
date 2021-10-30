#pragma once
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/Texture2D.h"
#include <list>

class Texture2D;

// Annotation info for a component
struct Annotation
{
	enum Flags { kHasIcon = 1 << 0, kHasGizmo = 1 << 1 };

	DECLARE_SERIALIZE_NO_PPTR(Annotation);
	Annotation ()
		: m_ClassID(-1), m_ScriptClass(""), m_Flags(0), m_IconEnabled(false), m_GizmoEnabled(false) {}

	Annotation (int classID, const std::string& scriptClass, int flags, bool gizmoEnabled, bool iconEnabled)
		: m_ClassID(classID), m_ScriptClass(scriptClass), m_Flags(flags), m_GizmoEnabled(gizmoEnabled), m_IconEnabled(iconEnabled) {}

	bool HasGizmo() { return (m_Flags & kHasGizmo) > 0;	}
	bool HasIcon () { return (m_Flags & kHasIcon) > 0;	}

	bool m_IconEnabled;				
	bool m_GizmoEnabled;			
	int m_ClassID;					// Builtin components are defined by classID (MonoBehavior classID if this is a user script)
	UnityStr m_ScriptClass;		// User scripts are defined by scriptclass name ("" if this is a builtin component)
	int m_Flags;
};




class AnnotationManager : public Object
{
public:
	REGISTER_DERIVED_CLASS (AnnotationManager, Object);
	DECLARE_OBJECT_SERIALIZE (AnnotationManager);

	AnnotationManager (MemLabelId label, ObjectCreationMode mode);

	// Current annotation state
	const std::vector<Annotation>& GetAnnotations ();
	const Annotation* GetAnnotation (Object *obj);
	const Annotation* GetAnnotation (int classID, const std::string& scriptClass);
	const std::vector<Annotation>& GetRecentlyChangedAnnotations ();
	void SetGizmoEnabled (int classID, const std::string& scriptClass, bool gizmoEnabled);
	void SetIconEnabled (int classID, const std::string& scriptClass, bool iconEnabled);

	// Presets of annotations
	const std::string& GetNameOfCurrentSetup ();
	std::vector<std::string> GetPresetList ();
	void SavePreset (const std::string& presetName);
	void LoadPreset (const std::string& presetName);
	void DeletePreset (const std::string& presetName);
	void ResetPresetsToFactorySettings ();

	// Misc 
	void InspectorExpandedStateWasChanged (int classID, const std::string& scriptClass, bool expanded);
	void SetIconForObject (Object* obj, const Texture2D* icon);
	Texture2D* GetIconForObject (Object* obj) const;
	void Set3dGizmosEnabled (bool enable);
	bool Is3dGizmosEnabled () const;
	void SetShowGrid (bool enable);
	bool GetShowGrid () const;
	void SetIconSize (float iconSize);
	float GetIconSize ();
	
private:
	Annotation* GetAnnotationInternal (int classID, const std::string& scriptClass);
	void SetMostRecentChanged (Annotation* a);
	void AnnotationWasChanged (Annotation* annotation);
	void Refresh();
	int FindPresetIndex (const std::string& presetName);
	void PrintCurrentSetup();

	// A 'Preset' is a list of states for e.g gizmoEnabled, iconEnabled etc.
	struct Preset
	{
		std::string m_Name;
		std::vector<Annotation> m_AnnotationList;
	};

	// Current preset
	Preset m_CurrentPreset;

	// Presets to choose from
	std::vector<Preset> m_Presets;

	// Misc data
	std::vector<Annotation> m_RecentlyChanged;
	float m_IconSize;
	bool m_Use3dGizmos;
	bool m_ShowGrid;
	UInt32 m_GizmosSyncID;
	bool m_UseInspectorExpandedState;
};

AnnotationManager& GetAnnotationManager ();




	//void IsIconEnabled(Object* obj); // set expanded state
	//void EnableIcon(Object* obj, bool enable); // set expanded state

 
//	bool IsEnabled(Object* obj, Type type);
//	void SetEnabled(bool enable, Object* obj, Type type);
//	enum Type {k_Icon, k_Gizmo, k_NumTypes};

//		std::map<SInt32, Annotation> m_AnnotationMap;
//		std::map<std::string, Annotation> mScriptStateMap;

//	// All 
// std::vector<SInt32> mComponentClassIDs;
//	std::vector<std::string> mScriptsThatDrawsGizmos;

//	AnnotationManager();
//	~AnnotationManager();
