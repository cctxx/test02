#include "UnityPrefix.h"
#include "PreloadManager.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
///////// FUSIONFALL todo: Fix this include by moving it to player i guess
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Threads/ThreadUtility.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Misc/ReproductionLog.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Graphics/SubstanceSystem.h"
#include "Runtime/Misc/AssetBundleUtility.h"

#if UNITY_ANDROID
	#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#endif
#if UNITY_WII
#include "PlatformDependent/Wii/WiiLoadingScreen.h"
#endif


PROFILER_INFORMATION (gPreloadLevel, "Application.LoadLevelAsync Integrate", kProfilerLoading);
PROFILER_INFORMATION (gPreloadBundle, "AssetBundle.LoadAsync Integrate", kProfilerLoading);
PROFILER_INFORMATION (gIntegrateAssetsInBackground, "Application.Integrate Assets in Background", kProfilerLoading);
PROFILER_INFORMATION (gAsyncOperationComplete, "Application.WaitForAsyncOperationToComplete", kProfilerLoading);

#if THREADED_LOADING
	#define THREADED_LOADING_MUTEX_AUTOLOCK(x) Mutex::AutoLock lock (x)
#else
	#define THREADED_LOADING_MUTEX_AUTOLOCK(x) {}
#endif

#define PROFILE_PRELOAD_MANAGER 0
#if PROFILE_PRELOAD_MANAGER
#include "Runtime/Input/TimeManager.h"
#endif

static void GetFileIDsForLoadingScene (const string& pathName, PreloadLevelOperation::LoadingMode loadingMode, vector<LocalIdentifierInFileType>& fileIDs, vector<int>& managerIndices);
PreloadData::~PreloadData ()
{}

template<class TransferFunction>
void PreloadData::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_Assets, "m_Assets");
}

IMPLEMENT_OBJECT_SERIALIZE(PreloadData)
IMPLEMENT_CLASS(PreloadData)

PreloadManager* gPreloadManager = NULL;

PreloadManager& GetPreloadManager()
{
	if (gPreloadManager != NULL)
		return *gPreloadManager;
	else
	{
		gPreloadManager = new PreloadManager();
		return *gPreloadManager;
	}
}

void ReleasePreloadManager()
{
	delete gPreloadManager;
	gPreloadManager = NULL;
}

void StopPreloadManager()
{
	if (gPreloadManager)
		gPreloadManager->Stop();
}

PreloadManager::PreloadManager()
:	m_ProcessingOperation (NULL)
{
	m_IntegrationOperation = NULL;
#if ENABLE_MONO
	m_InitDomain = NULL;
#endif
#if THREADED_LOADING
	m_Thread.SetName ("UnityPreload");
#endif
}

PreloadManager::~PreloadManager()
{
	Stop();
}

void PreloadManager::SetThreadPriority(ThreadPriority p)
{
#if THREADED_LOADING
	m_Thread.SetPriority(p);
#endif
}

ThreadPriority PreloadManager::GetThreadPriority ()
{
#if THREADED_LOADING
	return m_Thread.GetPriority();
#else
	return kNormalPriority;
#endif
}

#if UNITY_EDITOR
void PreloadManager::RemoveStopPlaymodeOperations ()
{
	THREADED_LOADING_MUTEX_AUTOLOCK(m_QueueMutex);
	for (int i=0;i<m_PreloadQueue.size();i++)
	{
		PreloadManagerOperation* op = m_PreloadQueue[i];
		if (op && op->IsPlaymodeLoadLevel())
		{
			op->CleanupCoroutine();
			op->Release();
			m_PreloadQueue.erase(m_PreloadQueue.begin() + i);
			i--;
		}
	}
}
#endif

