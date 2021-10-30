#include "UnityPrefix.h"
#include "SceneInspector.h"
#include "SelectionHistory.h"
#include "Editor/Src/Undo/Undo.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include "EditorExtensionImpl.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/VersionControl/VCCache.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include <set>
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "Runtime/Mono/MonoManager.h"

using std::set;
using std::less;
SceneTracker *gSceneTracker = NULL;

typedef std::set<SInt32, less<SInt32>, memory_pool<SInt32> > CallbackContainer;

#define FOR_EACH_INSPECTOR \
	for (SceneInspectorIterator c=m_SceneInspectors.begin(); c != m_SceneInspectors.end(); c++) \
	{ \
		ISceneInspector* inspector = *c; \
		if (inspector == NULL) \
			continue;


ISceneInspector::~ISceneInspector ()
{
	DebugAssertIf(find(GetSceneTracker ().m_SceneInspectors.begin(), GetSceneTracker ().m_SceneInspectors.end(), this) != GetSceneTracker ().m_SceneInspectors.end());
}

SceneTracker& GetSceneTracker ()
{
	AssertIf(gSceneTracker == NULL);
	return *gSceneTracker;
}

void SceneTracker::Initialize ()
{
	if (gSceneTracker == NULL)
	{
		Object::RegisterDirtyCallback (SetObjectDirty);
		Object::RegisterDestroyedCallback (DestroyObjectCallback);
		GameObject::RegisterDestroyedCallback (DestroyGameObjectCallback);
		GameObject::RegisterSetGONameCallback (SetGameObjectNameCallback);
		Transform::RegisterHierarchyChangedCallback (TransformHierarchyChangedCallback);
		Transform::RegisterHierarchyChangedSetParentCallback (TransformHierarchyChangedSetParentCallback);
		

		gSceneTracker = new SceneTracker();
	}
}

SceneTracker::SceneTracker ()
{
	m_DontSendSelectionChanged = 0;
	m_OldActive = m_Active = m_OldActiveGameObject = m_ActiveGameObject = 0;
	AssertIf (gSceneTracker);
	m_LockedInspector = 0;
	m_DontSendSelectionChanged = false;
	m_DirtyCallbacks = new CallbackContainer (less<SInt32> (), memory_pool<SInt32>());
	m_TransformHierarchyHasChanged = false;
}

SceneTracker::~SceneTracker ()
{
	AssertIf (m_DontSendSelectionChanged != 0);
	AssertIf (!m_SceneInspectors.empty ());

	delete static_cast<CallbackContainer*> (m_DirtyCallbacks);
	gSceneTracker = NULL;
}

void SceneTracker::SetActive (Object* o)
{
	SetActiveID (o == NULL ? 0 : o->GetInstanceID ());
}

void SceneTracker::SetActiveID (int ID)
{
	if (m_DontSendSelectionChanged)
		return;

	SET_ALLOC_OWNER(NULL);
	PPtr<Object> anObject (ID);

	if (m_Active == anObject.GetInstanceID ()) 
		return;

	RegisterSelectionUndo ();

	m_Active = anObject.GetInstanceID ();
	m_ActiveGameObject = 0;	
	Object* obj = anObject;
	if (dynamic_pptr_cast<GameObject*> (obj))
		m_ActiveGameObject = anObject.GetInstanceID ();
	else if (dynamic_pptr_cast<Unity::Component*> (obj))
	{
		Unity::Component& component = *static_cast<Unity::Component*> (obj);
		if (component.GetGameObjectPtr ())
			m_ActiveGameObject = component.GetGameObject ().GetInstanceID ();
	}
	
	m_Selection.clear ();
	if (m_ActiveGameObject)
		m_Selection.insert (m_ActiveGameObject);
	else if (m_Active)
		m_Selection.insert (m_Active);
	
#if ENABLE_SELECTIONHISTORY
	RegisterSelectionChange ();
#endif
}

Object* SceneTracker::GetActive ()
{
	return dynamic_instanceID_cast<Object*> (m_Active);
}

int SceneTracker::GetActiveID ()
{
	return m_Active;
}

GameObject* SceneTracker::GetActiveGO ()
{
	return dynamic_instanceID_cast<GameObject*> (m_ActiveGameObject);
}

void SceneTracker::SetSelectionID (const set<int>& newSelection)
{
	if (m_DontSendSelectionChanged)
		return;

	const bool changed = (m_Selection != newSelection);
	if (changed)
	{
		RegisterSelectionUndo ();
	}
	
	m_Selection = newSelection;
	
	if (m_Selection.empty ())
		SetActiveID (0);
	else
	{
		// We want to mimic the scene view picking system:
		// 	If we have a new GO in the selection, make that the active GO.
		for (set<int>::const_iterator i = newSelection.begin(); i != newSelection.end(); i++)
		{
			// If we have a new GO that is not part of the seleciton, make it the active
			if (m_Selection.count (*i) == 0)
			{	
				SetActiveID(*i);
				break;
			}
		}
		// The Selection doesn't include the active gameobject
		// -> Activate one of the objects from the selection
		if (m_Selection.count (m_ActiveGameObject) == 0 && m_Selection.count (m_Active) == 0)
			SetActiveID (*m_Selection.begin ());
	}
	
	m_Selection = newSelection;

#if ENABLE_SELECTIONHISTORY	
	if (changed)
	{
		RegisterSelectionChange ();
	}
#endif
}

template<typename container>
void SceneTracker::SetSelection (const container& sel)
{
	if (m_DontSendSelectionChanged)
		return;

	set<int> newSelection;
	for (typename container::const_iterator i = sel.begin ();i != sel.end ();i++)
	{
		if (*i)
			newSelection.insert ((**i).GetInstanceID ());
	}

	SetSelectionID (newSelection);
}

template void SceneTracker::SetSelection < TempSelectionSet  > (const TempSelectionSet& sel);
template void SceneTracker::SetSelection < std::set<Object*> > (const std::set<Object*>& sel);

void SceneTracker::SetSelectionPPtr (const std::set<PPtr<Object> >& sel)
{
	if (m_DontSendSelectionChanged)
		return;
	
	set<int> newSelection;
	for (set<PPtr<Object> >::const_iterator i = sel.begin ();i != sel.end ();i++)
		newSelection.insert (i->GetInstanceID ());
	
	SetSelectionID (newSelection);
}

void SceneTracker::GetSelection ( TempSelectionSet& selection)
{
	selection.clear();
	for (set<int>::iterator i = m_Selection.begin ();i != m_Selection.end ();i++)
	{
		Object* o = PPtr<Object> (*i);
		if (o)
			selection.insert (o);
	}
}

set<int> SceneTracker::GetSelectionID ()
{
	return m_Selection;
}

set<PPtr<Object> > SceneTracker::GetSelectionPPtr ()
{
	set<PPtr<Object> > selection;
	for (set<int>::iterator i = m_Selection.begin ();i != m_Selection.end ();i++)
	{
		selection.insert (PPtr<Object> (*i));
	}
	return selection;
}

void SceneTracker::AddSceneInspector (ISceneInspector* inspector)
{
	Assert(Thread::CurrentThreadIsMainThread());
	
	if (find(m_SceneInspectors.begin(), m_SceneInspectors.end(), inspector) == m_SceneInspectors.end())
	{
		m_SceneInspectors.push_back(inspector);
	}

	if (inspector->HasObjectChangedCallback())
	{
		if (find(m_SceneInspectorsObjectHasChanged.begin(), m_SceneInspectorsObjectHasChanged.end(), inspector) == m_SceneInspectorsObjectHasChanged.end())
			m_SceneInspectorsObjectHasChanged.push_back(inspector);
	}
}

void SceneTracker::RemoveSceneInspector (ISceneInspector* inspector)
{
	Assert(Thread::CurrentThreadIsMainThread());
	
	std::list<ISceneInspector*>::iterator found;
	found = find(m_SceneInspectors.begin(), m_SceneInspectors.end(), inspector);
	if (found != m_SceneInspectors.end())
		*found = NULL;

	found = find(m_SceneInspectorsObjectHasChanged.begin(), m_SceneInspectorsObjectHasChanged.end(), inspector);
	if (found != m_SceneInspectorsObjectHasChanged.end())
		*found = NULL;
}

void SceneTracker::DelayedDeleteSceneInspector (ISceneInspector* inspector)
{
	m_DelayDeleteMutex.Lock();
	m_DelayDeletedSceneInspectors.push_back(inspector);
	m_DelayDeleteMutex.Unlock();
}

void SceneTracker::NotifyGOHasChanged (PPtr<GameObject> go)
{
	m_LockedInspector++;
	
	FOR_EACH_INSPECTOR
		inspector->GOHasChanged (go);
	}

	m_LockedInspector--;
}

