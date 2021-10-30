#include "UnityPrefix.h"
#include "EditorHelper.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Scripting/TextAsset.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Selection.h"
#include "Application.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Filters/Particles/ParticleEmitter.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Mono/MonoAttributeHelpers.h"
#include "Editor/Src/Undo/Undo.h"
#include "EditorExtensionImpl.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Platform/Interface/ExternalEditor.h"
#include "Runtime/Graphics/Transform.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Shaders/ComputeShader.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/Graphs/GraphUtils.h"
#include "Runtime/Animation/AnimatorController.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Mono/MonoScriptCache.h"


static const char* kScriptsDefaultApp = "kScriptsDefaultApp";
static const char* kScriptEditorArgs = "kScriptEditorArgs";
static const char* kImagesDefaultApp = "kImagesDefaultApp";


#if UNITY_OSX
#define kInternalEditorPath "MonoDevelop.app"
#elif UNITY_WIN
#define kInternalEditorPath "../MonoDevelop/bin/MonoDevelop.exe"
#elif UNITY_LINUX
// TODO: MD build for linux
#define kInternalEditorPath "gedit"
#else
#error "Unknown platform"
#endif

using namespace std;

static void UnloadObjectEmptied (Object& object)
{
	// Create empty proxy and load empty data in object (So that eg. if it has a ptr to another object that isn't destroyed implicitly)
	// Then destroy it
	// This works because we deactivate and remove all components from gameobjects before a merge anyway, so this is just a garbage collection cleanup
	dynamic_array<UInt8> emptyData(kMemTempAlloc);
	Object* emptyProxy = Object::Produce (object.GetClassID ());
	emptyProxy->Reset();
	WriteObjectToVector (*emptyProxy, &emptyData);
	if ( object.GetCachedScriptingObject() != SCRIPTING_NULL)
		object.SetCachedScriptingObject(SCRIPTING_NULL); // Set the CachedScriptingObject to NULL to fix bug 545131.
	ReadObjectFromVector (&object, emptyData);
	emptyProxy->HackSetAwakeWasCalled();
	DestroySingleObject (emptyProxy);

	UnloadObject (&object);
}

void UnloadFileEmptied (const string& path)
{
	set<SInt32> ids;
	GetPersistentManager ().GetInstanceIDsAtPath (path, &ids);
	for (set<SInt32>::iterator i=ids.begin ();i != ids.end ();i++)
	{
		Object* o = Object::IDToPointer (*i);
		if (o)
			UnloadObjectEmptied (*o);
	}
	
	#if DEBUGMODE
	for (set<SInt32>::iterator i=ids.begin ();i != ids.end ();i++)
	{
		Object* o = Object::IDToPointer (*i);
		AssertIf (o);
	}
	#endif
}

bool WarnPrefab (Object* object)
{
	return WarnPrefab (object, NULL, NULL, NULL);
}


bool WarnPrefab (Object* object, const char* title, const char *warning, const char *okString)
{
	if (!IsPrefabInstanceWithValidParent (object))
		return true;
	
	if (!warning || strlen(warning) == 0)
		warning = "This action will lose the prefab connection. Are you sure you wish to continue?";
	if (!okString || strlen(okString) == 0)
		okString = "Continue";
	if (!title || strlen(title) == 0)
		title = "Losing prefab";
	
	return DisplayDialog (title, warning, okString, "Cancel");
}

bool WarnPrefab (const std::set<EditorExtension*>& selection)
{
	return WarnPrefab(selection, NULL, NULL, NULL);
}

bool WarnPrefab (const set<EditorExtension*>& selection, const char* title, const char *warning, const char *okString)
{
	bool showWarning = false;
	for (std::set<EditorExtension *>::const_iterator i = selection.begin(); i!= selection.end(); i++)
	{
		// Warn if we have an instantiated prefab
		if (IsPrefabInstanceWithValidParent (*i))
		{
			showWarning = true;
			break;
		}
	}
	if (showWarning)
	{
		if (!warning || strlen(warning) == 0)
			warning = "This action will lose the prefab connection. Are you sure you wish to continue?";
		if (!okString || strlen(okString) == 0)
			okString = "Continue";
		if (!title || strlen(title) == 0)
			title = "Losing prefab";
		
		return DisplayDialog (title, warning, okString, "Cancel");
	}
	return true;
}



