#include "GraphicsScriptingUtility.h"
#include "Runtime/Scripting/ICallString.h"

#if ENABLE_MONO
# include "Runtime/Mono/MonoIncludes.h"
# include "Runtime/Scripting/ScriptingUtility.h"
#endif

ShaderLab::FastPropertyName ScriptingStringToProperty(ICallString& iCallString)
{
	if(iCallString.IsNull())
		return ShaderLab::FastPropertyName();

#if ENABLE_MONO
	MonoString* msname = iCallString.str;
	int const kShortString = 255;
	char namebuf [kShortString+1];

	if( msname->length <= kShortString && FastTestAndConvertUtf16ToAscii(namebuf, mono_string_chars(msname), mono_string_length(msname)))
	{
		namebuf [mono_string_length(msname)] = '\0';
		return ShaderLab::Property(namebuf);
	}

	char* name = mono_string_to_utf8(msname);
	ShaderLab::FastPropertyName propertyName(ShaderLab::Property(name));
	g_free(name);

	return propertyName;

#else

	return ShaderLab::Property(iCallString.AsUTF8().c_str());

#endif
}