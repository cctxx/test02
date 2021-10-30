#ifndef EDITORHELPER_H
#define EDITORHELPER_H

#include <set>
#include <string>

#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Utilities/File.h"

template<class T>
class PPtr;
class Object;
class EditorExtension;
namespace Unity { class GameObject; }
using namespace Unity;

/// Destroys a group of objects safely in the right order
//void InOrderFileDeletion (const std::set<SInt32>& objects, bool secureDestruction);
void UnloadFileEmptied (const std::string& path);

/// Are users allowed to modify the asset?
bool IsUserModifiable (Object& object);
//void SetForceUserModifiable(Object& object);

/// Eg. are you allowed to edit a shader? Are you allowed to edit the animation?
bool IsUserModifiableScript (Object& object);

class Ticker
{
	public:
	
	Ticker (float rate);
	bool Tick ();

	private:

	float m_Rate;
	bool  m_IsInitialized;
	double m_Time;
};

typedef void StartupFunc();
void AddStartup (StartupFunc *);
void ExecuteStartups ();
// Macro for running a function at startup. Use STARTUP (MyFunc)
#define STARTUP(a) struct MENU_STARTUP##a { MENU_STARTUP##a () { AddStartup (&a) ; } }; static MENU_STARTUP##a gMENU_STARTUP##a;


bool IsApplicationActive ();

/// Warn the user that an action he is about to take will lose him the prefab
/// If any object in the selection is derived from a prefab, this function pops up a dialog warning the user
/// that this action will lose the prefab connection. The dialog contains an cancel & an ok button
/// If the user selects cancel, the function returns FALSE
bool WarnPrefab (const std::set<EditorExtension*>& selection, const char* title, const char *warning, const char *okString);
bool WarnPrefab (const std::set<EditorExtension*>& selection);
/// Warn the user that an action he is about to take will lose him the prefab
/// If the object is derived from a prefab, this function pops up a dialog warning the user
/// that this action will lose the prefab connection. The dialog contains an cancel & an ok button
/// If the user selects cancel, the function returns FALSE
bool WarnPrefab (Object* object, const char* title, const char *warning, const char *okString);
bool WarnPrefab (Object* object);

std::string GetMainAssetNameFromAssetPath (const std::string& path);

std::string GetBaseUnityDeveloperFolder();
// A folder where the editor and player folders are located ('./build'). Is meaningfull only when
// running Unity without proper packaging, i.e. built from source.
std::string GetTargetsBuildFolder();
std::string GetExternalScriptEditor();
std::string GetExternalScriptEditorArgs();

// Is this a build for internal UNITY TECHNOLOGIES developers.
// This will enable some menu commands and preferences normally not available.
bool IsDeveloperBuild (bool checkHumanControllingUs = true);

bool IsUserModifiable (Object& object);

bool IsUnitySceneFile (const std::string& path); 

bool TryOpenErrorFileFromConsole (const std::string& path, int line);

bool OpenScriptFile (const std::string& completePath, int line);

bool IsAssetAvailable (int instanceID);

/// Opens the file with the best matching application. Or simply selects the file.
bool OpenAsset (int instanceID, int tag = -1);

std::string RunSavePanelInProject (const std::string& title, const std::string& saveButton, std::string extension, const std::string message, const std::string path);

std::string RunSaveBuildPanel( BuildTargetPlatform target, const std::string& title, const std::string& directory, 
							  const std::string& defaultName, const std::string& extension );

/// Returns whether the object has an enabled button
/// 0 -> disabled
/// 1 -> enabled
/// -1 -> no enabled button
int GetObjectEnabled (Object* object);
void SetObjectEnabled (Object* obj, bool enabled);

void SetObjectNameSmart (PPtr<Object> obj, std::string name);

bool StripFatMacho (const std::string& path, bool generate_x86_64);

// Checks a string for inconsistent line endings, or mixed windows/unix styles.
// Returns true if detected and false if line endings are consistent.
bool WarnInconsistentLineEndings(const std::string& contents);


// Retrieves the path to the solution file and resolved path that should be opened when opening a cs / js file at path.
// Automatically detects if it should open the Project solution file or the Unity source code solution file.
void GetSolutionFileToOpenForSourceFile (const std::string& path, bool useMonoDevelop, std::string* solutionPath, std::string* outputFilePath);

#endif