void PreloadManager::Stop ()
{
#if THREADED_LOADING
	m_Thread.SignalQuit();

	{
		PROFILER_AUTO(gIntegrateAssetsInBackground, NULL)
		// Wait until loading is complete, to make sure everything is integrated so that we don't leak anything.
		while (m_Thread.IsRunning())
		{
			UpdatePreloadingSingleStep(true);
			Thread::Sleep(0.01F);
		}
	}

	m_Thread.WaitForExit();
#endif

	InvokeCoroutineCallbacks ();

	AssertIf(!m_CallCoroutineCallbackQueue.empty());

	{
		THREADED_LOADING_MUTEX_AUTOLOCK(m_QueueMutex);

		for (int i=0;i<m_PreloadQueue.size();i++)
		{
			m_PreloadQueue[i]->CleanupCoroutine();
			m_PreloadQueue[i]->Release();
		}
		m_PreloadQueue.clear();
		m_CallCoroutineCallbackQueue.clear();
	}

	Assert(m_IntegrationOperation == NULL);
	Assert(m_ProcessingOperation == NULL);
	#if ENABLE_MONO
	m_InitDomain = NULL;
	#endif
}

#if THREADED_LOADING
void* PreloadManager::Run (void* managerPtr)
{
#if ENABLE_PROFILER
	profiler_initialize_thread ("PreloadManager Thread", true);
#endif

	PreloadManager& manager = *static_cast<PreloadManager*> (managerPtr);
	manager.Run();

#if ENABLE_PROFILER
	profiler_cleanup_thread();
#endif

	return NULL;
}

void PreloadManager::Run ()
{
	#if ENABLE_MONO
	mono_thread_attach(m_InitDomain);
	m_InitDomain = NULL;
	#endif

#if PROFILE_PRELOAD_MANAGER
	float startTime = GetTimeSinceStartup();
	#define LOG_PROFILER(x) printf_console(x, m_ProcessingOperation->GetDebugName().c_str(), (GetTimeSinceStartup() - startTime) * 1000.0F); startTime = GetTimeSinceStartup();
#else
	#define LOG_PROFILER(x)
#endif

	while (!m_Thread.IsQuitSignaled())
	{
		m_QueueMutex.Lock();

		if (!m_PreloadQueue.empty())
		{
			Assert(m_IntegrationOperation == NULL);

			// Find highest priority queued item
			int index = PreloadManager::FindTopPriorityOperation (m_PreloadQueue);

			// Grab item and remove it from queue
			Assert(m_ProcessingOperation == NULL);
			m_ProcessingOperation = m_PreloadQueue[index];

			LOG_PROFILER("Begin Background load: %s [idle time: %f]\n");

			m_PreloadQueue.erase(m_PreloadQueue.begin() + index);

			m_QueueMutex.Unlock();

			m_LoadingMutex.Lock();

			// Process it
			m_ProcessingOperation->Perform();

			LOG_PROFILER("Async background load complete: %s - %f ms\n");

			bool hasIntegrate = m_ProcessingOperation->HasIntegrateMainThread();

			// Integrate any work into main thread, wait for completion,
			// so that the persistentmanager is kept clear until the integration thread is done
			if (hasIntegrate)
			{
				AssertIf(m_IntegrationOperation != NULL);

				m_IntegrationOperation = m_ProcessingOperation;

				// This is needed in order to make sure that m_IntegrationOperation is properly synced when
				// the signaled thread reads the value (otherwise we get an assert - so this is actually to prevent the assert.
				UnityMemoryBarrier();

				// Temporarily release the loading mutex so that the main thread can do
				// LockPreloading() without deadlocking.
				m_LoadingMutex.Unlock ();

				m_IntegrationSemaphore.WaitForSignal();
				Assert (NULL == m_IntegrationOperation);

				// Re-acquire the loading mutex while we finish the current operation.
				m_LoadingMutex.Lock ();
			}

			m_QueueMutex.Lock();
			// Pass operation over into m_CallCoroutineCallbackQueue, which will finally release it
			m_CallCoroutineCallbackQueue.push_back(m_ProcessingOperation);
			//m_ProcessingOperation->Release();
			LOG_PROFILER("Completed Integration step: %s - %f ms\n");
			m_ProcessingOperation = NULL;

			m_QueueMutex.Unlock();

			m_LoadingMutex.Unlock();
		}
		else
		{
			m_QueueMutex.Unlock();
			Thread::Sleep(0.1F);
		}
	}
	#if ENABLE_MONO
	mono_thread_detach(mono_thread_current ());
	#endif

#undef PROFILE_PRELOAD_MANAGER
}

#endif

