#include "UnityPrefix.h"
#include "EditorUpdateCheck.h"

#include <time.h>
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Utilities/HashFunctions.h"
#include "Configuration/UnityConfigureRevision.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Analytics.h"
#include "CurlRequest.h"
#include "YAMLNode.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Utilities/Argv.h"

// Wiki page: http://docwiki.unity3d.com/internal/index.php?n=Development.EditorUpdateCheck

#define UPDATE_URL "http://updatecheck.unity3d.com/cgi-bin/updatecheck.cgi"
#define UPDATE_URL_DEBUG "http://localhost/cgi-bin/small/perl/updatecheck.cgi"

#define MAX_UPDATE_INTERVAL 7*24*60*60
#define DEFAULT_UPDATE_INTERVAL 60 // If the server can't be contacted ask again if the editor is started again in more than DEFAULT_UPDATE_INTERVAL seconds 

namespace {
	struct UpdateCheckMessage: CurlRequestMessage
	{		
		virtual void Done() 
		{ 
			string latestVersionString;
			string latestVersionMessage;
			string updateURL;
			int updateInterval = DEFAULT_UPDATE_INTERVAL;
			if ( m_Success ) 
			{
				YAMLNode* pNode = ParseYAMLString(std::string(m_Result.c_str(), m_Result.size()));
				if ( pNode )
				{
					YAMLMapping *pMapping = dynamic_cast<YAMLMapping*>(pNode);
					if ( pMapping ) 
					{
						YAMLScalar *p;
						if ( (p = dynamic_cast<YAMLScalar*>(pMapping->Get("latestversionstring"))) != NULL ) 
						{
							latestVersionString = static_cast<const string &>(*p);
						}
						if ( (p = dynamic_cast<YAMLScalar*>(pMapping->Get("latestversionmessage"))) != NULL ) 
						{
							string s = static_cast<const string &>(*p);
							latestVersionMessage = CurlUrlDecode(s);
						}
						if ( (p = dynamic_cast<YAMLScalar*>(pMapping->Get("updateurl"))) != NULL ) 
						{
							updateURL = static_cast<const string &>(*p);
						}
						if ( (p = dynamic_cast<YAMLScalar*>(pMapping->Get("updateinterval"))) != NULL ) 
						{
							updateInterval = static_cast<int>(*p);
							if ( updateInterval > MAX_UPDATE_INTERVAL ) updateInterval = MAX_UPDATE_INTERVAL;
							if ( updateInterval < 0 ) updateInterval = DEFAULT_UPDATE_INTERVAL;
						}
					}
					delete pNode;
				}
				printf_console("EditorUpdateCheck: Response %s updateurl = %s interval = %d\n", m_Result.c_str(), updateURL.c_str(), updateInterval); 
			}
			else
			{
				printf_console("EditorUpdateCheck: Failed %s\n", m_Result.c_str()); 
			}
			
			EditorPrefs::SetInt("EditorUpdateNextCheck", time(0) + updateInterval);
			
			// If the server has a different version and this version is not marked to be skipped, the version should be updated				
			bool versionShouldBeUpdated = (!latestVersionString.empty() && latestVersionString != UNITY_VERSION_FULL_NICE) && EditorPrefs::GetString("EditorUpdateSkipVersionString") != latestVersionString;
			
			if ( (m_ShowUpdateWindow == kShowAlways) || (m_ShowUpdateWindow == kShowIfNewerVersionExists && versionShouldBeUpdated) ) 
			{
				if ( m_Success ) 
				{
					void* params[] = { 
						MonoStringNew(latestVersionString),
						MonoStringNew(latestVersionMessage),
						MonoStringNew(updateURL)
					};
					CallStaticMonoMethod ("EditorUpdateWindow", "ShowEditorUpdateWindow", params);
				}
				else 
				{
					void* params[] = { 
						MonoStringNew("Connection to the Update server failed")
					};
					CallStaticMonoMethod ("EditorUpdateWindow", "ShowEditorErrorWindow", params);
				}
			}
		}
		
		ShowUpdateWindow m_ShowUpdateWindow;
	};
}

