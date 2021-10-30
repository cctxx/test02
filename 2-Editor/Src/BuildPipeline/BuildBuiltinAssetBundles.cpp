#include "UnityPrefix.h"
#include "BuildBuiltinAssetBundles.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Camera/GraphicsSettings.h"
#include "Runtime/Serialize/BuildTargetVerification.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Src/Utility/AssetPreviewPostFixFile.h"
#include "Editor/Src/Utility/BuildPlayerUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/Utility/AssetPreviewGeneration.h"

static string GetBuiltinResourcesBuiltOutputDirectory ();

static void BuildingBuiltinResourceFileInstanceIDResolveCallback (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, void* context)
{
	if (ResolveInstanceIDMapping(id, localIdentifier, *reinterpret_cast<InstanceIDBuildRemap*>(context)))
		return;
	
	Assert(!GetPersistentManager().IsInstanceIDFromCurrentFileInternal(id));
	
	// References to anything outside of the builtin resource file itself are disallowed!
	localIdentifier.localSerializedFileIndex = 0;
	localIdentifier.localIdentifierInFile = 0;
}

static void BuildingBuiltinResourceExtraFileInstanceIDResolveCallback (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, void* context)
{
	if (ResolveInstanceIDMapping(id, localIdentifier, *reinterpret_cast<InstanceIDBuildRemap*>(context)))
		return;
	
	// We can reference builtin resources too
	if (IsDefaultResourcesObject(id))
		return GetPersistentManager ().InstanceIDToLocalSerializedObjectIdentifierInternal (id, localIdentifier);
	
	Assert(!GetPersistentManager().IsInstanceIDFromCurrentFileInternal(id));
	
	// This is a problem, most likely the referenced object is missing, or the user explicitly didn't want it to be included
	// So just mark it as a null reference so that when loading it, we don't actually try to load it from disk.
	// printf_console ("    OBJECT [player] %d (from '%s') is OUT!\n", id, GetPersistentManager ().GetPathName(id).c_str ());
	localIdentifier.localSerializedFileIndex = 0;
	localIdentifier.localIdentifierInFile = 0;
}

inline Object* FindBuiltinResource (const string& pathName, int classID)
{
	PersistentManager& pm = GetPersistentManager ();
	PersistentManager::ObjectIDs objects;
	pm.GetInstanceIDsAtPath (pathName, &objects);
	pm.GetInstanceIDsAtPath (GetMetaDataPathFromAssetPath (pathName), &objects);
	for (PersistentManager::ObjectIDs::iterator i=objects.begin ();i != objects.end ();i++)
	{
		Object* o = PPtr<Object> (*i);
		if (o && o->IsDerivedFrom (classID))
			return o;
	}
	return NULL;
}

bool GenerateBuiltinAssetPreviews ()
{
	string targetpath = GetBuiltinResourcesBuiltOutputDirectory ();
	targetpath = AppendPathName(targetpath, "builtin_previews");

	InstanceIDToLocalIdentifier remap;
	vector<LibraryRepresentation> builtins;
	BuiltinResourceManager::Resources& resources = GetBuiltinResourceManager().m_Resources;
	for (BuiltinResourceManager::Resources::iterator i=resources.begin ();i != resources.end ();i++)
	{
		Object* asset = PPtr<Object> (i->cachedInstanceID);
		if (asset != NULL)
		{
			LibraryRepresentation rep;
			rep.object = asset;
			builtins.push_back(rep);
			
			LocalIdentifierInFileType localIdentifierInFile = GetPersistentManager().GetLocalFileID(i->cachedInstanceID);
			remap.push_unsorted(i->cachedInstanceID, localIdentifierInFile);
		}
	}
	BuiltinResourceManager::Resources& resourcesExtra = GetBuiltinExtraResourceManager().m_Resources;
	for (BuiltinResourceManager::Resources::iterator i=resourcesExtra.begin ();i != resourcesExtra.end ();i++)
	{
		Object* asset = PPtr<Object> (i->cachedInstanceID);
		if (asset != NULL)
		{
			LibraryRepresentation rep;
			rep.object = asset;
			builtins.push_back(rep);

			LocalIdentifierInFileType localIdentifierInFile = GetPersistentManager().GetLocalFileID(i->cachedInstanceID);
			remap.push_unsorted(i->cachedInstanceID, localIdentifierInFile);
		}
	}
	remap.sort();
	
	LibraryRepresentation temp;
	return AppendAssetPreviewToMetaData (targetpath, temp, builtins, "", remap);
}

