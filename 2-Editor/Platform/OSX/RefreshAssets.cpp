#include "UnityPrefix.h"
#include "Editor/Platform/Interface/RefreshAssets.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/MetaFileUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/EditorSettings.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/vnode.h>

using namespace std;

bool ShouldIgnoreFile (const char* name, size_t length); // File.cpp

struct FInfoAttrBuf 
{
	unsigned long   length;
	attrreference_t name;
	fsobj_type_t    objType;
	struct timespec mtime;
	u_int32_t		flags;
};

#define kEntriesPerCall 64

inline bool ProcessFile (std::string &path, bool isDirectory, char *name, time_t time, bool isHidden, char *&lastFile, set<string>* needRefresh, set<string>* needRefreshMeta, set<string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options, bool useMetaFiles)
{
	size_t len = strlen(name);
	if (ShouldIgnoreFile(name, len))
		return false;

	bool isMeta = IsMetaFile(name, len);
	
	if (RequiresNormalization(name))
		path += NormalizeUnicode(name, false);
	else
		path += name;
	
	DateTime datetime;
	UnixTimeToUnityTime ( time, datetime);
	
	bool isAssetExist = true;
	
	// Ignore the hidden meta file.
	if (isMeta)
	{
		// If the last file is not the according asset.
		if (lastFile == NULL || strncmp(lastFile, name, len-5) != 0 )
			isAssetExist = false;
		
		// We don't want to ignore the meta file without the valid asset.
		if (isHidden && isAssetExist)
		{
			path.resize (path.size() - 5);
			
			if ( database->DoesAssetFileNeedUpdate(path, datetime, false, true, true) )
				needRefreshMeta->insert(path);
			
			lastFile = NULL;
			return true;
		}

		path.resize (path.size() - 5);
		if (useMetaFiles)
		{
			// Most of the times, the asset file is just the last one we looked at.
			// so check that before calling stat, as it's faster.
			if (lastFile == NULL || !isAssetExist)
			{
				struct stat statbuffer;
				if ( stat(path.c_str(), &statbuffer) != 0 )
				{
					if (IsTextMetaDataForFolder(path))
					{
						// For .meta files for folders we can actually remedy the situation and just create the folder.
						// This is useful e.g. when using Perforce that without VCS integration since it does not revision empty folders.
						if (CreateDirectory(PathToAbsolutePath(path)))
						{
							if (database->DoesAssetFileNeedUpdate(path, datetime, true, false))
								needRefresh->insert (path);
						}
						else
						{
							WarningString (Format("A meta data file (.meta) exists for folder but the folder '%s' can't be found and could not be created. When moving or deleting files outside of Unity, please ensure that the corresponding .meta file is moved or deleted along with it.", path.c_str()));
							needRemoveMetafile->insert(path);
						}
					}
					else
					{
						WarningString (Format("A meta data file (.meta) exists but its asset '%s' can't be found. When moving or deleting files outside of Unity, please ensure that the corresponding .meta file is moved or deleted along with it.", path.c_str()));
						needRemoveMetafile->insert(path);
					}
				}
			}
		}
		else
			needRemoveMetafile->insert(path);
		(*metaFileCount)++;
	}

	if (database->DoesAssetFileNeedUpdate(path, datetime, isDirectory, isMeta))
	{
		if (isMeta)
			needRefreshMeta->insert(path);
		else
			needRefresh->insert (path);
	}

	if (isMeta)
		lastFile = NULL;
	else
		lastFile = name;

	return true;
}	

