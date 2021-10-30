#ifndef MONOUTILITY_H
#define MONOUTILITY_H

#if !defined(SCRIPTINGUTILITY_H)
#error "Don't include MonoUtility.h, include ScriptingUtility.h instead"
#endif

#include "Configuration/UnityConfigure.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Scripting/ScriptingManager.h"

//Unity messes around deep in mono internals, bypassing mono's API to do certain things. One of those things is we have c++
//structs that we assume are identical to c# structs, and when a mono method calls into the unity runtime, we make the assumption
//that we can access the data stored in the c# struct directly.  When a MonoObject* gets passed to us by mono, and we know that the
//object it represents is a struct, we take the MonoObject*,  add 8 bytes, and assume the data of the c# struct is stored there in memory.
//When you update to a newer version of mono, and get weird crashes, the assumptions that these offsets make are a good one to verify.
enum { kMonoObjectOffset = sizeof(void*) * 2 };
enum { kMonoArrayOffset = sizeof(void*) * 4 };

#if UNITY_EDITOR
bool IsStackLargeEnough ();
void AssertStackLargeEnough ();
#undef SCRIPTINGAPI_STACK_CHECK
#define SCRIPTINGAPI_STACK_CHECK(NAME) AssertStackLargeEnough();
#endif

// TODO: move
struct UnityEngineObjectMemoryLayout
{
	int         instanceID;
	void*       cachedPtr;

#if MONO_QUALITY_ERRORS
	MonoString* error;
#endif
};

struct MonoObject;
struct MonoClass;
struct MonoException;
struct MonoArray;
class TrackedReferenceBase;


#if UNITY_WII
#define SCRIPTINGAPI_DEFINE_REF_ARG(t, n) MonoObject* _ ## n ## __mObject
#define SCRIPTINGAPI_FIX_REF_ARG(t, n) t n; n.object = _ ## n ## __mObject;
#else
#define SCRIPTINGAPI_DEFINE_REF_ARG(t, n) t n
#define SCRIPTINGAPI_FIX_REF_ARG(t, n)
#endif

/*
	Every MonoObject* that wraps an Object class contains the Reference::Data struct.
	It contains the instanceID and a cachedPtr.
	Only DEPLOY_OPTIMIZED mode (Player in release build) uses the cached ptr

	Normally we play safe and always dereference the pptr and also explicitly check
    for null so we catch null ptr's before dereferencing.

    Referenced objects can of course be destroyed, resulting in stable cachedPtr's
    Why does this work?

    If the script code expects that a reference might become null it needs to check against null.
    if (transform == null)
    If the script code does not check and the referenced is deleted,
    then a null exception will be thrown in the editor.
	In the DEPLOY_OPTIZMIZED mode no exception will be thrown but the program will just crash.
	This means that a game has to ship with no null exception thrown in the editor or debug player.
*/

template<class T>
inline T* ExtractMonoObjectDataPtr (MonoObject* object)
{
	return reinterpret_cast<T*> (reinterpret_cast<char*> (object) + kMonoObjectOffset);
}

template<class T>
inline T& ExtractMonoObjectData (MonoObject* object)
{
	return *reinterpret_cast<T*> (reinterpret_cast<char*> (object) + kMonoObjectOffset);
}


inline ScriptingObjectPtr ScriptingInstantiateObject(ScriptingClassPtr klass)
{
#if UNITY_EDITOR
	if (mono_unity_class_is_abstract (klass)) {
		// Cannot instantiate abstract class
		return SCRIPTING_NULL;
	}
#endif
	return mono_object_new(mono_domain_get(),klass);
}

template<class T> inline
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, T* dest)
{
	*dest = ExtractMonoObjectData<T>(so);
}

template<class T> inline
void MarshallNativeStructIntoManaged(const T& src, ScriptingObjectPtr dest)
{
	ExtractMonoObjectData<T>(dest) = src;
}


// If MonoClass is derived from any UnityEngine class returns its classID
// otherwise returns -1.

void mono_runtime_object_init_exception (MonoObject *thiss, MonoException** exception);
void mono_runtime_object_init_log_exception (MonoObject *thiss);

template<class T>
inline T& GetMonoArrayElement (MonoArray* array, int i)
{
	return Scripting::GetScriptingArrayElement<T>(array,i);
}


inline void* GetMonoArrayPtr(MonoArray* array)
{
	return (void*) (((char*) array) + kMonoArrayOffset);
}

