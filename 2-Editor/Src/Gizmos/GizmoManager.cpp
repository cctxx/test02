#include "UnityPrefix.h"
#include "GizmoManager.h"
#include "GizmoRenderer.h"
#include "Runtime/BaseClasses/GameObject.h"
//#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/Renderable.h"
#include "Runtime/Camera/Culler.h"
#include "Editor/Src/AnnotationManager.h"
#include "Editor/Src/Gizmos/GizmoDrawers.h"

using namespace std;
using namespace Unity;

void GizmoManager::AddGizmoRenderer (const string& className, DrawGizmo* drawGizmo, int options, CanDrawGizmo* canDraw, void* userData, bool isIcon /*= false*/)
{
	int classID = Object::StringToClassID (className);
	AssertIf (classID == -1);
	GizmoSetup setup;
	setup.requireOptions = options;
	setup.drawFunc = drawGizmo;
	setup.canDrawFunc = canDraw;
	setup.userData = userData;
	setup.isIcon = isIcon;

	// insert only class "className"
	if ((options & kAddDerivedClasses) == 0)
		m_GizmoSetups.insert (make_pair (classID, setup));
	// insert class "className" and all derived classes
	else
	{
		vector<SInt32> classes;
		Object::FindAllDerivedClasses (classID, &classes);

		for (int i=0;i<classes.size ();i++)
			m_GizmoSetups.insert (make_pair (classes[i], setup));
	}
}

bool GizmoManager::HasGizmo (int classID)
{
	pair<GizmoSetups::iterator, GizmoSetups::iterator> range;
	range = m_GizmoSetups.equal_range (classID);
	for (GizmoSetups::iterator i=range.first;i != range.second;++i)
		if (!i->second.isIcon)
			return true;
	
	return false;
}

bool GizmoManager::HasIcon (int classID)
{
	pair<GizmoSetups::iterator, GizmoSetups::iterator> range;
	range = m_GizmoSetups.equal_range (classID);
	for (GizmoSetups::iterator i=range.first;i != range.second;++i)
		if (i->second.isIcon)
			return true;

	return false;
}


bool GizmoManager::IsGizmosAllowedForObject (Object* obj)
{
	Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (obj);
	if (component)
	{
		// No gizmos for disabled components
		if (!component->IsActive ())
			return false;

		// No gizmos for invisible layers
		GameObject* go = component->GetGameObjectPtr();
		if (go && (go->GetLayerMask() & m_VisibleLayers) == 0)
			return false;

		// No gizmos if disabled by annotation manager
		if (const Annotation* a = GetAnnotationManager ().GetAnnotation (component))//!GetAnnotationManager().IsGizmoEnabled(component))
			if (!a->m_GizmoEnabled)
				return false;
	}	
	return true;
}


void GizmoManager::BuildGizmosForGameObject (GameObject& go, const std::set<EditorExtension*>& active, const std::set<EditorExtension*>& selectionAndChild, const std::set<EditorExtension*>& selection)
{
	// No gizmos if gameObject belongs to a invisible layer
	if ((go.GetLayerMask() & m_VisibleLayers) == 0)
		return;
	// If gameObject is not active then none of its components are. Data templates are always inactive.
	if (!go.IsActive())
		return;
	if (go.TestHideFlag (Object::kHideInHierarchy))
		return;

	// Build gizmo for GameObject itself (it can have its own icon)
	bool iconAdded = BuildGizmos (go, active, selectionAndChild, selection, false);

	// Add gizmos for each component
	for (int i=0; i<go.GetComponentCount(); i++)
	{
		Unity::Component& component = go.GetComponentAtIndex(i);

		if (component.TestHideFlag (Object::kHideInHierarchy))
			continue;

		iconAdded |= BuildGizmos (component, active, selectionAndChild, selection, iconAdded);
	}
}

