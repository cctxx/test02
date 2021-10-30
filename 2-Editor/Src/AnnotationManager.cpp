#include "UnityPrefix.h"
#include "Editor/Src/AnnotationManager.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/InspectorExpandedState.h"
#include "Editor/Src/Utility/ActiveEditorTracker.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Utilities/PlayerPrefs.h"


using std::vector;

namespace {

	AnnotationManager* gSingleton = NULL;
	const bool s_Debug = false;

	inline bool ScriptSort (MonoScript* lhs, MonoScript* rhs)
	{
		return StrICmp (lhs->GetName(), rhs->GetName()) < 0;
	}


	void ForceReloadInspector ()
	{
		GetSceneTracker().ForceReloadInspector(true);
	}

	std::string GetScriptClass (Object* obj)
	{
		MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (obj);
		if (behaviour)
			return behaviour->GetScriptClassName();
		return "";

	}
}


AnnotationManager& GetAnnotationManager ()
{
	AssertIf(gSingleton == NULL);
	return *gSingleton;
}



IMPLEMENT_CLASS(AnnotationManager)
IMPLEMENT_OBJECT_SERIALIZE(AnnotationManager)


AnnotationManager::AnnotationManager (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode), m_UseInspectorExpandedState(true), m_Use3dGizmos (true), m_ShowGrid (true), m_IconSize(0.03f), m_GizmosSyncID(0)
{
	AssertIf(gSingleton != NULL);
	gSingleton = this;
}

AnnotationManager::~AnnotationManager ()
{
	AssertIf(gSingleton != this);
	gSingleton = NULL;
}

const std::string& AnnotationManager::GetNameOfCurrentSetup ()
{
	return m_CurrentPreset.m_Name;
}


const std::vector<Annotation>& AnnotationManager::GetAnnotations ()
{
	Refresh ();	

	return m_CurrentPreset.m_AnnotationList;
}




Annotation* AnnotationManager::GetAnnotationInternal (int classID, const std::string& scriptClass)
{
	std::vector<Annotation>& curList = m_CurrentPreset.m_AnnotationList;

	// Are we looking for a script or a builtin component
	Annotation* a = NULL;
	if (!scriptClass.empty())
	{
		for (unsigned i=0; i<curList.size(); ++i)
			if (curList[i].m_ScriptClass == scriptClass)
			{
				a = &curList[i];
				break;
			}
	}
	else
	{
		for (unsigned i=0; i<curList.size(); ++i)
			if (curList[i].m_ClassID == classID)
			{
				a = &curList[i];
				break;
			}
	}

	if (a)
	{
		if (m_UseInspectorExpandedState)
		{
			// Ensure in-sync
			a->m_GizmoEnabled = GetInspectorExpandedState().IsInspectorExpanded (a->m_ClassID, a->m_ScriptClass);
		}
	}

	return a;
}

const Annotation* AnnotationManager::GetAnnotation (int classID, const std::string& scriptClass)
{
	return GetAnnotationInternal (classID, scriptClass);
}

const std::vector<Annotation>& AnnotationManager::GetRecentlyChangedAnnotations ()
{
	return m_RecentlyChanged;
}

const Annotation* AnnotationManager::GetAnnotation (Object *obj)
{
	int classID = obj->GetClassID ();
	string scriptClass = GetScriptClass (obj);

	return GetAnnotationInternal (classID, scriptClass);
}

void AnnotationManager::AnnotationWasChanged (Annotation* annotation)
{
	m_GizmosSyncID = GizmoManager::Get().SetGizmosDirty ();
	SetDirty();	
	SetMostRecentChanged (annotation);
}

void AnnotationManager::SetGizmoEnabled (int classID, const string& scriptClass, bool gizmoEnabled)
{
	if (Annotation* annotation = GetAnnotationInternal (classID, scriptClass))
	{
		if (annotation->HasGizmo() && annotation->m_GizmoEnabled != gizmoEnabled)
		{
			annotation->m_GizmoEnabled = gizmoEnabled;
			if (m_UseInspectorExpandedState)
			{
				// Sync inspector expanded state and ensure inspector reflects changes (collapsed state)
				GetInspectorExpandedState().SetInspectorExpanded (classID, scriptClass, annotation->m_GizmoEnabled);
				ForceReloadInspector ();
			}
			AnnotationWasChanged (annotation);
		}
	}
	else
	{
		LogString("Warning: Annotation not found!");
	}
}

void AnnotationManager::SetIconEnabled (int classID, const string& scriptClass, bool iconEnabled)
{
	if (Annotation* annotation = GetAnnotationInternal (classID, scriptClass))
	{
		if (annotation->HasIcon() && annotation->m_IconEnabled != iconEnabled)
		{
			annotation->m_IconEnabled = iconEnabled;
			AnnotationWasChanged (annotation);
		}
	}
	else
	{
		LogString("Warning: Annotation not found!");
	}
}