template<class T>
inline T* GetMono2DArrayData (MonoArray* array)
{
	char* raw = kMonoArrayOffset + sizeof(guint32) + (char*)array;
	return (T*)raw;
}


template<class T>
inline T* GetMono3DArrayData (MonoArray* array)
{
	char* raw = kMonoArrayOffset + sizeof(guint32)*2 + (char*)array;
	return (T*)raw;
}

int MonoDomainGetUniqueId();
void MonoDomainIncrementUniqueId();

int mono_array_length (MonoArray* array);
int mono_array_length_safe (MonoArray* array);

std::string MonoStringToCpp (MonoString* monoString);
std::string MonoStringToCppChecked (MonoObject* monoString);

#if UNITY_WIN || UNITY_XENON
std::wstring MonoStringToWideCpp (MonoString* monoString);
#endif

MonoString* MonoStringNew (const std::string& in);
MonoString* MonoStringNew (const char* in);
MonoString* MonoStringNewLength (const char* in, int length);
MonoString* MonoStringNewUTF16 (const wchar_t* in);
ScriptingStringPtr MonoStringFormat (const char* format, ...);

bool ExceptionToLineAndPath (const std::string& exception, int& line, std::string& path);
bool MonoObjectToBool (MonoObject* value);
int MonoObjectToInt (MonoObject* value);
MonoMethod* mono_reflection_method_get_method (MonoObject* ass);
MonoString* MonoStringFormat (const char* format, ...);
MonoArray *mono_array_new_3d (int size0, int size1, int size2, MonoClass *klass);
MonoArray *mono_array_new_2d (int size0, int size1, MonoClass *klass);
MonoAssembly* mono_load_assembly_from_any_monopath(const char* assemblyname);
MonoMethod* mono_unity_find_method(const char* assemblyname, const char* ns, const char* klass, const char* methodname);
MonoObject* mono_class_get_object (MonoClass* klass);
MonoClass* mono_type_get_class_or_element_class (MonoType* type);
MonoString* UnassignedReferenceString (MonoObject* instance, int classID, MonoClassField* field, int instanceID);

bool IsUtf16InAsciiRange( gunichar2 const* str, int length );
bool FastTestAndConvertUtf16ToAscii( char* dest, gunichar2 const* str, int length );
// str must contain only ascii characters
void FastUtf16ToAscii( char* destination, gunichar2 const* str, int length );

// Helper functions to go over a C++ vector<arbitrary struct> & create a MonoArray of it.
// it runs a template function on it in order to do the actual conversion of each object
/* Example usage:
C++RAW
struct MonoTreePrototype {
	MonoObject *prefab;
};

void TreePrototypeToMono (TreePrototype &src, MonoTreePrototype &dest) {
	dest.prefab = ObjectToScriptingObject (src.prefab);
}

void TreePrototypeToCpp (MonoTreePrototype &src, TreePrototype &dest) {
	dest.prefab = MonoObjectToObject<GameObject> (src.prefab);
}


CUSTOM_PROP TreePrototype[] treePrototypes
	{ return VectorToMonoStructArray<TreePrototype, MonoTreePrototype> (self->GetTreePrototypes(), MONO_COMMON.treePrototype, TreePrototypeToMono); }
	{ MonoStructArrayToVector<TreePrototype, MonoTreePrototype> (value, self->GetTreePrototypes(), TreePrototypeToCpp); }
*/

template<class T, typename Alloc>
void MonoObjectArrayToSet (MonoArray *array, std::set<T*, std::less<T*>, Alloc >& dest) {
	Scripting::RaiseIfNull (array);
	int len = mono_array_length_safe(array);
	dest.clear();
	for (int i = 0; i < len;i++)
	{
		int instanceID = Scripting::GetInstanceIDFromScriptingWrapper(GetMonoArrayElement<MonoObject*>(array, i));
		T* objectPtr = dynamic_instanceID_cast<T*> (instanceID);
		if (objectPtr)
			dest.insert(objectPtr);
	}
}

template<class T>
void MonoArrayToSet(MonoArray* array, std::set<T>& dest)
{
	Scripting::RaiseIfNull (array);
	int len = mono_array_length_safe(array);
	dest.clear();
	for (int i = 0; i < len;i++)
	{
		dest.insert(GetMonoArrayElement<T>(array, i));
	}
}

