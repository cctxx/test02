#include "UnityPrefix.h"
#include "ActiveEditorTracker.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/Selection.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Runtime/Camera/Projector.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "CreateEditor.h"

using namespace std;

std::vector<ActiveEditorTracker*> ActiveEditorTracker::s_AllActiveEditorTrackers;


ActiveEditorTracker::Element::~Element()
{
	if (m_Inspector != NULL)
	{
		DestroyObjectHighLevel (m_Inspector);
		m_Inspector = NULL;
	}
}

// static 
std::vector<Object*> ActiveEditorTracker::GetLockedObjects ()
{
	std::vector<Object*> result;
	for (unsigned i=0; i<s_AllActiveEditorTrackers.size(); ++i)
	{
		if (s_AllActiveEditorTrackers[i]->IsLocked())
		{
			Object* obj = s_AllActiveEditorTrackers[i]->m_LockedObject;
			result.push_back(obj);
		}
	}
	return result;
}

ActiveEditorTracker::ActiveEditorTracker ()
{
	GetSceneTracker().AddSceneInspector(this);
	s_AllActiveEditorTrackers.push_back (this);
}

ActiveEditorTracker::~ActiveEditorTracker ()
{
	GetSceneTracker().RemoveSceneInspector(this);
	std::vector<ActiveEditorTracker*>::iterator found = find(s_AllActiveEditorTrackers.begin(), s_AllActiveEditorTrackers.end(), this);
	if (found != s_AllActiveEditorTrackers.end())
		s_AllActiveEditorTrackers.erase(found);
}

void ActiveEditorTracker::DidFlushDirty ()
{
	std::set<PPtr<Object> > current;
	if (m_LockedObject.GetInstanceID() != 0)
	{
		if (m_LockedObject.IsNull())
			m_LockedObject = NULL;
		else
			current.insert (m_LockedObject);
	}

	if (current.empty())
		current = Selection::GetSelectionPPtr();
	
	vector<Element> elements;
	BuildSubObjectsVector (current, elements);

	if (m_Elements != elements)
	{
		Rebuild();
		return;
	}
}

void ActiveEditorTracker::VerifyModifiedMonoBehaviours ()
{
	for (int i=0;i<m_Elements.size();i++)
	{
		if (m_Elements[i].m_IsVisible == 0)
			continue;
		
		if (m_Elements[i].m_RequireStateComparision && m_Elements[i].m_Inspector)
		{
			// Build new state array so we can compare agains the last used state
			int flags = 0;
			if (m_InspectorMode >= kDebugInspector)
				flags |= kSerializeDebugProperties;
			
			dynamic_array<UInt8> newState(kMemTempAlloc);
			for (std::vector<PPtr<Object> >::iterator j = m_Elements[i].m_Objects.begin(); j != m_Elements[i].m_Objects.end(); j++)
			{
				if (j->IsValid ())
				{
					dynamic_array<UInt8> objState(kMemTempAlloc);
					WriteObjectToVector(**j, &objState, flags);
					newState.insert (newState.end(), objState.begin(), objState.end());
				}
			}
			
			if ( !newState.equals(m_Elements[i].m_LastState))
			{
				SetCustomEditorIsDirty(m_Elements[i].m_Inspector, true);
				m_Elements[i].m_LastState = newState;
			}
		}
	}
}


void ActiveEditorTracker::SetIsLocked (bool locked)
{
	if (locked)
		m_LockedObject = Selection::GetActive();
	else
		m_LockedObject = NULL;
}

void ActiveEditorTracker::ObjectHasChanged (PPtr<Object> object)
{
	for (Elements::iterator i=m_Elements.begin();i != m_Elements.end();i++)
	{
		for (std::vector<PPtr<Object> >::iterator j = i->m_Objects.begin(); j != i->m_Objects.end(); j++)
		{
			if (object == *j)
				SetCustomEditorIsDirty(i->m_Inspector, true);
		}
	}
}


void ActiveEditorTracker::ForceReloadInspector (bool fullRebuild)
{
	if (fullRebuild)
	{
		Rebuild();
	}
	else
	{
		m_CachedAssetImporter.clear();
		for (Elements::iterator i=m_Elements.begin();i != m_Elements.end();i++)
		{
			if (i->m_Inspector)
			{
				SetCustomEditorIsDirty(i->m_Inspector, true);
				i->m_Inspector->CallMethodInactive("OnForceReloadInspector");
			}
		}
	}
}

bool ActiveEditorTracker::IsDirty ()
{
	for (Elements::iterator i=m_Elements.begin();i != m_Elements.end();i++)
	{
		if (i->m_Inspector)
		{
			if (i->m_IsVisible != 0 && IsCustomEditorDirty(i->m_Inspector))
				return true;
		}
	}
	return m_IsDirty;
}

