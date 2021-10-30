#include "UnityPrefix.h"
#if UNITY_FLASH

#include "ScriptLanguagePortUtility.h"

bool IsFileCreated(const std::string& path)
{
	return Ext_FileContainer_IsFileCreatedAt(path.c_str());
}

#endif