bool PreloadManager::IsLoading()
{
	ASSERT_RUNNING_ON_MAIN_THREAD
#if THREADED_LOADING
	return m_LoadingMutex.IsLocked();
#else
	return false;
#endif
}

bool PreloadManager::IsLoadingOrQueued()
{
	ASSERT_RUNNING_ON_MAIN_THREAD

	if (IsLoading())
		return true;

	THREADED_LOADING_MUTEX_AUTOLOCK(m_QueueMutex);
	if (m_ProcessingOperation)
		return true;
	return !m_PreloadQueue.empty();
}

size_t PreloadManager::FindTopPriorityOperation (std::vector<PreloadManagerOperation*>& ops)
{
	Assert (!ops.empty ());
	int index = 0;
	int highestPriority = ops[0]->GetPriority();
	for (int i=1;i<ops.size();i++)
	{
		if (ops[i]->GetPriority() > highestPriority)
		{
			index = i;
			highestPriority = ops[i]->GetPriority();
		}
	}

	return index;
}

#if !THREADED_LOADING
void PreloadManager::ProcessPreloadOperation ()
{

	#if PROFILE_PRELOAD_MANAGER
	float startTime = GetTimeSinceStartup();
	#define LOG_PROFILER(x) printf_console(x, m_ProcessingOperation->GetDebugName().c_str(), (GetTimeSinceStartup() - startTime) * 1000.0F); startTime = GetTimeSinceStartup();
	#else
	#define LOG_PROFILER(x)
	#endif

	if (!m_PreloadQueue.empty())
	{
		Assert (m_IntegrationOperation == NULL);

		int index = PreloadManager::FindTopPriorityOperation (m_PreloadQueue);

		// Grab item and remove it from queue
		Assert(m_ProcessingOperation == NULL);
		m_ProcessingOperation = m_PreloadQueue[index];

		LOG_PROFILER("Begin background load (FAKE): %s [idle time: %f]\n");

		m_PreloadQueue.erase(m_PreloadQueue.begin() + index);
		m_ProcessingOperation->Perform();

		LOG_PROFILER("Async background load complete (FAKE): %s - %f ms\n");

		// Integrate any work into main thread, wait for completion,
		// so that the persistentmanager is kept clear until the integration thread is done
		if (m_ProcessingOperation->HasIntegrateMainThread())
		{
			Assert (m_IntegrationOperation == NULL);
			m_IntegrationOperation = m_ProcessingOperation;
		}

		m_CallCoroutineCallbackQueue.push_back(m_ProcessingOperation);

		LOG_PROFILER("Completed Integration step: %s - %f ms\n");

		m_ProcessingOperation = NULL;
	}

	#undef PROFILE_PRELOAD_MANAGER
}

void PreloadManager::UpdatePreloadingSingleStep (bool stopPreloading)
{
	// Do loading
	ProcessPreloadOperation ();

	// Upload texture data immediately after texture asset was loaded
	// On some platforms helps to avoid memory peak during level load
	Texture2D::IntegrateLoadedImmediately();

	// Integrate threaded objects for some time
	GetPersistentManager().IntegrateThreadedObjects (20.0F);

	// Perform final main thread integration step
	if (m_IntegrationOperation != NULL && (m_IntegrationOperation->GetAllowSceneActivation() || stopPreloading))
	{
		PreloadManagerOperation* operation = m_IntegrationOperation;
		m_IntegrationOperation = NULL;
		operation->IntegrateMainThread();
	}

	// Potentially call out coroutines
	InvokeCoroutineCallbacks();
}

#else

