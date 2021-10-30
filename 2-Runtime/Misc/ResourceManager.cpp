#include "UnityPrefix.h"
#include "ResourceManager.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "Configuration/UnityConfigureOther.h"
#include "Runtime/Misc/BuildSettings.h"
#if ENABLE_MONO
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoManager.h"
#endif
#if UNITY_EDITOR
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#include <vector>
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Graphs/GraphUtils.h"
#include "Editor/Src/EditorModules.h"
#include "ResourceManagerGUIDs.h"
#endif

using namespace std;

const char* kResourcePath = "Library/unity default resources";
const char* kOldWebResourcePath = "Library/unity_web_old";

static bool s_AreResourcesInitialized;

#if UNITY_EDITOR
static const char* kDefaultResourcesSource = "Resources/unity default resources";

static const char* kEditorDefaultResourcesSource = "Resources/unity editor resources";
const char* kEditorResourcePath = "Library/unity editor resources";
const char* kDefaultExtraResourcesPath = "Resources/unity_builtin_extra";

static BuiltinResourceManager* gBuiltinExtraResourceManager;

static void RegisterBuiltinEditorScript (int instanceID, const char*  className);
static void RegisterBuiltinEditorInternalScript (int instanceID, const char*  className);
static void DoRegisterBuiltinEditorScript (int instanceID, const char*  className, const char* _namespace, const char* assemblyName, bool isEditorScript);

void BuiltinResourceManager::InitializeExtraResources ()
{
	m_ResourcePath = kDefaultExtraResourcesPath;
	m_RequiredHideFlags = Object::kNotEditable;
	m_AllowResourceManagerAccess = true;

	// Issue RegisterResource shader calls in the expected loading order:
	// if any shader references another (e.g. UsePass), the referenced must come first!
	
	// Regular opaque
	RegisterShader (6, "Normal-VertexLit.shader", "VertexLit");
	RegisterShader (1, "Normal-DiffuseFast.shader", "Legacy Shaders/Diffuse Fast");
	RegisterShader (7, "Normal-Diffuse.shader", "Diffuse");
	RegisterShader (2, "Normal-Bumped.shader", "Bumped Diffuse");
	RegisterShader (3, "Normal-Glossy.shader", "Specular");
	RegisterShader (4, "Normal-BumpSpec.shader", "Bumped Specular");
	RegisterShader (5, "Normal-DiffuseDetail.shader", "Diffuse Detail");
	// Self-Illumin
	RegisterShader (14, "Illumin-VertexLit.shader", "Self-Illumin/VertexLit");
	RegisterShader (10, "Illumin-Diffuse.shader", "Self-Illumin/Diffuse");
	RegisterShader (11, "Illumin-Bumped.shader", "Self-Illumin/Bumped Diffuse");
	RegisterShader (12, "Illumin-Glossy.shader", "Self-Illumin/Specular");
	RegisterShader (13, "Illumin-BumpSpec.shader", "Self-Illumin/Bumped Specular");
	// Reflective
	RegisterShader (24, "Reflect-VertexLit.shader", "Reflective/VertexLit");
	RegisterShader (25, "Reflect-BumpNolight.shader", "Reflective/Bumped Unlit");
	RegisterShader (20, "Reflect-Diffuse.shader", "Reflective/Diffuse");
	RegisterShader (21, "Reflect-Bumped.shader", "Reflective/Bumped Diffuse");
	RegisterShader (26, "Reflect-BumpVertexLit.shader", "Reflective/Bumped VertexLit");
	RegisterShader (22, "Reflect-Glossy.shader", "Reflective/Specular");
	RegisterShader (23, "Reflect-BumpSpec.shader", "Reflective/Bumped Specular");
	// Transparent
	RegisterShader (34, "Alpha-VertexLit.shader", "Transparent/VertexLit");
	RegisterShader (30, "Alpha-Diffuse.shader", "Transparent/Diffuse");
	RegisterShader (31, "Alpha-Bumped.shader", "Transparent/Bumped Diffuse");
	RegisterShader (32, "Alpha-Glossy.shader", "Transparent/Specular");
	RegisterShader (33, "Alpha-BumpSpec.shader", "Transparent/Bumped Specular");
	// Transparent/Cutout
	RegisterShader (50, "AlphaTest-VertexLit.shader", "Transparent/Cutout/VertexLit");
	RegisterShader (51, "AlphaTest-Diffuse.shader", "Transparent/Cutout/Diffuse");
	RegisterShader (52, "AlphaTest-Bumped.shader", "Transparent/Cutout/Bumped Diffuse");
	RegisterShader (53, "AlphaTest-Glossy.shader", "Transparent/Cutout/Specular");
	RegisterShader (54, "AlphaTest-BumpSpec.shader", "Transparent/Cutout/Bumped Specular");
	// Parallax
	RegisterShader (8, "Normal-Parallax.shader", "Parallax Diffuse");
	RegisterShader (9, "Normal-ParallaxSpec.shader", "Parallax Specular");
	RegisterShader (15, "Illumin-Parallax.shader", "Self-Illumin/Parallax Diffuse");
	RegisterShader (16, "Illumin-ParallaxSpec.shader", "Self-Illumin/Parallax Specular");
	RegisterShader (27, "Reflect-Parallax.shader", "Reflective/Parallax Diffuse");
	RegisterShader (28, "Reflect-ParallaxSpec.shader", "Reflective/Parallax Specular");
	RegisterShader (35, "Alpha-Parallax.shader", "Transparent/Parallax Diffuse");
	RegisterShader (36, "Alpha-ParallaxSpec.shader", "Transparent/Parallax Specular");
	
	// Misc
	RegisterShader (100, "Decal.shader", "Decal");
	RegisterShader (101, "Flare.shader", "FX/Flare");
	RegisterShader (103, "skybox cubed.shader", "RenderFX/Skybox Cubed");
	RegisterShader (104, "skybox.shader", "RenderFX/Skybox");

	// Particle shaders
	RegisterShader (200, "Particle Add.shader", "Particles/Additive");
	RegisterShader (201, "Particle AddMultiply.shader", "Particles/~Additive-Multiply");
	RegisterShader (202, "Particle AddSmooth.shader", "Particles/Additive (Soft)");
	RegisterShader (203, "Particle Alpha Blend.shader", "Particles/Alpha Blended");
	//RegisterResource (204, "Particle Alpha Blend Deprecated.shader", "Shader"); // obsolete
	RegisterShader (205, "Particle Multiply.shader", "Particles/Multiply");
	RegisterShader (206, "Particle MultiplyDouble.shader", "Particles/Multiply (Double)");
	RegisterShader (207, "Particle Premultiply Blend.shader", "Particles/Alpha Blended Premultiply");
	RegisterShader (208, "Particle VertexLit Blended.shader", "Particles/VertexLit Blended");

#if ENABLE_SPRITES
	// Sprite shaders
	RegisterShader (10800, "Sprites-Diffuse.shader", "Sprites/Diffuse");
#endif

	// Various
	RegisterShader (10512, "AlphaTest-SoftEdgeUnlit.shader", "Transparent/Cutout/Soft Edge Unlit");

	// Terrain engine
	RegisterShader (10500, "TerrainShaders/Details/VertexLit.shader", "Hidden/TerrainEngine/Details/Vertexlit", false);
	RegisterShader (10501, "TerrainShaders/Details/WavingGrass.shader", "Hidden/TerrainEngine/Details/WavingDoublePass", false);
	RegisterShader (10502, "TerrainShaders/Details/WavingGrassBillboard.shader", "Hidden/TerrainEngine/Details/BillboardWavingDoublePass", false);
	RegisterShader (10505, "TerrainShaders/Splats/FirstPass.shader", "Nature/Terrain/Diffuse");
	RegisterShader (10503, "TerrainShaders/Splats/AddPass.shader", "Hidden/TerrainEngine/Splatmap/Lightmap-AddPass", false);
	RegisterShader (10507, "TerrainShaders/Trees/BillboardTree.shader", "Hidden/TerrainEngine/BillboardTree", false);
	//RegisterResource (10513, "TerrainShaders/Trees/SH-Tree Leaves customLit.shader", "Shader"); // not used anymore
	//RegisterResource (10514, "TerrainShaders/Trees/SH-Tree Bark customLit.shader", "Shader"); // not used anymore	

	// 2.x lightmap shaders
	RegisterShader (40, "Lightmap-VertexLit.shader", "Legacy Shaders/Lightmapped/VertexLit");
	RegisterShader (41, "Lightmap-Diffuse.shader", "Legacy Shaders/Lightmapped/Diffuse");
	RegisterShader (42, "Lightmap-Bumped.shader", "Legacy Shaders/Lightmapped/Bumped Diffuse");
	RegisterShader (43, "Lightmap-Glossy.shader", "Legacy Shaders/Lightmapped/Specular");
	RegisterShader (44, "Lightmap-BumpSpec.shader", "Legacy Shaders/Lightmapped/Bumped Specular");

	// 2.x tree shaders
	RegisterShader (10508, "Nature/SoftOcclusion/TreeSoftOcclusionBarkRendertex.shader", "Hidden/Nature/Tree Soft Occlusion Bark Rendertex", false);
	RegisterShader (10509, "Nature/SoftOcclusion/TreeSoftOcclusionBark.shader", "Nature/Tree Soft Occlusion Bark");
	RegisterShader (10510, "Nature/SoftOcclusion/TreeSoftOcclusionLeavesRendertex.shader", "Hidden/Nature/Tree Soft Occlusion Leaves Rendertex", false);
	RegisterShader (10511, "Nature/SoftOcclusion/TreeSoftOcclusionLeaves.shader", "Nature/Tree Soft Occlusion Leaves");

	// Tree Creator shaders
	RegisterShader (10600, "Nature/TreeCreator/TreeCreatorBark.shader", "Nature/Tree Creator Bark");
	RegisterShader (10601, "Nature/TreeCreator/TreeCreatorLeaves.shader", "Nature/Tree Creator Leaves");
	RegisterShader (10602, "Nature/TreeCreator/TreeCreatorBarkRendertex.shader", "Hidden/Nature/Tree Creator Bark Rendertex", false);
	RegisterShader (10603, "Nature/TreeCreator/TreeCreatorLeavesRendertex.shader", "Hidden/Nature/Tree Creator Leaves Rendertex", false);
	RegisterShader (10604, "Nature/TreeCreator/TreeCreatorBarkOptimized.shader", "Hidden/Nature/Tree Creator Bark Optimized", false);
	RegisterShader (10605, "Nature/TreeCreator/TreeCreatorLeavesOptimized.shader", "Hidden/Nature/Tree Creator Leaves Optimized", false);

	RegisterShader (10606, "Nature/TreeCreator/TreeCreatorLeavesFast.shader", "Nature/Tree Creator Leaves Fast");
	RegisterShader (10607, "Nature/TreeCreator/TreeCreatorLeavesFastOptimized.shader", "Hidden/Nature/Tree Creator Leaves Fast Optimized", false);

	// Terrain shaders
	RegisterShader (10620, "Nature/Terrain/TerrBumpFirstPass.shader", "Nature/Terrain/Bumped Specular");
	RegisterShader (10621, "Nature/Terrain/TerrBumpAddPass.shader", "Hidden/Nature/Terrain/Bumped Specular AddPass", false);

	// Mobile optimized shaders
	RegisterShader (10700, "Mobile/Mobile-Skybox.shader", "Mobile/Skybox");
	RegisterShader (10701, "Mobile/Mobile-VertexLit.shader", "Mobile/VertexLit");
	RegisterShader (10703, "Mobile/Mobile-Diffuse.shader", "Mobile/Diffuse");
	RegisterShader (10704, "Mobile/Mobile-Bumped.shader", "Mobile/Bumped Diffuse");
	RegisterShader (10705, "Mobile/Mobile-BumpSpec.shader", "Mobile/Bumped Specular");
	RegisterShader (10706, "Mobile/Mobile-BumpSpec-1DirectionalLight.shader", "Mobile/Bumped Specular (1 Directional Light)");
	RegisterShader (10707, "Mobile/Mobile-VertexLit-OnlyDirectionalLights.shader", "Mobile/VertexLit (Only Directional Lights)");
	RegisterShader (10708, "Mobile/Mobile-Lightmap-Unlit.shader", "Mobile/Unlit (Supports Lightmap)");

	RegisterShader (10720, "Mobile/Mobile-Particle-Add.shader", "Mobile/Particles/Additive");
	RegisterShader (10721, "Mobile/Mobile-Particle-Alpha.shader", "Mobile/Particles/Alpha Blended");
	RegisterShader (10722, "Mobile/Mobile-Particle-Alpha-VertexLit.shader", "Mobile/Particles/VertexLit Blended");
	RegisterShader (10723, "Mobile/Mobile-Particle-Multiply.shader", "Mobile/Particles/Multiply");

	// Unlit shaders
	RegisterShader (10750, "Unlit/Unlit-Alpha.shader", "Unlit/Transparent");
	RegisterShader (10751, "Unlit/Unlit-AlphaTest.shader", "Unlit/Transparent Cutout");
	RegisterShader (10752, "Unlit/Unlit-Normal.shader", "Unlit/Texture");

	// Materials & textures
	RegisterResource (10300, "Default-Particle.psd", "Texture2D");
	RegisterResource (10301, "Default-Particle.mat", "Material");
	RegisterResource (10302, "Default-Diffuse.mat", "Material");

	m_Resources.sort();
}

