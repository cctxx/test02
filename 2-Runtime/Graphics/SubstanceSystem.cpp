#include "UnityPrefix.h"
#include "SubstanceSystem.h"

#if ENABLE_SUBSTANCE
#include "ProceduralMaterial.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Serialize/LoadProgress.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/GameManager.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/SubstanceImporter.h"
#include "Editor/Src/Application.h"
#endif

using namespace std;

int SubstanceSystem::s_maximumSubstancePerFrame = 40;

SubstanceSystem::SubstanceSystem() :
	processedSubstance(NULL),
	outputCallback(NULL),
	lastProcessedSubstance(NULL),
	queryClearCache(NULL),
    isIntegrating(false),
	integrationTimeStamp(0)
{
	int Error = substanceContextInit( &m_Context, &m_Device );
	Assert( !Error && "Failed to initialize blend platform context!" );	// We use the BLEND platform
	
	// Setup memory callback
	Error = substanceContextSetCallback( m_Context, Substance_Callback_Malloc, (void*) OnMalloc );
	Assert( !Error && "Failed to setup malloc callback!" );
	Error = substanceContextSetCallback( m_Context, Substance_Callback_Free, (void*) OnFree );
	Assert( !Error && "Failed to setup free callback!" );
	
	// Setup the callback for image inputs
	Error = substanceContextSetCallback( m_Context, Substance_Callback_InputImageLock, (void*) OnInputImageLock );
	Assert( !Error && "Failed to setup image lock callback!" );
	Error = substanceContextSetCallback( m_Context, Substance_Callback_InputImageUnlock, (void*) OnInputImageUnlock );
	Assert( !Error && "Failed to setup image unlock callback!" );
	Error = substanceContextSetCallback( m_Context, Substance_Callback_OutOfMemory, (void*) OnOutOfMemory );
	Assert( !Error && "Failed to setup out_of_memory callback!" );

	SetOutputCallback();
	processingThread.SetName ("UnitySubstanceThread");
	processingThread.Run(ThreadMain, this);
	SetProcessorUsage(ProceduralProcessorUsage_One);
}

SubstanceSystem::~SubstanceSystem()
{
	processingThread.SignalQuit();
	threadSemaphore.Signal();
	processingThread.WaitForExit();

	if ( m_Context == NULL )
		return;	// Already released !
		
	// Release the context
	substanceContextRelease( m_Context );
	m_Context = NULL;
}

void SubstanceSystem::SetOutputCallback(void* callback)
{
	outputCallback = callback==NULL?(void*)OnOutputCompleted:callback;
	int err = substanceContextSetCallback( m_Context, Substance_Callback_OutputCompleted, outputCallback );
	AssertMsg(err == 0, "Failed to sertup render callback!");
}

bool SubstanceSystem::AreQueuesEmpty()
{
	return integrationQueue.size()==0
		&& waitingSubstanceQueue.size()==0
		&& processingSubstanceQueue.size()==0
		&& processedSubstance==NULL
		&& updatingSubstanceQueue.size()==0;
}

bool SubstanceSystem::AreIntegratingQueuesEmpty()
{
	{
		Mutex::AutoLock integrationLocker(integrationMutex);
		if (integrationQueue.size()>0)
		for (std::vector<ProceduralMaterial*>::iterator it=integrationQueue.begin() ; it!=integrationQueue.end() ; ++it)
			if ((*it)->integrationTimeStamp==integrationTimeStamp)
				return false;
	}

	{
		Mutex::AutoLock waitingLocker(waitingMutex);		
		if (waitingSubstanceQueue.size()>0)
			for (std::list<Substance*>::iterator it=waitingSubstanceQueue.begin() ; it!=waitingSubstanceQueue.end() ; ++it)
				if ((*it)->material->integrationTimeStamp==integrationTimeStamp)
					return false;
	}

	{
		Mutex::AutoLock processingLocker(processingMutex);
		if (processingSubstanceQueue.size()>0)
			for (std::list<Substance*>::iterator it=processingSubstanceQueue.begin() ; it!=processingSubstanceQueue.end() ; ++it)
				if ((*it)->material->integrationTimeStamp==integrationTimeStamp)
					return false;
	}
		
	{
		Mutex::AutoLock processedLocker(processedMutex);
		if (processedSubstance!=NULL && processedSubstance->material->integrationTimeStamp==integrationTimeStamp)
			return false;
	}

	{
		Mutex::AutoLock updatingLocker(updatingMutex);
		if (updatingSubstanceQueue.size()>0)
			for (std::list<Substance*>::iterator it=updatingSubstanceQueue.begin() ; it!=updatingSubstanceQueue.end() ; ++it)
				if ((*it)->material->integrationTimeStamp==integrationTimeStamp)
					return false;
	}

	return true;
}

