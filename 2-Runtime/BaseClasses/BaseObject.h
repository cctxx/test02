#ifndef BASEOBJECT_H
#define BASEOBJECT_H

#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Utilities/dynamic_array.h"

#include <string>
#include <vector>

#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/HashFunctions.h"

#include "Runtime/BaseClasses/ClassIDs.h"

class ProxyTransfer;
class SafeBinaryRead;
template<bool kSwap>
class StreamedBinaryRead;
template<bool kSwap>
class StreamedBinaryWrite;
class RemapPPtrTransfer;
class TypeTree;
class Object;
struct EventEntry;
#if SUPPORT_TEXT_SERIALIZATION
class YAMLRead;
class YAMLWrite;
#endif

#include "ObjectDefines.h"
#include <string>
#include <typeinfo>

//#define		DefineClassID( x, classID )
//#define		ClassID( x )

// Every non-abstract class that is derived from object has to place this inside the class Declaration
// (REGISTER_DERIVED_CLASS (Foo, Object))

// Every abstract class that is derived from object has to place this inside the class Declaration
// (REGISTER_DERIVED_ABSTRACT_CLASS (Foo, Object))

//In the cpp file of every object derived class you have to place eg. IMPLEMENT_CLASS (Foo)
//#define IMPLEMENT_CLASS(x)
// or IMPLEMENT_CLASS_HAS_INIT (x) which will call the static class Function InitializeClass (); on startup.

using std::string;

template<class T>
class PPtr
{
	SInt32	m_InstanceID;
	#if !UNITY_RELEASE
		mutable T*			m_DEBUGPtr;
	#endif

	protected:

	inline void AssignObject (const Object* o);

	private:
	static string s_TypeString;

	public:

	static const char* GetTypeString ();
	static bool IsAnimationChannel () { return false; }
	static bool MightContainPPtr () { return true; }
	static bool AllowTransferOptimization () { return false; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

	// Assignment
	explicit PPtr (int instanceID)
	{
		m_InstanceID = instanceID;
		#if !UNITY_RELEASE
		m_DEBUGPtr = NULL;
		#endif
	}
	PPtr (const T* o)								{ AssignObject (o); }
	PPtr (const PPtr<T>& o)
	{
		m_InstanceID = o.m_InstanceID;
		#if !UNITY_RELEASE
		m_DEBUGPtr = NULL;
		#endif
	}

	PPtr ()
	{
		#if !UNITY_RELEASE
		m_DEBUGPtr = NULL;
		#endif
		m_InstanceID = 0;
	}

	PPtr& operator = (const T* o)				{ AssignObject (o); return *this; }
	PPtr& operator = (const PPtr<T>& o)
	{
		#if !UNITY_RELEASE
		m_DEBUGPtr = NULL;
		#endif
		m_InstanceID = o.m_InstanceID; return *this;
	}

	void SetInstanceID (int instanceID)		{ m_InstanceID = instanceID; }
	int GetInstanceID ()const					{ return m_InstanceID; }

	// Comparison
	bool operator <  (const PPtr& p)const	{ return m_InstanceID < p.m_InstanceID; }
	bool operator == (const PPtr& p)const	{ return m_InstanceID == p.m_InstanceID; }
	bool operator != (const PPtr& p)const	{ return m_InstanceID != p.m_InstanceID; }

	// MSVC gets confused whether it should use operator bool(), or operator T* with implicit
	// comparison to NULL. So we add explicit functions and use them instead.
	bool IsNull() const;
	bool IsValid() const;

	operator T* () const;
	T* operator -> () const;
	T& operator * () const;
};

template<class T>
class ImmediatePtr
{
	mutable intptr_t m_Ptr;
	#if !UNITY_RELEASE
	mutable T* m_DEBUGPtr;
	#endif

	void AssignInstanceID (int instanceID)
	{
		AssertIf (instanceID & 1); m_Ptr = instanceID | 1; AssertIf ((m_Ptr & 1) == 0);
		#if !UNITY_RELEASE
		m_DEBUGPtr = NULL;
		#endif
	}
	void AssignObject (const T* o)
	{
		m_Ptr = (intptr_t)o; AssertIf (m_Ptr & 1);
		#if !UNITY_RELEASE
		m_DEBUGPtr = const_cast<T*>(o);
		#endif
	}
	void Load () const
	{
		AssertIf ((m_Ptr & 1) == 0);
		T* loaded = PPtr<T> (m_Ptr & (~1));
		m_Ptr = (intptr_t)(loaded);
		AssertIf (m_Ptr & 1);
		#if !UNITY_RELEASE
		m_DEBUGPtr = loaded;
		#endif
	}

	inline T* GetPtr () const
	{
		if ((m_Ptr & 1) == 0)
		{
			return (T*)(m_Ptr);
		}
		else
		{
			Load ();
			return (T*)(m_Ptr);
		}
	}

	static string s_TypeString;

	public:

	bool IsLoaded () const;

