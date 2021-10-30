#include "UnityPrefix.h"
#include "LightManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Light.h"
#include "UnityScene.h"
#include "CullResults.h"
#include "BaseRenderer.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/SphericalHarmonics.h"
#include "Runtime/Shaders/ShaderKeywords.h"
#include "Runtime/Camera/LightProbes.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Misc/BuildSettings.h"

static ShaderKeyword kKeywordVertexLight = keywords::Create("VERTEXLIGHT_ON");

static LightManager* s_LightManager = NULL;

LightManager& GetLightManager()
{
	DebugAssert (s_LightManager != NULL);
	return *s_LightManager;
}

void LightManager::InitializeClass()
{
	DebugAssert (s_LightManager == NULL);
	s_LightManager = new LightManager();
}

void LightManager::CleanupClass()
{
	DebugAssert (s_LightManager != NULL);
	delete s_LightManager;
	s_LightManager = NULL;
}




static float CalculateLightIntensityAtPoint (const Light& light, const Vector3f& position)
{
	float lum = light.GetColor().GreyScaleValue() * light.GetIntensity();

	if (light.GetType () == kLightDirectional)
	{
		if (light.GetShadows() != kShadowNone)
			return lum * 16.0f;
		else
			return lum;
	}
	else
	{
		float sqrDistance = SqrMagnitude (position - light.GetWorldPosition());
		float atten = light.AttenuateApprox (sqrDistance);
		return lum * atten;
	}
}


static ColorRGBAf CalculateLightColorAtPointForSH (const Light& light, const AABB& bounds, const AABB& localBounds, float scale)
{
	float lum = light.GetIntensity();

	if (light.GetType () == kLightDirectional)
	{
		return lum * GammaToActiveColorSpace (light.GetColor());
	}
	else
	{
		// objects larger than lights should be affected less
		float objectSizeSqr = SqrMagnitude(localBounds.GetExtent() * scale);
		float lightSizeSqr = light.GetRange() * light.GetRange();
		if (objectSizeSqr > lightSizeSqr)
		{
			lum *= lightSizeSqr / objectSizeSqr;
		}

		Vector3f pos = bounds.GetCenter();
		// don't ever treat SH lights as coming closer to center than at object bounds
		float sqrDistance = std::max (SqrMagnitude (pos - light.GetWorldPosition()), objectSizeSqr);
		float atten = light.AttenuateApprox (sqrDistance);
		return (lum * atten) * GammaToActiveColorSpace (light.GetColor());
	}
}


LightManager::LightManager ()
:	m_LastMainLight(NULL)
{
}


LightManager::~LightManager () {
}




void LightManager::SetupVertexLights (int lightCount, const ActiveLight* const* lights)
{
	GfxDevice& device = GetGfxDevice();

	int maxVertexLights = gGraphicsCaps.maxLights;
	AssertIf(lightCount > maxVertexLights);
	lightCount = std::min(lightCount, maxVertexLights); // just a safeguard, should not normally happen

	// setup vertex lights
	for( int i = 0; i < lightCount; ++i )
		lights[i]->light->SetupVertexLight (i, lights[i]->visibilityFade);

	// disable rest of lights
	device.DisableLights( lightCount );
}

void SetSHConstants (const float sh[9][3], BuiltinShaderParamValues& params)
{
	Vector4f vCoeff[3];

	static const float s_fSqrtPI = ((float)sqrtf(kPI));
	const float fC0 = 1.0f/(2.0f*s_fSqrtPI);
	const float fC1 = (float)sqrt(3.0f)/(3.0f*s_fSqrtPI);
	const float fC2 = (float)sqrt(15.0f)/(8.0f*s_fSqrtPI);
	const float fC3 = (float)sqrt(5.0f)/(16.0f*s_fSqrtPI);
	const float fC4 = 0.5f*fC2;

	int iC;
	for( iC=0; iC<3; iC++ )
	{
		vCoeff[iC].x = -fC1*sh[3][iC];
		vCoeff[iC].y = -fC1*sh[1][iC];
		vCoeff[iC].z =  fC1*sh[2][iC];
		vCoeff[iC].w =  fC0*sh[0][iC] - fC3*sh[6][iC];
	}

	params.SetVectorParam(kShaderVecSHAr, vCoeff[0]);
	params.SetVectorParam(kShaderVecSHAg, vCoeff[1]);
	params.SetVectorParam(kShaderVecSHAb, vCoeff[2]);

	for( iC=0; iC<3; iC++ )
	{
		vCoeff[iC].x =      fC2*sh[4][iC];
		vCoeff[iC].y =     -fC2*sh[5][iC];
		vCoeff[iC].z = 3.0f*fC3*sh[6][iC];
		vCoeff[iC].w =     -fC2*sh[7][iC];
	}

	params.SetVectorParam(kShaderVecSHBr, vCoeff[0]);
	params.SetVectorParam(kShaderVecSHBg, vCoeff[1]);
	params.SetVectorParam(kShaderVecSHBb, vCoeff[2]);

	vCoeff[0].x = fC4*sh[8][0];
	vCoeff[0].y = fC4*sh[8][1];
	vCoeff[0].z = fC4*sh[8][2];
	vCoeff[0].w = 1.0f;

	params.SetVectorParam(kShaderVecSHC, vCoeff[0]);
}

