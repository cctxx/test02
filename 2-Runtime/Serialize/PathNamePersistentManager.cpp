#include "UnityPrefix.h"
#include "PathNamePersistentManager.h"
#include "Runtime/Utilities/Word.h"

using namespace std;

int PathNamePersistentManager::InsertPathNameInternal (const string& pathname, bool create)
{
	SET_ALLOC_OWNER(NULL);
	AssertIf (!pathname.empty () && (pathname[0] == '/' || pathname[0] == '\\'));

	string lowerCasePathName = ToLower (pathname);
	
	PathToStreamID::iterator found = m_PathToStreamID.find (lowerCasePathName);
	if (found != m_PathToStreamID.end())
		return found->second;

	if (create)
	{
		m_PathToStreamID.insert (make_pair (lowerCasePathName, m_PathNames.size ()));
		m_PathNames.push_back (pathname);
		AddStream ();
		return m_PathNames.size () - 1;
	}
	else
		return -1;
}

int PathNamePersistentManager::InsertFileIdentifierInternal (FileIdentifier file, bool create)
{
	return InsertPathNameInternal(file.pathName, create);
}

FileIdentifier PathNamePersistentManager::PathIDToFileIdentifierInternal (int pathID)
{
	AssertIf (pathID < 0 || pathID >= m_PathNames.size ());
	FileIdentifier f;
	f.pathName = m_PathNames[pathID];
	return f;
}

string PathNamePersistentManager::PathIDToPathNameInternal (int pathID)
{
	AssertIf (pathID < 0 || pathID >= m_PathNames.size ());
	return m_PathNames[pathID];
}

void InitPathNamePersistentManager()
{
	UNITY_NEW_AS_ROOT( PathNamePersistentManager(0), kMemManager, "PathNameManager", "");
}
