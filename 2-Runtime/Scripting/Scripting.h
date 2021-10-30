#pragma once

#include "UnityPrefix.h"

#include "Runtime/Modules/ExportModules.h"

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

#define SCRIPTINGAPI_STACK_CHECK(NAME)

#if WEBPLUG
# undef DOES_NOT_RETURN
# define DOES_NOT_RETURN
#endif

class Object;
class TrackedReferenceBase;

namespace Unity
{
	class GameObject;
	class Component;
}

template<typename T>
class PPtr;


namespace Scripting
{	
ScriptingObjectPtr ScriptingObjectNULL(ScriptingClassPtr klass);
ScriptingObjectPtr ScriptingObjectNULL(ScriptingClassPtr klass, ScriptingStringPtr error);

//Exception helpers, we'll need to move this again, later.
DOES_NOT_RETURN TAKES_PRINTF_ARGS(1,2) void RaiseMonoException(const char* format, ...);
DOES_NOT_RETURN TAKES_PRINTF_ARGS(1,2) EXPORT_COREMODULE void RaiseNullException(const char* format, ...);
DOES_NOT_RETURN TAKES_PRINTF_ARGS(1,2) EXPORT_COREMODULE void RaiseArgumentException(const char* format, ...);
DOES_NOT_RETURN TAKES_PRINTF_ARGS(1,2) void RaiseOutOfRangeException(const char* format, ...);
DOES_NOT_RETURN TAKES_PRINTF_ARGS(1,2) void RaiseSecurityException(const char* format, ...);
DOES_NOT_RETURN TAKES_PRINTF_ARGS(1,2) void RaiseInvalidOperationException(const char* format, ...);
DOES_NOT_RETURN TAKES_PRINTF_ARGS(3,4) void RaiseManagedException(const char* ns, const char* type, const char* format, ...);
DOES_NOT_RETURN EXPORT_COREMODULE void RaiseNullExceptionObject(ScriptingObjectPtr object);

void RaiseIfNull(void* object);
void RaiseIfNull(ScriptingObjectPtr object);

EXPORT_COREMODULE int GetInstanceIDFromScriptingWrapper(ScriptingObjectPtr wrapper);
EXPORT_COREMODULE void SetInstanceIDOnScriptingWrapper(ScriptingObjectPtr wrapper, int instanceID);
EXPORT_COREMODULE void* GetCachedPtrFromScriptingWrapper(ScriptingObjectPtr wrapper);
EXPORT_COREMODULE void SetCachedPtrOnScriptingWrapper(ScriptingObjectPtr wrapper, void* cachedPtr);
void SetErrorOnScriptingWrapper(ScriptingObjectPtr wrapper, ScriptingStringPtr error);

ScriptingTypePtr ClassIDToScriptingType(int classiD);
ScriptingObjectPtr InstantiateScriptingWrapperForClassID(int classID);
ScriptingObjectPtr ConnectScriptingWrapperToObject(ScriptingObjectPtr object, Object* ptr);

/**These don't belong here either**/
bool SendScriptingMessage(Unity::GameObject& go, const char* name, ScriptingObjectPtr param);
bool BroadcastScriptingMessage(Unity::GameObject& go, const char* name, ScriptingObjectPtr param);
bool SendScriptingMessageUpwards(Unity::GameObject& go, const char* name, ScriptingObjectPtr param);
bool SendScriptingMessage(Unity::GameObject& go, const std::string& name, ScriptingObjectPtr param, int options);
bool BroadcastScriptingMessage(Unity::GameObject& go, const std::string& name, ScriptingObjectPtr param, int options);
bool SendScriptingMessageUpwards(Unity::GameObject& go, const std::string& name, ScriptingObjectPtr param, int options);
/** end **/

void DestroyObjectFromScripting(PPtr<Object> object, float t);
void DestroyObjectFromScriptingImmediate(Object* object, bool allowDestroyingAssets);
void UnloadAssetFromScripting(Object* object);

ScriptingObjectPtr GetScriptingWrapperOfComponentOfGameObject (Unity::GameObject& go, const std::string& name);

ScriptingObjectPtr GetScriptingWrapperForInstanceID(int instanceID);
ScriptingObjectPtr CreateScriptableObject(const std::string& name);
ScriptingObjectPtr CreateScriptableObjectWithType(ScriptingObjectPtr klassType);
void CreateEngineScriptableObject(ScriptingObjectPtr object);
int GetClassIDFromScriptingClass(ScriptingClassPtr klass);

bool CompareBaseObjects(ScriptingObjectPtr lhs, ScriptingObjectPtr rhs);

ScriptingObjectPtr EXPORT_COREMODULE ScriptingWrapperFor(Object* object);

void LogException(ScriptingExceptionPtr exception, int instanceID, const std::string& error = std::string());

enum FindMode
{
	kFindAssets = 0,
	kFindActiveSceneObjects = 1,
	kFindAnything = 2
};

ScriptingArrayPtr FindObjectsOfType(ScriptingObjectPtr reflectionTypeObject, FindMode mode);

ScriptingObjectPtr GetComponentObjectToScriptingObject(Unity::Component* com, Unity::GameObject& go, int classID);
ScriptingObjectPtr GetComponentObjectToScriptingObject(Object* com, Unity::GameObject& go, int classID);

//RH / GAB : Discuss with Joe what to do with this
/// Creates a MonoObject
/// object is a ptr to a RefCounted class.
/// type is the name MonoManager common classes
/// eg. animationState -> GetScriptingManager().GetCommonClasses().animationStates
#define TrackedReferenceBaseToScriptingObject(object, type) \
	Scripting::TrackedReferenceBaseToScriptingObjectImpl(object, GetScriptingManager().GetCommonClasses().type)

ScriptingObjectPtr TrackedReferenceBaseToScriptingObjectImpl(TrackedReferenceBase* base, ScriptingClassPtr klass);

template <class StringType>
ScriptingArrayPtr StringVectorToMono (const std::vector<StringType>& source);

// Array handling

void SetScriptingArrayObjectElementImpl(ScriptingArrayPtr a, int i, ScriptingObjectPtr value);
void SetScriptingArrayStringElementImpl(ScriptingArrayPtr a, int i, ScriptingStringPtr value);
ScriptingStringPtr* GetScriptingArrayStringElementImpl(ScriptingArrayPtr a, int i);
ScriptingObjectPtr* GetScriptingArrayObjectElementImpl(ScriptingArrayPtr a, int i);
ScriptingStringPtr* GetScriptingArrayStringStartImpl(ScriptingArrayPtr a);
ScriptingObjectPtr* GetScriptingArrayObjectStartImpl(ScriptingArrayPtr a);
ScriptingStringPtr GetScriptingArrayStringElementNoRefImpl(ScriptingArrayPtr a, int i);
ScriptingObjectPtr GetScriptingArrayObjectElementNoRefImpl(ScriptingArrayPtr a, int i);

template<class T>
inline void SetScriptingArrayElement(ScriptingArrayPtr array, int i, T value)
{
	void* raw = scripting_array_element_ptr(array, i, sizeof(T));
	*(T*)raw = value;
}

template<>
inline void SetScriptingArrayElement (ScriptingArrayPtr a, int i, ScriptingObjectPtr value)
{
	SetScriptingArrayObjectElementImpl(a, i, value);
}

template<>
inline void SetScriptingArrayElement (ScriptingArrayPtr a, int i, ScriptingStringPtr value)
{
	SetScriptingArrayStringElementImpl(a, i, value);
}


template<class T>
inline T* GetScriptingArrayStart(ScriptingArrayPtr array)
{
	return (T*)scripting_array_element_ptr(array, 0, sizeof(T));
}

template<>
inline ScriptingStringPtr* GetScriptingArrayStart(ScriptingArrayPtr a)
{
	return GetScriptingArrayStringStartImpl(a);
}

template<>
inline ScriptingObjectPtr* GetScriptingArrayStart(ScriptingArrayPtr a)
{
	return GetScriptingArrayObjectStartImpl(a);
}


template<class T>
inline T& GetScriptingArrayElement(ScriptingArrayPtr a, int i)
{
	return *((T*)scripting_array_element_ptr(a, i, sizeof(T)));
}

template<>
inline ScriptingStringPtr& GetScriptingArrayElement(ScriptingArrayPtr a, int i)
{
	return *GetScriptingArrayStringElementImpl(a, i);
}

template<>
inline ScriptingObjectPtr& GetScriptingArrayElement(ScriptingArrayPtr a, int i)
{
	return *GetScriptingArrayObjectElementImpl(a, i);
}


template<class T>
inline T GetScriptingArrayElementNoRef(ScriptingArrayPtr a, int i)
{
	return *((T*)scripting_array_element_ptr(a, i, sizeof(T)));
}

template<>
inline ScriptingStringPtr GetScriptingArrayElementNoRef(ScriptingArrayPtr a, int i)
{
	return GetScriptingArrayStringElementNoRefImpl(a, i);
}

template<>
inline ScriptingObjectPtr GetScriptingArrayElementNoRef(ScriptingArrayPtr a, int i)
{
	return GetScriptingArrayObjectElementNoRefImpl(a, i);
}

}//namespace Scripting