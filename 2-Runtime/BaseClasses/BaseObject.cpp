#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "BaseObject.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Profiler/MemoryProfilerStats.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "EventManager.h"
#include "EventIDs.h"
#if ENABLE_MONO
#include "Runtime/Mono/MonoIncludes.h"
#endif

#include "Runtime/Scripting/ScriptingUtility.h"

#if defined(__MWERKS__)
#include <hash_map>
#endif

#include "Configuration/UnityConfigure.h"
#if THREADED_LOADING
#include "Runtime/Threads/ThreadSpecificValue.h"
#include "Runtime/Threads/ProfilerMutex.h"
#endif

#if !UNITY_EXTERNAL_TOOL
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/MemoryProfiler.h"
#endif
#include "Runtime/Allocator/MemoryManager.h"

using namespace std;

#define SHOW_REGISTERED_CLASS_INFO 0


#if THREADED_LOADING
#define CHECK_IN_MAIN_THREAD DebugAssertIf(!Thread::EqualsCurrentThreadID(GetPersistentManager().GetMainThreadID()));
#define DEBUG_CHECK_IN_MAIN_THREAD AssertIf(!Thread::EqualsCurrentThreadID(GetPersistentManager().GetMainThreadID()));
#else
#define CHECK_IN_MAIN_THREAD
#define DEBUG_CHECK_IN_MAIN_THREAD
#endif


static bool IsDerivedFromRTTI (const Object::RTTI* klass, const Object::RTTI* derivedFrom)
{
	const Object::RTTI* i = klass;
	while (i)
	{
		if (derivedFrom == i)
			return true;

		i = i->base;
	}
	return false;
}

struct RegisterClassCallbackStruct
{
	RegisterClassCallback* registerClass;
	RegisterClassCallback* initClassEarly;
	RegisterClassCallback* initClass;
	RegisterClassCallback* postInitClass;
	RegisterClassCallback* cleanupClass;

	RegisterClassCallbackStruct()
	{
		registerClass = initClassEarly = initClass = postInitClass = cleanupClass = NULL;
	}
};

typedef UNITY_VECTOR(kMemPermanent, RegisterClassCallbackStruct) RegisterClassCallbacks;

Object::IDToPointerMap*   Object::ms_IDToPointer = NULL;
UInt32*	               Object::ms_IsDerivedFromBitMap = NULL;
unsigned                    Object::ms_MaxClassID = 0;
#if USE_NEW_IS_DERIVED_FROM
UInt32 gClassIDMask[32];
UInt32* Object::ms_ClassIDMask = 0;
UInt32* Object::ms_ClassIsDerivedFrom = 0;
#endif

static RegisterClassCallbacks*                gRegisterClassCallbacks = NULL;

#if defined(__MWERKS__)
#error("Metrowerks should not be used")
#endif

#if DEBUGMODE
RegisteredClassSet* gVerifyRegisteredClasses = NULL;
#endif

typedef map<char*, SInt32, smaller_cstring> StringToClassIDMap;
typedef pair<const SInt32, Object::RTTI> SInt32RTTIPair;
typedef map<SInt32, Object::RTTI, less<SInt32>, STL_ALLOCATOR(kMemPermanent, SInt32RTTIPair) > RTTIMap;

static StringToClassIDMap*                    gStringToClassID = NULL;
static RTTIMap*                               gRTTI = NULL;
static dynamic_bitset*                        gRegisteredClassIDs = NULL;
static dynamic_bitset*                        gIsDerivedFromBitMap = NULL;
static Object::ObjectDestroyCallbackFunction* gDestroyedCallbackFunc = NULL;

static int*                                   gBaseObjectManagerContainer = NULL;

namespace BaseObjectManager
{
	void StaticInitialize()
	{
		gBaseObjectManagerContainer = UNITY_NEW_AS_ROOT(int, kMemBaseObject, "Managers", "BaseObjectManager");
		SET_ALLOC_OWNER(gBaseObjectManagerContainer);
		gStringToClassID = UNITY_NEW(StringToClassIDMap, kMemBaseObject);
		gRTTI = UNITY_NEW(RTTIMap, kMemBaseObject);
		gRegisteredClassIDs = UNITY_NEW(dynamic_bitset, kMemBaseObject);
		gIsDerivedFromBitMap = UNITY_NEW(dynamic_bitset, kMemBaseObject);
		Object::StaticInitialize();
	}
	void StaticDestroy()
	{
		Object::StaticDestroy();
		UNITY_DELETE(gStringToClassID, kMemBaseObject);
		UNITY_DELETE(gRTTI, kMemBaseObject);
		UNITY_DELETE(gRegisteredClassIDs, kMemBaseObject);
		UNITY_DELETE(gIsDerivedFromBitMap, kMemBaseObject);
#if DEBUGMODE
		UNITY_DELETE(gVerifyRegisteredClasses, kMemBaseObject); // allocated on first access
#endif
		UNITY_DELETE(gBaseObjectManagerContainer, kMemBaseObject);
	}
}

static RegisterRuntimeInitializeAndCleanup s_BaseObjectManagerCallbacks(BaseObjectManager::StaticInitialize, BaseObjectManager::StaticDestroy);

#if UNITY_EDITOR
static Object::ObjectDirtyCallbackFunction*	  gSetDirtyCallbackFunc = NULL;
#endif

PROFILER_INFORMATION (gObjectCreationMutexLockInfo, "Object.CreateObject mutex lock", kProfilerLoading)

#if THREADED_LOADING
Mutex           gCreateObjectMutex;
#if DEBUGMODE
static UNITY_TLS_VALUE(int) gCheckObjectCreationMutex;
#endif
#endif

