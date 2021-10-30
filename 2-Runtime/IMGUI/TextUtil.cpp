#include "UnityPrefix.h"
#include "TextUtil.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Scripting/ScriptingUtility.h"

using namespace std;

class Font;

namespace TextUtil_Static
{
static Font *s_DefaultFont = NULL;
} // namespace TextUtil_Static
static PPtr <Font> s_CurrentFont = NULL;
static PPtr<Material> s_CurrentMaterial = NULL;

void SetTextFont (Font *font) { s_CurrentFont = font; }
void SetDrawTextMaterial (Material *m) { s_CurrentMaterial = m; }
Material *GetDrawTextMaterial () { return s_CurrentMaterial; }

extern "C" UInt16* AllocateUTF16Memory(int bytes)
{
	return (UInt16*)UNITY_MALLOC(kMemUTF16String,bytes);
}

#if ENABLE_SCRIPTING

#if ENABLE_MONO
UTF16String::UTF16String (MonoString *sourceString) {
	if (sourceString != SCRIPTING_NULL && mono_string_length (sourceString) != 0)
	{
		text = mono_string_chars (sourceString);
		length = mono_string_length (sourceString);
		owns = false;
	}
	else
	{
		owns = false;
		text = NULL;
		length = 0;
	}
}
#elif UNITY_WINRT
UTF16String::UTF16String (ScriptingStringPtr sourceString) 
{
	if (sourceString != SCRIPTING_NULL)
	{
		Platform::String^ utf16String = safe_cast<Platform::String^>(sourceString);
		length = utf16String->Length();
		int size = length * sizeof(UInt16);
		text = (UInt16*)UNITY_MALLOC(kMemUTF16String, size);
		memcpy(text, utf16String->Data(), size);
		owns = true;
	}
	else
	{
		owns = false;
		text = NULL;
		length = 0;
	}
}
#else
UTF16String::UTF16String (ScriptingStringPtr sourceString)
{
	if (sourceString != SCRIPTING_NULL)
	{
		std::string str = scripting_cpp_string_for(sourceString);
		text = AllocateUTF16Memory(str.size()*sizeof(UInt16));
		ConvertUTF8toUTF16 (str.c_str(), str.size(), text, length);
		owns = true;
	}
	else
	{
		owns = false;
		text = NULL;
		length = 0;
	}
}
#endif

#endif

extern "C" void UTF16String_TakeOverPreAllocatedUTF16Bytes(UTF16String* utfString, UInt16* bytes, int length)
{
	utfString->TakeOverPreAllocatedUTF16Bytes(bytes,length);
}

void UTF16String::TakeOverPreAllocatedUTF16Bytes(UInt16* bytes, int size)
{
	if (owns)
		UNITY_FREE(kMemUTF16String,text);
	text = bytes;
	length = size;
	owns = true;
}

void UTF16String::InitFromCharPointer (const char* str)
{
	int slen = strlen (str);
	if (slen != 0) {
		text = AllocateUTF16Memory(slen*sizeof(UInt16));
		ConvertUTF8toUTF16 (str, slen, text, length);
		owns = true;
	}
	else {
		text = NULL;
		length = 0;
		owns = false;
	}
}

UTF16String::UTF16String (const char* str) {
	InitFromCharPointer(str);
}

UTF16String::~UTF16String () {
	if (owns)
		UNITY_FREE(kMemUTF16String,text);
}

UTF16String::UTF16String (const UTF16String &other) {
	if (other.length != 0)
	{
		length = other.length;
		text =(UInt16*)UNITY_MALLOC(kMemUTF16String,length*sizeof(UInt16));
		memcpy(text,other.text,sizeof (UInt16) * length);
		owns = true;
	}
	else
	{
		length = 0;
		text = NULL;
		owns = false;
	}
}


void UTF16String::CopyString (const UTF16String &other)
{
	if (owns)
		UNITY_FREE(kMemUTF16String,text);
	if (other.length != 0)
	{
		length = other.length;
		text =(UInt16*)UNITY_MALLOC(kMemUTF16String,length*sizeof(UInt16));
		memcpy(text,other.text,sizeof (UInt16) * length);
		owns = true;
	}
	else
	{
		length = 0;
		text = NULL;
		owns = false;
	}	
}