	static const char* GetTypeString ();
	static bool IsAnimationChannel ()						{ return false; }
	static bool MightContainPPtr ()							{ return true; }
	static bool AllowTransferOptimization ()				{ return false; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

	// Assignment
	ImmediatePtr (const T* o)								{ AssignObject (o);  }
	ImmediatePtr (const ImmediatePtr<T>& o)					{ m_Ptr = o.m_Ptr; }
	ImmediatePtr () 										{ m_Ptr = 0; }

	ImmediatePtr& operator = (const T* o)					{ AssignObject (o); return *this; }

	void SetInstanceID (int instanceID)						{ AssignInstanceID (instanceID); }
	int GetInstanceID ()const
	{
		if ((m_Ptr & 1) == 0 && m_Ptr != 0)
		{
			T* o = (T*)(m_Ptr);
			SInt32 instanceID = o->GetInstanceID ();
			AssertIf (instanceID & 1);
			return instanceID;
		}
		else
			return m_Ptr & (~1);
	}

	inline bool operator == (const T* p)const	{ return GetPtr () == p; }
	inline bool operator != (const T* p)const	{ return GetPtr () != p; }

	inline operator T* () const	{ return GetPtr (); }
	inline T* operator -> () const { T* o = GetPtr (); AssertIf (o == NULL); return o; }
	inline T& operator * () const	{ T* o = GetPtr (); AssertIf (o == NULL); ANALYSIS_ASSUME(o); return *o; }
};

template<typename T> class PtrToType;
template<typename T> class PtrToType<T*>
{
public:
	typedef T value_type;
};

template<class T, class U>
T dynamic_pptr_cast (U* ptr)
{
	typedef typename PtrToType<T>::value_type Type;
	T castedPtr = (T)(ptr);
	if (castedPtr && castedPtr->IsDerivedFrom ( Type::GetClassIDStatic ()))
		return castedPtr;
	else
		return NULL;
}

template<class T, class U>
T dynamic_pptr_cast (const PPtr<U>& ptr)
{
	U* o = ptr;
	return dynamic_pptr_cast<T> (o);
}

template<class T> inline
T dynamic_instanceID_cast (int instanceID)
{
	Object* o = PPtr<Object> (instanceID);
	return dynamic_pptr_cast<T> (o);
}

template<class T, class U>
PPtr<T> assert_pptr_cast (const PPtr<U>& ptr)
{
	#if DEBUGMODE
		U* u = ptr;
		AssertIf (dynamic_pptr_cast<U*> (u) == NULL && u != NULL);
	#endif
	return PPtr<T> (ptr.GetInstanceID ());
}

// Enables boost::mem_fn to use PPtr properly, needed for boost::bind
template<typename T> inline T * get_pointer(PPtr<T> const & p)
{
    return p;
}


enum ObjectCreationMode
{
	// Create the object from the main thread in a perfectly normal way
	kCreateObjectDefault = 0,
	// Create the object from another thread. Might assign an instance ID but will not register with IDToPointer map.
	// Objects created like this, need to call,  AwakeFromLoadThraded, and Object::RegisterInstanceID and AwakeFromLoad (kDidLoadThreaded); from the main thread
	kCreateObjectFromNonMainThread = 1,
	// Create the object and register the instance id but do not lock the object
	// creation mutex because the code calling it already called LockObjectCreation mutex.
	kCreateObjectDefaultNoLock = 2
};


enum AwakeFromLoadMode
{
	// This is the default, usually called from the inspector or various serialization methods
	kDefaultAwakeFromLoad = 0,
	// The object was loaded from disk
	kDidLoadFromDisk = 1 << 0,
	// The object was loaded from a loading thread (in almost all cases through loading from disk asynchronously)
	kDidLoadThreaded = 1 << 1,
	// Object was instantiated and is now gettings it's first Awake function or it was created from code and gets the Awake function called
	kInstantiateOrCreateFromCodeAwakeFromLoad = 1 << 2,
	// GameObject was made active or a component was added to an active game object
	kActivateAwakeFromLoad = 1 << 3,

	kDefaultAwakeFromLoadInvalid = -1
};

class BaseAllocator;

enum ObjectDeleteMode
{
	kUnknownMode = 0
};

class EXPORT_COREMODULE Object
{
	protected:
	virtual ~Object ();

	public:

	Object (MemLabelId label, ObjectCreationMode mode);


	/// By default the destructor might get executed on another thread.
	/// This lets us distribute large level unloads to another thread and thus avoid hiccups.
	/// Some classes need to deallocate resources on the main thread.
	/// You can implement this function to delete the resources.
	///
	/// The destructor will still be called in this case, thus you need to ensure that values are set to NULL.
	/// MainThreadCleanup is only called if the deallocations are done on another thread.
	/// Thus the destructor needs to handle the case where it is not called correctly.
	///
	/// If you override the function return true, since it indicates that the class requires the function to be called.
	/// SA: DoesClassRequireMainThreadDeallocation
	virtual bool MainThreadCleanup ();

	/// To destroy objects use delete_object instead of delete operator
	/// The default way to destroy objects is using the DestroyObject Function, which also destroys the object from it's file
	/// Must be protected by LockObjectCreation / UnlockObjectCreation
	friend void delete_object_internal_step1 (Object* p);
	friend void delete_object_internal_step2 (Object* p);