void LockObjectCreation ()
{
	#if THREADED_LOADING
	LOCK_MUTEX (gCreateObjectMutex, gObjectCreationMutexLockInfo);
	#if DEBUGMODE
		++gCheckObjectCreationMutex;
	#endif
	#endif
}

void UnlockObjectCreation ()
{
	#if THREADED_LOADING
	gCreateObjectMutex.Unlock();
	#if DEBUGMODE
		--gCheckObjectCreationMutex;
	#endif
	#endif
}


static SInt32   gLowestInstanceID = -10;
static bool   gDisableImmediateDestruction = false;

#if UNITY_EDITOR
InstanceIDResolveCallback* gInstanceIDResolveCallback = NULL;
const void* gInstanceIDResolveContext = NULL;

void SetInstanceIDResolveCallback (InstanceIDResolveCallback callback, const void* context)
{
	gInstanceIDResolveCallback = callback;
	gInstanceIDResolveContext = context;
}
#endif

void InstanceIDToLocalSerializedObjectIdentifier (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier)
{
	#if UNITY_EDITOR
	// Early out if referenced object is null
	if (id == 0)
	{
		localIdentifier.localSerializedFileIndex = 0;
		localIdentifier.localIdentifierInFile = 0;
		return;
	}

	if (gInstanceIDResolveCallback == NULL)
	{
		GetPersistentManager ().InstanceIDToLocalSerializedObjectIdentifierInternal (id, localIdentifier);
		return;
	}
	else
	{
		gInstanceIDResolveCallback (id, localIdentifier, const_cast<void*>(gInstanceIDResolveContext));
	}
	#else
	GetPersistentManager ().InstanceIDToLocalSerializedObjectIdentifierInternal (id, localIdentifier);
	#endif
}

void LocalSerializedObjectIdentifierToInstanceID (const LocalSerializedObjectIdentifier& localIdentifier, SInt32& instanceID)
{
	GetPersistentManager ().LocalSerializedObjectIdentifierToInstanceIDInternal (localIdentifier, instanceID);
}

Object* ReadObjectFromPersistentManager (int id)
{
	if (id == 0)
		return NULL;
	else
	{
		// In the Player it is not possible to call MakeObjectPersistent,
		// thus instance id's that are positive are the only ones that can be loaded from disk
		#if !UNITY_EDITOR
		if (id < 0)
		{
			#if DEBUGMODE
			//AssertIf(GetPersistentManager ().ReadObject (id));
			#endif
			return NULL;
		}
		#endif

		Object* o = GetPersistentManager ().ReadObject (id);
		return o;
	}
}

void DestroyWithoutLoadingButDontDestroyFromFile (int instanceID)
{
	GetPersistentManager ().MakeObjectUnpersistent (instanceID, kDontDestroyFromFile);
	UnloadObject(Object::IDToPointer(instanceID));
}

void DestroySingleObject (Object* o)
{
	if (o == NULL)
		return;

	if (o->IsPersistent())
		GetPersistentManager ().MakeObjectUnpersistent (o->GetInstanceID (), kDestroyFromFile);

	// Lock changes to IDToPointer so that we can safely lookup pointers using IDToPointerThreadSafe
	LockObjectCreation();

	delete_object_internal (o);

	UnlockObjectCreation();
}


Object::Object (MemLabelId label, ObjectCreationMode mode)
{
	Assert(label.label < (1 << kMemLabelBits));
	m_MemLabel = label.label;
	m_InstanceID = 0;
	m_EventIndex = NULL;

	#if ENABLE_SCRIPTING
	m_MonoReference = 0;
	m_ScriptingObjectPointer = SCRIPTING_NULL;
	#endif

	#if !UNITY_RELEASE
	m_AwakeCalled					= 0;
	m_ResetCalled					= 0;
	m_AwakeThreadedCalled			= 0;
	m_AwakeDidLoadThreadedCalled	= 0;
	#endif


	#if UNITY_EDITOR
	m_DirtyIndex = 0;
	m_FileIDHint = 0;
	#endif
	m_HideFlags = 0;
	m_TemporaryFlags = 0;
	m_IsPersistent = false;

	DebugAssert(GetMemoryManager().GetAllocator(GetMemoryLabel())->Contains(this));
	m_IsRootOwner = GetMemoryManager().GetAllocator(GetMemoryLabel())->GetProfilerHeader(this) != NULL;
	#if UNITY_WINRT
	m_TemporaryUnusedAssetsFlags = 0;
	#endif
}

void Object::CalculateCachedClassID (Object* obj) 
{
	Assert(obj->GetClassIDVirtualInternal() < (1 << kCachedClassIDBits));
	obj->m_CachedClassID = obj->GetClassIDVirtualInternal();
}

void Object::InsertObjectInMap( Object* obj )
{
	SET_ALLOC_OWNER(gBaseObjectManagerContainer);
	Assert (ms_IDToPointer->find (obj->GetInstanceID ()) == ms_IDToPointer->end ());
	ms_IDToPointer->insert (make_pair (obj->GetInstanceID (), obj));
	
	PROFILER_REGISTER_OBJECT(obj);
}

void Object::RegisterInstanceID (Object* obj)
{
	CHECK_IN_MAIN_THREAD

	LockObjectCreation();
	Assert (obj != NULL);
	AssertIf(obj->m_InstanceID == 0);
	InsertObjectInMap (obj);
	
	UnlockObjectCreation();
}