bool GizmoManager::BuildGizmos (EditorExtension& object, const std::set<EditorExtension*>& allActive, const std::set<EditorExtension*>& selectionAndChild, const std::set<EditorExtension*>& selection, bool iconAdded)
{
	int classID = object.GetClassID ();
	pair<GizmoSetups::iterator, GizmoSetups::iterator> range;
	range = m_GizmoSetups.equal_range (classID);
	if (range.first == range.second)
		return false;

	// No gizmos if disabled by annotation manager
	bool gizmoEnabled = true;
	bool iconEnabled = true;
	if (const Annotation* annotation = GetAnnotationManager().GetAnnotation (&object))
	{
		gizmoEnabled = annotation->m_GizmoEnabled;
		iconEnabled = annotation->m_IconEnabled;
	}
	

	for (GizmoSetups::iterator i=range.first;i != range.second;++i)
	{
		GizmoSetup& setup = i->second;
		
		// We only allow to add one icon
		if (setup.isIcon)
			if	(iconAdded || !iconEnabled)
				continue;

		if (!setup.isIcon && !gizmoEnabled)
			continue;


		// To be inserted the type has to be drawSelected or the object actually is selected
		// It has to be a non-temporary object
		// components have to be active
		// the can draw function returns true if there is one
		int mask = 0;
		if (allActive.count(&object))
			mask |= kActive;
		if (selectionAndChild.count(&object))
			mask |= kSelectedOrChild;
		else
			mask |= kNotSelected;
		if (selection.count(&object))
			mask |= kSelected;

		if (mask & setup.requireOptions)
		{	
			if (setup.canDrawFunc == NULL || setup.canDrawFunc (object, mask, setup.userData))
			{
				ActiveGizmo active;
				active.options = mask;
				active.requireOptions = setup.requireOptions;
				active.object = PPtr<EditorExtension> (&object);
				active.drawFunc = setup.drawFunc;
				active.userData = setup.userData;
				m_ActiveGizmos.insert (active);				
				
				if (setup.isIcon)
					iconAdded = true;
			}
		}
	}
	return iconAdded;
}

void GizmoManager::RecalculateGizmos (const std::set<EditorExtension*>& active, const std::set<EditorExtension*>& selectionAndChild, const std::set<EditorExtension*>& selection)
{
	m_ActiveGizmos.clear ();
	
	vector<GameObject*> objects;
	Object::FindObjectsOfType (&objects);
	for (int i=0; i<objects.size (); i++)
	{
		GameObject& go = *static_cast<GameObject*> (objects[i]);
		BuildGizmosForGameObject (go, active, selectionAndChild, selection);
	}
	
	// In sync
	m_GizmosCleanID = m_GizmosDirtyID;
}


static void AddEntireGameObject (GameObject& go, set<EditorExtension*>& output)
{
	//	output.insert(go);
	int componentCount = go.GetComponentCount ();
	for (int i=0;i<componentCount;i++)
		output.insert (&go.GetComponentAtIndex (i));
}

void GetSelectionForGizmos (set<EditorExtension*>& active, set<EditorExtension*>& deep, set<EditorExtension*>& selection)
{
	Transform* transform = GetActiveTransform();
	if (transform)
		AddEntireGameObject (transform->GetGameObject(), active);
	
	set<Transform*> temp;
	
	temp = GetTransformSelection(kDeepSelection | kExcludePrefabSelection);
	for (set<Transform*>::iterator i=temp.begin();i!=temp.end();i++)
		AddEntireGameObject ((**i).GetGameObject(), deep);
	
	temp = GetTransformSelection(kExcludePrefabSelection);
	for (set<Transform*>::iterator i=temp.begin();i!=temp.end();i++)
		AddEntireGameObject ((**i).GetGameObject(), selection);
}


void GizmoManager::RecalculateGizmosIfDirty()
{
	if( m_GizmosDirtyID == m_GizmosCleanID)
		return;
	
	std::set<EditorExtension*> active, deep, selection;
	GetSelectionForGizmos (active, deep, selection);
	RecalculateGizmos (active, deep, selection);
}


void GizmoManager::ClearGizmoRenderers ()
{
	m_GizmoSetups.clear();
	m_ActiveGizmos.clear();
	m_StaticGizmos.clear();
}

