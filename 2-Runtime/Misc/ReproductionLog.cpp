#include "UnityPrefix.h"
#include "ReproductionLog.h"

#if SUPPORT_REPRODUCE_LOG
#include "Runtime/Input/InputManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Misc/CaptureScreenshot.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Export/WWW.h"
#include "PlatformDependent/CommonWebPlugin/UnityWebStream.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include <fstream>
#include "Runtime/Math/Random/Random.h"

#if UNITY_OSX || UNITY_LINUX
#include <sys/stat.h>
#endif


using namespace std;
struct RemappedWWWStreamData;

static std::ifstream* gReproduceInStream = NULL; 
static std::ofstream* gReproduceOutStream = NULL;

static std::string gReproductionPath;
#if SUPPORT_REPRODUCE_LOG_GFX_TRACE
static FILE* gReproduceGfxLogFile = NULL;
#endif
int gScreenshotCount = 0;
bool gDisableScreenShots = false;
bool gNormalPlaybackSpeed = false;
bool gRepeatScreenshots = true;
int gLastScreenshotTime = 0;
int gReproduceVersion = REPRODUCE_VERSION;
ReproduceMode gReproMode = kPlaybackUninitialized;
RemappedWWWStreamData* gRemappedWWWStreamData = NULL;
Mutex     gWaitForCompletionDownloadsMutex;
set<WWW*> gWaitForCompletionDownloads;

extern Rand gScriptingRand;

ReproduceMode GetReproduceMode()
{
	return gReproMode;
}

void ReproductionWriteExitMessage(int result)
{
	if (result == 0)
	{
		LogString("Successfully reached end of reproduction log");
	}
	else
	{
		LogString("Aborting Reproduction playback");
	}
}