static std::vector<StartupFunc *> *gStartups = NULL;
void AddStartup (StartupFunc *func) {
	if (!gStartups)
		gStartups = new std::vector<StartupFunc *>;
	gStartups->push_back (func);
}
void ExecuteStartups () {
	if (!gStartups)
		return;
	for (std::vector<StartupFunc *>::iterator i = gStartups->begin(); i != gStartups->end(); i++)
		(*i)();
}

std::string GetBaseUnityDeveloperFolder()
{
	return DeleteLastPathNameComponent(DeleteLastPathNameComponent( GetApplicationFolder() ));
}

std::string GetTargetsBuildFolder()
{
	return DeleteLastPathNameComponent( GetApplicationFolder() );
}

extern bool IsBatchmode();
bool IsApplicationActive () 
{ 
	return IsApplicationActiveOSImpl() || IsBatchmode();
}

bool IsDeveloperBuild (bool checkHumanControllingUs)
{
	if (checkHumanControllingUs && !IsHumanControllingUs()) return 0;

	static int gIsDeveloperBuild = -1;
	if (gIsDeveloperBuild == -1)
	{
		string baseFolder = GetBaseUnityDeveloperFolder();
		string engineFolder = AppendPathName (baseFolder, "Runtime"); 
		string editorFolder = AppendPathName (baseFolder, "Editor"); 
		gIsDeveloperBuild = IsDirectoryCreated (engineFolder) && IsDirectoryCreated (editorFolder);
	}
	return gIsDeveloperBuild;
}

bool IsUserModifiable (Object& object)
{
	return !object.TestHideFlag(Object::kNotEditable);
}

bool IsUnitySceneFile (const std::string& path)
{
	return StrICmp(GetPathNameExtension(path), "unity") == 0;
}

std::string  RunSaveBuildPanel( BuildTargetPlatform target, const std::string& title, const std::string& directory, 
					   const std::string& defaultName, const std::string& extension ) 
{
	if (!extension.empty() || UNITY_OSX || kBuildStandaloneLinux == target || kBuildStandaloneLinux64 == target || kBuildStandaloneLinuxUniversal == target)
	{
#if UNITY_OSX
		if (target == kBuild_iPhone)
			return RuniPhoneReplaceOrAppendPanel (title, directory, defaultName);
		else
#endif
			return RunSavePanel (title, directory, extension, defaultName);
	}
	else
	{
		return RunSaveFolderPanel(title, directory, defaultName, true);
	}
}


bool SavePanelIsValidInProject (void* userData, const string& fileName)
{
	string cPathName = GetProjectRelativePath (fileName);
	
	if (BeginsWith(ToLower(cPathName), "assets") == 0)
	{
		DisplayDialog("Need to save in the Assets folder", "You need to save the file inside of the project's assets folder", "Ok");
		return false;
	}
	
	if (!CheckValidFileName(GetLastPathNameComponent(fileName)))
	{
		DisplayDialog("Correct the file name", "Special characters and reserved names cannot be used for file names.", "Ok");
		return false;
	}
	
	return StrICmp (GetPathNameExtension(cPathName), *reinterpret_cast<string*> (userData)) == 0;
}

bool SavePanelShouldShowFileNameIsValidInProject (void* userData, const string& fileName)
{
	string cPathName = ToLower (fileName);
	// Show folders that 
	// - are inside the project path
	// - lead to the project path	
	if (IsDirectoryCreated (cPathName))
	{
		string projectPath = ToLower (File::GetCurrentDirectory ());
		if (cPathName.find (projectPath) == 0)
			return true;
		else if (projectPath.find (cPathName) == 0)
			return true;
		else
			return false;
	}
	// Show files with unity extension if inside project path
	else
	{
		cPathName = GetProjectRelativePath (cPathName);
		return StrICmp (GetPathNameExtension(cPathName), *reinterpret_cast<string*> (userData)) == 0;
	}
}

std::string RunSavePanelInProject (const std::string& title, const std::string& defaultName, std::string extension, const std::string message, const std::string path)
{
	string result = RunComplexSavePanel(title, message, "", PathToAbsolutePath(path), defaultName, extension, &SavePanelIsValidInProject, &SavePanelShouldShowFileNameIsValidInProject, reinterpret_cast<void*> (&extension));
	return GetProjectRelativePath (result);
}

