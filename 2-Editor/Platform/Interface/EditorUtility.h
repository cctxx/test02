#ifndef EDITORUTILITY_H
#define EDITORUTILITY_H

#include <string>
#include "Runtime/Math/Vector3.h"
#include <set>
#include <memory>
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Math/Vector2.h"
#include "ExternalProcess.h"

class Image;
class Object;
class Transform;
class Camera;
class EditorExtension;
class ImageReference;
class TextAsset;

#if UNITY_OSX
	#ifdef __OBJC__
		@class NSTask;
	#else
		typedef struct objc_object NSTask;
	#endif
#endif

namespace Unity { class GameObject; }

template<class T> class PPtr;

using namespace Unity;

static const char* const kTempExportPackage = "tempExportPackage.unitypackage";

// Keep in sync with CanAppendBuild in InternalEditorUtility.txt
enum CanAppendBuild
{
	kCanAppendBuildUnsupported = 0,
	kCanAppendBuildYes = 1,
	kCanAppendBuildNo = 2,
};

bool SetPermissionsReadWrite (const std::string& path);

// on OSX this sets everything in Library and Temp to read/write
// and checks if it can read everyhing in Assets.
// On win - Library, Temp read/write, Assets - not touched.
bool SetPermissionsForProjectFolder (const std::string& projectPath);

/// Displays a modal dialog. (okButton is the default Button)
/// Returns true if the ok button was clicked, false if the cancel button was clicked
bool DisplayDialog (const std::string& title, const std::string& content, const std::string& okButton, const std::string& cancelButton = std::string ());

int DisplayDialogComplex (const std::string& title, const std::string& content, const std::string& okButton, const std::string& secondary, const std::string& third);

ExternalProcess LaunchExternalProcess(const std::string& app, const std::vector<std::string>& arguments);

bool LaunchTask (const std::string& app, std::string* output, ...);
bool LaunchTaskArray (const std::string& app, std::string* output, const std::vector<std::string>& arguments, bool quoteArguments, const std::string& currentDirectory = std::string (), UInt32* exitCode = 0);
enum LaunchOptions {
	kLaunchQuoteArgs = (1<<0),
	kLaunchBackground = (1<<1),
};
bool LaunchTaskArrayOptions (const std::string& app, const std::vector<std::string>& arguments, UInt32 options, const std::string& currentDirectory = std::string ());

bool DecompressPackageTarGZ (const std::string& compressedPath, const std::string& destinationPath);

void CompressPackageTarGZAndDeleteFolder (const std::string& folder, const std::string& compressedPath, bool runAsyncAndShowInFinder);

// Relaunches the application with args.
// The user can prevent the relaunch by eg. being asked to save a scene and pressing cancel.
void RelaunchWithArguments (std::vector<std::string>& args);

std::string GetDefaultApplicationForFile (const std::string& path);

std::string RunOpenPanel (const std::string& title, const std::string& directory, const std::string& extension);
std::string RunSavePanel (const std::string& title, const std::string& directory, const std::string& extension, const std::string& defaultName);
std::string RunOpenFolderPanel (const std::string& title, const std::string& folder, const std::string& defaultName);
std::string RunSaveFolderPanel (const std::string& title,
                                const std::string& directory,
                                const std::string& defaultName,
                                bool canCreateFolder);
#if UNITY_OSX
CanAppendBuild iOSBuildCanBeAppended(std::string folder);
std::string RuniPhoneReplaceOrAppendPanel (const std::string& title, const std::string& directory, const std::string& defaultName);
#endif

typedef bool RunComplexOpenPanelDelegate (void* userData, const std::string& file);
std::string RunComplexOpenPanel (const std::string& title, const std::string message, const std::string& openButton, const std::string& directory, RunComplexOpenPanelDelegate* isValidFilename, RunComplexOpenPanelDelegate* shouldShowFileName, void* userData);
std::string RunComplexSavePanel (const std::string& title, const std::string message, const std::string& saveButton, const std::string& directory, const std::string& defaultName, const std::string& extension, RunComplexOpenPanelDelegate* isValidFilename, RunComplexOpenPanelDelegate* shouldShowFileName, void* userData);

void RevealInFinder (const std::string& path);
bool OpenWithDefaultApp (const std::string& path);
bool OpenFileWithApplication (const std::string& path, const std::string& app);
std::string GetDefaultEditorPath ();
std::string GetApplicationSupportPath ();