bool BuildBuiltinAssetBundle (BuiltinResourceManager& resourceManager, const string& targetFile, const string& resourcesFolder, InstanceIDResolveCallback* resolveCallback, const std::set<std::string>& ignoreList, BuildTargetSelection platform, int options)
{
	SwitchActiveBuildTargetForBuild(platform.platform, false); // do not check support for build target while building default resources
	
	CreateDirectory ("Temp");
	const char* kTempAssetBundle = "Temp/resourceFile";
	
	ResetSerializedFileAtPath (kTempAssetBundle);
	
	resourceManager.RegisterBuiltinScriptsForBuilding ();
	
	InstanceIDToBuildAsset assets;
	bool success = true;
	// Persist the builtin resources
	BuiltinResourceManager::Resources& resources = resourceManager.m_Resources;
	for (BuiltinResourceManager::Resources::iterator i=resources.begin ();i != resources.end ();i++)
	{
		if (ignoreList.count(i->name))
			continue;
		
		Object* object = NULL;
		if (i->classID == ClassID(MonoScript))
		{
			int instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(kResourcePath, i->fileID);
			object = PPtr<Object> (instanceID);
			if (object == NULL)
			{
				instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(AppendPathName ("GeneratedBuiltinScripts", i->name), 1);
				object = PPtr<Object> (instanceID);
				
			}
			
			if (object)
				object->SetHideFlags(0);
		}
		else
		{
			object = FindBuiltinResource (AppendPathName (resourcesFolder, i->name), i->classID);	
			if (object)
			{
				// We want all objects from builtin resource bundles to not be editable
				object->SetHideFlags (object->GetHideFlags() | Object::kNotEditable);
			}
		}
		
		if (object)
		{
			if (assets.count (object->GetInstanceID ()))
			{
				ErrorStringObject ("Asset is already added in the asset bundle!\n", object);
				success = false;
			}
			
			AddBuildAssetInfoChecked (object->GetInstanceID (), kTempAssetBundle, -1, assets, i->fileID, true);
			AssertIf(assets.count(object->GetInstanceID ()) == 0);
		}
		else
		{
			ErrorString ("The resource " + string(i->name) + " could not be found in the folder " + resourcesFolder);
			success = false;
		}
	}
	
	if (!success)
		return false;
	
	success &= WriteSharedAssetFile(kTempAssetBundle, assets, platform, resolveCallback, options);
	
	if (!success)
		return false;
	FileSizeInfo info;
	info.AddAssetFileSize(kTempAssetBundle, TemporaryFileToAssetPathMap(assets));
	info.Print();	
	
	if (!MoveReplaceFile (kTempAssetBundle, targetFile))
	{
		ErrorString ("Failed to copy asset bundle! " + targetFile);
		return false;
	}
	
	return true;
}

bool BuildEditorAssetBundles ()
{
	string unityFolder = GetBaseUnityDeveloperFolder();
	TransferInstructionFlags littleEndianMask = IsBigEndian() ? kSwapEndianess : kNoTransferInstructionFlags;
	
	CallStaticMonoMethod("AssembleEditorSkin", "DoIt");
	CallStaticMonoMethod("AssembleEditorSkin", "RegenerateAllIconsWithMipLevels");
	
	UnityGUID guid;
	set<UnityGUID> guids;
	const char* kBasePath = "Assets/Editor Default Resources";
	const char* kBasePathWithSlash = "Assets/Editor Default Resources/";
	if (GetGUIDPersistentManager().PathNameToGUID(kBasePath, &guid))
		AssetDatabase::Get().CollectAllChildren(guid, &guids);
	
	if (guids.empty())
	{
		ErrorString("No editor assets were found in Assets/Editor Default Resources");
		return false;
	}

	// Allow the editor resources bundle to reference all the shaders
	// in the extra resources by putting all those shaders on the always
	// included list.  This allows us to not have them included in the
	// editor resources which would lead to unnecessary duplication.
	//
	// Doing this is safe, though, as we only load the editor resources
	// into the editor where the extra resources are always available.
	vector<Object*> extraResources = GetBuiltinExtraResourceManager ().GetAllResources ();
	for (int i = 0; i < extraResources.size (); ++i)
	{
		Shader* shader = dynamic_cast<Shader*> (extraResources[i]);
		if (shader)
		{
			PPtr<Shader> shaderPPtr (shader);
			if (!GetGraphicsSettings ().IsAlwaysIncludedShader (shaderPPtr))
				GetGraphicsSettings ().AddAlwaysIncludedShader (shaderPPtr);
		}
	}
	
	vector<PPtr<Object> >	 objects;
	vector<string> names;
	for (set<UnityGUID>::iterator i=guids.begin();i != guids.end();i++)
	{
		if (IsDirectoryCreated(GetAssetPathFromGUID(*i)))
			continue;
		
		string path = GetAssetPathFromGUID(*i);
		Assert(path.find(kBasePathWithSlash) == 0);
		string strippedPath(path.begin() + strlen(kBasePathWithSlash), path.end());
		names.push_back(strippedPath);
		objects.push_back(GetMainAsset(path));
	}
	
	InstanceIDToBuildAsset assets;
	
	string dstPath = AppendPathName (unityFolder, "Editor/Resources/Common/unity editor resources");
	string internalName = "CustomAssetBundle-" + GUIDToString(guid);
	
	GetPersistentManager().SetPathRemap (internalName, dstPath);
	string error = BuildCustomAssetBundle(NULL, objects, &names, internalName, 0, assets, kBuildNoTargetPlatform, littleEndianMask, kAssetBundleIncludeCompleteAssets | kAssetBundleDeterministic);
	GetPersistentManager().SetPathRemap (internalName, "");
	
	if (!error.empty())
	{
		ErrorString(error);
		return false;
	}
	else
	{
		return true;
	}
}

