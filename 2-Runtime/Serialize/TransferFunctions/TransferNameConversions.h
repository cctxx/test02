#ifndef TRANSFERNAMECONVERSIONS_H
#define TRANSFERNAMECONVERSIONS_H

#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Modules/ExportModules.h"

using namespace std;

struct smaller_cstring_pair : std::binary_function<std::pair<char*, char*>, std::pair<char*, char*>, std::size_t>
{
	bool operator () (pair<char*, char*> lhs, pair<char*, char*> rhs) const
	{
		int first = strcmp (lhs.first, rhs.first);
		if (first != 0)
			return first < 0;
		else
			return strcmp (lhs.second, rhs.second) < 0;
	}
};

typedef std::multimap<char*, char*, smaller_cstring> AllowTypeNameConversions;
typedef std::map<std::pair<char*, char*>, set<char*, smaller_cstring>,  smaller_cstring_pair> AllowNameConversion;

class TranferNameConversionsManager
{
public:
	AllowTypeNameConversions* m_AllowTypeNameConversions;
	AllowNameConversion* m_AllowNameConversion;

	TranferNameConversionsManager();
	~TranferNameConversionsManager();
	
	static TranferNameConversionsManager* s_Instance;
	static void StaticInitialize();
	static void StaticDestroy();
};
inline TranferNameConversionsManager& GetTranferNameConversionsManager() { return *TranferNameConversionsManager::s_Instance; }


/// Allows type name conversion from oldTypeName to newTypeName(The passed strings will not be copied so you can only pass in constant strings)
/// (Useful for depracating types -> RegisterAllowTypeNameConversion ("UniqueIdentifier", "GUID");)
/// "UniqueIdentifier" can now be renamed to "GUID" and serialization will just work!
void RegisterAllowTypeNameConversion (const char* oldTypeName, const char* newTypeName);

/// Allows name conversion from oldName to newName? (The passed strings will not be copied so you can only pass in constant strings)
/// (Useful for deprecating names -> m_NewPosition will now load from m_DeprecatedPosition in an old serialized file
/// RegisterAllowNameConversion (MyClass::GetClassStringStatic(), "m_DeprecatedPosition", "m_NewPosition");
EXPORT_COREMODULE void RegisterAllowNameConversion (const char* type, const char* oldName, const char* newName);

const AllowNameConversion::mapped_type* GetAllowedNameConversions (const char* type, const char* name);

bool AllowTypeNameConversion (const UnityStr& oldType, const char* newTypeName);

void ClearTypeNameConversion ();

#endif
