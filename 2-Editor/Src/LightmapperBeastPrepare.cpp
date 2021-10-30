#include "UnityPrefix.h"

#if ENABLE_LIGHTMAPPER

#include "LightmapperBeast.h"

#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Camera/UnityScene.h"
#include "Runtime/Camera/RenderManager.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Terrain/TerrainRenderer.h"
#include "ExtractTerrainMesh.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Application.h"
#include "AssetPipeline/AssetPathUtilities.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Editor/Src/ProjectWizardUtility.h"
#include "Runtime/Graphics/LightProbeGroup.h"
#include "Runtime/Graphics/LightProbeGroup.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Geometry/TextureAtlas.h"
#include "Runtime/Interfaces/ITerrainManager.h"

#include "External/Beast/builds/include/beastapi/beastmanager.h"
#include "External/Beast/builds/include/license.h"
#include "External/Beast/builds/include/beastapi/beastscene.h"
#include "External/Beast/builds/include/beastapi/beastmesh.h"
#include "External/Beast/builds/include/beastapi/beastmaterial.h"
#include "External/Beast/builds/include/beastapi/beastinstance.h"
#include "External/Beast/builds/include/beastapi/beastlightsource.h"
#include "External/Beast/builds/include/beastapi/beastrenderpass.h"
#include "External/Beast/builds/include/beastapi/beasttarget.h"
#include "External/Beast/builds/include/beastapi/beasttargetentity.h"
#include "External/Beast/builds/include/beastapi/beasttexture.h"
#include "External/Beast/builds/include/beastapi/beastpointcloud.h"


#include <fstream>
#include "SimpleXMLWriter.h"

using namespace std;
using namespace BeastUtils;

const char* kExportingScene = "Exporting to Beast";

const float kBeastStart = 0.0f;
const float kBeastMeshRenderers = 0.2f;
const float kBeastSkeletalMeshRenderers = 0.4f;
const float kBeastTerrains = 0.45f;
const float kBeastLightProgress = 0.7f;
const float kBeastLightProbeProgress = 0.8f;
const float kBeastCreatingJob = 0.9f;

float GetProgressPercentage (float current, float total, float blockStart, float blockEnd)
{
	return blockStart + ((blockEnd - blockStart) * (current / total));
}

void LightmapRenderingPasses::CreateRenderPass(ILBJobHandle job)
{
	m_LightmapsMode = GetLightmapSettings().GetLightmapsMode();
	if (m_LightmapsMode == LightmapSettings::kDualLightmapsMode)
		BeastCall(ILBCreateIlluminationPass(job, _T("DualLightmapsPass"), ILB_IM_FULL_AND_INDIRECT, &m_RenderPass));
	else if (m_LightmapsMode == LightmapSettings::kSingleLightmapsMode)
		BeastCall(ILBCreateIlluminationPass(job, _T("SingleLightmapsPass"), ILB_IM_FULL, &m_RenderPass));
	else // kRNMMode || kDirectionalLightmapsMode
	{
		BeastCall(ILBCreateRNMPass(job, _T("RNMPass"), ILB_IM_FULL, 0, ILB_RB_HL2, &m_RenderPass));

		// Allow for negative values in RNMs, since only that allows to properly reconstruct lighting.
		BeastCall(ILBSetAllowNegative(m_RenderPass, ILB_AN_ALLOW));
	}
	
	LightmapEditorSettings& les = GetLightmapEditorSettings();
	if (les.GetAOAmount() > 0.0f)
	{
		BeastCall(ILBCreateAmbientOcclusionPass(job, _T("AmbientOcclusionPass"), les.GetAOMaxDistance(), 180.0f, &m_RenderPassAO));
		BeastCall(ILBSetAOContrast(m_RenderPassAO, les.GetAOContrast(), 1.0f));
		m_AOPassAmount = les.GetAOAmount();
	}
	m_AmbientLight = GetRenderSettings().GetAmbientLightInActiveColorSpace();
}
void LightmapRenderingPasses::CreateRenderPassLightProbes(ILBJobHandle job)
{
	m_LightmapsMode = GetLightmapSettings().GetLightmapsMode();
	BeastCall(ILBCreateIlluminationPassSH(job, _T("LightProbesPass"), ILB_IM_FULL_AND_INDIRECT, &m_RenderPassLightProbes));
}

void LightmapRenderingPasses::AddRenderPassToTarget(ILBTargetHandle target)
{
	BeastCall(ILBAddPassToTarget(target, m_RenderPass));
	if (m_AOPassAmount > 0)
		BeastCall(ILBAddPassToTarget(target, m_RenderPassAO));
}

void LightmapRenderingPasses::AddRenderPassLightProbesToTarget(ILBTargetHandle target)
{
	BeastCall(ILBAddPassToTarget(target, m_RenderPassLightProbes));
}

