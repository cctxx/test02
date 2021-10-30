#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/AssetPipeline/MovieImporter.h"
#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Camera/Flare.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/AssetPipeline/DefaultImporter.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Video/MovieTexture.h"
#include "Runtime/Graphics/CubemapTexture.h"
#include "Editor/Src/PackageUtility.h"
#include "Editor/Src/CommandImplementation.h"
#include "Editor/Src/Utility/ObjectNames.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Utilities/PlayerPrefs.h"


static const char* kResourcesFolder = "Resources";
static const char* kImportPackageMenuPrefix = "import-";
static const char* kImportScriptMenuPrefix = "script-";


bool HasNativeImporterForFile (const string& filename);

inline string GetResourcesFolder ()
{
	return AppendPathName (GetApplicationContentsPath (), kResourcesFolder);
}

bool HasNativeImporterForFile (void* userData, const string& filename)
{
	AssetImporterSelection importerSelection = AssetImporter::FindAssetImporterSelection (filename);
	return importerSelection.importerClassID != ClassID (DefaultImporter);
}

struct ScriptInfo {
	string menuName;
	string fileName;
	string srcFileName;
	int sortOrder;
};
static std::vector<ScriptInfo>* scripts = NULL;


static const char *FindNextMinus (const char *src)
{
	while (*src != '-')
	{
		if (!*src)
			return NULL;
		++src;
	}
	return ++src;
}

static void GetScriptsList(vector<ScriptInfo>& paths, const string &folder, const char *extension)
{
	int extLen = extension ? strlen (extension) : 0;
	
	if (IsDirectoryCreated (folder)) 
	{
		set<string> subpaths;
		GetFolderContentsAtPath(folder, subpaths);
		
		for (set<string>::iterator i=subpaths.begin();i != subpaths.end();i++)
		{
			ScriptInfo si;
			si.srcFileName = *i;
			string s = GetLastPathNameComponent (*i);
			// Skip if we have a required extension and it doesn't match
			if (extension)
			{
				int slen = s.length();
				if (slen < extLen || s.compare (slen - extLen, extLen, extension))
					continue;
				// Remove the .txt extension
				s = string (s, 0, slen - extLen);
			}
			
			const char *c1 = s.c_str ();
			const char *c2 = FindNextMinus (c1);
			const char *c3 = c2 ? FindNextMinus (c2) : NULL;
			if (c3)
			{
				si.sortOrder = atoi (c1);
				si.menuName = string (s, c2 - c1, c3 - c2 - 1);
				si.fileName = string (s, c3 - c1);
			}
			else {
				si.sortOrder = 100;
				if (c2)
				{
					si.fileName = c2;
					si.menuName = string (s, 0, c2 - c1 - 1);
				} else {
					si.fileName = s;
					si.menuName = MangleVariableName (DeletePathNameExtension (si.fileName).c_str());
				}
			}
			
			paths.push_back(si);
		}
	}
	return ;
}


struct AssetsMenu : public MenuInterface
{

	void CreateAsset (Object& obj, const string& newName, const string& extension)
	{
		void* params[] = { Scripting::ScriptingWrapperFor (&obj), MonoStringNew (AppendPathNameExtension(newName, extension)) };
		CallStaticMonoMethod ("ProjectWindowUtil", "CreateAsset", params);

	}
	
	
	void CreateDefaultAsset (const char* className, const char* assetName, const char* extension)
	{
		Object* obj = Object::Produce(Object::StringToClassID(className));
		SmartResetObject(*obj);
		
		CreateAsset (*obj, assetName, extension);
	}

	void AddRenderTexture ()
	{
		RenderTexture& renderTexture = *CreateObjectFromCode<RenderTexture> ();
		renderTexture.Reset();
		renderTexture.AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
		
		CreateAsset(renderTexture, "New Render Texture", "renderTexture");
	}

	void AddAnimationClip ()
	{
		AnimationClip& clip = *CreateObjectFromCode<AnimationClip> ();
		clip.SetAnimationType (AnimationClip::kGeneric);
		
		CreateAsset(clip, "New Animation", "anim");
	}

	void AddCubemap ()
	{
		Cubemap& cubemap = *NEW_OBJECT(Cubemap);
		cubemap.Reset();
		
		cubemap.InitTexture(64, 64, kTexFormatARGB32, Texture2D::kNoMipmap, 6);
		cubemap.SetIsReadable(false);
		// cubemaps will be presented to user to fill, so we clear them intially to avoid noise
		// we want it grey to avoid blending with gui
		// WARNING: relies on images to be layed-out sequentially (cubemap case exactly)
		::memset (cubemap.GetRawImageData(), 0xA0, cubemap.GetStorageMemorySize());
		
		SmartResetObject(cubemap); 		
		
		CreateAsset (cubemap, "New Cubemap", "cubemap");
	}