void LightManager::SetupForwardBaseLights (const ForwardLightsBlock& lights)
{
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();

	// Setup SH constants
	SetSHConstants (lights.sh, params);

	// Setup per-vertex light constants
	Assert (lights.vertexLightCount <= 4);
	Vector4f packedPosX;
	Vector4f packedPosY;
	Vector4f packedPosZ;
	Vector4f packedAtten;
	Vector4f lightColors[4];

	const ActiveLight* const* vertexLights = lights.GetLights() + lights.addLightCount;
	for (int i = 0; i < lights.vertexLightCount; ++i)
	{
		Light& light = *vertexLights[i]->light;
		Vector3f pos = light.GetWorldPosition();
		float intensity = light.GetIntensity() * 2.0f;
		if (i == 0 && lights.lastAddLightBlend != 1.0f)
			intensity *= (1.0f-lights.lastAddLightBlend);
		else if (i == lights.vertexLightCount-1)
			intensity *= lights.lastVertexLightBlend;
		ColorRGBAf color = GammaToActiveColorSpace (light.GetColor()) * intensity;
		float attenSq = Light::CalcQuadFac (light.GetRange());

		packedPosX.GetPtr()[i] = pos.x;
		packedPosY.GetPtr()[i] = pos.y;
		packedPosZ.GetPtr()[i] = pos.z;
		packedAtten.GetPtr()[i] = attenSq;
		lightColors[i].Set (color.r, color.g, color.b, color.a);
	}
	for (int i = lights.vertexLightCount; i < kMaxForwardVertexLights; ++i)
	{
		packedPosX.GetPtr()[i] = 0;
		packedPosY.GetPtr()[i] = 0;
		packedPosZ.GetPtr()[i] = 0;
		packedAtten.GetPtr()[i] = 1;
		lightColors[i].Set (0,0,0,0);
	}
	if (lights.vertexLightCount)
	{
		params.SetVectorParam(kShaderVecVertexLightPosX0, packedPosX);
		params.SetVectorParam(kShaderVecVertexLightPosY0, packedPosY);
		params.SetVectorParam(kShaderVecVertexLightPosZ0, packedPosZ);
		params.SetVectorParam(kShaderVecVertexLightAtten0, packedAtten);
		params.SetVectorParam(kShaderVecLight0Diffuse, lightColors[0]);
		params.SetVectorParam(kShaderVecLight1Diffuse, lightColors[1]);
		params.SetVectorParam(kShaderVecLight2Diffuse, lightColors[2]);
		params.SetVectorParam(kShaderVecLight3Diffuse, lightColors[3]);

		g_ShaderKeywords.Enable (kKeywordVertexLight);
	}
	else
	{
		g_ShaderKeywords.Disable (kKeywordVertexLight);
	}


	// Setup per-pixel main light constants
	if (lights.mainLight == NULL)
	{
		params.SetVectorParam(kShaderVecLightColor0, Vector4f(0,0,0,0));
		return;
	}

	Light* light = lights.mainLight->light;
	Assert (light->GetType() == kLightDirectional);

	const Transform& transform = light->GetComponent(Transform);

	Vector3f lightDir = transform.TransformDirection (Vector3f(0,0,-1));
	params.SetVectorParam(kShaderVecWorldSpaceLightPos0, Vector4f(lightDir.x, lightDir.y, lightDir.z, 0.0f));

	Matrix4x4f world2Light = light->GetWorldToLocalMatrix();
	light->GetMatrix (&world2Light, &params.GetWritableMatrixParam(kShaderMatLightMatrix));

	light->SetLightKeyword ();
	light->SetPropsToShaderLab (1.0f);
}

