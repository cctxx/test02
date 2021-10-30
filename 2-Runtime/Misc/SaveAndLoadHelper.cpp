#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "SaveAndLoadHelper.h"
#include "RegisterAllClasses.h"

#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/BaseClasses/ManagerContextLoading.h"
#include "Configuration/UnityConfigureRevision.h"
#include "Runtime/Misc/BuildSettings.h"
#if ENABLE_UNITYGUI
#include "Runtime/IMGUI/GUIManager.h"
#endif
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Misc/PreloadManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Animation/AnimationManager.h"
#include "QualitySettings.h"
#include "Runtime/Animation/AnimationClip.h"
#if ENABLE_AUDIO
#include "Runtime/Audio/AudioClip.h"
#endif
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Profiler/ProfilerHistory.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/MemoryProfilerStats.h"
#include "Runtime/Profiler/ProfilerConnection.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "CaptureScreenshot.h"
#include "Runtime/Graphics/Transform.h"
#include "GameObjectUtility.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/BaseClasses/Cursor.h"
#include "BatchDeleteObjects.h"

#include "Runtime/Testing/Testing.h"
#include "Runtime/Camera/UnityScene.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "External/shaderlab/Library/shaderlab.h"
#if ENABLE_MONO
#include "Runtime/Mono/MonoIncludes.h"
#endif
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoAttributeHelpers.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h" // ParticleSystem::UpdateAll ()
#include "Runtime/Filters/Particles/ParticleEmitter.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/IMGUI/GUIClip.h"
#include "Runtime/Threads/JobScheduler.h"
#include "External/shaderlab/Library/pass.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include "Runtime/Math/FloatExceptions.h"
#include "Runtime/Audio/AudioManager.h"

#if UNITY_EDITOR
#include "Runtime/BaseClasses/CleanupManager.h"
#include "Editor/Src/HierarchyState.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/AssetServer/ASCache.h"
#include "Editor/Src/EditorBuildSettings.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
	#if ENABLE_SPRITES
	#include "Editor/Src/SpritePacker/SpritePacker.h"
	#endif
#include "Editor/Src/BuildPipeline/LODGroupStripping.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorAssetGarbageCollectManager.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#include "Player.h"
#include "PlayerSettings.h"
#endif

#if ENABLE_WWW || ENABLE_CACHING
#include "PlatformDependent/CommonWebPlugin/UnityWebStream.h"
#include "CachingManager.h"
#include "PlatformDependent/CommonWebPlugin/CompressedFileStreamMemory.h"
#endif


#if SUPPORT_REPRODUCE_LOG
#include "ReproductionLog.h"
#endif

#include "Runtime/Network/PlayerCommunicator/EditorConnection.h"

const char* kMainData = "mainData";

using namespace std;

std::string SelectDataFolder ();

static void EditorBeforeLoadingCleanup ();
static void ResetManagersAfterLoadEditor ();
static void CollectAllSceneObjects (InstanceIDArray& instanceIDs);
static void CompleteAwakeSequence (const std::string& path, AwakeFromLoadQueue& awakeQueue);

void DestroyWorld (bool destroySceneAssets)
{
	InstanceIDArray objects;
	
	#if UNITY_EDITOR
	if (destroySceneAssets)
		CollectAllSceneObjects (objects);
	else
		CollectSceneGameObjects (objects);
	#else
	Assert(!destroySceneAssets);
	CollectSceneGameObjects (objects);
	#endif
	
	Object* o;
	// GameObjects first
	for (InstanceIDArray::iterator i=objects.begin ();i != objects.end ();++i)
	{
		o = Object::IDToPointer (*i);
		AssertIf (o && o->IsPersistent ());
		GameObject* go = dynamic_pptr_cast<GameObject*> (o);
		// Only Destroy root level GameObjects. The children will be destroyed
		// as part of them. That way, we ensure that the hierarchy is walked correctly,
		// and all objects in the hieararchy will be marked as deactivated when destruction happens.
		if (go != NULL && go->GetComponent(Transform).GetParent() == NULL)
			DestroyObjectHighLevel (o);
	}

	// normal objects whatever they might be after that
	for (InstanceIDArray::iterator i=objects.begin ();i != objects.end ();++i)
	{
		o = Object::IDToPointer (*i);
		AssertIf (o && o->IsPersistent ());
		DestroyObjectHighLevel (o);
	}

	objects.clear ();
	CollectLevelGameManagers (objects);

	// Gamemanagers & Scene last
	for (InstanceIDArray::iterator i=objects.begin ();i != objects.end ();++i)
	{
		o = Object::IDToPointer (*i);
		AssertIf (o && o->IsPersistent ());
		DestroyObjectHighLevel (o);
	}
	objects.clear ();
	
	GlobalCallbacks::Get().didUnloadScene.Invoke();

	#if DEBUGMODE
	ValidateNoSceneObjectsAreLoaded (destroySceneAssets);
	#endif
}