bool Equals(Annotation* a, Annotation* b)
{
	return (a->m_ClassID == b->m_ClassID) && (a->m_ScriptClass == b->m_ScriptClass);
}

void AnnotationManager::SetMostRecentChanged (Annotation* a)
{
	// Still most recent?
	if (!m_RecentlyChanged.empty())
		if ( Equals(a, &m_RecentlyChanged.front ()) )
			return;
	
	// Remove from list
	for (std::vector<Annotation>::iterator it = m_RecentlyChanged.begin(); it != m_RecentlyChanged.end(); ++it)
		if ( Equals(a, &(*it) )) 
		{
			m_RecentlyChanged.erase (it);
			break;
		}

	// Insert in front
	m_RecentlyChanged.insert(m_RecentlyChanged.begin(), *a);
}

void AnnotationManager::Refresh()
{
	std::vector<Annotation> annotations;

	// Get builtin components
	vector<SInt32> classIDs;
	Object::FindAllDerivedClasses (Object::StringToClassID ("Component"), &classIDs, true);
	int monoBehaviorClassID = Object::StringToClassID ("MonoBehaviour");
	for (unsigned i=0; i<classIDs.size(); ++i)
	{
		int classID = classIDs[i];
		if (classID == monoBehaviorClassID)
			continue;

		int flags = 0;	
		if (GizmoManager::Get().HasGizmo (classID))
			flags |= Annotation::kHasGizmo;
		if (GizmoManager::Get().HasIcon (classID))
			flags |= Annotation::kHasIcon;
		
		if (flags != 0)
			annotations.push_back(Annotation(classID, "", flags, true, true));
	}


	// Get scripts that implements OnDrawGizmos
	MonoScriptManager::AllScripts temp = GetMonoScriptManager().GetAllRuntimeScripts ();
	vector<MonoScript*> allScripts (temp.begin(), temp.end());
	sort (allScripts.begin (), allScripts.end (), ScriptSort);
	for (unsigned i=0; i<allScripts.size(); ++i)
	{
		MonoScript* script = allScripts[i];
		int flags = 0;
		
		if ( script->FindMethod("OnDrawGizmos"))
			flags |= Annotation::kHasGizmo;	

		if (script->FindMethod("OnDrawGizmosSelected"))
			flags |= Annotation::kHasGizmo;	
		
		if (script->GetIcon ())
			flags |= Annotation::kHasIcon;

		if (flags != 0)
		{
			// Note: We use the classID for MonoBehavior instead of MonoScript 
			// to ensure we use same classID as InspectorExpandedState
			annotations.push_back(Annotation(monoBehaviorClassID, script->GetScriptClassName(), flags, true, true));
		}
	}

	// Keep enabled state from previous state
	for (unsigned i=0; i<annotations.size(); ++i)
	{
		Annotation& newAnno = annotations[i];
		if (const Annotation* a = GetAnnotationInternal (newAnno.m_ClassID, newAnno.m_ScriptClass))
		{
			newAnno.m_GizmoEnabled = a->m_GizmoEnabled;
			newAnno.m_IconEnabled = a->m_IconEnabled;
		}

		if (m_UseInspectorExpandedState)
			newAnno.m_GizmoEnabled = GetInspectorExpandedState().IsInspectorExpanded (newAnno.m_ClassID, newAnno.m_ScriptClass);
	}

	// Set current state
	m_CurrentPreset.m_AnnotationList.swap (annotations);

	if (s_Debug)
		PrintCurrentSetup();
}

void AnnotationManager::PrintCurrentSetup ()
{
	printf_console ("Current AnnotationManager state:\n");
	for (unsigned i=0; i<m_CurrentPreset.m_AnnotationList.size(); ++i)
	{
		Annotation& a = m_CurrentPreset.m_AnnotationList[i];
		if (a.m_ScriptClass.empty ())
		{
			printf_console ("    %s: icon %d, gizmo %d\n",  Object::ClassIDToString (a.m_ClassID).c_str(), a.m_IconEnabled?1:0, a.m_GizmoEnabled?1:0 );
		}
		else
		{
			printf_console ("    %s: icon %d, gizmo %d\n", a.m_ScriptClass.c_str(), a.m_IconEnabled?1:0, a.m_GizmoEnabled?1:0);
		}
	}
}

void AnnotationManager::InspectorExpandedStateWasChanged (int classID, const std::string& scriptClass, bool expanded)
{
	// Rebuild our gizmo setups
	if (m_UseInspectorExpandedState)
	{
		if (Annotation* a = GetAnnotationInternal (classID, scriptClass))
		{
			a->m_GizmoEnabled = expanded;
		}
		else
		{
			// If the annotation was not found we need to refresh state (also syncs to the expanded state)
			Refresh ();
		}

		m_GizmosSyncID = GizmoManager::Get ().SetGizmosDirty ();
	}
}