void LightManager::SetupForwardAddLight (Light* light, float blend)
{
	Assert (light);

	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();
	Vector4f lightInfo;
	if (light->GetType() != kLightDirectional)
	{
		Vector3f worldPos = light->GetWorldPosition();
		lightInfo.Set (worldPos.x, worldPos.y, worldPos.z, 1.0f);
	}
	else
	{
		const Transform& transform = light->GetComponent(Transform);
		Vector3f worldDir = transform.TransformDirection (Vector3f(0,0,-1));
		lightInfo.Set (worldDir.x, worldDir.y, worldDir.z, 0.0f);
	}
	params.SetVectorParam(kShaderVecWorldSpaceLightPos0, lightInfo);

	Matrix4x4f world2Light = light->GetWorldToLocalMatrix();
	light->GetMatrix (&world2Light, &params.GetWritableMatrixParam(kShaderMatLightMatrix));

	light->SetLightKeyword ();
	light->SetPropsToShaderLab (blend);
}



void LightManager::AddLight (Light* source)
{
	DebugAssert (source);

	m_Lights.push_back(*source);
}

void LightManager::RemoveLight (Light* source)
{
	DebugAssert (source && source->IsInList());

	m_Lights.erase(source);

	// If this was the last main light, clear it
	if (m_LastMainLight == source)
		m_LastMainLight = NULL;
}

static float kRenderModeSortBias[Light::kRenderModeCount] = {
	0.0f,
	1000.0f,
	-1000.0f,
};




// Figures out lights for given object, sorts them by intensity.
// Fills them into an array of CulledLight.
// Returns number of lights filled.

struct CulledLight
{
	UInt32 lightIndex;
	float sortIntensity;
	friend bool operator< (const CulledLight& lhs, const CulledLight& rhs)
	{
		return lhs.sortIntensity > rhs.sortIntensity;
	}
};

static void SortLights (dynamic_array<CulledLight>& outLights, const UInt32* lightIndices, UInt32 lightCount, const ActiveLights& activeLights, const Vector3f& objectCenter)
{
	outLights.resize_uninitialized(lightCount);
	for (size_t i = 0; i < lightCount; i++)
	{
		// Light passed culling, add it
		CulledLight& outLight = outLights[i];
		outLight.lightIndex = lightIndices[i];
		const Light& light = *activeLights.lights[outLight.lightIndex].light;
		int lightRenderMode = light.GetRenderMode ();
		float blend = CalculateLightIntensityAtPoint (light, objectCenter);
		outLight.sortIntensity = blend + kRenderModeSortBias[lightRenderMode];
	}

	// Sort culled lights by intensity (most intensive lights first)
	std::sort( outLights.begin(), outLights.end() );
}

UNITY_VECTOR(kMemRenderer, Light*) LightManager::GetLights(LightType type, int layer)
{
	UNITY_VECTOR(kMemRenderer, Light*) lights;
	layer = 1 << layer;
	for (LightManager::Lights::iterator i= m_Lights.begin();i != m_Lights.end();i++)
	{
		Light* light = &*i;
		if (!light)
			continue;
		if (light->GetType() == type && (light->GetCullingMask() & layer) != 0)
			lights.push_back(light);
	}

	return lights;
}

void LightManager::FindVertexLightsForObject (dynamic_array<UInt8>& dest, const UInt32* lightIndices, UInt32 lightCount, const ActiveLights& activeLights, const VisibleNode& node)
{
	DebugAssert (node.renderer);

	dynamic_array<CulledLight> culledLights(kMemTempAlloc);
	SortLights (culledLights, lightIndices, lightCount, activeLights, node.worldAABB.GetCenter ());

	int resultLightCount = std::min<int> (lightCount, std::min<int>(gGraphicsCaps.maxLights,kMaxSupportedVertexLights));

	// allocate block to hold light count & light pointers
	size_t resultOffset = dest.size();
	size_t requiredSize = sizeof(VertexLightsBlock) + resultLightCount * sizeof(Light*);
	dest.resize_uninitialized(resultOffset + requiredSize);
	VertexLightsBlock* outBlock = reinterpret_cast<VertexLightsBlock*>(&dest[resultOffset]);
	const ActiveLight** outLights = reinterpret_cast<const ActiveLight**>(outBlock + 1);
	outBlock->lightCount = resultLightCount;
	for (int i = 0; i < resultLightCount; ++i)
		outLights[i] = &activeLights.lights[culledLights[i].lightIndex];
}



