#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "ScriptingUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Scripting.h"

inline static bool ComponentMatchesRequirement_ByScriptingClass(Unity::GameObject& go, int componentIndex, ScriptingClassPtr compareClass)
{
	int classID = go.GetComponentClassIDAtIndex(componentIndex);
	if (classID != ClassID (MonoBehaviour))
		return false;

	Unity::Component& component = go.GetComponentAtIndex(componentIndex);
	MonoBehaviour& behaviour = static_cast<MonoBehaviour&> (component);
	ScriptingClassPtr klass = behaviour.GetClass ();
	if (klass == SCRIPTING_NULL)
		return false;

	if (klass == compareClass)
		return true;

	return scripting_class_is_subclass_of (klass, compareClass);
}

inline static bool ComponentMatchesRequirement_ByClassID(Unity::GameObject& go, int componentIndex, int compareClassID, bool isCompareClassSealed)
{
	int classID = go.GetComponentClassIDAtIndex(componentIndex);
	
	if (classID == compareClassID)
		return true;

	if (isCompareClassSealed)
		return false;
	
	return Object::IsDerivedFromClassID(classID,compareClassID);
}

namespace SearchMethod
{
	enum {
		ByClassID,
		ByScriptingClass,
		DontEvenStart
	};
}

template<bool getOnlyOne, int searchMethod>
static bool GetComponentsImplementation (GameObject& go, bool includeInactive, void* compareData, void* output)
{
	if (!includeInactive && !go.IsActive())
		return false;

	bool foundAtLeastOne = false;

	bool isSealed;
	UInt64 compareClassID = (UInt64)(uintptr_t)(compareData);
	if (searchMethod == SearchMethod::ByClassID)
	{
		Assert(Transform::IsSealedClass());
		Assert(Animation::IsSealedClass());
		Assert(Camera::IsSealedClass());
		isSealed = compareClassID == ClassID(Transform) || compareClassID == ClassID(Animation) || compareClassID == ClassID(Camera);
	}

	int count = go.GetComponentCount ();
	for (int i = 0; i < count; i++)
	{
		if (searchMethod == SearchMethod::ByClassID && !ComponentMatchesRequirement_ByClassID(go,i,compareClassID,isSealed))
			continue;

		if (searchMethod == SearchMethod::ByScriptingClass && !ComponentMatchesRequirement_ByScriptingClass(go,i, reinterpret_cast<ScriptingClassPtr>(compareData)))
			continue;
		
		Unity::Component* component = &go.GetComponentAtIndex(i);
		if (getOnlyOne)
		{
			*((Unity::Component**) output) = component;
			return true;
		} 

		dynamic_array<Unity::Component*>* manyResults = (dynamic_array<Unity::Component*>*) output;
		if (manyResults->empty())
			manyResults->reserve(10);
		manyResults->push_back(component);
		foundAtLeastOne = true;
	}
	
	return foundAtLeastOne;
}

template<bool getOnlyOne, int searchMethod>
static bool GetComponentsImplementationRecurse (GameObject& go, bool includeInactive, void* compareData, void* output)
{	
	bool foundAtLeastOne = GetComponentsImplementation<getOnlyOne, searchMethod>(go,includeInactive,compareData,output);
	if (foundAtLeastOne && getOnlyOne)
		return true;
	
	// Recurse Transform hierarchy
	Transform& transform = go.GetComponentT<Transform> (ClassID (Transform));
	int count = transform.GetChildrenCount();
	for (int i = 0; i < count; i++)
	{
		Transform& child = transform.GetChild(i);
		foundAtLeastOne = GetComponentsImplementationRecurse<getOnlyOne,searchMethod>(child.GetGameObject(), includeInactive, compareData,output);
		if (foundAtLeastOne && getOnlyOne)
			return true;
	}
	return false;
}

#if MONO_QUALITY_ERRORS
static bool ScriptingClassIsValidGetComponentArgument(ScriptingClass* compareClass)
{
	if (mono_class_is_subclass_of(compareClass, GetMonoManager().GetCommonClasses().component, true))
		return true;

	if (mono_unity_class_is_interface(compareClass))
		return true;
	
	return false;
}


static void VerifyGetComponentsOfTypeFromGameObjectArgument(GameObject& go, ScriptingClassPtr compareKlass)
{
	if (compareKlass == NULL)
	{
		ScriptWarning("GetComponent asking for invalid type", &go);
		return;
	}

	if (ScriptingClassIsValidGetComponentArgument(compareKlass))
		return;

	const char* klassName = mono_class_get_name (compareKlass);
	ScriptWarning(Format("GetComponent requires that the requested component '%s' derives from MonoBehaviour or Component or is an interface.", klassName), &go);
}
#endif

