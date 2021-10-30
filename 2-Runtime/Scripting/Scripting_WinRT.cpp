#include "UnityPrefix.h"

#include "Scripting.h"
#include "ScriptingManager.h"
#include "Backend/ScriptingTypeRegistry.h"
#include "Backend/ScriptingBackendApi.h"
#include "PlatformDependent/MetroPlayer/MetroUtils.h"

#include <wrl/wrappers/corewrappers.h>

static BridgeInterface::IArrayTools^& GetWinRTArrayTools()
{
	static BridgeInterface::IArrayTools^ s_Cached = s_WinRTBridge->ArrayTools;
	return s_Cached;
}

static BridgeInterface::IUnityEngineObjectTools^& GetWinRTUnityEngineObjectTools()
{
	static BridgeInterface::IUnityEngineObjectTools^ s_Cached = s_WinRTBridge->UnityEngineObjectTools;
	return s_Cached;
}

namespace Scripting
{

void* GetCachedPtrFromScriptingWrapper(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		return NULL;
	
	return (void*)GetWinRTUnityEngineObjectTools()->ScriptingObjectGetReferenceDataCachedPtrGC(object.GetHandle());
}

void SetCachedPtrOnScriptingWrapper(ScriptingObjectPtr object, void* cachedPtr)
{
	GetWinRTUnityEngineObjectTools()->ScriptingObjectSetReferenceDataCachedPtrGC(object.GetHandle(), (int)cachedPtr);
}

int GetInstanceIDFromScriptingWrapper(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		return 0;

	return GetWinRTUnityEngineObjectTools()->ScriptingObjectGetReferenceDataInstanceIDGC(object.GetHandle());
}

void SetInstanceIDOnScriptingWrapper(ScriptingObjectPtr object, int instanceID)
{
	GetWinRTUnityEngineObjectTools()->ScriptingObjectSetReferenceDataInstanceIDGC(object.GetHandle(), instanceID);
}

void SetErrorOnScriptingWrapper(ScriptingObjectPtr object, ScriptingStringPtr error)
{

}

#if !UNITY_EXTERNAL_TOOL
ScriptingObjectPtr InstantiateScriptingWrapperForClassID(int classID)
{
	const string& name = Object::ClassIDToString(classID);

	ScriptingClassPtr klass = GetScriptingTypeRegistry().GetType("UnityEngine", name.c_str());
	if (klass != SCRIPTING_NULL)	
		return scripting_object_new(klass);

	int superClassID = Object::GetSuperClassID(classID);
	if(superClassID != ClassID(Object))
		return InstantiateScriptingWrapperForClassID(superClassID);

	return SCRIPTING_NULL;
}
#endif

ScriptingObjectPtr ScriptingObjectNULL(ScriptingClassPtr klass)
{ 
	return SCRIPTING_NULL; 
}

void RaiseSecurityException(const char* format, ...)
{
	va_list va;
	va_start(va, format);
	throw ref new Platform::FailureException(ConvertUtf8ToString(VFormat(format, va)));
}

void RaiseNullException(const char* format, ...)
{
	va_list va;
	va_start(va, format);
	throw ref new Platform::FailureException(ConvertUtf8ToString(VFormat(format, va)));
}

void RaiseNullExceptionObject(ScriptingObjectPtr object)
{
	throw ref new Platform::NullReferenceException();
}

void RaiseMonoException (const char* format, ...)
{
	va_list va;
	va_start(va, format);
	throw ref new Platform::FailureException(ConvertUtf8ToString(VFormat(format, va)));
}

void RaiseOutOfRangeException(const char* format, ...)
{
	va_list va;
	va_start(va, format);
	throw ref new Platform::OutOfBoundsException(ConvertUtf8ToString(VFormat(format, va)));
}

void RaiseArgumentException (const char* format, ...)
{
	va_list va;
	va_start(va, format);
	throw ref new Platform::InvalidArgumentException(ConvertUtf8ToString(VFormat(format, va)));
}

void RaiseIfNull(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		throw ref new Platform::NullReferenceException();
}

void RaiseIfNull(void* object)
{
	if(object == NULL)
		RaiseNullException("(null)");
}

void SetScriptingArrayObjectElementImpl(ScriptingArrayPtr a, int i, ScriptingObjectPtr value)
{
	GetWinRTArrayTools()->SetArrayValue(a.GetHandle(), i, value.GetHandle());
}

void SetScriptingArrayStringElementImpl(ScriptingArrayPtr a, int i, ScriptingStringPtr value)
{
	GetWinRTArrayTools()->SetArrayStringValue(a.GetHandle(), i, value);
}

ScriptingStringPtr* GetScriptingArrayStringElementImpl(ScriptingArrayPtr a, int i)
{
	FatalErrorMsg("Not allowed");
	return NULL;
}

ScriptingObjectPtr* GetScriptingArrayObjectElementImpl(ScriptingArrayPtr a, int i)
{
	FatalErrorMsg("Not allowed");
	return NULL;
}

ScriptingStringPtr* GetScriptingArrayStringStartImpl(ScriptingArrayPtr a)
{
	FatalErrorMsg("Not allowed");
	return NULL;
}

ScriptingObjectPtr* GetScriptingArrayObjectStartImpl(ScriptingArrayPtr a)
{
	FatalErrorMsg("Not allowed");
	return NULL;
}

ScriptingStringPtr GetScriptingArrayStringElementNoRefImpl(ScriptingArrayPtr a, int i)
{
	return GetWinRTArrayTools()->GetArrayStringValue(a.GetHandle(), i);
}

ScriptingObjectPtr GetScriptingArrayObjectElementNoRefImpl(ScriptingArrayPtr a, int i)
{
	return WinRTScriptingObjectWrapper(GetWinRTArrayTools()->GetArrayValue(a.GetHandle(), i));
}

} // namespace Scripting