bool SubstanceSystem::IsSubstanceProcessing(const ProceduralMaterial* substance)
{
	// Search into integration queue
	{
		Mutex::AutoLock integrationLocker(integrationMutex);
		std::vector<ProceduralMaterial*>::iterator it = std::find(integrationQueue.begin(), integrationQueue.end(), substance);
		if (it!=integrationQueue.end())
			return true;
	}

	// Search into processing queue
	{
		Mutex::AutoLock processingLocker(processingMutex);
		std::list<Substance*>::iterator it = std::find_if(processingSubstanceQueue.begin(), processingSubstanceQueue.end(), QueueFinder(substance));
		if (it!=processingSubstanceQueue.end())
			return true;
	}

	// Is currently processing ?
	{
		Mutex::AutoLock processedLocker(processedMutex);
		if (processedSubstance!=NULL && processedSubstance->material==substance)
			return true;
	}

	// Search into updating queue
	{
		Mutex::AutoLock updatingLocker(updatingMutex);
		std::list<Substance*>::iterator it = std::find_if(updatingSubstanceQueue.begin(), updatingSubstanceQueue.end(), QueueFinder(substance));
		if (it!=updatingSubstanceQueue.end())
			return true;
	}
	
	return false;
}

void SubstanceSystem::WaitFinished(LoadProgress* progress)
{
    isIntegrating = true;
	int integrate_count = integrationQueue.size();
	if (progress!=NULL)
	{
		progress->totalItems += integrate_count;
	}

	while (!AreIntegratingQueuesEmpty())
	{
		if (Thread::CurrentThreadIsMainThread())
			Update();

		if (progress!=NULL)
		{
			int new_count = integrationQueue.size();
			progress->ItemProcessed(integrate_count-new_count);
			integrate_count = integrationQueue.size();
		}

		Thread::Sleep(0.001);
	}
    isIntegrating = false;
}

void SubstanceSystem::WaitFinished(const ProceduralMaterial* substance)
{
    isIntegrating = true;
	while (IsSubstanceProcessing(substance))
	{
		if (Thread::CurrentThreadIsMainThread())
			Update();

		Thread::Sleep(0.001);
	}
    isIntegrating = false;
}

