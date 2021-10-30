#include "UnityPrefix.h"
#include "Editor/Src/UploadingBuildsManager.h"
#include "Runtime/Export/WWW.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Serialize/TransferFunctions/YAMLRead.h"
#include "Runtime/Utilities/Argv.h"


namespace {
	UploadingBuildsManager* gSingleton = NULL;
	
	const char *kPublishURL = "http://live.unity3d.com/publish";//"http://live.hq.unity3d.com/publish";//"http://localhost/publish";
	
	
	struct WebplayerAuthorizationMessage: CurlRequestMessage
	{
		WebplayerAuthorizationMessage (UploadingBuild *uploadingBuild): m_UploadingBuild (uploadingBuild) {}
		
		
		virtual void Done ()
		{
			YAMLRead read (m_Result.c_str (), m_Result.size (), 0);
			
			UnityStr uploadToken = "", newUploadToken = "", error = "";
			int recoverable = 0;
			
			read.Transfer (uploadToken, "upload_token");
			read.Transfer (newUploadToken, "new_upload_token");
			read.Transfer (error, "error");
			read.Transfer (recoverable, "recoverable");
			
			if (!m_Success || error.length () > 0)
			{
				m_UploadingBuild->Unauthorized (error.length () > 0 ? (std::string)error : "Authentication request failed: " + m_Result, recoverable != 0);
			}
			else
			{
				if (newUploadToken.length () > 0)
				{
					m_UploadingBuild->BuildExists (uploadToken, newUploadToken);
				}
				else
				{
					m_UploadingBuild->Authorized (uploadToken);
				}
			}
		}
		
		
		UploadingBuild *m_UploadingBuild;
	};


	struct WebplayerPublishMessage: CurlRequestMessage
	{
		WebplayerPublishMessage (UploadingBuild *uploadingBuild): m_UploadingBuild (uploadingBuild) {}
		
		
		virtual void Done () 
		{
			m_UploadingBuild->m_BytesSent = m_UploadingBuild->m_TotalSize;
			
			if (!m_Success || m_ResponseCode != 200)
			{
				m_UploadingBuild->UploadFailure ("Uploading file failed" + (m_Result.length () > 0 ? ": " + m_Result + "." : "."));
			}
			else
			{
				YAMLRead read (m_Result.c_str (), m_Result.size (), 0);

				UnityStr url = "";
				read.Transfer (url, "url");
				
				if (url.length () == 0)
				{
					m_UploadingBuild->UploadFailure ("Build publishing failed. Server returned no destination URL.");
					return;
				}

				m_UploadingBuild->m_URL = url;
				m_UploadingBuild->UploadSuccess ();
			}
		}
		
		
		virtual void Progress (size_t transferred, size_t total)
		{
			m_UploadingBuild->m_BytesSent = transferred;
			m_UploadingBuild->m_TotalSize = total;
		}
		
		
		UploadingBuild *m_UploadingBuild;
	};
	
	
	inline void LogForTest (const std::string& message)
	{
		if (!IsHumanControllingUs ())
		{
			LogString (message);
		}
	}
	
	
	const std::string GetPlayerPath (const std::string& buildPath)
	{
		std::set<string> childPaths;
		GetFolderContentsAtPath (buildPath, childPaths);
	
		for (std::set<string>::const_iterator i = childPaths.begin (); i != childPaths.end (); i++)
		{
			string extension = i->substr (i->find_last_of ('.') + 1, i->length ());
			if (extension == "unity3d")
			// TODO: Expand this for flash players and NACL builds when we support those
			{
				return *i;
			}
		}
	
		return "";
	}


	const std::string GetBuiltInTemplateName (const std::string& templateName)
	{
		size_t splitIndex = templateName.find (':');
	
		if (splitIndex == string::npos || templateName.substr (0, splitIndex) != "APPLICATION")
		{
			return "";
		}
	
		return templateName.substr (splitIndex + 1, templateName.length ());
	}