#if WEBPLUG
#define GET_DATA_FOLDER ""
#else
#define GET_DATA_FOLDER SelectDataFolder()
#endif

bool InitializeEngineNoGraphics ()
{
	static bool isInitialized = false;
	if (isInitialized == false)
	{
#if UNITY_EDITOR
		EditorAssetGarbageCollectManager::StaticInitialize();
#endif
		#if SUPPORT_THREADS
		Thread::mainThreadId = Thread::GetCurrentThreadID ();
		#endif
		#if ENABLE_PLAYERCONNECTION
			#if UNITY_EDITOR
			EditorConnection::Initialize();
			#else
			PlayerConnection::Initialize(GET_DATA_FOLDER);
			InstallPlayerConnectionLogging(true);
			#endif
		#endif
		
		#if ENABLE_PROFILER
		InitializeMemoryProfilerStats();
		UnityProfiler::Initialize();
		ProfilerConnection::Initialize();
		#if UNITY_EDITOR
		ProfilerHistory::Initialize();
		#endif
		#endif

#if ENABLE_PLAYERCONNECTION
#if UNITY_EDITOR
		EditorConnection::Get().PollWithCustomMessage();
#else
		// Bug: Calls GetBuildSettings() inside, but it's not yet initialized !!! At least on windows standalone player
#if !UNITY_WIN && !UNITY_OSX && !UNITY_IPHONE
		PlayerConnection::Get().Poll();
#endif
#endif
#endif

		InitializeBatchDelete();
		RegisterAllClasses ();
		Object::InitializeAllClasses ();
		GameObject::InitializeMessageIdentifiers ();
		ManagerContextInitializeClasses ();
		RenderBufferManager::InitRenderBufferManager ();

// On Linux Editor we initialize ScreenManager much earlier. Avoid doing that
// again here so we won't zero out the members.
#if !(UNITY_EDITOR && UNITY_LINUX)
		InitScreenManager ();
#endif

		InitFloatExceptions();		
		#if ENABLE_UNITYGUI
		InitGUIManager ();
		#endif
		
		// Init platform specific input support.
		// Aras: on windows input must be initialized later, after the
		// actual window is set up.
		#if !UNITY_WIN && !UNITY_WII && !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_FLASH && !UNITY_WEBGL && !UNITY_BB10 && !UNITY_TIZEN
		InputInit();
		#endif
		Object::CallInitializeClass ();
	}
	return true;
}

#if (UNITY_OSX || (UNITY_LINUX && SUPPORT_X11)) && !UNITY_PEPPER
void SetGraphicsBatchMode( bool batch );
#endif

bool InitializeEngineGraphics (bool batch)
{
	static bool isInitialized = false;
	if (isInitialized == false)
	{
		printf_console( "Initialize engine version: %s\n", UNITY_VERSION_FULL_NICE );

#if (UNITY_OSX || (UNITY_LINUX && SUPPORT_X11)) && !UNITY_PEPPER
		SetGraphicsBatchMode( batch );
#endif
#if UNITY_OSX || UNITY_FLASH || (UNITY_LINUX && WEBPLUG) || UNITY_WEBGL || (UNITY_WIN && !UNITY_WINRT) && !UNITY_PEPPER
		if( !InitializeGfxDevice() )
			return false;
#endif

#if ENABLE_PERFORMANCE_TESTS
		RUN_PERFORMANCE_TESTS();
#endif

		#if ENABLE_MULTITHREADED_CODE || ENABLE_MULTITHREADED_SKINNING
		CreateJobScheduler();
		#endif

		ShaderLab::InitShaderLab ();

		Object::CallPostInitializeClass ();
		GameObject::InitializeMessageHandlers ();
		BuiltinResourceManager::InitializeAllResources ();
		
		// Make sure the default shaders are always preloaded to avoid loading them during
		// threaded background loading
		Shader::LoadDefaultShaders ();

		isInitialized = true;
		GlobalCallbacks::Get().initializedEngineGraphics.Invoke();
	}
	return true;
}