void SubstanceSystem::Update(bool isQuitSignaled)
{
    if (!AreQueuesEmpty())
		threadSemaphore.Signal();

	// Clear waiting queue
	{
		Mutex::AutoLock waitingLocker(waitingMutex);		
		while (waitingSubstanceQueue.size()>0)
		{
			delete waitingSubstanceQueue.back();
			waitingSubstanceQueue.pop_back();
		}
	}
	
	// Update animated substances
	if (!isIntegrating && !isQuitSignaled)
	{
		Mutex::AutoLock deleteLocker(deletePingMutex);
		for (std::vector<ProceduralMaterial*>::iterator it=animatedSubstanceArray.begin();it!=animatedSubstanceArray.end();++it)
		{
			ProceduralMaterial *substance = *it;
			substance->UpdateAnimation(GetTimeManager().GetCurTime());
		}
	}	

	// Upload updated substances
	int uploadedCount = s_maximumSubstancePerFrame;
	Substance* updatedSubstance;
	while (uploadedCount-- && updatingSubstanceQueue.size()>0)
	{
        // Pick one if available
		{
			Mutex::AutoLock updatingLocker(updatingMutex);
			std::list<Substance*>::iterator it = updatingSubstanceQueue.begin();
			if (it==updatingSubstanceQueue.end())
				break;
			updatedSubstance = *it;
			updatingSubstanceQueue.erase(it);
		}

        // Upload textures
		{
			//WarningStringMsg("SUBSTANCE: Uploading %d textures from %s", updatedSubstance->updatedTextures.size(), updatedSubstance->material->GetName());
			for (std::map< ProceduralTexture*, SubstanceTexture >::iterator it=updatedSubstance->updatedTextures.begin();it!=updatedSubstance->updatedTextures.end();++it)
			{
				it->first->UploadSubstanceTexture(it->second);
#if UNITY_EDITOR
				SubstanceImporter::OnTextureModified(*updatedSubstance, *it->first, it->second);
				//WarningStringMsg("SUBSTANCE: Uploading %s from material %s", it->first->GetName(), it->first->GetSubstanceMaterial()->GetName());
#endif
            }

#if UNITY_EDITOR
			SubstanceImporter::OnSubstanceModified(*updatedSubstance);
#endif
			for (std::map< ProceduralTexture*, SubstanceTexture >::iterator it=updatedSubstance->updatedTextures.begin();it!=updatedSubstance->updatedTextures.end();++it)
			{
				OnFree(it->second.buffer);
			}
			delete updatedSubstance;
		}
	}

	if (isQuitSignaled)
	{
		// Clear processing queue
		Mutex::AutoLock processingLocker(processingMutex);
		for (std::list<Substance*>::iterator it=processingSubstanceQueue.begin();it!=processingSubstanceQueue.end();++it)
			delete *it;
		processingSubstanceQueue.clear();
	}	
}

void SubstanceSystem::UpdateThreaded()
{
	// Integrate loaded substances, if any
	Integrate();
	
	// Retrieve next substance to process
	{
		Mutex::AutoLock processingLocker(processingMutex);
		std::list<Substance*>::iterator it = processingSubstanceQueue.begin();
		if (it==processingSubstanceQueue.end())
			return;
		
		{
			Mutex::AutoLock processedLocker(processedMutex);
			processedSubstance = *it;
		}
		processingSubstanceQueue.erase(it);
	}

	// Set input values
	for (std::map<std::string, SubstanceValue>::iterator i=processedSubstance->inputValues.begin();i!=processedSubstance->inputValues.end();++i)
	{
		processedSubstance->material->Callback_SetSubstanceInput(i->first, i->second);
	}
	
	// Clear cache if required
	if (processedSubstance->material==queryClearCache)
	{
		ApplyMemoryBudget(processedSubstance->material, false);
		queryClearCache = NULL;
	}

	// Generate textures
	processedTextures.clear();
	if (!processedSubstance->material->ProcessTexturesThreaded(processedSubstance->updatedTextures))
	{
		Mutex::AutoLock processedLocker(processedMutex);
		delete processedSubstance;
		processedSubstance = NULL;
		return;
	}
	
	// Push substance into the updating queue
	{
		Mutex::AutoLock updatingLocker(updatingMutex);
		updatingSubstanceQueue.push_back(processedSubstance);
	}
	// Clear
	{
		Mutex::AutoLock processedLocker(processedMutex);
		processedSubstance = NULL;
	}
}

void SubstanceSystem::QueueInput( ProceduralMaterial* material, std::string inputName, SubstanceValue& inputValue )
{
	// Skip if the material is broken
	if (material->IsFlagEnabled(ProceduralMaterial::Flag_Broken))
		return;

	material->SetDirty();

#if UNITY_EDITOR
	SubstanceImporter::OnInputModified(*material, inputName, inputValue);
#endif

	// Directly apply value if not loaded
	if (material->GetSubstanceHandle()==NULL)
	{
		material->Callback_SetSubstanceInput(inputName, inputValue);
		return;
	}

	// Search if the substance is already in the process queue
	{
		Mutex::AutoLock locker(processingMutex);
		std::list<Substance*>::iterator it = std::find_if(processingSubstanceQueue.begin(), processingSubstanceQueue.end(), QueueFinder(material));
		if (it!=processingSubstanceQueue.end())
		{
			(*it)->inputValues[inputName] = inputValue;
			return;
		}
	}
	
	// Search if is already in the waiting queue
	{
		Mutex::AutoLock waitingLocker(waitingMutex);	
		std::list<Substance*>::iterator it = std::find_if(waitingSubstanceQueue.begin(), waitingSubstanceQueue.end(), QueueFinder(material));
		if (it!=waitingSubstanceQueue.end())
		{
			(*it)->inputValues[inputName] = inputValue;
			return;
		}
	}

	// Search if is already in the integration queue
	{
		Mutex::AutoLock integrationLocker(integrationMutex);
		std::vector<ProceduralMaterial*>::iterator i = std::find(integrationQueue.begin(), integrationQueue.end(), material);
		if (i!=integrationQueue.end())
		{
			material->Callback_SetSubstanceInput(inputName, inputValue);
			return;
		}
	}

	// Else add it in the waiting queue
	Substance* substance = new Substance();
	substance->material = material;
	substance->inputValues[inputName] = inputValue;
	{
		Mutex::AutoLock waitingLocker(waitingMutex);	
		waitingSubstanceQueue.push_back(substance);
	}		
}

