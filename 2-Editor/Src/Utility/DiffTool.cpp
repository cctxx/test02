#include "UnityPrefix.h"
#include "DiffTool.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Utilities/FileUtilities.h"
#if UNITY_WIN
#include <ShlObj.h>
#include "PlatformDependent/Win/Registry.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif

enum AvailablePlatforms {APMac, APWin, APAll};

struct DiffToolDesc {
	std::string name;
	AvailablePlatforms platforms;
	const char** possiblePaths;
	const char* registryPath; // used to detect presence on Windows only
	const char* diff2Command;
	const char* diff3Command;
	const char* mergeCommand;
	bool createEmptyOutputFile;
	std::string detectedPath;
};

static const char* kAppleFileMergePaths[] = {
	"/usr/bin/opendiff",
	NULL
};
static const char* kSourcegearDiffMergePaths[] = {
	"/Applications/DiffMerge.app/Contents/MacOS/DiffMerge",
	"~/Applications/DiffMerge.app/Contents/MacOS/DiffMerge",
	"/Applications/Utilities/DiffMerge.app/Contents/MacOS/DiffMerge",
	"~/Applications/Utilities/DiffMerge.app/Contents/MacOS/DiffMerge",
	"~/SourceGear/DiffMerge/DiffMerge.exe",
	NULL
};
static const char* kTkDiffPaths[] = {
	"/Applications/TkDiff.app/Contents/MacOS/Wish Shell",
	"~/Applications/TkDiff.app/Contents/MacOS/Wish Shell",
	"/Applications/Utilities/TkDiff.app/Contents/MacOS/Wish Shell",
	"~/Applications/Utilities/TkDiff.app/Contents/MacOS/Wish Shell",
	"~/TkDiff/tkdiff.exe",
	NULL
};
static const char* kP4MergePaths[] = {
	"/Applications/p4merge.app/Contents/Resources/launchp4merge",
	"~/Applications/p4merge.app/Contents/Resources/launchp4merge",
	"/Applications/Utilities/p4merge.app/Contents/Resources/launchp4merge",
	"~/Applications/Utilities/p4merge.app/Contents/Resources/launchp4merge",
	"~/Perforce/p4merge.exe",
	NULL
};

static const char* kPlasticSCMMergePaths[] = {
	"/Applications/PlasticSCM/client/mergetool",
	"~/PlasticSCM6/client/mergetool.exe",
	"~/PlasticSCM5/client/mergetool.exe",
	"~/PlasticSCM4/client/mergetool.exe",
	NULL
};


static DiffToolDesc g_DiffTools[] = {
{
	"Apple File Merge",
	APMac,
	kAppleFileMergePaths,
	NULL,
	"#LEFT #RIGHT",
	"#LEFT #RIGHT -ancestor #ANCESTOR",
	"#LEFT #RIGHT -ancestor #ANCESTOR -merge #OUTPUT",
	false,
	""
},
{
	"SourceGear DiffMerge",
	APAll,
	kSourcegearDiffMergePaths,
	NULL,
	"-t1 #LTITLE -t2 #RTITLE #LEFT #RIGHT",
	"-t1 #LTITLE -t2 #RTITLE -t3 #ATITLE -ro2 #LEFT #ANCESTOR #RIGHT",
	"-m -t1 #LTITLE -t2 #RTITLE -t3 #ATITLE -r #OUTPUT #LEFT #ANCESTOR #RIGHT",
	false,
	""
},
{
	"TkDiff",
	APAll,
	kTkDiffPaths,
	NULL,
	"#LEFT #RIGHT -L #LTITLE -L #RTITLE",
	"#LEFT #RIGHT -L #LTITLE -L #RTITLE -a #ANCESTOR",
	"#LEFT #RIGHT -L #LTITLE -L #RTITLE -a #ANCESTOR -o #OUTPUT",
	false,
	""
},
{
	"P4Merge",
	APAll,
	kP4MergePaths,
	NULL,
	"#ABSLEFT #ABSRIGHT",
	"#ABSANCESTOR #ABSLEFT #ABSRIGHT",
	"#ABSANCESTOR #ABSLEFT #ABSRIGHT #ABSOUTPUT",
	true,
	""
},
{
	"TortoiseMerge",
	APWin,
	NULL,
	"SOFTWARE/TortoiseSVN/TMergePath",
	"/base:#LEFT /mine:#RIGHT /basename:#LTITLE /minename:#RTITLE /readonly",
	"/theirs:#LEFT /mine:#RIGHT /theirsname:#LTITLE /minename:#RTITLE /base:#ANCESTOR /basename:#ATITLE /readonly",
	"/base:#ABSANCESTOR /theirs:#ABSLEFT /mine:#ABSRIGHT /theirsname:#LTITLE /minename:#RTITLE /merged:#ABSOUTPUT /mergedname:#ATITLE", // FIXME: mergedname should not be "ancestor title", but that's how it was used before, so don't want to break anything
	false,
	""
},
{
	"WinMerge",
	APWin,
	NULL,
	"SOFTWARE/Thingamahoochie/WinMerge/Executable",
	"#LEFT #RIGHT /e /x /u /wl /wr /dl #LTITLE /dr #RTITLE",
	"#LEFT #RIGHT /e /x /u /wl /wr /dl #LTITLE /dr #RTITLE",
	"#LEFT #RIGHT #OUTPUT /e /x /u /dl #LTITLE /dr #RTITLE",
	false,
	""
},
{
	"PlasticSCM Merge",
	APAll,
	kPlasticSCMMergePaths,
	NULL,
	"-s=#ABSLEFT -sn=#LTITLE -d=#ABSRIGHT -dn=#RTITLE",
	"-s=#ABSLEFT -sn=#LTITLE -d=#ABSRIGHT -dn=#RTITLE -b=#ABSANCESTOR -bn=#ATITLE",
	"-s=#ABSLEFT -sn=#LTITLE -d=#ABSRIGHT -dn=#RTITLE -b=#ABSANCESTOR -bn=#ATITLE -r=#ABSOUTPUT",
	false,
	""
}
///@TODO: add some other common tools on Windows
};