template<typename T>
MonoArray* SetToMonoArray(const std::set<T>& src, MonoClass* klass)
{
	MonoArray* array = mono_array_new (mono_domain_get (), klass, src.size());

	typename std::set<T>::const_iterator j = src.begin();
	for (int i=0;i<src.size();i++, j++)
	{
		Scripting::SetScriptingArrayElement(array, i, *j);
	}

	return array;
}

template<class T>
void MonoArrayToVector(MonoArray* array, std::vector<T>& dest)
{
	Scripting::RaiseIfNull (array);
	int len = mono_array_length_safe(array);
	dest.resize (len);
	for (int i = 0; i < len; i++)
		dest[i] = GetMonoArrayElement<T>(array, i);
}


template<class T, class T2, class U, class TConverter>
MonoArray *VectorToMonoStructArray (const U &source, MonoClass *klass, TConverter converter) {
	MonoArray *arr = mono_array_new (mono_domain_get (), klass, source.size());
	for (int i = 0; i < source.size();i++)
		converter (source[i], GetMonoArrayElement<T2> (arr, i));
	return arr;
}

template<class T, class T2>
void MonoStructArrayToVector (MonoArray *source, std::vector<T> &dest, void (*converter) (T2 &source, T &dest)) {
	Scripting::RaiseIfNull (source);
	int len = mono_array_length(source);
	dest.resize (len);
	for (int i = 0; i < len;i++)
		converter (GetMonoArrayElement<T2> (source, i), dest[i]);
}

template<class T, class T2>
std::vector<T> MonoStructArrayToVector (MonoArray *source, void (*converter) (T2 &source, T &dest)) {
	std::vector<T> dest;
	MonoStructArrayToVector<T, T2> (source, dest, converter);
	return dest;
}

template<class T, class T2, class U>
MonoArray *VectorToMonoClassArray (const U &source, MonoClass *klass, void (*converter) (const T &source, T2 &dest)) {
	MonoArray *arr = mono_array_new (mono_domain_get (), klass, source.size());
	for (int i = 0; i < source.size();i++) {
		MonoObject *obj = ScriptingInstantiateObject (klass);
		GetMonoArrayElement<MonoObject*> (arr,i) = obj;
		converter (source[i], ExtractMonoObjectData<T2> (obj));
	}
	return arr;
}

template<class T, class T2>
MonoArray *SetToMonoClassArray (const std::set<T> &source, MonoClass *klass, void (*converter) (const T &source, T2 &dest)) {
	MonoArray *arr = mono_array_new (mono_domain_get (), klass, source.size());
	int idx = 0;
	for (typename std::set<T>::const_iterator i = source.begin(); i != source.end();i++) {
		MonoObject *obj = ScriptingInstantiateObject (klass);
		GetMonoArrayElement<MonoObject*> (arr,idx++) = obj;
		converter (*i, ExtractMonoObjectData<T2> (obj));
	}
	return arr;
}

template<typename mapType, class T, class T2, class T3>
MonoArray *MapToMonoClassArray (const mapType &source, MonoClass *klass, void (*converter) (const T &first, const T2 &second, T3 &dest)) {
	MonoArray *arr = mono_array_new (mono_domain_get (), klass, source.size());
	int idx = 0;
	for (typename mapType::const_iterator i = source.begin(); i != source.end();i++) {
		MonoObject *obj = ScriptingInstantiateObject (klass);
		GetMonoArrayElement<MonoObject*> (arr,idx++) = obj;
		converter (i->first, i->second, ExtractMonoObjectData<T3> (obj));
	}
	return arr;
}

template<class T, class T2>
void MonoClassArrayToVector (MonoArray *source, std::vector<T> &dest, void (*converter) (T2 &source, T &dest)) {
	Scripting::RaiseIfNull (source);
	int len = mono_array_length(source);
	dest.resize (len);
	for (int i = 0; i < len;i++) {
		MonoObject *obj = GetMonoArrayElement<MonoObject*> (source, i);
		Scripting::RaiseIfNull (obj);
		converter (ExtractMonoObjectData<T2> (obj), dest[i]);
	}
}

template<class T, class T2>
std::vector<T> MonoClassArrayToVector (MonoArray *source, void (*converter) (T2 &source, T &dest)) {
	std::vector<T> dest;
	MonoClassArrayToVector<T, T2> (source, dest, converter);
	return dest;
}

void StringMonoArrayToVector (MonoArray* arr, std::vector<std::string>& container);
void StringMonoArrayToVector (MonoArray* arr, std::vector<UnityStr>& container);