	/// AwakeFromLoad is called after an object was read using Transfer (Either from disk or a vector)
	/// This means it is called after the inspector has been updated, after it is loaded from disk, after a prefab has been modified or
	/// the animation system has changed values behind your back.
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode)
	{
	#if !UNITY_RELEASE
		m_AwakeCalled		= 1;

		if( awakeMode & kDidLoadThreaded )
			m_AwakeDidLoadThreadedCalled = 1;
	#endif
	}

	virtual void AwakeFromLoadThreaded ()
	{
	#if !UNITY_RELEASE
		m_AwakeThreadedCalled			= 1;
		m_AwakeCalled					= 0;
		m_AwakeDidLoadThreadedCalled	= 0;
	#endif
	}

	/// For Subclasses: Makes sure that persistent variables are correct and if not corrects them
	/// It is called after Prefab propagation, SafeBinaryRead and PropertyEditor changes.
	/// It is called before AwakeFromLoad
	virtual void CheckConsistency () { }

	/// Override Reset in order to setup default values for the Object

	/// The difference between setting up default values in the constructor
	/// and Reset is that Reset is only called when the editor creates a new object
	/// or when the object uses SerializeSafeBinary read.
	/// Thus Reset can be used as a performance optimization for touching variables which are serialized only once during load.
	/// * All variables that are serialized and Reset in the Reset function, do not have to be initialized in the constructor*
	/// Reset functions might get called from different threads during loading, thus they may not derefence other objects, in that case use SmartReset.
	/// You can always rely on that AwakeFromLoad is called after Reset has been called.
	virtual void Reset ()
	{
	#if !UNITY_RELEASE
		m_ResetCalled					= 1;
		m_AwakeCalled					= 0;
		m_AwakeThreadedCalled			= 0;
		m_AwakeDidLoadThreadedCalled	= 0;
	#endif
	}

	// Smart Reset is called when Reset is selected or when AddComponent is called and a new ScriptableObject is created.
	// If you want to for example adjust a collider bounding volume by the renderers mesh, use SmartReset, you can not use Reset for this!
	virtual void SmartReset ()
	{
	#if !UNITY_RELEASE
		m_ResetCalled					= 1;
		m_AwakeCalled					= 0;
		m_AwakeThreadedCalled			= 0;
		m_AwakeDidLoadThreadedCalled	= 0;
	#endif
	}

#if !UNITY_RELEASE
	// use it to check AwakeFromLoad/AwakeFromLoadThreaded/Reset/SmartReset were correctly called
	void CheckCorrectAwakeUsage();

	// hacks to set debug flags in cases you REALLY know what you are doing

	// call when you don't want to Reset in case of object fully inited and don't need to be reset to default state
	// e.g. if you de-serialize object - you don't need reset
	inline void HackSetResetWasCalled() { m_ResetCalled = 1; }

	// call when AwakeFromLoad has some side-effects so you need to postpone that call for indefinite time
	// e.g. AudioClip will try to load sound in AwakeFromLoad, so you better do this only when needed
	inline void HackSetAwakeWasCalled() { m_AwakeCalled = 1; }

	// same as HackSetAwakeWasCalled but for Awake with kDidLoadThreaded param
	inline void HackSetAwakeDidLoadThreadedWasCalled () { m_AwakeDidLoadThreadedCalled = true; }
	#else
	inline void HackSetResetWasCalled() {}
	inline void HackSetAwakeWasCalled() {}
	inline void HackSetAwakeDidLoadThreadedWasCalled() {}
	#endif

	/// Get and set the name
	virtual char const* GetName () const { return ""; };
	virtual void SetName (char const* /*name*/) {  }
	void SetNameCpp (const std::string& name) { SetName(name.c_str()); }

	#if UNITY_EDITOR
	/// Return true if you want the inspector to automatically refresh without SetDirty being called.
	virtual bool HasDebugmodeAutoRefreshInspector () { return false; }
	virtual void WarnInstantiateDisallowed () {}
	#endif

	/// Returns the classID of the class
	static int GetClassIDStatic ()				{ return ClassID (Object); }

	// Is the class sealed (No other class can inherit from it)
	// A sealed class can perform a GetComponent call faster,
	// since it can compare the ClassID directly instead of using the RTTI system.
	static bool IsSealedClass ()				{ return false; }

	/// Returns true if the class is abstract
	static bool IsAbstract ()						{ return true; }

	/// Creates an object of type classID.
	/// if instanceID is 0 a unique id will be generated  if its non 0 the object will have the specified instanceID
	static Object* Produce (int classID, int instanceID = 0, MemLabelId = kMemBaseObject, ObjectCreationMode mode = kCreateObjectDefault);


	// Static initializa and destroy for BaseObject
	static void StaticInitialize();
	static void StaticDestroy();

	/// Registers instance id with IDToPointerMap
	/// useful for thread loading with delayed activation from main thread
	/// Can only be called from main thead
	static void RegisterInstanceID (Object* obj);
	static void RegisterInstanceIDNoLock (Object* obj);

	/// Allocates new instanceID and registers it with IDToPointerMap
	/// Can only be called from main thead
	static Object* AllocateAndAssignInstanceID (Object* obj);
	static Object* AllocateAndAssignInstanceIDNoLock (Object* obj);

	#if UNITY_EDITOR
	virtual void CloneAdditionalEditorProperties (Object& /*source*/) {  }