void AnnotationManager::SetIconForObject (Object* obj, const Texture2D* icon)
{
	// Set icon for GameObject or MonoScript
	if (Unity::GameObject* go = dynamic_pptr_cast<GameObject*> (obj))
	{
		go->SetIcon (icon);
	}
	else
	{
		// Check if obj is a script asset itself
		MonoScript* script = dynamic_pptr_cast<MonoScript*> (obj);
		if (!script)
		{
			// Get script asset from monobehavior if any
			if (MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (obj))
				script = behaviour->GetScript ();
		}
		if (script)
			script->SetIcon(icon);	
	}
}


Texture2D* AnnotationManager::GetIconForObject (Object* obj) const
{
	// Get icon for GameObject or MonoScript
	if (Unity::GameObject* go = dynamic_pptr_cast<GameObject*> (obj))
	{
		return go->GetIcon ();
	}
	else
	{
		// Check if obj is a script asset itself
		MonoScript* script = dynamic_pptr_cast<MonoScript*> (obj);
		if (!script)
		{
			// Get script asset from monobehavior if any
			if (MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (obj))
				script = behaviour->GetScript ();
		}
		if (script)
			return dynamic_pptr_cast<Texture2D*> (script->GetIcon ());
	}
	return NULL;
}

void AnnotationManager::SetIconSize (float iconSize)
{
	if (m_IconSize != iconSize)
	{
		m_IconSize = iconSize;
		SetDirty();
	}
}

float AnnotationManager::GetIconSize ()
{
	return m_IconSize;
}

void AnnotationManager::Set3dGizmosEnabled (bool enable)
{
	if (m_Use3dGizmos != enable)
	{
		m_Use3dGizmos = enable;
		SetDirty();
	}
}

bool AnnotationManager::Is3dGizmosEnabled() const
{
	return m_Use3dGizmos;
}

void AnnotationManager::SetShowGrid (bool enable)
{
	if (m_ShowGrid != enable)
	{
		m_ShowGrid = enable;
		SetDirty();
	}
}

bool AnnotationManager::GetShowGrid() const
{
	return m_ShowGrid;
}


template<class T>
void Annotation::Transfer (T& transfer)
{
	TRANSFER(m_IconEnabled);
	transfer.Align();
	TRANSFER(m_GizmoEnabled);
	transfer.Align();
	TRANSFER(m_ClassID);
	TRANSFER(m_ScriptClass);
	TRANSFER(m_Flags);
}

template<class T>
void AnnotationManager::Transfer (T& transfer)
{
	Super::Transfer(transfer);
	transfer.Transfer(m_CurrentPreset.m_AnnotationList, "m_CurrentPreset_m_AnnotationList");
	TRANSFER(m_RecentlyChanged);
	transfer.Transfer(m_IconSize, "m_WorldIconSize");
	TRANSFER(m_Use3dGizmos);
	transfer.Align();
	TRANSFER(m_ShowGrid);
	transfer.Align();
}


// PRESETS


std::vector<std::string> AnnotationManager::GetPresetList ()
{
	std::vector<std::string> presetNames;
	for (unsigned i=0; i<m_Presets.size (); ++i)
	{
		presetNames.push_back (m_Presets[i].m_Name);
	}
	return presetNames;
}

void AnnotationManager::SavePreset (const std::string& presetName)
{
	// Name current preset (before saving it)
	m_CurrentPreset.m_Name = presetName;

	// Save current preset
	int presetIndex = FindPresetIndex (presetName);
	if (presetIndex >= 0)
		m_Presets [presetIndex] = m_CurrentPreset;
	else
		m_Presets.push_back (m_CurrentPreset);

	SetDirty();
}

void AnnotationManager::LoadPreset (const std::string& presetName)
{
	int presetIndex = FindPresetIndex (presetName);
	AssertIf (presetIndex == -1);
	if (presetIndex >= 0)
	{
		m_CurrentPreset = m_Presets[presetIndex];
		if (m_UseInspectorExpandedState)
		{
			for (unsigned i=0;i<m_CurrentPreset.m_AnnotationList.size(); ++i)
			{
				Annotation& a = m_CurrentPreset.m_AnnotationList[i];
				GetInspectorExpandedState().SetInspectorExpanded(a.m_ClassID, a.m_ScriptClass, a.m_GizmoEnabled);
			}
			ForceReloadInspector ();
		}
	}
}

void AnnotationManager::DeletePreset (const std::string& presetName)
{
	int presetIndex = FindPresetIndex (presetName);
	if (presetIndex >= 0)
		m_Presets.erase (m_Presets.begin()+presetIndex);

}

void AnnotationManager::ResetPresetsToFactorySettings ()
{
	Assert(false);
}

int AnnotationManager::FindPresetIndex (const std::string& presetName)
{
	for (unsigned i=0; i<m_Presets.size (); ++i)
		if (m_Presets[i].m_Name == presetName)
			return i;

	return -1;
}