#if ENABLE_SCRIPTING

#if ENABLE_MONO
void UTF16String::BorrowString( ScriptingString *sourceString )
{
	if (owns)
		UNITY_FREE(kMemUTF16String,text);
	
	if (sourceString != NULL && mono_string_length (sourceString) != 0)
	{
		text = mono_string_chars (sourceString);
		length = mono_string_length (sourceString);
		owns = false;
	}
	else
	{
		owns = false;
		text = NULL;
		length = 0;
	}
}
#elif UNITY_WINRT
void UTF16String::BorrowString(ScriptingStringPtr sourceString)
{
	if (owns)
		UNITY_FREE(kMemUTF16String,text);
	
	if (sourceString != SCRIPTING_NULL)
	{
		Platform::String^ utf16String = safe_cast<Platform::String^>(sourceString);
		length = utf16String->Length();
		int size = length * sizeof(UInt16);
		text = (UInt16*)UNITY_MALLOC(kMemUTF16String, size);
		memcpy(text, utf16String->Data(), size);
		owns = true;
	}
	else
	{
		owns = false;
		text = NULL;
		length = 0;
	}
}
#endif

void UTF8ToUTF16String (const char* src, UTF16String& dst)
{
	UTF16String tmp(src);
	dst.TakeOverPreAllocatedUTF16Bytes(tmp.text, tmp.length);
	tmp.owns = false;
}

ScriptingStringPtr UTF16String::GetScriptingString ()
{
	if (!length || !text)
		return SCRIPTING_NULL;

#if ENABLE_MONO
	UInt16* buffer = new UInt16[length+1];
	memcpy(buffer, text, length * 2);
	buffer[length] = 0;
	MonoString* retval = MonoStringNewUTF16 ((const wchar_t*) buffer);
	delete[] buffer;
	return retval;
#elif UNITY_FLASH || UNITY_WINRT
	std::vector<char> tempStr;
	tempStr.resize(length * 4);
	int outLength;
	ConvertUTF16toUTF8 (text, length, &tempStr[0], outLength); 
	tempStr[outLength]=0;
	return scripting_string_new((char const*)&tempStr[0]);
#endif
}

#endif

Font*
GetTextFont ()
{
	using namespace TextUtil_Static;
	Font* font = s_CurrentFont;
	if (font == NULL) {
		// Make sure we have default resource
		if (s_DefaultFont == NULL) {
			s_DefaultFont = GetBuiltinResource<Font> (kDefaultFontName);
			if (!s_DefaultFont || !s_DefaultFont->GetMaterial()) {
				LogString ("Couldn't load default font or font material!");
			}
		}
		font = s_DefaultFont;
	}
	return font;
}


#include "TextMeshGenerator2.h"
static TextMeshGenerator2 &GetMeshGen (const UTF16String &text, Font *font, TextAnchor align, float width) {
	const float tabSize = 16;
	return TextMeshGenerator2::Get (text, font, align, kAuto, width, tabSize, 1.0f, false, true, ColorRGBA32(0xffffffff));
}
float CalcTextHeight (const UTF16String &text, float width) {
	TextMeshGenerator2 &gen = GetMeshGen (text, (Font*)GetTextFont(), kDontCare, width);
	return gen.GetSize().y;
}
Vector2f CalcTextSize (const UTF16String &text) {
	TextMeshGenerator2 &gen = GetMeshGen (text, (Font*)GetTextFont(), kDontCare, 0);
	return gen.GetSize ();
}
Vector2f GetCursorPosition (const Rectf &screenRect, const UTF16String &text, TextAnchor align, bool wordWrap, int textPosition) {
	float width = wordWrap ? screenRect.Width() : 0;
	TextMeshGenerator2 &gen = GetMeshGen (text, (Font*)GetTextFont(), align, width);
	return gen.GetCursorPosition(screenRect, textPosition);
}
