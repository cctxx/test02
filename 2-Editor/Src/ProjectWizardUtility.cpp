#include "UnityPrefix.h"
#include "ProjectWizardUtility.h"
#include "EditorSettings.h"
#include "GUIDPersistentManager.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "PackageUtility.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/Utilities/ArrayUtility.h"
#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include <ShlObj.h>
#endif

using namespace std;

static const char* kAssetsFolder = "Assets";
const int kMaxSavedPaths = 20;
const char* kProjectBasePath = "kProjectBasePath";
const char* kUnityPackageExtension = "unitypackage";
const char* kRecentlyUsedProjectPaths = "RecentlyUsedProjectPaths-%d";

const char* kProjectTemplateNames[kProjectTemplateCount] =
{
	"3D",
	"2D",
};


static bool gIsCreatingProject = false;
static vector<string> gProjectWizardImportPackages;
static ProjectDefaultTemplate gProjectWizardTemplate = kProjectTemplate3D;

static string GetPackagesPath ();

static string GetPackagesPath ()
{
	#if UNITY_WIN && !UNITY_RELEASE
	return AppendPathName (GetApplicationFolder (), "../../Editor/Resources/Standard Packages");
	#endif
	return AppendPathName (GetApplicationFolder (), "Standard Packages");
}


bool UpgradeStandardAssets ()
{
	PlayerSettings& settings = GetPlayerSettings();
	if (gIsCreatingProject || IsBatchmode())
	{
		settings.unityStandardAssetsVersion = UNITY_STANDARD_ASSETS_VERSION;
		settings.SetDirty ();
		return false;
	}
	
	string proStandardAssets = AppendPathName (GetPackagesPath (), "Pro Standard Assets.unityPackage");
	string standardAssets = AppendPathName (GetPackagesPath (), "Standard Assets.unityPackage");
	
	bool hasStandardAssets = IsDirectoryCreated ("Assets/Standard Assets") && IsFileCreated (standardAssets);
	bool hasProStandardAssets = IsDirectoryCreated ("Assets/Pro Standard Assets") && IsFileCreated (proStandardAssets);
	
	if (!hasStandardAssets && !hasProStandardAssets)
		return false;
	
	// Are the standard assets up to date?
	if (UNITY_STANDARD_ASSETS_VERSION > settings.unityStandardAssetsVersion)
	{
		string text = Format ("Your project contains an old version of the Standard Assets.\n(%s)\n"
		                      "You should upgrade to the latest version the Standard Assets.\n" , GetProjectPath ().c_str ());
				
		int result = DisplayDialogComplex ("Need to upgrade Standard Assets", text, "Upgrade", "Don't upgrade", "Upgrade later");
		// Upgrade
		if (result == 0)
		{
			vector<string> packages;
			if (hasStandardAssets)
				packages.push_back (standardAssets);
			if (hasProStandardAssets)
				packages.push_back (proStandardAssets);

			gProjectWizardImportPackages = packages;
			settings.unityStandardAssetsVersion = UNITY_STANDARD_ASSETS_VERSION;
			settings.SetDirty ();
			return true;
		}
		// Don't upgrade
		else if (result == 1)
		{
			text = "You might find some functionality missing or not working.\n"
			"To upgrade the Standard Assets later, use the menu Assets -> Import Package...";
			DisplayDialog ("Outdated Standard Assets", text, "Ok");
			settings.unityStandardAssetsVersion = UNITY_STANDARD_ASSETS_VERSION;
			settings.SetDirty ();
		}
		// Upgrade later
	}
	
	return false;
}

bool ExtractProjectAndScenePath (const string& path, string* project, string* scene)
{
	string projectPath = path;
	while (!projectPath.empty ())
	{
		if (IsProjectFolder (projectPath))
		{
			if (project)
				*project = projectPath;
			if (scene)
				*scene = string (path.begin () + projectPath.size () + 1, path.end ());
			return true;
		}
		projectPath = DeleteLastPathNameComponent (projectPath);
	}
	return false;
}


string GetProjectPath ()
{
	std::string path = TrimSlash(EditorPrefs::GetString(kProjectBasePath));
	ConvertSeparatorsToUnity( path );
	return path;
}

bool IsProjectFolder (const string& project)
{
	#if UNITY_OSX
	// Make sure that this is not a home directory or other user folder and spilling into the /Users/username/Library folder
	vector<string> components = FindSeparatedPathComponents (project, kPathNameSeparator);
	if (components.size () == 2 && components[0] == "Users")
		return false;
	
	// Make sure that some basic folders like Downloads, Libary, Desktop are now allowed as project folders
	if (components.size () == 3 && components[0] == "Users")
	{
		string lastPathComponent = components[2];
		const char* unsupportedFolders[] = { "Downloads", "Library", "Desktop", "Documents" };
		for (int i=0;i<ARRAY_SIZE(unsupportedFolders);i++)
		{
			if (unsupportedFolders[i] == lastPathComponent)
				return false;
		}
	}
	#endif
	
	return !project.empty () && IsAbsoluteFilePath(project) && IsDirectoryCreated (project) && IsDirectoryCreated (AppendPathName (project, kAssetsFolder));
}