Ticker::Ticker (float rate)
{
	m_Rate = rate;
	m_IsInitialized = false;
}

bool Ticker::Tick ()
{
	if (!m_IsInitialized)
	{
		m_IsInitialized = true;
		m_Time = GetTimeSinceStartup();
		return true;
	}
	
	float deltaTime = GetTimeSinceStartup() - m_Time;
	if (deltaTime < m_Rate)
		return false;

	m_Time = GetTimeSinceStartup();
	
	return true;
}


bool IsUserModifiableScript (Object& object)
{
	if (object.TestHideFlag (Object::kDontSave))
		return false;
		
	string serializedPath = GetPersistentManager ().GetPathName (object.GetInstanceID ());
	string pathName = GetGUIDPersistentManager ().AssetPathNameFromAnySerializedPath (serializedPath);
	// Object is not persistent. So it is stored in a scene.
	if (pathName.empty ())
		return false;
	else
		return true;
}


/*
bool HasFatherInSelection (GameObject& go, const set<PPtr<Object> >& selection)
{
	Transform* object = go.QueryComponent (Transform);
	if (object == NULL)
		return false;
		
	Transform* father = object->GetParent ();
	while (father)
	{
		GameObject* go = father->GetGameObjectPtr ();
		if (selection.count (go) == 1)
			break;
		father = father->GetParent ();
	}

	if (father == NULL)
		return false;
	else
		return true;
}
*/

string GetExternalScriptEditor()
{
	return EditorPrefs::GetString(kScriptsDefaultApp);
}

string GetExternalScriptEditorArgs()
{
	return EditorPrefs::GetString(kScriptEditorArgs);
}

string ProcessExternalEditorArgs(string args, const string& file, int line)
{
	if (!args.empty())
	{
		replace_string(args, "$(File)", file); 
		if (line >= 0)
			replace_string(args, "$(Line)", IntToString(line));
		else
			replace_string(args, "$(Line)", "0");
	}

	return args;
}

bool TryOpenErrorFileFromConsole (const string& path, int line)
{
	if (path.empty ())
		return false;

	// Absolute pathnames are always internal Unity source files, and not files
	// in the project.
	if( IsAbsoluteFilePath(path) )
	{
		if (!IsDeveloperBuild ())
		{
			return false;
		}
		if (GetPathNameExtension (path) == "cs" || GetPathNameExtension (path) == "as")
		{
			string externalScriptEditor = GetExternalScriptEditor();
			string externalScriptEditorArgs = GetExternalScriptEditorArgs();
			string completePath = PathToAbsolutePath(path); 
			externalScriptEditorArgs = ProcessExternalEditorArgs(externalScriptEditorArgs, completePath, line);
			if (OpenFileAtLine (completePath, line, externalScriptEditor, externalScriptEditorArgs, kOpenFileExternalEditor))
				return true;
		}
	}

	// Open asset 
	UnityGUID guid = GetGUIDPersistentManager ().GUIDFromAnySerializedPath (path);
	if (AssetDatabase::Get ().IsAssetAvailable (guid))
	{
		Object* o = AssetDatabase::Get ().AssetFromGUID (guid).mainRepresentation.object;
		if (o)
			return OpenAsset (o->GetInstanceID (), line);
	}
	return false;
}

bool OpenScriptFile (const std::string& completePath, int line)
{
	string externalScriptEditor = EditorPrefs::GetString(kScriptsDefaultApp);
	string externalScriptEditorArgs = EditorPrefs::GetString(kScriptEditorArgs);
	string internalEditor = AppendPathName(GetApplicationFolder(), kInternalEditorPath);

	externalScriptEditorArgs = ProcessExternalEditorArgs(externalScriptEditorArgs, completePath, line);

	if (!externalScriptEditor.empty())
	{
		if (OpenFileAtLine (completePath, line, externalScriptEditor, externalScriptEditorArgs, kOpenFileExternalEditor))
			return true;
	}
	if (OpenFileAtLine (completePath, line, internalEditor, "", kOpenFileInternalEditor))
		return true;

	// Open it with the default app if nothing else worked!
	if (OpenWithDefaultApp(completePath))
		return true;

	return false;
}

