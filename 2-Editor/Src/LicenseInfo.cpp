#include "UnityPrefix.h"
#include "Configuration/UnityConfigureOther.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Src/Utility/Base64.h"
#include "Runtime/Utilities/HashFunctions.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Src/LicenseManager.h"
#include "Editor/Platform/Interface/EditorUtility.h"

#include <string.h>
#if UNITY_OSX
#include <arpa/inet.h>
#endif
#if UNITY_WIN
#include <WinSock.h>
#include "PlatformDependent/Win/WinUnicode.h"
#pragma warning(disable:4200)
#endif

#define DEBUG_COPY_PROTECTION 0


LicenseManager* m_LicenseManager = NULL;

LicenseInfo* gSingleton = NULL;

LicenseInfo::LicenseInfo() : 
	m_DidCreateWindow(false)
	, m_UserClosedWindow(false)
	, m_ServerErrorSignaled(false)
	, m_ShouldExitOnCancel(false)
	, m_RunningUserTriggeredUpdate(false)
#if UNITY_WIN
	, m_RunningUserReactivation(false)
#endif
{
	InitializeProtection();
}


void LicenseInfo::InitializeProtection() 
{
	m_TX_RX_IDS.clear();
	if (m_LicenseManager == NULL)
		m_LicenseManager = new LicenseManager();
	int state = m_LicenseManager->Initialize();
	state = kLicenseStatus_Valid;
	switch(state)
	{
		case kLicenseStatus_Valid:
			break;

		case kLicenseStatus_Unknown:
			break;
		
		// No PACE or ULF license found
		case kLicenseErrorFlag_NoLicense:
			break;

		case kLicenseErrorFlag_PaceLicenseExpired:
			GenericErrorDialog("Your license has expired.");
			break;

		case kLicenseErrorFlag_FileCompromised:
			GenericErrorDialog("Unity license information is invalid.");
			break;

		case kLicenseErrorFlag_LicenseNotReadyForUse:
			GenericErrorDialog("Unity license is not yet valid.");
			break;

		case kLicenseErrorFlag_LicenseExpired:
			GenericErrorDialog("Unity license has expired.");
			break;
		
		case kLicenseErrorFlag_Initialization:
			if (DisplayDialog("Error initializing license system.", "Please contact support@unity3d.com\n\n", "Report a bug", "Quit"))
				LaunchBugReporter(kManualSimple);
			exit(1);
			break;
			
		case kLicenseErrorFlag_VersionMismatch:
			GenericErrorDialog("License is not for this version of Unity.");
			break;

		case kLicenseErrorFlag_MachineBinding1:
#if UNITY_WIN
			// If only Product ID changed we'll allow the user to deactivate the license and reactivate.
			if (DisplayDialog("License issue detected!", "There was a problem validating the license because the Operating System identification seems to have changed.\n\n Would you like to deactivate this license and re-activate?\n\n For more information see online help at\nhttp://unity3d.com/unity/err/license\nFor assistance contact support@unity3d.com", "Quit", "Re-activate"))
				exit(1);
			else
			{
				m_RunningUserReactivation = true;
				DisplayProgressbar("Re-activating", "Returning current license...", 0);
				m_LicenseManager->ReturnLicense();
			}
			break;
#endif
		case kLicenseErrorFlag_MachineBinding2:
		case kLicenseErrorFlag_MachineBinding4:
			GenericErrorDialog("Machine identification is invalid for current license.");
			break;

		default:
			UnknownErrorDialog (Format("An unknown error has occurred (0x%x).", state).c_str());
			break;
	}
	
	gSingleton = this;
	
	m_Tokens = m_LicenseManager->GetTokens();
	if (GetApplicationPtr())
		GetApplicationPtr()->ReadLicenseInfo();
}

void LicenseInfo::SetRXValue(const char* value)
{
	m_TX_RX_IDS = m_LicenseManager->m_txID + ";" + value;
}

void LicenseInfo::SignalUserClosedWindow()
{
	m_UserClosedWindow = true;
}


void LicenseInfo::GenericErrorDialog(const char* title)
{
	DisplayErrorDialog(title, "http://unity3d.com/unity/err/license");
}

void LicenseInfo::UnknownErrorDialog(const char* title)
{
	DisplayErrorDialog(title, "http://unity3d.com/unity/err/unknown");
}

void LicenseInfo::DisplayErrorDialog(const char* title, const char* url)
{
	if (DisplayDialog(title, Format("For more information see online help at\n%s\n\nFor assistance contact support@unity3d.com", url), "Quit", "Re-activate"))
	{
		exit(1);
	}
	else
	{
		m_ShouldExitOnCancel = true;
		NewActivation();
	}
}

// Flush editor UI state and repaint for when switching licensing
void LicenseInfo::ReloadEditorUI ()
{
	// When launching directly into the license system message pump (no license existed before) then this pointer can be null
	if (GetApplicationPtr ())
	{
		GetApplicationPtr ()->RequestScriptReload ();
		GetApplicationPtr ()->RequestRepaintAllViews ();
	}
}