	void LaunchErrorWindow (const std::string& errorMessage, bool recoverable)
	{
		void* params[1] = { MonoStringNew (errorMessage) };
		CallStaticMonoMethod ("BuildUploadCompletedWindow", Format ("LaunchFailure%s", recoverable ? "Recoverable" : "Critical").c_str (), params);
	}


	void LaunchSuccessWindow (const std::string& url)
	{
		void* params[1] = { MonoStringNew (url) };
		CallStaticMonoMethod ("BuildUploadCompletedWindow", "LaunchSuccess", params);
	}
}


UploadingBuildsManager& GetUploadingBuildsManager ()
{
	if (gSingleton == NULL)
	{
		gSingleton = new UploadingBuildsManager ();
	}
	
	return *gSingleton;
}


bool UploadingBuildsManager::ValidateSession ()
{
	m_SessionID = MonoStringToCpp ((MonoString*)CallStaticMonoMethod ("UploadingBuildsMonitor", "GetActiveSessionID"));
	
	return m_SessionID.length () != 0;
}


bool UploadingBuildsManager::VerifyLogin ()
{
	if (!ValidateSession ())
	{
		LogForTest ("Build publishing authorization failed: Not logged in.");
		
		CallStaticMonoMethod ("UploadingBuildsMonitor", "HandleNoSession");
		
		return false;
	}
	
	return true;
}


bool UploadingBuildsManager::VerifyLogin (const std::string& displayName)
{
	if (!ValidateSession ())
	{
		LogForTest ("Build publishing authorization failed: Not logged in.");
		
		void* params[1] = { MonoStringNew (displayName) };
		CallStaticMonoMethod ("UploadingBuildsMonitor", "HandleNoSession", params);
		
		return false;
	}
	
	return true;
}


void UploadingBuildsManager::BeginUploadBuild (const std::string& buildPath, bool autoRun)
{
	BeginUploadBuild (buildPath, GetPlayerSettings ().productName, UploadingBuild::kPrompt, autoRun);
}


void UploadingBuildsManager::BeginUploadBuild (const std::string& buildPath, const std::string& displayName, UploadingBuild::OverwriteHandling overwriteHandling, bool autoRun)
{
	UploadingBuild* uploadingBuild = new UploadingBuild (UploadingBuild::kAuthorizing, overwriteHandling);
	uploadingBuild->m_DisplayName = displayName;
	uploadingBuild->m_SessionID = m_SessionID;
	uploadingBuild->m_PlayerPathBeingUploaded = GetPlayerPath (buildPath);
	uploadingBuild->m_RunWhenUploaded = autoRun;

	bool alreadyExists = false;
	for (std::vector<UploadingBuild*>::iterator i = m_UploadingBuilds.begin (); i < m_UploadingBuilds.end (); i++)
	{
		UploadingBuild* previousBuild = *i;
		if (previousBuild->m_DisplayName == uploadingBuild->m_DisplayName)
		{
			switch (previousBuild->m_Status)
			{
				case UploadingBuild::kUploading:
					WarningStringWithoutStacktrace (Format ("Build '%s' is already uploading.", previousBuild->m_DisplayName.c_str ()));
				return;
				case UploadingBuild::kAuthorizing:
					WarningStringWithoutStacktrace (Format ("Build '%s' is already preparing for upload.", previousBuild->m_DisplayName.c_str ()));
				return;
			}

			delete previousBuild;
			*i = uploadingBuild;
						
			alreadyExists = true;
			break;
		}
	}
	
	if (!alreadyExists)
	{
		m_UploadingBuilds.push_back (uploadingBuild);
		CallStaticMonoMethod ("UploadingBuildsMonitor", "Activate");
	}
	
	if (VerifyLogin (displayName))
	{
		uploadingBuild->Upload ();
	}
	
	InternalStateChanged ();
}