void SceneTracker::NotifyGOWasDestroyed (GameObject* go)
{
	m_LockedInspector++;
	
	FOR_EACH_INSPECTOR
		inspector->GOWasDestroyed(go);
	}
	
	if (!go->TestHideFlag(Object::kHideInHierarchy))
	{
		m_TransformHierarchyHasChanged = true;
	}

	if (IsWorldPlaying ())
	{
		m_LockedInspector--;
		return;
	}	
	
	m_LockedInspector--;
}

void SceneTracker::TickInspector ()
{
	FOR_EACH_INSPECTOR
		inspector->TickInspector();
	}
}

void SceneTracker::TickInspectorBackground ()
{
	FOR_EACH_INSPECTOR
		inspector->TickInspectorBackground();
	}
}

void SceneTracker::Update ()
{
	FOR_EACH_INSPECTOR
		inspector->Update();
	}
	CallStaticMonoMethod("EditorApplication", "Internal_CallUpdateFunctions");
}

void SceneTracker::ForceReloadInspector (bool fullRebuild)
{
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		inspector->ForceReloadInspector(fullRebuild);
	}
	m_LockedInspector--;
}

bool SceneTracker::CanOpenScene ()
{
	bool canOpenScene = true;
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		if (!inspector->CanOpenScene())
		{
			canOpenScene = false;
			break;
		}
	}
	m_LockedInspector--;
	return canOpenScene;
}