inline ScriptingClassPtr GetScriptingTypeOfScriptingObject(ScriptingObjectPtr object)
{
	return mono_object_get_class(object);
}

inline bool ScriptingClassIsSubclassOf(ScriptingClassPtr c1, ScriptingClassPtr c2)
{
	return mono_class_is_subclass_of(c1,c2,true);
}

ScriptingClassPtr GetBuiltinScriptingClass(const char* name,bool optional=false);

inline int GetScriptingArraySize(ScriptingArrayPtr a)
{
	return mono_array_length_safe(a);
}


/*
 Code for switching between mono_runtime_invoke and mono_aot_get_method
 (later mono_method_get_unmanaged_thunk also will be included)
 */

void* ResolveMonoMethodPointer(MonoDomain *domain, MonoMethod *method);
bool MonoSetObjectField(MonoObject* target, const char* fieldname, MonoObject* value);
#if UNITY_EDITOR
bool IsStackLargeEnough ();
#endif

// Ensure current thread is attached to mono,
// error and return otherwise
#define MONO_RUNTIME_INVOKE_THREAD_CHECK_WITH_RET(Ret)									\
do{																						\
	if (!mono_thread_current ()) {														\
		ErrorStringWithoutStacktrace ("Thread is not attached to scripting runtime");	\
		return Ret;																		\
	}																					\
} while (0)

#define MONO_RUNTIME_INVOKE_THREAD_CHECK MONO_RUNTIME_INVOKE_THREAD_CHECK_WITH_RET(NULL)

// Ensure we have enough stack for a reasonable invocation
#if UNITY_EDITOR
#define MONO_RUNTIME_INVOKE_STACK_CHECK_WITH_RET(Ret)														\
do {																										\
	if (!IsStackLargeEnough ())	{																			\
		*exc = mono_exception_from_name_msg (mono_get_corlib (), "System", "StackOverflowException", "");	\
		return Ret;																							\
	}																										\
} while (0)

#define MONO_RUNTIME_INVOKE_STACK_CHECK MONO_RUNTIME_INVOKE_STACK_CHECK_WITH_RET(NULL)
#else
#define MONO_RUNTIME_INVOKE_STACK_CHECK_WITH_RET(Ret)	do {} while (0)
#define MONO_RUNTIME_INVOKE_STACK_CHECK					do {} while (0)
#endif

inline MonoObject* mono_runtime_invoke_profiled_fast (ScriptingMethodPtr method, MonoObject* obj, MonoException** exc, MonoClass* profileClassForCoroutine)
{
	MONO_RUNTIME_INVOKE_THREAD_CHECK;
	MONO_RUNTIME_INVOKE_STACK_CHECK;

	MONO_PROFILER_BEGIN(method,profileClassForCoroutine, obj);

	MonoObject* ret;
#if USE_MONO_AOT
	if (method->fastMonoMethod)
		ret = method->fastMonoMethod(obj, exc);
	else
#endif
		ret = mono_runtime_invoke(method->monoMethod, obj, NULL, exc);

	MONO_PROFILER_END;
	
	return ret;
}


/// returns false if an exception was thrown
inline bool mono_runtime_invoke_profiled_fast_bool (MonoMethod* method, FastMonoMethod fastMethod, MonoObject* obj, MonoException** exc, MonoClass* profileClassForCoroutine)
{
	Assert(USE_MONO_AOT || fastMethod == NULL );
	Assert(exc != NULL);

	MONO_RUNTIME_INVOKE_THREAD_CHECK_WITH_RET(false);
	MONO_RUNTIME_INVOKE_STACK_CHECK_WITH_RET(false);

	MONO_PROFILER_BEGIN(GetScriptingMethodRegistry().GetMethod(method), profileClassForCoroutine, obj);

	bool result = false;
#if USE_MONO_AOT
	if (fastMethod)
		result = fastMethod(obj, exc);
	else
#endif
	{
		MonoObject* _tmpres = mono_runtime_invoke(method, obj, NULL, exc);
		if (_tmpres != NULL && *exc == NULL)
			result = ExtractMonoObjectData<char>(_tmpres);
	}

	MONO_PROFILER_END;

	return result;
}