void CleanupAllObjects (bool reloadable)
{
	vector<SInt32> objects;

	// Before each loop dealing with Objects here, we refetch all Objects. This is because
	// deleting some game object might actually create new objects (i.e. components are
	// dereferenced in GO destructor).

	// Delete all non-temporary game objects first
	Object::FindAllDerivedObjects (ClassID (Object), &objects);
	for (int i=0;i<objects.size();i++)
	{
		Object* o = Object::IDToPointer(objects[i]);
		if (o && o->IsDerivedFrom(ClassID(GameObject)) && !o->IsPersistent() && !o->TestHideFlag(Object::kDontSave))
		{
			DestroyObjectHighLevel (o);
		}
	}

	// Delete all game objects first
	objects.clear();
	Object::FindAllDerivedObjects (ClassID (Object), &objects);
	for (int i=0;i<objects.size();i++)
	{
		Object* o = Object::IDToPointer(objects[i]);
#if WEBPLUG
		Transform* t = GetIdentityTransform();
#endif
		if (o && !o->IsPersistent() && o->IsDerivedFrom(ClassID(GameObject)))
		{
#if WEBPLUG
			if(reloadable && o == t->GetGameObjectPtr()) {
				// THIS IS A MASSIVE HACK
				// We do this because, when the Unity player is reloading, it frees all
				// the gameobjects in memory.
				// The Renderer object keeps a global static singleton Identity Transform,
				// attached to an invisible game object that is created on first startup.
				// Because the startup code is tied deeply into the initialization of
				// the graphics engine, it's less hacky to Not Destroy that one game object.
				// We seriously need to eliminate these global statics or place them under
				// the management of a game manager object, which can refresh them as needed.
				continue;
			}
#endif
			DestroyObjectHighLevel (o);
		}
	}
	
	LockObjectCreation();
	
	// Do cleanup after all game objects are killed thus
	// terrains will be deleted, and RenderTexture which are allocated by the render buffer manager will not be deleted yet.
	TextMeshGenerator2::Flush();
	if (GetRenderBufferManagerPtr())
		GetRenderBufferManager().Cleanup();
	ShaderLab::Pass::DidClearAllTempRenderTextures ();
	
	// First of all we need to delete all non-temporary objects
	// This is because usually there are dependencies from non-temporary objects
	// to temporary objects
	objects.clear();
	Object::FindAllDerivedObjects (ClassID (Object), &objects);
	for (int i=0;i<objects.size();i++)
	{
		Object* o = Object::IDToPointer(objects[i]);
		if (o && !o->IsDerivedFrom(ClassID(GameManager)))
		{
			if (o != NULL && o->TestHideFlag(Object::kDontSave))
				continue;
			delete_object_internal (o);
		}
	}
	
	// Finally also cleanup all temporary objects
	objects.clear();
	Object::FindAllDerivedObjects (ClassID (Object), &objects);
	for (int i=0;i<objects.size();i++)
	{
		Object* o = Object::IDToPointer(objects[i]);
		if (o && !o->IsDerivedFrom(ClassID(GameManager)))
		{
			if (reloadable)
			{
				if (o != NULL && o->TestHideFlag(Object::kDontSave))
				{
					bool forceUnload = false;
					// Make sure AssetBundles are unloaded.
					if (o->GetClassID() == ClassID(AssetBundle))
						forceUnload = true;
#if ENABLE_SCRIPTING						
					if (o->GetClassID() == ClassID(MonoBehaviour))
					{
						forceUnload = true;
						if (strcmp(o->GetName(), "GameSkin") != 0)
							AssertString(o->GetName());
					}
					
					if (o->GetClassID() == ClassID(MonoScript))
					{
						forceUnload = true;
					}
#endif					
					if (!forceUnload)
						continue;
				}
			}
			delete_object_internal (o);
		}
	}
	
	for (int i=ManagerContext::kManagerCount-1;i != 0;i--)
	{
		if (GetManagerContext().m_Managers[i])
		{
			GetPersistentManager().MakeObjectUnpersistent(GetManagerContext().m_Managers[i]->GetInstanceID(), kDontDestroyFromFile);
			delete_object_internal (GetManagerContext().m_Managers[i]);
			SetManagerPtrInContext (i, NULL);
		}
	}
	
	objects.clear();
	Object::FindAllDerivedObjects (ClassID (Object), &objects);
	for (int i=0;i<objects.size();i++)
	{
		Object* o = Object::IDToPointer(objects[i]);
		if (reloadable)
		{
			// Don't cleanup temporary objects (Except if its a gui skin from the builtin resources file)
			if (o != NULL && o->TestHideFlag(Object::kDontSave))
				continue;
		}
			
		delete_object_internal (o);
	}

	// Clear cached properties on all materials
	// This is because reloading the web player might delete some referenced textures (gui style default materials)
	// but not the material. The cache then gets out of sync and In that case, OpenGL will give an invalid error and text becomes black.
	// Thus we force reloading the textures from scratch.
	vector<Material*> materials;
	Object::FindObjectsOfType (&materials);
	for (int i=0;i<materials.size();i++)
	{
		materials[i]->ClearProperties();
	}
	
	#if CAPTURE_SCREENSHOT_AVAILABLE
	FinishAllCaptureScreenshot ();
	#endif
	#if SUPPORT_REPRODUCE_LOG
	PlayerCleanupReproduction();
	#endif
	
#if ENABLE_PROFILER && UNITY_EDITOR
	ProfilerHistory::Cleanup();
#endif	
	
	UnlockObjectCreation();
	
	CleanupBatchDelete ();
}

