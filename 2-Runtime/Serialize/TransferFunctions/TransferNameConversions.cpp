#include "UnityPrefix.h"
#include "TransferNameConversions.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"


TranferNameConversionsManager::TranferNameConversionsManager()
{
	m_AllowTypeNameConversions = UNITY_NEW(AllowTypeNameConversions,kMemSerialization);
	m_AllowNameConversion = UNITY_NEW(AllowNameConversion,kMemSerialization);
}

TranferNameConversionsManager::~TranferNameConversionsManager()
{
	UNITY_DELETE(m_AllowTypeNameConversions,kMemSerialization);
	UNITY_DELETE(m_AllowNameConversion,kMemSerialization);
}

TranferNameConversionsManager* TranferNameConversionsManager::s_Instance = NULL;
void TranferNameConversionsManager::StaticInitialize()
{
	s_Instance = UNITY_NEW_AS_ROOT(TranferNameConversionsManager, kMemManager, "SerializationBackwardsCompatibility", "");
}
void TranferNameConversionsManager::StaticDestroy()
{
	UNITY_DELETE(s_Instance, kMemManager);
}
static RegisterRuntimeInitializeAndCleanup s_TranferNameConversionsManagerCallbacks(TranferNameConversionsManager::StaticInitialize, TranferNameConversionsManager::StaticDestroy);

bool AllowTypeNameConversion (const UnityStr& oldType, const char* newTypeName)
{
	pair<AllowTypeNameConversions::iterator, AllowTypeNameConversions::iterator> range;
	range = GetTranferNameConversionsManager().m_AllowTypeNameConversions->equal_range (const_cast<char*>(oldType.c_str()));
	for (;range.first != range.second;range.first++)
	{
		if (strcmp(range.first->second, newTypeName) == 0)
			return true;
	}
	
	// Special support for Mono PPtr's
	// With Unity 1.6 MonoBehaviour pointers have a special prefix and keep the class name in the PPtr. [ PPtr<$MyClass> ]
	// With Unity 1.5.1 it was simply PPtr<MonoBehaviour>. This made correct typechecking unneccessarily hard.
	// Here we provide backwards compatibility with the old method.
	if (strncmp("PPtr<$", newTypeName, 6) == 0)
	{
		if (oldType.find("PPtr<") == 0)
			return true;
	}
	
	return false;
}

const AllowNameConversion::mapped_type* GetAllowedNameConversions (const char* type, const char* name)
{
	const AllowNameConversion::mapped_type* nameConversion = NULL;
	AllowNameConversion::iterator foundNameConversion = GetTranferNameConversionsManager().m_AllowNameConversion->find(make_pair(const_cast<char*>(type), const_cast<char*>(name)));
	if (foundNameConversion != GetTranferNameConversionsManager().m_AllowNameConversion->end())
		nameConversion = &foundNameConversion->second;
	return nameConversion;
}

void RegisterAllowTypeNameConversion (const char* from, const char* to)
{
	GetTranferNameConversionsManager().m_AllowTypeNameConversions->insert(make_pair(const_cast<char*>(from), const_cast<char*>(to)));
}

void RegisterAllowNameConversion (const char* type, const char* oldName, const char* newName)
{
	AllowNameConversion::mapped_type& allowed = (*GetTranferNameConversionsManager().m_AllowNameConversion)[make_pair(const_cast<char*>(type), const_cast<char*>(newName))];
	allowed.insert (const_cast<char*>(oldName));
}

void ClearTypeNameConversion()
{
	GetTranferNameConversionsManager().m_AllowTypeNameConversions->clear();
	GetTranferNameConversionsManager().m_AllowNameConversion->clear();
}
