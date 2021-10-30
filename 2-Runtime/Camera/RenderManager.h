#pragma once

#include "Runtime/Utilities/LinkedList.h"
#include <list>
#include <map>
#include "Runtime/Math/Rect.h"
#include "Camera.h"

class Renderable;
class MonoBehaviour;
typedef ListNode<MonoBehaviour> MonoBehaviourListNode;

// Camera handling
class RenderManager {
public:
	typedef std::list<PPtr<Camera> >	CameraContainer;
	typedef std::multimap<int, Renderable *> Renderables;

	RenderManager ();
	~RenderManager ();

	void RenderOffscreenCameras();
	void RenderCameras();

	Camera &GetCurrentCamera () 			{ Assert (!m_CurrentCamera.IsNull ()); return *m_CurrentCamera; }
	Camera* GetCurrentCameraPtr () 			{ return m_CurrentCamera; }
	void SetCurrentCamera (Camera *c)		{ m_CurrentCamera = c; }

	void AddCamera (Camera *c);
	void RemoveCamera (Camera *c);
	
	// Add/Remove Renderable. The Renderable.Render() method is called before the RenderQueue with index beforeRenderqueueIndex
	// is rendered in Camera.Render()
	void AddCameraRenderable (Renderable *r, int beforeRenderqueueIndex);
	void RemoveCameraRenderable (Renderable *r);
	const Renderables& GetRenderables () { return m_Renderables; }

	void AddOnRenderObject (MonoBehaviourListNode& beh) { m_OnRenderObjectCallbacks.push_back(beh); }
	void InvokeOnRenderObjectCallbacks ();
	
	CameraContainer& GetOnscreenCameras ()	{ return m_Cameras; }
	CameraContainer& GetOffscreenCameras ()	{ return m_OffScreenCameras; }

	// The window we're rendering into.
	// Most often xmin/xmax of this window are zero, except when using fixed aspect
	// in the game view.
	const Rectf &GetWindowRect() const { return m_WindowRect; }
	void SetWindowRect (const Rectf& r);

	const int* GetCurrentViewPort() const { return m_CurrentViewPort; }
	int* GetCurrentViewPortWriteable() { return m_CurrentViewPort; }
	
	#if UNITY_EDITOR
	bool HasFullscreenCamera () const;
	#endif

	static void UpdateAllRenderers();

	static void InitializeClass ();
	static void CleanupClass ();

private:
	void AddRemoveCamerasDelayed();
	
private:
	PPtr<Camera> m_CurrentCamera;
	CameraContainer m_Cameras, m_OffScreenCameras;
	CameraContainer m_CamerasToAdd;
	CameraContainer m_CamerasToRemove;
	bool m_InsideRenderOrCull;

	Rectf	m_WindowRect;
	int		m_CurrentViewPort[4]; // left, bottom, width, height; in pixels into current render target

	Renderables m_Renderables;
	typedef List<MonoBehaviourListNode> MonoBehaviourList;
	MonoBehaviourList m_OnRenderObjectCallbacks;
};

RenderManager& GetRenderManager ();
RenderManager* GetRenderManagerPtr ();
inline Camera &GetCurrentCamera () { return GetRenderManager().GetCurrentCamera(); }
inline Camera* GetCurrentCameraPtr () { return GetRenderManager().GetCurrentCameraPtr(); }