static string GetBuiltinResourcesBuiltOutputDirectory ()
{
	string unityFolder = GetBaseUnityDeveloperFolder();
	return AppendPathName (unityFolder, "External/Resources/builtin_resources/builds_output");
}

bool BuildBuiltinAssetBundles ()
{
	const char* kResourcesFolder = "Assets/DefaultResources";
	const char* kResourcesExtraFolder = "Assets/DefaultResourcesExtra";
	
	
	int littleEndianMask = IsBigEndian() ? kSwapEndianess : 0;
	int bigEndianMask = IsBigEndian() ? 0 : kSwapEndianess;
	
	set<string> emptyIgnore;
	
	// Verify font material since this keeps on happening...
	string fontMaterialPath = AppendPathName(kResourcesFolder, kDefaultFontName);
	Material* fontMaterial = AssetImporter::GetFirstDerivedObjectAtPath<Material>(fontMaterialPath);
	if (fontMaterial == NULL)
	{
		ErrorString(Format("Builtin resources Font has not been imported correctly. You must reimport it before you can build the builtin resources. (Material %s not found)", fontMaterialPath.c_str()));
		return false;
	}
	string fontShaderPath = GetAssetPathFromObject(fontMaterial->GetShader());
	string fontShaderTargetPath = AppendPathName(kResourcesFolder, "Font.shader");
	if (fontMaterial == NULL || StrICmp(fontShaderPath, fontShaderTargetPath) != 0)
	{
		ErrorString(Format("Builtin resources font has not been imported correctly. You must reimport it before you can build the builtin resources. (Shader expected to be '%s', was '%s')", fontShaderTargetPath.c_str(), fontShaderPath.c_str()));
		return false;
	}
	
	string targetPath = GetBuiltinResourcesBuiltOutputDirectory();
	BuiltinResourceManager& resources = GetBuiltinResourceManager();
	
	// Editor builtin bundle
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection::NoTarget(), bigEndianMask))
		return false;
	
	
	// Builtin extra asset bundle
	if (!BuildBuiltinAssetBundle (GetBuiltinExtraResourceManager(), AppendPathName (targetPath, "unity_builtin_extra"), kResourcesExtraFolder, BuildingBuiltinResourceExtraFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection::NoTarget(), bigEndianMask))
		return false;
	
	littleEndianMask |= kDisableWriteTypeTree | kSerializeGameRelease;
	bigEndianMask |= kDisableWriteTypeTree | kSerializeGameRelease;
	
	// All player builtin resources
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources player little endian"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildStandaloneWinPlayer,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources iphone es20"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuild_iPhone,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources android"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuild_Android,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources bb10"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildBB10,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources tizen"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildTizen,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources pcgles"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildWinGLESEmu,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources xbox360"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildXBOX360,0), bigEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources ps3"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildPS3,0), bigEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources alchemy"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildFlash,0), littleEndianMask))
		return false;
#if INCLUDE_WEBGLSUPPORT				
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources webgl"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildWebGL,0), littleEndianMask))
		return false;
#endif
	// Add for all three metro targets?!
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources metro"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildMetroPlayerX86, 0), littleEndianMask))
		return false;
#if INCLUDE_WP8SUPPORT
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources wp8"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildWP8Player, 0), littleEndianMask))
		return false;
