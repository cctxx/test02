#pragma once

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Scripting.h"
#include "ScriptingUtility.h"//We need to go back and remove this.

template<class T>
struct ScriptingObjectOfType
{	
private:
	ScriptingObjectPtr object;
public:
	ScriptingObjectOfType(ScriptingObjectPtr object)
	{
		this->object = object;
	}
	
	T& GetReference () const
	{
		T* ptr = GetPtr();
		
		if (ptr != NULL)
			return *ptr;
		
		Scripting::RaiseNullExceptionObject (object);
		return *(T*)NULL;
	}
	
	T* GetPtr () const
	{
		if (IsNullPtr())
			return NULL;
		
		void* cachedPtr = GetCachedPtr();
		if (cachedPtr != NULL)
		{
			AssertIf(reinterpret_cast<Object*> (cachedPtr)->GetInstanceID() != GetInstanceID());
			return (T*)cachedPtr;
		}

		T* temp = dynamic_instanceID_cast<T*> (GetInstanceID());
		return temp;
	}

    bool IsValidObjectReference () const
	{
		if (IsNullPtr())
			return false;
		
		void* cachedPtr = GetCachedPtr();
		if (cachedPtr != NULL)
		{
			AssertIf(reinterpret_cast<Object*> (cachedPtr)->GetInstanceID() != GetInstanceID());
			return (T*)cachedPtr;
		}

		T* temp = dynamic_instanceID_cast<T*> (GetInstanceID());
        if (temp == NULL)
            return false;
        
        // MonoBheaviours are only allowed to be the instance themselves (cached pointer)
        // otherwise they are dangling references who happen to have the same instanceID.
        // Thus not a valid reference.
        if (temp->GetClassID() == ClassID(MonoBehaviour))
            return false;
        
        return true;
	}

    
	operator T* () const
	{
		return GetPtr ();
	}
	
	operator PPtr<T> () const
	{
		if (IsNullPtr())
			return PPtr<T> ();
		
		return PPtr<T> (GetInstanceID());
	}
	
	T& operator * () const
	{
		return GetReference ();
	}
	
	T* operator -> () const
	{
		return &GetReference ();
	}
	
	bool IsNullPtr() const
	{
		return object == SCRIPTING_NULL;
	}

	inline int GetInstanceID() const
	{
		return Scripting::GetInstanceIDFromScriptingWrapper(object);
	}

	inline void SetCachedPtr (void* cachedPtr)
	{
		Scripting::SetCachedPtrOnScriptingWrapper(object, cachedPtr);
	}

	inline void SetInstanceID (int instanceID)
	{
		Scripting::SetInstanceIDOnScriptingWrapper(object, instanceID);
	}

	inline void* GetCachedPtr() const
	{
		return Scripting::GetCachedPtrFromScriptingWrapper(object);
	}

	#if MONO_QUALITY_ERRORS
	inline void SetError (ScriptingStringPtr error)
	{
		Scripting::SetErrorOnScriptingWrapper(object, error);
	}
	#endif

#if UNITY_WINRT
	ScriptingObjectOfType<T>& operator = (decltype(__nullptr))
	{
		object = nullptr;
		return *this;
	}
#endif
	
	inline ScriptingObjectPtr GetScriptingObject() const
	{
		return object;
	}
};




