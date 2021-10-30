#pragma once

#include "PlatformDependent/Win/WinUnicode.h"

namespace dirchanges {

// matches WinAPI FILE_ACTION_* values
enum ChangeActions {
	kFileAdded = 1,				// file was added
	kFileRemoved = 2,			// file was removed
	kFileModified = 3,			// file was modified
	kFileRenamedOldName = 4,	// file renamed, this is old name
	kFileRenamedNewName = 5,	// file renamed, this is new name
};

// matches WinAPI FILE_NOTIFY_CHANGE_* values
enum NotifyFlags {
	kNotifyChangeFileName = 0x1,
	kNotifyChangeDirName = 0x2,
	kNotifyChangeSize = 0x8,
	kNotifyChangeTouched = 0x10,
	kNotifyChangeCreated = 0x40,

	kDefaultNotifyFlags = kNotifyChangeFileName | kNotifyChangeDirName | kNotifyChangeSize | kNotifyChangeTouched | kNotifyChangeCreated,
};


struct ChangeRecord
{
	std::string	path; // UTF8
	ChangeActions action;
};

typedef std::vector<ChangeRecord> ChangeRecords;

class DirectoryWatcher;

DirectoryWatcher* StartWatchingDirectories( const int count, const char** dirs, UInt32 notifyFlags );
bool GetDirectoryChanges( DirectoryWatcher* obj, ChangeRecords* into );
bool DiscardDirectoryChanges( DirectoryWatcher* obj );
void StopWatchingDirectories( DirectoryWatcher* obj );

}; // namespace