static void RemapShadersToDefaultExtraResources ()
{
	// We have moved some shaders from builtin to default resources extra,
	// make sure we don't break any backwards compatibility by remapping all references to them on load
	// in the editor.
	
	const int ids[] = {
		6, 1, 7, 2, 3, 4, 5, // regular opaque
		14, 10, 11, 12, 13, // self-illumin
		24, 25, 20, 21, 26, 22, 23, // reflective
		34, 30, 31, 32, 33, // transparent
		50, 51, 52, 53, 54, // transparent/cutout
		8, 9, 15, 16, 27, 28, 35, 36, // parallax
		100, 101, 103, 104, // misc
		200, 201, 202, 203, 205, 206, 207, 208, // particles
		10512, // various
		10500, 10501, 10502, 10505, 10503, 10507, // terrain engine
		40, 41, 42, 43, 44, // 2.x lightmap shaders
		10508, 10509, 10510, 10511, // 2.x tree shaders
		10600, 10601, 10602, 10603, 10604, 10605, // Tree Creator shaders
		10300, 10301, 10302, // default textures & materials
	};
	
	PersistentManager& pm = GetPersistentManager();
	for (int i = 0; i < ARRAY_SIZE(ids); ++i)
	{
		pm.RemapInstanceIDOnLoad(kResourcePath, ids[i], kDefaultExtraResourcesPath, ids[i]);
	}
}


void BuiltinResourceManager::GetResourcesOfClass (int classID, std::vector< std::pair<std::string,int> >& outResources)
{
	for (Resources::iterator i = m_Resources.begin(); i != m_Resources.end(); ++i)
	{
		if (i->classID != classID || !i->userVisible)
			continue;
		// Cache and store the name, so we don't have to load the actual object
		// each time we do GetResourcesOfClass
		if (i->cachedDisplayName.empty())
		{
			NamedObject* no = dynamic_instanceID_cast<NamedObject*>(i->cachedInstanceID);
			i->cachedDisplayName = no ? no->GetName() : "<no name>";
		}
		outResources.push_back (std::make_pair(i->cachedDisplayName, i->cachedInstanceID));
	}
}

void BuiltinResourceManager::GetBuiltinResourcesOfClass (int classID, std::vector< std::pair<std::string,int> >& outResources)
{
	GetBuiltinResourceManager().GetResourcesOfClass (classID, outResources);
	GetBuiltinExtraResourceManager().GetResourcesOfClass (classID, outResources);
}

#endif // #if UNITY_EDITOR


#if WEBPLUG
static void RemapShadersToOldWebResources ()
{
	const int ids[] = {
		6, 1, 7, 2, 3, 4, 5, // regular opaque
		14, 10, 11, 12, 13, // self-illumin
		24, 25, 20, 21, 26, 22, 23, // reflective
		34, 30, 31, 32, 33, // transparent
		50, 51, 52, 53, 54, // transparent/cutout
		8, 9, 15, 16, 27, 28, 35, 36, // parallax
		100, 101, 103, 104, // misc
		200, 201, 202, 203, 205, 206, 207, 208, // particles
		10512, // various
		10500, 10501, 10502, 10505, 10503, 10507, // terrain engine
		10300, 10301, 10302, // default textures & materials
	};

	PersistentManager& pm = GetPersistentManager();
	for (int i = 0; i < ARRAY_SIZE(ids); ++i)
{
		pm.RemapInstanceIDOnLoad(kResourcePath, ids[i], kOldWebResourcePath, ids[i]);
	}
}
#endif // #if WEBPLUG