void Object::RegisterInstanceIDNoLock (Object* obj)
{
	CHECK_IN_MAIN_THREAD
	Assert (obj != NULL);
	AssertIf (obj->m_InstanceID == 0);
	CalculateCachedClassID (obj);
	InsertObjectInMap (obj);
}


Object* Object::AllocateAndAssignInstanceID (Object* obj)
{
	CHECK_IN_MAIN_THREAD
	AssertIf(obj->m_InstanceID != 0);

	LockObjectCreation();

	// Create a new unique instanceID for this Object.
	// The created id will be negative beginning with -1
	// Ids loaded from a file will be positive beginning with 1
	gLowestInstanceID-=2;
	obj->SetInstanceID (gLowestInstanceID);
	AssertIf (obj->GetInstanceID () & 1);

	CalculateCachedClassID (obj);
	InsertObjectInMap (obj);

	UnlockObjectCreation();

	obj->SetDirty();

	return obj;
}

Object* Object::AllocateAndAssignInstanceIDNoLock (Object* obj)
{
	CHECK_IN_MAIN_THREAD
	AssertIf(obj->m_InstanceID != 0);

	// Create a new unique instanceID for this Object.
	// The created id will be negative beginning with -1
	// Ids loaded from a file will be positive beginning with 1
	gLowestInstanceID-=2;
	obj->SetInstanceID (gLowestInstanceID);
	AssertIf (obj->GetInstanceID () & 1);
	
	CalculateCachedClassID (obj);

	InsertObjectInMap (obj);

	obj->SetDirty();

	return obj;
}

enum { kMonoObjectCachedPtrOffset = 12 };


// This must be executed on the main thread
void delete_object_internal_step1 (Object* object)
{
	PROFILER_UNREGISTER_OBJECT(object);
	
#if !UNITY_RELEASE
	object->CheckCorrectAwakeUsage();
#endif
	
#if THREADED_LOADING && DEBUGMODE
	Assert(gCheckObjectCreationMutex >= 1);
#endif
	
	// Send destroy message & clear event index
	if (object->m_EventIndex != NULL)
	{
		GetEventManager().InvokeEvent(object->m_EventIndex, object, kWillDestroyEvent);
		GetEventManager().RemoveEvent(object->m_EventIndex);
		object->m_EventIndex = NULL;
	}
	
	// Remove this objects instanceID from the table.
	AssertIf (Object::ms_IDToPointer->find (object->GetInstanceID ()) == Object::ms_IDToPointer->end ());
	Object::ms_IDToPointer->erase (object->GetInstanceID ());

	if (gDestroyedCallbackFunc)
		gDestroyedCallbackFunc (object->GetInstanceID ());
	
	object->m_InstanceID = 0;
}

Object::~Object ()
{
	// Ensure PreCleanupObject was called
	#if DEBUGMODE
	Assert(m_InstanceID == 0);
	#endif	
	
#if ENABLE_MONO //if unity3.5 succesfully shipped with this assert it may be removed. here to verify an assumption made in a refactor.
	Assert((m_MonoReference==0) == (m_ScriptingObjectPointer==NULL));
#endif
	
#if ENABLE_SCRIPTING
	if (m_ScriptingObjectPointer)
		SetCachedScriptingObject(SCRIPTING_NULL);
#endif
}

bool Object::MainThreadCleanup ()
{
	AssertString("MainThreadCleanup is not implemented for this class. See DoesClassRequireMainThreadDeallocation");
	return false;
}

void Object::SetIsPersistent( bool p )
{
	PROFILER_CHANGE_PERSISTANCY(GetInstanceID(), m_IsPersistent, p);
	m_IsPersistent = p;
}

#if ENABLE_SCRIPTING

#if UNITY_PS3 || UNITY_XENON
	extern "C" char* GC_clear_stack(char*);
#endif

void Object::SetupWeakHandle ()
{
	if (m_MonoReference != 0)
	{
		register ScriptingObjectPtr object = scripting_gchandle_get_target(m_MonoReference);
		UInt32 weakref = scripting_gchandle_weak_new (object);
		SetCachedScriptingObject(SCRIPTING_NULL);

#if UNITY_PS3 || UNITY_XENON
		// we need to make sure the object doesn't continue living on the stack.		// fixes http://fogbugz.unity3d.com/default.asp?444901#1065992062
		GC_clear_stack((char*)object);
#endif
		object = SCRIPTING_NULL;
		m_MonoReference = weakref;
	}
}

bool Object::RevertWeakHandle ()
{
	if (m_MonoReference != 0)
	{
		ScriptingObjectPtr target = scripting_gchandle_get_target (m_MonoReference);
		scripting_gchandle_free(m_MonoReference);
		m_MonoReference = 0;
		if (target)
		{
			SetCachedScriptingObject(target);
#if UNITY_WINRT
			// Restore cached ptr for managed object, not sure why we don't do this for Mono as well, because we reset cachedPtr in SetupWeakHandle
			// But on Mono it seems cachedPtr persists ?!
			// Maybe it's related to cachedPtr optimization
			register ScriptingObjectOfType<Object> instance(m_ScriptingObjectPointer);
			instance.SetCachedPtr(this);
#endif
		}
		return target != SCRIPTING_NULL;
	}
	else
		return false;
}


void Object::SetCachedScriptingObject (ScriptingObjectPtr object)
{
	if (object)
	{
		AssertIf(m_MonoReference != 0);
		m_MonoReference = scripting_gchandle_new (object);
		m_ScriptingObjectPointer = object;
		return;
	}

	if (m_ScriptingObjectPointer == SCRIPTING_NULL)
	{
		AssertString("Dont do this");
		return;
	}

	register ScriptingObjectOfType<Object> instance(m_ScriptingObjectPointer);
	instance.SetCachedPtr(0);

	scripting_gchandle_free (m_MonoReference);
	m_MonoReference = 0;

	m_ScriptingObjectPointer = SCRIPTING_NULL;
#if UNITY_WINRT
	instance = ScriptingObjectPtr(SCRIPTING_NULL);
#else
	instance = SCRIPTING_NULL;
#endif
}
#endif

