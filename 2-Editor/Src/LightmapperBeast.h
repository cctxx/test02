#ifndef LIGHTMAPPERBEAST_H
#define LIGHTMAPPERBEAST_H

#if ENABLE_LIGHTMAPPER


#if defined(WIN32)
#include <tchar.h>
#else
#ifndef _TCHAR
#define _TCHAR char
#define TCHAR _TCHAR
#define _T(x) (x)
#define _tmain main
#endif
#endif

#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/Light.h"
#include "Editor/Src/LightmapperLightProbes.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Graphics/ImageConversion.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Lightmapping.h"
#include "Runtime/Threads/Mutex.h"
#include "SceneInspector.h"
#include "Runtime/Terrain/TerrainData.h"
#include "ExtractTerrainMesh.h"

#include "External/Beast/builds/include/beastapi/beastapitypes.h"
#include "External/Beast/builds/include/beastapi/beastjob.h"
#include "External/Beast/builds/include/beastapi/beastutils.h"
#include "External/Beast/builds/include/beastapi/beaststring.h"
#include "External/Beast/builds/include/beastapi/beastmaterial.h"
#include "External/Beast/builds/include/beastapi/beasttexture.h"

// ----------------------------------------------------------------------
//  LightmapperBeastCommon
// ----------------------------------------------------------------------

// Converts Beast's string handle into std::string.
inline std::string ConvertStringHandle(ILBStringHandle h)
{
	int32 len;
	ILBGetLength(h, &len);
	// len -1 since basic_string allocates the null character
	std::basic_string<TCHAR> result(len - 1, '\0');
	ILBCopy(h, &result[0], static_cast<int32>(len));
	ILBReleaseString(h);
	return std::string(result.c_str(), result.size());
}

inline std::string StatusToErrorString(ILBStatus status)
{
	ILBStringHandle errorInfoHandle, extendedErrorInfoHandle;
	ILBErrorToString(status, &errorInfoHandle);
	std::string errorInfo = ConvertStringHandle(errorInfoHandle);
	ILBGetExtendErrorInformation(&extendedErrorInfoHandle);
	std::string extendedErrorInfo = ConvertStringHandle(extendedErrorInfoHandle);
	return Format("Something went wrong in the Beast lightmapper: %s%s%s This is a bug - please report it with a scene reproducing the issue.\n",
		errorInfo.c_str(), extendedErrorInfo.size() > 0 && errorInfo.size() > 0 ? " - " : "", extendedErrorInfo.c_str());
}

class BeastException
{
public:
	BeastException(ILBStatus s, Object* object = NULL) : m_What(StatusToErrorString(s)), m_Object(object) { ClearProgressbar (); }
	BeastException(std::string what, Object* object = NULL) : m_What(what), m_Object(object) { }
	
	std::string What() { return m_What; }
	Object* WhatObject() { return m_Object; }
private:
	std::string m_What;
	Object* m_Object;
};

// A wrapper around a Beast API call. Transforms return codes into exceptions.
#if UNITY_LINUX
// a dummy on Linux:
#define BeastCall(x)
#else
inline void BeastCall(ILBStatus status, Object* object = NULL)
{
	if(status != ILB_ST_SUCCESS)
	{
		throw BeastException(status, object);
	}
}
#endif

struct LightmapperDestinationInstance
{
	LightmapperDestinationInstance (Renderer* r, bool s, Rectf& uvb) : renderer (r), selected (s), uvBounds (uvb) {}

	Renderer* renderer;
	Rectf uvBounds;
	Vector2f uvSize;
	float size;
	bool selected;
};

struct LightmapperDestinationInstanceHandle : public LightmapperDestinationInstance
{
	LightmapperDestinationInstanceHandle (ILBInstanceHandle ih, Renderer* r, Rectf& uvb) : instanceHandle (ih), LightmapperDestinationInstance (r, true, uvb) {}

	ILBInstanceHandle instanceHandle;
};

typedef std::vector<LightmapperDestinationInstanceHandle> InstanceHandleToMeshRenderer;
typedef std::vector<LightmapperDestinationInstance> RendererAndUVSizes;
class LightmapRenderingPasses {
public:
	LightmapRenderingPasses()
	: m_RenderPass (0), m_RenderPassAO (0), m_RenderPassLightProbes (0),
	m_AOPassAmount (0), m_LightmapsMode(LightmapSettings::kDualLightmapsMode) { }

