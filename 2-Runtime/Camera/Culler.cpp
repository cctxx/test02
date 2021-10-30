#include "UnityPrefix.h"
#include "Culler.h"
#include "SceneCulling.h"
#include "UnityScene.h"
#include "CullResults.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "BaseRenderer.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Camera/IntermediateRenderer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/Transform.h"
#include "LightCulling.h"
#include "External/shaderlab/Library/intshader.h"

PROFILER_INFORMATION(gCulling, "Culling", kProfilerRender);
PROFILER_INFORMATION(gCullActiveLights, "CullAllVisibleLights", kProfilerRender);
PROFILER_INFORMATION(gCullPerObjectLights, "CullPerObjectLights", kProfilerRender);
PROFILER_INFORMATION(gCullScene, "CullSceneObjects", kProfilerRender);
PROFILER_INFORMATION(gCacheTransformInfo, "CacheTransformInfo.", kProfilerRender);


#if UNITY_EDITOR

inline bool IsRendererFiltered (BaseRenderer* r, CullFiltering filtering)
{
	if (filtering == kFilterModeOff)
		return true;
	
	if (r->GetRendererType() == kRendererIntermediate)
		return true;

	Renderer* casted = static_cast<Renderer*> (r);
	return IsGameObjectFiltered (casted->GetGameObject(), filtering);
}

bool IsGameObjectFiltered (Unity::GameObject& go, CullFiltering filtering)
{
	if (filtering == kFilterModeOff)
		return true;

	if (go.IsMarkedVisible())
		return filtering == kFilterModeShowFiltered;
	else
	{
		Transform& trs = go.GetComponent (Transform);
		if (trs.GetParent())
		{
			bool isFiltered = IsGameObjectFiltered(trs.GetParent()->GetGameObject(), filtering);
			go.SetMarkedVisible((isFiltered == (filtering == kFilterModeShowFiltered))?GameObject::kVisibleAsChild:GameObject::kNotVisible);
			return isFiltered;
		}
		else
			return filtering == kFilterModeShowRest;
	}
}
#endif

void InvokeOnWillRenderObject (const dynamic_array<BaseRenderer*>& renderers)
{
	// Invoke OnWillRenderObject callbacks.
	// These can only ever happen on non intermediate Renderers.
	// Scene needs to know we are calling scripts (case 445226).
	GetScene().SetPreventAddRemoveRenderer(true);
	for( int i = 0; i < renderers.size(); ++i )
	{
		Assert (renderers[i]->GetRendererType() != kRendererIntermediate);
		Renderer* r = static_cast<Renderer*>(renderers[i]);
		r->SendMessage (kOnWillRenderObject);
	}
	GetScene().SetPreventAddRemoveRenderer(false);
}


static void PrepareSceneNodes (const IndexList& visible, CullFiltering filtering, const SceneNode* nodes, VisibleNodes& output, dynamic_array<BaseRenderer*>& needsCullCallback)
{
	// Generate Visible nodes from all static & dynamic objects
	for (int i=0;i<visible.size;i++)
	{
		const SceneNode& node = nodes[visible.indices[i]];
		if (node.needsCullCallback)
			needsCullCallback.push_back(node.renderer);
		
		#if UNITY_EDITOR
		if (!IsRendererFiltered (node.renderer, filtering))
			continue;
		#endif
		
		VisibleNode& visibleNode = output.push_back ();
		visibleNode.renderer = node.renderer;
		visibleNode.lodFade = 0.0F;
	}
}

static void CacheTransformInfo (VisibleNodes& results)
{
	PROFILER_AUTO(gCacheTransformInfo, NULL)
	
	for( int i = 0; i < results.size(); ++i )
	{
		VisibleNode& node = results[i];
		node.SetTransformInfo (node.renderer->GetTransformInfo());
	}
}


///////////@TODO: Move this shit to the right places....
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Camera/ShadowCulling.h"

void CullShadowCasters (const Light& light, const ShadowCullData& cullData, bool excludeLightmapped, CullingOutput& cullingOutput  );

static void CullAllPerObjectLights (const VisibleNodes& visibleNodes, const ActiveLights& lights, const int renderPath, ObjectLightIndices& forwardLightIndices, ObjectLightIndices& forwardLightOffsets)
{
	PROFILER_AUTO(gCullPerObjectLights, NULL);
	
	forwardLightIndices.reserve(visibleNodes.size() * 3);
	forwardLightOffsets.resize_uninitialized(visibleNodes.size() + 1);
	
	const bool dualLightmapsMode = (GetLightmapSettings().GetLightmapsMode() == LightmapSettings::kDualLightmapsMode);
	const bool areLightProbesBaked = LightProbes::AreBaked();

	for( int i = 0; i < visibleNodes.size(); ++i )
	{
		const VisibleNode& node = visibleNodes[i];
		///@TODO: Should use SceneNode for this???
		Renderer* renderer = static_cast<Renderer*>(node.renderer);
		UInt32 layerMask = renderer->GetLayerMask();
		const bool isLightmapped = renderer->IsLightmappedForRendering();
		const bool isUsingLightProbes = renderer->GetRendererType() != kRendererIntermediate && renderer->GetUseLightProbes() && areLightProbesBaked;
		const bool directLightFromLightProbes = (renderPath == kRenderPathExtVertex) ? false : isUsingLightProbes && !dualLightmapsMode;
		
		forwardLightOffsets[i] = forwardLightIndices.size();
		CullPerObjectLights (lights, node.worldAABB, node.localAABB, node.worldMatrix, node.invScale, layerMask, isLightmapped || directLightFromLightProbes, dualLightmapsMode, forwardLightIndices);
	}
	
	forwardLightOffsets[visibleNodes.size()] = forwardLightIndices.size();
}

