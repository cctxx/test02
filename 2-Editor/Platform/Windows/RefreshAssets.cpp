#include "UnityPrefix.h"
#include "Editor/Platform/Interface/RefreshAssets.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/MetaFileUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Editor/Src/EditorSettings.h"

using namespace std;

bool ShouldIgnoreFile( const WIN32_FIND_DATAW& data ); // PlatformDependent\Win\File.cpp

void RecursiveRefreshInternal ( std::string &path, std::set<string>* needRefresh, std::set<string>* needRefreshMeta, std::set<string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options, bool useMetaFiles)
{ 
	path += "/";
	int oldSize = path.size();

	// find the first file
	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW( L"*", &findData);
	if( hFind == INVALID_HANDLE_VALUE )
		return;
	
	char lastFile[MAX_PATH] = "";
	while ( FindNextFileW( hFind, &findData ) )
	{		
		char utf8name[MAX_PATH];
		ConvertWindowsPathName( findData.cFileName, utf8name, MAX_PATH );
		size_t len = strlen (utf8name);
		bool isMeta = IsMetaFile (utf8name, len);

		path += utf8name;

		bool isAssetExists = true;
		if (isMeta && strncmp (lastFile, utf8name, len-5) != 0)
		{
			isAssetExists = false;
		}

		DateTime datetime;
		FileTimeToUnityTime( findData.ftLastWriteTime, datetime );

		// We don't want to ignore the meta file without the valid asset.
		// Only this case the isAssetExists could be false.
		if (isAssetExists && ShouldIgnoreFile(findData))
		{
			// If it's meta file with asset, we need to set the refreshFlag with kRefreshFoundMetaFile.
			if (isMeta)
			{
				path.resize (path.size() - 5);				

				if (database->DoesAssetFileNeedUpdate(path, datetime, false, true, true))
				{
					needRefreshMeta->insert (path);
				}

				lastFile[0] = '\0';
			}

			path.resize(oldSize);
			continue;
		}

		bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

		if (isMeta)
		{
			path.resize (path.size() - 5);
			if (useMetaFiles)
  			{
				if ( !isAssetExists )
				{
					if (!IsPathCreated(path)) 
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
				needRemoveMetafile->insert (path);

			(*metaFileCount)++;
		}

		if (database->DoesAssetFileNeedUpdate(path, datetime, isDirectory, isMeta))
		{
			if (isMeta)
				needRefreshMeta->insert (path);
			else
				needRefresh->insert (path);
		}
		
		if (isDirectory)
		{
			SetCurrentDirectoryW (findData.cFileName);
			RecursiveRefreshInternal (path, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles);
			SetCurrentDirectoryW (L"..");
		}
		path.resize (oldSize);

		if(isMeta)
			lastFile[0] = '\0';
		else
			strcpy (lastFile, utf8name);
	}
	FindClose( hFind );
}

void RecursiveRefresh (const char* path, std::set<string>* needRefresh, std::set<string>* needRefreshMeta, std::set<string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options)
{
	bool useMetaFiles = GetEditorSettings().GetVersionControlRequiresMetaFiles() || GetEditorSettings().GetExternalVersionControlSupport() == ExternalVersionControlAutoDetect;
	wchar_t oldPath[kDefaultPathBufferSize];
	GetCurrentDirectoryW (kDefaultPathBufferSize, oldPath);
	std::wstring widePath;
	ConvertUnityPathName( PathToAbsolutePath(path), widePath );
	SetCurrentDirectoryW (widePath.c_str());
	std::string strpath = path;
	RecursiveRefreshInternal (strpath, needRefresh, needRefreshMeta, needRemoveMetafile, database, metaFileCount, options, useMetaFiles);
	SetCurrentDirectoryW (oldPath);
}