void SubstanceSystem::QueueSubstance(ProceduralMaterial* material)
{
	// Skip if the material is broken
	if (material->IsFlagEnabled(ProceduralMaterial::Flag_Broken))
        return;

	if (material->GetSubstanceHandle()==NULL)
	{
		if (IsWorldPlaying() && material->GetLoadingBehavior()==ProceduralLoadingBehavior_None)
			material->EnableFlag(ProceduralMaterial::Flag_ForceGenerate);
		QueueLoading(material);
		return;
	}

	// If it exists in the processing queue, input values are already pushed -> leave it as is
	if (processingSubstanceQueue.size()>0)
	{
		Mutex::AutoLock locker(processingMutex);
		std::list<Substance*>::iterator it = std::find_if(processingSubstanceQueue.begin(), processingSubstanceQueue.end(), QueueFinder(material));
		if (it!=processingSubstanceQueue.end())
			return;
	}

	// If it exists in the waiting queue, push it into the processing queue
	if (waitingSubstanceQueue.size()>0)
	{
		Mutex::AutoLock waitingLocker(waitingMutex);	
		std::list<Substance*>::iterator it = std::find_if(waitingSubstanceQueue.begin(), waitingSubstanceQueue.end(), QueueFinder(material));
		if (it!=waitingSubstanceQueue.end())
		{
			Mutex::AutoLock locker(processingMutex);
			processingSubstanceQueue.push_back(*it);
			waitingSubstanceQueue.erase(it);
			return;
		}
	}	

	// Push it into the processing queue
	Substance* substance = new Substance();
	substance->material = material;
	Mutex::AutoLock locker(processingMutex);
	processingSubstanceQueue.push_back(substance);
}

void SubstanceSystem::QueueLoading(ProceduralMaterial* material)
{
	// Skip if the material is broken
	if (material->IsFlagEnabled(ProceduralMaterial::Flag_Broken))
		return;

	// Already loaded ?
	if (material->GetSubstanceHandle()!=NULL)
		return;

	// If it exists in the waiting queue, directly assign input values
	{
		Mutex::AutoLock waitingLocker(waitingMutex);	
		std::list<Substance*>::iterator it = std::find_if(waitingSubstanceQueue.begin(), waitingSubstanceQueue.end(), QueueFinder(material));
		if (it!=waitingSubstanceQueue.end())
		{
#if UNITY_EDITOR
			material->EnableFlag(ProceduralMaterial::Flag_Awake, false);
#endif
			for (std::map<std::string, SubstanceValue>::iterator i=(*it)->inputValues.begin() ; i!=(*it)->inputValues.end() ; ++i)
				material->Callback_SetSubstanceInput(i->first, i->second);
			delete *it;
			waitingSubstanceQueue.erase(it);
		}
	}

    // Push it to the integrationQueue if it isn't
	Mutex::AutoLock integrationLocker(integrationMutex);
	std::vector<ProceduralMaterial*>::iterator i = std::find(integrationQueue.begin(), integrationQueue.end(), material);
	if (i==integrationQueue.end())
		integrationQueue.push_back(material);
}

void SubstanceSystem::QueryClearCache(ProceduralMaterial* material)
{
	queryClearCache = material;
	QueueSubstance(material);
}

