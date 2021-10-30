#pragma once

#include "Runtime/Scripting/ScriptingUtility.h"

#if 1 //!UNITY_WINRT
#if ENABLE_MONO
extern int* s_MonoDomainContainer;
#endif
template<class T>
struct ScriptingObjectWithIntPtrField
{
	ScriptingObjectPtr object;

	ScriptingObjectWithIntPtrField(ScriptingObjectPtr o) : object(o) {}

	T& operator * () const
	{
		return GetReference ();
	}
	
	T* operator -> () const
	{
		return &GetReference ();
	}

	operator T* () const
	{
		return GetPtr ();
	}
	
	inline T& GetReference () const
	{
		void* nativePointer = GetPtr();

		if (nativePointer == NULL)
			Scripting::RaiseNullException ("");
		
		return *reinterpret_cast<T*> (nativePointer);
	}

	typedef void ExecuteOnManagedFinalizer(void* obj);

	inline void SetPtr(T* ptr, ExecuteOnManagedFinalizer* eomf = NULL)
	{
	#if ENABLE_MONO
		Assert(ptr == NULL || GET_CURRENT_ALLOC_ROOT_HEADER() == NULL
			|| GET_CURRENT_ALLOC_ROOT_HEADER() == GET_ALLOC_HEADER(s_MonoDomainContainer, kMemMono));
		ExtractMonoObjectData<T*>(object) = ptr;
		(void)eomf;
	#elif UNITY_FLASH
		
		__asm __volatile__("obj_g0 = marshallmap.getObjectWithId(%0);"::"r"(object));
		__asm __volatile__("obj_g0.m_Ptr = %0;"::"r"(ptr));

		//We only insert this if we actually have a way to clean it...if we don't then there's no point in inserting it in the finalizer map.
		if(eomf == NULL)
			return;
			
		__asm __volatile__("finalizePtrsMap[%0] = %1;" : : "r"(ptr),"r"(eomf));
		__asm __volatile__("finalizeMap[obj_g0] = %0;"::"r"(ptr));
	#elif UNITY_WINRT
		static BridgeInterface::IMarshalling^ marshaller = s_WinRTBridge->Marshalling;
		marshaller->MarshalNativePtrIntoFirstFieldGC(object.GetHandle(), (int)ptr);
		//typedef void (__stdcall *TMarshalNativePtrIntoFirstField)(ScriptingObjectPtr, int);
		//static TMarshalNativePtrIntoFirstField call = (TMarshalNativePtrIntoFirstField)GetWinRTMarshalling()->GetMarshalNativePtrIntoFirstFieldDelegatePtr();
		//call(object, (int)ptr);
	#endif	
	}

	inline T* GetPtr() const
	{
		if (!object)
			return NULL;
		
	#if ENABLE_MONO
		return ExtractMonoObjectData<T*>(object);
	#elif UNITY_FLASH
		T* result;
		__asm __volatile__("%0 = marshallmap.getObjectWithId(%1).m_Ptr;" : "=r"(result) : "r"(object));
		return result;
	#elif UNITY_WINRT
		static BridgeInterface::IMarshalling^ marshaller = s_WinRTBridge->Marshalling;
		return (T*)marshaller->MarshalFirstFieldIntoNativePtrGC(object.GetHandle());
		//typedef int (__stdcall *TMarshalFirstFieldIntoNativePtr)(ScriptingObjectPtr);
		//static TMarshalFirstFieldIntoNativePtr call = (TMarshalFirstFieldIntoNativePtr)GetWinRTMarshalling()->GetMarshalFirstFieldIntoNativePtrDelegatePtr();
		//return (T*)call(object);
	#endif	
	}

	inline ScriptingObjectPtr GetScriptingObject()
	{
		return object;
	}
};
#else
// THIS WON'T WORK because cachedPtr doesn't derive from Object, thus we cannot extract ScriptingObjectPtr from cachedPtr
template<class T>
struct ScriptingObjectWithIntPtrField
{
	T* cachedPtr;

	ScriptingObjectWithIntPtrField(void* o) : cachedPtr((T*)o) {}

	T& operator * () const
	{
		return *cachedPtr;
	}
	
	T* operator -> () const
	{
		return cachedPtr;
	}

	operator T* () const
	{
		return cachedPtr;
	}
	
	inline T& GetReference () const
	{
		return *cachedPtr;
	}

	inline T* GetPtr() const
	{
		return cachedPtr;
	}

	typedef void ExecuteOnManagedFinalizer(void* obj);

	inline void SetPtr(T* ptr, ExecuteOnManagedFinalizer* eomf = NULL)
	{
		if (cachedPtr != NULL)
		{
			s_WinRTBridge->Marshalling->MarshalNativePtrIntoFirstField(ObjectToScriptingObjectImpl(cachedPtr), (int)ptr);
		}
	}
};
#endif
