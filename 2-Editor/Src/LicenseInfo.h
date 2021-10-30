#ifndef LICENSE_INFO_H
#define LICENSE_INFO_H

#include <sys/types.h>

enum LicenseInfoFlag
{
	lf_pro_version=0,
	lf_unused=1,					// Was used to detect if one should show watermarks, which did not work for trial activations. Use lm.Flag(lf_activated) && ! lm.Flag(lf_trial) instead
	lf_maint_client=2,				// Asset server flag
	lf_iphone_basic=3,
	lf_iphone_pro=4,
	lf_xbox_360=5,
	lf_ps3=6,
	lf_mmo=7,
	lf_wii=8,
	lf_psp=9,
	lf_nokia_phone=10,
	lf_sony_ericsson_phone=11,
	lf_android_basic=12,
	lf_android_pro=13,
	lf_flash_basic=14,
	lf_flash_pro=15,
	lf_embedded=16,
	lf_bb10_basic=17,
	lf_bb10_pro=18,
	lf_winrt_basic=19,
	lf_winrt_pro=20,
#if CLUSTER_RENDERING
	lf_cluster_rendering=30,		// Flag to enable cluster rendering build
#endif
	lf_gis=31,
	lf_closed_network=32,
	lf_tizen_basic=33,
	lf_tizen_pro=34,

	lf_no_watermark=60,				// Used to disable the trial watermark. No features enabled/disabled
	lf_prototyping_watermark=61,	// Used to enable the prototyping watermark. No features enabled/disabled
	lf_unity_free=62,				// Should be shown as simply Unity (no Pro nor Indie) in the about dialogue,
	lf_edu_watermark=63,			// Used to enable the educational watermark. No features enabled/disabled
	
	lf_trial=64,					// Set to true if the current license is a trial license (and not only an unactivated demo)
};


class LicenseInfo
{
public:
	LicenseInfo();
	~LicenseInfo();
	
	static LicenseInfo* Get();
	static void Cleanup();
	std::string GetDisplaySerialNumber();
	void Reauthorize();
	void NewActivation();
	void ManualActivation();
	std::string GetLicenseString();
	std::string GetAuthToken();
	UInt64 GetRawFlags ();

	// This function is inlined for a reason!
	// Inlining it makes it a bit harder to disable the license checks in our code. A hacker would only have to change a single entry point and make it return true if it was a non-inlined function.
	// (There are other places in the license checking code that could be enhanced similarily. For instance, we should re-read the license flags from the copy protection once in a while to
	// verify that m_tokens hasn't been modified.)
	static bool Flag (LicenseInfoFlag f)
	{
		LicenseInfo* info = Get ();
		
		// During init tokens are read in and stored as bits in m_tokens
		// If f < 64, the flag corresponds to a token,
		// otherwise the check gets handed over to the GetLargeFlag method
		if (f < lf_trial)
		{
			return (info->m_Tokens>>f) & 1;
		}
		else
		{
			return info->GetLargeFlag(f);
		}
	}
	
	inline static bool Activated() { return Get()->GetRawFlags() != 0; }

	int SaveLicenseFile( std::string );
	void ReturnLicense();
	
	void Tick();

	void CallActivationServer();
	void CallActivationServer( std::string );
	static void QueryLicenseUpdate(bool forceCheck = false);
	
	bool OngoingServerCommunication();
	bool IsLicenseUpdated();
	void SetRXValue(const char* value);
	void SignalUserClosedWindow();
	void SignalServerError();
	bool WindowIsOpen() { return m_DidCreateWindow; }
	const std::string GetLicenseURL();
	bool UnknownStatus();

	void DestroyLicenseActivationWindow();
	
private:
	void GenericErrorDialog(const char* title);
	void UnknownErrorDialog(const char* title);
	void DisplayErrorDialog(const char* title, const char* url);
	void CreateLicenseActivationWindow();
	void Relaunch(std::vector<std::string>& args);
	void ReloadEditorUI ();
	void InitializeProtection() ;
	bool GetLargeFlag(LicenseInfoFlag f);
	std::string GetMachineID();
	void ShowUpdateFeedbackDialog(bool success);

	unsigned long m_AuthState;
	UInt64 m_Tokens; 
	char m_SerialNumber[32]; 
	std::string m_TX_RX_IDS;
	bool m_UserClosedWindow;
	bool m_ServerErrorSignaled;
	bool m_DidCreateWindow;
	bool m_ShouldExitOnCancel;
	bool m_RunningUserTriggeredUpdate;
#if UNITY_WIN
	bool m_RunningUserReactivation;
#endif
};

void LicenseLog (const char* format, ...);

#endif