void Object::RegisterDestroyedCallback (ObjectDestroyCallbackFunction* callback)
{
	gDestroyedCallbackFunc = callback;
}

// Register base class
void Object::RegisterClass ()
{
	RegisterClass (ClassID (Object), -1, "Object", sizeof (Object), NULL, true);
}

void Object::RegisterClass (int inClassID, int inBaseClass, const string& inName, int byteSize, FactoryFunction* inFunc, bool isAbstract)
{
	if (ClassIDToRTTI (inClassID))
		return;

	// Store ClassID -> RTTI
	AssertIf (gRTTI->find (inClassID) != gRTTI->end ());
	RTTIMap::iterator baseClass = gRTTI->find (inBaseClass);
	AssertIf (baseClass == gRTTI->end () && inBaseClass != -1);
	Object::RTTI& classInfo = (*gRTTI)[inClassID];
	classInfo.base = baseClass == gRTTI->end () ? NULL : &baseClass->second;
	classInfo.factory = inFunc;
	classInfo.className = inName;
	classInfo.classID = inClassID;
	classInfo.isAbstract = isAbstract;
	classInfo.size = byteSize;

	// Store String -> ClassID
	AssertIf (gStringToClassID->find (const_cast<char*> (inName.c_str ())) != gStringToClassID->end ());
	(*gStringToClassID)[const_cast<char*> (classInfo.className.c_str ())] = inClassID;
}

Object* Object::Produce (int classID, int instanceID, MemLabelId memLabel, ObjectCreationMode mode)
{
	// Object is already loaded assert
	AssertIf (mode == kCreateObjectDefault && IDToPointer (instanceID) != NULL);
	AssertIf (instanceID == 0 && mode == kCreateObjectFromNonMainThread);
	#if THREADED_LOADING
	AssertIf (!Thread::EqualsCurrentThreadID(GetPersistentManager().GetMainThreadID()) && mode != kCreateObjectFromNonMainThread);
	#endif
	AssertIf (instanceID & 1);

	// Find the appropriate Factory.
	RTTIMap::iterator i;
	i = gRTTI->find (classID);
	if (i == gRTTI->end () || i->second.factory == NULL)
	{
		return NULL;
	}

	Object* o;
	if (instanceID != 0)
	{
		o = i->second.factory (memLabel, mode);
		if (o == NULL)
			return NULL;
		o->SetInstanceID(instanceID);
		CalculateCachedClassID (o);

		// Register instanceID and set dirty
		if (mode == kCreateObjectDefault)
		{
			RegisterInstanceID(o);
			o->SetDirty();
		}
		else if (mode == kCreateObjectDefaultNoLock)
		{
			RegisterInstanceIDNoLock(o);
			o->SetDirty();
		}

		return o;
	}
	else
	{
		AssertIf(mode != kCreateObjectDefaultNoLock && mode != kCreateObjectDefault);
		o = i->second.factory (memLabel, mode);

		if (mode == kCreateObjectDefaultNoLock)
			AllocateAndAssignInstanceIDNoLock(o);
		else
			AllocateAndAssignInstanceID(o);
		return o;
	}
}

void Object::CheckInstanceIDsLoaded (SInt32* instanceIDs, int size)
{
	for (int i=0;i<size;i++)
	{
		if (ms_IDToPointer->count (instanceIDs[i]))
			instanceIDs[i] = 0;
	}
}

#if !UNITY_RELEASE
Object* Object::IDToPointer (int inInstanceID)
{
	DEBUG_CHECK_IN_MAIN_THREAD

	return Object::IDToPointerNoThreadCheck (inInstanceID);
}
#endif

Object* Object::IDToPointerThreadSafe (int inInstanceID)
{
	LockObjectCreation();
	Object* obj = Object::IDToPointerNoThreadCheck (inInstanceID);
		UnlockObjectCreation();
		return obj;
}

Object* Object::IDToPointerNoThreadCheck (int inInstanceID)
{
	if( !ms_IDToPointer) return NULL;

	IDToPointerMap::const_iterator i = ms_IDToPointer->find (inInstanceID);
	if (i != ms_IDToPointer->end ())
	{
		return i->second;
	}
	else
	{
		return NULL;
	}
}

#if THREADED_LOADING
Object* InstanceIDToObjectThreadSafe (int instanceID)
{
	if (Thread::EqualsCurrentThreadID (GetPersistentManager().GetMainThreadID()))
		return PPtr<Object> (instanceID);
	else
	{
		Object* obj = Object::IDToPointerThreadSafe(instanceID);
		if (obj == NULL)
			return GetPersistentManager().ReadObjectThreaded(instanceID);
		else
		return obj;
	}
}
#endif

void Object::FindAllDerivedClasses (int classID, vector<SInt32>* derivedClasses, bool onlyNonAbstract)
{
	AssertIf (derivedClasses == NULL);
	RTTIMap::iterator i;
	for (i=gRTTI->begin ();i!=gRTTI->end ();i++)
	{
		if (IsDerivedFromClassID (i->first, classID) && (!onlyNonAbstract || !i->second.isAbstract))
			derivedClasses->push_back (i->first);
	}
}

struct GetConstFirst
{
	template<typename T1, typename T2>
	const T1& operator()(const std::pair<T1, T2>& p) const {
		return p.first;
	}
};

