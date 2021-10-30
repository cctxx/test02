#include "UnityPrefix.h"
#if ENABLE_MONO
#include "MonoExportUtility.h"
#include "MonoTypeSignatures.h"
#include "MonoScript.h"
#include "MonoScriptCache.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/ScriptingUtility.h"


#if !GAMERELEASE
#include "Editor/Src/Utility/ObjectNames.h"
#endif

using namespace std;

#if UNITY_EDITOR
static ScriptingClassPtr ClassIDToScriptingClassIncludingBasicTypes (int classID)
{
	if (classID == ClassID (Undefined))
		return NULL;
	else if (classID <= kLargestEditorClassID)
		return Scripting::ClassIDToScriptingType (classID);
	else if (classID == ClassID (int))
		return mono_get_int32_class ();
	else if (classID == ClassID (bool))
		return mono_get_boolean_class();
	else if (classID == ClassID (float))
		return mono_get_single_class();
	else
	{
		AssertString("Unsupported classID value");
		return NULL;
	}
}

MonoObject* ClassIDToScriptingTypeObjectIncludingBasicTypes (int classID)
{
	ScriptingClassPtr klass = ClassIDToScriptingClassIncludingBasicTypes (classID);
	if (klass == NULL)
		return NULL;
	
	return mono_type_get_object(mono_domain_get(), mono_class_get_type(klass));
}

#endif


#endif //ENABLE_MONO