#if WEBPLUG || UNITY_EDITOR
void BuiltinResourceManager::InitializeOldWebResources ()
{
	Assert (m_Resources.empty ());

	m_ResourcePath = kOldWebResourcePath;
	m_RequiredHideFlags = Object::kHideAndDontSave | Object::kHideInspector;
	m_AllowResourceManagerAccess = true;

	RegisterShader (6, "Normal-VertexLit.shader", "VertexLit");
	RegisterShader (1, "Normal-DiffuseFast.shader", "Legacy Shaders/Diffuse Fast");
	RegisterShader (7, "Normal-Diffuse.shader", "Diffuse");
	RegisterShader (2, "Normal-Bumped.shader", "Bumped Diffuse");
	RegisterShader (3, "Normal-Glossy.shader", "Specular");
	RegisterShader (4, "Normal-BumpSpec.shader", "Bumped Specular");
	RegisterShader (5, "Normal-DiffuseDetail.shader", "Diffuse Detail");

	RegisterShader (14, "Illumin-VertexLit.shader", "Self-Illumin/VertexLit");
	RegisterShader (10, "Illumin-Diffuse.shader", "Self-Illumin/Diffuse");
	RegisterShader (11, "Illumin-Bumped.shader", "Self-Illumin/Bumped Diffuse");
	RegisterShader (12, "Illumin-Glossy.shader", "Self-Illumin/Specular");
	RegisterShader (13, "Illumin-BumpSpec.shader", "Self-Illumin/Bumped Specular");

	RegisterShader (24, "Reflect-VertexLit.shader", "Reflective/VertexLit");
	RegisterShader (25, "Reflect-BumpNolight.shader", "Reflective/Bumped Unlit");
	RegisterShader (20, "Reflect-Diffuse.shader", "Reflective/Diffuse");
	RegisterShader (21, "Reflect-Bumped.shader", "Reflective/Bumped Diffuse");
	RegisterShader (26, "Reflect-BumpVertexLit.shader", "Reflective/Bumped VertexLit");
	RegisterShader (22, "Reflect-Glossy.shader", "Reflective/Specular");
	RegisterShader (23, "Reflect-BumpSpec.shader", "Reflective/Bumped Specular");

	RegisterShader (34, "Alpha-VertexLit.shader", "Transparent/VertexLit");
	RegisterShader (30, "Alpha-Diffuse.shader", "Transparent/Diffuse");
	RegisterShader (31, "Alpha-Bumped.shader", "Transparent/Bumped Diffuse");
	RegisterShader (32, "Alpha-Glossy.shader", "Transparent/Specular");
	RegisterShader (33, "Alpha-BumpSpec.shader", "Transparent/Bumped Specular");

	RegisterShader (50, "AlphaTest-VertexLit.shader", "Transparent/Cutout/VertexLit");
	RegisterShader (51, "AlphaTest-Diffuse.shader", "Transparent/Cutout/Diffuse");
	RegisterShader (52, "AlphaTest-Bumped.shader", "Transparent/Cutout/Bumped Diffuse");
	RegisterShader (53, "AlphaTest-Glossy.shader", "Transparent/Cutout/Specular");
	RegisterShader (54, "AlphaTest-BumpSpec.shader", "Transparent/Cutout/Bumped Specular");

	RegisterShader (8, "Normal-Parallax.shader", "Parallax Diffuse");
	RegisterShader (9, "Normal-ParallaxSpec.shader", "Parallax Specular");
	RegisterShader (15, "Illumin-Parallax.shader", "Self-Illumin/Parallax Diffuse");
	RegisterShader (16, "Illumin-ParallaxSpec.shader", "Self-Illumin/Parallax Specular");
	RegisterShader (27, "Reflect-Parallax.shader", "Reflective/Parallax Diffuse");
	RegisterShader (28, "Reflect-ParallaxSpec.shader", "Reflective/Parallax Specular");
	RegisterShader (35, "Alpha-Parallax.shader", "Transparent/Parallax Diffuse");
	RegisterShader (36, "Alpha-ParallaxSpec.shader", "Transparent/Parallax Specular");

	RegisterShader (100, "Decal.shader", "Decal");
	RegisterShader (101, "Flare.shader", "FX/Flare");
	RegisterShader (103, "skybox cubed.shader", "RenderFX/Skybox Cubed");
	RegisterShader (104, "skybox.shader", "RenderFX/Skybox");

	RegisterShader (200, "Particle Add.shader", "Particles/Additive");
	RegisterShader (201, "Particle AddMultiply.shader", "Particles/~Additive-Multiply");
	RegisterShader (202, "Particle AddSmooth.shader", "Particles/Additive (Soft)");
	RegisterShader (203, "Particle Alpha Blend.shader", "Particles/Alpha Blended");
	RegisterShader (205, "Particle Multiply.shader", "Particles/Multiply");
	RegisterShader (206, "Particle MultiplyDouble.shader", "Particles/Multiply (Double)");
	RegisterShader (207, "Particle Premultiply Blend.shader", "Particles/Alpha Blended Premultiply");
	RegisterShader (208, "Particle VertexLit Blended.shader", "Particles/VertexLit Blended");

	RegisterShader (10512, "AlphaTest-SoftEdgeUnlit.shader", "Transparent/Cutout/Soft Edge Unlit");

	RegisterShader (10500, "TerrainShaders/Details/VertexLit.shader", "Hidden/TerrainEngine/Details/Vertexlit");
	RegisterShader (10501, "TerrainShaders/Details/WavingGrass.shader", "Hidden/TerrainEngine/Details/WavingDoublePass");
	RegisterShader (10502, "TerrainShaders/Details/WavingGrassBillboard.shader", "Hidden/TerrainEngine/Details/BillboardWavingDoublePass");
	RegisterShader (10505, "TerrainShaders/Splats/FirstPass.shader", "Nature/Terrain/Diffuse");
	RegisterShader (10503, "TerrainShaders/Splats/AddPass.shader", "Hidden/TerrainEngine/Splatmap/Lightmap-AddPass");
	RegisterShader (10507, "TerrainShaders/Trees/BillboardTree.shader", "Hidden/TerrainEngine/BillboardTree");

	// Materials & textures
	RegisterResource (10300, "Default-Particle.psd", "Texture2D");
	RegisterResource (10301, "Default-Particle.mat", "Material");
	RegisterResource (10302, "Default-Diffuse.mat", "Material");
	
	m_Resources.sort();
}
#endif // #if WEBPLUG || UNITY_EDITOR



template<class TransferFunction>
void ResourceManager::Dependency::Transfer (TransferFunction& transfer)
{
	transfer.Transfer (object, "m_Object");
	transfer.Transfer (dependencies, "m_Dependencies");
}