    void CreateRenderPass(ILBJobHandle job);
	void CreateRenderPassLightProbes(ILBJobHandle job);

	// Adds render passes to target
	void AddRenderPassToTarget(ILBTargetHandle target);
	void AddRenderPassLightProbesToTarget(ILBTargetHandle target);

	// Sets up lightmapData by obtaining textures for the given target and framebufferIndex
	string GetLightmapDataForTarget(const ILBTargetHandle target, const int lightmapIndex, const int lightmapOffset, const bool temp);

	ILBRenderPassHandle GetRenderPass() { return m_RenderPass; }
	ILBRenderPassHandle GetRenderPassLightProbes() { return m_RenderPassLightProbes; }
	ILBRenderPassHandle GetRenderPassAO() { return m_RenderPassAO; }
	int GetLightmapsMode() { return m_LightmapsMode; }
	ColorRGBAf GetAmbientLight() { return m_AmbientLight; }
	float GetAOAmount() { return m_AOPassAmount; }

private:
	ILBRenderPassHandle m_RenderPass; // this can be either DualLightmaps pass or FullIllumination (i.e. single lightmap)
	ILBRenderPassHandle m_RenderPassAO; // optional AO pass
	ILBRenderPassHandle m_RenderPassLightProbes;
	float m_AOPassAmount;
	int m_LightmapsMode;
	ColorRGBAf m_AmbientLight;
};

// ----------------------------------------------------------------------
//  LightmapperBeastCompute
// ----------------------------------------------------------------------

class LightmapperJobProgress
{
public:
	LightmapperJobProgress() { m_Activity = ""; m_Error = ""; };
	LightmappingProgress GetProgressState();
	void SetProgressState(LightmappingProgress progressState);
	float GetProgress();
	void SetProgress(float progress);
	std::string GetActivity();
	void SetActivity(const string& activity);
	std::string GetError();
	void SetError(const string& error);
private:
	Mutex m_Mutex;
	LightmappingProgress m_ProgressState;
	float m_Progress;
	std::string m_Activity;
	std::string m_Error;
};

// Info  that needs to be stored per bake instance.
// It is set up during the prepare phase and used in retrieve.
struct BakedInstanceInfo
{
	ILBTargetEntityHandle bakeInstance; 
	ILBTargetHandle target;
    PPtr<Renderer> renderer;
	Rectf uvBounds;
};
typedef std::vector<BakedInstanceInfo> BakedInstances;

struct Atlasing
{
	dynamic_array<Vector2f> scales;
	dynamic_array<Vector2f> offsets;
	dynamic_array<int> indices;
	int atlasCount;
};

class LightmapperBeastShared : ISceneInspector
{
public:

	// Atlas target for all the bake instances
	ILBTargetHandle AtlasTarget;

	// Terrain datas and positions
	std::vector<TerrainBakeInfo> Terrains;

	// Texture targets to use instead of atlased textured target to use when lockAtlas == true
	std::vector<ILBTargetHandle> TextureTargets;

	std::vector<bool> TextureTargetsUsed;

	// Point cloud stuff
	ILBTargetHandle m_LightProbeTarget;
	ILBTargetEntityHandle m_LightProbeEntity;
	dynamic_array<Vector3f> m_LightProbeSourcePositions;

	ILBJobHandle Job;
	LightmapRenderingPasses Lrp;

	// Thread responsible for polling Beast for progress. Beast stalls when not polled.
	Thread ProgressThread;

	// Stores progress information.
	LightmapperJobProgress JobProgress;

	bool m_KeepOldAtlasingInBakeSelected;

	// Handles to Beast bake instances and the corresponding texture targets and mesh renderers.
	BakedInstances m_BakedInstances;
	// Atlasing used by Beast in the current bake. Matches the order of m_BakedInstances.
	Atlasing m_Atlasing;

	std::vector<LightmapperDestinationInstance> m_DestinationInstances;
	// Atlasing for all the objects that were lightmapped before and are selected now.
	Atlasing m_DestinationAtlasing;

	// Handles to Beast instances that will not get lightmaps
	vector< PPtr<Object> > m_NonBakedInstances;