void CleanupEngine ()
{
	if (IsGfxDevice())
		GetGfxDevice().FinishRendering();
	TextMeshGenerator2::Flush();
	CleanupAllObjects(false);
	MessageIdentifier::Cleanup ();
	Object::CleanupAllClasses ();
	CleanupShaders ();
	RenderBufferManager::CleanupRenderBufferManager ();
	#if ENABLE_UNITYGUI
	CleanupGUIManager ();
	#endif
	#if ENABLE_MULTITHREADED_CODE || ENABLE_MULTITHREADED_SKINNING
	DestroyJobScheduler();
	#endif
	// on windows, device cleanup is done from separately (web player: called from separate thread)
	#if UNITY_OSX
	DestroyGfxDevice();
	#endif
	
	#if !UNITY_FLASH
	Cursors::CleanupCursors ();
	#endif

	ReleaseScreenManager();

	#if ENABLE_PLAYERCONNECTION && !UNITY_EDITOR
	InstallPlayerConnectionLogging(false);
	#endif
#if UNITY_EDITOR
	EditorAssetGarbageCollectManager::StaticDestroy();
	CleanupTypeTreeCache();
	CleanupMonoCompilationPipeline();
#endif
	ReleaseLogHandlers();
#if ENABLE_PROFILER
	CleanupMemoryProfilerStats();
#endif
}

void PostprocessSceneGenerateGUITextureAtlas ()
{
#if UNITY_EDITOR && ENABLE_RETAINEDGUI
	std::vector<Canvas*> canvas;
	Object::FindObjectsOfType(&canvas);

	for( unsigned i = 0 ; i < canvas.size() ; ++ i)
		BuildTextureAtlasForCanvas(canvas[i]);
#endif
}

#if ENABLE_EDITOR_HIERARCHY_ORDERING
void PostprocessSceneSortTransforms()
{
	const Transform::VisibleRootMap& rootMap = GetSceneTracker().GetVisibleRootTransforms();

	for (Transform::VisibleRootMap::const_iterator it = rootMap.begin(); it != rootMap.end(); ++it)
	{
		Transform* rootTrans = (*it).second;
		rootTrans->OrderChildrenRecursively();
	}
}
#endif

void PostprocessScene ()
{
#if UNITY_EDITOR
	// Disconnect all prefab instances.
	// This ensures that no prefab recording will happen which can be quite expensive.
	DisconnectAllPrefabInstances ();

	DestroyRenderersFromLODGroupInOpenScene(GetQualitySettings().GetStrippedMaximumLODLevel());
	ScriptingArguments no_arguments;
	CallMethodsWithAttribute(MONO_COMMON.postProcessSceneAttribute, no_arguments, NULL);
	PostprocessSceneGenerateGUITextureAtlas();
#endif

#if ENABLE_EDITOR_HIERARCHY_ORDERING
	PostprocessSceneSortTransforms();
#endif
}

void CleanupAfterLoad ()
{
	#if UNITY_EDITOR
	GetCleanupManager ().Flush ();
	RemoveDuplicateGameManagers ();
	#endif

	GarbageCollectSharedAssets (true);

	TextMeshGenerator2::Flush();
	GetRenderBufferManager().GarbageCollect(0);
	GetGfxDevice().InvalidateState();

	#if ENABLE_MONO
	mono_gc_collect (mono_gc_max_generation ());
	#endif

	ParticleSystem::BeginUpdateAll ();
	ParticleSystem::EndUpdateAll ();
	ParticleEmitter::UpdateAllParticleSystems();
	RenderManager::UpdateAllRenderers();

	// FIXME: Do this because we can't fully initialize the physicsmanager at startup.
	CALL_MANAGER_IF_EXISTS(ManagerContext::kPhysicsManager, AwakeFromLoad (kDefaultAwakeFromLoad))
	
	GetDelayedCallManager().Update(DelayedCallManager::kAfterLoadingCompleted);
	
	
	///@TODO: I am not sure why we do this...
	GetQualitySettings().ApplySettings();
}

