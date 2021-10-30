#include "UnityPrefix.h"
#include "GUILayer.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"

PROFILER_INFORMATION(gGUILayerProfile, "Camera.GUILayer", kProfilerRender);

GUILayer::GUIElements* GUILayer::ms_GUIElements = NULL;

GUILayer::GUILayer(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode) 	
{
}

GUILayer::~GUILayer ()
{
}

inline bool SortGUIByDepth (GUIElement* lhs, GUIElement* rhs)
{
	return lhs->GetComponent (Transform).GetLocalPosition ().z < rhs->GetComponent (Transform).GetLocalPosition ().z;
}

void GUILayer::InitializeClass ()
{
	ms_GUIElements = new GUIElements;
}

void GUILayer::CleanupClass()
{
	delete ms_GUIElements;
}


void GUILayer::RenderGUILayer ()
{
	PROFILER_AUTO_GFX(gGUILayerProfile, this);
	
	ms_GUIElements->apply_delayed ();
	if (ms_GUIElements->empty())
		return;

	typedef UNITY_TEMP_VECTOR(GUIElement*) TempElements;
	TempElements elements (ms_GUIElements->begin (), ms_GUIElements->end ());
	
	std::sort (elements.begin (), elements.end (), SortGUIByDepth);

	Camera& camera = GetComponent(Camera);
	UInt32 cullingMask = camera.GetCullingMask ();
	Rectf cameraRect = camera.GetScreenViewportRect();
	
	for (TempElements::iterator it=elements.begin(), itEnd = elements.end(); it != itEnd; ++it)
	{
		GUIElement& element = **it;
		if (element.GetGameObject ().GetLayerMask () & cullingMask)
			element.RenderGUIElement (cameraRect);
	}
}

GUIElement* GUILayer::HitTest (const Vector2f& screenPosition)
{
	Camera& camera = GetComponent (Camera);
	Vector3f viewportPos3D = camera.ScreenToViewportPoint (Vector3f (screenPosition.x, screenPosition.y, camera.GetNear()));
	Vector2f viewportPos (viewportPos3D.x, viewportPos3D.y);
	Rectf normalized (0.0F,0.0F,1.0F,1.0F);
	
	if (!normalized.Contains (viewportPos.x, viewportPos.y))
		return NULL;

	Rectf cameraRect = camera.GetScreenViewportRect();

	Rectf windowRect = GetRenderManager ().GetWindowRect ();
	viewportPos.x *= windowRect.Width ();
	viewportPos.y *= windowRect.Height ();
	
	GUIElement* topmost = NULL;
	float topmostZ = -std::numeric_limits<float>::infinity ();
	
	// GUI hit testing always ignores IgnoreRaycast layer
	UInt32 cullingMask = camera.GetCullingMask() & ~(kIgnoreRaycastMask);

	for (GUIElements::iterator it=ms_GUIElements->begin (), itEnd = ms_GUIElements->end (); it != itEnd; ++it)
	{
		GUIElement* element = *it;
		if (element && (element->GetGameObject ().GetLayerMask () & cullingMask) && element->HitTest (viewportPos, cameraRect))
		{
			float z = element->GetComponent (Transform).GetLocalPosition ().z;
			if (z > topmostZ)
			{
				topmost = element;
				topmostZ = z;
			}
		}
	}
	
	return topmost;
}

IMPLEMENT_CLASS_HAS_INIT(GUILayer)