	// PPtrs of lights that will be or will not be baked
	std::vector< PPtr<Light> > m_BakedLights;
	std::vector< PPtr<Light> > m_NonBakedLights;
	
	// The time when the user pressed 'Bake', so before scene export
	float m_BakeStartTime;

	// Was atlas locking enabled? If so, use TextureTargets. If not, use AtlasedTarget
	bool m_LockAtlas;

	// If this bake was for light probes only, all texture and atlas targets can be ignored
	bool m_LightProbesOnly;
	
	// If this bake was for selected objects
	bool m_SelectedOnly;

	int m_MaxAtlasSize;

	// Computes lightmaps and retrieves them synchronously
	void Compute();

	// Computes lightmaps asynchronously
	void ComputeAsync();

	// Cancels the thread.
	void Cancel();

private:
	virtual void TickInspectorBackground();
	virtual bool CanOpenScene();
	virtual void DidOpenScene();
	virtual bool CanEnterPlaymode();
	virtual bool CanTerminate();
	// Used in a separate thread to poll Beast for progress.
	static void* PollBeastForProgress(void *data);
};

// ----------------------------------------------------------------------
//  BeastUtils
// ----------------------------------------------------------------------

// Utilities that are Beast specific.
namespace BeastUtils
{
    typedef std::pair<ILBTargetHandle, int32> TargetFramebuffer;

	/// Prepare

	inline ILBTextureHandle Texture2DToBeastTexture(Texture2D& texture, ILBManagerHandle bm, ILBPixelFormat beastFormat = ILB_PF_RGBA_BYTE);
	inline void SetSpotlightCookie(const Light& light, ILBLightHandle beastLight, ILBManagerHandle bm);
	inline void SetLightRamp(const Light& light, ILBLightHandle beastLight);
	inline bool ValidateUVs(Vector2f* uv, int vertexCount);


	/// Compute

	// Transforms Beast job result status code to true when the job succeeded, false otherwise.
	bool GetJobResult(ILBJobHandle job);


	/// Retrieve

	Texture2D* GetTerrainTextureFromTarget(ILBTargetHandle target, LightmapRenderingPasses& lrp);

	// Multiplies the first texture by the color and by the alpha channel of the second texture and outputs to result. Used for Unity Self-Illum -> Beast Emissive material conversion.
	void CombineTextures(Texture2D& textureRGB, Texture2D& textureA, ColorRGBAf& color, Texture2D& result);
	// Multiplies the texture by the color
	void CombineTextureAndColor(Texture2D& textureRGBA, ColorRGBAf& color, Texture2D& result, bool useOnlyAlphaFromTexture);
	// Clamps alpha channel to match transparent cutout shaders behaviour and outputs in Beast's RGBTransparency-friendly way:
	// result.rgba = (textureRGBA.a > cutoff ? 0 : 255).rrrr
	void ClampAlphaForCutout(Texture2D& textureRGBA, float cutoff, Texture2D& result);

	Vector3f GetTreeInstancePosition(const TreeInstance& instance, const Vector3f& terrainSize, const Vector3f& terrainPosition);

	void HandleCastReceiveShadows(const ILBInstanceHandle& instance, bool castShadows);

	bool IsHighestLODLevel (Renderer& renderer);
	int  GetHighestLODLevel (Renderer& renderer);
	
	// Sets the lightmap index on all renderers to 0xFF (not lightmapped).
	void ClearLightmapIndices();
};


// ----------------------------------------------------------------------
//  TreeLightmapper
// ----------------------------------------------------------------------

namespace TreeLightmapper
{
	std::vector<Light*> GetLights(LightType type, int& maxShadowSamples);
	ColorRGBAf CalculateTreeLightmapColor(const std::vector<Light*>& lights, const vector<Quaternionf>& shadowJitter, const Vector3f& position, float maxDistance, float totalLightAmount, float grayscaleAmbient);
	vector<Quaternionf> SuperSampleShadowJitter (int shadowSamples);
	void LightmapTrees(TerrainData* terrainData, const Vector3f terrainPosition);
};

// ----------------------------------------------------------------------
//  LightmapperBeast
// ----------------------------------------------------------------------

// Lightmapper implementation that uses Beast.
class LightmapperBeast {
public:
    LightmapperBeast();