struct GetConstSecond
{
	template<typename T1, typename T2>
	const T2& operator()(const std::pair<T1, T2>& p) const {
		return p.second;
	}
};

struct IsDerivedFromClass
{
	IsDerivedFromClass(int classID): m_ClassID(classID) {}
	bool operator()(const Object::IDToPointerMap::value_type& el) const
	{
		return el.second->IsDerivedFrom (m_ClassID);
	}
private:
	int m_ClassID;
};

inline int DerivedObjectCount(const Object::IDToPointerMap& objmap, int classID)
{
	return std::count_if(objmap.begin(), objmap.end(), IsDerivedFromClass(classID));
}

template<typename ObjectGetter, typename Container, typename Predicate>
inline int FindAllDerivedObjectsImpl (const Object::IDToPointerMap& objmap, int classID,
	ObjectGetter getter, Container* derivedObjects, bool sorted, Predicate pred)
{
	if (NULL == derivedObjects)
		return DerivedObjectCount(objmap, classID);

	int count = 0;
	for (typename Object::IDToPointerMap::const_iterator i = objmap.begin();
		i != objmap.end(); ++i)
	{
		if (i->second->IsDerivedFrom (classID))
		{
			derivedObjects->push_back (getter(*i));
			count++;
		}
	}

	if (sorted && count)
		std::sort(derivedObjects->begin(), derivedObjects->end(), pred);

	return count;
}

int Object::FindAllDerivedObjects (int classID, vector<SInt32>* derivedObjects, bool sorted)
{
	return FindAllDerivedObjectsImpl (*ms_IDToPointer, classID,
		GetConstFirst(), derivedObjects, sorted, std::less<SInt32>());
}

struct CompareInstanceID
{
	bool operator () (const Object* lhs, const Object* rhs)  const
	{
		return lhs->GetInstanceID() < rhs->GetInstanceID();
	}
};

int Object::FindObjectsOfType (int classID, vector<Object*>* derivedObjects, bool sorted)
{
	return FindAllDerivedObjectsImpl (*ms_IDToPointer, classID,
		GetConstSecond(), derivedObjects, sorted, CompareInstanceID());
}

int Object::FindObjectsOfType (int classID, dynamic_array<Object*>* derivedObjects, bool sorted)
{
	int count = 0;
	IDToPointerMap::iterator i;
	for (i=ms_IDToPointer->begin ();i!=ms_IDToPointer->end ();i++)
	{
		if (i->second->IsDerivedFrom (classID))
		{
			if (derivedObjects != NULL)
				derivedObjects->push_back (i->second);
			count++;
		}
	}


	if (sorted && derivedObjects != NULL)
	{
		CompareInstanceID compare;
		sort(derivedObjects->begin(), derivedObjects->end(), compare);
	}


	return count;
}


const std::string& Object::ClassIDToString (int ID)
{
	static std::string emptyString;
	RTTIMap::iterator i = gRTTI->find (ID);
	if (i == gRTTI->end ())
		return emptyString;
	else
		return i->second.className;
}

int Object::StringToClassID (const string& classString)
{
	StringToClassIDMap::iterator i;
	i = gStringToClassID->find (const_cast<char*> (classString.c_str ()));
	if (i == gStringToClassID->end ())
		return -1;
	else
		return i->second;
}

int Object::StringToClassIDCaseInsensitive (const string& classString)
{
	StringToClassIDMap::iterator i;
	string lowerClass = ToLower(classString);
	for (StringToClassIDMap::iterator i = gStringToClassID->begin(); i!=gStringToClassID->end(); i++)
	{
		if (ToLower(string(i->first)) == lowerClass)
			return i->second;
	}
	return -1;
}

int Object::StringToClassID (const char* classString)
{
	StringToClassIDMap::iterator i;
	i = gStringToClassID->find (const_cast<char*> (classString));
	if (i == gStringToClassID->end ())
		return -1;
	else
		return i->second;
}

const std::string& Object::GetClassName () const
{
	return Object::ClassIDToString (GetClassID ());
}

int Object::GetSuperClassID (int classID)
{
	RTTIMap::iterator i = gRTTI->find (classID);
	AssertIf (i == gRTTI->end ());
	if (i->second.base)
		return i->second.base->classID;
	else
		return ClassID (Object);
}

Object::RTTI* Object::ClassIDToRTTI (int classID)
{
	RTTIMap::iterator i = gRTTI->find (classID);
	if (i == gRTTI->end ())
		return NULL;
	else
		return &i->second;
}

struct BuildClassInfo
{
	Object::RTTI* klass;
	Object::RTTI* base;
	UInt32 id;
	UInt32 subclasses;
	UInt32 level;
	UInt32 newId;
	UInt32 subClassIdAssign;
	void Clear(){
	       base = 0;
	       klass = 0;
	       id = (UInt32)-1;
	       subclasses = 0;
	       level = 0xffffffff;
	       newId = -1;
	       subClassIdAssign = 0;
	}
	void Init(Object::RTTI* klass, Object::RTTI* base)
	{
		this->klass = klass;
		this->base = base;
	}
	bool operator <( const BuildClassInfo& o) const
	{
		return level < o.level;
	}
};
int bitsRequired(int subclasses)
{
	int r = 0;
	while(subclasses)
	{
		r++;
		subclasses >>= 1;
	}
	return r;
}

void Object::StaticInitialize()
{
	SET_ALLOC_OWNER(gBaseObjectManagerContainer);
	Object::ms_IDToPointer = UNITY_NEW(Object::IDToPointerMap (1024 * 128), kMemBaseObject);
}