void* SubstanceSystem::ThreadMain(void*data)
{
	SubstanceSystem* system = static_cast<SubstanceSystem*>(data);
	while (!system->processingThread.IsQuitSignaled())
	{
		system->threadSemaphore.WaitForSignal();
		system->threadSemaphore.Reset();

		for (int i=0 ; i<s_maximumSubstancePerFrame ; ++i)
			system->UpdateThreaded();
	}
	return NULL;
}

#define SUBSTANCE_TRACE_MEM_CALLBACKS 0

void* SUBSTANCE_CALLBACK SubstanceSystem::OnMalloc(size_t bytesCount, size_t alignment)
{
	void *ptr = UNITY_MALLOC_ALIGNED_NULL(kMemSubstance, bytesCount, alignment);
	if (!ptr)
	{
		ErrorString("Could not allocate memory in OnMalloc (SubstanceSystem)");
	}
#if SUBSTANCE_TRACE_MEM_CALLBACKS
	Substance *substance = GetSubstanceSystem().processedSubstance;
	WarningStringMsg("\n%08X %d Alloc %s", ptr, bytesCount, (substance != NULL ? substance->material->GetName() : "No Substance"));
#endif
	return ptr;
}

void SUBSTANCE_CALLBACK SubstanceSystem::OnFree(void* bufferPtr)
{
#if SUBSTANCE_TRACE_MEM_CALLBACKS
	Substance *substance = GetSubstanceSystem().processedSubstance;
	WarningStringMsg("\n%08X Free %s", bufferPtr, (substance != NULL ? substance->material->GetName() : "No Substance"));
#endif
	UNITY_FREE(kMemSubstance, bufferPtr);
}

void SUBSTANCE_CALLBACK	SubstanceSystem::OnOutputCompleted( SubstanceHandle* _pHandle, unsigned int _OutputIndex, size_t _JobUserData )
{
	SubstanceOutputDesc outputDesc;
	if (substanceHandleGetOutputDesc(_pHandle, _OutputIndex, &outputDesc)==0)
	{
		std::map<unsigned int, ProceduralTexture*>::iterator it;
		it = GetSubstanceSystem ().processedTextures.find(outputDesc.outputId);
		if (it!=GetSubstanceSystem ().processedTextures.end())
		{
			ProceduralTexture* texture = it->second;
			Substance* substance = GetSubstanceSystem().processedSubstance;
			Assert( substance!=NULL && "Failed to update substance!" );
			//WarningStringMsg("SUBSTANCE: OnOutputCompleted received texture %s from substance %s", it->second->GetName(), substance->material->GetName());
			if (substanceHandleGetOutputs(substance->material->GetSubstanceHandle(), Substance_OutOpt_TextureId, texture->GetSubstanceTextureUID(), 1, &substance->updatedTextures[texture])!=0)
			{
				ErrorStringObject("Failed to retrieve substance texture data", substance->material);
				return;
			}
		}
	}
}

void SUBSTANCE_CALLBACK	SubstanceSystem::OnInputImageLock( SubstanceHandle* _pHandle, size_t _JobUserData, unsigned int _InputIndex, SubstanceTextureInput** _ppCurrentTextureInputDesc, const SubstanceTextureInput* _pPreferredTextureInputDesc )
{
	Substance* substance = GetSubstanceSystem().processedSubstance;
	Assert( substance!=NULL && "Failed to update substance input texture!" );
	substance->material->ApplyTextureInput(_InputIndex, *_pPreferredTextureInputDesc);
}

void SUBSTANCE_CALLBACK	SubstanceSystem::OnInputImageUnlock( SubstanceHandle* _pHandle, size_t _JobUserData, unsigned int _InputIndex, SubstanceTextureInput* _ppCurrentTextureInputDesc )
{	
}

void SUBSTANCE_CALLBACK SubstanceSystem::OnOutOfMemory(SubstanceHandle *handle, int memoryType)
{
	Substance* substance = GetSubstanceSystem().processedSubstance;
	if (substance->material->GetProceduralMemoryWorkBudget()!=ProceduralCacheSize_NoLimit)
	{
		GetSubstanceSystem().ApplyMemoryBudget(substance->material, true, true);
	}
}