	void Prepare (LightmapperBeastShared*& lbs, bool bakeSelected, bool lightProbesOnly);

private:
	void SendScene (bool bakeSelected, bool lightProbesOnly, std::vector<ILBLightHandle>& fullyBakedLightHandles, ILBPointCloudHandle &pointCloud);
	void BeginScene ();
	void AddSceneObject(Renderer* renderer, Mesh& mesh, ILBMeshHandle beastMesh, int index, bool isSkinnedMesh, bool bake);
	void AddSceneObjects (bool bakeSelected);
	void AddMeshRenderers (std::vector<MeshRenderer*> &renderers, int& index, bool bake);
	void AddSkinnedMeshRenderers (std::vector<SkinnedMeshRenderer*> &renderers, int& index, bool bake);
	void CreateMeshAndAddSceneObject (Renderer* r, Mesh* mesh, const dynamic_array<Vector3f>& vertices, const dynamic_array<Vector3f>& normals, bool isSkinnedMesh, int& index, bool bake);
	void AddTerrains ();
	ILBPointCloudHandle AddLightProbes (string& outError);
	ILBLightHandle AddSceneLight (const Light& light, int index);
	void AddSceneLights(std::vector<ILBLightHandle>& fullyBakedLightHandles);
	void EndScene ();

	void PrepareLightmapBakeTargets (bool bakeSelected, bool lightProbesOnly, const std::vector<ILBLightHandle>& fullyBakedLightHandles);

	void AddTerrainPatches(TerrainData* terrainData, const Vector3f& terrainPosition, bool castShadows, int terrainIndex, Material* templateMat);
	void AddTrees(TerrainData* terrainData, const Vector3f& terrainPosition, bool castShadows);

	void AddMaterials(Mesh& mesh, ILBMeshHandle beastMesh, vector<Material*> materials);
	// Return false if mesh creation failed
	bool CreateMeshWithMaterials(Mesh& m, const dynamic_array<Vector3f>& vertices, const dynamic_array<Vector3f>& normals, const std::vector<Material*>& materials, const std::string& meshName, bool validateUVs, ILBMeshHandle& outMeshHandle);
	ILBMaterialHandle CreateMaterial(string& matName, Material& material, ILBMeshHandle beastMesh, Mesh& mesh, vector<string>& uvLayers);

	void CreateConfigFile(const string& xmlFileName, bool sky) const;
	void RenderJob(ILBJobHandle job, ILBDistributionType distribution = ILB_RD_FORCE_LOCAL) const;
    
    ILBManagerHandle m_Manager;
    ILBSceneHandle m_Scene;

    // handles to Beast instances that will be atlased and baked with pointers to their appriopriate renderers
	InstanceHandleToMeshRenderer m_InstanceHandleToMeshRenderer;
	// handles to Beast instances of terrain patches, one vector per terrain
	vector<vector<ILBInstanceHandle> > m_TerrainPatchInstances;
	dynamic_array<TerrainBakeInfo> m_Terrains;
	
	// This object is passed to an instance of LightmapperBeastResults class,
	// and that class is responsible for destroying it
	LightmapperBeastShared* m_Lbs;

	float m_ObjectsCombinedArea;
};

// ----------------------------------------------------------------------
//  LightmapperBeastRetrieve
// ----------------------------------------------------------------------

struct AtlasInfo
{
	int index;
	Vector2f scale;
	Vector2f offset;
};

struct AtlasInfoSourceDest
{
	AtlasInfo dest;
	AtlasInfo source;
};

class LightmapperBeastResults {
public:
	enum { kLightmapPathsStride = 3, kLightmapPathsSize = 12 };
	static const char* const kLightmapPaths[kLightmapPathsSize];

	LightmapperBeastResults(LightmapperBeastShared* lbs);
	void Retrieve();

private:
	void UpdateAtlasingOnBakeInstances(int lastLightmapIndex);
	void FetchAtlasing(vector<AtlasInfoSourceDest>& atlasInfoSelected, vector<AtlasInfoSourceDest>& atlasInfoNonSelected);

	// Gets all the textures created by Beast for a given target.
	int GetTexturesFromTarget(ILBTargetHandle target);
	void RetrieveLightmaps();
	void RetrieveLightProbes();

	LightmapperBeastShared* m_Lbs;
	std::vector<string> m_LightmapAssetPaths;
};

#endif // #if ENABLE_LIGHTMAPPER

#endif
