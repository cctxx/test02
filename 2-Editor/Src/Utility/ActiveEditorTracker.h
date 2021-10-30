#pragma once

#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/Src/SceneInspector.h"
#include "InspectorMode.h"
#include "Runtime/Utilities/dynamic_array.h"

class ActiveEditorTracker : public ISceneInspector
{
	public:
	
	struct Element
	{
		std::vector <PPtr<Object> > m_Objects;
		MonoBehaviour*  m_Inspector;
		int             m_IsVisible;
		bool            m_IsInspectorHidden;
		bool            m_RequireStateComparision;
		dynamic_array<UInt8>   m_LastState;
		
		Element (std::vector<PPtr<Object> > objs) { m_Objects = objs; m_Inspector = NULL; m_IsVisible = -1; m_IsInspectorHidden = false; m_RequireStateComparision = false;}
		
		~Element ();

		friend bool operator != (const Element& lhs, const Element& rhs)
		{
			return lhs.m_Objects != rhs.m_Objects;
		}
		friend bool operator == (const Element& lhs, const Element& rhs)
		{
			return lhs.m_Objects == rhs.m_Objects;
		}
	};

	ActiveEditorTracker ();
	virtual ~ActiveEditorTracker ();

	static void InitializeSharedTracker();
	static ActiveEditorTracker* sSharedTracker;

	std::vector<MonoBehaviour*> GetEditors ();

	void SetVisible (unsigned i, int vis) { AssertIf(i >= m_Elements.size()); m_Elements[i].m_IsVisible = vis; }
	int GetVisible (unsigned i) { AssertIf(i >= m_Elements.size()); return m_Elements[i].m_IsVisible; }
	
	void VerifyModifiedMonoBehaviours ();
	
	virtual bool HasObjectChangedCallback () { return true; }
//	virtual void GOHasChanged (PPtr<GameObject> object) { }
//	virtual void ObjectWasDestroyed (PPtr<Object> object) { }
	virtual void ObjectHasChanged (PPtr<Object> obj);
	virtual void ForceReloadInspector (bool fullRebuild);

	void Rebuild ();
	virtual void DidFlushDirty ();
	
	bool IsDirty ();
	void ClearDirty () { m_IsDirty = false; }
	
	bool IsLocked () { return m_LockedObject.GetInstanceID () != 0; }
	void SetIsLocked (bool locked);

	InspectorMode GetInspectorMode () { return m_InspectorMode; }
	void SetInspectorMode (InspectorMode mode);
	
	bool HasComponentsWhichCannotBeMultiEdited () { return m_HasComponentsWhichCannotBeMultiEdited; }
	static void GetAllObjectsFromSelectionExcludingActiveObject (std::vector<Object*>& outObjects);
	
	static std::vector<Object*> GetLockedObjects ();

	private:

	static std::vector<ActiveEditorTracker*> s_AllActiveEditorTrackers;

	void BuildSubObjectsVector (std::set<PPtr<Object> > &objs, std::vector<ActiveEditorTracker::Element>& components);
	
	typedef std::vector<Element> Elements;
	Elements             m_Elements;
	PPtr<Object>         m_LockedObject;
	std::map<int, PPtr<Object> > m_CachedAssetImporter;
	bool                 m_IsDirty;
	InspectorMode	     m_InspectorMode;
	bool				 m_HasComponentsWhichCannotBeMultiEdited;
};

ActiveEditorTracker* GetSharedActiveEditorTracker ();