	/// Can assign variable allows you to do additional type checking when assiging a variable
	/// in the property	inspector.
	/// Eg. MonoBehaviours checks if a monobehaviour can be assigned based on the actual Mono class
	virtual bool CanAssignMonoVariable (const char* /*property*/, Object* /*object*/) { return false; }

	#endif

	virtual bool ShouldIgnoreInGarbageDependencyTracking ()				{ return false; }

	/// Gets the class ID
	ClassIDType GetClassID () const										{ Assert(m_CachedClassID != 0); return (ClassIDType)m_CachedClassID; }
	/// Gets the instance ID
	int			GetInstanceID () const									{ AssertIf(m_InstanceID == 0); return m_InstanceID; }
	bool		IsInstanceIDCreated () const							{ return m_InstanceID != 0; }

	/// Is this instance derived from compareClassID
	bool		IsDerivedFrom (int compareClassID)const					{ return IsDerivedFromClassID (GetClassID (), compareClassID); }

	#if UNITY_EDITOR

	/// Has this object been synced with the PersistentManager
	bool IsPersistentDirty () const						{ return m_DirtyIndex != 0; }

	void SetPersistentDirtyIndex (UInt32 dirtyIndex);
	UInt32 GetPersistentDirtyIndex () { return m_DirtyIndex; }

	////@TODO: Rename this to SetPersistentDirty

	/// Whenever variables that are being serialized in Transfer change, SetDirty () should be called
	/// This will allow tracking of objects that have changed since the last saving to disk or over the network
	void SetDirty ();

	/// This method can be called if you need to unload an object from memory even if it's dirty.
	void ClearPersistentDirty ();

	// Callback support for callbacks when SetDirty is called
	typedef void ObjectDirtyCallbackFunction (Object* ptr);
	static void RegisterDirtyCallback (ObjectDirtyCallbackFunction* callback);
	static ObjectDirtyCallbackFunction* GetDirtyCallback ();

	void SetFileIDHint (LocalIdentifierInFileType hint) { m_FileIDHint = hint; }
	LocalIdentifierInFileType GetFileIDHint () const { return m_FileIDHint; }

	#else
	void SetDirty () {  }
	void ClearPersistentDirty () { }

	#endif


	// The name of the class
	const std::string& GetClassName () const;

	enum
	{
		kHideInHierarchy = 1 << 0,
		kHideInspector = 1 << 1,
		kDontSave = 1 << 2,
		kNotEditable = 1 << 3,
		kHideAndDontSave = kDontSave | kHideInHierarchy | kNotEditable
	};

	int GetHideFlags () const { return m_HideFlags; }
	bool TestHideFlag (int mask) const { return (m_HideFlags & mask) == mask; }
	bool TestHideFlagAny (int mask) const { return (m_HideFlags & mask) != 0; }

	virtual void SetHideFlags (int flags) { m_HideFlags = flags; }
	void SetHideFlagsObjectOnly (int flags) { Assert(flags < (1 << kHideFlagsBits)); m_HideFlags = flags; }

	/// You must document all usage here in order to provide clear overview and avoid overlaps
	/// - Transform root calculation for animation component, when binding animation states (Runtime only)
	void SetTemporaryFlags (int flags) { Assert(flags < (1 << kTemporaryFlagsBits));  m_TemporaryFlags = flags; }
	int GetTemporaryFlags () const { return m_TemporaryFlags; }
#if UNITY_WINRT
	/// Used by WinRT's GarbageCollectSharedAssets
	void SetTemporaryUnusedAssetsFlags (int flags) { m_TemporaryUnusedAssetsFlags = flags; }
	int GetTemporaryUnusedAssetsFlags () const { return m_TemporaryUnusedAssetsFlags; }
#endif

#if ENABLE_SCRIPTING
	int GetGCHandle () const { return m_MonoReference; }
#endif

	/// Overall memory allocated for this object. Should calculate any memory allocated by other subystems as well.
	/// For example if OpenGL allocates memory for a texture it must return how much memory we "think" OpenGL will allocate for the texture.
	virtual int GetRuntimeMemorySize () const;

	/// Is this object persistent?
	bool IsPersistent () const { return m_IsPersistent; }

	typedef InstanceIdToObjectPtrHashMap IDToPointerMap;

	/// How many objects are there in memory?
	static IDToPointerMap::size_type GetLoadedObjectCount ()				{ return ms_IDToPointer->size (); }

	// Finds the pointer to the object referenced by instanceID (NULL if none found in memory)
	static Object* IDToPointer (int inInstanceID);
	static Object* IDToPointerThreadSafe (int inInstanceID);

	/// This function may not be called unless you use LockObjectCreation / UnlockObjectCreation from another thread first...
	/// If you don't know 100% what you are doing use: IDToPointerThreadSafe instead
	static Object* IDToPointerNoThreadCheck (int inInstanceID);

	/// Finds out if classID is derived from compareClassID
	static bool IsDerivedFromClassID (int classID, int derivedFromClassID);

	/// Returns the super Class ID of classID.
	/// if classID doesnt have any super Class	it will return ClassID (Object)
	static int GetSuperClassID (int classID);