void PatchRendererLightmapIndices (AwakeFromLoadQueue& awakeQueue)
{
	// offset the lightmap index in the newly loaded Renderers and Terrains by the current number of lightmaps
	int lightmapIndexOffset = GetLightmapSettings().GetLightmaps().size();
	if (lightmapIndexOffset == 0)
		return;

	AwakeFromLoadQueue::ItemArray& rendererItems = awakeQueue.GetItemArray(kGameObjectAndComponentQueue);
	for (int i = 0; i < rendererItems.size(); i++)
	{
		Object* object = Object::IDToPointer(rendererItems[i].objectPPtr.GetInstanceID());

		// renderers
		Renderer* r = dynamic_pptr_cast<Renderer*>(object);
		if (r != NULL)
		{
			if (r->IsLightmappedForRendering())
				r->SetLightmapIndexInt(r->GetLightmapIndexInt() + lightmapIndexOffset);
		}
	}
		
	#if ENABLE_TERRAIN
	PPtr<MonoScript> terrainScript = GetBuiltinResource<MonoScript>("Terrain");
	AwakeFromLoadQueue::ItemArray& monoBehaviourItems = awakeQueue.GetItemArray(kMonoBehaviourQueue);
	for (int i = 0; i < monoBehaviourItems.size(); i++)
	{
		Object* object = Object::IDToPointer(monoBehaviourItems[i].objectPPtr.GetInstanceID());

		// terrains
		MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*>(object);
		if (behaviour && behaviour->GetScript() == terrainScript)
		{
			MessageData data;
			data.SetData (lightmapIndexOffset, ClassID (int));
			SendMessageDirect(*behaviour, kShiftLightmapIndex, data);
		}
	}
	#endif
	
}

void MergeLightmapData(AwakeFromLoadQueue& awakeQueue)
{
	// We made sure the LightmapSettings object will get loaded along with the other stuff 
	// in PreloadLevelOperation::Perform(). Now let's update the lightmap indices of the 
	// loaded Renderers, append loaded lightmaps to the original lightmaps and destroy
	// the loaded LightmapSettings object.
	LightmapSettings* loadedLightmapSettings = NULL;

	AwakeFromLoadQueue::ItemArray& managerItems = awakeQueue.GetItemArray(kManagersQueue);
	for (int i = 0; i < managerItems.size(); i++)
	{
		loadedLightmapSettings = dynamic_instanceID_cast<LightmapSettings*> (managerItems[i].objectPPtr.GetInstanceID());
		if (loadedLightmapSettings)
			break;
	}
	if (loadedLightmapSettings == NULL)
		return;
	
	const std::vector<LightmapData>& loadedLightmapData = loadedLightmapSettings->GetLightmaps();

	// loaded level contains lightmaps, so merge them.
	if (loadedLightmapData.size() != 0)
	{
		int lightmapsMode = GetLightmapSettings ().GetLightmapsMode ();
		int loadedLightmapsMode = loadedLightmapSettings->GetLightmapsMode ();

		if (loadedLightmapsMode != lightmapsMode)
		{
			WarningString (Format("The loaded level has a different lightmaps mode than the current one. Current: %s. Loaded: %s. Will use: %s.",
				LightmapSettings::kLightmapsModeNames[lightmapsMode],
				LightmapSettings::kLightmapsModeNames[loadedLightmapsMode],
				LightmapSettings::kLightmapsModeNames[lightmapsMode]));
		}

		// first offset lightmap indices of the loaded renderers by the current number of lightmaps
		PatchRendererLightmapIndices(awakeQueue);
		// then add the new lightmaps at the end of the current lightmaps array
		GetLightmapSettings().AppendLightmaps(loadedLightmapData);
	}
	
	DestroyObjectHighLevel(loadedLightmapSettings);
}

void PostLoadLevelAdditive (const std::string& pathName, AwakeFromLoadQueue& awakeQueue)
{
	PostEditorLoadLevelAdditive(pathName, awakeQueue);
	
	PostprocessScene();
}

void PostEditorLoadLevelAdditive (const std::string& pathName, AwakeFromLoadQueue& awakeQueue)
{
	awakeQueue.RegisterObjectInstanceIDs();

	CompleteAwakeSequence(pathName, awakeQueue);
	
	MergeLightmapData(awakeQueue);
}

void VerifyNothingIsPersistentInLoadedScene (const std::string& pathName)
{
#if DEBUGMODE
	set<SInt32> persistentObjectsAtPath;
	GetPersistentManager().GetPersistentInstanceIDsAtPath(pathName, &persistentObjectsAtPath);
	for (set<SInt32>::iterator i=persistentObjectsAtPath.begin();i!=persistentObjectsAtPath.end();i++)
	{
		Object* target = Object::IDToPointer(*i);
		// Error on everything but global game managers. this handles the case where mainData contains both scene data and global game managers.
		if (target == NULL || !Object::IsDerivedFromClassID(target->GetClassID(), ClassID(GlobalGameManager)) || UNITY_EDITOR)
		{
			string className = target ? target->GetClassName() : "Not loaded";
			ErrorString("Failed to unpersist: " + className + " ID: " + IntToString(*i) + " FileID: " + IntToString(GetPersistentManager().GetLocalFileID(*i)));
		}
	}
	AssertIf(GetPersistentManager().IsStreamLoaded(pathName) && !GetPersistentManager().HasMemoryOrCachedSerializedFile(pathName));  
#endif
}