bool UploadingBuildsManager::ResumeBuildUpload (const std::string& displayName, bool replace)
{
	for (std::vector<UploadingBuild*>::iterator i = m_UploadingBuilds.begin (); i < m_UploadingBuilds.end (); i++)
	{
		UploadingBuild* previousBuild = *i;
		if (previousBuild->m_DisplayName == displayName)
		{
			switch (previousBuild->m_Status)
			{
				case UploadingBuild::kAuthorizing:
					previousBuild->m_SessionID = m_SessionID;
					previousBuild->Upload ();
				return true;
				case UploadingBuild::kAuthorized:
					previousBuild->UploadPlayer (!replace);
				return true;
				default:
					LogForTest (Format ("Build publishing failed: Tried to resume build upload '%s' which is in a bad state.", displayName.c_str ()).c_str ());
				return false;
			}
		}
	}
	
	LogForTest (Format ("Build publishing failed: Tried to resume unknown build upload '%s'.", displayName.c_str ()).c_str ());
	
	return false;
}


void UploadingBuildsManager::EndUploadBuild ()
{
	InternalStateChanged ();
}


const std::vector<UploadingBuild*>& UploadingBuildsManager::GetUploadingBuilds ()
{
	return m_UploadingBuilds;
}


void UploadingBuildsManager::RemoveUploadingBuild (const std::string& displayName)
{
	for (std::vector<UploadingBuild*>::iterator i = m_UploadingBuilds.begin(); i < m_UploadingBuilds.end (); i++)
	{
		if ((*i)->m_DisplayName == displayName)
		{
			delete (*i);
			m_UploadingBuilds.erase (i);
			break;
		}
	}
}


void UploadingBuildsManager::InternalStateChanged ()
{
	CallStaticMonoMethod ("UploadingBuildsMonitor", "InternalStateChanged");
}


//-------------------------------------


UploadingBuild::UploadingBuild (Status buildStatus, OverwriteHandling overwriteHandling)
{
	m_Status = buildStatus;
	m_OverwriteHandling = overwriteHandling;
	m_RunWhenUploaded = false;
}


void UploadingBuild::Upload ()
{
	if (m_PlayerPathBeingUploaded.length () == 0)
	// Verify that we have an actual player in the build path
	{
		m_Status = kUploadFailed;

		LogForTest ("Build publishing failed: No player file found at build path.");
		LaunchErrorWindow ("No player file found at build path.", false);

		return;
	}
	
	string templateName = GetBuiltInTemplateName (GetPlayerSettings ().GetWebPlayerTemplate ());
		// TODO: We currently only support using built-in templates with UDN hosting. Modify this when adding support for custom templates.
	if (templateName.length () == 0)
	// Verify that we're using a built in template
	{
		m_Status = kUploadFailed;
		
		LogForTest ("Build publishing failed: Unsupported template.");
		LaunchErrorWindow ("Unsupported webplayer template. Only built in templates are supported for UDN publishing at this time.", false);

		return;
	}
	
	// Set up the POST request for authorization //
	string postBody = Format (
		"unity_version=%s&beta=%d&session_token=%s&app_id=%s&build_size=%d&template[id]=%s&template[WIDTH]=%d&template[HEIGHT]=%d",
		CurlUrlEncode (UNITY_VERSION).c_str (),
		UNITY_IS_BETA,
		CurlUrlEncode (m_SessionID).c_str (),
		CurlUrlEncode (m_DisplayName).c_str (),
		GetFileLength (m_PlayerPathBeingUploaded),
		CurlUrlEncode (templateName).c_str (),
		GetPlayerSettings ().GetDefaultWebScreenWidth (),
		GetPlayerSettings ().GetDefaultWebScreenHeight ()
	);
	
	// Add custom template keys //
	std::vector<std::string> templateKeys = GetPlayerSettings ().GetTemplateCustomKeys ();
	for (std::vector<string>::const_iterator i = templateKeys.begin (); i != templateKeys.end (); i++)
	{
		postBody += Format (
			"&template[CUSTOM_%s]=",
			CurlUrlEncode (*i).c_str ()
		) + CurlUrlEncode (GetPlayerSettings ().GetTemplateCustomValue (*i));
	}
	
	WebplayerAuthorizationMessage *authorizationMessage = new WebplayerAuthorizationMessage (this);
	authorizationMessage->m_Uri = kPublishURL;
	authorizationMessage->m_Method = "POST";
	authorizationMessage->m_PostData = postBody;
	authorizationMessage->m_ShouldAbortOnExit = true;
	
	CurlRequestGet (authorizationMessage, kCurlRequestGroupMulti);
}