void Object::StaticDestroy()
{
	UNITY_DELETE(Object::ms_IDToPointer,kMemBaseObject);
}

void Object::InitializeAllClasses ()
{
	SET_ALLOC_OWNER(gBaseObjectManagerContainer);
	
	if (gRegisterClassCallbacks == NULL)
		return;

#if SHOW_REGISTERED_CLASS_INFO
	int registeredClasses = 0;
#endif

	RegisterClassCallbacks& callbacks = *gRegisterClassCallbacks;
	// The callback is the RegisterClass function defined in ObjectDefines.h
	// It calls the static Object::RegisterClass function which sets up the rtti system
	for (int i=0;i<callbacks.size ();i++)
	{
		if (callbacks[i].registerClass)
		{
			callbacks[i].registerClass ();
#if SHOW_REGISTERED_CLASS_INFO
			++registeredClasses;
#endif
		}
	}

#if SHOW_REGISTERED_CLASS_INFO
	printf_console ("Object::InitializeAllClasses: %d total, %d registered\n", callbacks.size(), registeredClasses);
#endif

	AssertIf (gRTTI->empty ());

	// Setup ms_IsDerivedFrom lookup bitmap
	if (UNITY_EDITOR)
	{
		ms_MaxClassID = (--gRTTI->end ())->first + 1;
		Assert(kLargestEditorClassID == ms_MaxClassID);
	}
	else
	{
		ms_MaxClassID = kLargestRuntimeClassID;
	}

	gIsDerivedFromBitMap->resize (ms_MaxClassID * ms_MaxClassID, false);
	ms_IsDerivedFromBitMap = (UInt32*) gIsDerivedFromBitMap->m_bits;
	gRegisteredClassIDs->resize (ms_MaxClassID, false);
	for (int i=0;i<ms_MaxClassID;i++)
	{
		RTTIMap::iterator iRTTI = gRTTI->find (i);
		(*gRegisteredClassIDs)[i] = iRTTI != gRTTI->end ();
		if ((*gRegisteredClassIDs)[i])
		{
			for (int j=0;j<ms_MaxClassID;j++)
			{
				RTTIMap::iterator jRTTI = gRTTI->find (j);
				if (jRTTI != gRTTI->end ())
					(*gIsDerivedFromBitMap)[i * ms_MaxClassID + j] = IsDerivedFromRTTI (&iRTTI->second, &jRTTI->second);
			}
		}
	}

#if USE_NEW_IS_DERIVED_FROM
	ms_ClassIDMask = &gClassIDMask[0];
	ms_ClassIsDerivedFrom = new UInt32[ms_MaxClassID];
	memset(ms_ClassIsDerivedFrom, 0xffffffff, sizeof(UInt32) * ms_MaxClassID);
	ms_ClassIsDerivedFrom[0] = 1;
	BuildClassInfo* info = (BuildClassInfo*)alloca(sizeof(BuildClassInfo) * ms_MaxClassID);
	for(int i = 0; i < ms_MaxClassID; ++i)
		info[i].Clear();
	int maxSubClasses[32];
	int bitShift[32];
	memset(maxSubClasses, 0, sizeof(maxSubClasses));
	memset(bitShift, 0, sizeof(bitShift));
	//search for the base class
	typedef RTTIMap::iterator itr;
	for(itr i = gRTTI->begin(); i != gRTTI->end(); ++i)
	{
		Object::RTTI& r = i->second;
		SInt32 oldId = i->first;
		info[oldId].Init(&r, r.base);
		info[oldId].id = oldId;
		int level = 0;
		Object::RTTI* base = r.base;
		while(base)
		{
			++level;
			base = base->base;
		}
		info[oldId].level = level;
		if(r.base)
			info[r.base->classID].subclasses++;
	}
	std::sort(&info[0], ms_MaxClassID + &info[0]);
	for(int i = 0; i < ms_MaxClassID; ++i)
	{
		int level = info[i].level;
		maxSubClasses[level] = info[i].subclasses > maxSubClasses[level] ? info[i].subclasses : maxSubClasses[level];
	}
	int totalBits = 1;
	UInt32 mask = 1;
	bitShift[0] = 0;
	gClassIDMask[0] = 1;
	for(int i = 1; i < 32; ++i)
	{
		int bitsReq = bitsRequired(maxSubClasses[i-1]);
		int bitsReq1 = bitsReq;
		while(bitsReq--)
			mask = (mask<<1) | 1;

		bitShift[i] = totalBits;
		totalBits += bitsReq1;
		AssertIf(totalBits > 32 - CLASS_ID_MASK_BITS); //OUT OF BITS
		gClassIDMask[i] = mask;
	}
#define VERIFY_CLASS_IDS 0
#if VERIFY_CLASS_IDS
	std::set<UInt32> ClassIdSet;
#endif
	int lastIndex = 0;
	for(int i = 0; i < ms_MaxClassID; ++i)
	{
		Object::RTTI* klass = info[i].klass;
		Object::RTTI* base = info[i].base;
		int level = info[i].level;
		int parentId = 0;
		int parentIndex = -1;
		int parentSubIndex = 1;
		if(base)
		{
			for(int j = 0; j < ms_MaxClassID; ++j)
			{
				if(info[j].klass == base)
				{
					parentIndex = j;
					break;
				}
			}
			AssertIf(parentIndex >= i);
			parentId = info[parentIndex].newId & CLASS_ID_MASK_IDS;
			AssertIf(-1 == parentId);
			parentSubIndex = info[parentIndex].subClassIdAssign++;
		}
		else
		{
			parentSubIndex = 1;
			if(i != 0)
			{
				lastIndex = i;
				break;
			}
		}
		int shift = bitShift[level];
		int id = (parentSubIndex << shift) | parentId;
		int fullId = id | (level<<(32-CLASS_ID_MASK_BITS));
#if VERIFY_CLASS_IDS
		AssertIf(ClassIdSet.find(fullId) != ClassIdSet.end() || ClassIdSet.find(id) != ClassIdSet.end() ); // DUPE CLASSID. should never happen
		ClassIdSet.insert(id);
		ClassIdSet.insert(fullId);
#endif
		info[i].newId = fullId;
		ms_ClassIsDerivedFrom[info[i].id] = fullId;
		AssertIf(info[i].subClassIdAssign != 0);
		info[i].subClassIdAssign = 1;
	}
#endif
}

