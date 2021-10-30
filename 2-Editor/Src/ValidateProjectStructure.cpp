#include "UnityPrefix.h"
#include "Editor/Src/File/FileScanning.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Profiler/TimeHelper.h"

static bool IsProjectLibraryDirectory (const std::string& path)
{
	if (StrICmp(GetLastPathNameComponent(path), "Library") != 0)
		return false;
	
	dynamic_array<DirectoryEntry> directories;
	GetDirectoryContents (path, directories, kFlatSearch);
	
	for (int i=0;i<directories.size();i++)
	{
		// Scan for extension and add to output
		if (directories[i].type == kFile && StrICmp(GetPathNameExtension(directories[i].name), "asset") == 0)
			return true;
	}
	
	return false;
}

static std::string ValidateProjectFolderStructure (const std::string& projectPath)
{
	if (IsProjectLibraryDirectory (projectPath))
		return projectPath;
	
	dynamic_array<DirectoryEntry> directories;
	GetDirectoryContents (projectPath, directories, kDeepSearch);
	
	std::string curPath = projectPath;
	
	std::string errorOutput;
	for (int i=0;i<directories.size();i++)
	{
		UpdatePathDirectoryOnly (directories[i], curPath);
		
		// Scan for extension and add to output
		if (directories[i].type == kDirectory && StrICmp(directories[i].name, "Library") == 0)
		{
			if (IsProjectLibraryDirectory (curPath))
				errorOutput += curPath + '\n';
		}
	}
	
	return errorOutput;
}

bool ValidateDragAndDropDirectoryStructure (const std::string& sourcePath)
{
	if (IsFileCreated(sourcePath))
		return true;
	
	return ValidateProjectFolderStructure (sourcePath).empty();
}

bool ValidateDragAndDropDirectoryStructure (const std::vector<std::string>& paths)
{
	for (unsigned int i = 0; i < paths.size(); ++i)
	{
		if (!ValidateDragAndDropDirectoryStructure(paths[i]))
		{		
			ErrorString (Format("Dragged directory contains a project's Library folder (%s). This is not allowed.", paths[i].c_str ()));
			return false;
		}
	}
	return true;
}

void ValidateProjectStructureAndAbort (const std::string& projectPath)
{
	printf_console("Validating Project structure ... ");
	
	ABSOLUTE_TIME validateStartTime = START_TIME;
	
	std::string invalidLibraryDirectories = ValidateProjectFolderStructure(AppendPathName(projectPath, "Assets"));
	
	if (!invalidLibraryDirectories.empty())
	{
		std::string message;
		message += "Detected multiple Library folders within your project.\n\nDid you copy a project into this project? Use \"Export Package\" to move assets between projects.\n\n\nYou need to remove the following Library folder(s) from your project before you can open it again:\n\n";
		message += invalidLibraryDirectories;
		
		FatalErrorStringDontReport (message);
	}
	
	printf_console("%f seconds.\n", GetElapsedTimeInSeconds(validateStartTime));
}