void SceneTracker::DidOpenScene ()
{
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		inspector->DidOpenScene();
	}
	m_LockedInspector--;
}

bool SceneTracker::CanEnterPlaymode ()
{
	bool canEnterPlayMode = true;
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		if (!inspector->CanEnterPlaymode())
		{
			canEnterPlayMode = false;
			break;
		}
	}
	m_LockedInspector--;
	return canEnterPlayMode;
}

void SceneTracker::DidPlayScene ()
{
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		inspector->DidPlayScene();
	}
	m_LockedInspector--;
}

bool SceneTracker::CanTerminate()
{
	bool canTerminate = true;
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		if (!inspector->CanTerminate())
		{
			canTerminate = false;
			break;
		}
	}
	m_LockedInspector--;
	return canTerminate;
}

void SceneTracker::ProjectWindowHasChanged ()
{
	m_LockedInspector++;
	FOR_EACH_INSPECTOR
		inspector->ProjectWindowHasChanged();
	}
	CallStaticMonoMethod("EditorApplication", "Internal_CallProjectWindowHasChanged");
	m_LockedInspector--;
}

bool SceneTracker::IsLocked()
{
	return m_LockedInspector != 0;
}

void SceneTracker::NotifyObjectWasDestroyed (int object)
{
	m_LockedInspector++;

	// Pass ObjectWasDestroyed along to sceneinspectors
	FOR_EACH_INSPECTOR
		inspector->ObjectWasDestroyed (PPtr<Object> (object));
	}

	// Remove from selection
	if (m_Selection.count (object) == 1)
	{
		m_Selection.erase (object);
	}
	
	if (object == m_Active || object == m_ActiveGameObject)
	{
		m_Active = 0;
		m_ActiveGameObject = 0;
	}
	
	m_LockedInspector--;
}

