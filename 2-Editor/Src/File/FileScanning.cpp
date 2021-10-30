#include "UnityPrefix.h"
#include "FileScanning.h"
#include "Runtime/Utilities/PathNameUtility.h"

using namespace std;


#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"

bool ShouldIgnoreFile( const WIN32_FIND_DATAW& data );

void GetDirectoryContentsRecurse (const char* relativePath, dynamic_array<DirectoryEntry> &allEntries, DirectorySearchMode mode)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName (relativePath, widePath, kDefaultPathBufferSize);
	SetCurrentDirectoryW (widePath);

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW (L"*", &findData);
	if ( hFind == INVALID_HANDLE_VALUE )
		return;

	dynamic_array<DirectoryEntry> dirEntries(kMemTempAlloc);

	std::string utf8Name;
	while ( FindNextFileW (hFind, &findData))
	{
		if (ShouldIgnoreFile(findData))
			continue;

		ConvertWindowsPathName (findData.cFileName, utf8Name);

		DirectoryEntry* de;
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && mode == kDeepSearch)
		{
			if (dirEntries.empty())
				dirEntries.reserve (100);

			de = &dirEntries.push_back();
		}
		else
			de = &allEntries.push_back();
		
		strcpy (de->name, utf8Name.c_str());

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			de->type = kDirectory;
		else	
			de->type = kFile;
	}
	// TODO handle errors in FindNextFile
	FindClose (hFind);

	for (dynamic_array<DirectoryEntry>::iterator i = dirEntries.begin(); i != dirEntries.end(); i++)
	{
		allEntries.push_back (*i);
		GetDirectoryContents(i->name, allEntries, kDeepSearch);
	}
	
	DirectoryEntry& de = allEntries.push_back();
	de.name[0] = '\0';
	de.type = kEndOfDirectory;

	SetCurrentDirectoryW (L"..");
}

void GetDirectoryContents (const std::string& path, dynamic_array<DirectoryEntry> &allEntries, DirectorySearchMode mode)
{
	// By chdiring to the directory, we avoid doing any AppendPathName(), and the memory allocation overhead
	// from creating all those strings.
	wchar_t oldPath[kDefaultPathBufferSize];
	GetCurrentDirectoryW (kDefaultPathBufferSize, oldPath);
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName (DeleteLastPathNameComponent(path).c_str(), widePath, kDefaultPathBufferSize);
	SetCurrentDirectoryW (widePath);
	
	GetDirectoryContentsRecurse (GetLastPathNameComponent(path).c_str(), allEntries, mode);
	
	SetCurrentDirectoryW (oldPath);
}
#else
#include <dirent.h>
#include <sys/stat.h>

bool ShouldIgnoreFile (const char* name, size_t length); // File.cpp

void GetDirectoryContentsRecurse (const char* relativePath, dynamic_array<DirectoryEntry> &allEntries, DirectorySearchMode mode)
{
	DIR *dirp;
    struct dirent *dp;	
	
	if ((dirp = opendir(relativePath)) == NULL)
        return;

	dynamic_array<DirectoryEntry> dirEntries;
	
	chdir (relativePath);
	
	while ( (dp = readdir(dirp)) )
	{
		size_t len = strlen(dp->d_name);
		if (ShouldIgnoreFile(dp->d_name, len))
			continue;

		DirectoryEntry* de;
		if (dp->d_type == DT_DIR && mode == kDeepSearch)
		{
			if (dirEntries.empty())
				dirEntries.reserve (100);

			de = &dirEntries.push_back();
		}
		else
			de = &allEntries.push_back();
		
		strcpy (de->name, dp->d_name);

		switch (dp->d_type)
		{
			case DT_DIR:
				de->type = kDirectory;
				break;
			case DT_LNK:
				de->type = kLink;
				break;
			case DT_REG:
				de->type = kFile;
				break;
			default:
				Assert("Unsupported file type");
				de->type = kUnknown;
		}
	}
	closedir(dirp);

	for (dynamic_array<DirectoryEntry>::iterator i = dirEntries.begin(); i != dirEntries.end(); i++)
	{
		allEntries.push_back (*i);
		GetDirectoryContents(i->name, allEntries, kDeepSearch);
	}
	
	DirectoryEntry& de = allEntries.push_back();
	de.name[0] = '\0';
	de.type = kEndOfDirectory;

	chdir ("..");
}

void GetDirectoryContents (const std::string& path, dynamic_array<DirectoryEntry> &allEntries, DirectorySearchMode mode)
{
	// By chdiring to the directory, we avoid doing any AppendPathName(), and the memory allocation overhead
	// from creating all those strings.
	char *oldPath = getcwd(NULL, 0);
	chdir (DeleteLastPathNameComponent(path).c_str());
	
	GetDirectoryContentsRecurse (GetLastPathNameComponent(path).c_str(), allEntries, mode);
	
	chdir (oldPath);
	free (oldPath);
}

#endif

void UpdatePathDirectoryOnly (DirectoryEntry& entry, std::string &path)
{
	if (entry.type == kDirectory)
		path = AppendPathName(path, entry.name);
	else if (entry.type == kEndOfDirectory)
		path = DeleteLastPathNameComponent (path);
}

void RecursiveFindFilesWithExtension (const char* path, const char* extension, std::vector<std::string>& output)
{
	dynamic_array<DirectoryEntry> foundEntries;
	
	GetDirectoryContents (path, foundEntries, kDeepSearch);
	
	std::string curPath = path;
	
	for (int i=0;i<foundEntries.size();i++)
	{
		if (foundEntries[i].type == kFile)
		{
			int len = strlen(foundEntries[i].name);
			
			// Scan for extension and add to output
			if (StrICmp(GetPathNameExtension(foundEntries[i].name, len), extension) == 0)
				output.push_back(AppendPathName(curPath, foundEntries[i].name) );
		}
		
		UpdatePathDirectoryOnly (foundEntries[i], curPath);
	}
}