void ReproductionExitPlayer (int result, bool writeExitMessage)
{
	if (writeExitMessage)
		ReproductionWriteExitMessage(result);

#if UNITY_OSX
	FinishAllCaptureScreenshot ();

	if (result == 0)
	{
		if (!PlayerCleanup(true,true))
			ErrorString("Failed to clean up player");
	}

	// On Safari 64-bit, the browser is a seperate process. Take that down as well.
	CFStringRef pluginHostID = CFSTR("com.apple.WebKit.PluginHost");
	CFStringRef appID = CFBundleGetIdentifier(CFBundleGetMainBundle()); 
	if (CFStringCompare(pluginHostID, appID, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	{
		printf_console("killing Safari\n");
		system("killall Safari");
	}
#endif
	exit(result);
}

void FailReproduction (const std::string& err)
{
	ErrorString(Format("%s\nFrame: %d", err.c_str(), GetTimeManager().GetFrameCount()));
	ReproductionExitPlayer(1);
}

struct RemappedWWWStreamData
{
	string absoluteUrl;
	string srcValue;
	int width;
	int height;
	vector<pair<string, string> > downloads;
	int remapCount;
	
	RemappedWWWStreamData () : remapCount(0) {}

	void Write ()
	{
		std::ofstream stream (AppendPathName(gReproductionPath, "WWWRemap.log").c_str());
		stream.setf(std::ios::fixed,std::ios::floatfield);
		stream.precision(15);
		
		stream << "absoluteUrl= " << absoluteUrl << endl;
		stream << "srcValue= " << srcValue << endl;
		stream << "width= " << width << endl;
		stream << "height= " << height << endl;
		for(int i=0;i<downloads.size();i++)
		{
			stream << "WWW= " << downloads[i].first << endl;
			stream << "Remap= " << downloads[i].second << endl;
		}
	}
	
	void Parse (std::ifstream& stream)
	{
		string temp;
		stream >> temp;
		if (temp == "absoluteUrl=")
			stream >> absoluteUrl;
		else
			FailReproduction("Failed parsing absolute url" + temp);
		
		stream >> temp;
		if (temp == "srcValue=")
			stream >> srcValue;
		else
			FailReproduction("Failed parsing src value" + temp);	
		
		stream >> temp; if (temp != "width=") FailReproduction("Failed parsing  width"); stream >> width;
		stream >> temp; if (temp != "height=") FailReproduction("Failed parsing  height"); stream >> height;
		while (true)
		{
			stream >> temp;
			if (stream.eof())
				break;
			
			if (!BeginsWith(temp, "WWW="))
				FailReproduction ("Failed parsing WWW GOT:" + temp);
			string url;
			stream >> url;
			
			stream >> temp;
			if (!BeginsWith(temp, "Remap="))
				FailReproduction ("Failed parsing Remap");
			string remap;
			stream >> remap;
			
			downloads.push_back(make_pair(url, remap));
		}
	}
};

void SetReproduceMode (string reproductionLogFolder)
{
	string reproductionLogPath = AppendPathName(reproductionLogFolder, "Reproduction.log");
	string remapLogPath = AppendPathName(reproductionLogFolder, "WWWRemap.log");

	if (gReproMode == kGenerateReproduceLog || gReproMode == kGenerateReproduceLogAndRemapWWW)
	{
		gReproduceOutStream = new std::ofstream (reproductionLogPath.c_str());
		gReproduceOutStream->setf(std::ios::fixed,std::ios::floatfield);
		gReproduceOutStream->precision(15);

		if (gReproMode == kGenerateReproduceLogAndRemapWWW)
		{
			gRemappedWWWStreamData = new RemappedWWWStreamData();
		}
		
		gScriptingRand.SetSeed(0);
	}
	else if (gReproMode == kPlaybackReproduceLog)
	{
		gReproduceInStream = new std::ifstream (reproductionLogPath.c_str());
		gReproduceInStream->setf(std::ios::fixed,std::ios::floatfield);
		gReproduceInStream->precision(15);
		
		std::ifstream stream (remapLogPath.c_str());
		if (stream.is_open())
		{
			stream.setf(std::ios::fixed,std::ios::floatfield);
			stream.precision(15);
			
			gRemappedWWWStreamData = new RemappedWWWStreamData();
			gRemappedWWWStreamData->Parse(stream);
		}
		
		gScriptingRand.SetSeed(0);
	}
}
	
void ReproduceVersion()
{
	if (gReproMode == kGenerateReproduceLog || gReproMode == kGenerateReproduceLogAndRemapWWW)
	{
		//set version
		*gReproduceOutStream << "Version" << " " << REPRODUCE_VERSION;
	}
	else if (gReproMode == kPlaybackReproduceLog)
	{
		//get version

		if (gReproduceInStream->peek() == 'V')
		{
			std::string testVersion;
			*gReproduceInStream >> testVersion;
			*gReproduceInStream >> gReproduceVersion;
		}
		else
		{
			gReproduceVersion = 1;
		}
	}
}

int GetReproduceVersion()
{
	return gReproduceVersion;
}

string GetScreenshotPath()
{
	string reproductionPathTemp = AppendPathName( gReproductionPath, "/Images/" );
	string imgOut = Format("%d.png", gScreenshotCount);
	reproductionPathTemp = AppendPathName( reproductionPathTemp, imgOut );
	gScreenshotCount++;

	return reproductionPathTemp;
}

void CaptureScreenshotReproduction(bool manual)
{
	if(manual)
		QueueScreenshot (GetScreenshotPath(), 0);

	if(gDisableScreenShots)
		return;
	
	if(GetInputManager().GetKeyDown(SDLK_F6) && gReproduceOutStream)
		gRepeatScreenshots = !gRepeatScreenshots;
	
	if(gReproduceInStream || gReproduceOutStream)
	{	
		if(GetInputManager().GetKey(SDLK_F5)) 
		{
			QueueScreenshot (GetScreenshotPath(), 0);
		#if SUPPORT_REPRODUCE_LOG_GFX_TRACE
			fclose(gReproduceGfxLogFile);
			gReproduceGfxLogFile = NULL;
		#endif
		}
	}
}

bool HasNormalPlaybackSpeed()
{
	return gNormalPlaybackSpeed;
}

void BatchInitializeReproductionLog ()
{
	if (gReproMode != kPlaybackUninitialized)
		return;
	gReproMode = kNormalPlayback;
	
	std::string instructionsFilePath;
#if UNITY_OSX
	string result = getenv ("HOME");
	if (!result.empty())
	{
		instructionsFilePath = AppendPathName( result, "Library/Logs/Unity/GlobalPlaybackInstructions.log");
	}	
#elif UNITY_WINRT
	// ?!-
#elif UNITY_WIN
	const char* tempPath = ::getenv("TEMP");
	if (!tempPath)
		tempPath = "C:";
	instructionsFilePath = AppendPathName (tempPath, "UnityGlobalPlaybackInstructions.log");
#elif UNITY_LINUX
	string result = getenv ("HOME");
	if (!result.empty())
	{
		instructionsFilePath = AppendPathName( result, ".unity/GlobalPlaybackInstructions.log");
	}	
#else
#error "Unknown platform"
#endif

	if (!instructionsFilePath.empty())
	{
		//@TODO: this won't work on Windows when user's name has non-ASCII characters!
		std::ifstream instructionsFile (instructionsFilePath.c_str());

		if (instructionsFile.is_open())
		{
			std::string line;
			std::vector<std::string> lines;
			
			while (!instructionsFile.eof())
			{
				std::getline (instructionsFile,line);
				lines.push_back (line);
			}
			
			gReproductionPath = lines[0];
			
			// The real instructionsfile is read to determine reproduction mode and playback.log path, other variables should be added here
			string reproductionPlayerLogPathComplete = AppendPathName(gReproductionPath, "PlayerComplete.log");
			LogOutputToSpecificFile(reproductionPlayerLogPathComplete.c_str());

			for (size_t i = 0; i < lines.size(); i++)
			{
				if(strcmp(lines[i].c_str(),"-reproduceInput") == 0)
					gReproMode = kPlaybackReproduceLog;
				else if(strcmp(lines[i].c_str(),"-recordInput") == 0)
					gReproMode = kGenerateReproduceLog;
				else if(strcmp(lines[i].c_str(),"-recordInputAndRemapWWW") == 0)
					gReproMode = kGenerateReproduceLogAndRemapWWW;
				
				if(!strcmp(lines[i].c_str(),"-disableScreenshots"))
				    gDisableScreenShots = true;
				if(!strcmp(lines[i].c_str(),"-normalPlaybackSpeed"))
				    gNormalPlaybackSpeed = true;
			
				// Do we want REF D3D9 device?
				#if GFX_SUPPORTS_D3D9 && ENABLE_FORCE_GFX_RENDERER
				if(!strcmp(lines[i].c_str(),"-force-d3d9-ref"))
					::g_ForceD3D9RefDevice = true;
				#endif
			}
			
			if (gReproMode == kNormalPlayback)
			{
				ErrorString("Failed setting Reproduction mode from reproduction log file");
			}
			
			SetReproduceMode (gReproductionPath);
			ReproduceVersion();

			// Kill the instructions file, so that it wont get reused next time you run Unity
			instructionsFile.close();
			DeleteFileOrDirectory(instructionsFilePath);
		}
	}
}

void CheckReproduceTagAndExit(const std::string& tag, std::ifstream& stream)
{
	if (!CheckReproduceTag(tag, stream))
		FailReproduction("Failed to read: " + tag);
}

bool CheckReproduceTag(const std::string& tag, std::ifstream& stream)
{
	int position = stream.tellg();
	string temp;
	stream >> temp;
	if (temp == tag)
		return true;

	stream.seekg(position, ios::beg);
	return false;
}



void ReadWriteAbsoluteUrl(UnityStr& srcValue, UnityStr& absoluteUrl)
{
	if (gRemappedWWWStreamData)
	{
		if (GetReproduceMode() == kPlaybackReproduceLog)
		{
			absoluteUrl = gRemappedWWWStreamData->absoluteUrl;
			srcValue = gRemappedWWWStreamData->srcValue;
		}
		else
		{
			gRemappedWWWStreamData->absoluteUrl = absoluteUrl;
			gRemappedWWWStreamData->srcValue = srcValue;
		}
	}
}

void WriteWebplayerSize(int width, int height)
{
	if (GetReproduceMode() == kGenerateReproduceLogAndRemapWWW)
	{
		gRemappedWWWStreamData->width = width;
		gRemappedWWWStreamData->height = height;
	}
}

#if ENABLE_WWW

void CleanupWWW (WWW* www)
{
	gWaitForCompletionDownloadsMutex.Lock();
	gWaitForCompletionDownloads.erase(www);
	gWaitForCompletionDownloadsMutex.Unlock();
}



bool ShouldWaitForCompletedDownloads ()
{
	if (!RunningReproduction())
		return false;

	gWaitForCompletionDownloadsMutex.Lock();
	set<WWW*>& oldSet = gWaitForCompletionDownloads;
	set<WWW*> newSet;
	bool wait = false;
	for (set<WWW*>::iterator i=oldSet.begin();i!=oldSet.end();++i)
	{
		WWW& www = **i;
		
		#if WWW_USE_BROWSER
		WWWBrowser* browser = static_cast<WWWBrowser*> (&www);
		if (browser)
			browser->ForceProgressDownload();
		#endif

		if(!www.IsDone() && ( www.GetUnityWebStream() == NULL || www.GetUnityWebStream()->GetProgressUntilLoadable() < 1 ))
		{
			newSet.insert(&www);
			wait = true;
		}
	}
	oldSet = newSet;
	gWaitForCompletionDownloadsMutex.Unlock();
	
	return wait;
}

void CreateWWWReproduce(WWW* www, const std::string& url, std::string& remappedUrl, int& postLength)
{
	remappedUrl = url;
	
	if (RunningReproduction())
	{
		gWaitForCompletionDownloadsMutex.Lock();
		gWaitForCompletionDownloads.insert(www);
		gWaitForCompletionDownloadsMutex.Unlock();
	}
	
	
	// Play back from remap data
	if (gRemappedWWWStreamData && GetReproduceMode() == kPlaybackReproduceLog)
	{
		int downloadNumber = gRemappedWWWStreamData->remapCount;
		if (downloadNumber >= gRemappedWWWStreamData->downloads.size())
		{
			FailReproduction("Failed remapping download because the content is trying to downloading more WWW than the original reproduction log did:" + url);
			return;
		}

		remappedUrl = Format("WWW-%d.bin", downloadNumber);
		postLength = 0;

		if (gRemappedWWWStreamData->downloads[downloadNumber].first != url)
			FailReproduction("Failed remapping download because the content is trying to download a different url than it originally did:" + url + "\nexpected: " + gRemappedWWWStreamData->downloads[downloadNumber].first);
		if (gRemappedWWWStreamData->downloads[downloadNumber].second != remappedUrl && gRemappedWWWStreamData->downloads[downloadNumber].second != string("INCOMPLETE"))
			FailReproduction("Failed remapping download because the remapped name is not matching correctly:" + url);
		gRemappedWWWStreamData->remapCount++;
	}
	// Setup remap data from downloaded content
	else if (gRemappedWWWStreamData && GetReproduceMode() == kGenerateReproduceLogAndRemapWWW)
	{
		*www->GetReproRemapCount() = gRemappedWWWStreamData->remapCount;
		gRemappedWWWStreamData->downloads.push_back(make_pair(url, string("INCOMPLETE")));
		gRemappedWWWStreamData->remapCount++;
	}
	
}

void CompleteWWWReproduce(WWW* www, const std::string& url, const UInt8* buffer, int size)
{
	// Store downloaded www data
	if (GetReproduceMode() == kGenerateReproduceLogAndRemapWWW)
	{
		int downloadNumber = *www->GetReproRemapCount();
		string fileName = Format("WWW-%d.bin", downloadNumber);
		if (!WriteBytesToFile(buffer, size, AppendPathName(gReproductionPath, fileName)))
			FailReproduction("Failed writing completed WWW");
		
		gRemappedWWWStreamData->downloads[downloadNumber].second = fileName;
	}
}

#endif

void ReproduceWriteMainDataFile(const UInt8* buffer, int size)
{
	if (GetReproduceMode() == kGenerateReproduceLogAndRemapWWW)
	{
		if (!WriteBytesToFile(buffer, size, AppendPathName(gReproductionPath, "main.unity3d")))
			FailReproduction("Failed writing completed WWW");
	}
}

void ReadWriteReproductionInput()
{
	if (gReproduceInStream)
	{
		GetInputManager().ReadLog(*gReproduceInStream);
		GetGUIManager().ReadLog(*gReproduceInStream);
	}
	else if (gReproduceOutStream)
	{
		GetInputManager().WriteLog(*gReproduceOutStream);
		GetGUIManager().WriteLog(*gReproduceOutStream);
	}

#if	SUPPORT_REPRODUCE_LOG_GFX_TRACE
	if(GetInputManager().GetKey(SDLK_F5)) 
	{
		string reproductionPathTemp = AppendPathName( gReproductionPath, "/Images/" );
		string imgOut = Format("%d.png", gScreenshotCount);
		reproductionPathTemp = AppendPathName( reproductionPathTemp, imgOut );		
		gReproduceGfxLogFile = fopen(reproductionPathTemp.c_str(), "w");
	}
#endif
}

void ReadWriteReproductionTime()
{
	if (gReproduceInStream)
		GetTimeManager().ReadLog(*gReproduceInStream);
	else if (gReproduceOutStream)
	{
		GetTimeManager().WriteLog(*gReproduceOutStream);
	}
}

std::ifstream* GetReproduceInStream ()
{
	return gReproduceInStream;
}

std::ofstream* GetReproduceOutStream ()
{
	return gReproduceOutStream;
}

void RepeatReproductionScreenshot()
{
	//making repeating screenshots
	//adjust modulus to make it repeat screenshotting often
	if (gRepeatScreenshots && gReproduceOutStream)
 	{
		bool takeScreenshot = false;
		int ms = RoundfToIntPos(GetTimeSinceStartup() * 1000);
		if (ms - gLastScreenshotTime > 500)
		{
			gLastScreenshotTime = ms;
			takeScreenshot = true;
		}
		GetInputManager().SetKeyState(SDLK_F5, takeScreenshot);
	}
}

void PlayerCleanupReproduction()
{
	if (GetReproduceMode() == kGenerateReproduceLogAndRemapWWW)
		gRemappedWWWStreamData->Write();

	if (gReproduceOutStream)
		gReproduceOutStream->flush();
}

bool gShouldExit = false;

bool ShouldExitReproduction()
{
	return gShouldExit;
}

void ReadWriteReproductionEnd()
{
	if (gReproduceInStream)
	{
		string test;
		*gReproduceInStream >> test;
		if (test != "EndOfFrame")
		{
			ErrorString("Error reading reproduce EndOfFrame tag in Reproduction log. Forwarding to next EndOfFrame.");
			
			while (!gReproduceInStream->eof() && test != "EndOfFrame")
			{
				*gReproduceInStream >> test;
			}
		}
		
		if (gReproduceInStream->eof())
		{
			delete gReproduceInStream;
			gReproduceInStream = NULL;
			gShouldExit = true;
		}
	}
	else if (gReproduceOutStream)
	{
		*gReproduceOutStream << "EndOfFrame";
	}
}

void WriteReproductionString (std::ostream& out, const std::string& value)
{
	int size = value.size();
	out << size;
	for (int i=0;i<size;i++)
		out << ' ' << (int)value[i];
	out << std::endl;
}

void ReadReproductionString (std::istream& in, string& value)
{
	int size = 0;
	in >> size;
	value.resize(size);
	for (int i=0;i<value.size();i++)
	{
		int val = 0;
		in >> val;
		value[i] = val;
	}
}

#if SUPPORT_REPRODUCE_LOG_GFX_TRACE
void LogToScreenshotLog(string str)
{
	if (gReproduceGfxLogFile != NULL)
	{
		fprintf(gReproduceGfxLogFile, str.c_str());
		fprintf(gReproduceGfxLogFile, "\n");
	}
}
#endif

bool RunningReproduction()
{
	if (gReproduceInStream != NULL || gReproduceOutStream != NULL)
		return true;
	else
		return false;
}

std::string GetReproductionDirectory()
{
	return gReproductionPath;
}


void WriteFloat (std::ostream& out, float& value)
{
	int intValue = RoundfToInt(value * 1000.0);
	out << intValue;
	value = intValue / 1000.0;
}

void ReadFloat (std::istream& in, float& value)
{
	int intValue = 0;
	in >> intValue;
	value = intValue / 1000.0;
}

void WriteFloat (std::ostream& out, double& value)
{
	int intValue = RoundfToInt(value * 1000.0);
	out << intValue;
	value = intValue / 1000.0;
}

void ReadFloat (std::istream& in, double& value)
{
	int intValue = 0;
	in >> intValue;
	value = intValue / 1000.0;
}

void WriteBigFloat (std::ostream& out, double& value)
{
	SInt64 intValue = RoundfToInt(value * 1000000.0);
	out << intValue;
	value = intValue / 1000000.0;
}

void ReadBigFloat (std::istream& in, double& value)
{
	SInt64 intValue = 0;
	in >> intValue;
	value = intValue / 1000000.0;
}

#endif
