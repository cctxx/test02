#ifndef _SCRIPTINGTYPEPROVIDER_FLASH
#define _SCRIPTINGTYPEPROVIDER_FLASH

#include "../IScriptingTypeProvider.h"
//#include "Runtime/Scripting/ScriptingUtility.h"

#if UNITY_FLASH

extern "C" ScriptingTypePtr Ext_Flash_GetScriptingTypeFromName(const char* name);

class ScriptingTypeProvider_Flash : public IScriptingTypeProvider
{
public:
	virtual BackendNativeType NativeTypeFor(const char* namespaze, const char* name)
	{
		if (strcmp(name,"Object") == 0)
			name = "_Object";

		if (strcmp(namespaze,"System")==0)
		{
			if (strcmp(name,"String")==0)
				return Ext_Flash_GetScriptingTypeFromName("String");
			if (strcmp(name,"Int32")==0)
				return Ext_Flash_GetScriptingTypeFromName("int");
			if (strcmp(name,"Single")==0)
				return Ext_Flash_GetScriptingTypeFromName("Number");
			if (strcmp(name,"Double")==0)
				return Ext_Flash_GetScriptingTypeFromName("Number");
			if (strcmp(name,"Byte")==0)
				return Ext_Flash_GetScriptingTypeFromName("int");
		}

		std::string combined(namespaze);
		if (combined.size() > 0)
			combined+=".";
		combined+=name;
		
		return Ext_Flash_GetScriptingTypeFromName(combined.c_str());
	}

	virtual ScriptingTypePtr Provide(BackendNativeType nativePtr)
	{
		return (ScriptingTypePtr)nativePtr;
	}

	virtual void Release(ScriptingTypePtr t)
	{
	}
};

#endif

#endif
