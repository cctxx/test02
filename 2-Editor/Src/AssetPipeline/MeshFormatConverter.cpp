#include "UnityPrefix.h"
#include "MeshFormatConverter.h"

#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Input/TimeManager.h"
#include <sstream>
#include <fstream>

namespace {
	static std::auto_ptr<ExternalTask> LaunchModoTask (const std::string& modoPath, const std::string& conversionScript, const std::string& inputFile, const char* outputFile, const char* logFile, bool useConsoleArgument)
	{	
		if (modoPath.empty ())
			return std::auto_ptr<ExternalTask>();
		
		std::ostringstream consoleArguments;
		
		// conversionScript has to be quoted, because otherwise it doesn't work when Unity path contains spaces
		consoleArguments << "-cmd:@\"" << conversionScript
			<< "\" {" << inputFile << "} {" << PathToAbsolutePath(outputFile) << "} {" << PathToAbsolutePath(logFile) << "}";

		std::vector<std::string> arguments;
		if (useConsoleArgument)
			arguments.push_back("-console");
		arguments.push_back(consoleArguments.str().c_str());
		
		std::auto_ptr<ExternalTask> task = ExternalTask::LauchTask(modoPath, arguments);

		static std::auto_ptr<ExternalTask> gModoWatchdogTask;	
		if (task.get())
			gModoWatchdogTask = task->AttachWatchDog();
				
		return task;
	}	
}

std::string ConvertModoToFBXSDKReadableFormat(const std::string& sourceFile, std::string* file)
{
	std::string conversionScript = AppendPathName(GetApplicationContentsPath(), "Tools/ModoToUnity.py");

	// we allow ModoToUnity.py to decide which format to use
	const char* kExportedModoFileWithoutExtension = "Temp/ExportedModoFile";
	const char* kExportedModoFileDae = "Temp/ExportedModoFile.dae";
	const char* kExportedModoFileFbx = "Temp/ExportedModoFile.fbx";
	const char* kLogFile = "Temp/ModoLogFile.txt";
	
	
	CreateDirectory ("Temp");
	DeleteFile (kExportedModoFileDae);
	DeleteFile (kExportedModoFileFbx);
	DeleteFile (kLogFile);
	
	// modo in batch mode is executed this way:
	// Windows:		modo_cl.exe "-cmd:..."
	// OSX:			modo.app/Contents/MacOS/modo -console "-cmd:..."

	bool useConsoleArgument = false;
	std::string modoPath = GetDefaultApplicationForFile(sourceFile);
	#if UNITY_OSX
		modoPath = AppendPathName(modoPath, "Contents/MacOS/modo");
		useConsoleArgument = true;
	#else
		modoPath = AppendPathName(DeleteLastPathNameComponent(modoPath), "modo_cl.exe");
	#endif
	
	// Launch Modo
	std::auto_ptr<ExternalTask> modoTask = LaunchModoTask(modoPath, conversionScript, sourceFile, kExportedModoFileWithoutExtension, kLogFile, useConsoleArgument);
	if (!modoTask.get())
	{
		return "Modo could not be launched and perform the conversion.\n"
			"Make sure that Modo is installed and the .lxo file has Modo as its 'Open with' application!";
	}
	
	double launchTime = GetTimeSinceStartup();
	
	const float kModoTimeOut = 60 * 3;
	bool timeOut = false;
	
 	while (true)
 	{
		ExternalTask::Sleep(10);
		
		if (GetTimeSinceStartup() - launchTime > kModoTimeOut)
		{
			timeOut = true;
			break;
		}
		
		if (!modoTask->IsRunning())
		{
			break;
		}	
 	}
	
	if (timeOut && modoTask->IsRunning())
	{
		// if conversion failed and task is still running we choose to terminate it
		// because it's probably hanging due to some reason
		modoTask->Terminate();
	}

	modoTask.reset();

	// by default ModoToUnity.py script should convert modo file to dae, 
    // but we left backdoor for users to switch conversion to modo->fbx->unity
	std::string exportedModoFile;
	if (IsFileCreated(kExportedModoFileDae))
		exportedModoFile = kExportedModoFileDae;
	else if (IsFileCreated(kExportedModoFileFbx))
		exportedModoFile = kExportedModoFileFbx;

	if (!exportedModoFile.empty())
	{
		*file = exportedModoFile;
		return "";
	}
	else
	{
		// No conversion took place	

		bool hasLog = IsFileCreated(kLogFile);
				
		//return std::string() +
		std::ostringstream str;
		str << "Modo couldn't convert the .lxo file to fbx file.\n";
		if (timeOut)
			str << "Conversion exceeded time-out of " << kModoTimeOut << "sec.\n";

		//str << "If you're using modo 401 then please make sure you have COLLADA exporter installed "
		//	<< "(you can download it from here: http://sourceforge.net/projects/colladamodo/). "
		//	<< "modo versions older than 401 are not officially supported.\n";
		str << "You need at least modo 501 to import .lxo files directly.\n";
			
		if (hasLog)
		{
			str << "Modo log file (" << kLogFile << ") contents:\n\n";

			string s;
			std::ifstream logFile(kLogFile);
			while (logFile)
			{
				getline(logFile, s);
				str << s << "\n";
			}
		}
		else
			str << "Modo didn't generate conversion log file.\n";
		
		return str.str().c_str();
	}
}