void SubstanceSystem::NotifySubstanceCreation(ProceduralMaterial* substance)
{
	// Add it into the animated substance array if needed
	if (substance->IsFlagEnabled(ProceduralMaterial::Flag_Animated))
	{
		if (std::find(animatedSubstanceArray.begin(), animatedSubstanceArray.end(), substance)==animatedSubstanceArray.end())
		{
			animatedSubstanceArray.push_back(substance);
		}
	}
}
	
void SubstanceSystem::NotifySubstanceDestruction(ProceduralMaterial* substance)
{
	// Remove it from animated array if in
	{
		if (substance->IsFlagEnabled(ProceduralMaterial::Flag_Animated))
		{
			Mutex::AutoLock deletePingLocker(deletePingMutex);
			std::vector<ProceduralMaterial*>::iterator i(std::find(animatedSubstanceArray.begin(), animatedSubstanceArray.end(), substance));
			if (i!=animatedSubstanceArray.end())
				animatedSubstanceArray.erase(i);
		}
	}

	// Remove it from integration array if in
	{
		Mutex::AutoLock deletePingLocker(deletePingMutex);
		Mutex::AutoLock deleteIntegrationLocker(deleteIntegrationMutex);
		Mutex::AutoLock integrationLocker(integrationMutex);
		std::vector<ProceduralMaterial*>::iterator i = std::find(integrationQueue.begin(), integrationQueue.end(), substance);
		if (i!=integrationQueue.end())
			integrationQueue.erase(i);
	}

	// Remove it from processing array if in
	{
		Mutex::AutoLock processingLocker(processingMutex);
		std::list<Substance*>::iterator it;			
		while((it=std::find_if(processingSubstanceQueue.begin(), processingSubstanceQueue.end(), QueueFinder(substance)))!=processingSubstanceQueue.end())
		{
			delete *it;
			processingSubstanceQueue.erase(it);
		}
	}

	// Wait if it's processing
	while(1)
	{
		{
			Mutex::AutoLock processedLocker(processedMutex);
			if (processedSubstance==NULL || substance->GetSubstanceData()!=processedSubstance->material->GetSubstanceData())
				break;
		}
		Thread::Sleep(0.001);
	}

	// Remove it from updating array if in
	{
		Mutex::AutoLock updatingLocker(updatingMutex);
		std::list<Substance*>::iterator it = std::find_if(updatingSubstanceQueue.begin(), updatingSubstanceQueue.end(), QueueFinder(substance));
		if (it!=updatingSubstanceQueue.end())
		{
			// Delete the output buffers from the Substance if they have not yet been pushed to the GPU
			// This can happen when building for example, where very quick cycles of load/generate/discard are performed
			for (std::map<ProceduralTexture*,SubstanceTexture>::iterator itTex = (*it)->updatedTextures.begin() ;
			     itTex != (*it)->updatedTextures.end() ; ++itTex)
			{
				OnFree(itTex->second.buffer);
			}

			delete *it;
			updatingSubstanceQueue.erase(it);
		}
	}

	// Clear last processed if needed
	{
		Mutex::AutoLock lastSubstanceLocker(lastProcessedSubstanceMutex);
		if (substance==lastProcessedSubstance)
			lastProcessedSubstance = NULL;
	}
}

void SubstanceSystem::NotifyTextureDestruction(ProceduralTexture* texture)
{
	if (texture->GetSubstanceMaterial()!=NULL)
		NotifySubstanceDestruction(texture->GetSubstanceMaterial());
}