// ----------------------------------------------------------------------
//  BeastUtils
// ----------------------------------------------------------------------
inline void UnpackNormals (ColorRGBA32* normals, int count)
{
	const float kOneOver255 = 1.0f / 255.0f;

	for (int i = 0; i < count; i++)
	{
		// bring the red value back from alpha and keep green
		normals[i].r = normals[i].a;
		normals[i].a = 255;

		// calculate blue
		float x = ((float)normals[i].r * kOneOver255) * 2.0f - 1.0f;
		float y = ((float)normals[i].g * kOneOver255) * 2.0f - 1.0f;
		normals[i].b = (UInt8)(clamp(sqrt(1.0f - x * x - y * y) * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
	}
}

inline ILBTextureHandle BeastUtils::Texture2DToBeastTexture(Texture2D& texture, ILBManagerHandle bm, ILBPixelFormat beastFormat)
{
	ILBTextureHandle texBeast;
	std::string texName = Format("Texture-%i", texture.GetInstanceID());

	// check for the texture in the cache
	if (ILBFindTexture(bm, texName.c_str(), &texBeast) == ILB_ST_SUCCESS)
		return texBeast;

	// texture not in the cache - create it
	const int width = texture.GetDataWidth();
	const int height = texture.GetDataHeight();
	BeastCall(ILBBeginTexture(bm, texName.c_str(), width, height, beastFormat, &texBeast));
	BeastCall(ILBSetInputGamma(texBeast, ILB_IG_GAMMA, 2.2f));

	// copy the data
	if (beastFormat == ILB_PF_RGBA_BYTE)
	{
		dynamic_array<ColorRGBA32> texPixels;
		texPixels.reserve(width*height);
		texture.GetPixels32(0, texPixels.begin());
		if (texture.GetUsageMode() == kTexUsageNormalmapDXT5nm)
		{
			UnpackNormals (texPixels.data (), width*height);
		}
		BeastCall(ILBAddPixelDataLDR(texBeast, reinterpret_cast<UInt8*>(texPixels.begin()), width*height));
	}
	else if(beastFormat == ILB_PF_MONO_BYTE)
	{
		Image image(width, height, kTexFormatAlpha8);
		texture.ExtractImage(&image);
		UInt8* texData = image.GetImageData();
		BeastCall(ILBAddPixelDataLDR(texBeast, texData, width*height));
	}
	else
	{
		throw BeastException("Unsupported Beast image format. Supported formats are ILB_PF_RGBA_BYTE and ILB_PF_MONO_BYTE.");
	}

	// end texture
	BeastCall(ILBEndTexture(texBeast));

	return texBeast;
}

inline void BeastUtils::SetSpotlightCookie(const Light& light, ILBLightHandle beastLight, ILBManagerHandle bm)
{
	Texture2D* texture = dynamic_pptr_cast <Texture2D*>(light.GetCookie());
	if (!texture)
		texture = GetRenderSettings().GetDefaultSpotCookie();
	if (!texture)
	{
		WarningStringObject(Format("Unable to get the default cookie for the spot light %s", light.GetName()), &light);
		return;
	}

	ILBTextureHandle texBeast = BeastUtils::Texture2DToBeastTexture(*texture, bm, ILB_PF_MONO_BYTE);

	// set the texture as a cookie for the beast spotlight
	BeastCall(ILBSetLightProjectedTexture(beastLight, texBeast));
}

inline void BeastUtils::SetLightRamp(const Light& light, ILBLightHandle beastLight)
{
	// TODO: cache this stuff or smth
	// match Beast's light falloff to Unity's:
	const float nrSamples = 100;
	for (int i = 0; i <= (int)nrSamples; ++i) {
		float dist = i/nrSamples;
		float atten = Light::AttenuateNormalized(dist*dist);
		ILBLinearRGB attenRGB(atten, atten, atten);
		ILBSetLightRampEntry(beastLight, dist, &attenRGB);
	}
}

inline bool BeastUtils::ValidateUVs(Vector2f* uv, int vertexCount)
{
	for(int i = 0; i < vertexCount; i++)
	{
		if(uv[i].x < 0.0f || uv[i].x > 1.0f || uv[i].y < 0.0f || uv[i].y > 1.0f)
			return false;
	}
	return true;
}

void BeastUtils::ClampAlphaForCutout(Texture2D& textureRGBA, float cutoff, Texture2D& result)
{
	int width = textureRGBA.GetDataWidth();
	int height = textureRGBA.GetDataHeight();

	Image imageRGBA(width, height, kTexFormatRGBA32);
	textureRGBA.ExtractImage(&imageRGBA);
	UInt8* dataRGBA = imageRGBA.GetImageData();

	result.InitTexture(width, height, kTexFormatRGBA32);
	ImageReference ref;
	result.GetWriteImageReference(&ref, 0, 0);
	UInt8* data = ref.GetImageData();
	
	UInt8 cutoff8 = cutoff*255.0f;

	for( int i = 0; i < width*height*4; i+=4)
	{
		UInt8 alpha = dataRGBA[i + 3] > cutoff8 ? 0 : 255;
		data[i + 0] = alpha;
		data[i + 1] = alpha;
		data[i + 2] = alpha;
		data[i + 3] = alpha;
	}
}

void BeastUtils::CombineTextureAndColor(Texture2D& textureRGBA, ColorRGBAf& color, Texture2D& result, bool useOnlyAlphaFromTexture)
{
	int width = textureRGBA.GetDataWidth();
	int height = textureRGBA.GetDataHeight();

	// extract textureRGBA into result texture
	result.InitTexture(width, height, kTexFormatRGBA32);
	ImageReference ref;
	result.GetWriteImageReference(&ref, 0, 0);
	textureRGBA.ExtractImage(&ref);

	ColorRGBA32 color32(color);
	ColorRGBA32* resultColor = reinterpret_cast<ColorRGBA32*>(ref.GetImageData());

	if (useOnlyAlphaFromTexture)
	{
		for( int i = 0; i < width*height; i++)
		{
			*resultColor = color32 * (*resultColor).a;
			resultColor++;
		}
	}
	else
	{
		for( int i = 0; i < width*height; i++)
		{
			*resultColor *= color32;
			resultColor++;
		}
	}
}

void BeastUtils::CombineTextures(Texture2D& textureRGB, Texture2D& textureA, ColorRGBAf& color, Texture2D& result)
{
	int width = textureRGB.GetDataWidth();
	int height = textureRGB.GetDataHeight();

	Image imageRGB(width, height, kTexFormatRGB24);
	textureRGB.ExtractImage(&imageRGB);
	UInt8* dataRGB = imageRGB.GetImageData();

	Image imageA(width, height, kTexFormatAlpha8);
	textureA.ExtractImage(&imageA);
	UInt8* dataA = imageA.GetImageData();

	result.InitTexture(width, height, kTexFormatRGB24);
	ImageReference ref;
	result.GetWriteImageReference(&ref, 0, 0);
	UInt8* data = ref.GetImageData();

	ColorRGBA32 color32(color);
	
	for( int rgb = 0, a = 0; a < width*height; rgb+=3, a++)
	{
		UInt8 alpha = dataA[a];
		data[rgb + 0] = dataRGB[rgb + 0] * alpha * color32.r / (255 * 255);
		data[rgb + 1] = dataRGB[rgb + 1] * alpha * color32.g / (255 * 255);
		data[rgb + 2] = dataRGB[rgb + 2] * alpha * color32.b / (255 * 255);
	}
}

Vector3f BeastUtils::GetTreeInstancePosition(const TreeInstance& instance, const Vector3f& terrainSize, const Vector3f& terrainPosition)
{
	return Scale(instance.position, terrainSize) + terrainPosition;
}

void BeastUtils::HandleCastReceiveShadows(const ILBInstanceHandle& instance, bool castShadows)
{
	ILBRenderStatsMask stats = 0;
	if (!castShadows)
		stats |= ILB_RS_CAST_SHADOWS;
	
	BeastCall(ILBSetRenderStats(instance, stats, ILB_RSOP_DISABLE));
}

int BeastUtils::GetHighestLODLevel (Renderer& renderer)
{
	SceneHandle handle = renderer.GetSceneHandle();
	if (handle == kInvalidSceneHandle)
		return -1;

	const SceneNode& node = GetScene().GetRendererNode(handle);
	// No LOD
	if (node.lodIndexMask == 0)
		return -1;
	
	if (renderer.GetLODGroup() == NULL)
	{
		AssertString("Invalid LODGroup setup");
		return -1;
	}
	
	return LowestBit(node.lodIndexMask);
}	

bool BeastUtils::IsHighestLODLevel (Renderer& renderer)
{
	return GetHighestLODLevel(renderer) <= 0;
}	

void BeastUtils::ClearLightmapIndices()
{
	int lightmapIndex = -1;

	std::vector<Renderer*> renderers;
	Object::FindObjectsOfType(&renderers);
	for (size_t i = 0; i < renderers.size(); ++i)
	{
		Renderer* r = renderers[i];
		
		// Only allow scene objects
		if (r->IsPersistent())
			continue;
		
		// clear the previously set lightmap index, tiling and offset
		r->SetLightmapIndexInt(lightmapIndex);
		Vector4f noST(1.0f, 1.0f, 0.0f, 0.0f);
		r->SetLightmapST(noST);
	}

#if ENABLE_TERRAIN
	ITerrainManager* terrain = GetITerrainManager();
	if (terrain)
		terrain->SetLightmapIndexOnAllTerrains(lightmapIndex);
#endif
}

float GetLightmapLODLevelScale (Renderer& renderer)
{
	int highestLOD = GetHighestLODLevel(renderer);
	float scale = 1.0F;
	for (int i=0;i<highestLOD;i++)
		scale *= 0.5F;
	return scale;
}

struct HighLODModel
{
	int lodGroup;
	Renderer* renderer;
	ILBInstanceHandle instance;
	
	HighLODModel (int l, Renderer* r, ILBInstanceHandle i) : lodGroup (l), renderer (r), instance (i)
	{}
	
	friend bool operator < (const HighLODModel& lhs, const HighLODModel& rhs) { return lhs.lodGroup < rhs.lodGroup; }
};

static void ConnectLowLODInstances (const InstanceHandleToMeshRenderer& bakedInstances)
{
	typedef multiset<HighLODModel> HighLODModels;
	HighLODModels highestLODModels;
	
	///@TODO: support disabled LODGroups. (Should not be part of lightmapping...)
	
	// Build a set of all renderers that are contained in the highest LOD level
	InstanceHandleToMeshRenderer::const_iterator i;
	for(i = bakedInstances.begin(); i != bakedInstances.end(); ++i)
	{
		Renderer* renderer = i->renderer;
		ILBInstanceHandle instance = i->instanceHandle;
		
		int lodlevel = GetHighestLODLevel(*renderer);
		
		if (lodlevel == 0)
		{
			int lodIndex = renderer->GetLODGroup()->GetLODGroup();
			highestLODModels.insert(HighLODModel(lodIndex, renderer, instance));
		}
	}
	
	// Use ILBAddLODInstance to connect a low LOD to a high LOD lightmap (This will create extra lightmaps for low lod geometry instead of reusing it)
	for(i = bakedInstances.begin(); i != bakedInstances.end(); ++i)
	{
		Renderer* renderer = i->renderer;
		
		int lodLevel = GetHighestLODLevel(*renderer);
		
		// Renderer is not part of the highest LOD level
		// -> Thus we should connect it to a highest LOD level model
		if (lodLevel >= 1)
		{
			int lodIndex = renderer->GetLODGroup()->GetLODGroup();
			pair<HighLODModels::iterator, HighLODModels::iterator> range = highestLODModels.equal_range(HighLODModel(lodIndex, NULL, NULL));
			for (HighLODModels::iterator h=range.first;h != range.second;h++)
			{
				BeastCall(ILBAddLODInstance (i->instanceHandle, h->instance));
			}
		}
	}
}	


// ----------------------------------------------------------------------
//  LightmapperBeast
// ----------------------------------------------------------------------

const char* kBeastManagerPath = "./Temp/Beast";
const char* kUVMaterialLayer = "UV0";
const char* kUVBakeLayer = "UV1";
const float kMinScaleInLightmap = 0.0000001f;
unsigned char kLicenseKeyPro[] = { 125, 152, 240, 40, 209, 43, 209, 80, 16, 70, 64, 148, 67, 1, 223, 178 };
// this key disables Global Illumination in Beast for the Unity free users:
unsigned char kLicenseKeyFree[] = { 144, 196, 174, 0, 137, 230, 208, 2, 187, 34, 245, 172, 179, 67, 125, 150 };

LightmapperBeast::LightmapperBeast() : m_Manager(NULL), m_Scene(NULL), m_ObjectsCombinedArea(0.0f), m_Lbs(new LightmapperBeastShared())
{
	m_Lbs->m_BakeStartTime = GetTimeSinceStartup();

	BeastCall(ILBSetLogTarget(ILB_LT_ERROR, ILB_LS_STDERR, 0));
	
	#if defined(WIN32)
		BeastCall(ILBSetLogTarget(ILB_LT_INFO, ILB_LS_DEBUG_OUTPUT, 0));
	#else
		BeastCall(ILBSetLogTarget(ILB_LT_INFO, ILB_LS_STDOUT, 0));
	#endif

    DeleteFileOrDirectory(kBeastManagerPath);

	unsigned char* key = GetBuildSettings().hasAdvancedVersion ? kLicenseKeyPro : kLicenseKeyFree;
	unsigned char* mac = systeminfo::GetMacAddressForBeast();
	BeastCall(ILBCreateManagerWithSessionLicense(kBeastManagerPath, ILB_CS_LOCAL, key, mac, &m_Manager));

	BeastCall(ILBSetBeastPath(m_Manager, AppendPathName(GetApplicationContentsPath(), "Tools/Beast").c_str()));
	BeastCall(ILBClearCache(m_Manager));
}

void LightmapperBeast::BeginScene()
{
	BeastCall(ILBBeginScene(m_Manager, "BakingScene", &m_Scene));
}

void LightmapperBeast::EndScene()
{
	BeastCall(ILBEndScene(m_Scene));
}

ILBLightHandle LightmapperBeast::AddSceneLight (const Light& light, int index)
{
	// TODO : set falloff

	const float radius = light.GetRange();
	const LightType lightType = light.GetType();

	ILBLightHandle beastLight;
	Matrix4x4f lightMat;
	Quaternionf zUp = AxisAngleToQuaternion(Vector3f(1,0,0), -kPI*0.5f);
	if (lightType == kLightDirectional)
	{
		// matrix for directional light should only contain the rotational part
		// otherwise a bug in beast doesn't allow for soft shadows
		QuaternionToMatrix(light.GetComponent(Transform).GetRotation() * zUp, lightMat);
	}
	else
	{
		Matrix4x4f rotate, scale, temp1, temp2;
		temp1 = light.GetComponent(Transform).GetLocalToWorldMatrixNoScale();
		QuaternionToMatrix(zUp, rotate);
		scale.SetScale(Vector3f(radius, radius, radius));
		if (lightType == kLightArea)
		{
			Vector2f areaSize = light.GetAreaSize() * 0.5f;
			scale.SetScale(Vector3f(areaSize.x, 1.0f, areaSize.y));
		}
		else
		{
			const float radius = light.GetRange();
			scale.SetScale(Vector3f(radius, radius, radius));
		}
		MultiplyMatrices4x4 (&temp1, &rotate, &temp2);
		MultiplyMatrices4x4 (&temp2, &scale, &lightMat);
	}
	lightMat.Transpose();

	std::string lightName = Format("Light-%i", index);
	ColorRGBAf color = GammaToActiveColorSpace(light.GetColor()) * light.GetIntensity() * 2.0f;
	ILBLinearRGB beastColor(color.r, color.g, color.b);

	switch (lightType)
	{
	case kLightPoint:
		BeastCall(ILBCreatePointLight(m_Scene, lightName.c_str(), (const ILBMatrix4x4*)&lightMat, &beastColor, &beastLight));
		BeastUtils::SetLightRamp(light, beastLight);
		break;
	case kLightDirectional:
		BeastCall(ILBCreateDirectionalLight(m_Scene, lightName.c_str(), (const ILBMatrix4x4*)&lightMat, &beastColor, &beastLight));
		break;
	case kLightSpot:
		BeastCall(ILBCreateSpotLight(m_Scene, lightName.c_str(), (const ILBMatrix4x4*)&lightMat, &beastColor, &beastLight));
		// set spot angle
		BeastCall(ILBSetSpotlightCone(beastLight, Deg2Rad(light.GetSpotAngle()), 0, 0));
		// set falloff
		BeastUtils::SetLightRamp(light, beastLight);
		// set cookie
		BeastUtils::SetSpotlightCookie(light, beastLight, m_Manager);
		break;
	case kLightArea:
		BeastCall(ILBCreateAreaLight(m_Scene, lightName.c_str(), (const ILBMatrix4x4*)&lightMat, &beastColor, &beastLight));
		break;
	}

	if (light.GetShadows() != kShadowNone)
	{
		BeastCall(ILBSetCastShadows(beastLight, true));
		BeastCall(ILBSetShadowSamples(beastLight, light.GetShadowSamples()));
		if (lightType == kLightDirectional)
			BeastCall(ILBSetShadowAngle(beastLight, Deg2Rad(light.GetShadowAngle())));
		else if (lightType == kLightSpot || lightType == kLightPoint)
			BeastCall(ILBSetShadowRadius(beastLight, light.GetShadowRadius()));
	}

	BeastCall(ILBSetIntensityScale(beastLight, 1.0f, light.GetIndirectIntensity()));

	return beastLight;
}

struct CompareTransformPathsAndInstanceIDs
{
	bool operator ()(Transform* lhs, Transform* rhs)
	{
		int cmp = StrICmp(CalculateTransformPath(*lhs, NULL), CalculateTransformPath(*rhs, NULL));
		// if paths are identical, compare InstanceIDs
		return	cmp != 0 ? cmp > 0 : lhs->GetInstanceID() >= rhs->GetInstanceID();
	}

	bool operator ()(Light* lhs, Light* rhs)
	{
		return operator()(&lhs->GetComponent(Transform), &rhs->GetComponent(Transform));
	}

	bool operator ()(Renderer* lhs, Renderer* rhs)
	{
		return operator()(&lhs->GetComponent(Transform), &rhs->GetComponent(Transform));
	}

	bool operator ()(LightProbeGroup* lhs, LightProbeGroup* rhs)
	{
		return operator()(&lhs->GetComponent(Transform), &rhs->GetComponent(Transform));
	}
};


void LightmapperBeast::AddSceneLights(std::vector<ILBLightHandle>& fullyBakedLightHandles)
{
	DisplayProgressbar(kExportingScene, "Lights", kBeastLightProgress);
	// Gather all lights
	m_Lbs->m_BakedLights.clear();
	m_Lbs->m_NonBakedLights.clear();
	bool noLightsForLightmapping = true;
	std::vector<Light*> lights;
	Object::FindObjectsOfType(&lights);
	std::stable_sort(lights.begin(), lights.end(), CompareTransformPathsAndInstanceIDs());

	bool hasPro = GetBuildSettings().hasAdvancedVersion;

	for (size_t i = 0; i < lights.size(); ++i)
	{
		Light* l = lights[i];

		// skip persistent lights, we don't want to modify them,
		// so they can't even end up in m_NonBakedLights
		if (l->IsPersistent())
			continue;

		if (!l->GetEnabled() || !l->GetGameObject().IsActive())
		{
			m_Lbs->m_NonBakedLights.push_back (l);
			continue;
		}
		// ignore non-lightmap lights
		if(l->GetLightmappingForBake() == Light::kLightmappingRealtimeOnly)
		{
			m_Lbs->m_NonBakedLights.push_back (l);
			continue;
		}
		// ignore area lights for non-Pro licenses
		if (!hasPro && l->GetType() == kLightArea)
		{
			m_Lbs->m_NonBakedLights.push_back (l);
			continue;
		}

		m_Lbs->m_BakedLights.push_back (l);

		noLightsForLightmapping = false;

		// add light to the Beast scene
		ILBLightHandle beastLight = AddSceneLight(*l, i);

		// store fully baked lights to be marked fully baked later
		if (l->GetLightmappingForBake() == Light::kLightmappingBakedOnly)
			fullyBakedLightHandles.push_back(beastLight);
	}
}

string MatrixToString(Matrix4x4f& matrix)
{
	string result;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			result.append(Format("%f", matrix.Get(i, j)));
			if (j < 3)
				result.append(", ");
		}
		result.append("\n");
	}
	return result;
}