static bool TryOpenAssetInUnity (int instanceID, int line)
{
	// Find and execute all correctly formed [Callbacks.OnOpenAsset] attributed methods
	ScriptingArguments arguments;
	arguments.AddInt(instanceID);
	arguments.AddInt(line);
	
	ScriptingMethodPtr cmpParamMethod = GetScriptingManager().GetScriptingMethodRegistry().GetMethod("UnityEditor", "Empty", "OnOpenAsset");
	return CallMethodsWithAttributeAndReturnTrueIfUsed (MONO_COMMON.onOpenAssetAttribute, arguments, cmpParamMethod->monoMethod);
}

UnityGUID GUIDFromAsset (int instanceID)
{
	string serializedPath = GetPersistentManager ().GetPathName (instanceID);
	return GetGUIDPersistentManager ().GUIDFromAnySerializedPath (serializedPath);
}

bool IsAssetAvailable (int instanceID)
{
	Object* o = PPtr<Object> (instanceID);
	if (o == NULL)
		return false;

	UnityGUID guid = GUIDFromAsset (instanceID);

	return AssetDatabase::Get ().IsAssetAvailable (guid);
}

bool OpenAsset (int instanceID, int line)
{
	Object* o = PPtr<Object> (instanceID);
	if (o == NULL)
		return false;
	
	UnityGUID guid = GUIDFromAsset (instanceID);
	
	// Assets that were created with external tools and imported should be opened with that tool!
	if (AssetDatabase::Get ().IsAssetAvailable (guid))
	{
		int type = AssetDatabase::Get ().AssetFromGUID (guid).type;
		AnimatorController* controller = dynamic_pptr_cast<AnimatorController*> (o);
		if (type == kCopyAsset)
		{
			string assetPath = GetAssetPathFromGUID (guid);
			string completePath = PathToAbsolutePath (assetPath);
			
			if (TryOpenAssetInUnity (instanceID, line))
				return true;

			// Try to open scripts
			if (dynamic_pptr_cast<TextAsset*> (o) || dynamic_pptr_cast<ComputeShader*> (o))
			{
				if (OpenScriptFile (completePath, line))
					return true;
			}
			// Open textures (but not movies) with external graphics tool
			else if (o->IsDerivedFrom (Object::StringToClassID ("Texture")) && ToLower (GetPathNameExtension (assetPath)) != "mov")
			{
				string imageApp = EditorPrefs::GetString(kImagesDefaultApp);
				if (!imageApp.empty() && OpenFileWithApplication(completePath, imageApp))
					return true;
			}
			// Open unity scene files in Unity.
			else if (ToLower (GetPathNameExtension (assetPath)) == "unity")
			{
				return GetApplication().OpenFileGeneric(completePath);
			}			
			
			// Open it with the default app if nothing else worked!
			if (OpenWithDefaultApp(completePath))
				return true;
			WarningStringWithoutStacktrace ("Unable to open " + assetPath + ": Check external application preferences");
			
			// Do not select copy assets
			return false;
		}
		else if(controller)
		{
			ScriptingInvocation(kGraphsEditorBaseNamespace, "AnimatorControllerTool", "DoWindow").Invoke();
		}
	}
	else if (IsGraph(o))
	{
		ScriptingInvocation(kGraphsEditorBaseNamespace, "FlowWindow", "ShowFlowWindow").Invoke();
	}

	// Just select the object
	SetActiveObject(o);
	
	// Serialized assets we just select (e.g when double clicking a object reference for a material that material is selected)
	Selection::SetActiveID (instanceID);
	return true;
}

int GetObjectEnabled (Object* object)
{
	Behaviour *behaviour = dynamic_pptr_cast<Behaviour *> (object);
	Renderer *renderer = dynamic_pptr_cast<Renderer *> (object);
	Collider *collider = dynamic_pptr_cast<Collider *> (object);
	LODGroup *lodGroup = dynamic_pptr_cast<LODGroup *> (object);
	ParticleEmitter *emitter = dynamic_pptr_cast<ParticleEmitter *> (object);
	GameObject *go = dynamic_pptr_cast<GameObject *> (object);

	if (behaviour && behaviour->ShouldDisplayEnabled())
		return behaviour->GetEnabled ();
	else if (renderer)
		return renderer->GetEnabled ();
	else if (emitter)
		return emitter->GetEnabled ();
	else if (collider)
		return collider->GetEnabled ();
	else if (lodGroup)
		return lodGroup->GetEnabled ();
	else if (go && !go->IsPrefabParent ())
		return go->IsActive ();
	else
		return -1;
}