static string GetFlags ()
{
	UInt64 f = 0;
	if ( !IsHumanControllingUs () )   //using negated value, because this flag used to contain AmRunningOnBuildServer(), which the serversidecode is filtering for.
		f |= 0x0001; 
	if ( IsBatchmode() )
		f |= 0x0002; 
	if ( IsDeveloperBuild () )
		f |= 0x0004; 
	if ( EditorPrefs::GetBool ("EnableEditorAnalytics", true) )
		f |= 0x0008; 
	if ( LicenseInfo::Flag (lf_trial) ) 
		f |= 0x0010; 
	if ( EditorPrefs::GetInt ("UserSkin", 1) != 0 ) 
		f |= 0x0020; 
 
	char buffer[128];
	
	UInt64 tokens = LicenseInfo::Get()->GetRawFlags ();
	sprintf(buffer, "%.8x%.8x%.8x%.8x", UInt32(tokens >> 32), UInt32(tokens), UInt32(f >> 32), UInt32(f));
	return buffer;
}

bool IsTimeToRunUpdateCheck()
{
	if (!IsHumanControllingUs())
		return false;

	int nextUpdateTime = EditorPrefs::GetInt("EditorUpdateNextCheck", 0);
	int currentTime = time(0); 
	
	// Sanity check the nextUpdateTime time. If it is more than MAX_UPDATE_INTERVAL something is wrong and it is reset
	if ( nextUpdateTime > currentTime + MAX_UPDATE_INTERVAL )  
	{
		printf_console("IsTimeToCheckForNewEditor: Update time (%d) was reset by sanity check\n", nextUpdateTime);
		EditorPrefs::SetInt("EditorUpdateNextCheck", 0);
		nextUpdateTime = 0;
	}
	
	printf_console("IsTimeToCheckForNewEditor: Update time %d current %d\n", nextUpdateTime, currentTime);
	return nextUpdateTime == 0 || nextUpdateTime < currentTime; 
}

static bool IsDebug()
{
	const char* envvar = "UNITY_DEBUG_EDITOR_UPDATE";
#if UNITY_WIN
	char result[100];
	DWORD success = GetEnvironmentVariable(envvar,result,100);
	if (!success) return false;
#else
	const char* result = getenv(envvar);
	if (result == NULL) return false;
#endif
	
	if (StrICmp(result, "1") == 0) return true;
	return false;
}

void EditorUpdateCheck(ShowUpdateWindow showUpdateWindow, bool editorstartup)
{
	string updateURL = UPDATE_URL;
	string currentVersionString = UNITY_VERSION_FULL_NICE;
	string currentVersion = "0";
	string authToken = LicenseInfo::Get()->GetAuthToken();
	string flags = GetFlags();
	
	bool debug = IsDebug();
	if ( debug )
	{
		updateURL = UPDATE_URL_DEBUG;
		currentVersionString = "0.0.0b0 (aaaaaaaaaaaa)";
	}
		
	// Hash the authToken, version and flags so we can check the validity on the server
	UInt8 hash[16];
	string s = "Vienaragis" + authToken + currentVersion + currentVersionString + flags;
	ComputeMD5Hash((const UInt8*)s.c_str(), s.size(), hash);
	string hashString = MD5ToString(hash); 	
	
	// Compose the message to send to the server
	UpdateCheckMessage *pMessage = new UpdateCheckMessage();
	string encodedCurrentVersionString = CurlUrlEncode(currentVersionString);
	pMessage->m_Uri = Format("%s?version=%s&versionString=%s", updateURL.c_str(), currentVersion.c_str(), encodedCurrentVersionString.c_str());
	if ( debug ) 
		pMessage->m_Uri += "&debug=1";
	
	pMessage->m_Headers.push_back("X-UNITY-FLAGS:" + flags);
	if ( !editorstartup ) 
	{
		// Add a header to tell that this is a manual updatecheck, and not a regular check at startup
		pMessage->m_Headers.push_back("X-UNITY-CHECK:1");
	}
	pMessage->m_Headers.push_back("X-UNITY-AUTH:" + authToken);
	pMessage->m_Headers.push_back("X-UNITY-HASH:" + hashString);
	pMessage->m_ShowUpdateWindow = showUpdateWindow;
	CurlRequestGet(pMessage);
}