	void AddFlare ()
	{
		string name = "New Flare";
		Texture *texture = dynamic_pptr_cast<Texture*> (GetActiveObject ());
		if (texture && texture->GetClassID() == ClassID(Cubemap))
			texture = NULL;

		Flare& flare = *CreateObjectFromCode<Flare> ();
		SmartResetObject(flare);
		if (texture)
			flare.SetTexture (texture);
		
		CreateAsset (flare, name, "flare");
	}

	void AddAnimatorController()
	{
		CallStaticMonoMethod ("ProjectWindowUtil", "CreateAnimatorController");
	}


	void AddMaterial ()
	{
		// Find out shader we want to attach to and a nice name
		Shader* shader = dynamic_pptr_cast<Shader*> (GetActiveObject ());
		string name = "New Material";
		if (shader)
			name = shader->GetNamedObjectName();
		else
			shader = GetScriptMapper().GetDefaultShader();
		
		Material* material = Material::CreateMaterial(*shader, 0);
		if (material)
			CreateAsset (*material, name, "mat");
	}

	void AddFolder ()
	{
		CallStaticMonoMethod ("ProjectWindowUtil", "CreateFolder");
	}

	void CreateCopyAsset (const string &srcFile, const string &destFile)
	{
		void *params[] = { MonoStringNew (srcFile), MonoStringNew (destFile) };
		CallStaticMonoMethod ("ProjectWindowUtil", "CreateScriptAsset", params);
	}

	void AddPrefab ()
	{
		CallStaticMonoMethod ("ProjectWindowUtil", "CreatePrefab");
	}
		
	bool CanOpenAsset ()
	{
		TempSelectionSet objects;
		GetObjectSelection(0,objects);
		return !objects.empty ();
	}
	
	bool CanDeleteSelectedAssets ()
	{
		set<UnityGUID> guids = GetSelectedAssets ();
		if (guids.empty ())
			return false;

		// If selection contains the assets folder disallow deletion
		for (set<UnityGUID>::const_iterator i=guids.begin();i != guids.end();i++)
			if (*i == kAssetFolderGUID)
				return false;

		return true;
	}
	
	void OpenAsset ()
	{
		TempSelectionSet objects;
		GetObjectSelection(0,objects);
		for (TempSelectionSet::iterator i=objects.begin ();i != objects.end ();i++)
		{
			::OpenAsset ((**i).GetInstanceID ());
		}
	}
	void DeleteSelectedAssets()
	{
		bool askIfSure = true;
		void* param[] = { &askIfSure };
		CallStaticMonoMethod("ProjectBrowser", "DeleteSelectedAssets", param);
	}

	void RevealInFinder ()
	{
		set<UnityGUID> selection = GetSelectedAssets ();
		for (set<UnityGUID>::iterator i = selection.begin ();i != selection.end ();i++)
			::RevealInFinder (GetAssetPathFromGUID (*i));
		if (selection.empty ())
			::RevealInFinder (GetAssetPathFromGUID (GetSelectedAsset ()));
	}
	
	void Reimport ()
	{
		set<UnityGUID> selection = GetSelectedAssets ();
		AssetInterface::Get ().ImportAssets (selection, kForceUpdate | kImportRecursive | kDontImportWithCacheServer | kMayCancelImport);
	}
	
	
	void CreateATonOfTestAssets ()
	{
		for (int i=0;i<1000;i++)
		{
			string dir = AppendPathName("Assets", IntToString(i));
			CreateDirectory(dir);
			for (int j=0;j<30;j++)
			{
				string path = AppendPathName(dir, Format("MyAsset %d.txt", j));
				if (!IsFileCreated(path))
					WriteBytesToFile("TestData", strlen("TestData"), path);
			}
		}
		
		AssetInterface::Get().Refresh();
	}
	