static void SetActiveRecursively (GameObject& go, bool active)
{
	go.SetSelfActive(active);

	Transform* trans = go.QueryComponent(Transform);
	if (trans)
	{
		for (int i = 0; i < trans->GetChildrenCount(); i++)
		{
			SetActiveRecursively (trans->GetChild (i).GetGameObject(), active);
		}
	}
}

void SetObjectEnabled (Object* object, bool enabled)
{
	if (object == NULL || object->TestHideFlag(Object::kNotEditable))
		return;
	
	Behaviour *behaviour = dynamic_pptr_cast<Behaviour *> (object);
	Renderer *renderer = dynamic_pptr_cast<Renderer *> (object);
	Collider *collider = dynamic_pptr_cast<Collider *> (object);
	LODGroup *lodGroup = dynamic_pptr_cast<LODGroup *> (object);
	ParticleEmitter *emitter = dynamic_pptr_cast<ParticleEmitter *> (object);
	GameObject *go = dynamic_pptr_cast<GameObject *> (object);

	if (behaviour)
		behaviour->SetEnabled(enabled);
	else if (renderer)
		renderer->SetEnabled(enabled);
	else if (collider)
		collider->SetEnabled(enabled);
	else if (emitter)
		emitter->SetEnabled(enabled);
	else if (lodGroup)
		lodGroup->SetEnabled(enabled);
	else if (go && !go->IsPrefabParent())
	{
		bool recursive = false;
		Transform *t = go->QueryComponent (Transform);
		if (t)
		{
			if (t->GetChildrenCount())
			{
				if (enabled)
				{
					if (DisplayDialog ("Activate game object", "Do you wish to activate all child objects", "Activate children", "Only this game object"))
						recursive = true;
				}
				else
				{
					if (DisplayDialog ("Deactivate game object", "Do you wish to deactivate all child objects", "Deactivate children", "Only this game object"))
						recursive = true;
				}
				
			}
		}
		
		if (recursive)
			SetActiveRecursively(*go, enabled);
		else
			go->SetSelfActive(enabled);
	}
}

bool replace(std::string& str, const std::string& from, const std::string& to)
{
	size_t start_pos = str.find(from);
	if(start_pos == std::string::npos)
		return false;
	str.replace(start_pos, from.length(), to);
	return true;
}

// Based on AssetInterface::RenameAsset -- renames the class inside the script when the script gets renamed
std::string RenameScript (const UnityGUID& guid, std::string name, MonoScript* script)
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset == NULL)
		return "The source asset could not be found";

	if (name.empty() || name.find_last_not_of (' ') == string::npos)
		return "Can't rename to empty name";

	// Calculate the new asset path
	string oldAssetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
	string folderPath = DeleteLastPathNameComponent (oldAssetPath);
	if (folderPath.empty ())
		return "No folder in asset's path";

	string newAssetPath = AppendPathName (folderPath, name);

	if (asset->type != kFolderAsset)
		newAssetPath = AppendPathNameExtension (newAssetPath, GetPathNameExtension (oldAssetPath));

	// Just return success in case we are changing the asset name to the exact same name as before
	if (newAssetPath == oldAssetPath)
		return string();

	string error = AssetDatabase::Get().ValidateMoveAsset (guid, newAssetPath);

	if (error.empty())
	{
		error = AssetInterface::Get().MoveAsset (guid, newAssetPath);

		if (error.empty())
		{
			AssetInterface::Get().ImportAtPath(newAssetPath);
		}
	}
	return error;
}

