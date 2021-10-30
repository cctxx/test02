#ifndef PATHNAMEPERSISTENTMANAGER_H
#define PATHNAMEPERSISTENTMANAGER_H

#include "PersistentManager.h"
#include "SerializedFile.h"

class PathNamePersistentManager : public PersistentManager
{
	typedef map<string, SInt32>			PathToStreamID;
	PathToStreamID		m_PathToStreamID; // Contains lower case pathnames
	vector<string>		m_PathNames;// Contains pathnames as they were given
	
	public:
	
	PathNamePersistentManager (int options, int cacheCount = 2)
		: PersistentManager (options, cacheCount) {}

	protected:
	
	virtual int InsertPathNameInternal (const std::string& pathname, bool create);
	virtual int InsertFileIdentifierInternal (FileIdentifier file, bool create);

	virtual string PathIDToPathNameInternal (int pathID);

	virtual FileIdentifier PathIDToFileIdentifierInternal (int pathID);
};

void InitPathNamePersistentManager();


#endif