void ValidateNoSceneObjectsAreLoaded (bool includeAllEditorExtensions)
{
#if (UNITY_EDITOR && DEBUGMODE) || !UNITY_RELEASE
	// This happens when objects are accidentally loaded from disk again because someone still had a pointer to them.
	InstanceIDArray objects;
#if UNITY_EDITOR
	if (includeAllEditorExtensions)
		CollectAllSceneObjects (objects);
	else
#endif
		CollectSceneGameObjects (objects);
	if (!objects.empty ())
	{
		ErrorString ("Some objects were not cleaned up when closing the scene. (Did you spawn new GameObjects from OnDestroy?)");
	}

	vector<Object*> levelManagers;
	Object::FindObjectsOfType (ClassID (LevelGameManager), &levelManagers);
	for (int i=0;i<levelManagers.size();i++)
	{
		ErrorString (Format("Manager %s is still loading after clearing scene", levelManagers[i]->GetClassName().c_str()));
	}
#endif
}

void CompletePreloadMainData (AwakeFromLoadQueue& awakeQueue)
{
	ResetInput();
	
	// Cleanup exisiting level maangers
	DestroyLevelManagers ();
	
	awakeQueue.RegisterObjectInstanceIDs();

	// Load LevelManagers
	LoadManagers(awakeQueue);
	
	// Load everything else
	CompleteAwakeSequence(kMainData, awakeQueue);
	
	// FIXME: Do this because we can't fully initialize the physicsmanager at startup.
	CALL_MANAGER_IF_EXISTS(ManagerContext::kPhysicsManager, AwakeFromLoad (kDefaultAwakeFromLoad))
	
	GetDelayedCallManager().Update(DelayedCallManager::kAfterLoadingCompleted);

	GetQualitySettings().ApplySettings();
}

void CompletePreloadManagerLoadLevel (const std::string& path, AwakeFromLoadQueue& awakeQueue)
{
	ResetInput();

	awakeQueue.RegisterObjectInstanceIDs();

	LoadManagers(awakeQueue);
	
	CompleteAwakeSequence(path, awakeQueue);
	
	PostprocessScene();
	
	CleanupAfterLoad ();
}

static void CompleteAwakeSequence (const std::string& path, AwakeFromLoadQueue& awakeQueue)
{
#if UNITY_EDITOR
	// Merge all prefab instances (Requires that prefab backwards compatilibyt has been applied - AwakeFromLoad has not yet been called)
	MergeAllPrefabInstances(&awakeQueue);
#endif

	// Unload stream
	// - Don't unload if the stream came from an AssetBundle
	if ( !GetPersistentManager().HasMemoryOrCachedSerializedFile( path ) )
		GetPersistentManager().UnloadStream(path);
	
	// Invoke AwakeFromLoad and friends.
	GetPersistentManager().IntegrateAllThreadedObjectsStep2(awakeQueue);
	

	//@TODO: write a check that all objects in the AwakeQueue are !IsPersistent()
	
#if UNITY_EDITOR
	GetSceneTracker().ReloadTransformHierarchyRoots();
#endif
	
}

#if UNITY_EDITOR

void LoadLevelAdditiveEditor (const std::string& level)
{
	PreloadLevelOperation* op = PreloadLevelOperation::LoadLevel (level, "", -1, PreloadLevelOperation::kLoadEditorAdditiveLevel, true);
	
	GetPreloadManager().WaitForAllAsyncOperationsToComplete();

	op->Release();
}

void CompletePreloadManagerLoadLevelEditor (const std::string& path, AwakeFromLoadQueue& awakeQueue, int preloadOperationMode)
{
	Assert (!path.empty ());

	awakeQueue.RegisterObjectInstanceIDs();
	
	LoadManagers(awakeQueue);
	
	CompleteAwakeSequence(path, awakeQueue);
	
	if (preloadOperationMode == PreloadLevelOperation::kOpenSceneEditorPlaymode)
		PostprocessScene ();
	
	CleanupAfterLoad ();

	ResetManagersAfterLoadEditor ();

	if (preloadOperationMode == PreloadLevelOperation::kOpenSceneEditorPlaymode)
		PlayerInitState();
}

