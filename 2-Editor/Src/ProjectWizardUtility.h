
bool UpgradeStandardAssets ();
bool ExtractProjectAndScenePath (const std::string& path, std::string* project, std::string* scene);
void PopulateRecentProjectsList (std::vector<std::string>& recentProjects);
void PopulatePackagesList ( std::vector<std::string>* newPaths, std::vector<bool>* enabled );
void ImportProjectWizardPackagesAndTemplate ();

enum ProjectDefaultTemplate
{
	kProjectTemplate3D = 0,
	kProjectTemplate2D,
	kProjectTemplateCount
};
extern const char* kProjectTemplateNames[];


void SetIsCreatingProject (const std::vector<std::string>& packages, ProjectDefaultTemplate templ);
bool IsCreatingProject ();


bool IsProjectFolder (const std::string& project);
std::string ChooseProjectDirectory ();

void SetProjectPath (const std::string& projectPath, bool dontAddToRecentList);
std::string GetProjectPath ();

extern const char* kRecentlyUsedProjectPaths;
extern const char* kProjectBasePath;