void PreloadManager::UpdatePreloadingSingleStep (bool stopPreloading)
{
	// Should we start up the preload manager thread? Do it as soon as something needs to be processed
	if (!m_Thread.IsRunning())
	{
		////@TODO: Also use stopPreloading here...
		if (!m_PreloadQueue.empty())
		{
			AssertIf(m_IntegrationOperation);
			#if ENABLE_MONO
			Assert(m_InitDomain == NULL);
			m_InitDomain = mono_domain_get();
			#endif
#if UNITY_WII
			// On Wii if we set thread priority below normal and don't call Sleep on MainThread, the child thread will never run!
			// Update : Threaded loading actually works now on Wii, but it requires that allocators should be thread safe, and
			// it's not the case on Wii, so if there will be plans to make threaded loading work, allocators should be made thread-safe
			m_Thread.SetPriority(kNormalPriority);
#else
			m_Thread.SetPriority(kBelowNormalPriority);
#endif

			////@TODO: This is currently a very high value because MonoBehaviours might do funky recursion!
			// Reduce stacksize sensibly when we stop the madness.

			unsigned stackSize = 512 * 1024;
			if (UNITY_EDITOR)
				stackSize = 2 * 1024 * 1024;

#if UNITY_XENON
			const int kProcessor = 4;
#else
			const int kProcessor = 1;
#endif
			m_Thread.Run(&PreloadManager::Run, this, stackSize, kProcessor);
		}
	}

	// Upload texture data immediately after texture asset was loaded
	// On some platforms helps to avoid memory peak during level load
	Texture2D::IntegrateLoadedImmediately();

	// Update substance integration, since we are not in the gameloop here
	#if ENABLE_SUBSTANCE
	SubstanceSystem* system = GetSubstanceSystemPtr ();
	if (system != NULL && !system->AreIntegratingQueuesEmpty())
		system->Update(stopPreloading);
	#endif

	// Figure out sensible time for how long we are allowed to integrate assets on the main thread each frame
	float ms = 4.0F;
	ThreadPriority p = GetThreadPriority();
	if (p == kLowPriority)
		ms = 2.0F;
	else if (p == kBelowNormalPriority)
		ms = 4.0F;
	else if (p == kNormalPriority)
		ms = 10.0F;
	else if (p == kHighPriority)
		ms = 50.0F;

	// Integrate threaded objects for some time
	GetPersistentManager().IntegrateThreadedObjects(ms / 1000.0F);

	// Perform final main thread integration step
	if (m_IntegrationOperation != NULL && (m_IntegrationOperation->GetAllowSceneActivation() || stopPreloading))
	{
		PreloadManagerOperation* operation = m_IntegrationOperation;
		m_IntegrationOperation = NULL;
		operation->IntegrateMainThread();

		// This is needed in order to make sure that m_IntegrationOperation is properly synced when
		// the signaled thread reads the value
		UnityMemoryBarrier();
		m_IntegrationSemaphore.Signal();
	}

	// Potentially call out coroutines
	InvokeCoroutineCallbacks();
}
#endif

void PreloadManager::InvokeCoroutineCallbacks ()
{
	// Grab coroutines that need to be invoked this frame
	// And invoke them
	std::vector<PreloadManagerOperation*> callbacks;

	{
		THREADED_LOADING_MUTEX_AUTOLOCK(m_QueueMutex);
		callbacks.swap(m_CallCoroutineCallbackQueue);
	}

	for (int i=0;i<callbacks.size();i++)
	{
		callbacks[i]->InvokeCoroutine();
		callbacks[i]->Release();
	}
}


void PreloadManager::AddToQueue (PreloadManagerOperation* operation)
{
	THREADED_LOADING_MUTEX_AUTOLOCK(m_QueueMutex);
	operation->Retain();
	m_PreloadQueue.push_back(operation);
}

void PreloadManager::LockPreloading ()
{
	#if THREADED_LOADING
	PROFILER_AUTO(gIntegrateAssetsInBackground, NULL)
	while (!m_LoadingMutex.TryLock())
	{
		UpdatePreloadingSingleStep(false);
		Thread::Sleep(0.004F);
	}
	#endif
}

void PreloadManager::UnlockPreloading ()
{
#if THREADED_LOADING
	m_LoadingMutex.Unlock();
#endif
}

void PreloadManager::WaitForAllAsyncOperationsToComplete()
{
	PROFILER_AUTO(gAsyncOperationComplete, NULL)

	while(IsLoadingOrQueued())
	{
		UpdatePreloadingSingleStep(false);

		#if THREADED_LOADING
		if (!GetPersistentManager ().HasThreadedObjectsToIntegrate ())
			LevelLoadingLoop();
		#endif
	}
}