bool LoadSceneEditor (const string& pathName, std::map<LocalIdentifierInFileType, SInt32>* hintFileIDToHeapID, int options)
{
	AssertIf (pathName.empty ());

	bool enterPlaymode = options & kEditorPlayMode;
	PreloadLevelOperation::LoadingMode preloadOperationMode = enterPlaymode ? PreloadLevelOperation::kOpenSceneEditorPlaymode : PreloadLevelOperation::kOpenSceneEditor;

	// Make sure nothing is in the queue (We are calling DestroyWorld, then doing AsyncLoad)
	GetPreloadManager().WaitForAllAsyncOperationsToComplete();

	// Destroy world must be called before we start loading.
	// Otherwise objects will receive different instanceIDs in Playmode then they have in edit mode (hintFileIDToHeapID)
	DestroyWorld(true);
	EditorBeforeLoadingCleanup ();
	SetIsWorldPlaying (enterPlaymode);
	
	// Any left-over playmode load operations need to be cleared here.
	GetPreloadManager().RemoveStopPlaymodeOperations ();

	// Now load
	if (hintFileIDToHeapID)
		GetPersistentManager().SuggestFileIDToHeapIDs (pathName, *hintFileIDToHeapID);
	
	PreloadLevelOperation* op = PreloadLevelOperation::LoadLevel (pathName, "", -1, preloadOperationMode, true);

	GetPreloadManager().WaitForAllAsyncOperationsToComplete();

	op->Release();

	return true;
}
#endif


static void DestroyAllAtPath (const std::string& path)
{
#if DEBUGMODE
	GetPersistentManager().SetDebugAssertLoadingFromFile(path);
#endif
	PersistentManager::ObjectIDs ids;
	GetPersistentManager().GetLoadedInstanceIDsAtPath(path, &ids);
	
	LockObjectCreation();
	for (PersistentManager::ObjectIDs::iterator i=ids.begin();i!=ids.end();i++)
	{
		Object* o = Object::IDToPointer (*i);
		AssertIf(o != NULL && !o->IsPersistent());
		delete_object_internal (o);
	}
	UnlockObjectCreation();

#if DEBUGMODE
	ids.clear();
	GetPersistentManager().GetLoadedInstanceIDsAtPath(path, &ids);
	if (!ids.empty())
	{
		ErrorString("UnloadAssetBundle failed");
	}
	GetPersistentManager().SetDebugAssertLoadingFromFile("");
#endif
	
	GetPersistentManager().RemoveObjectsFromPath(path);
}

static bool DoesGfxDeviceRequireAssetsToBeReloadableFromDisk ()
{
// For Windows Phone 8 reloading may be required at any point in time (when user pauses - resumes app, for instance)
#if UNITY_WP8
	return true;
#else
	return false;
#endif
}

void UnloadAssetBundle (AssetBundle& file, bool unloadAllLoadedObjects)
{
	if (DoesGfxDeviceRequireAssetsToBeReloadableFromDisk () && !unloadAllLoadedObjects)
	{
		ErrorString("AssetBundle.Unload(false) shouldn't be called because used assets might have to be reloaded at any point in time. If no assets are used, call AssetBundle.Unload(true).");
		return;
	}

	GetPreloadManager().LockPreloading();
	
	PPtr<AssetBundle> resourceFilePPtr = &file;

	// Destroy all objects loaded from the AssetBundle path (or unassociate
	// them from the path if unloadAllLoadedObjects==false) and unload
	// all streams associated with the bundle.
	//
	// Note that DestroyAllAtPath() will actually delete the AssetBundle
	// instance itself so be careful not to reference any data from it
	// after the call.
	
	#if ENABLE_WWW
	UnityWebStream* stream = file.m_UnityWebStream;
	if (stream && stream->GetFileStream() && 
		(stream->GetFileStream()->GetType() == kCompressedFileStreamMemoryType ||
		 stream->GetFileStream()->GetType() == kUncompressedFileStreamMemoryType))
	{
		FileStream* compressedFile = stream->GetFileStream();
		FileStream::Decompressed files = compressedFile->m_Files;

		for (FileStream::iterator f=files.begin();f != files.end();f++)
		{
			if (unloadAllLoadedObjects)
				DestroyAllAtPath(f->name);
			else
				GetPersistentManager().RemoveObjectsFromPath(f->name);
		}
		
		DestroyWithoutLoadingButDontDestroyFromFile(resourceFilePPtr.GetInstanceID());

		for (FileStream::iterator i=files.begin();i != files.end();i++)
		{
			GetPersistentManager().UnloadStream(i->name);
		}
	}
	else
	#endif // ENABLE_WWW
	#if ENABLE_CACHING
	if (file.m_CachedUnityWebStream)
	{
		// Don't const reference files, as the changed is being changed in the loop
		vector<string> files = file.m_CachedUnityWebStream->m_Files;

		for (int i=0;i<files.size();i++)
		{
			const string& path = files[i];
			if (unloadAllLoadedObjects)
				DestroyAllAtPath(path);
			else
				GetPersistentManager().RemoveObjectsFromPath(path);
		}
		
		DestroyWithoutLoadingButDontDestroyFromFile(resourceFilePPtr.GetInstanceID());

		for (int i=0;i<files.size();i++)
		{
			GetPersistentManager().UnloadStream(files[i]);
		}
	}
	else
	#endif // ENABLE_CACHING
	if (file.m_UncompressedFileInfo)
	{
		AssetBundle::UncompressedFileInfoContainer* uncompFileInfo = file.m_UncompressedFileInfo;

		for (AssetBundle::UncompressedFileInfoContainer::iterator f = uncompFileInfo->begin();
			f != uncompFileInfo->end();
			f++)
		{
			const string& path = f->fileName;
			if (unloadAllLoadedObjects)
				DestroyAllAtPath(path);
			else
				GetPersistentManager().RemoveObjectsFromPath(path);
		}

		DestroyWithoutLoadingButDontDestroyFromFile(resourceFilePPtr.GetInstanceID());

		for (AssetBundle::UncompressedFileInfoContainer::iterator f = uncompFileInfo->begin();
			f != uncompFileInfo->end();
			f++)
		{
			GetPersistentManager().UnloadStream(f->fileName);
		}
		UNITY_DELETE(uncompFileInfo, kMemFile);
	}
	else
	{
		ErrorString("Resource file has already been unloaded.");
	}
	
	GetPreloadManager().UnlockPreloading();
}