static void DetermineSearchMethod(ScriptingClassPtr klass, int* outputSearchMethod, void** outputCompareData)
{
	int classid = GetScriptingManager().ClassIDForScriptingClass(klass);
	if (classid == -1)
	{
		*outputSearchMethod =  SearchMethod::ByScriptingClass;
		*outputCompareData = (void*)klass;
		return;	
	}

	*outputCompareData = (void*) classid;
	*outputSearchMethod = SearchMethod::ByClassID;
}

typedef bool GetComponentsFunction (GameObject& go, bool includeInactive, void* compareData, void* output);

template<bool getOnlyOne>
static GetComponentsFunction* DetermineGetComponentsImplementationFunction(int searchMethod,bool recursive)
{
	if (searchMethod == SearchMethod::ByClassID && recursive)
		return GetComponentsImplementationRecurse<getOnlyOne,SearchMethod::ByClassID>;

	if (searchMethod == SearchMethod::ByClassID && !recursive)
		return GetComponentsImplementation<getOnlyOne,SearchMethod::ByClassID>;

	if (searchMethod == SearchMethod::ByScriptingClass && recursive)
		return GetComponentsImplementationRecurse<getOnlyOne,SearchMethod::ByScriptingClass>;

	if (searchMethod == SearchMethod::ByScriptingClass && !recursive)
		return GetComponentsImplementation<getOnlyOne,SearchMethod::ByScriptingClass>;

	if (searchMethod == SearchMethod::DontEvenStart)
		return NULL;

	return NULL;
}

template<bool getOnlyOne>
static void GetComponentsOfTypeFromGameObject(GameObject& go, ScriptingClassPtr compareKlass, bool generateErrors, bool recursive, bool includeInactive, void* results)
{
#if MONO_QUALITY_ERRORS
	if (generateErrors)
		VerifyGetComponentsOfTypeFromGameObjectArgument(go,compareKlass);	
#endif	

	if (compareKlass == SCRIPTING_NULL)
		return;
	
	int searchMethod;
	void* compareData;

	DetermineSearchMethod(compareKlass,&searchMethod,&compareData);

	GetComponentsFunction* getComponents = DetermineGetComponentsImplementationFunction<getOnlyOne>(searchMethod,recursive);
	if (getComponents)
		getComponents(go,includeInactive,compareData,results);
}

ScriptingArrayPtr ScriptingGetComponentsOfType (GameObject& go, ScriptingObjectPtr systemTypeInstance, bool useSearchTypeAsArrayReturnType, bool recursive, bool includeInactive)
{
	dynamic_array<Unity::Component*> components(kMemTempAlloc);	
	ScriptingClassPtr compareKlass = GetScriptingTypeRegistry().GetType (systemTypeInstance);
	GetComponentsOfTypeFromGameObject<false>(go,compareKlass, true, recursive, includeInactive, &components);

#if ENABLE_MONO || UNITY_WINRT
	ScriptingClassPtr componentClass = GetMonoManager ().ClassIDToScriptingClass (ClassID (Component));
	ScriptingClassPtr classForArray = useSearchTypeAsArrayReturnType ? compareKlass : componentClass;
	// ToDo: why aren't we using useSearchTypeAsArrayReturnType for flash ?! Lucas, Ralph?
#elif UNITY_FLASH
	ScriptingClassPtr classForArray = compareKlass;
#endif
	return CreateScriptingArrayFromUnityObjects(components,classForArray);
}

ScriptingObjectPtr ScriptingGetComponentOfType (GameObject& go, ScriptingObjectPtr systemTypeInstance, bool generateErrors)
{
	if (systemTypeInstance == SCRIPTING_NULL)
	{
		Scripting::RaiseArgumentException ("Type can not be null.");
		return SCRIPTING_NULL;
	}

	ScriptingClassPtr compareKlass = GetScriptingTypeRegistry().GetType (systemTypeInstance);
	Unity::Component* result = NULL;
	
	GetComponentsOfTypeFromGameObject<true>(go,compareKlass, generateErrors, false, true, &result);

	if (result != NULL)
		return Scripting::ScriptingWrapperFor(result);
	
#if MONO_QUALITY_ERRORS
	if(generateErrors)
		return MonoObjectNULL(compareKlass, MissingComponentString(go,compareKlass));
#endif

	return SCRIPTING_NULL;
}

ScriptingObjectPtr ScriptingGetComponentOfType (GameObject& go, ScriptingClassPtr systemTypeInstance)
{
	Unity::Component* result = NULL;
	GetComponentsOfTypeFromGameObject<true>(go, systemTypeInstance, false, false, true, &result);
	if (result) 
		return Scripting::ScriptingWrapperFor(result);

	return SCRIPTING_NULL;
}


#endif