static void FindShadowCastingLights (ActiveLights& activeLights, ShadowedLights& shadowedLights)
{
	size_t index = 0;
	size_t endIndex = activeLights.numDirLights + activeLights.numSpotLights + activeLights.numPointLights;
	
	// Must reserve here otherwise the shadowlight pointer in activelight might become invalid...
	shadowedLights.reserve(endIndex);
	
	for ( ; index < endIndex; index++)
	{
		ActiveLight& light = activeLights.lights[index];
		if (light.isVisibleInPrepass && light.insideShadowRange && light.light->GetShadows() != kShadowNone)
		{
			ShadowedLight& shadowedLight = shadowedLights.push_back();
			shadowedLight.lightIndex = index;
			light.shadowedLight = &shadowedLight;
		}
		else
			light.shadowedLight = NULL;

	}
}

static void CullLights (const SceneCullingParameters& cullingParameters, CullResults& results)
{
	PROFILER_BEGIN(gCullActiveLights, NULL)
	FindAndCullActiveLights(cullingParameters, *results.shadowCullData, results.activeLights);
	PROFILER_END
	
	///@TODO: This is not very awesome, we now cache transform info twice...
	// Figure out how to not do that...
	CacheTransformInfo (results.nodes);
	
	if (cullingParameters.cullPerObjectLights)
		CullAllPerObjectLights (results.nodes, results.activeLights, cullingParameters.renderPath, results.forwardLightIndices, results.forwardLightOffsets);

	// Shadows might be disabled in quality setting
	if (GetQualitySettings().GetCurrent().shadows == QualitySettings::kShadowsDisable)
		return;
	
	FindShadowCastingLights (results.activeLights, results.shadowedLights);
}


static void CullSendEvents (CullResults& results, dynamic_array<BaseRenderer*>& needsCullCallback)
{
	// Send OnBecameVisible / OnBecameInvisible callback
	GetScene().NotifyVisible (results.sceneCullingOutput);
	
	// Invoke renderer callbacks
	// Invoke renderer callbacks (Only scene visible objects)
	InvokeOnWillRenderObject(needsCullCallback);
	
	// Cache the transform info.
	// We do this after OnWillRenderObject since OnWillRenderObject might modify the transform positions.
	// For example the pre-mecanim animation system will sample animations in OnWillRenderObject when using animation culling.
	// Which can result in a change to transform. pos / rot / scale
	CacheTransformInfo(results.nodes);
}


void CullScene (SceneCullingParameters& cullingParameters, CullResults& results)
{
	PROFILER_AUTO(gCulling, NULL);
	
	CullingOutput& cullingOutput = results.sceneCullingOutput;
	
	if (cullingParameters.useOcclusionCulling)
	{
		PROFILER_AUTO(gCullScene, NULL);
		// Cull the scene, outputs index lists of visible renderers
		CullSceneWithUmbra( cullingParameters, cullingOutput );
	}
	else
	{
		PROFILER_AUTO(gCullScene, NULL);
		// Cull the scene, outputs index lists of visible renderers
		CullSceneWithoutUmbra( cullingParameters, cullingOutput );
	}

	dynamic_array<BaseRenderer*> needsCullCallback (kMemTempAlloc);
	
	
	// Extract visible nodes & the objects needing culling callbacks from the visible indices
	CullFiltering cullFiltering = cullingParameters.filterMode;
	for (int i=0;i<kVisibleListCount;i++)
		PrepareSceneNodes(cullingOutput.visible[i], cullFiltering, cullingParameters.renderers[i].nodes, results.nodes, needsCullCallback);

	if (cullingParameters.cullLights)
		CullLights(cullingParameters, results);

	CullSendEvents(results, needsCullCallback);
}

void CullIntermediateRenderersOnly (const SceneCullingParameters& cullingParameters, CullResults& results)
{
	Assert(cullingParameters.renderers[kDynamicRenderer].nodes == NULL);
	
	const RendererCullData& cullData = cullingParameters.renderers[kCameraIntermediate];
	for( size_t i = 0; i < cullData.rendererCount; ++i )
	{
		BaseRenderer* r = cullData.nodes[i].renderer;
		Assert (r);
		
		VisibleNode& visibleNode = results.nodes.push_back ();
		visibleNode.renderer = r;
		visibleNode.lodFade = 0.0F;
		
		visibleNode.SetTransformInfo (r->GetTransformInfo());
	}
	
	if (cullingParameters.cullLights)
		CullLights(cullingParameters, results);
}

///* Fix Lightmanager::FindActiveLights to not be ugly
///* Move actual culling functions out of Lightmanager into their own LightCulling.cpp
///* Make Terrains and terrain shadow culling work...