void SceneTracker::ClearUnloadedDirtyCallbacks ()
{
	CallbackContainer& callbacks = *reinterpret_cast<CallbackContainer*> (m_DirtyCallbacks);
	
	CallbackContainer::iterator next;
	for (CallbackContainer::iterator i=callbacks.begin ();i != callbacks.end ();i=next)
	{
		next = i;
		next++;
		
		Object* ptr = Object::IDToPointer(*i);
		if (ptr == NULL)
			callbacks.erase(i);
	}
}

void SceneTracker::FlushDirty ()
{	
	AssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
//	AssertIf(m_LockedInspector != 0);
	if (m_LockedInspector != 0)
		return;
	
	SET_ALLOC_OWNER(NULL);
	m_LockedInspector = 1;

	// Since managed code is holding some scene inspectors, we need some solution to delete scene inspectors from a different thread.
	// I want to avoid adding mutex to all scene inspector update calls, thus we just delay destruction until the next flush dirty.
	m_DelayDeleteMutex.Lock();
	std::list<ISceneInspector*>::iterator next;
	for (std::list<ISceneInspector*>::iterator i=m_DelayDeletedSceneInspectors.begin();i != m_DelayDeletedSceneInspectors.end();i=next)
	{
		next = i;
		next++;
		RemoveSceneInspector(*i);
		delete *i;
		m_DelayDeletedSceneInspectors.erase(i);
	}
	m_DelayDeleteMutex.Unlock();


	CallbackContainer& callbacks = *reinterpret_cast<CallbackContainer*> (m_DirtyCallbacks);
	CallbackContainer callbackDuplicate (less<SInt32> (), callbacks.get_allocator());
	callbackDuplicate.swap (callbacks);
	ISceneInspector* inspector = NULL;
	
	
	// Early out if there is no work to be done.
	if (callbackDuplicate.empty() && m_Active == m_OldActive &&
	    m_OldActiveGameObject == m_ActiveGameObject && m_OldSelection == m_Selection && !m_TransformHierarchyHasChanged)
	{	
		m_LockedInspector = 0;
		return;
	}
	
	set<Prefab*> alreadyMergedTemplates;
	if (!IsWorldPlaying ())
	{
		// If anything in the parent prefab has changed, merge all instances
		for (CallbackContainer::iterator i=callbackDuplicate.begin ();i != callbackDuplicate.end ();i++)
		{
			Object* ptr = Object::IDToPointer(*i);
			if (ptr == NULL)
				continue;
			
			EditorExtension* object = dynamic_pptr_cast<EditorExtension*> (ptr);
			if (object && object->IsPersistentDirty () && object->IsPrefabParent () && alreadyMergedTemplates.count (object->m_Prefab) == 0)
			{
				alreadyMergedTemplates.insert (object->m_Prefab);
				MergeAllPrefabInstances (object->m_Prefab);
			}
		}

		// If anything in the instance has changed, record all prefab changes in the instance
		for (CallbackContainer::iterator i=callbackDuplicate.begin ();i != callbackDuplicate.end ();i++)
		{
			Object* ptr = Object::IDToPointer (*i);
			if (ptr == NULL)
				continue;
			
			EditorExtension* object = dynamic_pptr_cast<EditorExtension*> (ptr);
			if (object && object->IsPersistentDirty ())
				RecordPrefabInstancePropertyModificationsAndValidate (*object);
		}
	}
	
	callbackDuplicate.insert (callbacks.begin (), callbacks.end ());
	callbacks.clear ();

	for (CallbackContainer::iterator i=callbackDuplicate.begin ();i != callbackDuplicate.end ();i++)
	{
		Object* ptr = Object::IDToPointer (*i);
		if (ptr == NULL)
			continue;

		// Update GameObjects if it is one
		if (ptr->GetClassID () == ClassID (GameObject))
		{
			GameObject* go = static_cast<GameObject*> (ptr);
			Transform* transform = go->QueryComponent(Transform);
			if (transform)
				TransformHierarchyChanged (transform);
			NotifyGOHasChanged (go);
		}
		
		// Call object that have changed
		SceneInspectorIterator calbackBegin = m_SceneInspectorsObjectHasChanged.begin();
		for (;calbackBegin != m_SceneInspectorsObjectHasChanged.end(); calbackBegin++)
		{
			inspector = *calbackBegin;
			if (inspector)
				inspector->ObjectHasChanged (PPtr<Object> (*i));
		}
	}
		
	// We call DidFlushDirty before changing selection because the 
	// tableviews that might update delayed will want to have their new objects added before changing selection
	for (SceneInspectorIterator c= m_SceneInspectors.begin();c != m_SceneInspectors.end(); c++)
	{
		inspector = *c;
		if (inspector)
			inspector->DidFlushDirty();
	}
	
	// Update active
	if (m_Active != m_OldActive)
	{
		// Some SceneInspectors remove/add themselves inside ActiveGOHasChanged
		// Prevent recursive calling of activate function from tableViewSelectionDidChange
		m_DontSendSelectionChanged++;

		// Call SceneInspector ActiveObjectHasChanged
		for (SceneInspectorIterator c= m_SceneInspectors.begin();c != m_SceneInspectors.end(); c++)
		{
			inspector = *c;
			if (inspector)
				inspector->ActiveObjectHasChanged (PPtr<Object> (m_Active));
		}
		m_DontSendSelectionChanged--;
		m_OldActive = m_Active;
	}
	
	// Update active gameobject
	if (m_OldActiveGameObject != m_ActiveGameObject)
	{
		// Prevent recursive calling of activate function from tableViewSelectionDidChange
		m_DontSendSelectionChanged++;
		// Call SceneInspector ActiveGOHasChanged
		for (SceneInspectorIterator c= m_SceneInspectors.begin();c != m_SceneInspectors.end(); c++)
		{
			inspector = *c;
			if (inspector)
				inspector->ActiveGOHasChanged (PPtr<GameObject> (m_ActiveGameObject));
		}
		m_DontSendSelectionChanged--;
		m_OldActiveGameObject = m_ActiveGameObject;
	}

	// Update selection
	if (m_OldSelection != m_Selection)
	{
		m_DontSendSelectionChanged++;
		
		for (SceneInspectorIterator c= m_SceneInspectors.begin();c != m_SceneInspectors.end(); c++)
		{
			inspector = *c;
			if (inspector)
			{
				set<int> selection = m_Selection;
				inspector->SelectionHasChanged (selection);
			}
		}
		m_DontSendSelectionChanged--;
		m_OldSelection = m_Selection;
	}

		
	callbackDuplicate.clear();

	m_SceneInspectors.remove (NULL);
	m_SceneInspectorsObjectHasChanged.remove (NULL);

	m_LockedInspector = 0;
}