bool GizmoManager::DrawAllGizmos ()
{
	RecalculateGizmosIfDirty();
	
	m_IsDrawingGizmos = true;
	gizmos::ClearGizmos();
	for (ActiveGizmos::iterator i=m_ActiveGizmos.begin ();i != m_ActiveGizmos.end ();i++)
	{
		Object* object = i->object;
		Unity::Component* cmp = dynamic_pptr_cast<Unity::Component*> (object);
		if (cmp)
		{
			if (!GetCurrentCameraPtr()->IsFiltered(cmp->GetGameObject()))
				continue;
		}	
		if (object)
		{
			int options = i->options;
			options &= ~kPickable;
			ClearGizmoMatrix ();
			i->drawFunc (*object, options, i->userData);
		}
	}

	for(StaticGizmos::iterator i=m_StaticGizmos.begin();i != m_StaticGizmos.end();i++)
	{
		StaticDrawGizmo* draw = *i;
		ClearGizmoMatrix ();
		gizmos::BeginGizmo( Vector3f::zero );
		draw();
	}
	
	gizmos::RenderGizmos();
	
	bool retval = false;
	m_IsDrawingGizmos = false;
	return retval;
}

void GizmoManager::DrawAllGizmosWithFunctor( PrerenderGizmoFunctor functor, void* userData )
{
	RecalculateGizmosIfDirty();
	
	m_IsDrawingGizmos = true;
	
	gizmos::ClearGizmos();
	for (ActiveGizmos::iterator i=m_ActiveGizmos.begin ();i != m_ActiveGizmos.end ();i++)
	{
		EditorExtension* object = i->object;
		if (object)
		{
			int options = i->options;
			if (i->requireOptions & kPickable)
				options |= kPickable;
			else
				continue;
			
			functor (userData, object);
			i->drawFunc (*object, options, i->userData);
		}
	}
		
	gizmos::RenderGizmos();
	
	m_IsDrawingGizmos = false;
}


void GizmoManager::CallFunctorForGizmos( PrerenderGizmoFunctor functor, void* userData )
{
	RecalculateGizmosIfDirty();
	for (ActiveGizmos::iterator i=m_ActiveGizmos.begin ();i != m_ActiveGizmos.end ();i++)
	{
		EditorExtension* object = i->object;
		if (object)
		{
			int options = i->options;
			if (i->requireOptions & kPickable)
				options |= kPickable;
			else
				continue;
			
			functor (userData, object);
		}
	}
}



void GizmoManager::SetVisibleLayers (UInt32 layers)
{
	if (layers == m_VisibleLayers)
		return;
	m_VisibleLayers = layers;
	
	SetGizmosDirty ();
	RecalculateGizmosIfDirty ();
}


GizmoManager::GizmoManager()
:	m_VisibleLayers(0xFFFFFFFF)
,	m_IsDrawingGizmos(false)
,	m_GizmosDirtyID (1)
,	m_GizmosCleanID (0)
{
	GetSceneTracker().AddSceneInspector(this);
}

GizmoManager::~GizmoManager()
{
	GetSceneTracker().RemoveSceneInspector(this);
}


GizmoManager& GizmoManager::Get ()
{
	static GizmoManager man;
	return man;
}

UInt32 GizmoManager::SetGizmosDirty ()
{
	return ++m_GizmosDirtyID;
}

// ISceneInspector methods
//@TODO: when rebuilding gizmos, keep a set of instanceIDs to check whether we actually need to rebuild
void GizmoManager::SelectionHasChanged (const std::set<int>& selection)
{
	SetGizmosDirty ();
}

void GizmoManager::GOHasChanged (PPtr<GameObject> object)
{
	SetGizmosDirty ();
}

void GizmoManager::GOWasDestroyed (GameObject* object)
{
	SetGizmosDirty ();
}

class GameViewDrawGizmos : public Renderable
{
	virtual void RenderRenderable (const CullResults& cullResults)
	{
		GizmoManager::Get ().DrawAllGizmos ();
	}
};

Renderable& GetGameViewGizmoRenderable()
{
	static GameViewDrawGizmos s_Renderable;
	return s_Renderable;
}