void SubstanceSystem::NotifyPackageDestruction(SubstanceArchive* package)
{
	// Remove it from animated array if in
	{
		Mutex::AutoLock deletePingLocker(deletePingMutex);
		std::vector<ProceduralMaterial*>::iterator it;
		while((it=std::find_if(animatedSubstanceArray.begin(), animatedSubstanceArray.end(), ArrayPackageFinder(package)))!=animatedSubstanceArray.end())
		{
			animatedSubstanceArray.erase(it);
		}
	}
	
	// Remove it from integration array if in
	{
		Mutex::AutoLock deletePingLocker(deletePingMutex);
		Mutex::AutoLock deleteIntegrationLocker(deleteIntegrationMutex);
		Mutex::AutoLock integrationLocker(integrationMutex);
		std::vector<ProceduralMaterial*>::iterator i;
		while((i=std::find_if(integrationQueue.begin(), integrationQueue.end(), ArrayPackageFinder(package)))!=integrationQueue.end())
			integrationQueue.erase(i);
	}

	// Remove it from processing array if in
	{
		Mutex::AutoLock processingLocker(processingMutex);
		std::list<Substance*>::iterator it;			
		while((it=std::find_if(processingSubstanceQueue.begin(), processingSubstanceQueue.end(), QueuePackageFinder(package)))!=processingSubstanceQueue.end())
		{
			delete *it;
			processingSubstanceQueue.erase(it);
		}
	}

	// Wait if it's processing
	while(1)
	{
		{
			Mutex::AutoLock processedLocker(processedMutex);
			if (processedSubstance==NULL || package!=processedSubstance->material->GetSubstancePackage())
				break;
		}
		Thread::Sleep(0.001);
	}
	
	// Remove it from updating array if in
	{
		Mutex::AutoLock updatingLocker(updatingMutex);
		std::list<Substance*>::iterator it;			
		while((it=std::find_if(updatingSubstanceQueue.begin(), updatingSubstanceQueue.end(), QueuePackageFinder(package)))!=updatingSubstanceQueue.end())
		{
			// Delete the output buffers from the Substance if they have not yet been pushed to the GPU
			// This can happen when building for example, where very quick cycles of load/generate/discard are performed
			for (std::map<ProceduralTexture*,SubstanceTexture>::iterator itTex = (*it)->updatedTextures.begin() ;
			     itTex != (*it)->updatedTextures.end() ; ++itTex)
			{
				OnFree(itTex->second.buffer);
			}

			delete *it;
			updatingSubstanceQueue.erase(it);
		}
	}
	
	// Clear last processed if needed
	{
		Mutex::AutoLock lastSubstanceLocker(lastProcessedSubstanceMutex);
		if (lastProcessedSubstance!=NULL && package==lastProcessedSubstance->GetSubstancePackage())
			lastProcessedSubstance = NULL;
	}
}

void SubstanceSystem::SetProcessorUsage(ProceduralProcessorUsage usage)
{
	m_ProcessorUsage = usage;

	switch(m_ProcessorUsage)
	{
		case ProceduralProcessorUsage_Half:
			processingThread.SetPriority(kNormalPriority);	
			break;
		case ProceduralProcessorUsage_All:
			processingThread.SetPriority(kNormalPriority);	
			break;
		default:
			processingThread.SetPriority(kLowPriority);	
	}
}

void SubstanceSystem::ClearProcessingQueue()
{
	Mutex::AutoLock processingLocker(processingMutex);
	for (std::list<Substance*>::iterator it=processingSubstanceQueue.begin() ; it!=processingSubstanceQueue.end() ; ++it)
	{
		delete *it;
	}
	processingSubstanceQueue.clear();
}

SubstanceSystem::Context::Context(ProceduralProcessorUsage usage)
{
	m_OldProcessorUsage = GetSubstanceSystem().GetProcessorUsage();
	GetSubstanceSystem().SetProcessorUsage(usage);
}
	
SubstanceSystem::Context::~Context()
{
	GetSubstanceSystem().SetProcessorUsage(m_OldProcessorUsage);
}

void SubstanceSystem::ForceSubstanceResults(std::map<ProceduralTexture*, SubstanceTexture>& results)
{
	Assert( processedSubstance!=NULL && "Substance caching incorrect behavior!" );
	processedSubstance->updatedTextures = results;
}

void SubstanceSystem::UpdateMemoryBudget()
{
	Mutex::AutoLock lastSubstanceLocker(lastProcessedSubstanceMutex);
	if (lastProcessedSubstance!=NULL && 
	    lastProcessedSubstance->GetSubstanceHandle()!=processedSubstance->material->GetSubstanceHandle())
	{
		//WarningStringMsg("Emptying Substance cache for handle %p, for substance %s\n", lastProcessedSubstance->GetSubstanceHandle(), lastProcessedSubstance->GetName());
		ApplyMemoryBudget(lastProcessedSubstance, false);
	}

	lastProcessedSubstance = processedSubstance->material;
	ApplyMemoryBudget(lastProcessedSubstance, true);
}

