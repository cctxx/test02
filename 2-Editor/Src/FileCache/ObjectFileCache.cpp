#include "UnityPrefix.h"
#include "Editor/Src/FileCache/ObjectFileCache.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Serialize/PersistentManager.h"

ObjectFileCache::ObjectFileCache (const std::string& cachePath, UInt64 maximumCacheSize)
: LocalFileCache (cachePath, maximumCacheSize)
{
}

SInt32 ObjectFileCache::LoadAndGetInstanceID (const MdFour& hash, LocalIdentifierInFileType fileID)
{
	DirtyModificationDate(hash);
	
	std::string path = GetPath(hash);
	if (IsPathCreated(path))
	{
		GetPersistentManager().LoadFileCompletely(path);
		SInt32 instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(path, fileID);
		return instanceID;
	}
	else
		return 0;
}

void ObjectFileCache::Store (const MdFour& hash, dynamic_array<Object*>& objects)
{
	RemoveFile(hash);
	
	std::string path = GetPath(hash);
	LocalIdentifierInFileType fileID = 1;
	for (dynamic_array<Object*>::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		Object* obj = *it;
		int heapdID = obj->GetInstanceID();
		GetPersistentManager().MakeObjectsPersistent(&heapdID, &fileID, 1, path);
		++fileID;
	}
	
	CreateDirectory(m_CachePath);
	GetPersistentManager().WriteFile(path, BuildTargetSelection::NoTarget());
	AddFile(hash);
}

void ObjectFileCache::OnRemoveFile (const MdFour& hash)
{
	std::string path = GetPath(hash);
	GetPersistentManager().DeleteFile (path, PersistentManager::kDontDeleteLoadedObjects);
}