	void ImportNewAsset ()
	{
		const char* message = 
			"You can also import files by dragging them from the Finder into the Project view\n"
			"or manually copying them to the asset folder.";
		string file = RunComplexOpenPanel ("Import New Asset", message, "Import", ResolveSymlinks ("~/Desktop"), &HasNativeImporterForFile, &HasNativeImporterForFile, NULL);
		
		if (!file.empty ())
		{
			// Find file name to place it in.
			// Place it as a child of the current selection or root folder
			string parentFolder = GetAssetPathFromGUID (GetSelectedAsset ());
			string newFileName;
			if (!IsDirectoryCreated (parentFolder))
				parentFolder = DeleteLastPathNameComponent (parentFolder);

			newFileName = AppendPathName (parentFolder, GetLastPathNameComponent (file));
			newFileName = GenerateUniquePath (newFileName);
			
			// Copy file
			if (!CopyFileOrDirectory (file, newFileName))
			{
				ErrorString (Format ("Failed copying file %s to %s.", file.c_str (), newFileName.c_str ()));
			}

			// Refresh
			AssetInterface::Get ().Refresh ();
			
			// Select the the created asset						
			UnityGUID asset;
			if (GetGUIDPersistentManager ().PathNameToGUID (newFileName, &asset) && AssetDatabase::Get ().IsAssetAvailable (asset))
			{
				SetActiveObject (AssetDatabase::Get ().AssetFromGUID (asset).mainRepresentation.object);
				FocusProjectView();
			}
		}
	}

	void RebuildAssets ()
	{
		if (DisplayDialog ("Are you sure you want to reimport all assets?", "Rebuilding assets is only used if your project has been corrupted due to an internal Unity bug. It can take several hours to complete, depending on the size of your project. \n\nPlease submit a bug report detailing the steps leading up to here.", "Reimport", "Cancel"))
		{
			vector<string> args;
			args.push_back ("-rebuildlibrary");
			RelaunchWithArguments (args);
		}
	}
	
	void ImportPackage ()
	{
		string fileName = RunOpenPanel("Import package ...", EditorPrefs::GetString("LastAssetImportDir", ""), "unitypackage");
		if (fileName != "")
		{
			EditorPrefs::SetString("LastAssetImportDir", TrimSlash(fileName));
			ImportPackageGUI(fileName);
		}
	}
	
	void ExportPackage ()
	{
		CallStaticMonoMethod ("PackageExport", "ShowExportPackage");
	}

	set<UnityGUID> ObjectsToGUIDs (const set<Object*>& objects)
	{
		set<UnityGUID> guids;
		for (set<Object*>::const_iterator i=objects.begin();i != objects.end();i++)
		{
			UnityGUID guid = ObjectToGUID (*i);
			if (guid != UnityGUID())
				guids.insert(guid);
		}
		return guids;
	}

	void SelectDependencies ()
	{
		
		// Calculate the selected guids (children of folders are automatically added)
		// if nothing is selected we select all
		set<UnityGUID> guids = GetSelectedAssets (kDeepAssetsSelection);
		// Objects from the selection
		TempSelectionSet objects;
		GuidsToObjects(guids, objects);
		if (objects.empty())
			GetObjectSelection(0,objects);
		
		// Collect dependencies
		TempSelectionSet objectDeps;
		CollectAllDependencies(objects,objectDeps);

		// Don't select everything in the project view.
		// Just the root assets
		GuidsToObjects(ObjectsToGuids(objectDeps), objectDeps);
		
		// Set selection
		SetObjectSelection(objectDeps);

		CallStaticMonoMethod ("ProjectBrowser", "ShowSelectedObjectsInLastInteractedProjectBrowser");
	}

	void ExportOggFile()
	{
		set<UnityGUID> selection = GetSelectedAssets ();
		TempSelectionSet objects;
		GuidsToObjects(selection, objects);

		for (TempSelectionSet::iterator i=objects.begin ();i != objects.end ();i++)
		{
#if ENABLE_MOVIES
			MovieTexture *m = dynamic_cast<MovieTexture*> (*i);
			if(m != NULL)
			{
				string prompt = Append("Save Ogg file for Movie ", m->GetName());
				string name = m->GetName();
				string extension = "ogv";
				string path = RunSavePanel(prompt, "" , extension, name);
				if(!path.empty())
					ExtractOggFile(m, path);
			}
			else
#endif
			{
				AudioClip *c = dynamic_cast<AudioClip*> (*i);
				if(c != NULL && (c->GetType() == FMOD_SOUND_TYPE_OGGVORBIS || c->GetType() == FMOD_SOUND_TYPE_MPEG))
				{
					string prompt = Append("Save compressed file for AudioClip ", c->GetName());
					string name = c->GetName();
					string extension = c->GetType() == FMOD_SOUND_TYPE_OGGVORBIS?"ogg":"mp3";
					string path = RunSavePanel(prompt, "" ,extension,name);
					if(!path.empty())
						ExtractOggFile(c, path);
				}
			}
		}
	}
	