void SetObjectNameSmart (PPtr<Object> obj, string name)
{
	UnityGUID guid = ObjectToGUID(obj);
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);

	if (asset && asset->mainRepresentation.object.GetInstanceID () == obj.GetInstanceID())
	{
		string err;
		
		if (obj->GetClassName() == MonoScript::GetTypeString())
		{
			Object* objPtr = obj;
			MonoScript* monoScript = static_cast<MonoScript*>(objPtr);
			
			if (monoScript->GetScriptType () == kScriptTypeMonoBehaviourDerived)
			{
				err = RenameScript(guid, name, monoScript);
			}
			else
			{
				err = AssetInterface::Get().RenameAsset(guid, name);
			}
		}
		else
		{
			err = AssetInterface::Get().RenameAsset(guid, name);
		}
		if (!err.empty())
			WarningStringObject(err, obj);
	}
	else
	{
		Object* target = obj;
		if (target == NULL)
			return;
		
		if (target->GetName() == name)
			return;

		if (target->TestHideFlag(Object::kNotEditable))
			return;

		Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (target);
		if (com && com->GetGameObjectPtr())
			target = com->GetGameObjectPtr();
			
		RecordUndoDiff(target, Append("Rename ", target->GetName()));
		target->SetNameCpp(name);
	}
}


struct _fat_header
{
	UInt32 magic;
	UInt32 nfat_arch;
};

struct _fat_arch
   {
   int    cputype;
   int    cpusubtype;
   UInt32 offset;
   UInt32 size;
   UInt32 align;
};

enum MachoBitness {
	kMachoBitness32 = 0,
	kMachoBitness64 = 0x01000000
};

enum MachoCPUType {
	kMachoArchi386 = 7,
	kMachoArchPPC = 18,
	kMachoArchx86_64 = (kMachoArchi386 | kMachoBitness64)
};

bool StripFatMacho (const std::string& path, bool generate_x86_64)
{
	File file;
	file.Open(path, File::kReadWritePermission);
	
	_fat_header header;
	file.Read(&header, sizeof(_fat_header));
	
	#if UNITY_LITTLE_ENDIAN
	SwapEndianBytes(header.magic);
	SwapEndianBytes(header.nfat_arch);
	#endif
		
	if (header.magic == 0xCAFEBABE)
	{
		for (int i=0;i<header.nfat_arch;i++)
		{
			_fat_arch arch;
			file.Read(&arch, sizeof(_fat_arch));
			
			#if UNITY_LITTLE_ENDIAN
			SwapEndianBytes(arch.cputype);
			SwapEndianBytes(arch.cpusubtype);
			SwapEndianBytes(arch.offset);
			SwapEndianBytes(arch.size);
			SwapEndianBytes(arch.align);
			#endif
			
			if ((generate_x86_64 && arch.cputype == kMachoArchx86_64) || (!generate_x86_64 && arch.cputype == kMachoArchi386))
			{
				UInt8* data = new UInt8[arch.size];
				file.Read(arch.offset, data, arch.size);

				file.SetFileLength(arch.size);
				
				file.Write(0, data, arch.size);

				file.Close();
				delete[] data;
				return true;
			}
		}
	}
	
	file.Close();
	return false;
}