void Object::CallInitializeClass()
{
	RegisterClassCallbacks& callbacks = *gRegisterClassCallbacks;

	// Call the IntializeClass function for classes that registered for it (IMPLEMENT_CLASS_HAS_INIT)
	// This is done after all classes are registered and the rtti setup
	// so that the rtti system can be used insie InitializeClass ()
	for (int i=0;i<callbacks.size ();i++)
	{
		if (callbacks[i].initClass)
		{
			callbacks[i].initClass ();
		}
	}
}

void Object::CallPostInitializeClass()
{
	RegisterClassCallbacks& callbacks = *gRegisterClassCallbacks;

	// Call the PostIntializeClass function for classes that registered for it (IMPLEMENT_CLASS_HAS_POSTINIT)
	// This is done after all classes are registered and the rtti setup
	// so that the rtti system can be used inside PostInitializeClass ()
	for (int i=0;i<callbacks.size ();i++)
	{
		if (callbacks[i].postInitClass)
		{
			callbacks[i].postInitClass ();
		}
	}
}

void Object::AddEvent (EventCallback* callback, void* userData)
{
	m_EventIndex = GetEventManager().AddEvent(callback, userData, m_EventIndex);
}

void Object::RemoveEvent (EventCallback* callback, void* userData)
{
	m_EventIndex = GetEventManager().RemoveEvent(m_EventIndex, callback, userData);
}

bool Object::HasEvent (EventCallback* callback, const void* userData) const
{
	return EventManager::HasEvent(m_EventIndex, callback, userData);
}

void Object::InvokeEvent (int eventType)
{
	EventManager::InvokeEvent(m_EventIndex, this, eventType);
}

bool IsObjectAvailable (int instanceID)
{
	Object* temp = Object::IDToPointer (instanceID);
	if (temp != NULL)
		return true;

	return GetPersistentManager ().IsObjectAvailable (instanceID);
}
#if !USE_NEW_IS_DERIVED_FROM
#if !UNITY_RELEASE

bool Object::IsDerivedFromClassID (int classID, int compareClass)
{
	if (classID >= ms_MaxClassID || classID < 0)
	{
		char buffy[512];
		sprintf (buffy, "The class with classID: %d out of bounds", classID);
		AssertString (buffy);
		return false;
	}
	if (compareClass >= ms_MaxClassID || compareClass < 0)
	{
	/*
		char buffy[512];
		sprintf (buffy, "The compare class with classID: %d out of bounds", compareClass);
		AssertString (buffy);
	*/
		return false;
	}

	AssertIf (classID >= ms_MaxClassID || classID < 0);
	AssertIf (compareClass >= ms_MaxClassID || compareClass < 0);


	if (!(*gRegisteredClassIDs)[classID])
	{
		char buffy[512];
		sprintf (buffy, "The class with classID: %d is not registered (see ClassIDs.h)", classID);
		AssertString (buffy);

	}

	// When doing classID stripping
	#if !ALLOW_CLASS_ID_STRIPPING
	if (!(*gRegisteredClassIDs)[compareClass])
	{
		char buffy[512];
		sprintf (buffy, "The class with classID: %d is not registered (see ClassIDs.h)", compareClass);
		AssertString (buffy);
	}
	#endif

	int index = classID * ms_MaxClassID + compareClass;
	int block = index >> 5;
	int bit = index - (block << 5);
	return (ms_IsDerivedFromBitMap[block]) & (1 << bit);
}

#endif
#endif

INSTANTIATE_TEMPLATE_TRANSFER_WITH_DECL (Object, EXPORTDLL);

template<class TransferFunction>
void Object::Transfer (TransferFunction& transfer)
{
#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease() && SerializePrefabIgnoreProperties(transfer))
	{
		UInt32 flags = m_HideFlags;
		transfer.Transfer(flags, "m_ObjectHideFlags", kHideInEditorMask);
		m_HideFlags = flags;
	}

	if (transfer.GetFlags () & kSerializeDebugProperties)
	{
		SInt32 instanceID = GetInstanceID ();
		transfer.Transfer (instanceID, "m_InstanceID");

		LocalIdentifierInFileType fileID;
		if (IsPersistent ())
			fileID = GetPersistentManager ().GetLocalFileID (instanceID);
		else
			fileID = GetFileIDHint ();

		transfer.Transfer (fileID, "m_LocalIdentfierInFile");
	}
#endif
}

#if UNITY_EDITOR

void Object::RegisterDirtyCallback (ObjectDirtyCallbackFunction* callback)
{
	gSetDirtyCallbackFunc = callback;
}

Object::ObjectDirtyCallbackFunction* Object::GetDirtyCallback ()
{
	return gSetDirtyCallbackFunc;
}