	bool CanExportCompressedFile()
	{
		set<UnityGUID> selection = GetSelectedAssets ();
		TempSelectionSet objects;
		GuidsToObjects(selection, objects);

		for (TempSelectionSet::iterator i=objects.begin ();i != objects.end ();i++)
		{
#if ENABLE_MOVIES
			MovieTexture *m = dynamic_cast<MovieTexture*> (*i);
			if(m != NULL)
				return true;
			else
#endif
			{
				AudioClip *c = dynamic_cast<AudioClip*> (*i);
				if(c != NULL)
					if(c->GetType() == FMOD_SOUND_TYPE_OGGVORBIS || c->GetType() == FMOD_SOUND_TYPE_MPEG)
						return true;
			}
		}
		return false;
	}

	virtual bool Validate (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) {
		case 1:
			return !GetSelectedAssets ().empty (); // Reimport
		
		//case 2: Reveal in Finder/Explorer does not need validation. If no assets is selected we fall back to the root Assets folder

		case 3:
			return CanOpenAsset ();

		case 4:
			return CanDeleteSelectedAssets ();

		case 107:
			return IsDirectoryCreated("Assets/Editor Default Resources");

		default:
			return true;
		}
		return false;
	}
	
	virtual void Execute (const MenuItem &menuItem)
	{
		if (BeginsWith (menuItem.m_Command, kImportPackageMenuPrefix))
		{
			std::string packagePath = menuItem.m_Command.substr (strlen(kImportPackageMenuPrefix), menuItem.m_Command.size()-strlen(kImportPackageMenuPrefix));
			ImportPackageGUI (packagePath);
			return;
		}
		
		if (BeginsWith (menuItem.m_Command, kImportScriptMenuPrefix))
		{
			int idx = atoi (menuItem.m_Command.substr (strlen(kImportScriptMenuPrefix), menuItem.m_Command.size()-strlen(kImportScriptMenuPrefix)).c_str());
			ScriptInfo &si = (*scripts)[idx];
			CreateCopyAsset (si.srcFileName, si.fileName);
			return;
		}
		
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) {

		case 1:
			Reimport ();
			break;
		case 2:
			RevealInFinder ();
			break;
		case 3:
			OpenAsset ();
			break;
		case 4:
			DeleteSelectedAssets();
			break;
		case 50:
			AddPrefab ();
			break;
		case 52:
			AddAnimationClip ();
			break;
		case 53:
			AddFolder ();
			break;
		case 60:
			AddMaterial ();
			break;
		case 61:
			AddCubemap();
			break;
		case 62:
			AddFlare ();
			break;
		case 63:
			CreateDefaultAsset ("Font", "New Font", "fontsettings");
			break;
		case 64:
			AddRenderTexture ();
			break;
		case 65:
			CreateDefaultAsset ("PhysicMaterial", "New Physic Material", "physicMaterial");
			break;
		case 66:
			CreateDefaultAsset ("PhysicsMaterial2D", "New Physics2D Material", "physicsMaterial2D");
			break;
		case 67:
			AddAnimatorController();
			break;
		case 71 :
			CreateDefaultAsset ("AvatarMask", "New Avatar Mask", "mask");
			break;		
		case 72 :
			CreateDefaultAsset ("AnimatorOverrideController", "New Animator Override Controller", "overrideController");
			break;		
		case 90:
			ImportNewAsset ();
			break;

		case 100:
			AssetInterface::Get ().Refresh ();
			break;
		case 101:
			RebuildAssets ();
			break;

		case 102:
			ImportPackage ();
			break;
		case 103:
			ExportPackage ();
			break;

		case 105:
			SelectDependencies ();
			break;
		case 301:
			CreateATonOfTestAssets();
		break;
		}
		return;
		
	}
};

