#ifndef GUIContent_H
#define GUIContent_H

#include "Runtime/IMGUI/TextUtil.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Misc/UTF8.h"

struct GUIContent 
{
	UTF16String		m_Text;
	UTF16String		m_Tooltip;
	PPtr<Texture>	m_Image;
	
	void operator = (const GUIContent& other)
	{
		m_Text.CopyString(other.m_Text);
		m_Tooltip.CopyString(other.m_Tooltip);
		m_Image = other.m_Image;
	}
};

#if ENABLE_SCRIPTING

struct MonoGUIContent 
{
	ScriptingStringPtr m_Text;
	ScriptingObjectPtr m_Image;
	ScriptingStringPtr m_Tooltip;
};

GUIContent &MonoGUIContentToTempNative (ScriptingObjectPtr monoContent);
void MonoGUIContentToNative (ScriptingObjectPtr monoContent, GUIContent& cppContent);
#endif

#endif
