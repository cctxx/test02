#include "UnityPrefix.h"
#include "UserAuthorizationManager.h"
#include "Runtime/Misc/GameObjectUtility.h"

#if WEBPLUG
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Utilities/GlobalPreferences.h"
#include "PlatformDependent/CommonWebPlugin/Verification.h"
#endif

#if UNITY_EDITOR
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#endif

UserAuthorizationManager gUserAuthorizationManager;

UserAuthorizationManager& GetUserAuthorizationManager()
{
	return gUserAuthorizationManager;
}

UserAuthorizationManager::UserAuthorizationManager()
{
	m_AuthorizationOperation = NULL;
	Reset ();
}

#if WEBPLUG && !UNITY_PEPPER
std::string GetUserAuthorizationModeKey ()
{
	static const char *kUserAuthorizationModeKey = "UserAuthorizationMode";
	std::string domain = GetStrippedPlayerDomain ();
	std::string hash = GenerateHash ((unsigned char*)&domain[0], domain.size());
	return kUserAuthorizationModeKey + hash;
}
#endif

void UserAuthorizationManager::Reset()
{
#if WEBPLUG && !UNITY_PEPPER
	m_AuthorizationMode = kNone;
	if (GetPlayerSettingsPtr())
	{
		std::string auth = GetGlobalPreference(GetUserAuthorizationModeKey().c_str());
		sscanf(auth.c_str(), "%x", &m_AuthorizationMode);
	}
#else
#if UNITY_EDITOR
	if (GetEditorUserBuildSettingsPtr() != NULL && GetBuildTargetGroup(GetEditorUserBuildSettings().GetActiveBuildTarget()) == kPlatformWebPlayer)
		m_AuthorizationMode = kNone;
	else
#endif
	// Allow Webcam and Microphone on all others targets than webplayer.
	m_AuthorizationMode = kWebCam | kMicrophone;
#endif
	if (m_AuthorizationOperation)
	{
		m_AuthorizationOperation->Release();
		m_AuthorizationOperation = NULL;
	}
	m_AuthorizationRequest = kNone;
}

AsyncOperation *UserAuthorizationManager::RequestUserAuthorization(int mode)
{
	if (m_AuthorizationOperation != NULL)
	{
		ErrorString ("A RequestUserAuthorization is already pending.");
	}	
	else if (!HasUserAuthorization (mode))
	{
		m_AuthorizationRequest = mode;
		m_AuthorizationOperation = new UserAuthorizationManagerOperation ();
		m_AuthorizationOperation->Retain();
		return m_AuthorizationOperation;
	}

	return new UserAuthorizationManagerErrorOperation ();
}	

void UserAuthorizationManager::ReplyToUserAuthorizationRequest(bool reply, bool remember)
{
	if (reply)
	{
		m_AuthorizationMode |= m_AuthorizationRequest;
#if WEBPLUG && !UNITY_PEPPER
		if (remember)
			SetGlobalPreference(GetUserAuthorizationModeKey().c_str(), Format("%x", m_AuthorizationMode).c_str());
#endif
	}
	m_AuthorizationRequest = kNone;
	if (m_AuthorizationOperation)
	{
		m_AuthorizationOperation->InvokeCoroutine();
		m_AuthorizationOperation->Release();
		m_AuthorizationOperation = NULL;
	}
}	

MonoBehaviour *UserAuthorizationManager::GetAuthorizationDialog ()
{
	if (m_AuthorizationRequest != kNone)
	{
		if (!m_AuthorizationDialog.IsValid())	 	
		{
			m_AuthorizationDialog = &CreateGameObject ("", "Transform", "UserAuthorizationDialog", NULL);
			m_AuthorizationDialog->SetHideFlags (Object::kHideInHierarchy);
		}
		return m_AuthorizationDialog->QueryComponent (MonoBehaviour);
	}
	else
	{
		if (m_AuthorizationDialog.IsValid ())
			DestroyObjectHighLevel (m_AuthorizationDialog, true);
		return NULL;
	}
}