bool LaunchApplication (const std::string& path);

void OpenURLInWebbrowser (const std::string& url);
void OpenPathInWebbrowser (const std::string& path);

void UnityBeep();

bool AmIBeingDebugged();

std::string GetBundleVersionForApplication (const std::string& path);

bool SelectFileWithFinder (const std::string& path);



enum ProgressBarState { kPBSNone = 0, kPBSWantsToCancel = 1 };
struct ProgressBarCancelled {};

// Progressbar only used when building a player
ProgressBarState DisplayBuildProgressbar (const std::string& title, const std::string& text, Vector2f buildInterval, bool canCancel = false);
inline void DisplayBuildProgressbarThrowOnCancel (const std::string& title, const std::string& text, Vector2f buildInterval)
{
	if (DisplayBuildProgressbar (title, text, buildInterval, true) == kPBSWantsToCancel)
		throw ProgressBarCancelled();
}

// General purpose progress bar (can be called when building a player, but the progress will be relative to the current buildInterval and title text will be ignored)
ProgressBarState DisplayProgressbar (const std::string& title, const std::string& info, float progress, bool canCancel = false);
inline void DisplayProgressbarThrowOnCancel (const std::string& title, const std::string& info, float progress)
{
	if (DisplayProgressbar (title, info, progress, true) == kPBSWantsToCancel)
		throw ProgressBarCancelled();
}

// Remove the progress bar (calling this while building a player has no effect, it will be removed at end of build progress)
void ClearProgressbar ();




/// We use this to verify if the project folder is being moved while Unity is running. (The cocoa current directory value is changed when the folder is being moved)
void SetCocoaCurrentDirectory (std::string path);
std::string GetCocoaCurrentDirectory();

bool IsApplicationActiveOSImpl();
void ActivateApplication ();

bool IsOptionKeyDown();

bool CheckIPhoneXCodeInstalled ();
bool CheckIPhoneXCode4Installed ();
void LaunchIPhoneXCode4 ();
void TerminateIPhoneXCode4 ();

/// same as cp -RL
/// This lives here because it has a screwed up dependency to launch tasks on os x (Should really be in fileutilities.mm)
bool CopyFileOrDirectoryFollowSymlinks (const std::string& from, const std::string& to);


// Show "busy" cursor before long operation
void BeginBusyCursor();
// Remove "busy" cursor before long operation
void EndBusyCursor();
// Ensure that "busy" cursor is still shown (in the middle of long operation, if there were some dialogs presented, the cursor might have been reset)
void EnsureBusyCursor();


#if UNITY_WIN
bool PauseResumeProcess(DWORD dwOwnerPID, bool bResumeThread);
// TODO : Don't use this - use ExternalTask class instead which is multiplatform (add any functionality if you need)
bool LaunchTaskArrayOptions (const std::string& app, const std::vector<std::string>& arguments, UInt32 options, const std::string& currentDirectory, PROCESS_INFORMATION& processInfo );
bool IsProcessRunning(DWORD pid);
HBITMAP LoadPNGFromResource( HDC dc, unsigned int resourceID, int& width, int& height );
// This will kill the process pid if Unity crashes or quits!
bool AttachWatchDog(PROCESS_INFORMATION& procInfo, int pid);
#elif UNITY_OSX
// This will kill the process pid if Unity crashes or quits!
NSTask* AttachWatchDog(int pid);
#endif

class ExternalTask : public NonCopyable
{
private:
	ExternalTask();

public:
	static std::auto_ptr<ExternalTask> LauchTask(const std::string& taskPath, const std::vector<std::string>& arguments);

	~ExternalTask();

	bool IsRunning() const;
	void Terminate();

	// This will kill the task if Unity crashes or quits!
	std::auto_ptr<ExternalTask> AttachWatchDog() const;

	// TODO : should probably placed somewhere globally
	static void Sleep(int miliseconds);

private:

#if UNITY_WIN
	PROCESS_INFORMATION task;
#elif UNITY_OSX
	NSTask* task;
#endif

};

// Convert to UTF8 (try to detect src encoding)
std::string ConvertToUTF8(const void* str,
						  unsigned byteLen, bool &encodingDetected);

#endif