void SceneTracker::TickHierarchyWindowHasChanged ()
{
	if (m_TransformHierarchyHasChanged)
	{
		m_TransformHierarchyHasChanged = false;
		for (SceneInspectorIterator c= m_SceneInspectors.begin();c != m_SceneInspectors.end(); c++)
		{
			ISceneInspector* inspector = *c;
			if (inspector)
				inspector->HierarchyWindowHasChanged();
		}
		CallStaticMonoMethod("EditorApplication", "Internal_CallHierarchyWindowHasChanged");
	}
}

void SceneTracker::DestroyGameObjectCallback (GameObject* go)
{
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
	gSceneTracker->NotifyGOWasDestroyed(go);
}

void SceneTracker::DestroyObjectCallback (int instanceID)
{
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
	gSceneTracker->NotifyObjectWasDestroyed(instanceID);
}

void SceneTracker::SetGameObjectNameCallback (GameObject* go)
{
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
	if (!go->TestHideFlag (Object::kHideInHierarchy))
		gSceneTracker->m_TransformHierarchyHasChanged = true;
}

// Will be called whenever an object is calling SetDirty ();
void SceneTracker::SetObjectDirty (Object* ptr)
{
	SET_ALLOC_OWNER(NULL);
	AssertIf (ptr == NULL);
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
	CallbackContainer& callbacks = *reinterpret_cast<CallbackContainer*> (gSceneTracker->m_DirtyCallbacks);

	if ((ptr->GetHideFlags() & Object::kDontSave) == 0)
	{
		callbacks.insert (ptr->GetInstanceID ());
	}
}

