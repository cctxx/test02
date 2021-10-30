#include "UnityPrefix.h"
#include "ReplacementRenderLoop.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/BaseRenderer.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/Camera.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/UnityScene.h"
#include "Runtime/GfxDevice/GfxDevice.h"


struct RODataReplacement {
	float				distance;
	int					subshaderIndex;
	Material*			material;
	const VisibleNode*  visibleNode;
	Shader*				shader;
	int					materialIndex;
	GlobalLayeringData  globalLayeringData;
};

typedef UNITY_TEMP_VECTOR(RODataReplacement) RenderObjects;

struct ROSorterReplacement {
	bool operator()( const RODataReplacement& ra, const RODataReplacement& rb ) const;
};


bool ROSorterReplacement::operator()( const RODataReplacement& ra, const RODataReplacement& rb ) const
{
	// Sort by layering depth. //@TODO:should this be here?
	bool globalLayeringResult;
	if (CompareGlobalLayeringData(ra.globalLayeringData, rb.globalLayeringData, globalLayeringResult))
		return globalLayeringResult;
	
	// Sort by subshader index used
	if (ra.subshaderIndex != rb.subshaderIndex)
		return ra.subshaderIndex < rb.subshaderIndex;
	
	// Sort front to back
	return ra.distance > rb.distance;
}


static inline float EvaluateObjectDepth (const Matrix4x4f& cameraMatrix, const TransformInfo& info)
{
	Vector3f center = info.worldAABB.GetCenter();
	float d = cameraMatrix.MultiplyPoint3( center ).z;
	Assert(IsFinite(d));
	return d;
}


static void PerformRenderingReplacement (Camera& camera, const Matrix4x4f& curCameraMatrix, RenderObjects& renderData)
{
	// Sort
	std::sort (renderData.begin(), renderData.end(), ROSorterReplacement());
	
	
	GfxDevice& device = GetGfxDevice();
	size_t ndata = renderData.size();
	device.SetViewMatrix (curCameraMatrix.GetPtr());

	for( size_t i = 0; i < ndata; ++i )
	{
		const RODataReplacement& roData = renderData[i];
		
		const VisibleNode* node = roData.visibleNode;
		Assert (node);
		BaseRenderer* renderer = node->renderer;
		Assert (renderer);
		Shader* shader = roData.shader;
		
		device.SetInverseScale(1.0f);

		//@TODO: if this returns true and we have any sort of batching, we'd have to break batches here
		renderer->ApplyCustomProperties(*roData.material, shader, roData.subshaderIndex);

		ShaderLab::SubShader& subshader = roData.shader->GetShaderLabShader()->GetSubShader (roData.subshaderIndex);
		int shaderPassCount = subshader.GetValidPassCount();
		for (int p = 0; p < shaderPassCount; ++p)
		{	
			const ChannelAssigns* channels = roData.material->SetPassWithShader(p, shader, roData.subshaderIndex);
			if (channels)
			{
				SetupObjectMatrix (node->worldMatrix, node->transformType);
				renderer->Render( renderer->GetSubsetIndex(roData.materialIndex), *channels );
			}
		}
	}
}

static void AddReplacementObject (
								  RenderObjects& renderObjects,
								  Material* mat,
								  Shader* replacementShader,
								  bool noReplacementTag,
								  int replacementTagID,
								  const VisibleNode* visibleNode,
								  float distanceForSort,
								  int materialIndex,
								  GlobalLayeringData globalLayeringData

								  )
{
	if( mat == NULL )
		mat = Material::GetDefault();
	Shader *shader = mat->GetShader();
	
	// Note: do not check whether object is in geometry queue range,
	// let shader replacement handle that. E.g. terrain billboard shaders are actually
	// beyond geometry queue, but still can output meaningful depth/normals information.
	
	// Handle shader replacement
	// Given a replacement shader and tag name:
	// 1. if tag name is empty, then all objects are just rendered with replacement shader's first subshader
	// 2. if tag name is given:
	//    * real object's subshader is queried for tag value.
	//    * if it does not have that tag, the object is not rendered.
	//    * subshader is found in the replacement shader, that has given tag with the given value. If no subshader found, object is not rendered.
	//    * that subshader is used instead to render the object.
	int usedSubshaderIndex;
	if (noReplacementTag)
	{
		usedSubshaderIndex = 0;
	}
	else
	{
		int subshaderTypeID = shader->GetShaderLabShader()->GetTag (replacementTagID, true);
		if (subshaderTypeID < 0)
			return; // skip rendering
		usedSubshaderIndex = replacementShader->GetSubShaderWithTagValue (replacementTagID, subshaderTypeID);
		if (usedSubshaderIndex == -1)
			return; // skip rendering
	}
	
	renderObjects.push_back(RODataReplacement());
	RODataReplacement& roData = renderObjects.back();
	roData.visibleNode = visibleNode;
	roData.distance = distanceForSort;
	
	DebugAssertIf( !mat );
	roData.material = mat;
	roData.materialIndex = materialIndex;
	
	roData.shader = replacementShader;
	roData.subshaderIndex = usedSubshaderIndex;
	
	roData.globalLayeringData = globalLayeringData;
}