static void AddLightToSH (const VisibleNode& node, const Light& source, ForwardLightsBlock* lights, float blend)
{
	Vector3f lightDir;
	if (source.GetType() != kLightDirectional)
	{
		lightDir = NormalizeSafe(source.GetWorldPosition() - node.worldAABB.GetCenter());
	}
	else
	{
		const Transform& lightTransform = source.GetComponent (Transform);
		lightDir = lightTransform.TransformDirection (Vector3f(0,0,-1));
	}

	ColorRGBAf color = CalculateLightColorAtPointForSH (source, node.worldAABB, node.localAABB, 1.0f/node.invScale) * (blend * 2.0f);

	float shR[9], shG[9], shB[9];
	SHEvalDirectionalLight9 (
		lightDir.x, lightDir.y, lightDir.z,
		color.r, color.g, color.b,
		shR, shG, shB);
	for (int i = 0; i < 9; ++i) {
		lights->sh[i][0] += shR[i];
		lights->sh[i][1] += shG[i];
		lights->sh[i][2] += shB[i];
	}
}


// Light we want to blend is L1 (with L0 the brighter one before it, and L2 the dimmer one after it).
// Blend is: (L1-L2)/(L0-L2)
static inline bool CalculateLightBlend (const CulledLight* lights, int lightsCount, int index, float* outBlend)
{
	if (index <= 0 || index >= lightsCount-1)
		return false;

	float l0 = lights[index-1].sortIntensity;
	float l1 = lights[index  ].sortIntensity;
	float l2 = lights[index+1].sortIntensity;
	if (l0 - l2 >= kRenderModeSortBias[Light::kRenderImportant])
		return false;

	*outBlend = clamp01 ((l1 - l2) / (l0 - l2 + 0.001f));
	return true;
}


static void CrossBlendForwardLights (
									 CulledLight* culledLights,
									 const ActiveLights& activeLights,
									 int culledLightsCount,
									 int lastAutoAddLightIndex,
									 bool lightmappedObject,
									 dynamic_array<UInt8>& dest,
									 size_t resultOffset,
									 const VisibleNode& node
									)
{
	ForwardLightsBlock* lights = reinterpret_cast<ForwardLightsBlock*>(&dest[resultOffset]);
	int lastVertexLightIndex = lights->addLightCount + lights->vertexLightCount-1;

	// How much we need to blend the last additive light?
	lights->lastAddLightBlend = 1.0f;
	bool blendLastAddLight = CalculateLightBlend (culledLights, culledLightsCount, lastAutoAddLightIndex, &lights->lastAddLightBlend);
	if (blendLastAddLight && !lightmappedObject)
	{
		// For non-lightmapped objects, we need to add oppositely blended
		// vertex or SH light.
		UInt32 blendIndex = culledLights[lastAutoAddLightIndex].lightIndex;
		const Light& blendLight = *activeLights.lights[blendIndex].light;
		if (blendLight.GetType() != kLightDirectional)
		{
			// Non-directional light: insert one vertex light
			dest.resize_uninitialized(dest.size() + sizeof(Light*));
			lights = reinterpret_cast<ForwardLightsBlock*>(&dest[resultOffset]); // could result in reallocation; recalculate block pointer
			const ActiveLight** lightPtrs = reinterpret_cast<const ActiveLight**>(lights + 1); // ActiveLight array begins at end of struct
			// move all vertex lights
			for (int i = lights->addLightCount+lights->vertexLightCount-1; i >= lights->addLightCount-1; --i)
				lightPtrs[i+1] = lightPtrs[i];
			Assert (lights->vertexLightCount <= kMaxForwardVertexLights);
			++lights->vertexLightCount;
			if (lights->vertexLightCount > kMaxForwardVertexLights)
			{
				--lastVertexLightIndex;
				lights->vertexLightCount = kMaxForwardVertexLights;
			}
		}
		else
		{
			// Directional light: add to SH
			AddLightToSH (node, blendLight, lights, 1.0f-lights->lastAddLightBlend);
		}
	}

	// If we have vertex lights, we might want to blend last one
	if (lights->vertexLightCount > 0)
	{
		float vertexBlend;
		lights->lastVertexLightBlend = 1.0f;
		bool blendLastVertexLight = CalculateLightBlend (culledLights, culledLightsCount, lastVertexLightIndex, &vertexBlend);
		if (blendLastVertexLight)
			lights->lastVertexLightBlend = vertexBlend;
	}
}

