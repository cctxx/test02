#include "UnityPrefix.h"
#include "GfxDeviceRecreate.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Dynamics/ClothRenderer.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Filters/Misc/TextMesh.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/Graphics/GeneratedTextures.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ComputeShader.h"
#if ENABLE_PROFILER
#include "Runtime/Profiler/ProfilerImpl.h"
#endif
#if UNITY_EDITOR
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Editor/Src/Application.h"
#endif

#if ENABLE_TERRAIN
#include "Runtime/Interfaces/ITerrainManager.h"
#endif

#if ENABLE_WEBCAM
#include "Runtime/Video/VideoTexture.h"
#endif

static void UnloadAllTextures()
{
	#if ENABLE_WEBCAM
	BaseVideoTexture::SuspendVideoTextures ();
	#endif

	Texture::ReloadAll (true, false, true);
}

static void LoadAllTextures()
{
	Texture::ReloadAll (false, true);

	#if ENABLE_WEBCAM
	BaseVideoTexture::ResumeVideoTextures ();
	#endif
}

static void UnloadAllShaders()
{
	std::vector<SInt32> allShaders;
	Shader::DeleteAllShaders(allShaders);
	Shader::UnloadDefaultShaderLabShader();

	// Now, some shaders might have been unsupported, which means they were pointing to default
	// ShaderLab shader. Go over all of them and make sure ShaderLab shaders are NULL everywhere.
	for (std::vector<SInt32>::iterator i = allShaders.begin(); i != allShaders.end(); ++i)
	{
		Shader *s = PPtr<Shader> (*i);
		if (s)
			s->ResetInternalPointersToNull();
	}
}

static void LoadAllShaders()
{
	// Default pink shader is handled specially in shader load/unload because it's shared
	// between multiple shader objects. We have to recreate it here.
	Shader::LoadDefaultShaderLabShader();

	std::vector<SInt32> allShaders;
	Object::FindAllDerivedObjects (ClassID (Shader), &allShaders);
	Shader::RecreateAllShaders(allShaders);
}

static void UnloadAllComputeShaders()
{
	vector<SInt32> objs;
	Object::FindAllDerivedObjects (ClassID (ComputeShader), &objs);
	for (size_t i = 0, n = objs.size(); i != n; ++i)
	{
		ComputeShader* obj = PPtr<ComputeShader> (objs[i]);
		obj->UnloadFromGfxDevice();
	}
}

static void LoadAllComputeShaders()
{
	vector<SInt32> objs;
	Object::FindAllDerivedObjects (ClassID (ComputeShader), &objs);
	for (size_t i = 0, n = objs.size(); i != n; ++i)
	{
		ComputeShader* obj = PPtr<ComputeShader> (objs[i]);
		obj->ReloadToGfxDevice();
	}
}

static void UnloadAllMeshes()
{
	vector<SInt32> meshes;
	Object::FindAllDerivedObjects (ClassID (Mesh), &meshes, true);
	for (size_t i = 0, n = meshes.size(); i != n; ++i)
	{
		Mesh* mesh = PPtr<Mesh> (meshes[i]);
		mesh->UnloadVBOFromGfxDevice();
	}
}

static void LoadAllMeshes()
{
	vector<SInt32> meshes;
	Object::FindAllDerivedObjects (ClassID (Mesh), &meshes, true);
	for (size_t i = 0, n = meshes.size(); i != n; ++i)
	{
		Mesh* mesh = PPtr<Mesh> (meshes[i]);
		mesh->ReloadVBOToGfxDevice();
	}
}

// Note: nothing needs to be done to "unload" fonts
static void LoadAllFonts()
{
	vector<SInt32> fonts;
	Object::FindAllDerivedObjects (ClassID (Font), &fonts, true);
	for (size_t i = 0, n = fonts.size(); i != n; ++i)
	{
		Font* font = PPtr<Font> (fonts[i]);
		font->ResetCachedTexture();
	}
}

static void UnloadAllSkins()
{
	vector<SInt32> skins;
	Object::FindAllDerivedObjects (ClassID (SkinnedMeshRenderer), &skins, true);
	for (size_t i = 0, n = skins.size(); i != n; ++i)
	{
		SkinnedMeshRenderer* skin = PPtr<SkinnedMeshRenderer> (skins[i]);
		skin->UnloadVBOFromGfxDevice();
	}
}