void ActiveEditorTracker::SetInspectorMode (InspectorMode mode)
{
	if (m_InspectorMode == mode)
		return;
	
	m_InspectorMode = mode;
	Rebuild();
}

std::vector<MonoBehaviour*> ActiveEditorTracker::GetEditors ()
{
	////@TODO we should automatically trigger rebuilds just in time for OnGUI if something doesn't match up!
	std::vector<MonoBehaviour*> editors;
	editors.reserve(m_Elements.size());
	for (int i=0;i<m_Elements.size();i++)
	{
		if (m_Elements[i].m_Inspector)
		{
			bool valid = true;
			for (std::vector<PPtr<Object> >::iterator j = m_Elements[i].m_Objects.begin(); j != m_Elements[i].m_Objects.end(); j++)
			{
				if (!j->IsValid())
				{
					valid = false;
					break;
				}
			}
			if (valid)
				editors.push_back(m_Elements[i].m_Inspector);
		}
	}
	return editors;
}

void ActiveEditorTracker::Rebuild ()
{
	m_CachedAssetImporter.clear ();
	m_IsDirty = true;
	m_HasComponentsWhichCannotBeMultiEdited = false;

	m_Elements.clear();
	
	std::set<PPtr<Object> > current;
	if (m_LockedObject.GetInstanceID() != 0)
		current.insert (m_LockedObject);
	else
		current = Selection::GetSelectionPPtr();
	
	BuildSubObjectsVector (current, m_Elements);
	
	for (int i=0;i<m_Elements.size();i++)
	{
		std::vector <PPtr<Object> > &objs = m_Elements[i].m_Objects;
		m_Elements[i].m_Inspector = CreateInspectorMonoBehaviour (objs, NULL, m_InspectorMode, m_Elements[i].m_IsInspectorHidden);

		if (m_Elements[i].m_Inspector == NULL)
			m_HasComponentsWhichCannotBeMultiEdited = true;
		m_Elements[i].m_RequireStateComparision = false;

		if (objs.size())
		{
			MonoBehaviour* target = dynamic_pptr_cast<MonoBehaviour*> (objs[0]);
			m_Elements[i].m_RequireStateComparision = m_InspectorMode >= kDebugInspector && objs[0]->HasDebugmodeAutoRefreshInspector();
			m_Elements[i].m_RequireStateComparision |= target != NULL;
		}
	}
}

/// Return true if the two components will have the same default map key.
static bool ComponentsHaveCollidingDefaultKeys (Unity::Component* com1, Unity::Component* com2)
{
	int classID1 = com1->GetClassID();
	int classID2 = com2->GetClassID();

	// Components of different classes never collide.
	if (classID1 != classID2)
		return false;

	// MonoBehaviors with missing (NULL) scripts or with the
	// same script will collide.
	if (classID1 == ClassID (MonoBehaviour))
	{
		MonoBehaviour* mb1 = (MonoBehaviour*) com1;
		MonoBehaviour* mb2 = (MonoBehaviour*) com2;

		MonoScript* script1 = mb1->GetScript ();
		MonoScript* script2 = mb2->GetScript ();

		return (script1 == script2);
	}

	Assert (!com1->GetNeedsPerObjectTypeTree ());
	Assert (!com2->GetNeedsPerObjectTypeTree ());

	return true;
}

// Returns a string that is based on the component/MonoBehaviour type
// and the index of the occurance of that element on the GameObject.
std::string GetElementMapKey (Unity::Component* com)
{
	GameObject *go = com->GetGameObjectPtr();
	Assert (go != NULL);
	
	// Default element key to name of component's class.
	std::string key (com->GetClassName ());

	// If it's a MonoBehavior, though, that has a valid script,
	// use a key based on the script's full class name.
	if (com->GetClassID() == ClassID (MonoBehaviour))
	{
		MonoBehaviour* mb = (MonoBehaviour*)com;
		MonoScript* script = mb->GetScript();
		if (script != NULL)
		{
			// Add the instance id as well, so we can handle different scripts with the same name
			// e.g. a C# script and JavaScript with the same class name.
			key = Format ("%s:%i:", script->GetScriptFullClassName ().c_str (), script->GetInstanceID ());
		}
		else
		{
			// The script is missing.  We want to make sure that two such components
			// that are at the same *index* in two *different* game objects don't
			// get the same key so we simply tag the script instance ID onto the key.
			key += Format ("%i", mb->GetScript ().GetInstanceID ());
		}
	}
	
	// Tag an index onto the key so we can differentiate multiple components
	// of the same type.
	int componentTypeIndex = 0;
	for (int i = 0; i < go->GetComponentCount(); i++)
	{
		Unity::Component* comAtIndex = &go->GetComponentAtIndex (i);
		if (comAtIndex == com)
		{
			key += IntToString(componentTypeIndex);
			break;
		}
		else if (ComponentsHaveCollidingDefaultKeys (comAtIndex, com))
			++ componentTypeIndex;
	}
	
	return key;
}

