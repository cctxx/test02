#include "UnityPrefix.h"
#include "RenderManager.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Camera.h"
#include "Renderable.h"
#include <vector>
#include "UnityScene.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Camera/CullResults.h"
#include "Runtime/Camera/CameraCullingParameters.h"

static RenderManager* gRenderManager = NULL;


// NOTE: AddCamera/RemoveCamera defer the actual addition/removal when called from inside Culling/Rendering
// loop. Reason: add/remove may invalidate iterators; and that happens in cases like disabling self (or some other)
// camera from OnPostRender etc.

RenderManager::RenderManager ()
:	m_InsideRenderOrCull(false)
{
	m_WindowRect = Rectf(0.0f, 0.0f, 128.0f, 128.0f );
	m_CurrentViewPort[0] = m_CurrentViewPort[1] = 0;
	m_CurrentViewPort[2] = m_CurrentViewPort[3] = 128;
	m_CurrentCamera = NULL;
}

RenderManager::~RenderManager () {
	Assert (m_Cameras.empty ());
}


PROFILER_INFORMATION(gCameraRenderManagerProfile, "Camera.Render", kProfilerRender)
PROFILER_INFORMATION(gCameraUpdateRenderersProfile, "Rendering.UpdateDirtyRenderers", kProfilerRender)


void RenderManager::RenderOffscreenCameras()
{
	m_InsideRenderOrCull = true;

	// Make sure current viewport is fully preserved after rendering offscreen cameras
	int savedViewport[4];
	for (int i = 0; i < 4; ++i)
		savedViewport[i] = m_CurrentViewPort[i];
	
	// Render all offscreen cameras so they are ready for the real cameras.
	for (CameraContainer::iterator i = m_OffScreenCameras.begin(); i != m_OffScreenCameras.end(); ++i )
	{
		Camera* cam = *i;
		PROFILER_AUTO_GFX(gCameraRenderManagerProfile, cam)
	
		m_CurrentCamera = cam;
		CullResults cullResults;
		if( cam && cam->GetEnabled() ) // might become NULL or disabled in OnPreCull
			cam->Cull(cullResults);
		if( cam && cam->GetEnabled() ) // check again, might get disabled in culling
			cam->Render( cullResults, Camera::kRenderFlagSetRenderTarget);
	}

	for (int i = 0; i < 4; ++i)
		m_CurrentViewPort[i] = savedViewport[i];

	m_InsideRenderOrCull = false;
	AddRemoveCamerasDelayed();
}

void RenderManager::RenderCameras()
{
	m_InsideRenderOrCull = true;
	
	Unity::Scene& scene = GetScene();

	// Render on-screen cameras.
	for (CameraContainer::iterator i = m_Cameras.begin(); i != m_Cameras.end(); ++i )
	{
		Camera* cam = *i;	
		PROFILER_AUTO_GFX(gCameraRenderManagerProfile, cam)
		
		/////@TODO: This is not reflected in the standalone render function...
		scene.BeginCameraRender();
		m_CurrentCamera = cam;
		CullResults cullResults;
		if( cam && cam->GetEnabled() ) // might become NULL or disabled in OnPreCull
			cam->Cull(cullResults);
		if( cam && cam->GetEnabled() ) // check again, might get disabled in culling
			cam->Render( cullResults, Camera::kRenderFlagSetRenderTarget );
		scene.EndCameraRender();
	}

	m_InsideRenderOrCull = false;
	AddRemoveCamerasDelayed();
}


/** Get a render callback for each Camera */
void RenderManager::AddCameraRenderable (Renderable *r, int depth) {
//	Assert (depth <= Camera::kRenderQueueCount);
	#if DEBUGMODE
	for (Renderables::iterator i = m_Renderables.begin (); i != m_Renderables.end();i++)  {
		if (i->second == r && i->first == depth)
			AssertString ("RenderManager: renderable with same depth already added");
	}
	#endif

	m_Renderables.insert (std::make_pair (depth, r));
}

void RenderManager::RemoveCameraRenderable (Renderable *r) {
	Renderables::iterator next;
	for (Renderables::iterator i = m_Renderables.begin (); i != m_Renderables.end();i=next)  {
		next = i;
		next++;
		if (i->second == r)
		{
			m_Renderables.erase (i);
		}
	}
}