void SetIsCreatingProject (const vector<string>& packages, ProjectDefaultTemplate templ)
{
	gProjectWizardImportPackages = packages;
	gProjectWizardTemplate = (ProjectDefaultTemplate)clamp<int>(templ, 0, kProjectTemplateCount-1);
	gIsCreatingProject = true;
}

bool IsCreatingProject ()
{
	return gIsCreatingProject;
}

void ImportProjectWizardPackagesAndTemplate ()
{
	for (int i=0;i<gProjectWizardImportPackages.size ();i++)
	{
		if (!ImportPackageNoGUI (gProjectWizardImportPackages[i]))
			ErrorString ("Failed to import package " + gProjectWizardImportPackages[i]);
	}

	if (gProjectWizardTemplate == kProjectTemplate2D)
	{
		GetEditorSettings().SetDefaultBehaviorMode(EditorSettings::kMode2D);
		GetPersistentManager().WriteFile(kEditorSettingsPath);
	}
}

void PopulatePackagesList ( vector<string>* newPaths, vector<bool>* enabled )
{
	vector<PackageInfo> packages;
	GetPackageList (packages);
	newPaths->clear();
	
	for (vector<PackageInfo>::iterator i=packages.begin();i != packages.end();i++)	
	{
		if ( !LicenseInfo::Flag (lf_pro_version) ) {
			string name = ToLower (GetLastPathNameComponent (i->packagePath));
			if (name.find ("pro ") == 0 || name.find ("(pro only)") != string::npos )
				continue;
		}
		newPaths->push_back (i->packagePath);
	}

	enabled->resize (newPaths->size (), false);
	
	for (int i=0;i<newPaths->size();i++)
	{
		string package = GetLastPathNameComponent((*newPaths)[i]);
		(*enabled)[i] = StrICmp(package, "Standard Assets.unityPackage") == 0 || StrICmp(package, "Pro Standard Assets.unityPackage") == 0;
	}
}


void SetProjectPath (const string& projectPath, bool dontAddToRecentList)
{
	AssertIf( projectPath.empty() );

	if (!dontAddToRecentList)
	{
		vector<string> paths;
		PopulateRecentProjectsList(paths);

		// Remove occurrences in the project path list
		for(int i=0;i<paths.size();i++)
		{
			if (StrICmp(paths[i], projectPath) == 0)
			{
				paths.erase(paths.begin() + i);
				i--;
			}
		}
		// Add project path as the first
		paths.insert(paths.begin(), projectPath);


		// Write recently opened project paths
		for (int i=0;i<kMaxSavedPaths;i++)
		{
			string key = Format(kRecentlyUsedProjectPaths, i);
			if (i < paths.size())
				EditorPrefs::SetString(key, paths[i]);
			else
				EditorPrefs::DeleteKey(key);
		}
	}

	// Write current project path
	EditorPrefs::SetString(kProjectBasePath, projectPath);

	EditorPrefs::Sync();
}


void PopulateRecentProjectsList (std::vector<std::string>& recentProjects)
{
	recentProjects.clear();
	for (int i=0;i<kMaxSavedPaths;i++)
	{
		// Trim slashes just in case it has them
		string value = TrimSlash(EditorPrefs::GetString(Format(kRecentlyUsedProjectPaths, i)));
		ConvertSeparatorsToUnity(value);

		// skip duplicated paths
		if (IsProjectFolder(value) && std::find(recentProjects.begin(), recentProjects.end(), value) == recentProjects.end())
		{
			recentProjects.push_back(value);
		}
	}	
}

std::string ChooseProjectDirectory ()
{
	// Locate parent folder for project (user's home or documents folder)

	std::string parentFolder;

	#if UNITY_WIN

	wchar_t widePath[kDefaultPathBufferSize];
	widePath[0] = 0;
	SHGetFolderPathW( NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, widePath );
	ConvertWindowsPathName( widePath, parentFolder );

	#elif UNITY_OSX || UNITY_LINUX

	const char* homeFolder = getenv("HOME");
	if( homeFolder == NULL )
		homeFolder = "~";
	parentFolder = homeFolder;

	#else
	#error "Unknown platform"
	#endif

	// Generate suitable project folder name.
	std::string projectFolder = GenerateUniquePath( AppendPathName(parentFolder, "New Unity Project") );
	return projectFolder;
}