void RecursiveRefreshInternalFast (struct attrlist &attrList, std::string &path, set<string>* needRefresh, set<string>* needRefreshMeta, set<string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options, bool useMetaFiles)
{
	// We add 64 bytes per entry for the file names.
	// While file names may be longer then this, on average they aren't. If the buffer won't hold the information
	// for all files, then we will get the next files in the next getdirentriesattr call.
	char attrBuf[kEntriesPerCall * (sizeof(FInfoAttrBuf) + 64)];

	int dir = open(path.c_str(), O_RDONLY, 0);
	if (dir < 0)
	{
		printf_console ("error opening directory %s\n", path.c_str());
        return;
	}

	path += "/";
	int oldSize = path.size();
	int result;
	char* lastFile = NULL;
	do
	{
		unsigned long count = kEntriesPerCall;
		unsigned long unused;
		unsigned long state;
		result = getdirentriesattr(dir, &attrList, &attrBuf, sizeof(attrBuf), &count, &unused, &state, 0);
		if (result < 0)
		{
			printf_console("error getting directory contents %s\n", path.c_str());
			break;
		}
		FInfoAttrBuf *thisEntry = (FInfoAttrBuf *) attrBuf;
		for (int i = 0; i < count; i++) 
		{
			char* name = ((char *)&thisEntry->name) + thisEntry->name.attr_dataoffset;
			bool isDirectory = thisEntry->objType == VDIR;
			if (thisEntry->objType == VLNK)
			{
				// If the file is a link, use stat to find out if it's a directory, as stat
				// will follow the link.
				path += name;
				struct stat statbuffer;
				stat(path.c_str(), &statbuffer);
				isDirectory = S_ISDIR(statbuffer.st_mode);
				path.resize(oldSize);
			}
			if (ProcessFile (path, isDirectory, name, thisEntry->mtime.tv_sec, (thisEntry->flags & UF_HIDDEN) > 0, lastFile, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles))
			{
				if (isDirectory)
					RecursiveRefreshInternalFast (attrList, path, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles);
				path.resize(oldSize);
			}
			thisEntry = (FInfoAttrBuf *)(((char *)thisEntry) + thisEntry->length);
		}
 	}
	while (result == 0);
	
	close(dir);
}


void RecursiveRefreshInternalSafe (std::string &path, set<string>* needRefresh, set<string>* needRefreshMeta, set<string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options, bool useMetaFiles)
{
	DIR *dirp;
    struct dirent *dp;	
	
	if ((dirp = opendir(path.c_str())) == NULL)
        return;

	path += "/";
	int oldSize = path.size();

	char* lastFile = NULL;
	while ( (dp = readdir(dirp)) )
	{
		path += dp->d_name;
		struct stat statbuffer;
		stat(path.c_str(), &statbuffer);
		bool isDirectory = S_ISDIR(statbuffer.st_mode);
		path.resize(oldSize);
		
		if (ProcessFile (path, isDirectory, dp->d_name, statbuffer.st_mtime, (statbuffer.st_flags & UF_HIDDEN) > 0, lastFile, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles))
		{
			if (isDirectory)
				RecursiveRefreshInternalSafe (path, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles);
			path.resize(oldSize);
		}	
	}
	closedir(dirp);
}


void RecursiveRefresh (const char *path, set<string>* needRefresh, set<string>* needRefreshMeta, set<string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options)
{
	std::string strpath = path;
	bool useMetaFiles = GetEditorSettings().GetVersionControlRequiresMetaFiles() || GetEditorSettings().GetExternalVersionControlSupport() == ExternalVersionControlAutoDetect;

	// We have two implementations of RecursiveRefresh.
	// One uses readdir & stat, which should work everywhere
	// The other one is about 1.6 x faster, but uses getdirentriesattr, which (According to "Mac OS X Internals: A Systems Approach")
	// is only supported on HFS/HFS+/AFP.

	// Use statfs to get file system mount point for path.
	struct statfs statfsbuffer;
	if (statfs  (path, &statfsbuffer) == 0)
	{
		// check capabilities for file system.
		struct attrlist attrList;
		memset(&attrList, 0, sizeof(attrList));
		attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
		
		vol_capabilities_attr_t capabilities;
		attrList.volattr = ATTR_VOL_CAPABILITIES;
		if (getattrlist(statfsbuffer.f_mntonname, &attrList, &capabilities, sizeof(capabilities), 0) == 0)
		{
			// test if file system supports getdirentriesattr
			if (capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_READDIRATTR)
			{
				attrList.volattr = 0;
				attrList.commonattr = ATTR_CMN_NAME | ATTR_CMN_OBJTYPE | ATTR_CMN_MODTIME | ATTR_CMN_FLAGS;
				RecursiveRefreshInternalFast (attrList, strpath, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles);
				return;
			}
		}
	}
	
	printf_console("File system does not support getdirentriesattr, performing slow refresh.\n");
	RecursiveRefreshInternalSafe (strpath, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles);
}


