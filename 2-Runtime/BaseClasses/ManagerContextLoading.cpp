#include "UnityPrefix.h"
#include "ManagerContextLoading.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"


PROFILER_INFORMATION(gCollectGameManagers, "CollectGameManagers", kProfilerLoading)

void CollectLevelGameManagers (InstanceIDArray& outputObjects)
{
	PROFILER_AUTO(gCollectGameManagers,NULL);
	const ManagerContext& context = GetManagerContext ();
	for (int i=0;i<ManagerContext::kManagerCount;i++)
	{
		#if !UNITY_EDITOR
		// In players we might have stripped all managers
		if (!context.m_Managers[i])
			continue;
		#endif

		Object& object = *context.m_Managers[i];
		if (object.IsDerivedFrom (ClassID (LevelGameManager)))
		{
			AssertIf (object.IsPersistent () || object.TestHideFlag (Object::kDontSave));
			outputObjects.push_back(object.GetInstanceID ());
		}
	}
}

void DestroyLevelManagers ()
{
	InstanceIDArray loadedLevelManagers;
	CollectLevelGameManagers (loadedLevelManagers);
	for (InstanceIDArray::iterator i=loadedLevelManagers.begin ();i != loadedLevelManagers.end ();++i)
	{
		Object* o = Object::IDToPointer (*i);
		AssertIf (o == NULL || o->IsPersistent ());
		DestroyObjectHighLevel (o);
	}
}


/// Setup all managers to be called for a given mode.
/// When we're in edit mode, the Input, Dynamics, Fixed, Animation & Behaviour managers don't get 
/// Updated. In Play mode, everything runs.
/// @param mode kPlayMode or kEditMode



void RemoveDuplicateGameManagers ()
{
	const ManagerContext& context = GetManagerContext ();

	for (int i=0;i<ManagerContext::kManagerCount;i++)
	{
		Assert(GetManagerPtrFromContext(i) != NULL);
	}
	
	vector<GameManager*> managers;
	Object::FindObjectsOfType (&managers);

	// Remove all managers that are not in the manager context!
	for (int m=0;m<managers.size ();m++)
	{
		bool isUsed = false;
		for (int i=0;i<ManagerContext::kManagerCount;i++)
		{
			if ((Object*)managers[m] == context.m_Managers[i])
				isUsed = true;
		}
		
		if (!isUsed)
		{
			Object* obj = managers[m];
			FatalErrorIf (obj->IsPersistent ());
			ErrorString (Format("Removing duplicate game manager (%s)!", obj->GetClassName().c_str()));
			FatalErrorIf (PPtr<Object> (managers[m])->IsPersistent ());
			DestroyObjectHighLevel (managers[m]);
		}
	}

	ErrorIf (Object::FindAllDerivedObjects (ClassID (GameManager), NULL) != ManagerContext::kManagerCount);
}


typedef vector_map<SInt32, SInt32> ClassIDToInstanceID; 
static void ExtractGlobalManagers (const std::string& path, ClassIDToInstanceID& managers)
{
	GetPersistentManager ().Lock();
	SerializedFile* stream = GetPersistentManager ().GetSerializedFileInternal(path);
	if (stream == NULL)
	{
		GetPersistentManager ().Unlock();
		return;
	}

	vector<LocalIdentifierInFileType> sourceFileIDs;
	stream->GetAllFileIDs(&sourceFileIDs);
	
	for (int i=0;i<sourceFileIDs.size ();i++)
	{
		LocalIdentifierInFileType fileID = sourceFileIDs[i];
		SInt32 classID = stream->GetClassID (fileID);
		
		if (Object::IsDerivedFromClassID (classID, ClassID (GlobalGameManager)))
		{
			SInt32 instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID (path, fileID); 
			managers.push_unsorted (classID, instanceID);
		}
	}
	
	managers.sort();
	
	GetPersistentManager ().Unlock();
}

static Object* LoadManager (const ClassIDToInstanceID& managers, int classID)
{
	ClassIDToInstanceID::const_iterator i = managers.find(classID);
	if (i == managers.end())
		return NULL;
	
	Object* obj = dynamic_instanceID_cast<Object*> (i->second);
	Assert(dynamic_pptr_cast<GlobalGameManager*> (obj) != NULL);
	
	return obj;
}