#if UNITY_EDITOR


static void EditorBeforeLoadingCleanup ()
{
	GetDelayedCallManager ().ClearAll ();
}

static void ResetManagersAfterLoadEditor ()
{
	ResetInput();
	GetTimeManager ().ResetTime ();
	GetQualitySettings().ApplySettings();
}

void CreateWorldEditor ()
{
	// Create any managers that are not created yet and setup manager context
	AwakeFromLoadQueue emptyQueue (kMemTempAlloc);
	LoadManagers(emptyQueue);
	
	EditorBeforeLoadingCleanup ();
	
	GetSceneTracker().ReloadTransformHierarchyRoots();
	
	ResetManagersAfterLoadEditor ();
}
#endif


PROFILER_INFORMATION(gCollectSceneGameObjects, "CollectGameObjects", kProfilerLoading)

void CollectSceneGameObjects (InstanceIDArray& outputObjects)
{
	PROFILER_AUTO(gCollectSceneGameObjects,NULL);
	vector<GameObject*> gameObjects;
	Object::FindObjectsOfType (&gameObjects);
	for (vector<GameObject*>::iterator i= gameObjects.begin ();i != gameObjects.end ();++i)
	{
		GameObject& object = **i;

		// Verify that game objects dont accidentally end up being active and persistent
		#if DEBUGMODE
		if (object.IsActive() && object.IsPersistent())
		{
			ErrorStringObject("Persistent object inconsistency", &object);
		}
		#endif
		
		if (object.IsPersistent ())
			continue;
		if (object.TestHideFlag (Object::kDontSave))
			continue;
		
		#if UNITY_EDITOR
		if (object.IsPrefabParent ())
		{
			ErrorStringObject ("A prefab somehow lost its way out of its asset file. Ignoring it. Save your scene and next time you launch the editor it will be gone.", &object);
			continue;
		}
		#endif

		outputObjects.push_back(object.GetInstanceID ());
	}	
}
#if UNITY_EDITOR
static void CollectAllSceneObjects (InstanceIDArray& instanceIDs)
{
	vector<SInt32> objects;
	set<Prefab*> prefabInstances;
	Object::FindAllDerivedObjects (ClassID (EditorExtension), &objects);
	for (vector<SInt32>::iterator i= objects.begin ();i != objects.end ();++i)
	{
		EditorExtension& object = *PPtr<EditorExtension> (*i);
		
#if DEBUGMODE
		GameObject* go = dynamic_pptr_cast<GameObject*> (&object);
		if (go)
		{
			if (go->IsActive() && go->IsPersistent())
			{
				ErrorStringObject("Persistent object inconsistency", go);
			}
		}
#endif
		
		if (object.IsPersistent ())
			continue;
		
		if (object.IsPrefabParent())
		{
			ErrorStringObject ("A prefab somehow lost its way out of its asset file. Ignoring it. Save your scene and next time you launch the editor it will be gone.", &object);
			continue;
		}
		
		if (object.TestHideFlag (Object::kDontSave))
			continue;
		if (object.IsDerivedFrom (ClassID (GameManager)))
			continue;
		if (object.GetClassID () >= ClassID (SmallestEditorClassID))
			continue;
		
		instanceIDs.push_back(object.GetInstanceID ());
	}
}

#endif