void PreloadManager::UpdatePreloading()
{
	// Check if there are any operation that must be completed right away
	bool mustCompleteNow = false;
	{
		THREADED_LOADING_MUTEX_AUTOLOCK(m_QueueMutex);
		for (int i=0;i<m_PreloadQueue.size();i++)
			mustCompleteNow |= m_PreloadQueue[i]->MustCompleteNextFrame();
		if (m_ProcessingOperation != NULL)
			mustCompleteNow |= m_ProcessingOperation->MustCompleteNextFrame();
	}

	if (mustCompleteNow)
	{
		WaitForAllAsyncOperationsToComplete ();
#if UNITY_ANDROID
		StopActivityIndicator();
#endif
	}
	else
	{
		PROFILER_AUTO(gIntegrateAssetsInBackground, NULL)
		UpdatePreloadingSingleStep (false);
	}
}


float PreloadManagerOperation::GetProgress ()
{
	return m_Progress;
}

bool PreloadManagerOperation::IsDone ()
{
	return m_Complete;
}

void PreloadLevelOperation::Perform ()
{
	// Should indicate threaded loading
	//GetGlobalAllocators().ActivateLoadingAllocator();

	PersistentManager& pm = GetPersistentManager();

#if UNITY_WII
	wii::StartLoadingScreen();
#endif

#if ENABLE_SUBSTANCE
	// Notify for preload operation
	SubstanceSystem* system = GetSubstanceSystemPtr ();
	if (system != NULL)
		system->BeginPreloading();
#endif

	// When loading a level, grab the PreloadData and extract PPtrs of all assets not contained in the primary assets file
	if (!m_LevelAssetDataPath.empty())
	{
		int preloadInstanceID = pm.GetInstanceIDFromPathAndFileID(m_LevelAssetDataPath, 1);
		// @TODO: This should be moved to main thread or the preload data should always be killed immediately
		Object* obj = Object::IDToPointerThreadSafe (preloadInstanceID);
		if (obj == NULL)
			obj = pm.ReadObjectThreaded(preloadInstanceID);

		PreloadData* preload = dynamic_pptr_cast<PreloadData*> (obj);
		if (preload)
		{
			int size = preload->m_Assets.size();
			m_PreloadAssets.resize_uninitialized(size);
			if (size > 0)
			{
				SInt32* target = &m_PreloadAssets[0];
				PPtr<Object>* source = &preload->m_Assets[0];
				for (int i=0;i<size;i++)
					target[i] = source[i].GetInstanceID();
			}
		}
		else
		{
			// In The Editor we load scenes that contain no preload data
			// In the web player Unity 3.3 had a bug where preloaddata was not included in the build.
			if (!WEBPLUG && !UNITY_EDITOR && m_LoadMode != kLoadAssetBundle)
			{
				AssertString("PreloadData is missing. It should always be there.");
			}
		}
	}

//	printf_console("Number of Preload assets %d\n", m_PreloadAssets.size());
//	for (int i=0;i<m_PreloadAssets.size();i++)
//	{
//		printf_console("Will load Preload assets %d %s\n", m_PreloadAssets[i], GetPersistentManager().GetPathName(m_PreloadAssets[i]).c_str());
//	}

	// Figure out exactly which objects in the file we need to load
	// We want to know what we're loading before preloading to indicate the progress properly
	vector<LocalIdentifierInFileType> fileIDs;
	vector<int> managerIndices;
	if (!m_LevelPath.empty())
		GetFileIDsForLoadingScene(m_LevelPath, m_LoadMode, fileIDs, managerIndices);

	LoadProgress loadProgress (fileIDs.size () + m_PreloadAssets.size (), 0.9f, &m_Progress);
	// Preload bundle assets & preload external level assets
	GetPersistentManager().LoadObjectsThreaded(m_PreloadAssets.begin(), m_PreloadAssets.size(), &loadProgress);

	// Load Level
	if (!m_LevelPath.empty())
	{
		/// Load all assets
		pm.LoadFileCompletelyThreaded(m_LevelAssetDataPath, NULL, NULL, -1, false, &loadProgress);

		// Wait for all assets to be integrated using timeslicing from the main thread
		pm.AllowIntegrationWithTimeoutAndWait ();


		///@TODO: these two are not needed anymore...
		std::vector<SInt32>	instanceIDs;
		instanceIDs.resize(fileIDs.size());

		// Lock up to extracting all objects into the AwakeFromLoadQueue
		// This prevents the main thread from loading and integrating random objects during the level load
		pm.Lock();

		Assert(!pm.HasThreadedObjectsToIntegrate());

		// Load level
		pm.LoadFileCompletelyThreaded(m_LevelPath, &fileIDs[0], &instanceIDs[0], fileIDs.size(), true, &loadProgress);

		pm.PrepareAllThreadedObjectsStep1 (m_AwakeFromLoadQueue);

		// Unlock - See above
		pm.Unlock();


		#if ENABLE_SUBSTANCE
		// Wait Substance finish to integrate
		SubstanceSystem* system = GetSubstanceSystemPtr ();
		if (system != NULL)
			system->WaitFinished(&loadProgress);
		#endif

		m_Progress = 0.9F;
	}
	else
	{
		// Wait for all assets to be integrated using timeslicing from the main thread
		pm.AllowIntegrationWithTimeoutAndWait ();

		#if ENABLE_SUBSTANCE
		// Wait Substance finish to integrate
		SubstanceSystem* system = GetSubstanceSystemPtr ();
		if (system != NULL)
			system->WaitFinished(&loadProgress);
		#endif

		m_Progress = 1.0F;
		UnityMemoryBarrier();
		m_Complete = true;
	}
#if UNITY_WII
	wii::EndLoadingScreen();
#endif
}

