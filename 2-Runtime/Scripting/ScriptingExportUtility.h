#ifndef _SCRIPTINGEXPORTUTILITY_H_
#define _SCRIPTINGEXPORTUTILITY_H_

#if ENABLE_SCRIPTING

#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ICallString.h"
#include "Runtime/Scripting/Scripting.h"


template<class T>
inline
ScriptingArrayPtr CreateScriptingArray (ScriptingClassPtr klass, int count)
{
#if ENABLE_MONO
	//here to force template argument being used on caller, so the callsite will be compatible
	//with the flash version of CreateScriptingArray, which actually needs to know what will be in the array beforehand.
	//TODO: find a cleaner way of enforcing usage:
#	if DEBUGMODE
		int nativeSize = sizeof(T);
		int monoSize = mono_class_array_element_size(klass);
		DebugAssert(nativeSize == monoSize || !count);
		(void)monoSize; // silence unused variable warning
		nativeSize++;
#	endif
	return mono_array_new(mono_domain_get(),klass,count);

#elif UNITY_FLASH 
	UInt8* memoryblob = (UInt8*) malloc(sizeof(T) * count + sizeof(int));
	*(int*)memoryblob = count;
	return (ScriptingArrayPtr) memoryblob;
#elif UNITY_WINRT
	return ScriptingArrayPtr(GetWinRTObjectInstantiation()->CreateScriptingArrayGC(klass->metroType, count));
#endif
}

template<class T>
inline
ScriptingArrayPtr CreateScriptingArray2D (ScriptingClassPtr klass, int count1, int count2)
{
#if ENABLE_MONO
	return mono_array_new_2d (count1, count2, klass);
#elif UNITY_FLASH 
	FatalErrorMsg("ToDo:CreateScriptingArray2D");
#elif UNITY_WINRT
	return ScriptingArrayPtr(GetWinRTObjectInstantiation()->CreateScriptingArray2DGC(klass->metroType, count1, count2));
#endif
}
template<class T>
inline
ScriptingArrayPtr CreateScriptingArray3D (ScriptingClassPtr klass, int count1, int count2, int count3)
{
#if ENABLE_MONO
	return mono_array_new_3d (count1, count2, count3, klass);
#elif UNITY_FLASH 
	FatalErrorMsg("ToDo:CreateScriptingArray3D");
#elif UNITY_WINRT
	return ScriptingArrayPtr(GetWinRTObjectInstantiation()->CreateScriptingArray3DGC(klass->metroType, count1, count2, count3));
#endif
}


inline
ScriptingArrayPtr CreateEmptyStructArray (ScriptingClassPtr klass)
{
	return CreateScriptingArray<int>(klass,0);  //using int as random template argument, as it doesn't matter, since size=0
}

template<class T>
inline
ScriptingArrayPtr CreateScriptingArray (const T* data, int count, ScriptingClassPtr klass)
{
	if (data == NULL)
		count = 0;
	
	ScriptingArrayPtr array = CreateScriptingArray<T>(klass,count);
	memcpy(Scripting::GetScriptingArrayStart<T>(array), data, sizeof(T) * count );
	return array;
}

template<class T>
ScriptingArrayPtr CreateScriptingArrayFromUnityObjects (const T& container, int classID)
{
	ScriptingClassPtr klass = GetScriptingManager().ClassIDToScriptingClass (classID);

	ScriptingArrayPtr array = CreateScriptingArray<ScriptingObjectPtr>(klass,container.size());
	typename T::const_iterator j = container.begin();
	for (int i=0;i<container.size();i++, j++)
	{
		Scripting::SetScriptingArrayElement(array,i, Scripting::ScriptingWrapperFor(*j));
	}
	return array;
}

template<class T > inline
ScriptingArrayPtr CreateScriptingArrayFromUnityObjects(T& unityobjects,ScriptingClassPtr classForArray)
{
	ScriptingArrayPtr array = CreateScriptingArray<ScriptingObjectPtr> (classForArray , unityobjects.size ());
	for (int i=0;i<unityobjects.size ();i++)
		Scripting::SetScriptingArrayElement (array, i, Scripting::ScriptingWrapperFor (unityobjects[i]));
	return array;
}

template<class T>
inline
ScriptingArrayPtr CreateScriptingArrayStride (const void* data, int count, ScriptingClassPtr klass, int inputStride)
{
	if (data == NULL)
		count = 0;

	ScriptingArrayPtr array = CreateScriptingArray<T> (klass, count);
	UInt8* src = (UInt8*)data;
	UInt8* dst = (UInt8*)Scripting::GetScriptingArrayStart<T> (array);
	for (int i=0; i<count; ++i, src += inputStride, dst += sizeof(T))
		memcpy(dst, src, sizeof(T));
	
	return array;
}

template<class T, class T2, class U, class TConverter>
void ScriptingStructArrayToVector (ScriptingArrayPtr source, U &dest, TConverter converter) 
{
	dest.clear();
	if (source != SCRIPTING_NULL)
	{
		int len = GetScriptingArraySize(source);
		dest.resize (len);
		for (int i = 0; i < len;i++) 
			converter (Scripting::GetScriptingArrayElement<T2>(source, i), dest[i]);
	}
}

