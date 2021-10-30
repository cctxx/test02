#ifndef GIZMOMANAGER_H
#define GIZMOMANAGER_H

#include "Runtime/BaseClasses/EditorExtension.h"
#include "GizmoUtil.h"
#include "Editor/Src/SceneInspector.h"
#include <map>
#include <set>
#include <list>

class Renderable;


// Queues gizmo rendering callbacks for Objects that should be rendered
class GizmoManager : ISceneInspector
{
public:
	~GizmoManager();

	/// Adds a new gizmo for a class.
	/// DrawGizmo is called when a gizmo is determined to be drawn.
	/// CanDrawGizmo can be used to conditionally add gizmos
	/// (eg. PythonBehaviour's use it to check if the PythonBehaviours implement supports DrawGizmos)
	/// the options mask can be:
	/// - kPickable -> The gizmo can be picked using the mouse otherwise it is ignored when picking
	/// - kDrawUnselected -> Draw the gizmo even if the object is not selected
	/// - kAddDerivedClasses -> All Derived classes will render the gizmo as well
	/// By default a gizmo is not pickable, only drawn when selected
	typedef void DrawGizmo (Object& o, int mask, void* userData);
	typedef void StaticDrawGizmo ();
	typedef bool CanDrawGizmo (Object& o, int mask, void* userData);
	enum { kPickable = 1 << 0, kNotSelected = 1 << 1, kSelected = 1 << 2, kActive = 1 << 3, kSelectedOrChild = 1 << 4, kAddDerivedClasses = 1 << 5 };
	void AddGizmoRenderer (const std::string& className, DrawGizmo* drawGizmo, int requiredOptions, CanDrawGizmo* canDraw = NULL, void* userData = NULL, bool isIcon = false);
	void AddStaticGizmoRenderer (StaticDrawGizmo* drawGizmo) { m_StaticGizmos.push_back(drawGizmo); }
	void ClearGizmoRenderers ();
	
	// Draws all gizmos
	bool DrawAllGizmos ();
	
	// Renders all gizmos, calling functor before drawing each.
	// This will only draw gizmos that have kPickable in requireOptions.
	typedef void PrerenderGizmoFunctor( void* userData, const Object* o );
	void DrawAllGizmosWithFunctor( PrerenderGizmoFunctor functor, void* userData );

	/// Call a functor for each visible gizmos that has kPickable in requireOptions.
    /// Used for rect selection to check the positions against the selection rect
    void CallFunctorForGizmos( PrerenderGizmoFunctor functor, void* userData );

	// Gets the GizmoManager singleton
	static GizmoManager& Get ();
		
	// Call this when a lot of gizmos have changed
	// completeSelection is the selection including components
	void RecalculateGizmos (const std::set<EditorExtension*>& active, const std::set<EditorExtension*>& selectionAndChild, const std::set<EditorExtension*>& selection);
	
	void SetVisibleLayers (UInt32 layers);
	
	bool IsDrawingGizmos () const { return m_IsDrawingGizmos; }
	void SetIsDrawingGizmos (bool value) { m_IsDrawingGizmos = value; }

	bool HasGizmo (int classID);
	bool HasIcon (int classID);
	bool IsGizmosAllowedForObject (Object* obj);

	UInt32 SetGizmosDirty ();
	UInt32 GetGizmosDirtyID () const { return m_GizmosDirtyID;}
private:
	// ISceneInspector
	virtual bool HasObjectChangedCallback () { return false; }
	virtual void SelectionHasChanged (const std::set<int>& selection);
	virtual void GOHasChanged (PPtr<GameObject> object);
	virtual void GOWasDestroyed (GameObject* object);

private:
	GizmoManager();
	enum GizmoType {Mesh, Icon};

	friend void RebuildGizmoRenderers();
	void RecalculateGizmosIfDirty();
	
	// Build gizmos for the gameObject and its components (we only allow one icon gizmo per gameobject)
	void BuildGizmosForGameObject (GameObject& go, const std::set<EditorExtension*>& active, const std::set<EditorExtension*>& selectionAndChild, const std::set<EditorExtension*>& selection);
	
	// Returns true if a icon gizmo was added
	bool BuildGizmos (EditorExtension& object, const std::set<EditorExtension*>& active, const std::set<EditorExtension*>& selectionAndChild, const std::set<EditorExtension*>& selection, bool iconAdded);
		

	struct ActiveGizmo {
		int        options;
		int        requireOptions;
		DrawGizmo* drawFunc;
		void*      userData;
		PPtr<EditorExtension> object;

		bool operator < (const ActiveGizmo& rhs) const { return object < rhs.object; }
	};
	
	struct GizmoSetup
	{
		bool isIcon;
		int	requireOptions;
		void* userData;
		DrawGizmo* drawFunc;
		CanDrawGizmo* canDrawFunc;
			
	};
	typedef std::multimap<int, GizmoSetup> GizmoSetups;
	typedef std::multiset<ActiveGizmo> ActiveGizmos;
	typedef std::list<StaticDrawGizmo*> StaticGizmos;
	GizmoSetups  m_GizmoSetups;
	StaticGizmos m_StaticGizmos;
	ActiveGizmos m_ActiveGizmos;
	
	UInt32 m_VisibleLayers;
	
	bool m_IsDrawingGizmos;
	UInt32 m_GizmosDirtyID;
	UInt32 m_GizmosCleanID;
};

void GetSelectionForGizmos (std::set<EditorExtension*>& active, std::set<EditorExtension*>& deep, std::set<EditorExtension*>& selection);

Renderable& GetGameViewGizmoRenderable();


#endif
