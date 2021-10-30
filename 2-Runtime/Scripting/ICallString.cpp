#include "UnityPrefix.h"

#if ENABLE_SCRIPTING

#if !UNITY_FLASH
#include "ICallString.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"


#if ENABLE_MONO
std::string ICallString::AsUTF8() const
{
	return scripting_cpp_string_for(str).c_str();
}
#elif UNITY_WINRT
#include "PlatformDependent/MetroPlayer/MetroUtils.h"
std::string ICallString::AsUTF8() const
{
	return ConvertStringToUtf8(str);
}
#endif

#if ENABLE_MONO
// todo: remove this useless include once we figure out where to take mono_string_length from.
#include "Runtime/Scripting/ScriptingUtility.h"
#endif

int ICallString::Length()
{
#if ENABLE_MONO
	return mono_string_length(str);
#elif UNITY_WINRT
	return wcslen(str);
	//return safe_cast<Platform::String^>(str)->Length();
#endif
}
#endif

#endif