// Takes the selected objects and an array (initially empty) of editors to be filled in.
void ActiveEditorTracker::BuildSubObjectsVector (std::set<PPtr<Object> > &_objs, vector<ActiveEditorTracker::Element>& components)
{
	components.clear();
	
	// Get valid objects.
	std::set<PPtr<Object> > objs;
	for (std::set<PPtr<Object> >::iterator objsIterator = _objs.begin(); objsIterator != _objs.end(); objsIterator++)
	{
		if (objsIterator->IsValid())
			objs.insert (*objsIterator);
	}	
	
	if (objs.empty())
		return;
	
	// Class id of first component in the first object in the selection.
	// This is used for checking that it's the same for all the objects in the selection.
	int classID = (**objs.begin()).GetClassID();
	
	// Loop over objects and see if we have any components that can't be shown in the inspector
	for (std::set<PPtr<Object> >::iterator objsIterator = objs.begin(); objsIterator != objs.end(); objsIterator++)
	{
		// If not all selected assets are of the same basic type, don't create any editors.
		// @TODO: We need to still create hidden editors for use in OnSceneGUI.
		if ((**objsIterator).GetClassID() != classID)
		{
			m_HasComponentsWhichCannotBeMultiEdited = true;
			return;
		}
		if (classID == ClassID(MonoBehaviour))
		{
			MonoBehaviour* mb1 = dynamic_pptr_cast<MonoBehaviour*>(*objs.begin());
			MonoBehaviour* mb2 = dynamic_pptr_cast<MonoBehaviour*>(*objsIterator);
			if (mb1->GetScript() != mb2->GetScript())
			{
				m_HasComponentsWhichCannotBeMultiEdited = true;
				return;
			}				
		}
	}
	
	/// Add the object itself
	{
		std::vector<PPtr<Object> > element;
		for (std::set<PPtr<Object> >::iterator objsIterator = objs.begin(); objsIterator != objs.end(); objsIterator++)
			element.push_back (*objsIterator);		
		components.push_back (element);
	}

	/// Add the components (excluding filters) of the GameObjects
	typedef std::vector<std::pair<std::string, std::vector<PPtr<Object> > > > ElementMap; // use vector of pairs instead of map because we don't want sorting
	ElementMap elementMap;
	for (std::set<PPtr<Object> >::iterator objsIterator = objs.begin (); objsIterator != objs.end (); objsIterator++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*objsIterator);
		// There are no components if it's not a GameObject so just continue then
		if (!go)
			continue;
		
		// Loop through components
		for (int i = 0; i < go->GetComponentCount (); i++)
		{
			Unity::Component* com = &go->GetComponentAtIndex (i);
			
			if (com->TestHideFlag (Object::kHideInspector))
				continue;

			std::string key (GetElementMapKey (com));
			bool foundKey = false;
			for (ElementMap::iterator mapIterator = elementMap.begin (); mapIterator != elementMap.end (); mapIterator++)
			{
				// If key already exists, just push another target the corresponding editor
				if (mapIterator->first == key)
				{
					mapIterator->second.push_back (com);
					foundKey = true;
					break;
				}
			}
			
			// If key was not found, create the key and corresponding editor
			if (!foundKey)
			{
				std::vector<PPtr<Object> > element (1, com);
				elementMap.push_back (make_pair (key, element));
			}
		}
	}
	
	// Add the elements we found to the list of components
	for (ElementMap::iterator mapIterator = elementMap.begin (); mapIterator != elementMap.end (); mapIterator++)
	{
		Element element (mapIterator->second);
		
		// If this element doesn't have all the targets, we don't show it in the inspector,
		// but we still add it for use in OnSceneGUI
		if (element.m_Objects.size () != objs.size ())
		{
			m_HasComponentsWhichCannotBeMultiEdited = true;
			element.m_IsInspectorHidden = true;
		}
		components.push_back (element);
	}
	
	// Get the materials used in the components
	std::set<PPtr<Object> > materials;
	std::set<PPtr<Object> > materials2;
	bool first = true;
	for (std::set<PPtr<Object> >::iterator objsIterator = objs.begin(); objsIterator != objs.end(); objsIterator++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*objsIterator);
		if (!go)
			continue;
		
		// For the first GameObject, add all materials encountered.
		// For every subsequent GameObject, start a new list from scratch and only
		// add materials that we also encountered in all the previous GameObjects.
		// The wost case complexity of this approach is: nr_gos * nr_mats * log nr_mats
		// (An alternative approach would be to add all materials and count at the end
		// if nr of mats equals nr of gos. However, to avoid the same mat being added
		// more than once for the same go, we'd have to only add it if was not already
		// added for the same iteration, and this would lead to the same complexity.)
		materials2.clear ();
		
		if (go->CountDerivedComponents (ClassID (Renderer)) == 1) 
		{
			Renderer& renderer = go->GetComponent (Renderer);

#if ENABLE_SPRITES && 0 //TODO(2D):New Material workflow might not need this.
			// Special case for SpriteRenderer: only show the OverrideMaterial.
			if (renderer.GetRendererType() == kRendererSprite)
			{
				SpriteRenderer& sr = (SpriteRenderer&)renderer;
				Material* material = sr.GetOverrideMaterial();
				if (material && !material->TestHideFlag(Object::kHideInspector))
				{
					// Add if first GameObject or if encountered in all previous GameObjects.
					if (first || materials.find (material) != materials.end ())
						materials2.insert (material);
				}
			}
			else
#endif
			{
				int materialCount = renderer.GetMaterialCount ();
				for (int i=0;i<materialCount;i++)
				{
					EditorExtension* material = PPtr<EditorExtension> (renderer.GetMaterial (i).GetInstanceID ());
					if (material && !material->TestHideFlag(Object::kHideInspector))
					{
						// Add if first GameObject or if encountered in all previous GameObjects.
						if (first || materials.find (material) != materials.end ())
							materials2.insert (material);
					}
				}
			}
		}
		if (go->CountDerivedComponents (ClassID (Projector)) == 1) 
		{
			Projector& projector = go->GetComponent (Projector); 
			EditorExtension* material = PPtr<EditorExtension> (projector.GetMaterial ().GetInstanceID ());
			if (material && !material->TestHideFlag(Object::kHideInspector))
			{
				// Add if first GameObject or if encountered in all previous GameObjects.
				if (first || materials.find (material) != materials.end ())
					materials2.insert (material);
			}
		}
		materials = materials2;
		first = false;
	}
	// Add the found materials
	for (std::set<PPtr<Object> >::iterator material = materials.begin(); material != materials.end(); material++)
	{
		std::vector<PPtr<Object> > element;
		element.push_back (*material);
		components.push_back (element);			
	}
	
	// Stop here if not all the objects are persistent and are main assets
	for (std::set<PPtr<Object> >::iterator i = objs.begin(); i != objs.end(); i++)
	{
		if (!(**i).IsPersistent() || !IsMainAsset((**i).GetInstanceID()))
			return;
	}
	
	// Create an importer element
	std::vector<PPtr<Object> > element;
	int importerClassID = -1;
	// Loop through objects and push their importers to the importer element
	for (std::set<PPtr<Object> >::iterator i = objs.begin(); i != objs.end(); i++)
	{
		// Find the AssetImporter for the object
		
		// Cache asset importer because FindAssetImporterForObject will lock the main thread when we are doing async background loading
		std::map<int, PPtr<Object> >::iterator entry = m_CachedAssetImporter.find(i->GetInstanceID());
		
		PPtr<Object> targetImporter;
		if (entry != m_CachedAssetImporter.end())
			targetImporter = entry->second;
		else
		{
			// Find the AssetImporter the slow way
			AssetImporter* importer = FindAssetImporterForObject(i->GetInstanceID());
			if (importer)
			{
				targetImporter = importer;
				m_CachedAssetImporter[i->GetInstanceID()] = importer;
			}
		}
		
		if (targetImporter)
		{
			if (targetImporter->GetClassID() == importerClassID || importerClassID == -1)
			{
				importerClassID = targetImporter->GetClassID();
				element.push_back (targetImporter);
			}
			else
				break;
		}
	}
	// Only add if all objects have the importer
	if (element.size () == objs.size ())
		components.insert (components.begin(), element);
	else
		m_HasComponentsWhichCannotBeMultiEdited = true;
}

ActiveEditorTracker* ActiveEditorTracker::sSharedTracker = NULL;

void ActiveEditorTracker::InitializeSharedTracker()
{
	Assert(sSharedTracker == NULL);
	sSharedTracker = new ActiveEditorTracker ();
}

ActiveEditorTracker* GetSharedActiveEditorTracker ()
{	
	Assert(ActiveEditorTracker::sSharedTracker != NULL);
	return ActiveEditorTracker::sSharedTracker;
}