	/// Returns all classIDs that are derived from ClassID
	static void FindAllDerivedClasses (int classID, std::vector<SInt32>* allDerivedClasses, bool returnOnlyNonAbstractClasses = true);
	/// Returns how many objects are derived from classID
	/// If allDerivedObjects != NULL, adds all derived object instanceIDs to the container
	static int FindAllDerivedObjects (int classID, std::vector<SInt32>* derivedObjects, bool sorted = false);

	static int FindObjectsOfType (int classID, std::vector<Object*>* derivedObjects, bool sorted = false);
	template<class T>
	static int FindObjectsOfType (std::vector<T*>* derivedObjects)
	{
		std::vector<Object*>* casted = reinterpret_cast<std::vector<Object*>*> (derivedObjects);
		return FindObjectsOfType (T::GetClassIDStatic (), casted);
	}

	static int FindObjectsOfType (int classID, dynamic_array<Object*>* derivedObjects, bool sorted = false);
	template<class T>
	static int FindObjectsOfType (dynamic_array<T*>* derivedObjects)
	{
		dynamic_array<Object*>* casted = reinterpret_cast<dynamic_array<Object*>*> (derivedObjects);
		return FindObjectsOfType (T::GetClassIDStatic (), casted);
	}

	template<class T>
	static int FindObjectsOfTypeSorted (std::vector<T*>* derivedObjects)
	{
		std::vector<Object*>* casted = reinterpret_cast<std::vector<Object*>*> (derivedObjects);
		return FindObjectsOfType (T::GetClassIDStatic (), casted, true);
	}



	/// Get the class name from the classID
	static const std::string& ClassIDToString (int classID);
	/// Get the classID from the class name, returns -1 if no classID was found
	static int StringToClassID (const std::string& classString);
	static int StringToClassIDCaseInsensitive (const std::string& classString);
	static int StringToClassID (const char* classString);

	/// Callback support for callbacks when an object is destroyed
	typedef void ObjectDestroyCallbackFunction (int instanceID);
	static void RegisterDestroyedCallback (ObjectDestroyCallbackFunction* callback);

	/// Sets up the rtti for all classes that are derived from Object and
	/// use the macro IMPLEMENT_CLASS or IMPLEMENT_CLASS_HAS_INIT
	/// Calls the static function InitializeClass on every class that used
	/// IMPLEMENT_CLASS_HAS_INIT instead of IMPLEMENT_CLASS
	static void InitializeAllClasses ();
	static void CallInitializeClassEarly();
	static void CallInitializeClass();
	static void CallPostInitializeClass();

	static void CleanupAllClasses ();

	/// Checks if an array of instance id's are loaded.
	/// If an instanceID is loaded it is set to 0.
	static void CheckInstanceIDsLoaded (SInt32* instanceIDs, int size);


	typedef Object* FactoryFunction (MemLabelId label, ObjectCreationMode mode);
	struct RTTI
	{
		RTTI*                    base;// super rtti class
		Object::FactoryFunction* factory;// the factory function of the class
		int                      classID;// the class ID of the class
		std::string              className;// the name of the class
		int                      size;// sizeof (Class)
		bool                     isAbstract;// is the class Abstract?
	};

	/// Returns the RTTI information for a classID
	static RTTI*  ClassIDToRTTI (int classID);

	MemLabelId GetMemoryLabel () const;

	static void DoneLoadingManagers ();

	static IDToPointerMap& GetIDToPointerMapInternal () { return *ms_IDToPointer; }

	virtual int GetClassIDVirtualInternal () const { AssertString("Bad"); return ClassID(Object); }
	void PreCleanupObject ();

	// Generic Event callback support.
	typedef void EventCallback (void* userData, void* sender, int eventType);

	void AddEvent (EventCallback* callback, void* userData);
	void RemoveEvent (EventCallback* callback, void* userData);
	bool HasEvent (EventCallback* callback, const void* userData) const;
	void InvokeEvent (int eventType);

private:

	static	UInt32*				ms_IsDerivedFromBitMap;
	static	unsigned               ms_MaxClassID;
	static	IDToPointerMap*         ms_IDToPointer;
	static	UInt32* ms_ClassIDMask;
	static	UInt32* ms_ClassIsDerivedFrom;

	SInt32                m_InstanceID;

	enum Bits
	{
		kMemLabelBits = 13,
		kIsRootOwnerBits = 1,
		kTemporaryFlagsBits = 1,
		kHideFlagsBits = 4,
		kIsPersistentBits = 1,
		kCachedClassIDBits = 12
	};


	UInt32                m_MemLabel      : kMemLabelBits;       // 13 bits
	UInt32                m_IsRootOwner   : kIsRootOwnerBits;    // 14 bits
	UInt32                m_TemporaryFlags: kTemporaryFlagsBits; // 15 bits
	UInt32                m_HideFlags     : kHideFlagsBits;      // 19 bits
	UInt32                m_IsPersistent  : kIsPersistentBits;   // 20 bits
	UInt32				  m_CachedClassID : kCachedClassIDBits;  // 32 bits

	EventEntry*           m_EventIndex;


	#if !UNITY_RELEASE
	UInt32				  m_DEBUGCLASSID:16;
	UInt32                m_AwakeCalled:1;
	UInt32				  m_ResetCalled:1;
	UInt32				  m_AwakeThreadedCalled:1;
	UInt32				  m_AwakeDidLoadThreadedCalled:1;
	#endif