void AssetsRegisterMenu () {
	AssetsMenu* menu = new AssetsMenu;

	// Start of Assets/Create
	
	// Folder is unique
	MenuController::AddMenuItem ("Assets/Create/Folder", "53", menu, 20);	
	
	// All scripts
	if (!scripts)
		scripts = new vector<ScriptInfo> ();
	else {
		scripts->clear ();
	}
	GetScriptsList (*scripts, AppendPathName (GetResourcesFolder (), "ScriptTemplates"), ".txt");
	GetScriptsList (*scripts, AppendPathName (File::GetCurrentDirectory (), "Assets/ScriptTemplates"), ".txt");
	for (int i = 0; i < scripts->size(); i++)
	{
		ScriptInfo& si = (*scripts)[i];
		MenuController::AddMenuItem ("Assets/Create/" + si.menuName, Format ("%s%d", kImportScriptMenuPrefix, i), menu, si.sortOrder);		
	}
	
	// Prefab is unique
	MenuController::AddMenuItem ("Assets/Create/Prefab", "50", menu, 201);
	
	// Rendering / graphics related
	MenuController::AddMenuItem ("Assets/Create/Material", "60", menu, 301);
	MenuController::AddMenuItem ("Assets/Create/Cubemap", "61", menu, 302);
	MenuController::AddMenuItem ("Assets/Create/Lens Flare", "62", menu, 303);
	if (LicenseInfo::Flag (lf_pro_version))
	{
		MenuController::AddMenuItem ("Assets/Create/Render Texture", "64", menu, 304);
	}
	
	// Animation related
	MenuController::AddMenuItem ("Assets/Create/Animator Controller", "67", menu, 401);	
	MenuController::AddMenuItem ("Assets/Create/Animation", "52", menu, 402);
	MenuController::AddMenuItem ("Assets/Create/Animator Override Controller", "72", menu, 403);	
	MenuController::AddMenuItem ("Assets/Create/Avatar Mask", "71", menu, 404);
	
	// Physics related
	MenuController::AddMenuItem ("Assets/Create/Physic Material", "65", menu, 501);
	MenuController::AddMenuItem ("Assets/Create/Physics2D Material", "66", menu, 502);
	
	// GUI related
	// 601 reserved for GUISkin
	MenuController::AddMenuItem ("Assets/Create/Custom Font", "63", menu, 602);
	
	// End of Assets/Create

	#if UNITY_OSX
	MenuController::AddMenuItem ("Assets/Reveal in Finder", "2", menu, 20);
	#elif UNITY_WIN
	MenuController::AddMenuItem ("Assets/Show in Explorer", "2", menu, 20);
	#elif UNITY_LINUX
	MenuController::AddMenuItem ("Assets/Open Containing Folder", "2", menu, 20);
	#else
	#error "Unknown platform"
	#endif
	MenuController::AddMenuItem ("Assets/Open", "3", menu, 20);
	MenuController::AddMenuItem ("Assets/Delete", "4", menu, 20);

	
	MenuController::AddSeparator ("Assets/", 20);

	MenuController::AddMenuItem ("Assets/Import New Asset...", "90", menu, 20);
	MenuController::AddMenuItem ("Assets/Import Package/Custom Package...", "102", menu, 20);

	// list standard packages
	// for now, do not list asset store packages here. If someone is feeling adventurous, they can insert
	// a menu item that would open asset store download manager here.
	std::vector<PackageInfo> packages;
	GetPackageList (packages, true, false);
	if (!packages.empty()) MenuController::AddSeparator ("Assets/Import Package/", 20);

	std::string menuPath = "Assets/Import Package/";
	for (std::vector<PackageInfo>::const_iterator i=packages.begin();i != packages.end();i++)
	{
		std::string packageName = DeletePathNameExtension (GetLastPathNameComponent(i->packagePath));
		if(packageName[0]!='[') 
			MenuController::AddMenuItem (menuPath + packageName, kImportPackageMenuPrefix + i->packagePath, menu, 20);
		else
		{
			const size_t bracketIndex = packageName.find_first_of(']');
			std::string menuString = packageName.substr(1,bracketIndex-1);
			std::string subMenuString = packageName.substr(bracketIndex+1,packageName.length());
			std::string fullMenuString = menuPath + menuString + "/" + subMenuString;
			MenuController::AddMenuItem (fullMenuString, kImportPackageMenuPrefix + i->packagePath, menu, 20);
		}
	}

	MenuController::AddMenuItem ("Assets/Export Package...", "103", menu, 20);
	MenuController::AddMenuItem ("Assets/Select Dependencies", "105", menu, 30);
	MenuController::AddSeparator ("Assets/", 40);

	MenuController::AddMenuItem ("Assets/Refresh %r", "100", menu, 40);
	MenuController::AddMenuItem ("Assets/Reimport", "1", menu, 40);
	
	MenuController::AddSeparator ("Assets/", 40);
	
	MenuController::AddMenuItem ("Assets/Reimport All", "101", menu, 40);

	if (IsDeveloperBuild())
		MenuController::AddMenuItem ("Assets/Internal/Create A Ton of Test Assets", "301", menu, 40);
}

STARTUP (AssetsRegisterMenu)	// Call this on startup.