void BuiltinResourceManager::InitializeResources ()
{
	m_ResourcePath = kResourcePath;
	m_RequiredHideFlags = Object::kHideAndDontSave | Object::kHideInspector;
	m_AllowResourceManagerAccess = true;

	AssertIf (!m_Resources.empty ());


	// Issue RegisterResource shader calls in the expected loading order:
	// if any shader references another (e.g. UsePass), the referenced must come first!

	RegisterShader (60, "Shadow-ScreenBlur.shader", "Hidden/Shadow-ScreenBlur", false);
	RegisterShader (61, "Camera-DepthTexture.shader", "Hidden/Camera-DepthTexture", false);
	RegisterShader (62, "Camera-DepthNormalTexture.shader", "Hidden/Camera-DepthNormalTexture", false);
	RegisterShader (63, "Internal-PrePassLighting.shader", "Hidden/Internal-PrePassLighting", false);
	RegisterShader (64, "Internal-PrePassCollectShadows.shader", "Hidden/Internal-PrePassCollectShadows", false);
	RegisterShader (65, "Internal-CombineDepthNormals.shader", "Hidden/Internal-CombineDepthNormals", false);
	RegisterShader (66, "Internal-BlitCopy.shader", "Hidden/BlitCopy", false);
	RegisterShader (67, "Shadow-ScreenBlurRotated.shader", "Hidden/Shadow-ScreenBlurRotated", false);
	RegisterShader (68, "Internal-Clear.shader", "Hidden/InternalClear", false);

	RegisterShader (102, "Internal-Flare.shader", "Hidden/Internal-Flare", false);
	RegisterShader (105, "Internal-Halo.shader", "Hidden/Internal-Halo", false);

	//RegisterResource (300, "FX-Water.shader", "Shader"); // obsolete
	//RegisterResource (500, "r7000bumpmap.cgvp", "CGProgram"); // obsolete
	//RegisterResource (501, "r7000bumpmap2.cgvp", "CGProgram"); // obsolete

	// Light stuff
	//RegisterResource (10000, "Med.psd", "Cubemap"); // obsolete
	RegisterResource (10001, "Soft.psd", "Texture2D", false);

	// internal GUI stuff
	RegisterShader (9000, "Internal-GUITextureClip.shader", "Hidden/Internal-GUITextureClip", false);
	RegisterShader (9001, "Internal-GUITextureClipText.shader", "Hidden/Internal-GUITextureClipText", false);
	RegisterShader (9002, "Internal-GUITexture.shader", "Hidden/Internal-GUITexture", false);
	RegisterShader (9003, "Internal-GUITextureBlit.shader", "Hidden/Internal-GUITextureBlit", false);

	// Default font
	RegisterResource (10100, kDefaultFontName, "Material", false);
	RegisterShader (10101, "Font.shader", "GUI/Text Shader");
	RegisterResource (10102, kDefaultFontName, "Font");
	RegisterResource (10103, kDefaultFontName, "Texture2D", false);

	// Default meshes
	RegisterResource (10202, "Cube.fbx", "Mesh");

	RegisterResource (10206, "New-Cylinder.fbx", "Mesh");
	RegisterResource (10207, "New-Sphere.fbx", "Mesh");
	RegisterResource (10208, "New-Capsule.fbx", "Mesh");
	RegisterResource (10209, "New-Plane.fbx", "Mesh");
	RegisterResource (10210, "Quad.fbx", "Mesh");
	// These 3 have zero normals, because pre-4.3 light prepass shader expects it
	RegisterResource (10211, "icosphere.fbx", "Mesh");
	RegisterResource (10212, "icosahedron.fbx", "Mesh");
	RegisterResource (10213, "pyramid.fbx", "Mesh");

	// DEPRECATED builtin meshes (Deprecated in version 1.5 (Adjust scale (eg. capsule is 2 meters high instead of 4)))
	RegisterResource (10200, "Sphere.fbx", "Mesh", false);
	RegisterResource (10203, "Cylinder.fbx", "Mesh", false);
	RegisterResource (10204, "Plane.fbx", "Mesh", false);
	RegisterResource (10205, "Capsule.fbx", "Mesh", false);

	// Watermark, splash etc.
	RegisterResource (10400, "UnityWaterMark-small.png", "Texture2D", false);
	RegisterResource (10401, "EscToExit_back.png", "Texture2D", false);
	RegisterResource (10402, "EscToExit_text.png", "Texture2D", false);
	RegisterResource (10407, "UnityWaterMark-trial.png", "Texture2D", false);
	RegisterResource (10408, "UnityWaterMark-beta.png", "Texture2D", false);
	RegisterResource (10409, "UnityWaterMark-edu.png", "Texture2D", false);
	RegisterResource (10410, "UnityWaterMark-dev.png", "Texture2D", false);
	RegisterResource (10411, "WarningSign.psd", "Texture2D", false);
	RegisterResource (10412, "UnityWatermark-DebugFlashPlayer.png", "Texture2D", false);
	RegisterResource (10413, "UnityWaterMark-proto.png", "Texture2D", false);
	RegisterResource (10414, "UnityWaterMarkPlugin-beta.png", "Texture2D", false);

#if !UNITY_IPHONE
	// Standalone
	RegisterResource (10403, "UnitySplash.png", "Texture2D", false);
	RegisterResource (10404, "UnitySplash2.png", "Texture2D", false);
	RegisterResource (10405, "UnitySplash3.png", "Texture2D", false);
	RegisterResource (10406, "UnitySplashBack.png", "Texture2D", false);
#endif

#if ENABLE_SPRITES
	RegisterShader   (10753, "Sprites-Default.shader", "Sprites/Default");
	RegisterResource (10754, "Sprites-Default.mat", "Material");	
#endif

	// Built-in GUISkin
	RegisterResource (11000, "GameSkin/GameSkin.guiskin", "MonoBehaviour", false);
	RegisterResource (11001, "GameSkin/box.png", "Texture2D", false);
	RegisterResource (11002, "GameSkin/button active.png", "Texture2D", false);
	RegisterResource (11003, "GameSkin/button hover.png", "Texture2D", false);
	RegisterResource (11004, "GameSkin/button on hover.png", "Texture2D", false);
	RegisterResource (11005, "GameSkin/button on.png", "Texture2D", false);
	RegisterResource (11006, "GameSkin/button.png", "Texture2D", false);
	RegisterResource (11007, "GameSkin/horizontal scrollbar thumb.png", "Texture2D", false);
	RegisterResource (11008, "GameSkin/horizontal scrollbar.png", "Texture2D", false);
	RegisterResource (11009, "GameSkin/horizontalslider.png", "Texture2D", false);
	RegisterResource (11010, "GameSkin/slider thumb active.png", "Texture2D", false);
	RegisterResource (11011, "GameSkin/slider thumb.png", "Texture2D", false);
	RegisterResource (11012, "GameSkin/slidert humb hover.png", "Texture2D", false);
	RegisterResource (11013, "GameSkin/toggle active.png", "Texture2D", false);
	RegisterResource (11014, "GameSkin/toggle hover.png", "Texture2D", false);
	RegisterResource (11015, "GameSkin/toggle on hover.png", "Texture2D", false);
	RegisterResource (11016, "GameSkin/toggle on.png", "Texture2D", false);
	RegisterResource (11017, "GameSkin/toggle on active.png", "Texture2D", false);
	RegisterResource (11018, "GameSkin/toggle.png", "Texture2D", false);
	RegisterResource (11019, "GameSkin/vertical scrollbar thumb.png", "Texture2D", false);
	RegisterResource (11020, "GameSkin/vertical scrollbar.png", "Texture2D", false);
	RegisterResource (11021, "GameSkin/verticalslider.png", "Texture2D", false);
	RegisterResource (11022, "GameSkin/window on.png", "Texture2D", false);
	RegisterResource (11023, "GameSkin/window.png", "Texture2D", false);
	RegisterResource (11024, "GameSkin/textfield.png", "Texture2D", false);
	RegisterResource (11025, "GameSkin/textfield on.png", "Texture2D", false);
	RegisterResource (11026, "GameSkin/textfield hover.png", "Texture2D", false);

	// 12000 range is used by scripts in RegisterBuiltinScripts
	RegisterResource(11998, "DeveloperConsole", "MonoScript", false);
	RegisterResource(11999, "UserAuthorizationDialog", "MonoScript", false);
	RegisterResource(12000, "Terrain", "MonoScript", false);
	RegisterResource(12001, "GUISkin", "MonoScript", false);

	#if UNITY_LOGIC_GRAPH
	// 130xx are for logic graph engine side scripts
	RegisterResource(13003, "GraphBehaviour", "MonoScript", false);
	RegisterResource(13006, "OnAnimationEventDummy", "MonoScript", false);
	RegisterResource(13007, "OnMouseEventDummy", "MonoScript", false);
	RegisterResource(13008, "OnTriggerEventDummy", "MonoScript", false);
	RegisterResource(13009, "OnCollisionEventDummy", "MonoScript", false);
	#endif

	m_Resources.sort();

}

void BuiltinResourceManager::InitializeAllResources ()
{
#if WEBPLUG
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion_OldWebResourcesAdded))
	{
		GetBuiltinOldWebResourceManager().InitializeOldWebResources();
		RemapShadersToOldWebResources();
	}
#endif

#if UNITY_EDITOR

	// Setup up path remapping for builtin resources
	GetPersistentManager().SetPathRemap(kResourcePath, AppendPathName (GetApplicationContentsPath (), kDefaultResourcesSource));
	GetPersistentManager().SetPathRemap(kEditorResourcePath, AppendPathName (GetApplicationContentsPath (), kEditorDefaultResourcesSource));
	GetPersistentManager().SetPathRemap(kDefaultExtraResourcesPath, AppendPathName (GetApplicationContentsPath (), kDefaultExtraResourcesPath));

	// Initialize resources
	GetBuiltinResourceManager().InitializeResources();
	GetBuiltinExtraResourceManager().InitializeExtraResources();
	RemapShadersToDefaultExtraResources();
#else
	GetBuiltinResourceManager().InitializeResources();
#endif

	s_AreResourcesInitialized = true;
}

bool BuiltinResourceManager::AreResourcesInitialized()
{
	return s_AreResourcesInitialized;
}


#if UNITY_EDITOR