	#if ENABLE_MONO
	UInt32                m_MonoReference;
	#elif UNITY_FLASH
	SInt32				  m_MonoReference;
	#elif UNITY_WINRT
	SInt32				  m_MonoReference;
	UInt32				  m_TemporaryUnusedAssetsFlags;
	#endif
	#if ENABLE_SCRIPTING
	ScriptingObjectPtr      m_ScriptingObjectPointer;
	#endif

	#if UNITY_EDITOR
	UInt32                m_DirtyIndex;
	LocalIdentifierInFileType m_FileIDHint;
	#endif

	public:

	#if ENABLE_SCRIPTING
	void SetupWeakHandle ();
	bool RevertWeakHandle ();

	void SetCachedScriptingObject (ScriptingObjectPtr cachedPointer);
	ScriptingObjectPtr GetCachedScriptingObject () { return m_ScriptingObjectPointer; }
	#endif

private:

	static void CalculateCachedClassID (Object* obj);
	static void InsertObjectInMap (Object* obj);

	void SetIsPersistent (bool p);

	Object (const Object& o);					// Disallow copy constructor
	Object& operator = (const Object& o);	// Disallow assignment

	void SetInstanceID (int inID)				{ m_InstanceID = inID; }

	protected:

	static void RegisterClass (int inClassID, int inBaseClass, const std::string& inName, int size, FactoryFunction* inFunc, bool isAbstract);