static void LoadAllSkins()
{
	vector<SInt32> skins;
	Object::FindAllDerivedObjects (ClassID (SkinnedMeshRenderer), &skins, true);
	for (size_t i = 0, n = skins.size(); i != n; ++i)
	{
		SkinnedMeshRenderer* skin = PPtr<SkinnedMeshRenderer> (skins[i]);
		skin->ReloadVBOToGfxDevice();
	}
}


static void UnloadAllCloths()
{
#	if ENABLE_CLOTH
	vector<SInt32> cloths;
	Object::FindAllDerivedObjects (ClassID (ClothRenderer), &cloths, true);
	for (size_t i = 0, n = cloths.size(); i != n; ++i)
	{
		ClothRenderer* cloth = PPtr<ClothRenderer> (cloths[i]);
		cloth->UnloadVBOFromGfxDevice();
	}
#	endif // #if ENABLE_CLOTH
}

static void LoadAllCloths()
{
#	if ENABLE_CLOTH
	vector<SInt32> cloths;
	Object::FindAllDerivedObjects (ClassID (ClothRenderer), &cloths, true);
	for (size_t i = 0, n = cloths.size(); i != n; ++i)
	{
		ClothRenderer* cloth = PPtr<ClothRenderer> (cloths[i]);
		cloth->ReloadVBOToGfxDevice();
	}
#	endif // #if ENABLE_CLOTH
}

#if ENABLE_TERRAIN
static void UnloadAllTerrains()
{
	GetITerrainManager()->UnloadTerrainsFromGfxDevice();
}

static void LoadAllTerrains()
{
	GetITerrainManager()->ReloadTerrainsToGfxDevice();
}
#endif

static void AwakeFromLoadTextMeshes()
{
	vector<SInt32> textMeshes;
	Object::FindAllDerivedObjects (ClassID (TextMesh), &textMeshes, true);
	for (size_t i = 0, n = textMeshes.size(); i != n; ++i)
	{
		TextMesh* textMesh = PPtr<TextMesh> (textMeshes[i]);
		textMesh->AwakeFromLoad(kDefaultAwakeFromLoad);
	}
}

void CleanupAllGfxDeviceResources()
{
	#if UNITY_EDITOR && UNITY_WIN // GfxWindow thing only exists on Windows right now
	ReleaseGfxWindowOnAllGUIViews();
	#endif

	TextMeshGenerator2::Flush();
	RenderTexture::ReleaseAll();
	ComputeBuffer::UnloadAllFromGfxDevice();
	UnloadAllComputeShaders();
	UnloadAllCloths();
	UnloadAllSkins();
	UnloadAllShaders();
	UnloadAllTextures();
	UnloadAllMeshes();

#if ENABLE_TERRAIN
	UnloadAllTerrains();
#endif

#if ENABLE_PROFILER
	UnityProfiler::CleanupGfx();
#endif

	GetGfxDevice().FinishRendering();

	DestroyGfxDevice();
}

void RecreateAllGfxDeviceResources()
{
	// Reinitialize graphics caps
	gGraphicsCaps = GraphicsCaps();
	#if UNITY_EDITOR
	gGraphicsCaps.ResetOriginalEmulationCaps();
	#endif

	InitializeGfxDevice();

	LoadAllMeshes();
	LoadAllTextures();
	LoadAllShaders();
	LoadAllFonts();
	LoadAllSkins();
	LoadAllCloths();
	LoadAllComputeShaders();
	ComputeBuffer::ReloadAllToGfxDevice();

#if ENABLE_TERRAIN
	LoadAllTerrains();
#endif

	// Neet to re-setup buit-in texture props on the new gfx device
	builtintex::ReinitBuiltinTextures();

	// Need to re-setup fog values etc.
	GetRenderSettings().AwakeFromLoad(kDefaultAwakeFromLoad);

	// Resetup text meshes, because dynamic font textures were reset
	AwakeFromLoadTextMeshes();

	#if UNITY_EDITOR && UNITY_WIN // GfxWindow thing only exists on Windows right now
	GetApplication().UpdateMainWindowTitle();
	CreateGfxWindowOnAllGUIViews();
	GUIView::RepaintAll(true);
	#endif

	// Reload current scene in the editor, so that any scripts that depend on the gfx device
	// capabilities are reinitialized.
	#if UNITY_EDITOR
	GetApplication().OpenScene(GetApplication().GetCurrentScene());
	#endif
}

void RecreateGfxDevice()
{
	CleanupAllGfxDeviceResources();
	RecreateAllGfxDeviceResources();
}

void RecreateSkinnedMeshResources()
{
	UnloadAllSkins();
	LoadAllSkins();
}