// Disable builtin resource loading. This is useful to ensure there are no dependencies on builtin resources when building them.
void BuiltinResourceManager::SetAllowLoadingBuiltinResources (bool allowLoadingBuiltinResources)
{
	GetBuiltinResourceManager().m_AllowResourceManagerAccess = allowLoadingBuiltinResources;
	GetBuiltinOldWebResourceManager().m_AllowResourceManagerAccess = allowLoadingBuiltinResources;
	GetBuiltinExtraResourceManager().m_AllowResourceManagerAccess = allowLoadingBuiltinResources;
}


void BuiltinResourceManager::RegisterBuiltinScriptsForBuilding ()
{
	// Regenerate scripts from scratch and serialize them
	for (Resources::iterator i=m_Resources.begin ();i != m_Resources.end ();i++)
	{
		if (i->classID == ClassID (MonoScript))
		{
			string path = AppendPathName("GeneratedBuiltinScripts", i->name);
			int oldID = GetPersistentManager().GetInstanceIDFromPathAndFileID(path, 1);
			if (dynamic_instanceID_cast<MonoScript*>(oldID) == NULL)
			{
				MonoScript* script = NEW_OBJECT(MonoScript);
				script->Reset();
				script->Init("", i->name, "UnityEngine", "UnityEngine.dll", false);
				script->SetName(i->name);
				script->SetPathName(i->name);
				GetPersistentManager().MakeObjectPersistentAtFileID(script->GetInstanceID(), 1, path);
				script->AwakeFromLoad(kDefaultAwakeFromLoad);
			}
		}
	}
}

static void RegisterGraphScript (int id, const char* className, const char* _namspace)
{
	DoRegisterBuiltinEditorScript(id, className, _namspace, "UnityEditor.Graphs.dll", true);
}