void RenderSceneShaderReplacement (const VisibleNodes& contents, Shader* shader, const std::string& shaderReplaceTag)
{
	ShaderReplaceData replaceData;
	replaceData.replacementShader = shader;
	replaceData.replacementTagSet = !shaderReplaceTag.empty();
	replaceData.replacementTagID = ShaderLab::GetShaderTagID(shaderReplaceTag);
	
	RenderSceneShaderReplacement(contents, replaceData);
}

	
void RenderSceneShaderReplacement (const VisibleNodes& contents, const ShaderReplaceData& shaderReplace)
{
	Assert (shaderReplace.replacementShader != NULL);

	const bool noReplacementTag = !shaderReplace.replacementTagSet;
	const int replacementTagID = shaderReplace.replacementTagID;
	Shader* replacementShader = shaderReplace.replacementShader;
	Camera& camera = GetRenderManager().GetCurrentCamera();
	Matrix4x4f curCameraMatrix = camera.GetWorldToCameraMatrix();	
	
	RenderObjects renderObjects;
	renderObjects.reserve (contents.size()/4);

	// Go over the objects
	for( VisibleNodes::const_iterator i = contents.begin(); i != contents.end(); ++i )
	{
		float distanceForSort = EvaluateObjectDepth (curCameraMatrix, *i);
		
		const BaseRenderer* renderer = i->renderer;
		
		int matCount = renderer->GetMaterialCount();
		for (int mi = 0; mi < matCount; ++mi)
		{
			Material* mat = renderer->GetMaterial(mi);
			AddReplacementObject (
								  renderObjects,
								  mat,
								  replacementShader,
								  noReplacementTag,
								  replacementTagID,
								  &*i,
								  distanceForSort,
								  mi,
								  renderer->GetGlobalLayeringData()
								  );
		}
	}
	
	// Render
	PerformRenderingReplacement (camera, curCameraMatrix, renderObjects);
}

void RenderSceneShaderReplacement (const RenderObjectDataContainer& contents, Shader* replacementShader, const std::string& replacementTag)
{
	Assert (replacementShader);

	const bool noReplacementTag = replacementTag.empty();
	const int replacementTagID = ShaderLab::GetShaderTagID(replacementTag);

	Camera& camera = GetRenderManager().GetCurrentCamera();
	Matrix4x4f curCameraMatrix = camera.GetWorldToCameraMatrix();	

	RenderObjects renderObjects;
	renderObjects.reserve (contents.size()/4);

	// Go over the objects
	for (RenderObjectDataContainer::const_iterator i = contents.begin(); i != contents.end(); ++i)
	{
		const RenderObjectData& ro = *i;
		const BaseRenderer* renderer = ro.visibleNode->renderer;
		Assert (renderer);
		Material* mat = renderer->GetMaterial(ro.sourceMaterialIndex);
		AddReplacementObject (
			renderObjects,
			mat,
			replacementShader,
			noReplacementTag,
			replacementTagID,
			ro.visibleNode,
			ro.distance,
			ro.sourceMaterialIndex,
			renderer->GetGlobalLayeringData()
			);
	}

	// Render
	PerformRenderingReplacement (camera, curCameraMatrix, renderObjects);
}

