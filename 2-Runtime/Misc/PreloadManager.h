#pragma once

#include "Configuration/UnityConfigure.h"
#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "ResourceManager.h"
#include "Runtime/Threads/Semaphore.h"
#include "AsyncOperation.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"

struct MonoDomain;
class PreloadManagerOperation;
class AssetBundle;


class PreloadData : public NamedObject
{
	public:
	
	DECLARE_OBJECT_SERIALIZE (PreloadData)
	REGISTER_DERIVED_CLASS (PreloadData, NamedObject)

	PreloadData (MemLabelId label, ObjectCreationMode mode) : Super(label, mode) {  } 
	// ~PreloadData (); declared-by-macro

	virtual bool ShouldIgnoreInGarbageDependencyTracking () { return true; }
	
	std::vector<PPtr<Object> > m_Assets;
};

class PreloadManager
{
	#if THREADED_LOADING
	Thread     m_Thread;
	Semaphore  m_IntegrationSemaphore;
	Mutex      m_QueueMutex;
	Mutex      m_LoadingMutex;
	#endif
	
	std::vector<PreloadManagerOperation*> m_PreloadQueue;
	std::vector<PreloadManagerOperation*> m_CallCoroutineCallbackQueue;
	PreloadManagerOperation* m_ProcessingOperation;
	
	PreloadManagerOperation* volatile m_IntegrationOperation;
	#if ENABLE_MONO
	MonoDomain* m_InitDomain;
	#endif
	
	static void* Run (void* managerPtr);
	void Run ();
	void ProcessPreloadOperation ();
	void InvokeCoroutineCallbacks ();
	
	static size_t FindTopPriorityOperation (std::vector<PreloadManagerOperation*>& ops);
	
	public:
	
	PreloadManager();
	~PreloadManager();
	
	bool IsLoading();
	bool IsLoadingOrQueued();
	bool IsLoadingOrQueuedLevel();
	
	void AddToQueue (PreloadManagerOperation* operation);
	
	void LockPreloading ();
	void UnlockPreloading ();
	
	void UpdatePreloading ();
	
	void SetThreadPriority(ThreadPriority p);
	ThreadPriority GetThreadPriority ();

#if UNITY_EDITOR
	void RemoveStopPlaymodeOperations ();
#endif

	void WaitForAllAsyncOperationsToComplete();

	void Stop();	

private:

	void UpdatePreloadingSingleStep (bool stopPreloadManager);
};

PreloadManager& GetPreloadManager();
void ReleasePreloadManager();
void StopPreloadManager();

class PreloadManagerOperation : public AsyncOperation
{
	protected:
	int		m_Priority;
	bool	m_Complete;
	float	m_Progress;

	PreloadManagerOperation () { m_Priority = 0; m_Complete = false; m_Progress = 0.0F; }
	
	public:

	virtual int GetPriority () { return m_Priority; }
	virtual void SetPriority (int priority) { m_Priority = priority; }
	
	virtual float GetProgress ();

	/// Returns true when the operation has completed.
	/// Subclasses must set m_Complete to true from either Perform or IntegrateMainThread.
	virtual bool IsDone ();
	
	/// Performs the actual work on the preload manager thread
	virtual void Perform () = 0;

	/// When complete gives the operation a chance to integrate some work on the main thread,
	/// without any other preload operation running at the same time
	virtual void IntegrateMainThread () {  }

	/// Override this and return true if you want IntegrateMainThread to be called.
	virtual bool HasIntegrateMainThread () { return false; }

	/// Do we have to complete the preload operation by the next frame?
	virtual bool MustCompleteNextFrame () { return false; }

#if UNITY_EDITOR
	virtual bool IsPlaymodeLoadLevel () { return false; }
#endif	
	
#if ENABLE_PROFILER
	virtual std::string GetDebugName () = 0;
#endif
};

///@TODO: We should seperate SceneLoading from assetbundle loading code here. Make it into two AsyncOperation classes.


class PreloadLevelOperation : public PreloadManagerOperation
{
	public:
	
	static void PreloadBundleSync (AssetBundle& bundle, const std::string& name);

	static PreloadLevelOperation* LoadAssetBundle (AssetBundle& bundle, const std::string& name);
	
	enum LoadingMode { kLoadLevel = 0, kLoadAdditiveLevel = 1, kLoadMainData = 2, kLoadAssetBundle = 3, kOpenSceneEditor = 3, kOpenSceneEditorPlaymode = 4, kLoadEditorAdditiveLevel = 5 };
	static PreloadLevelOperation* LoadLevel (const std::string& levelPath, const std::string& levelAssetPath, int levelIndex, LoadingMode mode, bool mustCompleteNextFrame);

	
	virtual void IntegrateMainThread ();
	virtual bool HasIntegrateMainThread ();

	virtual bool GetAllowSceneActivation ();
	virtual void SetAllowSceneActivation (bool allow);
	
	virtual void Perform ();
	
	virtual bool MustCompleteNextFrame () { return m_MustCompleteNextFrame; }
	
#if ENABLE_PROFILER
	virtual std::string GetDebugName () { return m_DebugName; }
#endif

#if UNITY_EDITOR
	virtual bool IsPlaymodeLoadLevel () { return m_LoadMode == kLoadLevel || m_LoadMode == kLoadAdditiveLevel; }
#endif	

private:
	bool                m_AllowSceneActivation;
	dynamic_array<SInt32> m_PreloadAssets;	
	int                 m_LoadLevelIndex;
	std::string			m_LevelPath;
	std::string			m_LevelAssetDataPath;
	LoadingMode         m_LoadMode;
	bool                m_MustCompleteNextFrame;

	AwakeFromLoadQueue  m_AwakeFromLoadQueue;
	
#if ENABLE_PROFILER
	std::string			m_DebugName;
#endif	
	
	void CleanupMemory();
	
	PreloadLevelOperation ();
	virtual ~PreloadLevelOperation ();

	static PreloadLevelOperation* CreateDummy ();
};

class UnloadUnusedAssetsOperation : public PreloadManagerOperation
{	
	UnloadUnusedAssetsOperation () : PreloadManagerOperation () {  }
	
	public:

	static UnloadUnusedAssetsOperation* UnloadUnusedAssets ();
	
	virtual void IntegrateMainThread ();
	virtual bool HasIntegrateMainThread () {return true;}

	virtual void Perform () {}
	
#if ENABLE_PROFILER
	virtual std::string GetDebugName () { return "Garbage Collect Assets"; }
#endif
};

void UnloadUnusedAssetsImmediate (bool managedObjects);