/// Never call mono_runtime_invoke directly, otherwise the profiler will not be able to pick it up!
inline MonoObject* mono_runtime_invoke_profiled (MonoMethod *method, MonoObject *obj, void **params, MonoException **exc, MonoClass* classContextForProfiler=NULL)
{
	MONO_RUNTIME_INVOKE_THREAD_CHECK;
	MONO_RUNTIME_INVOKE_STACK_CHECK;

	MONO_PROFILER_BEGIN (GetScriptingMethodRegistry().GetMethod(method), classContextForProfiler, obj);
	MonoObject* ret = mono_runtime_invoke(method, obj, params, exc);
	MONO_PROFILER_END;
	
	return ret;
}

template<class T>
ScriptingObjectPtr CreateScriptingObjectFromNativeStruct(ScriptingClassPtr klass, T& thestruct)
{
	ScriptingObjectPtr mono = ScriptingInstantiateObject (klass);
	T& destStruct = ExtractMonoObjectData<T> (mono);
	destStruct = thestruct;
	return mono;
}

inline
char* ScriptingStringToAllocatedChars(const ICallString& str)
{
	return mono_string_to_utf8(str.str);
}

MonoClassField* GetMonoArrayFieldFromList (int type, MonoType* monoType, MonoClassField* field);

int EXPORT_COREMODULE mono_array_length_safe_wrapper(MonoArray* array);

inline void ScriptingStringToAllocatedChars_Free(const char* str)
{
	g_free((void*)str);
}

template<class T>
struct ScriptingObjectOfType;

template<class T>
inline T* ScriptingObjectToObject(ScriptingObjectPtr so)
{
	ScriptingObjectOfType<T> ref(so);
	return ref.GetPtr();
}

MonoClassField* GetMonoArrayFieldFromList (int type, MonoType* monoType, MonoClassField* field);

/// ToDo: remove these (MonoObjectArrayToVector, MonoObjectArrayToPPtrVector) later, or change it to more unified version
/// vector<GameObject*> gos;
/// gos = MonoObjectArrayToVector(MonoArray*);
template<class T>
void MonoObjectArrayToVector (MonoArray *source, std::vector<T*>& dest) {
	Scripting::RaiseIfNull (source);
	int len = mono_array_length(source);
	dest.resize (len);
	for (int i = 0; i < len;i++)
		dest[i] = ScriptingObjectToObject<T> (GetMonoArrayElement<MonoObject*> (source, i));
}

/// vector<GameObject*> gos;
/// gos = MonoObjectArrayToVector(MonoArray*);
template<class T>
void MonoObjectArrayToPPtrVector (MonoArray *source, std::vector<PPtr<T> >& dest) {
	Scripting::RaiseIfNull (source);
	int len = mono_array_length(source);
	dest.resize (len);
	for (int i = 0; i < len;i++)
		dest[i] = ScriptingObjectToObject<T> (GetMonoArrayElement<MonoObject*> (source, i));
}

template<class T>
inline ScriptingObjectPtr ScriptingGetObjectReference(PPtr<T> object)
{
	return ObjectToScriptingObject(object);
}

template<class T>
inline ScriptingObjectPtr ScriptingGetObjectReference(T* object)
{
	return Scripting::ScriptingWrapperFor(object);
}

#define CreateScriptingParams(VariableName, ParamCount) ScriptingParams VariableName[ParamCount];
// Don't use if Value is ScriptingObjectPtr!!!
#define SetScriptingParam(VariableName, Index, Value) VariableName[Index] = &Value;
#define GetSafeString(ClassName, Getter) Getter

std::string ErrorMessageForUnsupportedEnumField(MonoType* enumType, MonoType* classType, const char * fieldName);

MonoObject* MonoObjectNULL (int classID, MonoString* error);

namespace Unity { class GameObject; class Component;}
class Object;

#if MONO_QUALITY_ERRORS
// Wrap null ptr with pseudo null object
ScriptingObjectPtr MonoObjectNULL (ScriptingClassPtr klass, ScriptingStringPtr error);
inline ScriptingObjectPtr MonoObjectNULL (ScriptingClassPtr klass){ return MonoObjectNULL (klass, SCRIPTING_NULL); }

// Component missing string error
MonoString* MissingComponentString (Unity::GameObject& go, int classID);
MonoString* MissingComponentString (Unity::GameObject& go, ScriptingTypePtr klass);

#else

inline ScriptingObjectPtr MonoObjectNULL (ScriptingClassPtr klass) { return SCRIPTING_NULL; }

#endif 

#endif//ENABLE_MONO