string PlayerLoadSettingsAndInput(const std::string& dataFile)
{
	ClassIDToInstanceID managers;
	ExtractGlobalManagers (dataFile, managers);

	ManagerContext::Managers loadManagers[] =
	{
		ManagerContext::kPlayerSettings,
		ManagerContext::kInputManager,
		ManagerContext::kBuildSettings,
		ManagerContext::kGraphicsSettings,
		ManagerContext::kQualitySettings,
	};

	const ManagerContext& ctx = GetManagerContext();
	for( int i = 0; i < sizeof(loadManagers)/sizeof(loadManagers[0]); ++i )
	{
		int index = loadManagers[i];
		int classID = ctx.m_ManagerClassIDs[index];
		SetManagerPtrInContext(index, LoadManager(managers, classID));
		if (ctx.m_Managers[index] == NULL || !ctx.m_Managers[index]->IsDerivedFrom(classID))
			return Format("Could..... not preload global game manager #%i   i=%i", index,i);
	}

	return string();
}

string PlayerLoadGlobalManagers (const char* dataFile)
{
	ClassIDToInstanceID managers;
	ExtractGlobalManagers (dataFile, managers);
	
	// Load all game managers! All global game managers are coming first in the main data
	// (Global game managers have to be loaded before selecting the screen resolution.
	// ProjectSettings i used by screen selector and RenderManager, InputManager by screen switching)
	for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
	{
		int classID = GetManagerContext().m_ManagerClassIDs[i];
		
		Object* manager = NULL;
	
		// Manager is dead-code stripped
		if (classID != -1)
		{
			// Try to load manager
			manager = LoadManager(managers, classID);
			
			// Manager could not be loaded, create it from code instead
			if (classID != -1 && manager == NULL)
			{
				manager = CreateGameManager (classID);
				printf_console("Loading manager failed, creating from code %d\n", classID);
			}
		}

		// Assign manager as soon as it is created.
		SetManagerPtrInContext(i, manager);
	}

	std::string resetError = ResetManagerContextFromLoaded();
	if( !resetError.empty() )
		return Format("PlayerLoadGlobalManagers: %s\n", resetError.c_str());
	
	GetPersistentManager().DoneLoadingManagers();

	return string();
}

string ResetManagerContextFromLoaded ()
{
	GlobalCallbacks::Get().managersWillBeReloadedHack.Invoke();

	string error;
	const ManagerContext& context = GetManagerContext ();

	vector<GameManager*> allManagers;
	Object::FindObjectsOfType (&allManagers);

	for (int i=0;i<ManagerContext::kManagerCount;i++)
	{
		SetManagerPtrInContext(i, NULL);
		
		if (context.m_ManagerClassIDs[i] == -1)
			continue;
			
		vector<GameManager*> specificManagers;
		for (int j=0;j<allManagers.size();j++)
		{
			if (allManagers[j]->IsDerivedFrom(context.m_ManagerClassIDs[i]))
			{
				specificManagers.push_back(allManagers[j]);
			}
		}
		
		if (specificManagers.size () == 1)
			SetManagerPtrInContext(i, specificManagers[0]);
		else if (specificManagers.size () == 0)
		{
			// missing global managers are serious errors
			if( i < ManagerContext::kGlobalManagerCount )
				error += " Missing " + Object::ClassIDToString (context.m_ManagerClassIDs[i]);
		}
		else
		{
			// missing global managers are serious errors
			if( i < ManagerContext::kGlobalManagerCount )
				error += " Too many instances of " + Object::ClassIDToString (context.m_ManagerClassIDs[i]);
		}
	}
	return error;
}

void LoadManagers (AwakeFromLoadQueue& awakeFromLoadQueue)
{
	AwakeFromLoadMode awakeMode = (AwakeFromLoadMode)(kDidLoadFromDisk | kDidLoadThreaded);

	awakeFromLoadQueue.PersistentManagerAwakeFromLoad(kManagersQueue, awakeMode, NULL);
	awakeFromLoadQueue.ClearQueue(kManagersQueue);
	
	// Get all managers that are in memory
	map<int, set<GameManager*> > managers;
	vector<GameManager*> temp;
	Object::FindObjectsOfType (&temp);
	for (int i=0;i<temp.size ();i++)
		managers[temp[i]->GetClassID ()].insert (temp[i]);
	
	const ManagerContext& context = GetManagerContext();
	// Load the managers in the order defined by the context.
	// Create new managers if necessary.
	for (int i=0;i<ManagerContext::kManagerCount;i++)
	{
		SetManagerPtrInContext(i, NULL);

		if (context.m_ManagerClassIDs[i] == -1)
			continue;
		
		const set<GameManager*>& manager = managers[context.m_ManagerClassIDs[i]];
		
		if (manager.size () == 1)
			SetManagerPtrInContext(i, *manager.begin ());
		else if (manager.size () == 0)
			SetManagerPtrInContext(i, CreateGameManager (context.m_ManagerClassIDs[i]));
		else
		{
			ErrorString("Multiple managers are loaded of type: " + Object::ClassIDToString(context.m_ManagerClassIDs[i]));
			SetManagerPtrInContext(i, *manager.begin ());
		}
	}
}