	static Object* PRODUCE (MemLabelId /*label*/, ObjectCreationMode /*mode*/)						{ AssertString ("Can't produce abstract class"); return NULL; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

	public:

	/// Returns whether or not the class needs one typetree per object, not per classID
	/// Having a per object typetree makes serialization considerably slower because safeBinaryTransfer is always used
	/// Since no TypeTree can be generated before reading the object.
	/// The File size will also increase because the typetree is not shared among the same classes.
	/// It is used for example in PythonBehaviour
	/// Also for one class you have to always returns true or always false.
	virtual bool GetNeedsPerObjectTypeTree () const { return false; }

	// Sets up RTTI, the object factory (Produce) and string <-> classID
	// conversion. RegisterClass() has to be called once for every class
	// derived from object, before any Objects are allocated
	static void RegisterClass ();

	// Required by serialization
	virtual void VirtualRedirectTransfer (StreamedBinaryWrite<false>&){ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<false>&) { AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualRedirectTransfer (RemapPPtrTransfer&)	{ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualRedirectTransfer (ProxyTransfer&)		{ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
#if SUPPORT_SERIALIZED_TYPETREES
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<true>&) { AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualRedirectTransfer (SafeBinaryRead&)		{ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualRedirectTransfer (StreamedBinaryWrite<true>&){ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
#endif
#if SUPPORT_TEXT_SERIALIZATION
	virtual void VirtualRedirectTransfer (YAMLRead&)	{ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualRedirectTransfer (YAMLWrite&)	{ AssertString ("Serialization not implemented for type " + Object::ClassIDToString (GetClassID ())); }
	virtual void VirtualStrippedRedirectTransfer (YAMLWrite& t) { VirtualRedirectTransfer(t); }
#endif
#if ENABLE_SERIALIZATION_BY_CODEGENERATION
	virtual void DoLivenessCheck (RemapPPtrTransfer&)	{ AssertString ("DoLivenessCheck not implemented for type " + Object::ClassIDToString (GetClassID ())); }
#endif
	static const char* GetClassStringStatic (){ return "Object"; }
	static const char* GetPPtrTypeString (){ return "PPtr<Object>"; }

	friend class PersistentManager;
	friend class SerializedFile;
};

struct LocalSerializedObjectIdentifier
{
	SInt32 localSerializedFileIndex;
	#if LOCAL_IDENTIFIER_IN_FILE_SIZE == 64
	UInt64 localIdentifierInFile;
	#else
	SInt32 localIdentifierInFile;
	#endif

	LocalSerializedObjectIdentifier()
	{
		localIdentifierInFile = 0;
		localSerializedFileIndex = 0;
	}
};

typedef void InstanceIDResolveCallback (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, void* context);
void SetInstanceIDResolveCallback (InstanceIDResolveCallback* callback, const void* context = NULL);

void EXPORT_COREMODULE InstanceIDToLocalSerializedObjectIdentifier (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier);
void EXPORT_COREMODULE LocalSerializedObjectIdentifierToInstanceID (const LocalSerializedObjectIdentifier& fileID, SInt32& memoryID);
EXPORT_COREMODULE Object* ReadObjectFromPersistentManager (int instanceID);

#if THREADED_LOADING
EXPORT_COREMODULE Object* InstanceIDToObjectThreadSafe (int instanceID);
#else
#	define InstanceIDToObjectThreadSafe  PPtr<Object>
#endif

// This is used by the build game process. When writing for game release
// we want to null all pptrs that can't be loaded anymore.
// And when building default resources (culls all external references)
enum { kWriteNULLWhenNotLoaded = 1 << 0, kConstrainedExternalReferences = 1 << 1 };
void SetSerializeWritePPtrFlags (int flags, const std::set<string>& paths);

void EXPORT_COREMODULE SetDisableImmediateDestruction (bool disable);
bool EXPORT_COREMODULE GetDisableImmediateDestruction ();


/// Returns if the object can possibly be loaded or is already in memory, without actually performing the loading.
bool IsObjectAvailable (int instanceID);

//Implementation
#if UNITY_RELEASE
#	if UNITY_PS3
	__attribute__((always_inline)) inline Object* Object::IDToPointer (int inInstanceID)
#else
	inline Object* Object::IDToPointer (int inInstanceID)
#	endif
{
	if( !ms_IDToPointer ) return NULL;
	IDToPointerMap::const_iterator i = ms_IDToPointer->find (inInstanceID);
	if (i != ms_IDToPointer->end ())
		return i->second;
	else
		return NULL;
}
#endif

template<class T>
inline void PPtr<T>::AssignObject (const Object* o)
{
	if (o == NULL)
		m_InstanceID = 0;
	else
		m_InstanceID = o->GetInstanceID ();
	#if !UNITY_RELEASE
	m_DEBUGPtr = (T*) (o);
	#endif
}

template<class T> inline
PPtr<T>::operator T* () const
{
	if (GetInstanceID () == 0)
 		return NULL;

	Object* temp = Object::IDToPointer (GetInstanceID ());
	if (temp == NULL)
		temp = ReadObjectFromPersistentManager (GetInstanceID ());

	#if !UNITY_RELEASE
	m_DEBUGPtr = (T*) (temp);
	#endif

	#if DEBUGMODE || UNITY_EDITOR
		T* casted = dynamic_pptr_cast<T*> (temp);
		if (casted == temp)
			return casted;
		else
		{
			ErrorStringObject ("PPtr cast failed when dereferencing! Casting from " + temp->GetClassName () + " to " + T::GetClassStringStatic () + "!", temp);
			return casted;
		}
	#else
		return static_cast<T*> (temp);
	#endif
}

template<class T> inline
T* PPtr<T>::operator -> () const
{
	Object* temp = Object::IDToPointer (GetInstanceID ());
	if (temp == NULL)
		temp = ReadObjectFromPersistentManager (GetInstanceID ());

	#if !UNITY_RELEASE
		m_DEBUGPtr = (T*) (temp);
	#endif

	#if DEBUGMODE || !GAMERELEASE
		T* casted = dynamic_pptr_cast<T*> (temp);
		if (casted != NULL)
			return casted;
		else
		{
			if (temp != NULL)
			{
				ErrorStringObject ("PPtr cast failed when dereferencing! Casting from " + temp->GetClassName () + " to " + T::GetClassStringStatic () + "!", temp);
			}
			else
			{
				ErrorString ("Dereferencing NULL PPtr!");
			}
			return casted;
		}
	#else
		return static_cast<T*> (temp);
	#endif
}

template<class T> inline
T& PPtr<T>::operator * () const
{
	Object* temp = Object::IDToPointer (GetInstanceID ());
	if (temp == NULL)
		temp = ReadObjectFromPersistentManager (GetInstanceID ());

	#if !UNITY_RELEASE
		m_DEBUGPtr = (T*) (temp);
	#endif

	#if DEBUGMODE || !GAMERELEASE
		T* casted = dynamic_pptr_cast<T*> (temp);
		if (casted != NULL)
			return *casted;
		else
		{
			if (temp != NULL)
			{
				ErrorStringObject ("PPtr cast failed when dereferencing! Casting from " + temp->GetClassName () + " to " + T::GetClassStringStatic () + "!", temp);
			}
			else
			{
				ErrorString ("Dereferencing NULL PPtr!");
			}
			ANALYSIS_ASSUME(casted);
			return *casted;
		}
	#else
		return *static_cast<T*> (temp);
	#endif
}

template<class T> inline
bool PPtr<T>::IsNull() const
{
	T* casted = *this;
	return casted == NULL;
}

template<class T> inline
bool PPtr<T>::IsValid() const
{
	T* casted = *this;
	return casted != NULL;
}

template<class T>
string PPtr<T>::s_TypeString;

template<class T> inline
const char* PPtr<T>::GetTypeString ()
{
	return T::GetPPtrTypeString ();
}

template<class T>
template<class TransferFunction> inline
void PPtr<T>::Transfer (TransferFunction& transfer)
{
	LocalSerializedObjectIdentifier localIdentifier;

	if (transfer.NeedsInstanceIDRemapping ())
	{
		AssertIf (!transfer.IsWriting () && !transfer.IsReading ());

		if (transfer.IsReading ())
		{
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
			LocalSerializedObjectIdentifierToInstanceID (localIdentifier, m_InstanceID);
		}
		else if (transfer.IsWriting ())
		{
			InstanceIDToLocalSerializedObjectIdentifier (m_InstanceID, localIdentifier);
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
		}
		else
		{
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
		}
	}
	else
	{
		transfer.Transfer (m_InstanceID, "m_FileID", kHideInEditorMask);
		transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
	}
}

template<class T> inline
bool ImmediatePtr<T>::IsLoaded () const
{
	if (m_Ptr & 1)
	{
		return Object::IDToPointer(m_Ptr & (~1)) != NULL;
	}
	else
	{
		AssertIf(Object::IDToPointer(GetInstanceID()) == NULL);
		return true;
	}
}
template<class T>
string ImmediatePtr<T>::s_TypeString;

template<class T> inline
const char* ImmediatePtr<T>::GetTypeString ()
{
	if(s_TypeString.empty())
	{
		SET_ALLOC_OWNER(NULL);
		s_TypeString = string ("PPtr<") + T::GetClassStringStatic () + ">";
	}
	return s_TypeString.c_str ();
}

template<class T>
template<class TransferFunction> inline
void ImmediatePtr<T>::Transfer (TransferFunction& transfer)
{
	LocalSerializedObjectIdentifier localIdentifier;

	if (transfer.NeedsInstanceIDRemapping ())
	{
		AssertIf (!transfer.IsWriting () && !transfer.IsReading ());

		if (transfer.IsReading ())
		{
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
			SInt32 instanceID;
			LocalSerializedObjectIdentifierToInstanceID (localIdentifier, instanceID);
			AssignInstanceID (instanceID);
		}
		else if (transfer.IsWriting ())
		{
			InstanceIDToLocalSerializedObjectIdentifier (GetInstanceID (), localIdentifier);
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
		}
		else
		{
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
		}
	}
	else
	{
		if (transfer.IsReading ())
		{
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
			SetInstanceID (localIdentifier.localSerializedFileIndex);
		}
		else if (transfer.IsWriting ())
		{
			localIdentifier.localSerializedFileIndex = GetInstanceID ();
			localIdentifier.localIdentifierInFile = 0;
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
			AssertIf (localIdentifier.localSerializedFileIndex != GetInstanceID ());
		}
		else
		{
			transfer.Transfer (localIdentifier.localSerializedFileIndex, "m_FileID", kHideInEditorMask);
			transfer.Transfer (localIdentifier.localIdentifierInFile, "m_PathID", kHideInEditorMask);
		}
	}
}
#if UNITY_PS3
#define USE_NEW_IS_DERIVED_FROM 1
#else
#define USE_NEW_IS_DERIVED_FROM 0
#endif

#if !USE_NEW_IS_DERIVED_FROM
#if UNITY_RELEASE
inline bool Object::IsDerivedFromClassID (int classID, int compareClass)
{
	AssertIf (classID >= ms_MaxClassID || classID < 0);
	AssertIf (compareClass >= ms_MaxClassID || compareClass < 0);
	int index = classID * ms_MaxClassID + compareClass;
	int block = index >> 5;
	int bit = index - (block << 5);
	return (ms_IsDerivedFromBitMap[block]) & (1 << bit);
}
#endif
#else
#define CLASS_ID_MASK_BITS 4
#define CLASS_ID_MASK_IDS 0x0fffffff
inline bool Object::IsDerivedFromClassID(int klass, int base)
{
	int klassId = ms_ClassIsDerivedFrom[klass];
	int baseId = ms_ClassIsDerivedFrom[base];
	int mask = ms_ClassIDMask[ 0xf&(baseId >> (32-CLASS_ID_MASK_BITS))];
	return (klassId&mask) == (baseId&CLASS_ID_MASK_IDS);
}
#endif

void LockObjectCreation ();
void UnlockObjectCreation ();

// Destroys a Object removing from memory and disk when needed.
// Might load the object as part of destruction which is probably unwanted.
// @TODO: Refactor code to not do that
void EXPORT_COREMODULE DestroySingleObject (Object* o);
void UnloadObject (Object* o);

/// Destroys the object if it is loaded. (Will not load the object from disk if it is not loaded at the moment)
/// Will remove it from any remapping tables
/// Will not removed it from the actual serialized file, with the assumption that the file will be unloaded from disk later.
void DestroyWithoutLoadingButDontDestroyFromFile (int instanceID);

#if DEBUGMODE
typedef std::set<int, std::less<int>, STL_ALLOCATOR(kMemPermanent,int) > VerifyRegisteredClass;
void EXPORT_COREMODULE AddVerifyClassRegistration (int classID);
typedef std::set<int, std::less<int>, STL_ALLOCATOR(kMemBaseObject, int) > RegisteredClassSet;
const RegisteredClassSet& GetVerifyClassRegistration ();
#endif

/// Helper to create object correctly from code. Will call Reset and AwakeFromLoad
template <typename T> T* CreateObjectFromCode( AwakeFromLoadMode awakeMode=kInstantiateOrCreateFromCodeAwakeFromLoad, MemLabelId label = kMemBaseObject )
{
	Assert(Object::ClassIDToRTTI(T::GetClassIDStatic()) != NULL);
	T* obj = NEW_OBJECT_USING_MEMLABEL(T, label);
	SET_ALLOC_OWNER(obj);
	obj->Reset();
	obj->AwakeFromLoad(awakeMode);
	return obj;
}

template<typename T>
inline T* ResetAndAwake (T* object)
{
	object->Reset();
	object->AwakeFromLoad (kDefaultAwakeFromLoad);
	return object;
}

void delete_object_internal (Object* p);
void delete_object_internal_step1 (Object* object);
void delete_object_internal_step2 (Object* object);

#endif