bool WarnInconsistentLineEndings(const std::string& contents)
{
	int length = contents.size();
	int width = 1;		
	// Unicode line ending is 0xE280A8
	
	// UTF-8 and UTF-7 are variable width encodings and only use 1 byte codes for LF/CR just like ASCII
	if (length >= 3 && ((unsigned char)contents.at(0)) == 0xef && ((unsigned char)contents.at(1)) == 0xbb && ((unsigned char)contents.at(2)) == 0xbf)
	{
		//printf_console("Detected UTF-8 encoding");
		width = 1;
	}
	else if (length >= 3 && ((unsigned char)contents.at(0)) == 0x2b && ((unsigned char)contents.at(1)) == 0x2f && ((unsigned char)contents.at(2)) == 0x76)
	{
		//printf_console("Detected UTF-7 encoding");
		width = 1;
	}
	else if (length >= 2 && ((unsigned char)contents.at(0)) == 0xff && ((unsigned char)contents.at(1)) == 0xfe)
	{
		//printf_console("Detected UTF-16, UCS-2LE, UCS-4LE, UCS16-LE encoding");
		width = 2;
	}
	else if (length >= 2 && ((unsigned char)contents.at(0)) == 0xfe && ((unsigned char)contents.at(1)) == 0xff)
	{
		//printf_console("Detected UCS-2, UCS16 encoding");
		width = 2;
	}
	if (length >= 4 && ((unsigned char)contents.at(0)) == 0x0 && ((unsigned char)contents.at(1)) == 0x0 && ((unsigned char)contents.at(2)) == 0xfe && ((unsigned char)contents.at(3)) == 0xff)
	{
		//printf_console("Detected UTF-32, UCS-4 encoding");
		width = 4;
	}
	
	if (length > width && contents.at(length-width) == 0xA && contents.at(length-(2*width)) == 0xD)
	{
		//printf_console("Detected CR/LF, windows line endings\n");
		string::size_type location = contents.find(0x0A, 0);
		while (location != string::npos && location < contents.size())
		{
			if (location >= width && contents.at(location - width) != 0x0D)
			{
				//printf_console("WARNING: Inconsistent line endings, found Mac OS X line ending at %d where there should only be Windows\n", location);
				return true;
			}
			location = contents.find(0x0A, location+1);
		}
	}
	else if (length >= width && contents.at(length-width) == 0xA)
	{
		//printf_console("Detected LF, Mac OS X and Unix line endings\n");
		string::size_type location = contents.find(0x0D, 0);
		if (location != string::npos)
		{
			//printf_console("WARNING: Inconsistent line endings, found windows line ending at byte %d where there should only be Mac OS X\n", location);
			return true;
		}
	}
	// Try to detect a line ending and then see if another type is also present, might not work where multibyte characters might contain a portion of CR or LF byte code
	else
	{
		// First instance of LF
		string::size_type location = contents.find(0x0A, 0);
		if (location != string::npos)
		{
			// Windows line ending
			if (location >= width && contents.at(location - width) == 0x0D)
			{
				// Parse through all LFs and make sure they are all preceeded with CR
				location = contents.find(0x0A, location+width);
				while (location != string::npos && location < contents.size())
				{
					if (contents.at(location - width) != 0x0D)
					{
						return true;
					}
					location = contents.find(0x0A, location+width);
				}
			}	
			// UNIX line ending						
			else
			{
				// There should not be any cases of 0x0D bytes with UNIX type line endings
				location = contents.find(0x0D, 0);
				if (location != string::npos)
				{
					return true;
				}
			}
		}
	}
	return false;
}

std::string GetMainAssetNameFromAssetPath (const std::string& path)
{
	if (IsDirectoryCreated(path))	
		return GetLastPathNameComponent (path);
	else
		return DeletePathNameExtension (GetLastPathNameComponent (path));
}
/*
string GetProjectRelativePath (const string& path)
{
	string lowerPath = ToLower (path);
	if (lowerPath.find (ToLower (File::GetCurrentDirectory ()) + '/') != 0)
		return string ();
	int offset = File::GetCurrentDirectory ().size () + 1;
	string newPath (path.begin () + offset, path.end ());	
	return StandardizePathName (newPath);
}
*/
void GetSolutionFileToOpenForSourceFile (const std::string& path, bool useMonoDevelop, std::string* solutionPath, std::string* outputFilePath)
{
	// Is the file a Unity source code file
	bool isUnitySourceCode = false;
	if (IsDeveloperBuild())
	{
		string lowerPath = ToLower (path);
		if (lowerPath.find (ToLower (GetBaseUnityDeveloperFolder () + "/Runtime") ) == 0)
			isUnitySourceCode = true;
		if (lowerPath.find (ToLower (GetBaseUnityDeveloperFolder () + "/Editor") ) == 0)
			isUnitySourceCode = true;
	}
	
	if (isUnitySourceCode)
	{
		*solutionPath = AppendPathName (GetBaseUnityDeveloperFolder (), "Projects/CSharp/CSharpProjects.sln");

		//@TODO: The path fo the file goes through a symlink in this solution.
		//       The fix below doesn't work on os x since we use FSRef which automatically resolves the symlink.
		
//		string relativePath = path;
//		relativePath.erase(0, GetBaseUnityDeveloperFolder ().size () + 1);
//		string tempNewPath = AppendPathName(GetBaseUnityDeveloperFolder (), "Projects/CSharp/root");
//		tempNewPath = AppendPathName(tempNewPath, relativePath);
//		if (IsFileCreated(tempNewPath))
//			*outputFilePath = tempNewPath;
//		else
//			*outputFilePath = path;
		*outputFilePath = path;
	}
	else
	{
		*outputFilePath = path;
		
		*solutionPath = PathToAbsolutePath( GetLastPathNameComponent( File::GetCurrentDirectory() ) );
		if (useMonoDevelop)
			*solutionPath += ".sln";
		else
			*solutionPath += "-csharp.sln";
	}
}