void GenerateEditorScripts ()
{
	RegisterBuiltinEditorScript(12002, "TerrainInspector");

	RegisterBuiltinEditorScript(12003, "ConsoleWindow");
	RegisterBuiltinEditorScript(12004, "ContainerWindow");
	RegisterBuiltinEditorScript(12005, "GUIView");
	RegisterBuiltinEditorScript(12006, "DockArea");
	RegisterBuiltinEditorScript(12007, "EditorWindow");
	RegisterBuiltinEditorScript(12008, "MainWindow");
	RegisterBuiltinEditorScript(12009, "PaneDragTab");
	RegisterBuiltinEditorScript(12010, "SplitView");
	RegisterBuiltinEditorScript(12011, "Toolbar");
	RegisterBuiltinEditorScript(12012, "Tools");
	RegisterBuiltinEditorScript(12013, "SceneView");
	RegisterBuiltinEditorScript(12014, "ProjectBrowser");
	RegisterBuiltinEditorScript(12015, "GameView");
	RegisterBuiltinEditorScript(12016, "PopupEditor");
	RegisterBuiltinEditorScript(12017, "ColorPicker");
	RegisterBuiltinEditorScript(12018, "AboutWindow");
	RegisterBuiltinEditorScript(12019, "InspectorWindow");
	RegisterBuiltinEditorScript(12020, "ShaderInspector");
	//RegisterBuiltinEditorScript(12021, "ServerDemo"); // was used for testing only
//	RegisterBuiltinEditorScript(12022, "GUIDebugHelper");
	//RegisterBuiltinEditorScript(12023, "ButtonsDemo"); // was used for testing only
	RegisterBuiltinEditorScript(12024, "AssetImporterInspector");
	RegisterBuiltinEditorScript(12025, "TextureImporterInspector");
	RegisterBuiltinEditorScript(12026, "MovieImporterInspector");
	RegisterBuiltinEditorScript(12027, "TransformInspector");
	RegisterBuiltinEditorScript(12028, "ModelImporterEditor");
	RegisterBuiltinEditorScript(12029, "AudioImporterInspector");
	RegisterBuiltinEditorScript(12030, "TrueTypeFontImporterInspector");
	RegisterBuiltinEditorScript(12031, "TextureInspector");
	RegisterBuiltinEditorScript(12032, "GameObjectInspector");
	RegisterBuiltinEditorScript(12033, "MaterialEditor");
	RegisterBuiltinEditorScript(12034, "SaveWindowLayout");
	RegisterBuiltinEditorScript(12035, "DeleteWindowLayout");
	RegisterBuiltinEditorScript(12036, "RenderTextureInspector");
	RegisterBuiltinEditorScript(12037, "ASMainWindow");
	RegisterBuiltinEditorScript(12038, "CubemapTextureInspector");
	RegisterBuiltinEditorScript(12039, "RagdollBuilder");
	RegisterBuiltinEditorScript(12040, "GenericInspector");
	RegisterBuiltinEditorScript(12041, "ProfilerIPWindow");
	RegisterBuiltinEditorScript(12042, "AppStatusBar");
	RegisterBuiltinEditorScript(12043, "BuildPlayerWindow");
	RegisterBuiltinEditorScript(12044, "PreferencesWindow");
	RegisterBuiltinEditorScript(12045, "Settings");
	RegisterBuiltinEditorScript(12046, "SnapSettings");
	RegisterBuiltinEditorScript(12047, "HostView");

	RegisterBuiltinEditorScript(12048, "TerrainWizard");
	RegisterBuiltinEditorScript(12049, "LightmapWizard");
	RegisterBuiltinEditorScript(12050, "ImportRawHeightmap");
	RegisterBuiltinEditorScript(12051, "ExportRawHeightmap");
	RegisterBuiltinEditorScript(12052, "SetResolutionWizard");
	RegisterBuiltinEditorScript(12053, "TreeWizard");
	RegisterBuiltinEditorScript(12054, "SplatWizard");
	RegisterBuiltinEditorScript(12055, "DetailMeshWizard");
	RegisterBuiltinEditorScript(12056, "DetailTextureWizard");
	RegisterBuiltinEditorScript(12057, "PlaceTreeWizard");
	RegisterBuiltinEditorScript(12058, "FlattenHeightmap");

	RegisterBuiltinEditorScript(12059, "FallbackEditorWindow");
	RegisterBuiltinEditorScript(12060, "MaximizedHostView");
	RegisterBuiltinEditorScript(12061, "HierarchyWindow");
	RegisterBuiltinEditorScript(12062, "PackageExport");
	RegisterBuiltinEditorScript(12063, "PackageImport");
	RegisterBuiltinEditorScript(12064, "EyeDropper");
	RegisterBuiltinEditorScript(12065, "WelcomeScreen");
	RegisterBuiltinEditorScript(12066, "MonoScriptInspector");
	RegisterBuiltinEditorScript(12067, "AudioClipInspector");
	RegisterBuiltinEditorScript(12068, "ModelInspector");
	RegisterBuiltinEditorScript(12069, "TooltipView");
	RegisterBuiltinEditorScript(12070, "ProfilerWindow");
	RegisterBuiltinEditorScript(12071, "AnimationWindow");
	RegisterBuiltinEditorScript(12072, "ASHistoryWindow");
	RegisterBuiltinEditorScript(12073, "AnimationEventPopup");
	RegisterBuiltinEditorScript(12074, "EditorSettingsInspector");
	RegisterBuiltinEditorScript(12075, "AssetInspector");
	RegisterBuiltinEditorScript(12076, "AnimationCleanupPopup");
	RegisterBuiltinEditorScript(12077, "ScriptReloadProperties");
	RegisterBuiltinEditorInternalScript(12078, "MacroWindow");
	RegisterBuiltinEditorScript(12079, "LightmappingWindow");
	RegisterBuiltinEditorScript(12080, "AuxWindow");
	RegisterBuiltinEditorScript(12081, "ObjectSelector");
	RegisterBuiltinEditorScript(12082, "LabelCompletion");
	RegisterBuiltinEditorScript(12083, "CurveEditorWindow");
	RegisterBuiltinEditorScript(12084, "MonoScriptImporterInspector");
	RegisterBuiltinEditorScript(12085, "MovieTextureInspector");
	RegisterBuiltinEditorScript(12086, "TextAssetInspector");
	RegisterBuiltinEditorScript(12087, "LightEditor");
	RegisterBuiltinEditorScript(12088, "AudioSourceInspector");
	RegisterBuiltinEditorScript(12089, "AudioLowPassFilterInspector");
	RegisterBuiltinEditorScript(12090, "OcclusionCullingWindow");
	RegisterBuiltinEditorScript(12091, "WebCamTextureInspector");
	RegisterBuiltinEditorScript(12092, "ShaderImporterInspector");

	RegisterBuiltinEditorScript(12099, "AudioReverbZoneEditor");
	RegisterBuiltinEditorScript(12100, "PhysicsManagerInspector");

	RegisterBuiltinEditorScript(12101, "ClothInspector");
	RegisterBuiltinEditorScript(12102, "BumpMapSettingsFixingWindow");
	RegisterBuiltinEditorScript(12103, "PragmaFixingWindow");
	RegisterBuiltinEditorScript(12104, "WindInspector");

	DoRegisterBuiltinEditorScript (12105, "TreeEditor", "TreeEditor", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript (12106, "TreeData", "TreeEditor", "UnityEditor.dll", true);

	RegisterBuiltinEditorScript(12107, "PlayerSettingsEditor");
	RegisterBuiltinEditorScript(12108, "EditorUpdateWindow");

	RegisterBuiltinEditorScript(12109, "AudioReverbFilterEditor");

	RegisterBuiltinEditorScript(12110, "WebView");
	RegisterBuiltinEditorScript(12111, "AssetStoreWindow");
	RegisterBuiltinEditorScript(12112, "AssetStoreContext");

	RegisterBuiltinEditorScript(12113, "AssembleEditorSkin");
	RegisterBuiltinEditorScript(12114, "OcclusionAreaEditor");
	RegisterBuiltinEditorScript(12115, "CameraEditor");

	RegisterBuiltinEditorScript(12116, "WindowFocusState");

	RegisterBuiltinEditorScript(12117, "AndroidKeystoreWindow");

	RegisterBuiltinEditorScript(12118, "AudioDistortionFilterEditor");
	RegisterBuiltinEditorScript(12119, "AudioEchoFilterEditor");
	RegisterBuiltinEditorScript(12120, "AudioHighPassFilterEditor");
	RegisterBuiltinEditorScript(12121, "AudioChorusFilterEditor");

	RegisterBuiltinEditorScript(12122, "BoxColliderEditor");
	RegisterBuiltinEditorScript(12123, "SphereColliderEditor");
	RegisterBuiltinEditorScript(12124, "CapsuleColliderEditor");
	RegisterBuiltinEditorScript(12125, "MeshColliderEditor");
	RegisterBuiltinEditorScript(12126, "WheelColliderEditor");
	RegisterBuiltinEditorScript(12127, "RigidbodyEditor");

	RegisterBuiltinEditorScript(12128, "AssetSaveDialog");

	RegisterBuiltinEditorScript(12129, "AnnotationWindow");
	RegisterBuiltinEditorScript(12130, "IconSelector");
	RegisterBuiltinEditorScript(12131, "ScriptExecutionOrderInspector");

	RegisterBuiltinEditorScript(12132, "AnimationEditor");
	RegisterBuiltinEditorScript(12133, "SkinnedMeshRendererEditor");
	RegisterBuiltinEditorScript(12134, "PreviewWindow");
	RegisterBuiltinEditorScript(12135, "LODGroupEditor");

	RegisterBuiltinEditorScript(12136, "OffMeshLinkInspector");
	RegisterBuiltinEditorScript(12137, "ParticleSystemWindow");
	RegisterBuiltinEditorScript(12138, "ParticleSystemInspector");
	RegisterBuiltinEditorScript(12139, "MeshRendererEditor");

	RegisterBuiltinEditorScript(12140, "GradientPicker");

	RegisterBuiltinEditorScript(12141, "NavMeshEditorWindow");
	RegisterBuiltinEditorScript(12142, "FontInspector");

	RegisterBuiltinEditorScript(12143, "BuildUploadCompletedWindow");
	RegisterBuiltinEditorScript(12144, "UploadingBuildsMonitor");

	RegisterBuiltinEditorScript(12145, "TerrainSplatEditor");

	#if ENABLE_SPRITES
	RegisterBuiltinEditorScript(12150, "SpriteRendererInspector");
	RegisterBuiltinEditorScript(12151, "SpriteInspector");
	#endif

	RegisterBuiltinEditorScript(12153, "TagManagerInspector");

	RegisterBuiltinEditorScript(12200, "SubstanceImporterInspector");
 //	RegisterBuiltinEditorScript(12201, "SubstanceArchiveInspector");
	RegisterBuiltinEditorScript(12202, "ProceduralMaterialInspector");
	RegisterBuiltinEditorScript(12203, "ProceduralTextureInspector");
	RegisterBuiltinEditorScript(12204, "NavMeshAgentInspector");
	RegisterBuiltinEditorScript(12205, "LightProbeGroupInspector");
	RegisterBuiltinEditorScript(12206, "LightProbeGroupSelection");
	RegisterBuiltinEditorScript(12207, "View");
	RegisterBuiltinEditorScript(12208, "OcclusionPortalInspector");
	RegisterBuiltinEditorScript(12209, "NavMeshObstacleInspector");

	RegisterBuiltinEditorScript(12210, "PopupList");

	RegisterBuiltinEditorScript(12211, "AssetStoreAssetInspector");
	RegisterBuiltinEditorScript(12212, "AssetStoreInstaBuyWindow");
	RegisterBuiltinEditorScript(12213, "AssetStoreLoginWindow");

	DoRegisterBuiltinEditorScript(12216, "EndNameEditAction", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12217, "DoCreateNoneAsset", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);	
	DoRegisterBuiltinEditorScript(12218, "DoCreateNewAsset", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12219, "DoCreateFolder", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12220, "DoCreatePrefab", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12221, "DoCreateScriptAsset", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12223, "DoCreateAnimatorController", "UnityEditor.ProjectWindowCallback", "UnityEditor.dll", true);

	RegisterBuiltinEditorScript(12214, "LightProbesInspector");

	RegisterBuiltinEditorScript(12215, "AddComponentWindow");

	RegisterBuiltinEditorScript(12222, "LicenseManagementWindow");
	
	DoRegisterBuiltinEditorScript(12300, "WindowResolve", "UnityEditor.VersionControl", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12301, "WindowChange", "UnityEditor.VersionControl", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12303, "WindowPending", "UnityEditor.VersionControl", "UnityEditor.dll", true);
	DoRegisterBuiltinEditorScript(12304, "WindowRevert", "UnityEditor.VersionControl", "UnityEditor.dll", true);

	RegisterBuiltinEditorScript(12306, "CharacterControllerEditor");

	RegisterBuiltinEditorScript(12307, "QualitySettingsEditor");
	RegisterBuiltinEditorScript(12308, "SavedSearchFilters");
	// RegisterBuiltinEditorScript(12309, ""); // Unused
	RegisterBuiltinEditorScript(12310, "TextMeshInspector");
	RegisterBuiltinEditorScript(12311, "Texture3DInspector");

	//RegisterBuiltinEditorScript(12312, "MetroCreateTestCertificateWindow");
	//RegisterBuiltinEditorScript(12313, "MetroCertificatePasswordWindow");
	
	RegisterBuiltinEditorScript(12314, "ModelImporterRigEditor");
	RegisterBuiltinEditorScript(12315, "ModelImporterModelEditor");
	RegisterBuiltinEditorScript(12316, "ModelImporterClipEditor");
	
	RegisterBuiltinEditorScript(12318, "AvatarPreviewSelection");
	RegisterBuiltinEditorScript(12319, "PopupWindow");

	RegisterBuiltinEditorScript(12320, "PresetLibraryManager");
	RegisterBuiltinEditorScript(12321, "GradientPresetLibrary");
	RegisterBuiltinEditorScript(12322, "CurvePresetLibrary");
	RegisterBuiltinEditorScript(12323, "ColorPresetLibrary");
	RegisterBuiltinEditorScript(12324, "DoubleCurvePresetLibrary");

	#if ENABLE_SPRITES
	RegisterBuiltinEditorScript(12325, "AtlasEditorWindow");
	RegisterBuiltinEditorScript(12326, "SpritePackerWindow");
	#endif

	#if ENABLE_2D_PHYSICS
	RegisterBuiltinEditorScript(12327, "PolygonColliderEditor");
	RegisterBuiltinEditorScript(12329, "Physics2DSettingsInspector");
	#endif

	RegisterBuiltinEditorScript(12330, "GameViewSizes");
	
	RegisterBuiltinEditorScript(12331, "ColorPresetLibraryEditor");
	RegisterBuiltinEditorScript(12332, "GradientPresetLibraryEditor");
	RegisterBuiltinEditorScript(12333, "CurvePresetLibraryEditor");
	RegisterBuiltinEditorScript(12334, "DoubleCurvePresetLibraryEditor");

	RegisterBuiltinEditorScript(12340, "BB10SigningAuthorityWindow");
	RegisterBuiltinEditorScript(12341, "BB10UnregisterWarningWindow");

	RegisterBuiltinEditorScript(12342, "TizenSigningAuthorityWindow");
	RegisterBuiltinEditorScript(12343, "TizenUnregisterWarningWindow");

	#if ENABLE_2D_PHYSICS
	RegisterBuiltinEditorScript(12350, "BoxCollider2DEditor");
	RegisterBuiltinEditorScript(12351, "CircleCollider2DEditor");
	RegisterBuiltinEditorScript(12352, "EdgeCollider2DEditor");
	#endif


	// 12410-12449 are for Toolbar & GUI Editor 

	#if UNITY_LOGIC_GRAPH
	// 131xx are for logic graph editor side scripts
	RegisterGraphScript(13100, "FlowWindow", kGraphsEditorBaseNamespace);
	RegisterGraphScript(13101, "LogicGraph", kLogicGraphEditorNamespace);
	RegisterGraphScript(13102, "LogicGraphGUI", kLogicGraphEditorNamespace);
	RegisterGraphScript(13103, "FunctionNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13104, "VariableNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13105, "FunctionNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13106, "VariableNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13107, "ClassNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13108, "ExpressionNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13109, "ExpressionNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13110, "CompareNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13111, "CompareNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13112, "IfNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13113, "IfNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13114, "RandomNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13115, "RandomNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13116, "ClassNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13117, "EventNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13118, "EventNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13119, "SetPropertyNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13120, "SetPropertyNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13121, "GetPropertyNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13122, "GetPropertyNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13123, "OnAnimationEventNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13124, "OnAnimationEventNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13125, "ExternalCallReceiver", kLogicGraphEditorNamespace);
	RegisterGraphScript(13126, "ExternalCallReceiverEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13127, "ExternalCallSender", kLogicGraphEditorNamespace);
	RegisterGraphScript(13128, "ExternalCallSenderEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13129, "EvalNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13130, "EvalNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13131, "StructExpressionNode", kLogicGraphEditorNamespace);

	RegisterGraphScript(13132, "StructExpressionNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13133, "ToggleNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13134, "ToggleNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13135, "GetComponentNode", kLogicGraphEditorNamespace);
	RegisterGraphScript(13136, "GetComponentNodeEditor", kLogicGraphEditorNamespace);
	RegisterGraphScript(13137, "GraphBehaviourInspector", kLogicGraphEditorNamespace);
	#endif

	RegisterGraphScript(13138, "Graph", kGraphsEditorBaseNamespace);
	RegisterGraphScript(13139, "GraphInspector", kGraphsEditorBaseNamespace);
	RegisterGraphScript(13140, "GraphGUI", kGraphsEditorBaseNamespace);
	RegisterGraphScript(13141, "Node", kGraphsEditorBaseNamespace);
	RegisterBuiltinEditorScript(12901, "AnimatorInspector");
	RegisterBuiltinEditorScript(12902, "AnimatorOverrideControllerInspector");
	RegisterBuiltinEditorScript(12905, "AnimationClipEditor");
	RegisterBuiltinEditorScript(12906, "AvatarEditor");
	RegisterGraphScript(12907, "TransitionInspector", kAnimationStateMachineNamespace);
	RegisterGraphScript(12914, "AnimatorControllerTool", kGraphsEditorBaseNamespace);
	RegisterBuiltinEditorScript(12915, "AvatarMaskInspector");
	RegisterBuiltinEditorScript(12916, "BlendTreeInspector");

	RegisterBuiltinEditorScript(12917, "AvatarMappingEditor");
	RegisterBuiltinEditorScript(12918, "AvatarMuscleEditor");

	RegisterGraphScript(12919, "GraphGUI", kAnimationBlendTreeNamespace);
	RegisterGraphScript(12920, "Graph", kAnimationBlendTreeNamespace);
	RegisterGraphScript(12921, "Node", kAnimationBlendTreeNamespace);

	RegisterGraphScript(12930, "GraphGUI", kAnimationStateMachineNamespace);
	RegisterGraphScript(12931, "Graph", kAnimationStateMachineNamespace);
	RegisterGraphScript(12932, "StateNode", kAnimationStateMachineNamespace);
	RegisterGraphScript(12933, "AnyStateNode", kAnimationStateMachineNamespace);
	RegisterGraphScript(12934, "StateMachineNode", kAnimationStateMachineNamespace);
	RegisterGraphScript(12935, "StateEditor", kAnimationStateMachineNamespace);
	RegisterGraphScript(12936, "StateMachineInspector", kAnimationStateMachineNamespace);
	RegisterGraphScript(12937, "AnyStateNodeInspector", kAnimationStateMachineNamespace);

	RegisterBuiltinEditorScript(13201, "HeapshotWindow");
}

static void DoRegisterBuiltinEditorScript (int fileID, const char*  className, const char* _namespace, const char* assemblyName, bool isEditorScript)
{
#if ENABLE_MONO
	MonoScript* script = NEW_OBJECT(MonoScript);
	script->Reset();
	script->Init("", className, _namespace, assemblyName, isEditorScript);
	script->SetName(className);
	script->SetPathName(className);

	SInt32 oldInstanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(kResourcePath, fileID);
	if (Object::IDToPointer(oldInstanceID) != NULL)
	{
		ErrorString("Duplicate editor builtin script " + string(className));
	}

	GetPersistentManager().MakeObjectPersistentAtFileID(script->GetInstanceID(), fileID, kResourcePath);
	script->SetHideFlags(Object::kDontSave);

	if (isEditorScript)
		GetMonoScriptManager().RegisterEditorScript(*script);
	else
		GetMonoScriptManager().RegisterRuntimeScript(*script);
	script->AwakeFromLoad(kDefaultAwakeFromLoad);

#endif
}

static void RegisterBuiltinEditorScript (int instanceID, const char*  className)
{
	DoRegisterBuiltinEditorScript (instanceID, className, "UnityEditor", "UnityEditor.dll", true);
}

static void RegisterBuiltinEditorInternalScript (int instanceID, const char*  className)
{
	DoRegisterBuiltinEditorScript (instanceID, className, "UnityEditorInternal", "UnityEditor.dll", true);
}

#if ENABLE_UNIT_TESTS
void BuiltinResourceManager::RegisterShadersWithRegistry (ScriptMapper* registry)
{
	for (OrderedShaderStartup::iterator i = m_OrderedShaderStartup.begin ();
		 i != m_OrderedShaderStartup.end (); i++)
	{
		const ShaderInitializationData& data = *i;
		registry->AddBuiltinShader (data.shaderClassName, PPtr<Shader> (data.cachedInstanceID));
	}
}
#endif

void BuiltinResourceManager::LoadDefaultResourcesFromEditor ()
{
	if (!m_AllowResourceManagerAccess)
		return;

	PersistentManager& pm = GetPersistentManager ();

	// They need to show up in the script mapper
	for (OrderedShaderStartup::iterator i=m_OrderedShaderStartup.begin ();i != m_OrderedShaderStartup.end ();i++)
	{
		const ShaderInitializationData& data = *i;
		GetScriptMapper ().AddBuiltinShader (data.shaderClassName, PPtr<Shader>(data.cachedInstanceID));

		void AddBuiltinShaderInfoForMenu (const std::string& name, SInt32 id);
		AddBuiltinShaderInfoForMenu (data.shaderClassName, data.cachedInstanceID);
	}

	// In non-release builds, verify that the setup of all shaders is correct and no shaders have errors.
	#if !UNITY_RELEASE
	for (OrderedShaderStartup::iterator i=m_OrderedShaderStartup.begin ();i != m_OrderedShaderStartup.end ();i++)
	{
		Shader* shader = dynamic_pptr_cast<Shader*> (GetResource (ClassID (Shader), i->resourceName));
		Assert(shader != NULL);
		if (shader)
		{
			if (shader->GetScriptClassName() != i->shaderClassName)
			{
				AssertString("Shadername is: " + shader->GetScriptClassName() + " but hardcoded name was: " + i->shaderClassName);
			}

			Assert(shader->GetHideFlags() == m_RequiredHideFlags);
			if (shader->GetErrors().HasErrorsOrWarnings())
				shader->GetErrors().LogErrors(shader->GetScriptClassName().c_str(), "BUILTIN SHADER", shader, shader->GetInstanceID());
		}
	}
	#endif

	/////@TODO: Verify that all assets in the builtin resource file are actually being registered!

	// Scripts need to register with mono manager
	for (Resources::iterator i=m_Resources.begin ();i != m_Resources.end ();i++)
	{
		if (i->classID == ClassID (MonoScript))
		{
			int instanceID = pm.GetInstanceIDFromPathAndFileID (m_ResourcePath, i->fileID);
			MonoScript* script = dynamic_instanceID_cast<MonoScript*> (instanceID);
			if (script)
				GetMonoScriptManager().RegisterRuntimeScript(*script);
			else
				Assert("Script can't be found");
		}
	}
}


void BuiltinResourceManager::LoadAllDefaultResourcesFromEditor ()
{
	GetBuiltinResourceManager().LoadDefaultResourcesFromEditor();
	GetBuiltinExtraResourceManager().LoadDefaultResourcesFromEditor();

	// Editor scripts need to be generated and registered
	GenerateEditorScripts ();
}
#endif


vector<Object*> BuiltinResourceManager::GetAllResources ()
{
	vector<Object*> result;
	result.reserve (m_Resources.size ());

	for (Resources::const_iterator iter = m_Resources.begin (); iter != m_Resources.end (); ++iter)
		result.push_back (GetResource (iter->classID, iter->name));

	return result;
}

// Returns the first found resource with name that is derived from classID
Object* BuiltinResourceManager::GetResource (int classID, const string& name)
{
	if (!m_AllowResourceManagerAccess)
	{
		bool mustLoadBuiltinShader =
			name == "Internal-Halo.shader" ||
			name == "Internal-GUITexture.shader" ||
			name == "Normal-Diffuse.shader";
		// Allow for some specific exceptions!
		if (!mustLoadBuiltinShader)
		{
			AssertString("Loading builtin resources is forbidden when building them: " + name + "\nIf it is really necessary you can add a exception case at this assert.");
			return NULL;
		}
	}

	Resource proxy;
	proxy.classID = classID;
	proxy.name = name.c_str();

	Resources::iterator found = m_Resources.find (proxy);
	if (found == m_Resources.end())
	{
		AssertString("Failed to find " + name);
		return NULL;
	}

	AssertIf (found->cachedInstanceID == 0);
	Object* ptr = PPtr<Object> (found->cachedInstanceID);
	if (ptr == NULL || !ptr->IsDerivedFrom (classID))
	{
		ErrorString ("The resource " + name + " could not be loaded from the resource file!");
		return NULL;
	}
	else
	{
		AssertIf(ptr->GetHideFlags() != m_RequiredHideFlags);
		return ptr;
	}
}

void BuiltinResourceManager::RegisterShader (LocalIdentifierInFileType fileID, const char* name, const char* shaderClassName, bool userVisible)
{
#if UNITY_EDITOR
	int cachedInstanceID =
#endif
		RegisterResourceInternal(fileID, name, "Shader", shaderClassName, userVisible);

#if UNITY_EDITOR
	// Register shader initialization
	ShaderInitializationData data;
	data.shaderClassName = shaderClassName;
	data.resourceName = name;
	data.cachedInstanceID = cachedInstanceID;
	m_OrderedShaderStartup.push_back(data);
#endif
}

int BuiltinResourceManager::RegisterResourceInternal (LocalIdentifierInFileType fileID, const char* name, const char* className, const char* displayName, bool userVisible)
{
	RegisterDebugUsedFileID (fileID, name);

	Resource r;
	r.name = name;
	r.classID = Object::StringToClassID (className);
	r.fileID = fileID;
	r.cachedInstanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(m_ResourcePath, fileID);
	r.userVisible = userVisible;
	#if UNITY_EDITOR
	if (displayName != NULL)
		r.cachedDisplayName = displayName;
	#endif
	m_Resources.push_unsorted (r);

	return r.cachedInstanceID;
}

void BuiltinResourceManager::RegisterResource (LocalIdentifierInFileType fileID, const char* name, const char* className, bool userVisible)
{
	// RegisterResource can not be used on shaders, use RegisterShader
	Assert(strcmp(className, "Shader") != 0);

	RegisterResourceInternal(fileID, name, className, NULL, userVisible);
}

static BuiltinResourceManager* gBuiltinResourceManager;
static BuiltinResourceManager* gBuiltinOldWebResourceManager;


void BuiltinResourceManager::StaticInitialize()
{
	gBuiltinResourceManager = UNITY_NEW(BuiltinResourceManager,kMemResource);
	gBuiltinOldWebResourceManager = UNITY_NEW(BuiltinResourceManager,kMemResource);
	
#if UNITY_EDITOR
	gBuiltinExtraResourceManager = UNITY_NEW(BuiltinResourceManager,kMemResource);
#endif

	s_AreResourcesInitialized = false;
}

void BuiltinResourceManager::StaticDestroy()
{
	s_AreResourcesInitialized = false;

	UNITY_DELETE(gBuiltinResourceManager,kMemResource);
	UNITY_DELETE(gBuiltinOldWebResourceManager,kMemResource);
#if UNITY_EDITOR
	UNITY_DELETE(gBuiltinExtraResourceManager,kMemResource);
#endif
}

static RegisterRuntimeInitializeAndCleanup s_BuiltinResourceManagerCallbacks(BuiltinResourceManager::StaticInitialize, BuiltinResourceManager::StaticDestroy);

BuiltinResourceManager& GetBuiltinResourceManager ()
{
	return *gBuiltinResourceManager;
}

BuiltinResourceManager& GetBuiltinOldWebResourceManager ()
{
	return *gBuiltinOldWebResourceManager;
}


void BuiltinResourceManager::DestroyAllResources()
{
	for (Resources::iterator i=m_Resources.begin ();i != m_Resources.end ();i++)
	{
		Object* obj = Object::IDToPointer (i->cachedInstanceID);
		if(obj)
		{
			DestroySingleObject(obj);
		}
	}
	m_Resources.clear();
}

#if DEBUGMODE
void BuiltinResourceManager::RegisterDebugUsedFileID (LocalIdentifierInFileType fileID, const char* name)
{
	if (!m_UsedFileIDs.insert (fileID).second)
		ErrorString (Format ("File ID %d is registered twice (%s)", (int)fileID, name));
}
#endif

#if UNITY_EDITOR

BuiltinResourceManager& GetBuiltinExtraResourceManager ()
{
	return *gBuiltinExtraResourceManager;
}

#endif // #if UNITY_EDITOR

ResourceManager::ResourceManager (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_NeedsReload = true;
}

ResourceManager::~ResourceManager ()
{
}

ResourceManager::range ResourceManager::GetAll ()
{
	#if UNITY_EDITOR
	if (m_NeedsReload)
		RebuildResources();
	#endif

	return make_pair (m_Container.begin(), m_Container.end());
}

ResourceManager::range ResourceManager::GetPathRange (const string& path)
{
	#if UNITY_EDITOR
	if (m_NeedsReload)
		RebuildResources();
	#endif

	return m_Container.equal_range(ToLower(path));
}

#if UNITY_EDITOR
void ResourceManager::RebuildResources ()
{
	m_NeedsReload = !AssetDatabase::Get().GetAllResources(m_Container);
}
#endif

void ResourceManager::ClearDependencyInfo ()
{
	m_DependentAssets.clear ();
}

void ResourceManager::PreloadDependencies (SInt32 instanceId)
{
	std::set<SInt32> parents;
	PreloadDependencies (instanceId, parents);
}

void ResourceManager::PreloadDependencies (SInt32 instanceId, std::set<SInt32>& visited)
{
	if (visited.find (instanceId) != visited.end ())
		return;

	DependencyContainer::iterator it = std::lower_bound (m_DependentAssets.begin (), m_DependentAssets.end (), instanceId, ResourceManager::Dependency::Sorter ());
	// Check if we actually found info for the object we're looking for. As we're using lower_bound, we might get back
	// another object that is closest smallest.
	if (it == m_DependentAssets.end () || it->object.GetInstanceID () != instanceId)
		return;

	visited.insert (it->object.GetInstanceID ());

	for (size_t i = 0; i<it->dependencies.size (); ++i)
	{
		PPtr<Object> pptr = it->dependencies[i];
		*pptr; // fetch
		PreloadDependencies (pptr.GetInstanceID (), visited);
	}
}

template<class TransferFunc>
void ResourceManager::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	AssertIf (transfer.GetFlags() & kPerformUnloadDependencyTracking);

	transfer.Transfer(m_Container, "m_Container");

	// Wee need dependent-assets only in the player
	if (transfer.IsSerializingForGameRelease ())
		transfer.Transfer(m_DependentAssets, "m_DependentAssets");

	if (transfer.IsReading ())
		std::sort (m_DependentAssets.begin (), m_DependentAssets.end (), ResourceManager::Dependency::Sorter ());
}

bool ResourceManager::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}

IMPLEMENT_CLASS (ResourceManager)
IMPLEMENT_OBJECT_SERIALIZE (ResourceManager)
GET_MANAGER (ResourceManager)