static Rectf UVBounds (const StrideIterator<Vector2f> &uvStart, const StrideIterator<Vector2f> &uvEnd)
{
	if (uvStart == uvEnd)
		return Rectf(0,0,0,0);

	Vector4f b(0,0,0,0);
	b.x += (*uvStart).x;
	b.y += (*uvStart).y;
	b.z += b.x;
	b.w += b.y;

	for (StrideIterator<Vector2f> uv = uvStart; uv != uvEnd; ++uv)
	{
		Vector2f& a = *uv;
		b.x = min (b.x, a.x);
		b.y = min (b.y, a.y);
		b.z = max (b.z, a.x);
		b.w = max (b.w, a.y);
	}

	return Rectf(b.x, b.y, b.z - b.x, b.w - b.y);
}

void LightmapperBeast::AddSceneObject(Renderer* renderer, Mesh& mesh, ILBMeshHandle beastMesh, int index, bool isSkinnedMesh, bool bake)
{
	// create object
	ILBInstanceHandle inst;
	Matrix4x4f object2world;
	Transform& t = renderer->GetComponent(Transform);
	if (isSkinnedMesh)
		object2world = renderer->GetTransformInfo().worldMatrix;
	else
		t.CalculateTransformMatrix(object2world);
	object2world.Transpose();

	bool shouldBakeIntoLightmap = renderer->GetScaleInLightmap() > 0;
	
	// LOD'd objects that do not receive lightmaps should be treated as not receiving lightmaps
	// But they should also not cast any shadows...
	if (!shouldBakeIntoLightmap && !IsHighestLODLevel (*renderer))
	{	
		m_Lbs->m_NonBakedInstances.push_back(renderer);
		return;
	}
	
	std::string name = Format("Instance-%i", index);

	BeastCall(ILBCreateInstance(m_Scene, beastMesh, name.c_str(), (const ILBMatrix4x4*)&object2world, &inst));

	HandleCastReceiveShadows(inst, renderer->GetCastShadows());
	//BeastCall(ILBSetRenderStats(inst, ILB_RS_SHADOW_BIAS, ILB_RSOP_ENABLE));

	const int uvchannel = mesh.IsAvailable(kShaderChannelTexCoord1) ? 1 : 0;
	Rectf uvBounds = UVBounds(mesh.GetUvBegin(uvchannel), mesh.GetUvEnd(uvchannel));

	if (bake)
	{
		// decide whether the object should receive a lightmap or not
		if (shouldBakeIntoLightmap)
			m_InstanceHandleToMeshRenderer.push_back(LightmapperDestinationInstanceHandle(inst, renderer, uvBounds));
		else
			m_Lbs->m_NonBakedInstances.push_back(renderer);

		m_Lbs->m_DestinationInstances.push_back(LightmapperDestinationInstance(renderer, bake, uvBounds));
	}
	else
	{
		// if the object is currently lightmapped add it to the destination atlasing
		if (renderer->IsLightmappedForRendering())
			m_Lbs->m_DestinationInstances.push_back(LightmapperDestinationInstance(renderer, bake, uvBounds));
	}
}

vector<Material*> GetMaterials(Renderer& renderer)
{
	// Materials
	const Renderer::MaterialArray& originalMaterials = renderer.GetMaterialArray();
	vector<Material*> materials(originalMaterials.size());

	bool hasAtLeastOneMaterial = false;
	bool atLeastOneMaterialMissing = false;
	for(int i = 0; i < materials.size(); i++)
	{
		materials[i] = originalMaterials[i];
		if(!materials[i])
		{
			atLeastOneMaterialMissing = true;
			continue;
		}
		hasAtLeastOneMaterial = true;
	}

	if(!hasAtLeastOneMaterial)
	{
		WarningStringObject(Format("All materials on %s are missing!", renderer.GetName()), &renderer);
		materials.clear();
		return materials;
	}

	if(atLeastOneMaterialMissing)
		WarningStringObject(Format("Some materials on %s are missing - corresponding submeshes will not be lightmapped.", renderer.GetName()), &renderer);

	return materials;
}

ILBPointCloudHandle LightmapperBeast::AddLightProbes(string& outError)
{
	DisplayProgressbar(kExportingScene, "Light Probes", kBeastLightProbeProgress);
	if (!GetBuildSettings().hasAdvancedVersion)
	{
		m_Lbs->m_LightProbeSourcePositions.clear();
		outError = "Light probe baking requires Unity Pro.";
		return NULL;
	}
	
	dynamic_array<Vector3f> orgPositions;

	std::vector<LightProbeGroup*> lightProbeGroups;
	Object::FindObjectsOfType(&lightProbeGroups);
	// Needs to be sorted to always get the same order for testing
	std::stable_sort(lightProbeGroups.begin(), lightProbeGroups.end(), CompareTransformPathsAndInstanceIDs());

	for (std::vector<LightProbeGroup*>::iterator group = lightProbeGroups.begin(); group != lightProbeGroups.end(); group++)
	{
		if(!(**group).IsActive ())
			continue;
		
		const Vector3f* groupPositions = (*group)->GetPositions();
		const int newPositionsStart = orgPositions.size();
		orgPositions.insert(orgPositions.end(), groupPositions, groupPositions + (*group)->GetPositionsSize());
		const int count = orgPositions.size();
		for (int i = newPositionsStart; i < count; i++)
			orgPositions[i] = (*group)->GetComponent(Transform).TransformPoint(orgPositions[i]);
	}

	dynamic_array<Vector3f>& positions = m_Lbs->m_LightProbeSourcePositions;
	positions.clear();

	if (orgPositions.size() == 0)
	{
		outError = "There are no light probes to bake.";
		return NULL;
	}

	// Check if the probe positions can be tetrahedralized
	int* indices;
	Vector3f* vertices;
	int indexCount, vertexCount;
	LightProbeUtils::Tetrahedralize(&orgPositions[0], orgPositions.size(), &indices, &indexCount, &vertices, &vertexCount);
	delete[] indices;
	delete[] vertices;
	if (indexCount == 0)
	{
		outError = "Probe positions can't be tetrahedralized.";
		return NULL;
	}

	LightProbeUtils::RemoveDuplicateLightProbePositions(&orgPositions[0], orgPositions.size(), positions);
	
	dynamic_array<Vector3f> normals;
	const int count = positions.size();
	normals.resize_initialized(count, Vector3f(1, 0, 0));

	ILBPointCloudHandle pch;
	BeastCall (ILBCreatePointCloud(m_Scene, "LightProbes", &pch));
	BeastCall (ILBAddPointCloudData(pch, reinterpret_cast<const ILBVec3*> (&positions[0]), reinterpret_cast<const ILBVec3*> (&normals[0]), count));
	BeastCall (ILBEndPointCloud(pch));

	return pch;
}

void LightmapperBeast::AddSceneObjects(bool bakeSelected)
{
	// Make sure all renderers are in the scene. We use it to calculate LODGroup information.
	RenderManager::UpdateAllRenderers();

	std::vector<MeshRenderer*> meshRenderers;
	std::vector<SkinnedMeshRenderer*> skinnedMeshRenderers;
	int instanceIndex = 0;

	if (bakeSelected)
	{
		set<Transform*> selection = GetTransformSelection(kDeepSelection);
		for (set<Transform*>::iterator i=selection.begin(); i != selection.end(); i++)
		{
			if ((*i) != NULL)
			{
				if ((*i)->QueryComponent (MeshRenderer) != NULL)
					meshRenderers.push_back(&(*i)->GetComponent(MeshRenderer));
				if ((*i)->QueryComponent (SkinnedMeshRenderer) != NULL)
					skinnedMeshRenderers.push_back(&(*i)->GetComponent(SkinnedMeshRenderer));
			}
		}

		// Add non selected mesh renderers
		{
			std::vector<MeshRenderer*> allMeshRenderers, nonSelectedMeshRenderers;
			Object::FindObjectsOfType(&allMeshRenderers);
			for (int i = 0; i < allMeshRenderers.size(); i++)
			{
				MeshRenderer* mr = allMeshRenderers[i];
				int j = 0;
				for (; j < meshRenderers.size(); j++)
					if (mr == meshRenderers[j])
						break;
				// if not found in selected
				if (j == meshRenderers.size())
					nonSelectedMeshRenderers.push_back(mr);
			}
			AddMeshRenderers(nonSelectedMeshRenderers, instanceIndex, false);
		}

		// Add non selected skinned mesh renderers
		{
			std::vector<SkinnedMeshRenderer*> allSkinnedMeshRenderers, nonSelectedSkinnedMeshRenderers;
			Object::FindObjectsOfType(&allSkinnedMeshRenderers);
			for (int i = 0; i < allSkinnedMeshRenderers.size(); i++)
			{
				SkinnedMeshRenderer* smr = allSkinnedMeshRenderers[i];
				int j = 0;
				for (; j < skinnedMeshRenderers.size(); j++)
					if (smr == skinnedMeshRenderers[j])
						break;
				// if not found in selected
				if (j == skinnedMeshRenderers.size())
					nonSelectedSkinnedMeshRenderers.push_back(smr);
			}
			AddSkinnedMeshRenderers(nonSelectedSkinnedMeshRenderers, instanceIndex, false);
		}

	}
	else
	{
		Object::FindObjectsOfType(&meshRenderers);
		Object::FindObjectsOfType(&skinnedMeshRenderers);
	}

	std::stable_sort(meshRenderers.begin(), meshRenderers.end(), CompareTransformPathsAndInstanceIDs());
	std::stable_sort(skinnedMeshRenderers.begin(), skinnedMeshRenderers.end(), CompareTransformPathsAndInstanceIDs());

	AddMeshRenderers(meshRenderers, instanceIndex, true);
	AddSkinnedMeshRenderers(skinnedMeshRenderers, instanceIndex, true);
	ConnectLowLODInstances(m_InstanceHandleToMeshRenderer);
}