void SubstanceSystem::ApplyMemoryBudget(ProceduralMaterial* substance, bool requireMaximum, bool outOfMemory)
{
	if (substance->GetSubstanceHandle()==NULL)
	{
		// The substance is not loaded now, so skip it.
		return;
	}

	// Always use tiny memory budget in the editor if the material is loading, this to lower memory usage.
	// Only do this when NOT playing (in Play mode, the runtime-set mem. budget must be used)
#if UNITY_EDITOR
	if (!IsWorldPlaying() && requireMaximum && substance->IsFlagEnabled(ProceduralMaterial::Flag_Awake))
		substance->SetProceduralMemoryWorkBudget(ProceduralCacheSize_Tiny);
#endif

	SubstanceHardResources memoryBudget;
	memset(&memoryBudget, 0, sizeof(SubstanceHardResources));
	int processorCount = max(systeminfo::GetProcessorCount(), 1);
	int mediumLimit = max(processorCount/2, 1);

	for (int index=0;index<SUBSTANCE_CPU_COUNT_MAX;++index)
	{
		unsigned char processorUsage(Substance_Resource_FullUse);

		if ((m_ProcessorUsage==ProceduralProcessorUsage_Half && index>=mediumLimit)
			|| (m_ProcessorUsage==ProceduralProcessorUsage_One && index>0))
		{
			processorUsage = Substance_Resource_DoNotUse;
		}

		memoryBudget.cpusUse[index] = processorUsage;
	}
	
	if (outOfMemory)
	{
		substance->SetProceduralMemoryWorkBudget(
			(ProceduralCacheSize)((int)substance->GetProceduralMemoryWorkBudget()+1));
	}

	size_t workSize = GetProceduralMemoryBudget(substance->GetProceduralMemoryWorkBudget());
	if (workSize==1)
		workSize = 128 * 1024 * 1024;
	size_t sleepSize = GetProceduralMemoryBudget(substance->GetProceduralMemorySleepBudget());

	memoryBudget.systemMemoryBudget = requireMaximum?workSize:sleepSize;

	if (substanceHandleSwitchHard(substance->GetSubstanceHandle(), Substance_Sync_Synchronous, &memoryBudget, NULL, 0)!=0)
		ErrorStringObject("Failed to set substance memory budget", substance);
	
	if (!outOfMemory)
	{
		if (substanceHandleStart(substance->GetSubstanceHandle(), Substance_Sync_Synchronous)!=0)
			ErrorStringObject("Failed to update substance memory budget", substance);
	}
}

void SubstanceSystem::Integrate()
{
	if (integrationQueue.size()>0)
	{
		Mutex::AutoLock deleteLocker(deleteIntegrationMutex);

		// Retrieve materials to pack
		std::vector<ProceduralMaterial*> packedMaterials;
		{
			Mutex::AutoLock integrationLocker(integrationMutex);	
			packedMaterials = integrationQueue;
		}

		// Pack all available substances
		ProceduralMaterial::PackSubstances(packedMaterials);

		for (std::vector<ProceduralMaterial*>::iterator it=packedMaterials.begin() ; it!=packedMaterials.end() ; ++it)
		{
			if (!(*it)->IsFlagEnabled(ProceduralMaterial::Flag_Broken)
				&& ((*it)->IsFlagEnabled(ProceduralMaterial::Flag_ForceGenerate)
					|| !IsWorldPlaying() || (*it)->GetLoadingBehavior()!=ProceduralLoadingBehavior_None))
			{
				QueueSubstance(*it);
				queryClearCache = *it;
			}

			// Remove it from the integrationQueue
			Mutex::AutoLock integrationLocker(integrationMutex);	
			std::vector<ProceduralMaterial*>::iterator i = std::find(integrationQueue.begin(), integrationQueue.end(), *it);
			integrationQueue.erase(i);
		}
	}
}

SubstanceSystem& GetSubstanceSystem ()
{
	return SubstanceArchive::GetSubstanceSystem();
}

SubstanceSystem* GetSubstanceSystemPtr ()
{
	return SubstanceArchive::GetSubstanceSystemPtr();
}


#endif //ENABLE_SUBSTANCE
