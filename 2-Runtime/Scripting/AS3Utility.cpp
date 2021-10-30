#include "UnityPrefix.h"
#include "ScriptingUtility.h"
#include "Runtime/Utilities/LogAssert.h" 
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingArguments.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

extern "C" bool NativeExt_SendMessage(ScriptingStringPtr path, ScriptingStringPtr method, ScriptingObjectPtr value)
{
	Transform* transform = FindActiveTransformWithPath( (const char*) path );
	if( transform != NULL)
	{
		return Scripting::SendScriptingMessage( transform->GetGameObject(), (const char*) method, value );
	}
	return false;
}

bool ReadStringFromFile (TEMP_STRING* outData, const string& path)
{
	const char* outDataFlash = Ext_FileContainer_ReadStringFromFile(path.c_str());
	if(outDataFlash != NULL){
		*outData = outDataFlash;
		return true;
	}
	return false;
}

ScriptingObjectPtr ScriptingInstantiateObjectFromClassName(const char* name)
{
	return Ext_GetNewMonoBehaviour(name); 
}