static bool ShouldRendererBeLightmapped(Renderer* r)
{
	// only static objects go to the beast scene AND become lightmapped
	if(!r->GetGameObject().AreStaticEditorFlagsSet (kLightmapStatic))
		return false;

	if (!r->GetGameObject().IsActive())
		return false;
	if (!r->GetEnabled())
		return false;
	LODGroup* lodGroup = r->GetLODGroup();
	if (lodGroup != NULL && lodGroup->GetLODGroup() == kDisabledLODGroup)
		return false;

	return true;
}

const char* kNoNormalsNoLightmapping = "Mesh %s doesn't have normals. Renderer %s, which uses this mesh, will not be lightmapped.";
const char* kNoVerticesNoLightmapping = "Mesh %s failed to retrieve vertices and normals. Renderer %s, which uses this mesh, will not be lightmapped.";

void LightmapperBeast::AddMeshRenderers(std::vector<MeshRenderer*> &renderers, int& index, bool bake)
{
	for (size_t i = 0; i < renderers.size(); ++i)
	{
		DisplayProgressbar(kExportingScene, "Mesh Renderers", GetProgressPercentage (i, renderers.size (), kBeastStart, kBeastMeshRenderers));
		MeshRenderer* r = renderers[i];

		if (!ShouldRendererBeLightmapped(r))
			continue;

		Mesh* mesh = r->GetSharedMesh();
		if (!mesh)
			continue;

		int vertexCount = mesh->GetVertexCount();

		dynamic_array<Vector3f> normals;
		if (mesh->IsAvailable(kShaderChannelNormal))
		{
			normals.resize_uninitialized(vertexCount);
			mesh->ExtractNormalArray(normals.data ());
		}
		else
		{
			WarningStringObject(Format(kNoNormalsNoLightmapping, mesh->GetName(), r->GetName()), r);
			continue;
		}

		dynamic_array<Vector3f> vertices (vertexCount, kMemTempAlloc);
		mesh->ExtractVertexArray(vertices.data ());

		CreateMeshAndAddSceneObject(r, mesh, vertices, normals, false, index, bake);
	}
}

void LightmapperBeast::AddSkinnedMeshRenderers(std::vector<SkinnedMeshRenderer*> &renderers, int& index, bool bake)
{
	for (size_t i = 0; i < renderers.size(); ++i)
	{
		DisplayProgressbar(kExportingScene, "Skinned Mesh Renderers", GetProgressPercentage (i, renderers.size (), kBeastMeshRenderers, kBeastSkeletalMeshRenderers));

		SkinnedMeshRenderer* r = renderers[i];

		if (!ShouldRendererBeLightmapped(r))
			continue;

		Mesh* mesh = r->GetMesh();
		if (!mesh)
			continue;

		dynamic_array<Vector3f> vertices, normals;
		if (!r->GetSkinnedVerticesAndNormals(&vertices, &normals))
		{
			WarningStringObject(Format(kNoVerticesNoLightmapping, mesh->GetName(), r->GetName()), r);
			continue;
		}
		if (normals.size() == 0)
		{
			WarningStringObject(Format(kNoNormalsNoLightmapping, mesh->GetName(), r->GetName()), r);
			continue;
		}

		CreateMeshAndAddSceneObject(r, mesh, vertices, normals, true, index, bake);
	}
}

void LightmapperBeast::CreateMeshAndAddSceneObject(Renderer* r, Mesh* mesh, const dynamic_array<Vector3f>& vertices, const dynamic_array<Vector3f>& normals, bool isSkinnedMesh, int& index, bool bake)
{
	// create mesh if we don't have it yet
	ILBMeshHandle meshHandle;
	vector<Material*> materials = GetMaterials(*r);
	if (materials.empty())
		return;

	string meshName = Format("Mesh-%s-%i", r->GetName(), r->GetInstanceID());

	if (CreateMeshWithMaterials(*mesh, vertices, normals, materials, meshName, r->GetScaleInLightmap() > 0.0f, meshHandle))
	{
		AddSceneObject(r, *mesh, meshHandle, index, isSkinnedMesh, bake);
		index++;
	}
}

void CreateGameObjectFromTerrainPatch(Mesh& mesh, const Vector3f& terrainPosition)
{
	GameObject& go = CreateGameObject(mesh.GetName(), "Transform", "MeshFilter", "MeshRenderer", NULL);

	Transform &tc = go.GetComponent(Transform);
	tc.SetPosition(terrainPosition);	
	tc.SetRotation(Quaternionf::identity());
	go.GetComponent(MeshFilter).SetSharedMesh (&mesh);

	Shader* shader = GetScriptMapper().FindShader("Diffuse");
	Material* material = Material::CreateMaterial(*shader, Object::kHideAndDontSave);
	go.GetComponent(MeshRenderer).SetMaterial(material, 0);

	go.SetStaticEditorFlags ((StaticEditorFlags)0xFFFFFFFF);
}

#define DEBUG_TERRAIN_MESH 0

void LightmapperBeast::AddTerrainPatches(TerrainData* terrainData, const Vector3f& terrainPosition, bool castShadows, int terrainIndex, Material* templateMat)
{
	// get meshes and base material for current terrain
	TerrainRenderer tr(0, terrainData, terrainPosition, -1);
	std::vector<Mesh*> meshes = tr.GetMeshPatches();
	Material* baseMaterial = tr.m_SplatMaterials->GetSplatBaseMaterial(templateMat);

	std::vector<Material*> materials;
	materials.push_back(baseMaterial);

	// add all terrain patches as Beast instances
	float terrainBlockSize = (kBeastTerrains - kBeastSkeletalMeshRenderers) / m_Terrains.size ();
	float terrainBlockStart = (terrainBlockSize * terrainIndex) + kBeastSkeletalMeshRenderers;
	float terrainBlockEnd = terrainBlockStart + terrainBlockSize;
	for (int j = 0; j < meshes.size(); j++)
	{
		DisplayProgressbar(kExportingScene, "Terrain", GetProgressPercentage (j,  meshes.size(), terrainBlockStart, terrainBlockEnd));
	
		Mesh* mesh = meshes[j];

		#if DEBUG_TERRAIN_MESH
			CreateGameObjectFromTerrainPatch(*mesh, terrainPosition);
		#endif

		string meshName = Format("Mesh-%s(%i)[%i]", terrainData->GetName(), terrainData->GetInstanceID(), j);
		int vertexCount = mesh->GetVertexCount();
		dynamic_array<Vector3f> vertices (vertexCount, kMemTempAlloc), normals (vertexCount, kMemTempAlloc);
		mesh->ExtractVertexArray(vertices.data ());
		if (mesh->IsAvailable(kShaderChannelNormal))
			mesh->ExtractNormalArray(normals.data ());
		ILBMeshHandle beastMesh;
		if (CreateMeshWithMaterials(*mesh, vertices, normals, materials, meshName, false, beastMesh))
		{
			// create the instance
			Matrix4x4f objectToWorld;
			objectToWorld.SetTranslate(terrainPosition);
			objectToWorld.Transpose();
			std::string name = Format("TerrainInstance-%i-%i", terrainIndex, j);

			ILBInstanceHandle instance;
			BeastCall(ILBCreateInstance(m_Scene, beastMesh, name.c_str(), (const ILBMatrix4x4*)&objectToWorld, &instance));

			HandleCastReceiveShadows(instance, castShadows);

			m_TerrainPatchInstances[terrainIndex].push_back(instance);
		}
	}
}

void LightmapperBeast::AddTrees(TerrainData* terrainData, const Vector3f& terrainPosition, bool castShadows)
{
	TreeDatabase& treeDatabase = terrainData->GetTreeDatabase();
	std::vector<TreeInstance>& treeInstances = treeDatabase.GetInstances();
	Vector3f terrainSize = terrainData->GetHeightmap().GetSize();

	std::set<int> prototypesWithoutNormals;
	int k = 0;
	for (std::vector<TreeInstance>::iterator it = treeInstances.begin(), end = treeInstances.end(); it != end; ++it, k++)			 
	{
		TreeInstance& treeInstance = *it;
		int prototypeIndex = treeInstance.index;
		const TreeDatabase::Prototype& prototype = treeDatabase.GetPrototypes()[prototypeIndex];

		Mesh* mesh = prototype.mesh;

		if (!mesh)
			continue;

		int vertexCount = mesh->GetVertexCount();

		// check if the mesh has normals
		dynamic_array<Vector3f> normals;
		if (mesh->IsAvailable(kShaderChannelNormal))
		{
			normals.resize_uninitialized(vertexCount);
			mesh->ExtractNormalArray(normals.data ());
		}
		else
		{
			// mesh doesn't have normals, continue with the next instance
			prototypesWithoutNormals.insert(prototypeIndex);
			continue;
		}

		dynamic_array<Vector3f> vertices (vertexCount, kMemTempAlloc);
		mesh->ExtractVertexArray(vertices.data ());

		string meshName = Format("Mesh-%s(%i)-TreePrototype[%i]", terrainData->GetName(), terrainData->GetInstanceID(), prototypeIndex);
		const std::vector<Material*>& materials = prototype.materials;

		ILBMeshHandle beastMesh;
		if (CreateMeshWithMaterials(*mesh, vertices, normals, materials, meshName, false, beastMesh))
		{
			// create the instance
			Matrix4x4f objectToWorld;
			Vector3f position = BeastUtils::GetTreeInstancePosition(treeInstance, terrainSize, terrainPosition);
			objectToWorld.SetTRS(position, Quaternionf::identity(), Vector3f(treeInstance.widthScale, treeInstance.heightScale, treeInstance.widthScale));
			objectToWorld.Transpose();

			ILBInstanceHandle instance;
			std::string name = Format("%s-TreeInstance-%i", terrainData->GetName(), k);
			BeastCall(ILBCreateInstance(m_Scene, beastMesh, name.c_str(), (const ILBMatrix4x4*)&objectToWorld, &instance));

			HandleCastReceiveShadows(instance, castShadows);
		}
	}

	// emit a warning for each tree prototype that doesn't have normals
	std::set<int>::iterator it;
	for (it = prototypesWithoutNormals.begin(); it != prototypesWithoutNormals.end(); it++)
	{
		const TreeDatabase::Prototype& prototype = treeDatabase.GetPrototypes()[*it];
		GameObject* prefab = prototype.prefab;
	
		WarningStringObject(Format("Mesh used in tree prototype %s doesn't have normals. Normals are needed for lightmapping. No instances of this prototype will be lightmapped.", prefab ? prefab->GetName() : ""), prefab);
	}
}