// This method is called from the main loop - void Application::TickTimer()
void LicenseInfo::Tick() 
{
	// No license found? or do we need to update an existing license?
	if( m_LicenseManager->m_LicenseStatus != kLicenseStatus_Valid || m_LicenseManager->m_LicenseStatus != kLicenseStatus_Return )
	{
		// Wait for server communication to finish
		if( ! LicenseInfo::Get()->OngoingServerCommunication() )
		{
			std::vector<string> params;

			switch ( m_LicenseManager->m_LicenseStatus )
			{
				case kLicenseStatus_New:
				case kLicenseStatus_Update:
				case kLicenseStatus_Convert:
				case kLicenseStatus_Return:

					if( m_LicenseManager->m_rxID.size() == 0 )
					{
						if (!m_DidCreateWindow)
						{
							m_UserClosedWindow = false;
							CreateLicenseActivationWindow();
						}

						// Only continue flow if user didn't close the license server dialog.
						if( m_UserClosedWindow )
						{
							if( m_LicenseManager->m_LicenseStatus == kLicenseStatus_Update )
							{
								m_UserClosedWindow = false;
								DestroyLicenseActivationWindow();
								// Revert back to valid status (you can only go to update status from a valid one)
								m_LicenseManager->m_LicenseStatus = kLicenseStatus_Valid;
							}
							else if (m_LicenseManager->m_LicenseStatus != kLicenseStatus_Valid && !m_LicenseManager->RecoverBackupLicense())
							{
								//DisplayDialog ("Unity license activation flow aborted.", "", "Quit");
								ExitDontLaunchBugReporter ();                        
							}
							else if (m_ShouldExitOnCancel)
								exit(1);
							else
								m_ShouldExitOnCancel = false;
						}

						if (m_ServerErrorSignaled)
						{
							m_ServerErrorSignaled = false;
							if (m_DidCreateWindow)
								DestroyLicenseActivationWindow();

							m_LicenseManager->HandleServerErrorAndExit();
						}

						if( m_TX_RX_IDS.size() > 0 )
						{
							CallActivationServer(m_TX_RX_IDS);
						}
					}
					else
					{
						CallActivationServer();
					}  
 
					break;
					
				case kLicenseStatus_Updated:
					m_LicenseManager->m_LicenseStatus = kLicenseStatus_Valid;
					InitializeProtection();
					ShowUpdateFeedbackDialog(true);
					ReloadEditorUI ();
					break;

				case kLicenseStatus_Converted:
					InitializeProtection();
					DestroyLicenseActivationWindow();
					ReloadEditorUI ();
					break;
					
				case kLicenseStatus_Issued:
					InitializeProtection();
					ReloadEditorUI ();
					break;
					
				case kLicenseStatus_Returned:
					
					m_LicenseManager->DeleteLicenseFile();
#if UNITY_WIN
					if (m_RunningUserReactivation)
					{
						ClearProgressbar();
						InitializeProtection();
						m_RunningUserReactivation = false;
						break;
					}
#endif
					DisplayDialog ("Unity license returned.", "", "Quit");
					ExitDontLaunchBugReporter ();
					break;

				case kLicenseStatus_Valid:
					ShowUpdateFeedbackDialog(false);
					if( m_UserClosedWindow )
					{
						m_UserClosedWindow = false;
						DestroyLicenseActivationWindow();
					}
				default:
					break;
			}            
		}
	}
}

void LicenseInfo::ShowUpdateFeedbackDialog(bool success)
{
	if (m_RunningUserTriggeredUpdate)
	{
		m_RunningUserTriggeredUpdate = false;
		if (success)
			DisplayDialog("Information", "License has been updated.", "OK");
		else
			DisplayDialog("Warning", "Failed to get updated license from license server. Please try again later or check out http://unity3d.com/unity/err/license for possible resolutions.", "OK");
	}
}


bool LicenseInfo::GetLargeFlag(LicenseInfoFlag f)
{
	switch(f)
	{
	case lf_trial:
		return m_LicenseManager->m_HasExpirationDate;
	default:
		LicenseLog("Unknown feature: %d\n", f);
	}
	return false;
}