template<class T, class T2, class U, class TConverter>
void ScriptingStructArrayToDynamicArray (ScriptingArrayPtr source, U &dest, TConverter converter) 
{
	dest.clear();
	if (source != SCRIPTING_NULL)
	{
		int len = GetScriptingArraySize(source);
		dest.resize_initialized (len);
		for (int i = 0; i < len;i++) 
			converter (Scripting::GetScriptingArrayElement<T2>(source, i), dest[i]);
	}
}

template<class T, class T2, class U>
void ScriptingClassArrayToVector (ScriptingArrayPtr source, U &dest, void (*converter) (T2 &source, T &dest)) 
{
	dest.clear();
	if (source != SCRIPTING_NULL)
	{
		int len = GetScriptingArraySize(source);
		dest.resize (len);
		for (int i = 0; i < len;i++) 
		{
			T2 nativeSourceObject;
			ScriptingObjectPtr element = Scripting::GetScriptingArrayElementNoRef<ScriptingObjectPtr>(source, i);
			Scripting::RaiseIfNull(element);
			MarshallManagedStructIntoNative<T2>(element, &nativeSourceObject);
			converter (nativeSourceObject, dest[i]);
		}
	}
}

template<class T, class T2>
std::vector<T> ScriptingClassArrayToVector (ScriptingArrayPtr source, void (*converter) (T2 &source, T &dest)) 
{
	std::vector<T> dest;
	ScriptingClassArrayToVector (source, dest, converter);
	return dest;
}


template<class T, class T2, class U>
ScriptingArrayPtr VectorToScriptingClassArray (const U &source, ScriptingClassPtr klass, void (*converter) (const T &source, T2 &dest)) 
{
	// ToDo: if all good, remove mono pass, and use unified pass
#if ENABLE_MONO
	return VectorToMonoClassArray<T, T2>(source, klass, converter);
#else
	ScriptingArrayPtr arr =  CreateScriptingArray<ScriptingObjectPtr>(klass, source.size());
	for (int i = 0; i < source.size();i++) 
	{
		T2 obj;
		converter (source[i], obj);
		ScriptingObjectPtr managedObject = CreateScriptingObjectFromNativeStruct(klass, obj);
		Scripting::SetScriptingArrayElement(arr, i, managedObject);
	}
	return arr;
#endif
}

template<class T, class T2, class U, class TConverter>
ScriptingArrayPtr VectorToScriptingStructArray (const U &source, ScriptingClassPtr klass, TConverter converter) 
{
	// ToDo: if all good, remove mono pass, and use unified pass
#if ENABLE_MONO
	return VectorToMonoStructArray<T, T2>(source, klass, converter);
#else
	ScriptingArrayPtr arr =  CreateScriptingArray<ScriptingObjectPtr>(klass, source.size());
	for (int i = 0; i < source.size();i++) 
	{
		T2 obj;
		converter (source[i], obj);
		Scripting::SetScriptingArrayElement(arr, i, obj);
	}
	return arr;
#endif
}


template<class T>
void ScriptingArrayToDynamicArray(ScriptingArrayPtr a, dynamic_array<T>& dest)
{
	Scripting::RaiseIfNull (a);
	int len = GetScriptingArraySize(a);
	dest.resize_uninitialized (len);
	for (int i = 0; i < len; i++)
		dest[i] = Scripting::GetScriptingArrayElement<T>(a, i);
}

template<class T, class T2>
void ScriptingArrayToDynamicArray(ScriptingArrayPtr a, dynamic_array<T>& dest, void (*converter) (T2 &source, T &dest))
{
	Scripting::RaiseIfNull (a);
	int len = GetScriptingArraySize(a);
	dest.resize_uninitialized (len);
	for (int i = 0; i < len; i++)
		converter (Scripting::GetScriptingArrayElement<T2> (a, i), dest[i]);
}

template<class T, class T2>
ScriptingArrayPtr DynamicArrayToScriptingStructArray (const dynamic_array<T> &source, ScriptingClassPtr klass, void (*converter) (const T &source, T2 &dest)) {
	ScriptingArrayPtr arr = CreateScriptingArray<T2> (klass, source.size());
	for (int i = 0; i < source.size();i++)
		converter (source[i], Scripting::GetScriptingArrayElement<T2> (arr, i));
	return arr;
}

inline
std::string GetStringFromArray(ScriptingArrayPtr a, int i)
{
#if UNITY_WINRT
	return ConvertStringToUtf8(safe_cast<Platform::String^>(Scripting::GetScriptingArrayStringElementNoRefImpl(a, i)));
#elif UNITY_FLASH
	// not supported yet
	return "";
#else
	return MonoStringToCpp(Scripting::GetScriptingArrayStringElementNoRefImpl(a, i));
#endif
}

inline
bool GetBoolFromArray(ScriptingArrayPtr a, int i)
{
#if UNITY_WINRT
	return safe_cast<Platform::Boolean>(Scripting::GetScriptingArrayObjectElementNoRefImpl(a, i));
#else
	return Scripting::GetScriptingArrayObjectElementNoRefImpl(a, i);
#endif
}

#endif
#endif