void LightmapperBeast::AddTerrains()
{
	ExtractStaticTerrains(kLightmapStatic, m_Terrains);

	m_TerrainPatchInstances.clear();
	m_TerrainPatchInstances.resize(m_Terrains.size(), vector<ILBInstanceHandle>());

	// for each active terrain
	for (int i = 0; i < m_Terrains.size(); i++)
	{
		TerrainBakeInfo& bakeInfo = m_Terrains[i];
		AddTerrainPatches(bakeInfo.terrainData, bakeInfo.position, bakeInfo.castShadows, i, bakeInfo.templateMaterial);
		AddTrees(bakeInfo.terrainData, bakeInfo.position, bakeInfo.castShadows);
	}
}

float CalculateTexelScale (Renderer* renderer, float resolution)
{
	// Set texel scale as the Renderer's ScaleInLightmap
	float scaleInLightmap = renderer->GetScaleInLightmap();
	
	// LOD level default scale
	scaleInLightmap *= GetLightmapLODLevelScale(*renderer);
	
	// Scale by global resolution
	scaleInLightmap *= resolution;

	// minimum lightmap scale
	if(scaleInLightmap <= 0.0f)
		scaleInLightmap = kMinScaleInLightmap;
	
	return scaleInLightmap;
}

struct CompareRenderersBySize
{
	bool operator() (const LightmapperDestinationInstanceHandle& lhs, const LightmapperDestinationInstanceHandle& rhs)
	{
		return	lhs.size > rhs.size;
	}

	bool operator() (const LightmapperDestinationInstance& lhs, const LightmapperDestinationInstance& rhs)
	{
		return	lhs.size > rhs.size;
	}
};

template<class T>
static void AtlasBakeInstances (T& instances, dynamic_array<Vector2f>& outScales, dynamic_array<Vector2f>& outOffsets, dynamic_array<int>& outIndices, int& atlasCount)
{
	const int count = instances.size ();

	outScales.resize_uninitialized (count);

	LightmapEditorSettings& les = GetLightmapEditorSettings ();
	float resolution = les.GetResolution ();
	float maxAtlasSize = std::min (les.GetTextureWidth (), les.GetTextureHeight ());

	for(int i = 0; i < count; ++i)
	{
		Renderer* r = instances[i].renderer;
		Rectf& uvBounds = instances[i].uvBounds;
		if (instances[i].selected)
		{
			// calculate new scale for selected objects
			MeshRenderer* mr = dynamic_cast<MeshRenderer*>(r);
			SkinnedMeshRenderer* smr = mr ? NULL : dynamic_cast<SkinnedMeshRenderer*>(r);
			float area = mr ? mr->GetCachedSurfaceArea () : smr->GetCachedSurfaceArea ();

			float scale = CalculateTexelScale (r, resolution);
			scale *= sqrt (area);
			scale = floor (scale);
			scale = clamp (scale, 1.0f, maxAtlasSize);
			
			instances[i].uvSize.Set (uvBounds.width * scale, uvBounds.height * scale);
		}
		else
		{
			// non-selected objects need to preserve their size in the lightmap
			const Vector4f& scaleAndOffset = r->GetLightmapST() * maxAtlasSize;

			instances[i].uvSize.Set (scaleAndOffset.x * uvBounds.width, scaleAndOffset.y * uvBounds.height);
		}

		instances[i].size = instances[i].uvSize.x + instances[i].uvSize.y;
	}

	std::stable_sort (instances.begin (), instances.end (), CompareRenderersBySize ());

	for(int i = 0; i < count; ++i)
	{
		outScales[i] = instances[i].uvSize;
	}

	PackAtlases (outScales, maxAtlasSize, std::max (les.GetPadding(), 2), outOffsets, outIndices, atlasCount);

	float maxAtlasSizeInv = 1.0f / maxAtlasSize;
	for(int i = 0; i < count; ++i)
	{
		Rectf& uvBounds = instances[i].uvBounds;

		outScales[i] *= maxAtlasSizeInv;
		outScales[i].x /= uvBounds.width;
		outScales[i].y /= uvBounds.height;

		outOffsets[i] *= maxAtlasSizeInv;
		outOffsets[i].x -= uvBounds.x * outScales[i].x;
		outOffsets[i].y -= uvBounds.y * outScales[i].y;
	}
}

void LightmapperBeast::PrepareLightmapBakeTargets(bool bakeSelected, bool lightProbesOnly, const std::vector<ILBLightHandle>& fullyBakedLightHandles)
{
	if (lightProbesOnly)
		return;

	m_Lbs->m_BakedInstances.clear ();
	
	LightmapEditorSettings& les = GetLightmapEditorSettings();

	les.UpdateResolutionOnBakeStart();
	// TODO: use both width and height
	int atlasSize = std::min(les.GetTextureWidth(), les.GetTextureHeight());
	m_Lbs->m_MaxAtlasSize = atlasSize;

	m_Lbs->Lrp.CreateRenderPass(m_Lbs->Job);

	bool atLeastOneBakeInstance = false;

	const bool lockAtlas = les.GetLockAtlas();
	m_Lbs->m_LockAtlas = lockAtlas;
	if (!lockAtlas || bakeSelected)
	{
		// TODO: keep track of when we need to atlas again
		m_Lbs->m_KeepOldAtlasingInBakeSelected = false;

		dynamic_array<Vector2f>& scales = m_Lbs->m_Atlasing.scales;
		dynamic_array<Vector2f>& offsets = m_Lbs->m_Atlasing.offsets;
		dynamic_array<int>& indices = m_Lbs->m_Atlasing.indices;

		AtlasBakeInstances<InstanceHandleToMeshRenderer> (m_InstanceHandleToMeshRenderer, scales, offsets, indices, m_Lbs->m_Atlasing.atlasCount);

		if (bakeSelected)
			AtlasBakeInstances<RendererAndUVSizes> (m_Lbs->m_DestinationInstances, m_Lbs->m_DestinationAtlasing.scales, m_Lbs->m_DestinationAtlasing.offsets, m_Lbs->m_DestinationAtlasing.indices, m_Lbs->m_DestinationAtlasing.atlasCount);

		m_Lbs->TextureTargets.resize(m_Lbs->m_Atlasing.atlasCount);
		for (int i = 0; i < m_Lbs->m_Atlasing.atlasCount; i++)
		{
			BeastCall(ILBCreateTextureTarget(m_Lbs->Job, Format("TextureTarget[%i]", i).c_str(), atlasSize, atlasSize, &(m_Lbs->TextureTargets[i])));
			m_Lbs->Lrp.AddRenderPassToTarget(m_Lbs->TextureTargets[i]);
		}

		const int instanceCount = m_InstanceHandleToMeshRenderer.size ();
		for (int i = 0; i < instanceCount; i++)
		{
			ILBTargetEntityHandle bakeInstance;
			LightmapperDestinationInstanceHandle& instance = m_InstanceHandleToMeshRenderer[i];
			ILBTargetHandle& targetHandle = m_Lbs->TextureTargets[indices[i]];

			// Add bake instance to target
			BeastCall(ILBAddBakeInstance(targetHandle, instance.instanceHandle, &bakeInstance));
			BakedInstanceInfo bakedInstanceInfo = { bakeInstance, targetHandle, instance.renderer, instance.uvBounds };
			m_Lbs->m_BakedInstances.push_back(bakedInstanceInfo);

			// Set scale and offset
			BeastCall(ILBSetUVTransform(bakeInstance, (const ILBVec2*)&(offsets[i]), (const ILBVec2*)&(scales[i])));

			// Set the UV set to be used for baking
			BeastCall(ILBSetBakeUVSet(bakeInstance, kUVBakeLayer));
		}

		atLeastOneBakeInstance = (instanceCount > 0);
	}
	else
	{
		// create as many texture targets as there are textures in LightmapSettings
		const std::vector<LightmapData>& lightmaps = GetLightmapSettings().GetLightmaps();
		const int lightmapCount = lightmaps.size();
		m_Lbs->TextureTargets.resize(lightmapCount);
		m_Lbs->TextureTargetsUsed.resize(lightmapCount, false);
		for (int i = 0; i < lightmapCount; i++)
		{
			int width = atlasSize;
			int height = atlasSize;
			if (!lightmaps[i].m_Lightmap.IsNull())
			{
				width = lightmaps[i].m_Lightmap->GetDataWidth();
				height = lightmaps[i].m_Lightmap->GetDataHeight();
			}
			BeastCall(ILBCreateTextureTarget(m_Lbs->Job, Format("TextureTarget[%i]", i).c_str(), width, height, &(m_Lbs->TextureTargets[i])));
			m_Lbs->Lrp.AddRenderPassToTarget(m_Lbs->TextureTargets[i]);
		}

		// for each scene instance that should be lightmapped, if we don't want to use atlasing
		InstanceHandleToMeshRenderer::iterator iter;
		for(iter = m_InstanceHandleToMeshRenderer.begin(); iter != m_InstanceHandleToMeshRenderer.end(); ++iter)
		{
			// Add bake instance to target
			ILBTargetEntityHandle bakeInstance;
			const int lightmapIndex = iter->renderer->GetLightmapIndex();
			if (lightmapIndex < 0 || lightmapIndex >= LightmapSettings::kMaxLightmaps )
				// This object is not supposed to get a lightmap, skip silently
				continue;
			if (lightmapIndex >= m_Lbs->TextureTargets.size())
			{
				WarningStringObject(Format("Lightmap index %i on Mesh Renderer %s is out of lightmap array bounds, skipping.", lightmapIndex, iter->renderer->GetName()), iter->renderer);
				continue;
			}
			atLeastOneBakeInstance = true;
			ILBTargetHandle& targetHandle = m_Lbs->TextureTargets[lightmapIndex];
			m_Lbs->TextureTargetsUsed[lightmapIndex] = true;
			BeastCall(ILBAddBakeInstance(targetHandle, iter->instanceHandle, &bakeInstance));
			BakedInstanceInfo bakedInstanceInfo = { bakeInstance, targetHandle, iter->renderer, iter->uvBounds };
			m_Lbs->m_BakedInstances.push_back(bakedInstanceInfo);

			// Set scale and offset
			const Vector4f& scaleAndOffset = iter->renderer->GetLightmapST();
			const Vector2f scale(scaleAndOffset.x, scaleAndOffset.y);
			const Vector2f offset(scaleAndOffset.z, scaleAndOffset.w);
			BeastCall(ILBSetUVTransform(bakeInstance, (const ILBVec2*)&offset, (const ILBVec2*)&scale));

			// Set the UV set to be used for baking
			BeastCall(ILBSetBakeUVSet(bakeInstance, kUVBakeLayer));
		}
	}

	for (int i = 0; i < m_Terrains.size(); i++)
	{
		// if lightmapSize is 0, don't create bake instances for this terrain (just like scaleInLightmap being 0 for renderers)
		if (m_Terrains[i].lightmapSize == 0)
		{
			m_Lbs->m_NonBakedInstances.push_back(m_Terrains[i].terrainData);
			continue;
		}

		bool bake = !bakeSelected || (bakeSelected && m_Terrains[i].selected);

		ILBTargetHandle textureTarget;
		if (!lockAtlas || bakeSelected)
		{
			if (bake)
			{
				BeastCall (ILBCreateTextureTarget (m_Lbs->Job, Format ("TerrainTarget[%i]", i).c_str (), m_Terrains[i].lightmapSize, m_Terrains[i].lightmapSize, &textureTarget));
				m_Lbs->Lrp.AddRenderPassToTarget (textureTarget);
				m_Lbs->TextureTargets.push_back (textureTarget);
			}
			m_Lbs->Terrains.push_back (m_Terrains[i]);
		}
		else
		{
			const int lightmapIndex = m_Terrains[i].lightmapIndex;
			if (lightmapIndex < 0 || lightmapIndex >= LightmapSettings::kMaxLightmaps )
				// This object is not supposed to get a lightmap, skip silently
				continue;
			if (lightmapIndex >= m_Lbs->TextureTargets.size())
			{
				WarningString(Format("Lightmap index %i on Terrain %s is out of lightmap array bounds, skipping.", lightmapIndex, m_Terrains[i].terrainData->GetName()));
				continue;
			}
			atLeastOneBakeInstance = true;
			textureTarget = m_Lbs->TextureTargets[lightmapIndex];
			m_Lbs->TextureTargetsUsed[lightmapIndex] = true;
		}

		Assert(m_Terrains.size() == m_TerrainPatchInstances.size());

		if (bake)
		{
			for (int j = 0; j < m_TerrainPatchInstances[i].size (); j++)
			{
				// Add bake instance to target
				ILBTargetEntityHandle bakeInstance;
				BeastCall (ILBAddBakeInstance (textureTarget, m_TerrainPatchInstances[i][j], &bakeInstance));

				// Set the UV set to be used for baking
				BeastCall (ILBSetBakeUVSet (bakeInstance, kUVBakeLayer));
			}
		}
	}

	if (lockAtlas && !atLeastOneBakeInstance)
	{
		// the scene is setup incorrectly, nothing will actually be baked and we can't do anything about it, because atlas lock was requested
		DisplayDialog("Nothing to bake", "Lock Atlas is enabled and lightmap indices on objects in the scene don't index into existing lightmap slots. Make sure the indices are correct and that the lightmaps array is big enough.", "Ok");
		throw BeastException("Lightmapping cancelled, nothing to bake.");
	}

	if (!lockAtlas && m_Lbs->m_BakedInstances.size() == 0 && m_Lbs->Terrains.size() == 0)
	{
		DisplayDialog("Nothing to bake", Format("None of the %sMesh Renderers or Terrains in the scene are marked as static. Only static objects will be lightmapped.", bakeSelected ? "selected " : ""), "Ok");
		throw BeastException("Lightmapping cancelled, nothing to bake. Mark the objects you want to lightmap as static.");
	}

	// Mark appropriate lights as fully baked (aka baked only)
	ILBRenderPassHandle renderPass = m_Lbs->Lrp.GetRenderPass();
	for (std::vector<ILBLightHandle>::const_iterator it = fullyBakedLightHandles.begin(); it != fullyBakedLightHandles.end(); it++)
		ILBAddFullyBakedLight(renderPass, *it);
}

