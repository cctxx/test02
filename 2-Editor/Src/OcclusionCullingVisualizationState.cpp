#include "UnityPrefix.h"
#include "OcclusionCullingVisualizationState.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Camera/UnityScene.h"

using namespace std;

OcclusionCullingVisualization* gOcclusionCullingVisualization = NULL;

OcclusionCullingVisualization* GetOcclusionCullingVisualization()
{
	if (!gOcclusionCullingVisualization)
		gOcclusionCullingVisualization = new OcclusionCullingVisualization;
	return gOcclusionCullingVisualization;
}

OcclusionCullingVisualization::OcclusionCullingVisualization ()
{
	m_ShowOcclusionCulling	= false;
	m_ShowPreVis			= false;
	m_ShowViewVolumes		= true;
	m_ShowGeometryCulling	= true;
    m_ShowVFCulling			= true;
	m_ShowPortals			= false;
	m_ShowVisLines			= false;
	m_SmallestHole			= 0.5F;
}

void OcclusionCullingVisualization::SetShowPreVis (bool state)
{
	if (GetScene().GetUmbraDataSize() > 0)
		m_ShowPreVis = state;
}

bool OcclusionCullingVisualization::GetShowPreVis ()
{
	return m_ShowPreVis || GetScene().GetUmbraDataSize() == 0;
}

void OcclusionCullingVisualization::SetShowGeometryCulling (bool state)
{
	if (GetScene().GetUmbraDataSize() > 0)
		m_ShowGeometryCulling = state;
}

bool OcclusionCullingVisualization::GetShowGeometryCulling ()
{
	return m_ShowGeometryCulling && GetScene().GetUmbraDataSize() > 0;
}

Camera* FindPreviewOcclusionCamera ()
{
	Transform* transform = GetActiveTransform();
	if (transform && transform->QueryComponent(Camera))
		return transform->QueryComponent(Camera);
	
	return FindMainCamera();
}
