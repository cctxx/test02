#include "UnityPrefix.h"
#include "Runtime/IMGUI/GUIContent.h"
#include "Runtime/Scripting/ScriptingUtility.h"

#if ENABLE_SCRIPTING
static GUIContent s_TempGUIContent;
#if UNITY_WINRT
void MonoGUIContentToTempNativeCallback(Platform::String^ text, Platform::String^ tooltip, long long image)
{
	s_TempGUIContent.m_Text.BorrowString (text);
	s_TempGUIContent.m_Tooltip.BorrowString (tooltip);
	s_TempGUIContent.m_Image = ScriptingObjectToObject<Texture> (ScriptingObjectPtr(image));
}
BridgeInterface::ScriptingGUIContentToTempNativeDelegateGC^ GetMonoGUIContentToTempNativeCallback()
{
	static BridgeInterface::ScriptingGUIContentToTempNativeDelegateGC^ s_Callback  = ref new BridgeInterface::ScriptingGUIContentToTempNativeDelegateGC(MonoGUIContentToTempNativeCallback);
	return s_Callback;
}

#endif

GUIContent &MonoGUIContentToTempNative (ScriptingObjectPtr scriptingContent)
{
	MonoGUIContentToNative (scriptingContent, s_TempGUIContent);
	return s_TempGUIContent;
}

void MonoGUIContentToNative (ScriptingObjectPtr scriptingContent, GUIContent& cppContent)
{
	if (scriptingContent == SCRIPTING_NULL)
	{
		WarningString("GUIContent is null. Use GUIContent.none.");
		cppContent.m_Text = (UTF16String) "";
		cppContent.m_Tooltip = (UTF16String)  "";
		cppContent.m_Image = NULL;

		return;
	}

#if UNITY_WINRT
	GetWinRTUtils()->ScriptingGUIContentToTempNativeGC(scriptingContent.GetHandle(), GetMonoGUIContentToTempNativeCallback());
#else
	MonoGUIContent nativeContent;
 	MarshallManagedStructIntoNative(scriptingContent, &nativeContent);
	
#	if ENABLE_MONO 
		cppContent.m_Text.BorrowString (nativeContent.m_Text);
		cppContent.m_Tooltip.BorrowString (nativeContent.m_Tooltip);
#	elif UNITY_FLASH
		Ext_WriteTextAndToolTipIntoUTF16Strings(scriptingContent,&cppContent.m_Text,&cppContent.m_Tooltip);
#	endif	

	cppContent.m_Image = ScriptingObjectToObject<Texture> (nativeContent.m_Image);
#endif
	return;
}

#endif