void LightmapperBeast::SendScene (bool bakeSelected, bool lightProbesOnly, std::vector<ILBLightHandle>& fullyBakedLightHandles, ILBPointCloudHandle &pointCloud)
{
	BeginScene();
	AddSceneObjects(bakeSelected);
	AddTerrains();
	// even if in bakeSelected mode, pass all the lights to Beast anyway and let it do the culling
	AddSceneLights(fullyBakedLightHandles);

	string outError;
	pointCloud = AddLightProbes (outError);
	if (!pointCloud && lightProbesOnly)
		throw BeastException (outError);

	EndScene();
}

static void HandleDirectionalLightmapsProOnly ()
{
	if ((GetLightmapSettings().GetLightmapsMode() == LightmapSettings::kDirectionalLightmapsMode) && !GetBuildSettings().hasAdvancedVersion)
		throw BeastException ("Directional Lightmaps require a Pro license.");
}

void LightmapperBeast::Prepare( LightmapperBeastShared*& lbs, bool bakeSelected, bool lightProbesOnly )
{
	DisplayProgressbar(kExportingScene, "Exporting to Beast", kBeastStart);
	HandleDirectionalLightmapsProOnly ();

	lbs = m_Lbs;
	std::vector<ILBLightHandle> fullyBakedLightHandles;
	ILBPointCloudHandle pointCloud;

	SendScene(bakeSelected, lightProbesOnly, fullyBakedLightHandles, pointCloud);
	
	std::string configFilePath = "./Temp/BeastBake.xml";
	CreateConfigFile(configFilePath, false);
	ILBJobHandle job;
	BeastCall(ILBCreateJob(m_Manager, _T("TestJob"), m_Scene, configFilePath.c_str(), &job));
	m_Lbs->Job = job;

	m_Lbs->m_LightProbesOnly = lightProbesOnly;
	m_Lbs->m_SelectedOnly = bakeSelected;

	// Add the atlas or textured targets
	PrepareLightmapBakeTargets(bakeSelected, lightProbesOnly, fullyBakedLightHandles);

	if (pointCloud)
	{
		m_Lbs->Lrp.CreateRenderPassLightProbes(m_Lbs->Job);
		BeastCall(ILBCreatePointCloudTarget(m_Lbs->Job, "pointCloudTarget", &m_Lbs->m_LightProbeTarget));
		BeastCall(ILBAddBakePointCloud(m_Lbs->m_LightProbeTarget, pointCloud, &m_Lbs->m_LightProbeEntity));
		m_Lbs->Lrp.AddRenderPassLightProbesToTarget(m_Lbs->m_LightProbeTarget);
		
		// Mark appropriate lights as fully baked (aka baked only)
		ILBRenderPassHandle renderPassSH = m_Lbs->Lrp.GetRenderPassLightProbes();
		for (std::vector<ILBLightHandle>::iterator it = fullyBakedLightHandles.begin(); it != fullyBakedLightHandles.end(); it++)
			ILBAddFullyBakedLight(renderPassSH, *it);
	}
	DisplayProgressbar(kExportingScene, "Starting Job", kBeastCreatingJob);
	
	// Finally render the scene
	RenderJob(job);

	ClearProgressbar();
}

// Calculates if the current configuration requires tangents to be passed to beast
inline bool NeedsTangents (Mesh& mesh, vector<Material*> materials)
{
	// Directional Lightmaps require tangents
	int lightmapsMode = GetLightmapSettings().GetLightmapsMode();
	if (lightmapsMode == LightmapSettings::kRNMMode || lightmapsMode == LightmapSettings::kDirectionalLightmapsMode)
		return true;

	// Lightmapping with normalmaps requires tangents and a Pro license
	if (GetBuildSettings().hasAdvancedVersion)
	{
		unsigned visibleSubmeshCount = std::min<unsigned>(materials.size(), mesh.GetSubMeshCount());
		for (int i=0; i < visibleSubmeshCount; i++)
		{
			if (materials[i] && materials[i]->HasProperty (ShaderLab::Property("_BumpMap")))
				return true;
		}
	}
	return false;
}

// Starts creating a Beast mesh with vertex data and UVs.
// After calling this method you need to add triangle data per material group and end the mesh.
bool LightmapperBeast::CreateMeshWithMaterials(Mesh& mesh, const dynamic_array<Vector3f>& vertices, const dynamic_array<Vector3f>& normals, const std::vector<Material*>& materials, const std::string& meshName, bool validateUVs, ILBMeshHandle& outBeastMesh)
{
	// check for mesh in the cache
	if (ILBFindMesh(m_Manager, meshName.c_str(), &outBeastMesh) == ILB_ST_SUCCESS)
		return true;

	if (!mesh.IsAvailable (kShaderChannelTexCoord0) && !mesh.IsAvailable (kShaderChannelTexCoord1))
	{
		WarningStringObject(Format("Both primary and secondary UV sets on %s are missing!", mesh.GetName()), &mesh);
		return false;
	}

	if (!mesh.IsAvailable (kShaderChannelTexCoord0))
	{
		// TODO: this shouldn't actually lead to ditching the mesh entirely - try checking if the mesh actually needs any UVs
		// (might not need any textures and have 0 scale in lightmap, so doesn't need lightmap UVs either)
		WarningStringObject(Format("Primary UV set on %s is missing!", mesh.GetName()), &mesh);
		return false;
	}
	
	const int vertexCount = mesh.GetVertexCount();
	dynamic_array<Vector2f> materialUV (vertexCount, kMemTempAlloc), bakeUVContainer;
	mesh.ExtractUvArray (0, materialUV.data ());
	if (mesh.IsAvailable(kShaderChannelTexCoord1))
	{
		bakeUVContainer.resize_uninitialized (vertexCount);
		mesh.ExtractUvArray (1, bakeUVContainer.data());
	}
	
	bool usingPrimaryUVs = bakeUVContainer.empty ();
	Vector2f* bakeUV = usingPrimaryUVs ? materialUV.data () : bakeUVContainer.data ();



	// UVs
	if(validateUVs && !ValidateUVs(bakeUV, vertexCount))
	{
		string wrongUVsWarning = Format(
			"%s UV set on %s is incorrect%s. Lightmapper needs UVs inside the [0,1]x[0,1] range. Skipping this mesh...\n"
			"Choose the \'Generate Lightmap UVs\' option in the Mesh Import Settings or provide proper UVs for lightmapping from your 3D modelling app.",
			usingPrimaryUVs ? "Primary" : "Secondary",
			mesh.GetName(),
			usingPrimaryUVs ? " and the secondary UV set is missing" : "");
		WarningStringObject(wrongUVsWarning, &mesh);
		return false;
	}

	BeastCall(ILBBeginMesh(m_Manager, meshName.c_str(), &outBeastMesh));

	BeastCall(ILBAddVertexData(outBeastMesh, reinterpret_cast<const ILBVec3*>(vertices.data()),
        reinterpret_cast<const ILBVec3*>(normals.data()), vertexCount));

	BeastCall(ILBBeginUVLayer(outBeastMesh, kUVMaterialLayer));
	BeastCall(ILBAddUVData(outBeastMesh, (const ILBVec2*)materialUV.data (), vertexCount));
	BeastCall(ILBEndUVLayer(outBeastMesh));

	BeastCall(ILBBeginUVLayer(outBeastMesh, kUVBakeLayer));
	BeastCall(ILBAddUVData(outBeastMesh, (const ILBVec2*)bakeUV, vertexCount));
	BeastCall(ILBEndUVLayer(outBeastMesh));

	// Materials
	AddMaterials(mesh, outBeastMesh, materials);

	if (NeedsTangents (mesh, materials))
	{
		dynamic_array<Vector3f> tangents, bitangents;
		tangents.reserve(vertexCount);
		bitangents.reserve(vertexCount);

		int vertexCount = mesh.GetVertexCount();
		dynamic_array<Vector4f> orgTangents;

		if (mesh.IsAvailable(kShaderChannelTangent))
		{
			orgTangents.resize_uninitialized(vertexCount);
			mesh.ExtractTangentArray(orgTangents.data());
		}

		if (normals.empty() || orgTangents.empty())
		{
			// without normals or tangents the lighting will be wrong, but it's not a fatal error
			string whatsMissing = (normals.empty() && orgTangents.empty()) ? "Normals and tangents" : (normals.empty() ? "Normals" : "Tangents");
			
			int lightmapsMode = GetLightmapSettings().GetLightmapsMode();

			string lightmapsModeName;
			bool normalMapRequirement = false;
			switch (lightmapsMode)
			{
				case LightmapSettings::kSingleLightmapsMode:
					lightmapsModeName = "Single Lightmaps";
					normalMapRequirement = true;
					break;
				case LightmapSettings::kDualLightmapsMode:
					lightmapsModeName = "Dual Lightmaps";
					normalMapRequirement = true;
					break;
				case LightmapSettings::kDirectionalLightmapsMode:
					lightmapsModeName = "Directional Lightmaps";
					break;
				case LightmapSettings::kRNMMode:
					lightmapsModeName = "RNM";
					break;
				default:
					lightmapsModeName = "Unknown Lightmaps";
			}

			WarningStringObject(Format("%s are missing on %s. Lightmapper needs both normals and tangents in %s mode%s", 
				whatsMissing.c_str(), 
				mesh.GetName(), 
				lightmapsModeName.c_str(), 
				normalMapRequirement ? " if the material has a normal map." : "."),
				&mesh);
		}
		else
		{
			for (int i = 0; i < vertexCount; i++)
			{
				Vector4f orgTangent = orgTangents[i];
				Vector3f tangent(orgTangent.x, orgTangent.y, orgTangent.z);
				tangents.push_back(tangent);
				bitangents.push_back(Cross(normals[i], tangent) * orgTangent.w);
			}

			// Tangents
			BeastCall(ILBBeginTangents(outBeastMesh));
			BeastCall(ILBAddTangentData(outBeastMesh, reinterpret_cast<const ILBVec3*>(tangents.begin()), reinterpret_cast<const ILBVec3*>(bitangents.begin()), vertexCount));
			BeastCall(ILBEndTangents(outBeastMesh));
		}
	}

	ILBStatus status = ILBEndMesh(outBeastMesh);
	if (status != ILB_ST_SUCCESS)
	{
		WarningStringObject(StatusToErrorString(status), &mesh);
		BeastCall(ILBEraseCachedMesh(m_Manager, meshName.c_str()));
		return false;
	}

	return true;
}