void PreloadLevelOperation::PreloadBundleSync (AssetBundle& bundle, const std::string& name)
{
	// Calculate preload index & size into preload table of asset bundle
	int index = 0, size = 0;
	if (name.empty())
	{
		index = bundle.m_MainAsset.preloadIndex;
		size = bundle.m_MainAsset.preloadSize;
		if (Object::IDToPointer(bundle.m_MainAsset.asset.GetInstanceID()))
			return;
	}
	else
	{
		AssetBundle::range range = bundle.GetPathRange(name);
		if (range.first != range.second)
		{
			index = range.first->second.preloadIndex;
			size = range.first->second.preloadSize;
			if (Object::IDToPointer(range.first->second.asset.GetInstanceID()))
				return;
		}
	}

	if (size == 0)
		return;

	PPtr<Object>* source = &bundle.m_PreloadTable[index];
	for (int i=0;i<size;i++)
	{
		Object* forceLoad = source[i];
		UNUSED(forceLoad);
	}
}

bool PreloadLevelOperation::GetAllowSceneActivation ()
{
	return m_AllowSceneActivation;
}

void PreloadLevelOperation::SetAllowSceneActivation (bool allow)
{
	m_AllowSceneActivation = allow;
}

void PreloadLevelOperation::CleanupMemory()
{
	m_PreloadAssets.clear();
	m_AwakeFromLoadQueue.Clear();
}


bool PreloadLevelOperation::HasIntegrateMainThread ()
{
	return !m_LevelPath.empty();
}

void PreloadLevelOperation::IntegrateMainThread ()
{
	Texture2D::IntegrateLoadedImmediately();

	if (!m_LevelPath.empty())
	{
		PROFILER_AUTO(gPreloadLevel, NULL)
		if (m_LoadMode == kLoadAdditiveLevel)
		{
			PostLoadLevelAdditive (m_LevelPath, m_AwakeFromLoadQueue);
		}
		else if (m_LoadMode == kLoadEditorAdditiveLevel)
		{
			PostEditorLoadLevelAdditive (m_LevelPath, m_AwakeFromLoadQueue);
		}
		else if (m_LoadMode == kLoadLevel)
		{
			PROFILER_AUTO(gPreloadLevel, NULL)

			PlayerLoadLevelFromThread(m_LoadLevelIndex, m_LevelPath, m_AwakeFromLoadQueue);
		}
		else if (m_LoadMode == kLoadMainData)
		{
			PROFILER_AUTO(gPreloadLevel, NULL)
			CompletePreloadMainData (m_AwakeFromLoadQueue);
		}
		#if UNITY_EDITOR
		else if (m_LoadMode == kOpenSceneEditor || m_LoadMode == kOpenSceneEditorPlaymode)
		{
			PROFILER_AUTO(gPreloadLevel, NULL)
			CompletePreloadManagerLoadLevelEditor (m_LevelPath, m_AwakeFromLoadQueue, m_LoadMode);
		}
		#endif
		else
		{
			AssertString("Cant reach");
		}

		VerifyNothingIsPersistentInLoadedScene(m_LevelPath);
	}
	else
	{
		PROFILER_AUTO(gPreloadBundle, NULL)
		GetPersistentManager().IntegrateAllThreadedObjects();
	}

	CleanupMemory();

	m_Progress = 1.0F;
	UnityMemoryBarrier();
	m_Complete = true;

	if (RunningReproduction())
	{
		LogString(Format("Completed loading level: '%s' (time: %f, frame: %d)", m_LevelPath.c_str(), GetTimeManager().GetCurTime(), GetTimeManager().GetFrameCount()));
	}
}