string LicenseInfo::GetLicenseString ()
{
	string licenseType;
	
	if (Flag(lf_pro_version))
		licenseType = "License type: Unity Pro";
	else if (Flag(lf_unity_free))
		licenseType = "License type: Unity";
	else
		licenseType = "License type: Unity Indie";

	if (Flag(lf_embedded))
		licenseType = "License type: Unity for Embedded Systems";
			
	if (Flag(lf_maint_client))	
		licenseType += ", Team";
	
	if (Flag(lf_iphone_pro))	
		licenseType += ", iPhone Pro";
	else if (Flag(lf_iphone_basic))
		licenseType += ", iPhone";
	
	if (Flag(lf_android_pro))	
		licenseType += ", Android Pro";
	else if (Flag(lf_android_basic))
		licenseType += ", Android";
	
	if (Flag(lf_flash_pro))
		licenseType += ", Flash Pro";
	else if (Flag(lf_flash_basic))
		licenseType += ", Flash";
	
	if (Flag(lf_winrt_pro))
		licenseType += ", WinStore Pro";
	else if (Flag(lf_winrt_basic))
		licenseType += ", WinStore";

	if (Flag(lf_bb10_pro))
		licenseType += ", BlackBerry Pro";
	else if (Flag(lf_bb10_basic))
		licenseType += ", BlackBerry";

    if (Flag(lf_tizen_pro))
		licenseType += ", Tizen Pro";
	else if (Flag(lf_tizen_basic))
		licenseType += ", Tizen";

	if (Flag(lf_wii))	
		licenseType += ", Wii";
	
	if (Flag(lf_xbox_360))	
		licenseType += ", Xbox 360";
	
	if (Flag(lf_ps3))	
		licenseType += ", PlayStation 3";
	
	if (Flag(lf_prototyping_watermark))
		licenseType += "\nNot for release";
	
	if (Flag(lf_edu_watermark))
		licenseType += "\nFor educational use only";
	
	if (Flag(lf_pro_version)) 
		return Format ("%s\nSerial number: %s", licenseType.c_str(), GetDisplaySerialNumber().c_str());
	else
		return Format ("%s\nSerial number: %s", licenseType.c_str(), GetDisplaySerialNumber().c_str());
}

bool LicenseInfo::OngoingServerCommunication()
{
	if( m_LicenseManager->m_State == kOngoingLicenseServerCommunication || m_LicenseManager->m_State == kOngoingActivationServerCommunication )
	{
		return true;
	}
	else
	{
		return false;
	}
}

string LicenseInfo::GetMachineID()
{
	return m_LicenseManager->GetMachineID();	
}

string LicenseInfo::GetAuthToken ()
{
	return m_LicenseManager->GetAuthToken(); 
}


// Warning, m_Tokens is only updated in InitializeProtection, if the LicenseManager has updated
// it's m_Tokens after a server update InitializeProtection must be called
UInt64 LicenseInfo::GetRawFlags ()
{
	if (m_Tokens == 0)
	{
		m_Tokens = 12345678;
	}

	return m_Tokens;
}


void LicenseInfo::Reauthorize()
{
	CallStaticMonoMethod("LicenseManagementWindow", "ShowWindow");
}


LicenseInfo::~LicenseInfo()
{
	if (gSingleton == this)
		gSingleton = NULL;
	
	delete m_LicenseManager;
}


LicenseInfo* LicenseInfo::Get ()
{
	if(gSingleton == NULL)
		gSingleton = new LicenseInfo();
	return gSingleton;
}

void LicenseInfo::Cleanup ()
{
	delete gSingleton;
	gSingleton = NULL;
}


string LicenseInfo::GetDisplaySerialNumber ()
{
	string displaySerial = m_LicenseManager->SerialNumber();
	if( displaySerial.size() > 4 ) {
		for (int i=displaySerial.size()-4;i<displaySerial.size();i++)
			displaySerial[i] = 'X';
	}
	return displaySerial;
}


int LicenseInfo::SaveLicenseFile ( string licenseData )
{
	return m_LicenseManager->WriteLicenseFile( licenseData, true );
}


void LicenseInfo::ReturnLicense ()
{
	if( DisplayDialog ("Information", "Are you sure you want to return your license?", "Yes", "No") )
	{
		m_LicenseManager->ReturnLicense();
	}

}


bool LicenseInfo::IsLicenseUpdated ()
{
	return m_LicenseManager->IsLicenseUpdated();
}


void LicenseInfo::CallActivationServer ()
{
	m_LicenseManager->CallActivationServer();
}

void LicenseInfo::CallActivationServer (string TX_RX_IDS)
{
	m_LicenseManager->CallActivationServer(TX_RX_IDS);
}


void LicenseInfo::QueryLicenseUpdate (bool forceCheck)
{
	if( (m_LicenseManager->m_LicenseStatus == kLicenseStatus_Valid && m_LicenseManager->m_MustUpdate) || forceCheck )
	{
		 if (forceCheck)
			gSingleton->m_RunningUserTriggeredUpdate = true;
		m_LicenseManager->QueryLicenseUpdate();
	}
}


void LicenseInfo::SignalServerError()
{
	m_ServerErrorSignaled = true;
}

void LicenseInfo::NewActivation ()
{
	m_TX_RX_IDS.clear();
	m_LicenseManager->NewActivation();
}

void LicenseInfo::ManualActivation ()
{
	m_LicenseManager->ManualActivation();
}

const std::string LicenseInfo::GetLicenseURL()
{
	return m_LicenseManager->GetLicenseURL();
}

bool LicenseInfo::UnknownStatus()
{
	return m_LicenseManager->UnknownStatus();
}

void LicenseLog (const char* format, ...)
{
	#if DEBUG_COPY_PROTECTION
	time_t getTime = time(NULL);
	struct tm *now = localtime(&getTime);
	va_list va;
	va_start( va, format );
	printf_console("\nLICENSE SYSTEM [%d%d%d %d:%d:%d] %s\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, VFormat(format, va).c_str());
	#endif
}