void LightmapperBeast::AddMaterials(Mesh& mesh, ILBMeshHandle beastMesh, vector<Material*> materials)
{
	unsigned visibleSubmeshCount = std::min<unsigned>(materials.size(), mesh.GetSubMeshCount());

	vector<string> uvLayers;
	uvLayers.reserve(visibleSubmeshCount);

	for(int i = 0; i < visibleSubmeshCount; i++)
	{
		Material* material = materials[i];
		if(!material)
			continue;

		Mesh::TemporaryIndexContainer indices;
		mesh.GetTriangles(indices, i);
		if (indices.size() == 0)
			continue;

		// create beast material
		string matName;
		CreateMaterial(matName, *material, beastMesh, mesh, uvLayers);

		// create a material group that will contain all polygons from appropriate submesh
		BeastCall(ILBBeginMaterialGroup(beastMesh, matName.c_str()));
		BeastCall(ILBAddTriangleData(beastMesh, (const int32*)&indices[0], indices.size()));
		BeastCall(ILBEndMaterialGroup(beastMesh));
	}
}

bool NeedsUVModification(Material& material, string textureName, Vector2f& scale, Vector2f& offset, bool& clamp)
{
	ShaderLab::FastPropertyName textureProperty = ShaderLab::Property(textureName);
	if (!material.HasProperty(textureProperty))
		return false;

	Texture* texture = material.GetTexture(textureProperty);
	if (!texture)
		return false;
	offset = material.GetTextureOffset(textureProperty);
	scale = material.GetTextureScale(textureProperty);
	clamp = texture->GetWrapMode() == kTexWrapClamp;

	return offset.x != 0 || offset.y != 0 || scale.x != 1 || scale.y != 1 || clamp;
}

bool NeedsUVModification(Material& material, string textureName)
{
	Vector2f scale, offset;
	bool clamp;
	return NeedsUVModification(material, textureName, scale, offset, clamp);
}

void ScaleOffsetClampUVs (Mesh& mesh, Material& material, string textureName, ILBMeshHandle beastMesh, ILBMaterialHandle mat, ILBMaterialChannel channel, vector<string>& uvLayers)
{
	Vector2f scale, offset;
	bool clamp;
	if (!NeedsUVModification (material, textureName, scale, offset, clamp))
	{
		// if the UV channel doesn't need modification, just set it for the Beast's material channel
		BeastCall(ILBSetChannelUVLayer(mat, channel, kUVMaterialLayer));
		return;
	}

	// UV layers are local for a mesh, so use a name that we can use for cache (uvLayers vector) lookup
	std::string modifiedLayerName = Format("%f,%f,%f,%f,%i", scale.x, scale.y, offset.x, offset.y, clamp ? 1 : 0);

	if (find(uvLayers, modifiedLayerName) == uvLayers.end())
	{
		// modify the UVs
		const int vertexCount = mesh.GetVertexCount();
		
		StrideIterator<Vector2f> orgUV = mesh.GetUvBegin();
		vector<Vector2f> modifiedUV;
		modifiedUV.resize(vertexCount);
		for(vector<Vector2f>::iterator it = modifiedUV.begin (), end = modifiedUV.end (); it != end; ++it, ++orgUV)
		{
			Vector2f v = *orgUV;
			v.x = v.x*scale.x + offset.x;
			v.y = v.y*scale.y + offset.y;
			if (clamp)
			{
				v.x = clamp01<float>(v.x);
				v.y = clamp01<float>(v.y);
			}
			*it = v;
		}

		// create the UV layer
		BeastCall(ILBBeginUVLayer(beastMesh, modifiedLayerName.c_str()));
		BeastCall(ILBAddUVData(beastMesh, (const ILBVec2*)(&modifiedUV[0]), vertexCount));
		BeastCall(ILBEndUVLayer(beastMesh));

		uvLayers.push_back(modifiedLayerName);
	}

	// assign the UV layer to Beast's material channel for the beastMesh
	BeastCall(ILBSetChannelUVLayer(mat, channel, modifiedLayerName.c_str()));
}

ColorRGBAf GetColor(Material& material, const string& propertyName)
{
	ColorRGBAf color = ColorRGBAf(1.0,1.0,1.0,1.0);
	ShaderLab::FastPropertyName colorProperty = ShaderLab::Property(propertyName);
	if (material.HasProperty(colorProperty))
		color = material.GetColor(colorProperty);

	return color;
}

static ILBTextureHandle SetTextureAndColor (ILBManagerHandle manager, Mesh& mesh, Material& material, ILBMeshHandle beastMesh, ILBMaterialHandle beastMaterial, std::string texturePropName, std::string colorPropName, bool useOnlyAlphaFromTexture, std::vector<string>& uvLayers, ILBMaterialChannel channel)
{
	ILBTextureHandle beastColorTimesTexture = NULL;
	ColorRGBAf color = GetColor (material, colorPropName);

	const ColorRGBA32 color32 (color);
	const ColorRGBA32 white32 (255, 255, 255, 255);

	Texture2D* texture = dynamic_pptr_cast<Texture2D*> (material.GetTexture (ShaderLab::Property (texturePropName)));
	if(texture)
	{
		// compare 8-bit channels, since white transformed to linear space is not exactly 1.0f
		if (color32 != white32)
		{
			// multiply texture by color
			Texture2D* colorTimesTexture = CreateObjectFromCode<Texture2D> ();
			CombineTextureAndColor (*texture, color, *colorTimesTexture, useOnlyAlphaFromTexture);
			beastColorTimesTexture = BeastUtils::Texture2DToBeastTexture (*colorTimesTexture, manager);
			BeastCall (ILBSetMaterialTexture (beastMaterial, channel, beastColorTimesTexture));
			DestroySingleObject (colorTimesTexture);
		}
		else
		{
			// main color is white, just set the main tex for the diffuse channel
			beastColorTimesTexture = BeastUtils::Texture2DToBeastTexture (*texture, manager);
			BeastCall (ILBSetMaterialTexture (beastMaterial, channel, beastColorTimesTexture));
		}
		ScaleOffsetClampUVs (mesh, material, texturePropName, beastMesh, beastMaterial, channel, uvLayers);
	}
	else
	{
		ILBLinearRGBA beastColor (color.r, color.g, color.b, color.a);
		BeastCall (ILBSetMaterialColor (beastMaterial, channel, &beastColor));
	}

	return beastColorTimesTexture;
}