static const char kDiffToolCount = ARRAY_SIZE(g_DiffTools);

static bool g_DiffToolsInitialized = false;

static void InitializeDiffTools()
{
	if( g_DiffToolsInitialized )
		return;

	#if UNITY_WIN
	// we'll need program files path
	wchar_t widePath[kDefaultPathBufferSize];
	string pfilesPath;
	const string defaultpFilesPath = "C:/Program Files";
	if (SHGetSpecialFolderPathW(NULL, widePath, CSIDL_PROGRAM_FILES, false))
		ConvertWindowsPathName(widePath, pfilesPath);
	else
		pfilesPath = defaultpFilesPath;
	#endif

	for( int i = 0; i < kDiffToolCount; ++i ) 
	{
		DiffToolDesc& tool = g_DiffTools[i];
		// detect based on known paths
		if( tool.possiblePaths != NULL ) 
		{
			for( int j = 0; tool.possiblePaths[j] != NULL; ++j ) 
			{
				std::string path = tool.possiblePaths[j];

				#if UNITY_WIN
				// on windows replace ~ with program files folder
				if (path[0] == '~')
				{
					path.replace(path.begin(), path.begin() + 1, pfilesPath.begin(), pfilesPath.end());
				}

				#elif UNITY_OSX || UNITY_LINUX
				///@TODO: on Unixes if first char is '~', replace with home folder
				#else
				#error "Unknown platform"
				#endif

				if( IsFileCreated(path) )
				{
					tool.detectedPath = path;
					break;
				}
				
				// Explictly check for 64 bit binaries if needed
				#if UNITY_WIN
				path = tool.possiblePaths[j];
				if (path[0] == '~' && pfilesPath != defaultpFilesPath)
				{
					path.replace(path.begin(), path.begin() + 1, defaultpFilesPath.begin(), defaultpFilesPath.end());
				}
				
				if( IsFileCreated(path) )
				{
					tool.detectedPath = path;
					break;
				}
				#endif
			}
		}
		// windows only: detect based on registry
		#if UNITY_WIN
		if( tool.registryPath != NULL ) {
			std::string regPath = tool.registryPath;
			std::string regKey = DeleteLastPathNameComponent(regPath);
			std::string regValue = GetLastPathNameComponent(regPath);
			ConvertSeparatorsToWindows(regKey);
			ConvertSeparatorsToWindows(regValue);
			std::string path = registry::getString( regKey, regValue, "" );
			if( !path.empty() ) {
				ConvertSeparatorsToUnity(path);
				tool.detectedPath = path;
			}
		}
		#endif
	}
	
	g_DiffToolsInitialized = true;
}

std::vector<std::string> GetAvailableDiffTools()
{
	InitializeDiffTools();
	std::vector<std::string> result;
	for( int i = 0; i < kDiffToolCount; ++i ) {
		const DiffToolDesc& tool = g_DiffTools[i];
		if( !tool.detectedPath.empty() )
			result.push_back( tool.name );
	}
	return result;
}

static int FindDiffTool()
{
	InitializeDiffTools();
	std::string name = EditorPrefs::GetString("kDiffsDefaultApp");
	int firstAvailableDiffTool = -1;
	for( int i = 0; i < kDiffToolCount; ++i ) {
		const DiffToolDesc& tool = g_DiffTools[i];
		if( !tool.detectedPath.empty() )
		{
			if( firstAvailableDiffTool == -1 )
				firstAvailableDiffTool = i;
			if( tool.name == name )
				return i;
		}
	}

	if( firstAvailableDiffTool != -1 ) {
		name = g_DiffTools[firstAvailableDiffTool].name;
		EditorPrefs::SetString("kDiffsDefaultApp", name);
	}
	return -1;
}

