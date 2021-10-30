#pragma once

#if ENABLE_SCRIPTING

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include <string>

#if ENABLE_MONO
struct ICallString
{
	MonoString* str;

	EXPORT_COREMODULE std::string AsUTF8() const;
	operator std::string () const { return AsUTF8(); }
	int Length();
	bool IsNull () {return !str;}

	MonoString* GetNativeString() {return str;}
};
#endif

#if UNITY_FLASH
struct ICallString
{
	const char* utf8stream;

	std::string AsUTF8() const { return utf8stream; }
	operator std::string () const { return AsUTF8(); }

	int Length() { return strlen(utf8stream); }
	bool IsNull () {return !utf8stream;}
	const char* GetNativeString() {return utf8stream;}
};
#endif

#if UNITY_WINRT
struct ICallString
{
private:
	const wchar_t* str;
	//Platform::Object^ str;

	ICallString(const ICallString& other){}
	ICallString& operator = (const ICallString& rhs)
	{
		return *this;
	}
public:
	ICallString(){}
	ICallString(const wchar_t* _str)
		: str(_str)
	{
	}
	std::string AsUTF8() const;
	operator std::string () const { return AsUTF8(); }

	int Length();
	bool IsNull () {return (str == SCRIPTING_NULL);}

	ScriptingStringPtr GetNativeString() {return ref new Platform::String(str);}
};
#endif

/*
//For the next step

struct structwithsomename
{
char buf[256];
char* fallback;

structwithsomename(ICallString& ics)
{


}

~structwithsomename()
{
delete fallback;
}

const char* Get()
{
fastutf8into(buf);
}

const char* operator () { return Get(); }
};*/

#endif