ILBMaterialHandle LightmapperBeast::CreateMaterial (string& beastMatName, Material& material, ILBMeshHandle beastMesh, Mesh& mesh, vector<string>& uvLayers)
{
	beastMatName = Format ("Material-%i", material.GetInstanceID());
	ILBMaterialHandle beastMaterial;

	int lightmapsMode = GetLightmapSettings ().GetLightmapsMode ();
	bool needsBumpMap = GetBuildSettings().hasAdvancedVersion && (lightmapsMode != LightmapSettings::kRNMMode && lightmapsMode != LightmapSettings::kDirectionalLightmapsMode);

	// check for the material in the cache, but it still needs to be processed for every mesh, 
	// if mesh UVs need modification due to some clamp/scale/offset texture settings
	if (ILBFindMaterial (m_Scene, beastMatName.c_str(), &beastMaterial) == ILB_ST_SUCCESS)
	{
		if (NeedsUVModification (material, "_MainTex") ||
			NeedsUVModification (material, "_Illum") ||
			NeedsUVModification (material, "_TransparencyLM")||
			(needsBumpMap && NeedsUVModification (material, "_BumpMap")))
		{
			// if the material is in the cache, but we need to make a new version with modified UVs,
			// it should also have it's name modified by the name of the mesh (can't have two materials with identical names)
			beastMatName.append (Format ("-Mesh-%i", mesh.GetInstanceID()));
			// check again in the cache - maybe we already have a version for this mesh
			if (ILBFindMaterial (m_Scene, beastMatName.c_str(), &beastMaterial) == ILB_ST_SUCCESS)
				return beastMaterial;
		}
		else
		{
			// if the material is in the cache and we don't need new UVs, use it!
			return beastMaterial;
		}
	}

	std::string shaderName (material.GetShader()->GetName());

	BeastCall (ILBCreateMaterial (m_Scene, beastMatName.c_str (), &beastMaterial));

	ColorRGBAf mainColor = GetColor (material, "_Color");
	Texture2D* mainTexture = dynamic_pptr_cast<Texture2D*> (material.GetTexture (ShaderLab::Property ("_MainTex")));

	// Diffuse channel
	ILBTextureHandle beastMainColorTimesMainTexture = SetTextureAndColor (m_Manager, mesh, material, beastMesh, beastMaterial, "_MainTex", "_Color", false, uvLayers, ILB_CC_DIFFUSE);

	// Specular channel
	if (shaderName.find ("Specular") != string::npos)
	{
		SetTextureAndColor (m_Manager, mesh, material, beastMesh, beastMaterial, "_MainTex", "_SpecColor", true, uvLayers, ILB_CC_SPECULAR);

		// Shininess
		float shininess = material.GetFloat (ShaderLab::Property ("_Shininess"));
		BeastCall (ILBSetMaterialScale (beastMaterial, ILB_CC_SHININESS, shininess));
	}

	// Emissive channel
	if (shaderName.find ("Self-Illumin") != string::npos)
	{
		float emissionMultiplier = material.GetFloat (ShaderLab::Property ("_EmissionLM"));
		
		if (emissionMultiplier > 0.0f)
		{
			BeastCall (ILBSetMaterialScale (beastMaterial, ILB_CC_EMISSIVE, emissionMultiplier));

			Texture2D* illumTexture = dynamic_pptr_cast<Texture2D*> (material.GetTexture (ShaderLab::Property ("_Illum")));
			if (illumTexture)
			{
				// if the illumination texture is set, combine it with the main texture and color and set for the emissive channel
				Texture2D* combinedIllumTexture = CreateObjectFromCode<Texture2D> ();
				if (mainTexture)
					CombineTextures (*mainTexture, *illumTexture, mainColor, *combinedIllumTexture);
				else
					CombineTextureAndColor (*illumTexture, mainColor, *combinedIllumTexture, true);
				ILBTextureHandle beastCombinedIllumTexture = BeastUtils::Texture2DToBeastTexture (*combinedIllumTexture, m_Manager);
				BeastCall (ILBSetMaterialTexture (beastMaterial, ILB_CC_EMISSIVE, beastCombinedIllumTexture));
				DestroySingleObject (combinedIllumTexture);

				// use clamp/scale/offset settings of the _Illum texture (this will ignore these settings from the _MainTex, though)
				ScaleOffsetClampUVs (mesh, material, "_Illum", beastMesh, beastMaterial, ILB_CC_EMISSIVE, uvLayers);
			}
			else if (beastMainColorTimesMainTexture)
			{
				// main texture multiplied by the main color was already exported
				BeastCall (ILBSetMaterialTexture (beastMaterial, ILB_CC_EMISSIVE, beastMainColorTimesMainTexture));

				// use clamp/scale/offset settings of the _MainTex texture
				ScaleOffsetClampUVs (mesh, material, "_MainTex", beastMesh, beastMaterial, ILB_CC_EMISSIVE, uvLayers);
			}
			else
			{
				ILBLinearRGBA beastColor (mainColor.r, mainColor.g, mainColor.b, mainColor.a);
				BeastCall (ILBSetMaterialColor (beastMaterial, ILB_CC_EMISSIVE, &beastColor));
			}
		}
	}

	// Has and needs a normal map
	if (needsBumpMap && material.HasProperty (ShaderLab::Property("_BumpMap")))
	{
		Texture2D* bumpMapTexture = dynamic_pptr_cast<Texture2D*> (material.GetTexture(ShaderLab::Property("_BumpMap")));
		if (bumpMapTexture)
		{
			ILBTextureHandle beastBumpMapTexture = BeastUtils::Texture2DToBeastTexture (*bumpMapTexture, m_Manager);
			BeastCall(ILBSetMaterialTexture (beastMaterial, ILB_CC_NORMAL, beastBumpMapTexture));
			ScaleOffsetClampUVs (mesh, material, "_BumpMap", beastMesh, beastMaterial, ILB_CC_NORMAL, uvLayers);
		}
	}

	// Transparency channel
	if (material.HasProperty (ShaderLab::Property ("_TransparencyLM")))
	{
		// set RGB-style transparency
		Texture2D* transparencyLMTexture = dynamic_pptr_cast<Texture2D*> (material.GetTexture (ShaderLab::Property ("_TransparencyLM")));
		if (transparencyLMTexture)
		{
			ILBTextureHandle beastTransparencyLMTexture = BeastUtils::Texture2DToBeastTexture (*transparencyLMTexture, m_Manager);
			BeastCall (ILBSetMaterialTexture (beastMaterial, ILB_CC_TRANSPARENCY, beastTransparencyLMTexture));
			ScaleOffsetClampUVs (mesh, material, "_TransparencyLM", beastMesh, beastMaterial, ILB_CC_TRANSPARENCY, uvLayers);
		}
	}
	else if(shaderName.find ("Transparent") != string::npos ||
		(shaderName.find ("Tree") != string::npos && (shaderName.find ("Leaf") != string::npos || shaderName.find ("Leaves") != string::npos)))
	{
		// set traditional transparency model
		BeastCall (ILBSetAlphaAsTransparency (beastMaterial, true));

		if (material.HasProperty (ShaderLab::Property ("_Cutoff")))
		{
			// If there's a _Cutoff value defined, let's take the main texture's alpha channel,
			// clamp it to 0 or 1 (depending if below or above cutoff) and send it to Beast
			// as the ILB_CC_TRANSPARENCY channel texture.
			// (Could be also done by modifying the _MainTex's alpha, setting it as ILB_CC_DIFFUSE
			// and enabling ILBSetAlphaAsTransparency, but this seems cleaner.)
			if (mainTexture)
			{
				BeastCall (ILBSetAlphaAsTransparency (beastMaterial, false));

				float cutoff = material.GetFloat (ShaderLab::Property ("_Cutoff")) / mainColor.a;
				Texture2D* alphaCutout = CreateObjectFromCode<Texture2D> ();
				ClampAlphaForCutout (*mainTexture, cutoff, *alphaCutout);
				ILBTextureHandle beastAlphaCutoutTexture = BeastUtils::Texture2DToBeastTexture (*alphaCutout, m_Manager);
				BeastCall (ILBSetMaterialTexture (beastMaterial, ILB_CC_TRANSPARENCY, beastAlphaCutoutTexture));
				DestroySingleObject (alphaCutout);
				ScaleOffsetClampUVs (mesh, material, "_MainTex", beastMesh, beastMaterial, ILB_CC_TRANSPARENCY, uvLayers);
			}
		}
	}

	return beastMaterial;
}

void LightmapperBeast::CreateConfigFile(const string& xmlFileName, bool sky) const
{
	// If <SceneName>/BeastSettings.xml exists, use it to override our settings
	string config = GetSceneBakedAssetsPath();
	config.append("/BeastSettings.xml");
	if( IsFileCreated(config) )
	{
		CopyReplaceFile(config, xmlFileName);
		return;
	}

	// Otherwise create our own xml settings file
	LightmapEditorSettings& les = GetLightmapEditorSettings();

	// High quality bake settings
	bool fgCheckVisibility = true;
	float minSampleRate = 0;
	float maxSampleRate = 2;
	std::string secondaryIntegrator = "None";
	bool fgPreview = false;
	int maxShadowRays = 10000;
	std::string filter = "Gauss";
	float filterSize = 2.2f;

	// Low quality bake settings
	if(les.GetQuality() == LightmapEditorSettings::kBakeQualityLow)
	{
		fgCheckVisibility = false;
		minSampleRate = 0;
		maxSampleRate = 0;
		// might be worth a try, but the Beast guys have to make the output of different integrators to match
		//secondaryIntegrator = "PathTracer";
		fgPreview = true;
		maxShadowRays = 4;
		filter = "Box";
		filterSize = 1.0f;
	}
	

	// Create settings xml file
	{
		using namespace bex;
		std::ofstream ofs(xmlFileName.c_str(), std::ios_base::out | std::ios_base::trunc);
		XMLWriter xml(ofs);
		
		{ScopedTag _x(xml, "ILConfig");
			{ScopedTag _x(xml, "AASettings");
				xml.data("samplingMode", "Adaptive");
				xml.data("clamp", false);
				xml.data("contrast", 0.1f);
				xml.data("diagnose", false);
				xml.data("minSampleRate", minSampleRate);
				xml.data("maxSampleRate", maxSampleRate);
				xml.data("filter", filter);
				{ScopedTag _x(xml, "filterSize");
					xml.data("x", filterSize);
					xml.data("y", filterSize);
				}
			}
			{ScopedTag _x(xml, "RenderSettings");
				// Bias needed to avoid shadow rays intersecting the same triangle as the primary ray did.
				// If set to zero this value is computed automatically depending on the scene size.
				xml.data("bias", 0.0f);
				xml.data("maxShadowRays", maxShadowRays);
			}
			{ScopedTag _x(xml, "EnvironmentSettings");
				float skyLightIntensity = les.GetSkyLightIntensity();
				if (skyLightIntensity > 0.0f) {
					xml.data("giEnvironment", "SkyLight");
					ColorRGBAf c = les.GetSkyLightColor();
					xml.data<ILBLinearRGBA>("skyLightColor", ILBLinearRGBA(c.r,c.g,c.b,c.a));
					xml.data("giEnvironmentIntensity", skyLightIntensity);
				}
			}
			{ScopedTag _x(xml, "FrameSettings");
				xml.data("inputGamma", 1);
			}
			
			{ScopedTag _x(xml, "SurfaceTransferSettings");
				xml.data("frontRange", 0.0F);
				xml.data("frontBias", 0.0F);
				xml.data("backRange", les.GetLODSurfaceMappingDistance() * 2.0F);
				xml.data("backBias", -les.GetLODSurfaceMappingDistance());
				xml.data("selectionMode", "Normal");
			}
			{ScopedTag _x(xml, "GISettings");
				xml.data("enableGI", les.GetBounces() > 0);
				xml.data("fgPreview", fgPreview);
				xml.data("fgRays", les.GetFinalGatherRays());
				xml.data("fgContrastThreshold", les.GetFinalGatherContrastThreshold());
				xml.data("fgGradientThreshold", les.GetFinalGatherGradientThreshold());
				xml.data("fgCheckVisibility", fgCheckVisibility);
				xml.data("fgInterpolationPoints", les.GetFinalGatherInterpolationPoints());
				xml.data("fgDepth", les.GetBounces());
				xml.data("primaryIntegrator", "FinalGather");
				xml.data("primaryIntensity", les.GetBounceIntensity());
				xml.data("primarySaturation", 1);
				xml.data("secondaryIntegrator", secondaryIntegrator);
				xml.data("secondaryIntensity", 1);
				xml.data("secondarySaturation", 1);
				xml.data("diffuseBoost", les.GetBounceBoost());
			}
			{ScopedTag _x(xml, "TextureBakeSettings");
				{ScopedTag _x(xml, "bgColor");
					xml.data("r", 1);
					xml.data("g", 1);
					xml.data("b", 1);
					xml.data("a", 1);
				}

				// make sure the lightmap can be bilinearly filtered without sampling the background in the biggest mip level
				xml.data("bilinearFilter", true);

				// lighting will be calculated for texels that are merely touched by bake instances
				xml.data("conservativeRasterization", true);

				// don't use Beast's edge dilation, since a better algorithm is part of the import pipeline
				xml.data("edgeDilation", 0);
			}
		}
	}
}

void LightmapperBeast::RenderJob(ILBJobHandle job, ILBDistributionType distribution) const
{
	BeastCall(ILBStartJob(job, ILB_SR_NO_DISPLAY, distribution));
}


#endif // #if ENABLE_LIGHTMAPPER
