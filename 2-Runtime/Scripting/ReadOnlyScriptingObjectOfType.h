#pragma once

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Scripting.h"

#define SUPPORT_DIRECT_CACHEDPTR_PASSING_FOR_READONLY_OBJECTS UNITY_WINRT

#if !SUPPORT_DIRECT_CACHEDPTR_PASSING_FOR_READONLY_OBJECTS
template<class T>
struct ReadOnlyScriptingObjectOfType
{	
private:
	ScriptingObjectPtr object;
public:
	ReadOnlyScriptingObjectOfType(ScriptingObjectPtr object)
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

	T& operator * () const
	{
		return GetReference ();
	}
		
	operator T* () const
	{
		return GetPtr ();
	}
		
	T* operator -> () const
	{
		return &GetReference ();
	}
	
	bool IsNull() const
	{
		return object == SCRIPTING_NULL;
	}

	operator PPtr<T> () const
	{
		if (IsNull())
			return PPtr<T> ();
		
		return PPtr<T> (GetInstanceID());
	}

private:
#if UNITY_EDITOR
public:
	// ToDo: Fix this later, ask TomasD or Lucas
	inline ScriptingObjectPtr GetScriptingObject() const
	{
		return object;
	}
#endif

	inline int GetInstanceID() const
	{
		return Scripting::GetInstanceIDFromScriptingWrapper(object);
	}
	T* GetPtr () const
	{
		if (IsNull())
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

	inline void* GetCachedPtr() const
	{
		return Scripting::GetCachedPtrFromScriptingWrapper(object);
	}
};

#else

template<class T>
struct ReadOnlyScriptingObjectOfType
{
private:
	T* cachedPtr;
public:
	ReadOnlyScriptingObjectOfType(void* p)
	{
		this->cachedPtr = (T*)p;
	}

	T& GetReference () const
	{
		if (cachedPtr != NULL)
			return *(T*)cachedPtr;
		
		Scripting::RaiseNullExceptionObject (SCRIPTING_NULL);
		return *(T*)NULL;
	}

	T& operator * () const
	{
		return GetReference ();
	}

	operator T* () const
	{
		return cachedPtr;
	}

	T* operator -> () const
	{
		return &GetReference ();
	}

	bool IsNull() const
	{
		return cachedPtr == NULL;
	}

	operator PPtr<T> () const
	{
		if (IsNull())
			return PPtr<T> ();
		
		return PPtr<T> (cachedPtr->GetInstanceID());
	}

};

#endif