void UploadingBuild::Unauthorized (const std::string& errorMessage, bool recoverable)
{
	m_Status = kUploadFailed;
	
	LogForTest (Format ("Build publishing authorization failed: %s.", errorMessage.c_str ()).c_str ());
	LaunchErrorWindow (errorMessage, recoverable);
		
	GetUploadingBuildsManager().EndUploadBuild ();
}


void UploadingBuild::BuildExists (const std::string& uploadToken, const std::string& newUploadToken)
{
	m_UploadToken = uploadToken;
	m_NewUploadToken = newUploadToken;
	
	LogForTest ("Build publishing: Build exists on server.");

	void* params[1] = { MonoStringNew (m_DisplayName) };
	
	switch (m_OverwriteHandling)
	{
		case kPrompt:
			m_Status = kAuthorized;

			CallStaticMonoMethod ("UploadingBuildsMonitor", "OverwritePrompt", params);
		break;
		case kOverwrite:
			m_Status = kUploading;
			
			LogForTest ("Overwriting online build.");
			
			UploadPlayer ();
		break;
		case kVersion:
			m_Status = kUploading;
			
			LogForTest ("Uploading new version of existing build.");
			
			UploadPlayer (true);
		break;
		case kCancel:
			m_Status = kUploadFailed;
			
			LogForTest ("Canceling upload.");
		break;
		default:
			ErrorString ("Unhandled enum");
		break;
	}
}


void UploadingBuild::Authorized (const std::string& uploadToken)
{
	m_UploadToken = uploadToken;
	m_Status = kUploading;
	
	LogForTest ("Build publishing authorized.");
	
	UploadPlayer ();
}


void UploadingBuild::UploadPlayer (bool useNewToken)
{
	string token = useNewToken ? m_NewUploadToken : m_UploadToken;
	
	if (token.length () == 0)
	{
		m_Status = kUploadFailed;
		
		LogForTest ("Build publishing upload failed: No upload token set.");
		LaunchErrorWindow ("Upload failed: No upload token set.", false);
		
		return;
	}
	
	WebplayerPublishMessage *publishMessage = new WebplayerPublishMessage (this);
	publishMessage->m_Uri = Format ("%s/%s", kPublishURL, token.c_str ());
	publishMessage->m_Method = "PUT";
	publishMessage->m_Headers.push_back ("Expect:");
		// Clear the Expect header to bypass issue with our lighttpd servers
	publishMessage->m_UploadPathName = m_PlayerPathBeingUploaded;
	publishMessage->m_ShouldAbortOnExit = true;
	
	CurlRequestGet (publishMessage, kCurlRequestGroupMulti);
}


void UploadingBuild::UploadFailure (const std::string& errorMessage)
{
	m_Status = kUploadFailed;
	
	LogForTest (Format ("Build publishing upload failed: %s.", errorMessage.c_str ()).c_str ());
	LaunchErrorWindow (errorMessage, false);
	
	GetUploadingBuildsManager().EndUploadBuild ();
}


void UploadingBuild::UploadSuccess ()
{
	m_Status = kUploaded;
	
	LogForTest (Format ("Build publishing succeeded. URL is: %s.", m_URL.c_str ()).c_str ());
	
	if (m_RunWhenUploaded)
	{
		OpenWithDefaultApp (m_URL);
	}
	else
	{
		LaunchSuccessWindow (m_URL);
	}

	GetUploadingBuildsManager().EndUploadBuild ();
}


float UploadingBuild::GetUploadProgress ()
{
	if (m_Status == kUploading)
	{
		if (m_TotalSize != 0)
		{
			return (m_BytesSent / (float)m_TotalSize);
		}
	}
	else if (m_Status == kUploaded)
	{
		return 1.0f;
	}
	
	return 0.0f;
}