#endif

	set<string> webplayerIgnore;
	webplayerIgnore.insert("UnitySplash.png");
	webplayerIgnore.insert("UnitySplash2.png");
	webplayerIgnore.insert("UnitySplash3.png");
	webplayerIgnore.insert("UnitySplashBack.png");
	// All web player builtin resources
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources nacl"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, webplayerIgnore, BuildTargetSelection(kBuildNaCl,0), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources web d3d"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, webplayerIgnore, BuildTargetSelection(kBuildWebPlayerLZMA,kWebBuildSubtargetDirect3D), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity default resources web gl"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, webplayerIgnore, BuildTargetSelection(kBuildWebPlayerLZMA,kWebBuildSubtargetOpenGL), littleEndianMask))
		return false;
	
	return true;
}


bool BuildBuiltinOldWebAssetBundles ()
{
	const char* kResourcesFolder = "Assets";
	
	int littleEndianMask = IsBigEndian() ? kSwapEndianess : 0;
	littleEndianMask |= kSerializeGameRelease;
		
	set<string> emptyIgnore;
	string targetPath = AppendPathName (GetBaseUnityDeveloperFolder(), "External/Resources/builtin_web_old/builds_output");
	
	BuiltinResourceManager& resources = GetBuiltinOldWebResourceManager();
	resources.InitializeOldWebResources();
	
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity_web_d3d"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildWebPlayerLZMA,kWebBuildSubtargetDirect3D), littleEndianMask))
		return false;
	if (!BuildBuiltinAssetBundle (resources, AppendPathName (targetPath, "unity_web_gl"), kResourcesFolder, &BuildingBuiltinResourceFileInstanceIDResolveCallback, emptyIgnore, BuildTargetSelection(kBuildWebPlayerLZMA,kWebBuildSubtargetOpenGL), littleEndianMask))
		return false;
	
	return true;
}

static void AddShaderToBuildAssets (Shader* shader, const string& targetPath, InstanceIDToBuildAsset& buildAssets)
{
	AddBuildAssetInfoWithLocalIdentifier (
		shader->GetInstanceID (),
		targetPath,
		-1,
		GetPersistentManager ().GetLocalFileID (shader->GetInstanceID ()),
		buildAssets);
}

static void AddShadersAndDependenciesToBuildAssets (const PPtr<Shader>* shaders, int count, const string& targetPath, InstanceIDToBuildAsset& buildAssets)
{
	for (int i = 0; i < count; ++i)
	{
		Shader* shader = (Shader*) shaders[i];
		if (!shader)
			continue;

		AddShaderToBuildAssets (shader, targetPath, buildAssets);

		const ShaderPtrVector& dependencies = shader->GetDependencies ();
		AddShadersAndDependenciesToBuildAssets (dependencies.data (), dependencies.size (), targetPath, buildAssets);
	}
}

/// Build "Resources/unity_builtin_extra" for the given player, if necessary.
void BuildExtraResourcesBundleForPlayer (BuildTargetPlatform platform, const string& outputDirectory)
{
	// Create path to our extra resources files.  Note that we deliberately use an explicit path here
	// rather than relying on remapped paths like we do for other files we generate during player builds.
	// The reason is that the extra resources path is already remapped to the resource bundle in the
	// editor build and even if we try to temporarily replace that mapping, bad things happen.
	string targetPath = AppendPathName (outputDirectory, kDefaultExtraResourcesPath);

	// Gather all always-included shaders and their dependencies.
	InstanceIDToBuildAsset buildAssets;
	GraphicsSettings::ShaderArray shaders = GetGraphicsSettings ().GetAlwaysIncludedShaders ();
	AddShadersAndDependenciesToBuildAssets (shaders.data (), shaders.size (), targetPath, buildAssets);

	// If we have some shaders to bundle up, write the file.
	if (buildAssets.size () > 0)
	{
		// Create directory structure.
		CreateDirectoryRecursive (AppendPathName (outputDirectory, "Resources"));

		// Gather transfer flags.
		TransferInstructionFlags transferFlags = CalculateEndianessOptions (platform) | kSerializeGameRelease;
		if (!IsWebPlayerTargetPlatform (platform))
			transferFlags |= kDisableWriteTypeTree;

		// Write file.
		WriteSharedAssetFile (
			targetPath,
			buildAssets,
			BuildTargetSelection (platform, 0),
			BuildingBuiltinResourceExtraFileInstanceIDResolveCallback,
			transferFlags);
	}
}