void RenderManager::InvokeOnRenderObjectCallbacks ()
{
	if (m_OnRenderObjectCallbacks.empty())
		return;

#if ENABLE_MONO || UNITY_WINRT
	SafeIterator<MonoBehaviourList> it (m_OnRenderObjectCallbacks);
	while (it.Next())
	{
		MonoBehaviour& beh = **it;
		beh.InvokeOnRenderObject ();
	}
#endif
}


#if UNITY_EDITOR

bool RenderManager::HasFullscreenCamera () const
{
	for (CameraContainer::const_iterator i = m_Cameras.begin(); i != m_Cameras.end(); i++) {
		Rectf viewRect = (*i)->GetNormalizedViewportRect  ();
		if (CompareApproximately(Rectf(0,0,1,1), viewRect))
			return true;
	}
	return false;
}

#endif // UNITY_EDITOR


void RenderManager::AddCamera (Camera *c)
{
	Assert (c != NULL);
	
	PPtr<Camera> cam(c);
	if( m_InsideRenderOrCull )
	{
		m_CamerasToRemove.remove( cam );
		m_CamerasToAdd.push_back( cam );
		return;
	}

	m_CamerasToAdd.remove(c);
	m_CamerasToRemove.remove(c);

	m_Cameras.remove( cam );
	m_OffScreenCameras.remove( cam );
	CameraContainer &queue = (c->GetTargetTexture() == NULL) ? m_Cameras : m_OffScreenCameras;
	
	for (CameraContainer::iterator i=queue.begin ();i != queue.end ();i++)
	{
		Camera* curCamera = *i;
		if (curCamera && curCamera->GetDepth () > c->GetDepth ())
		{
			queue.insert (i, c);
			return;
		}
	}
	queue.push_back (c);
}

void RenderManager::RemoveCamera (Camera *c)
{
	PPtr<Camera> cam(c);

	m_CamerasToAdd.remove(c);
	m_CamerasToRemove.remove(c);

	if( m_InsideRenderOrCull )
	{
		m_CamerasToRemove.push_back( cam );
	}
	else
	{
		m_Cameras.remove( cam );
		m_OffScreenCameras.remove( cam );
	}

	Camera* currentCamera = m_CurrentCamera;
	if (currentCamera == c)
	{
		if (m_Cameras.empty ())
			m_CurrentCamera = NULL;
		else
			m_CurrentCamera = m_Cameras.front (); // ??? maybe better choose next
	}
}

void RenderManager::AddRemoveCamerasDelayed()
{
	DebugAssertIf( m_InsideRenderOrCull );
	for( CameraContainer::iterator i = m_CamerasToRemove.begin(); i != m_CamerasToRemove.end(); /**/ )
	{
		Camera* cam = *i;
		++i; // increment iterator before removing camera; as it changes the list
		RemoveCamera( cam );
	}
	m_CamerasToRemove.clear();
	for( CameraContainer::iterator i = m_CamerasToAdd.begin(); i != m_CamerasToAdd.end(); /**/ )
	{
		Camera* cam = *i;
		++i; // increment iterator before adding camera; as it changes the list
		AddCamera( cam );
	}
	m_CamerasToAdd.clear();
}


void RenderManager::SetWindowRect (const Rectf& r)
{
	m_WindowRect = r;
	for( CameraContainer::iterator i=m_Cameras.begin ();i != m_Cameras.end (); ++i )
		(**i).WindowSizeHasChanged();
}


void RenderManager::UpdateAllRenderers()
{
	ParticleSystem::SyncJobs();
	
	PROFILER_AUTO(gCameraUpdateRenderersProfile, NULL) 
	Renderer::UpdateAllRenderersInternal();
}


RenderManager& GetRenderManager ()
{
	return *gRenderManager;
}

RenderManager* GetRenderManagerPtr ()
{
	return gRenderManager;
}

void RenderManager::InitializeClass ()
{
	Assert(gRenderManager == NULL);
	gRenderManager = new RenderManager ();
}

void RenderManager::CleanupClass ()
{
	Assert(gRenderManager != NULL);
	delete gRenderManager;
	gRenderManager = NULL;
}