string GetNoDiffToolsDetectedMessage()
{
	InitializeDiffTools();	

	string message = "No supported Asset Server diff tools were found. Please install one of the following tools:";
	for( int i = 0; i < kDiffToolCount; ++i ) 
	{
		DiffToolDesc& tool = g_DiffTools[i];
		#if UNITY_WIN
		if (tool.platforms == APWin || tool.platforms == APAll)
			message += "\n\t- " + tool.name;
		#elif UNITY_OSX || UNITY_LINUX
		if (tool.platforms == APMac || tool.platforms == APAll)
			message += "\n\t- " + tool.name;
		#else
		#error "Unknown platform"
		#endif
	}

	return message;
}

static void ReplaceTokenInArgs( std::vector<std::string>& args, const std::string& token, const std::string& value )
{
	size_t n = args.size();
	for( size_t i = 0; i < n; ++i ) {
		std::string& arg = args[i];
		if( arg == token ) {
			arg = value;
		} else {
			size_t pos = arg.find(token);
			if( pos != std::string::npos ) {
				arg.erase( pos, token.size() );
				arg.insert( pos, value );
			}
		}
	}
}

static inline std::string OptionalQuote( const std::string& s )
{
	#if UNITY_WIN
	return QuoteString(s);
	#else
	return s;
	#endif
}

static std::vector<std::string> ProcessCommandLine(
		const std::string& command,
		const std::string& leftTitle, const std::string& leftFile,
		const std::string& rightTitle, const std::string& rightFile,
		const std::string& ancestorTitle, const std::string& ancestorFile,
		const std::string& outputFile )
{
	std::vector<std::string> args = FindSeparatedPathComponents(command, ' ');
	ReplaceTokenInArgs( args, "#LTITLE", OptionalQuote(leftTitle) );
	ReplaceTokenInArgs( args, "#RTITLE", OptionalQuote(rightTitle) );
	ReplaceTokenInArgs( args, "#ATITLE", OptionalQuote(ancestorTitle) );
	ReplaceTokenInArgs( args, "#LEFT", OptionalQuote(leftFile) );
	ReplaceTokenInArgs( args, "#RIGHT", OptionalQuote(rightFile) );
	ReplaceTokenInArgs( args, "#ANCESTOR", OptionalQuote(ancestorFile) );
	ReplaceTokenInArgs( args, "#OUTPUT", OptionalQuote(outputFile) );
	ReplaceTokenInArgs( args, "#ABSLEFT", OptionalQuote(PathToAbsolutePath(leftFile)) );
	ReplaceTokenInArgs( args, "#ABSRIGHT", OptionalQuote(PathToAbsolutePath(rightFile)) );
	ReplaceTokenInArgs( args, "#ABSANCESTOR", OptionalQuote(PathToAbsolutePath(ancestorFile)) );
	ReplaceTokenInArgs( args, "#ABSOUTPUT", OptionalQuote(PathToAbsolutePath(outputFile)) );
	return args;
}

std::string InvokeDiffTool(
		const std::string& leftTitle, const std::string& leftFile,
		const std::string& rightTitle, const std::string& rightFile,
		const std::string& ancestorTitle, const std::string& ancestorFile )
{
	const int toolIndex = FindDiffTool();
	if( toolIndex == -1 )
		return GetNoDiffToolsDetectedMessage();
	
	const DiffToolDesc& tool = g_DiffTools[toolIndex];
	std::string command;
	if( ancestorFile.empty() || ancestorFile==leftFile || ancestorFile==rightFile )
		command = tool.diff2Command;
	else
		command = tool.diff3Command;
	
	std::vector<std::string> args = ProcessCommandLine( command, leftTitle, leftFile, rightTitle, rightFile, ancestorTitle, ancestorFile, "" );
	if( !LaunchTaskArrayOptions( tool.detectedPath, args, 0 ) )
		return "Could not launch diff tool";
	
	return "";
}

std::string InvokeMergeTool(
		const std::string& leftTitle, const std::string& leftFile,
		const std::string& rightTitle, const std::string& rightFile,
		const std::string& ancestorTitle, const std::string& ancestorFile,
		const std::string& outputFile )
{
	const int toolIndex = FindDiffTool();
	if( toolIndex == -1 )
		return GetNoDiffToolsDetectedMessage();
	
	const DiffToolDesc& tool = g_DiffTools[toolIndex];
	std::string command = tool.mergeCommand;
	
	if (tool.createEmptyOutputFile)
		WriteStringToFile("", outputFile, kNotAtomic, kFileFlagDontIndex | kFileFlagTemporary);
	
	std::vector<std::string> args = ProcessCommandLine( command, leftTitle, leftFile, rightTitle, rightFile, ancestorTitle, ancestorFile, outputFile );
	if( !LaunchTaskArrayOptions( tool.detectedPath, args, 0 ) )
		return "Could not launch diff tool";
	
	return "";
}