#if UNITY_EDITOR
Transform::VisibleRootMap& SceneTracker::GetVisibleRootTransforms()
{
	return m_VisibleRootTransforms;
}
#endif

void SceneTracker::ReloadTransformHierarchyRoots ()
{
	vector<Transform*> transforms;
	Object::FindObjectsOfType(&transforms);
	for (int i=0;i<transforms.size();i++)
		TransformHierarchyChanged (transforms[i]);
}

void SceneTracker::TransformDidSetParent (Transform* obj, Transform* oldParent, Transform* newParent)
{
	PPtr<Transform> pptr = obj;
	PPtr<Transform> oldParentPPtr = oldParent;
	PPtr<Transform> newParentPPtr = newParent;
	if (!obj->TestHideFlag(Object::kHideInHierarchy))
	{
		for (SceneInspectorIterator c= m_SceneInspectors.begin();c != m_SceneInspectors.end(); c++)
		{
			ISceneInspector* inspector = *c;
			if (inspector)
				inspector->TransformDidSetParent(pptr, oldParentPPtr, newParentPPtr);
		}	
	}
}

void SceneTracker::TransformHierarchyChanged (Transform* t)
{
	SET_ALLOC_OWNER(NULL);
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));

	// If transform was part of visible hierarchy, always remove it first
	// (will add it with new key if needed below)
	Transform::VisibleRootMap::iterator* rootIt = t->GetVisibleRootIterator();
	if (rootIt != NULL)
	{
		m_VisibleRootTransforms.erase(*rootIt);
		t->ClearVisibleRootIterator();
		m_TransformHierarchyHasChanged = true;
	}

	bool shouldBePartOfVisibleHierarchy = !t->IsPersistent() && !t->TestHideFlag(Object::kHideInHierarchy);
	if (shouldBePartOfVisibleHierarchy)
		m_TransformHierarchyHasChanged = true;

	// Transform is a visible root transform; add to the roots
	if (shouldBePartOfVisibleHierarchy && t->GetParent () == NULL)
	{
#if ENABLE_EDITOR_HIERARCHY_ORDERING
		Transform::VisibleRootSecondaryKey secondaryKey = Transform::VisibleRootSecondaryKey(t->GetName(), t->GetInstanceID());
		Transform::VisibleRootKey key = Transform::VisibleRootKey(t->GetOrder(), secondaryKey);
#else
		
		Transform::VisibleRootKey key = Transform::VisibleRootKey(t->GetName(), t->GetInstanceID());
#endif
		Transform::VisibleRootMap::iterator it = m_VisibleRootTransforms.insert(Transform::VisibleRootMap::value_type(key, t)).first;
		t->SetVisibleRootIterator(it);
		m_TransformHierarchyHasChanged = true;
	}
}

void SceneTracker::TransformHierarchyChangedCallback (Transform* t)
{
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
	gSceneTracker->TransformHierarchyChanged(t);
}

void SceneTracker::TransformHierarchyChangedSetParentCallback (Transform* t, Transform* oldParent, Transform* parent)
{
	DebugAssertIf (!Thread::EqualsCurrentThreadID(Thread::mainThreadId));
	gSceneTracker->TransformHierarchyChanged(t);
	gSceneTracker->TransformDidSetParent(t, oldParent, parent);
}
