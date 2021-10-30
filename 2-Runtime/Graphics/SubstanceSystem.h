#pragma once

#include "ProceduralMaterial.h"
#include "SubstanceArchive.h"
#include "Texture2D.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Semaphore.h"
#include <queue>
#include <algorithm>

#if ENABLE_SUBSTANCE
#include "External/Allegorithmic/builds/Engines/include/substance/handle.h"
#include "External/Allegorithmic/builds/Engines/include/substance/device.h"

class LoadProgress;

class SubstanceSystem
{
	SubstanceDevice m_Device;
	SubstanceContext* m_Context;
	
public:	
	
	SubstanceSystem ();
	~SubstanceSystem ();

	SubstanceContext* GetContext () { return m_Context; }
	
	// Set the callback handling generated texture
	// The default substance system is used when set to NULL
	void SetOutputCallback(void* callback=NULL);

	// State queries
	bool AreQueuesEmpty();
	bool AreIntegratingQueuesEmpty();
	bool IsSubstanceProcessing(const ProceduralMaterial* substance);

	// State wait
	void WaitFinished(LoadProgress* progress = NULL);
	void WaitFinished(const ProceduralMaterial* substance);

	// Update substance system, called from the main thread
	void Update(bool isQuitSignaled=false);

	// Update substance system, called from the substance thread
	// . generate queued substances
	// . integrate substances
	void UpdateThreaded();

	// Queued updates coming from ProceduralMaterial
	void QueueInput(ProceduralMaterial* material, std::string inputName, SubstanceValue& inputValue);
	void QueueSubstance(ProceduralMaterial* material);
	void QueueLoading(ProceduralMaterial* material);

	void QueryClearCache(ProceduralMaterial* material);

	static void * ThreadMain(void*data);
	
	// Substance callbacks
	static void* SUBSTANCE_CALLBACK OnMalloc(size_t bytesCount, size_t alignment=16);
	static void SUBSTANCE_CALLBACK OnFree(void* bufferPtr);
	static void SUBSTANCE_CALLBACK OnOutputCompleted(SubstanceHandle* _pHandle, unsigned int _OutputIndex, size_t _JobUserData);
	static void SUBSTANCE_CALLBACK OnInputImageLock(SubstanceHandle* _pHandle, size_t _JobUserData, unsigned int _InputIndex, SubstanceTextureInput** _ppCurrentTextureInputDesc, const SubstanceTextureInput* _pPreferredTextureInputDesc);
	static void SUBSTANCE_CALLBACK OnInputImageUnlock(SubstanceHandle* _pHandle, size_t _JobUserData, unsigned int _InputIndex, SubstanceTextureInput* _ppCurrentTextureInputDesc);
	static void SUBSTANCE_CALLBACK OnOutOfMemory(SubstanceHandle *handle, int memoryType);

	// Substance notifications
	void NotifySubstanceCreation(ProceduralMaterial* substance);
	void NotifySubstanceDestruction(ProceduralMaterial* substance);
	void NotifyTextureDestruction(ProceduralTexture* texture);
	void NotifyPackageDestruction(SubstanceArchive* package);
	
	// Priority handling
	void SetProcessorUsage(ProceduralProcessorUsage usage);
	ProceduralProcessorUsage GetProcessorUsage() const { return m_ProcessorUsage; }
	void ClearProcessingQueue();

	// Queued substance data
	struct Substance
	{
		ProceduralMaterial* material;
		std::map<std::string, SubstanceValue> inputValues;
		std::map<ProceduralTexture*, SubstanceTexture> updatedTextures;
	};
	std::map<unsigned int, ProceduralTexture*> processedTextures;
	void ForceSubstanceResults(std::map<ProceduralTexture*, SubstanceTexture>& results);

	// Memory budget management
	void UpdateMemoryBudget();

	// Context of generation which temporary set processor usage
	class Context
	{
	public:
		Context(ProceduralProcessorUsage usage);
		~Context();
	private:
		ProceduralProcessorUsage m_OldProcessorUsage;
	};

	void BeginPreloading() { ++integrationTimeStamp; }
	unsigned int integrationTimeStamp;

private:
	void ApplyMemoryBudget(ProceduralMaterial* substance, bool requireMaximum, bool outOfMemory=false);

	// Threaded integration / packing of substances
	void Integrate();

	// Substance queue helpers
	struct QueueFinder
	{
		const ProceduralMaterial* m_Material;
		QueueFinder(const ProceduralMaterial* material) : m_Material(material) {}
		bool operator()(const Substance* substance) { return substance->material == m_Material; }
	};

	struct QueuePackageFinder
	{
		const SubstanceArchive* m_Package;
		QueuePackageFinder(const SubstanceArchive* package) : m_Package(package) {}
		bool operator()(const Substance* substance) { return substance->material->GetSubstancePackage()==m_Package; }
	};

	struct ArrayPackageFinder
	{
		const SubstanceArchive* m_Package;
		ArrayPackageFinder(const SubstanceArchive* package) : m_Package(package) {}
		bool operator()(const ProceduralMaterial* substance) { return substance->GetSubstancePackage()==m_Package; }
	};

	std::list<Substance*> waitingSubstanceQueue;
	std::list<Substance*> processingSubstanceQueue;
	std::list<Substance*> updatingSubstanceQueue;
	std::vector<ProceduralMaterial*> integrationQueue;
	Thread processingThread;
	Mutex processingMutex;
	Mutex updatingMutex;
	Mutex processedMutex;
	Mutex integrationMutex;
	Mutex waitingMutex;
	Substance* processedSubstance;
	void* outputCallback;
	ProceduralMaterial* lastProcessedSubstance;
	Mutex lastProcessedSubstanceMutex;
	std::vector<ProceduralMaterial*> animatedSubstanceArray;
	ProceduralProcessorUsage m_ProcessorUsage;
	ProceduralMaterial* queryClearCache;
	Mutex deleteIntegrationMutex;
	Mutex deletePingMutex;
    bool isIntegrating;
	Semaphore threadSemaphore;
	static int s_maximumSubstancePerFrame;
};

SubstanceSystem& GetSubstanceSystem ();
SubstanceSystem* GetSubstanceSystemPtr ();

#endif