static size_t CountNonOffscreenVertexLights (const UInt32* lightIndices, UInt32 lightCount, const ActiveLights& activeLights)
{
	size_t visibleLightCount = 0;
	for (visibleLightCount=0;visibleLightCount<lightCount;visibleLightCount++)
	{
		if (activeLights.lights[lightIndices[visibleLightCount]].isOffscreenVertexLight)
			break;
	}
	
	for (int j=visibleLightCount;j<lightCount;j++)
	{
		DebugAssert(activeLights.lights[lightIndices[j]].isOffscreenVertexLight);
	}
	
	
	return visibleLightCount;
}

void LightManager::FindForwardLightsForObject (dynamic_array<UInt8>& dest, const UInt32* lightIndices, UInt32 lightCount, const ActiveLights& activeLights, const VisibleNode& node, bool lightmappedObject, bool dualLightmapsMode, bool useVertexLights, int maxAutoAddLights, bool disableAddLights, const ColorRGBAf& ambient)
{
	DebugAssert (node.renderer);

	// cull and sort lights into temporary memory block
	Renderer* renderer = static_cast<Renderer*>(node.renderer);
	UInt32 layerMask = renderer->GetLayerMask();
	const bool isUsingLightProbes = node.renderer->GetRendererType() != kRendererIntermediate && renderer->GetUseLightProbes() && LightProbes::AreBaked();
	const bool directLightFromLightProbes = isUsingLightProbes && !dualLightmapsMode;
	
	dynamic_array<CulledLight> culledLights(kMemTempAlloc);
	
	// If we don't support vertex lights we can skip rendering any offscreen lights completely (offscreen lights always come after visible lights in the index list)
	if (!useVertexLights)
		lightCount = CountNonOffscreenVertexLights (lightIndices, lightCount, activeLights);
	
	
	SortLights (culledLights, lightIndices, lightCount, activeLights, node.worldAABB.GetCenter ());

	// put ForwardLightsBlock header structure into buffer
	size_t resultOffset = dest.size();
	size_t arrayOffset = resultOffset + sizeof(ForwardLightsBlock);
	dest.resize_uninitialized(arrayOffset);
	ForwardLightsBlock* lights = reinterpret_cast<ForwardLightsBlock*>(&dest[resultOffset]);
	lights->addLightCount = 0;
	lights->vertexLightCount = 0;
	lights->mainLight = NULL;
	lights->lastAddLightBlend = 1.0f;
	lights->lastVertexLightBlend = 1.0f;

	if (useVertexLights)
	{
		// if we want vertex lights as result, just take N brightest ones
		int resultLightCount = std::min<int> (lightCount, std::min<int>(gGraphicsCaps.maxLights,kMaxSupportedVertexLights));

		// set SH to zero (not really used for rendering, but having garbage there would break batches)
		memset (lights->sh, 0, sizeof(lights->sh));

		// allocate block to hold light pointers
		dest.resize_uninitialized(arrayOffset + sizeof(ActiveLight*) * resultLightCount);
		// could result in reallocation; recalculate block pointer
		lights = reinterpret_cast<ForwardLightsBlock*>(&dest[resultOffset]);
		const ActiveLight** lightPtrs = reinterpret_cast<const ActiveLight**>(lights + 1);
		lights->vertexLightCount = resultLightCount;
		for (int i = 0; i < resultLightCount; ++i)
			lightPtrs[i] = &activeLights.lights[culledLights[i].lightIndex];
	}
	else
	{
		// we want main light + SH + additive lights as result

		// Main light if any will be first one in activeLights
		const ActiveLight* mainLight = NULL;
		if (activeLights.hasMainLight )
			mainLight = &activeLights.lights[0];

		// Take globally main light if that fits our layer mask, it does not have a cookie, and it's not an auto light while we're using probes
		if (mainLight && (mainLight->cullingMask & layerMask) != 0 && !mainLight->hasCookie &&
			!(directLightFromLightProbes && mainLight->lightmappingForRender == Light::kLightmappingAuto))
			lights->mainLight = mainLight;

		// put ambient into SH
		SHEvalAmbientLight(ambient, &lights->sh[0][0]);
		for (int i = 1; i < 9; ++i) {
			lights->sh[i][0] = 0.0f;
			lights->sh[i][1] = 0.0f;
			lights->sh[i][2] = 0.0f;
		}

		// go over result lights and place them
		int lastAutoAddLightIndex = -1;
		for (int i = 0; i < lightCount; ++i)
		{
			UInt32 lightIndex = culledLights[i].lightIndex;
			const ActiveLight& activeLight = activeLights.lights[lightIndex];
			const int lightRenderMode = activeLight.lightRenderMode; 
			const int lightmappingMode = activeLight.lightmappingForRender;

			Assert(!activeLight.isOffscreenVertexLight);
		
			// BasePass for lightmapped objects has no code for run-time lighting
			// therefore we have to promote RuntimeOnly directional lights to Additive pass
			// (This bug was fixed in 3.5. Thus we keep behaviour in the webplayer)
			bool forceAdditive = lightmappedObject && lightmappingMode == Light::kLightmappingRealtimeOnly;
			forceAdditive &= IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_5_a1);
			
			// If this is already set as main light: skip it
			if (lights->mainLight && lightIndex == 0 && !forceAdditive)
			{
				// nothing
			}
			// If we don't have main light yet and this could be it: do it
			else if (lights->mainLight == NULL && activeLight.lightType == kLightDirectional && lightRenderMode != Light::kRenderNotImportant && !activeLight.hasCookie && !forceAdditive)
			{
				lights->mainLight = &activeLight;
			}
			// If it's an important light, add it to additive lights
			else if (((lightRenderMode == Light::kRenderImportant) || (lightRenderMode!=Light::kRenderNotImportant && lights->addLightCount < maxAutoAddLights)) && !disableAddLights)
			{
				// now that we know it will not be rendered as a vertex light, we can check if it's actually visible;
				// can't do that for vertex lights, as they influence object past the range
				size_t curOffset = dest.size();
				dest.resize_uninitialized(curOffset + sizeof(ActiveLight*));
				*reinterpret_cast<const ActiveLight**>(&dest[curOffset]) = &activeLight;
				// could result in reallocation; recalculate block pointer
				lights = reinterpret_cast<ForwardLightsBlock*>(&dest[resultOffset]);
				++lights->addLightCount;
				if (lights->addLightCount == maxAutoAddLights && lightRenderMode != Light::kRenderImportant)
					lastAutoAddLightIndex = i;
			}
			// All Vertex/SH lights for non-lightmapped objects
			else if (!lightmappedObject)
			{
				// Add some non-directional lights as vertex lights
				if (activeLight.lightType != kLightDirectional && lights->vertexLightCount < kMaxForwardVertexLights)
				{
					size_t curOffset = dest.size();
					dest.resize_uninitialized(curOffset + sizeof(Light*));
					*reinterpret_cast<const ActiveLight**>(&dest[curOffset]) = &activeLight;
					// could result in reallocation; recalculate block pointer
					lights = reinterpret_cast<ForwardLightsBlock*>(&dest[resultOffset]);
					++lights->vertexLightCount;
				}
				// Otherwise, add light to SH
				else
				{
					AddLightToSH (node, *activeLight.light, lights, 1.0f);
				}
			}
		}

		// Blend light transitions: full to vertex lit; and vertex lit to SH.
		CrossBlendForwardLights (culledLights.data(), activeLights, lightCount, lastAutoAddLightIndex, lightmappedObject, dest, resultOffset, node);

		if (isUsingLightProbes)
		{
			float coefficients[kLightProbeCoefficientCount];
			GetLightProbes()->GetInterpolatedLightProbe(renderer->GetLightProbeInterpolationPosition(node.worldAABB), renderer, &coefficients[0]);
			for (int i = 0; i < kLightProbeCoefficientCount; i++)
			{
				lights->sh[0][i] += coefficients[i];
			}
		}
	}
}
