#pragma once

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Graphics/Transform.h"
class Prefab;

class ISceneInspector
{
	public:

	virtual ~ISceneInspector ();

	// The selection or active object has changed
	virtual bool HasObjectChangedCallback () { return false; }
	virtual void TransformDidSetParent (PPtr<Transform> /*object*/, PPtr<Transform> /*oldParent*/, PPtr<Transform> /*newParent*/) { }

///@TODO: SWITCH THIS TO USE PPTR<Object>
	// The selection or active object has changed
	virtual void SelectionHasChanged (const std::set<int>& /*selection*/) { }

	virtual void ActiveGOHasChanged (PPtr<GameObject> /*go*/) { }

	virtual void ActiveObjectHasChanged (PPtr<Object> /*object*/) { }

	// The GameObject has changed or it was created
	virtual void GOHasChanged (PPtr<GameObject> /*object*/) { }
	// The GameObject was destroyed (go.GetInstanceID () is non 0)
	virtual void GOWasDestroyed (GameObject* /*object*/) { }

	// The object has changed or it was created
	virtual void ObjectHasChanged (PPtr<Object> /*object*/) { }

	// The object was destroyed (go.GetInstanceID () is non 0)
	virtual void ObjectWasDestroyed (PPtr<Object> /*object*/) { }

	virtual void HierarchyWindowHasChanged () { }
	virtual void ProjectWindowHasChanged () { }

	virtual void DidFlushDirty () {  }

	virtual void Update () {  }

	virtual void TickInspector () { }
	virtual void TickInspectorBackground () { }
	virtual void ForceReloadInspector (bool /*fullRebuild*/) { }
	virtual void DidOpenScene () { }
	virtual void DidPlayScene () { }

	virtual bool CanOpenScene () { return true; }
	virtual bool CanEnterPlaymode (){ return true; }
	virtual bool CanTerminate () { return true; }
};

class SceneTracker
{
	public:

	static void Initialize ();

	SceneTracker ();
	~SceneTracker ();

	/// Select an object.
	void SetActive (Object* anObject);
	void SetActiveID (int anObject);

	/// Get the currently active object.
	Object* GetActive ();
	int GetActiveID ();

	/// Get the currently active Game Object.
	GameObject* GetActiveGO ();

	/// Get the array of all selected objects (including active)
	void GetSelection (TempSelectionSet& selection);
	std::set<PPtr<Object> > GetSelectionPPtr ();
	std::set<int> GetSelectionID ();

	/// Set selection as an set of instanceIDs
	void SetSelectionID (const std::set<int>& sel);
	template<typename container>
	void SetSelection (const container& sel);
	void SetSelectionPPtr (const std::set<PPtr<Object> >& sel);

	/// Add an inspector for scene-based callbacks
	void AddSceneInspector (ISceneInspector* inspector);

	/// Remove an inspector
	void RemoveSceneInspector (ISceneInspector* inspector);

	// Since managed code is holding some scene inspectors, we need some solution to delete scene inspectors from a different thread.
	// I want to avoid adding mutex to all scene inspector update calls, thus we just delay destruction until the next flush dirty.
	void DelayedDeleteSceneInspector (ISceneInspector* inspector);

	/// Execute all dirty objects that are waiting to get their SetDirty function executed.
	void FlushDirty ();

	/// We are currently flushing are performing scene inspection callbacks. Thus it is forbidden to call FlushDirty!!!
	bool IsLocked ();

	void Update ();

	void ClearUnloadedDirtyCallbacks ();


	void TickInspector ();
	void TickInspectorBackground ();
	void ForceReloadInspector (bool fullRebuild = false);
	bool CanOpenScene ();
	void DidOpenScene ();
	bool CanEnterPlaymode ();
	void DidPlayScene ();
	bool CanTerminate ();

	void ProjectWindowHasChanged ();
	void TickHierarchyWindowHasChanged ();

#if UNITY_EDITOR
	Transform::VisibleRootMap& GetVisibleRootTransforms();
#endif

	/// Needs to be called after a level has been loaded.
	/// During loading objects are marked persistent thus will not register!
	void ReloadTransformHierarchyRoots ();
	void DirtyTransformHierarchy () { m_TransformHierarchyHasChanged = true; }

private:

	void NotifyGOHasChanged (PPtr<GameObject> go);
	void NotifyGOWasDestroyed (GameObject* go);
	void NotifyObjectWasDestroyed (int object);

	static void DestroyGameObjectCallback (GameObject* go);
	static void DestroyObjectCallback (int instanceID);
	static void SetGameObjectNameCallback (GameObject* go);
	static void SetObjectDirty (Object* ptr);
	static void TransformHierarchyChangedCallback (Transform* t);
	static void TransformHierarchyChangedSetParentCallback (Transform* obj, Transform* oldParent, Transform* newParent);

	void TransformHierarchyChanged (Transform* t);
	void TransformDidSetParent (Transform* obj, Transform* old, Transform* parent);

#if UNITY_EDITOR
	Transform::VisibleRootMap	m_VisibleRootTransforms;
#endif

	std::set<int>	m_Selection;
	std::set<int>	m_OldSelection;
	typedef std::list<ISceneInspector*>::iterator SceneInspectorIterator;
	std::list<ISceneInspector*>  m_SceneInspectors;
	std::list<ISceneInspector*>  m_SceneInspectorsObjectHasChanged;

	Mutex                        m_DelayDeleteMutex;
	std::list<ISceneInspector*>  m_DelayDeletedSceneInspectors;

	int m_Active;
	int m_OldActive;
	int m_OldActiveGameObject;
	int m_ActiveGameObject;

	int m_DontSendSelectionChanged;
	int m_LockedInspector;
	void* m_DirtyCallbacks;
	bool m_TransformHierarchyHasChanged;


	friend class ISceneInspector;
};

SceneTracker& GetSceneTracker ();