void Object::SetDirty ()
{
	// When we run out of dirty indices, make sure it stays at 1
	m_DirtyIndex++;
	if (m_DirtyIndex == 0)
		m_DirtyIndex = 1;

	if (gSetDirtyCallbackFunc)
		gSetDirtyCallbackFunc (this);

	#if !UNITY_RELEASE
		m_DEBUGCLASSID = GetClassID ();
	#endif
}

void Object::ClearPersistentDirty ()
{
	m_DirtyIndex = 0;
}

void Object::SetPersistentDirtyIndex (UInt32 dirtyValue)
{
	m_DirtyIndex = dirtyValue;
}


#endif

#if DEBUGMODE
void AddVerifyClassRegistration (int classID)
{
	if (gVerifyRegisteredClasses == NULL)
	{
		SET_ALLOC_OWNER(gBaseObjectManagerContainer);
		gVerifyRegisteredClasses = UNITY_NEW(RegisteredClassSet, kMemManager)();
	}
	gVerifyRegisteredClasses->insert(classID);
}
const RegisteredClassSet& GetVerifyClassRegistration ()
{
	if (gVerifyRegisteredClasses == NULL)
	{
		SET_ALLOC_OWNER(gBaseObjectManagerContainer);
		gVerifyRegisteredClasses = UNITY_NEW(RegisteredClassSet, kMemManager)();
	}
	return *gVerifyRegisteredClasses;
}

#endif


void RegisterInitializeClassCallback (int classID,
									  RegisterClassCallback* registerClass,
									  RegisterClassCallback* initClass,
									  RegisterClassCallback* postInitClass,
									  RegisterClassCallback* cleanupClass)
{
	if (gRegisterClassCallbacks == NULL)
	{
		SET_ALLOC_OWNER(gBaseObjectManagerContainer);
		gRegisterClassCallbacks = UNITY_NEW(RegisterClassCallbacks,kMemBaseObject);
	}
	if (gRegisterClassCallbacks->size () <= classID)
		gRegisterClassCallbacks->resize (classID + 1);

	RegisterClassCallbacks& callbacks = *gRegisterClassCallbacks;
	if (callbacks[classID].registerClass != NULL || callbacks[classID].initClass != NULL || callbacks[classID].postInitClass != NULL || callbacks[classID].cleanupClass != NULL)
	{
		char buffer[512];
		sprintf (buffer, "ClassID: %d is already registered. ClassID's have to be unique", classID);
		FatalErrorString (buffer);
		AssertBreak(false);
	}
	callbacks[classID].registerClass = registerClass;
	callbacks[classID].initClass = initClass;
	callbacks[classID].postInitClass = postInitClass;
	callbacks[classID].cleanupClass = cleanupClass;
}

void Object::CleanupAllClasses ()
{
	AssertIf(!ms_IDToPointer->empty());

	if (!gRegisterClassCallbacks)
		return;

	RegisterClassCallbacks& callbacks = *gRegisterClassCallbacks;
	for (int i=0;i<callbacks.size ();i++)
	{
		if (callbacks[i].cleanupClass)
			callbacks[i].cleanupClass ();
	}

	UNITY_DELETE(gRegisterClassCallbacks, kMemBaseObject);
}

void SetDisableImmediateDestruction (bool disable)
{
	gDisableImmediateDestruction = disable;
}

bool GetDisableImmediateDestruction ()
{
	return gDisableImmediateDestruction;
}

void delete_object_internal (Object* p)
{
	if (!p)
		return;

	delete_object_internal_step1 (p);
	delete_object_internal_step2 (p);
}

// This can be execute on any thread.
void delete_object_internal_step2 (Object* p)
{
	MemLabelId label = p->GetMemoryLabel();
	p->~Object ();
	UNITY_FREE(label, p);
}

void UnloadObject (Object* p)
{
	if (!p)
		return;

	LockObjectCreation();
	delete_object_internal(p);
	UnlockObjectCreation();
}

void Object::DoneLoadingManagers()
{
	// We are done loading managers. Start instance IDs from a high constant value here,
	// so new managers and built-in resources can be added without changed instanceIDs
	// used by the content.
	if (gLowestInstanceID > -10000)
	{
		gLowestInstanceID = -10000;
	}
}

MemLabelId Object::GetMemoryLabel() const
{
	MemLabelIdentifier id = (MemLabelIdentifier)m_MemLabel;
	MemLabelId label(id, NULL);
	if(m_IsRootOwner)
		label.SetRootHeader(GET_ALLOC_HEADER((void*)this, label));
	return label;
}

int Object::GetRuntimeMemorySize() const
{
#if ENABLE_MEM_PROFILER
	return GetMemoryProfiler()->GetRelatedMemorySize((void*)this);
#else
	return 0;
#endif
}


#if !UNITY_RELEASE

void Object::CheckCorrectAwakeUsage()
{
	// check only if saw that object already to allow delayed awake and immediate destroy
	if ( m_AwakeCalled == 0 )
		AssertStringObject(Format("Awake has not been called '%s' (%s). Figure out where the object gets created and call AwakeFromLoad correctly.", GetName(), GetClassName().c_str()), this);

	if ( m_ResetCalled == 0 )
		AssertStringObject(Format("Reset has not been called '%s' (%s). Figure out where the object gets created and call Reset correctly.", GetName(), GetClassName().c_str()), this);

	if ( m_AwakeThreadedCalled && !m_AwakeDidLoadThreadedCalled )
		AssertStringObject(Format("AwakeFromLoadThreaded has not been called '%s' (%s). Figure out where the object gets created and call AwakeFromLoadThreaded correctly.", GetName(), GetClassName().c_str()), this);
}


#endif // !UNITY_RELEASE
