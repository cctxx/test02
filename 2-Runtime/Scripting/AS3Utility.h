#ifndef AS3UTILITY_H
#define AS3UTILITY_H

#if !defined(SCRIPTINGUTILITY_H)
#error "Don't include AS3Utility.h, include ScriptingUtility.h instead"
#endif

typedef UInt32 AS3Handle;
struct UTF16String;

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

#include "Scripting.h"

using namespace std;

#define htons(A) (A)
#define htonl(A) (A)
#define ntohs htons
#define ntohl htohl

#define SCRIPTINGAPI_THREAD_CHECK(NAME)
#define SCRIPTINGAPI_CONSTRUCTOR_CHECK(NAME)
#define SCRIPTINGAPI_DEFINE_REF_ARG(t, n) t n
#define SCRIPTINGAPI_FIX_REF_ARG(t, n)

extern "C" void Ext_FileContainer_MakeFilesAvailable();
extern "C" int Ext_FileContainer_GetFileLength(const char* filename);
extern "C" char* Ext_Flash_GetNameFromClass(ScriptingType* handle);
extern "C" void Ext_Stack_Store(void* ptr);
extern "C" void Ext_Flash_LogCallstack();
extern "C" void Ext_Flash_ThrowError(const char*) __attribute__((noreturn));

extern "C" int Ext_MarshallTo(ScriptingObjectPtr obj, void* dest);
extern "C" int Ext_UnmarshallFrom(ScriptingObjectPtr obj, void* src);
//extern "C" void Ext_SetNativePtr(ScriptingObjectPtr obj, void* ptr);
//extern "C" void* Ext_GetNativePtr(ScriptingObjectPtr obj);

//ScriptingArray* MonoFindObjectsOfType (ScriptingType* reflectionTypeObject, int mode);

namespace Unity { class GameObject; class Component; }
using namespace Unity;

extern "C" const char* Ext_FileContainer_ReadStringFromFile(const char* filename);

//Used by MonoBehaviourSerialization
extern "C" void Ext_SerializeMonoBehaviour(ScriptingObjectPtr behaviour);
extern "C" UInt8* Ext_DeserializeMonoBehaviour(ScriptingObjectPtr behaviour);
extern "C" void Ext_RemapPPtrs(ScriptingObjectPtr behaviour);

extern "C" ScriptingObjectPtr Ext_Scripting_InstantiateScriptingWrapperForClassWithName(const char* name);

extern "C" bool Ext_FileContainer_IsFileCreatedAt(const char* filename);
extern "C" void Ext_WriteTextAndToolTipIntoUTF16Strings(ScriptingObject* obj, UTF16String* text, UTF16String* tooltip);

extern "C" bool Ext_Trace(const char* msg);
extern "C" ScriptingObjectPtr Ext_GetNewMonoBehaviour(const char* name);

//External interface / openurl / application.
extern "C" void Ext_OpenURL(const char* url);
extern "C" ScriptingString* Ext_ExternalCall(const char* function, ScriptingObjectPtr args);

extern "C" ScriptingObjectPtr Ext_Flash_getProperty(ScriptingObjectPtr object, const char* property);

template<class T>
ScriptingObjectPtr Flash_CreateScriptingObjectFromNativeStruct(ScriptingClass* klass, T& thestruct);

template<class T> inline
ScriptingObjectPtr CreateScriptingObjectFromNativeStruct(ScriptingClass* klass, T& thestruct)
{
	//printf_console("creating instance of klass: %d",klass);
	ScriptingObjectPtr obj = scripting_object_new(klass);
	//printf_console("populating it with native data stored at %d",&thestruct);
	Ext_UnmarshallFrom(obj, &thestruct);
	return obj;
}

inline void RaiseIfNull(ScriptingObjectPtr o) {}

inline int GetScriptingArraySize(ScriptingArray* a)
{
	if(a == NULL)
		return 0;

	//POD arrays are encoded by the as3 glue as a memoryblob: one int describing the size, following the actual bytes.
	return *(int*)(a);
}

extern "C" int Ext_MarshallTo(ScriptingObjectPtr obj, void* dest);
extern "C" int Ext_UnmarshallFrom(ScriptingObjectPtr obj, void* src);

template<class T> inline
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, T* dest)
{
	Ext_MarshallTo(so,(void*)dest);
}

template<class T> inline
void MarshallNativeStructIntoManaged(T& src, ScriptingObjectPtr dest)
{
	Ext_UnmarshallFrom(dest,(void*)&src);
}

template<class T>
inline T* ScriptingObjectToObject(ScriptingObjectPtr so)
{
	//todo: create the optimized path where we directly grab the cachedptr
	PPtr<T> p;
	p.SetInstanceID(Scripting::GetInstanceIDFromScriptingWrapper(so));
	return p;
}

ScriptingObjectPtr ScriptingInstantiateObjectFromClassName(const char* name);

inline
char* ScriptingStringToAllocatedChars(const ICallString& str)
{
	return strdup(str.utf8stream);
}

inline void ScriptingStringToAllocatedChars_Free(const char* str)
{
	free((void*)str);
}

template<class T>
inline ScriptingObjectPtr ScriptingGetObjectReference(PPtr<T> object)
{
	return Scripting::ScriptingWrapperFor(object);
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

#endif