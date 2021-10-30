#ifndef FILE_SCANNING_H
#define FILE_SCANNING_H

#include "Runtime/Utilities/dynamic_array.h"

enum DirectoryEntryType
{
	kFile,
	kDirectory,
	kLink,
	kEndOfDirectory,
	kUnknown
};

struct DirectoryEntry
{
	char name[256];
	int type;
	
	DirectoryEntry ()
	{
		name[0] = '\0';
		type = kUnknown;
	}
	bool operator ==(const DirectoryEntry &other) const
	{
		if ((type == kEndOfDirectory) != (other.type == kEndOfDirectory))
			return false;
		return strcmp (name, other.name) == 0;
	}
	
	bool operator <(const DirectoryEntry &other) const
	{
		if ((type == kEndOfDirectory) != (other.type == kEndOfDirectory))
			return type < other.type;
		return strcmp (name, other.name) < 0;
	}
};

enum DirectorySearchMode { kDeepSearch = 0, kFlatSearch = 1 };

void GetDirectoryContents (const std::string& path, dynamic_array<DirectoryEntry>& allEntries, DirectorySearchMode mode);

void UpdatePathDirectoryOnly (DirectoryEntry& entry, std::string &path);

void RecursiveFindFilesWithExtension (const char* path, const char* extension, std::vector<std::string>& output);

#endif