PreloadLevelOperation* PreloadLevelOperation::LoadLevel (const std::string& levelPath, const std::string& levelAssetPath, int levelIndex, LoadingMode  mode, bool mustCompleteNextFrame)
{
	if (!mustCompleteNextFrame && !GetBuildSettings().hasPROVersion)
	{
		ErrorString("Asynchronous Background loading is only supported in Unity Pro.\nPlease use Application.LoadLevel or Application.LoadLevelAdditive instead.");
		mustCompleteNextFrame = true;
	}

	PreloadLevelOperation* operation = new PreloadLevelOperation ();
	operation->m_LevelPath = levelPath;
	operation->m_LevelAssetDataPath = levelAssetPath;
	operation->m_LoadMode = mode;
	operation->m_LoadLevelIndex = levelIndex;
	operation->m_MustCompleteNextFrame = mustCompleteNextFrame;
	#if ENABLE_PROFILER || PROFILE_PRELOAD_MANAGER
	operation->m_DebugName = "Loading " + levelPath;
	#endif
	GetPreloadManager().AddToQueue(operation);

	return operation;
}

UnloadUnusedAssetsOperation* UnloadUnusedAssetsOperation::UnloadUnusedAssets ()
{
	UnloadUnusedAssetsOperation* operation = new UnloadUnusedAssetsOperation ();
	GetPreloadManager().AddToQueue(operation);

	return operation;
}

void UnloadUnusedAssetsOperation::IntegrateMainThread ()
{
	GarbageCollectSharedAssets(true);
	m_Progress = 1.0F;
	UnityMemoryBarrier();
	m_Complete = true;
}

PreloadLevelOperation* PreloadLevelOperation::CreateDummy ()
{
	PreloadLevelOperation* operation = new PreloadLevelOperation ();
	operation->m_Progress =  1.0F;
	UnityMemoryBarrier();
	operation->m_Complete = true;
	return operation;
}

/// @TODO: one string can identify multiple different resources.
/// We must somehow preload all or make preload tables be shared by name
PreloadLevelOperation* PreloadLevelOperation::LoadAssetBundle (AssetBundle& bundle, const std::string& name)
{
	if (!GetBuildSettings().hasPROVersion)
	{
		ErrorString("Asynchronous Background loading is only supported in Unity Pro.\nPlease use AssetBundle.Load instead");
		return CreateDummy ();
	}

	PreloadLevelOperation* operation = NULL;

	// Calculate preload index & size into preload table of asset bundle
	int index = 0, size = 0;
	if (name.empty())
	{
		index = bundle.m_MainAsset.preloadIndex;
		size = bundle.m_MainAsset.preloadSize;
		if (Object::IDToPointer(bundle.m_MainAsset.asset.GetInstanceID()))
			return CreateDummy ();
	}
	else
	{
		AssetBundle::range range = bundle.GetPathRange(name);
		if (range.first != range.second)
		{
			index = range.first->second.preloadIndex;
			size = range.first->second.preloadSize;
			if (Object::IDToPointer(range.first->second.asset.GetInstanceID()))
				return CreateDummy ();
		}
	}

	if (size == 0)
		return CreateDummy ();

	operation = new PreloadLevelOperation ();
	#if ENABLE_PROFILER || PROFILE_PRELOAD_MANAGER
	operation->m_DebugName = "Loading asset bundle asset: " + name;
	#endif

	operation->m_PreloadAssets.resize_uninitialized(size);
	operation->m_LoadMode = kLoadAssetBundle;

	SInt32* target = &operation->m_PreloadAssets[0];
	PPtr<Object>* source = &bundle.m_PreloadTable[index];
	for (int i=0;i<size;i++)
		target[i] = source[i].GetInstanceID();

	GetPreloadManager().AddToQueue(operation);

	return operation;
}

PreloadLevelOperation::PreloadLevelOperation ()
	: PreloadManagerOperation (),
	m_AwakeFromLoadQueue(kMemSerialization)
{
	m_LoadLevelIndex = -1;
	m_MustCompleteNextFrame = false;
	m_AllowSceneActivation = true;
}


PreloadLevelOperation::~PreloadLevelOperation ()
{
}

static void GetFileIDsForLoadingScene (const string& pathName, PreloadLevelOperation::LoadingMode operation, vector<LocalIdentifierInFileType>& fileIDs, vector<int>& managerIndices)
{
	GetPersistentManager ().Lock();
	SerializedFile* stream = GetPersistentManager ().GetSerializedFileInternal(pathName);
	if (stream == NULL)
	{
		GetPersistentManager ().Unlock();
		return;
	}

	vector<LocalIdentifierInFileType> sourceFileIDs;
	stream->GetAllFileIDs(&sourceFileIDs);
	fileIDs.reserve(sourceFileIDs.size());

	#if UNITY_EDITOR
	int editorExtensionImplClassID = Object::StringToClassID("EditorExtensionImpl");
	#endif


	// GameManager no longer references EditorExtensionImpl. We need to not load EditorExtensionImpl related to game managers.
	// But we do need to load EditorExtensionImpl of game objects & components in order to grab deprecated data describing prefabs from it.
	// If we dont they might in turn load the manager, which we need very tight control over when we want them to be loaded.
	// Sometime in the past EditorExtensionImpl were referenced by GameManager.
	// So we skip over loading any editor extension impl objects before any scene objects are being loaded.
	// Loading the editorextension impl of a manager is dangerous because it will load it by dereferencing it's m_Object reference.
	// Don't load any editor extension impl for any managers
	// All editor extension impl's in scenes are always serialized directly after their related object.
	int baseSceneDataIndex = 0;
	for (int i=0;i<sourceFileIDs.size ();i++)
	{
		LocalIdentifierInFileType fileID = sourceFileIDs[i];
		int classID = stream->GetClassID (fileID);

		if (Object::IsDerivedFromClassID (classID, ClassID (GameManager)))
			baseSceneDataIndex = i + 1;
		#if UNITY_EDITOR
		else if (classID == editorExtensionImplClassID)
			continue;
		#endif
		else
			break;
	}

	for (int i=0;i<sourceFileIDs.size ();i++)
	{
		LocalIdentifierInFileType fileID = sourceFileIDs[i];
		int classID = stream->GetClassID (fileID);

		if (Object::IsDerivedFromClassID (classID, ClassID (GlobalGameManager)))
			continue;

		if (Object::IsDerivedFromClassID (classID, ClassID (LevelGameManager)))
		{
			// Additive loaded levels need lightmap settings from the scene file. It will be merged into the active scene.
			bool loadAdditive = operation == PreloadLevelOperation::kLoadEditorAdditiveLevel || operation == PreloadLevelOperation::kLoadAdditiveLevel;
			if (loadAdditive && classID != ClassID(LightmapSettings))
				continue;

			managerIndices.push_back(fileIDs.size());
		}

		#if UNITY_EDITOR
		if (classID == editorExtensionImplClassID && i < baseSceneDataIndex)
			continue;
		#endif

		fileIDs.push_back(fileID);
	}

	GetPersistentManager ().Unlock();
}


#undef THREADED_LOADING_MUTEX_AUTOLOCK // Mutex::AutoLock lock (x) / {}

void UnloadUnusedAssetsImmediate (bool includeMonoReferencesAsRoots)
{
	GetPreloadManager().LockPreloading();
	GarbageCollectSharedAssets(includeMonoReferencesAsRoots);
	GetPreloadManager().UnlockPreloading();
}
