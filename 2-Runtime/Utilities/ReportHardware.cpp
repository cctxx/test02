#include "UnityPrefix.h"
#include "ReportHardware.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "HashFunctions.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Configuration/UnityConfigureOther.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Export/WWW.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/CPUInfo.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Utilities/GlobalPreferences.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/Word.h"

#if UNITY_LINUX
#include "PlatformDependent/Linux/X11Quarantine.h"
#endif

#if SUPPORT_REPRODUCE_LOG
#include "Runtime/Misc/ReproductionLog.h"
#endif

#if UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/AndroidSystemInfo.h"
#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#endif

#if UNITY_IPHONE
	extern "C" const char*	UnityAdvertisingIdentifier();
	extern "C" bool 		UnityAdvertisingTrackingEnabled();
#endif

#if UNITY_METRO
#include "PlatformDependent/MetroPlayer/AppCallbacks.h"
#include "PlatformDependent/MetroPlayer/MetroCapabilities.h"
#endif

#if UNITY_OSX
static int numberForKey( CFDictionaryRef desc, CFStringRef key )
{
    CFNumberRef value;
    int num = 0;
    if ( (value = (CFNumberRef)CFDictionaryGetValue(desc, key)) == NULL )
        return 0;
    CFNumberGetValue(value, kCFNumberIntType, &num);
    return num;
}
#endif

#if UNITY_EDITOR
const char* kHardwareReportDoneKey = "Editor StatsDone";
#elif UNITY_STANDALONE
const char* kHardwareReportDoneKey = "StandaloneStatsDone";
#else
const char* kHardwareReportDoneKey = "StatsDone";
#endif

// URL to report hardware info to
const char* kHardwareReportURL = "http://stats.unity3d.com/HWStats.cgi";


static bool CheckHardwareReportNeeded()
{
	#if !UNITY_ENABLE_HWSTATS_REPORT
	return false;
	#endif

	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
		return false;
	#endif

	#if !WEBPLUG
	if (!IsHumanControllingUs())
		return false;
	#endif

	#if UNITY_ANDROID
	// development player == no stats
	if (UNITY_DEVELOPER_BUILD)
		return false;

	if (!GetPlayerSettings().GetEnableHWStatistics())
		return false;

	// Only report once per OS version
	std::string osName = systeminfo::GetOperatingSystem();
	UInt8 md5Value[16];
	ComputeMD5Hash( reinterpret_cast<const UInt8*>( osName.c_str() ), osName.size(), md5Value );
	std::string md5String = BytesToHexString( md5Value, 16 );

	if (GetGlobalPreference(UNITY_VERSION_DIGITS) == md5String)
		return false;

	SetGlobalPreference(UNITY_VERSION_DIGITS, md5String);

	return true;
	#endif // #if UNITY_ANDROID


	#if UNITY_METRO
	if (metro::Capabilities::IsSupported(metro::Capabilities::kInternetClient, "", false) == false &&
		metro::Capabilities::IsSupported(metro::Capabilities::kInternetClientServer, "", false) == false)
		return false;
	#endif // #if UNITY_METRO

	#if UNITY_IPHONE || UNITY_BB10 || UNITY_TIZEN
	if (!GetPlayerSettings().GetEnableHWStatistics())
		return false;
	#endif

	if ( GetGlobalBoolPreference (kHardwareReportDoneKey, false) )
		return false;
	SetGlobalBoolPreference (kHardwareReportDoneKey, true);

	return true;
}


static std::string EscapeFieldString( const std::string& s )
{
	const char* kHexToLiteral = "0123456789abcdef";
	const char *kForbidden = "@&;:<>=?\"'/\\!#%+";

	std::string result;
	result.reserve( s.size() );
	for( size_t i = 0; i < s.size(); ++i ) {
		unsigned char c = s[i];
		if( c == 32 )
			result += '+';
		else if( c < 32 || c > 126 || strchr(kForbidden, c) != NULL )
		{
			result += '%';
			result += kHexToLiteral[c >> 4];
			result += kHexToLiteral[c & 0xF];
		}
		else
			result += c;
	}
	return result;
}


void HardwareInfoReporter::Shutdown()
{
	#if ENABLE_WWW
	if( m_InfoPost != NULL )
	{
		m_InfoPost->Release();
		m_InfoPost = NULL;
	}
	#endif
}

static void StripGfxDriverVersion(std::string& fullName, const std::string& version)
{
	size_t pos = fullName.find(version);

	if (pos != std::string::npos && pos > 1)
		fullName.erase(pos-1, version.size()+1);
}

void HardwareInfoReporter::ReportHardwareInfo()
{
#if ENABLE_WWW
	AssertIf( m_InfoPost != NULL );

	// If hardware info already reported, do nothing
	if( !CheckHardwareReportNeeded() )
		return;


	// Get all the information to post
	std::string infoUUID; // Used to be sent by 2.0.2, not anymore with 2.1. Keeping to match the hash value. Now used by Android and iPhone only.
	std::string deviceModel; // Used by Android and iPhone only
	// Report runtime version used
	std::string infoOS = systeminfo::GetOperatingSystem();
	std::string infoCPU = systeminfo::GetProcessorType();
	std::string infoGfxName = gGraphicsCaps.rendererString;
	std::string infoGfxVendor = gGraphicsCaps.vendorString;
	std::string infoGfxVersion = gGraphicsCaps.fixedVersionString;
	std::string infoGfxDriver = gGraphicsCaps.driverLibraryString;
#if UNITY_ANDROID || UNITY_IPHONE || UNITY_BB10 || UNITY_TIZEN
	std::string appId = GetPlayerSettings().GetiPhoneBundleIdentifier();
#else
	std::string appId = Format("%s.%s",GetPlayerSettings().GetCompanyName().c_str(), GetPlayerSettings().GetProductName().c_str());
#endif
	int infoCpuCount = systeminfo::GetProcessorCount();
	int infoRAM = systeminfo::GetPhysicalMemoryMB();
	int infoVRAM = gGraphicsCaps.videoMemoryMB;

	std::string unityVersion = UNITY_VERSION;	// Report runtime version used
	std::string buildVersion = "0.0.0";					// Unknown build version used

	// Get desktop display mode dimensions. Do this directly because
	// we initiate the report before screen manager is even set up
	// (so that in case something horrible happens while initializing
	// the game, we'd still get a chance to get the report).
	#if UNITY_WIN && !UNITY_WINRT
	DEVMODE displayMode;
	memset( &displayMode, 0, sizeof(displayMode) );
	displayMode.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &displayMode );
	std::string infoScreenRes = Format("%i x %i", displayMode.dmPelsWidth, displayMode.dmPelsHeight);

	#elif UNITY_OSX
	CFDictionaryRef currentMode = CGDisplayCurrentMode(kCGDirectMainDisplay);
	int desktopWidth = numberForKey(currentMode, kCGDisplayWidth);
	int desktopHeight = numberForKey(currentMode, kCGDisplayHeight);
	std::string infoScreenRes = Format("%i x %i", desktopWidth, desktopHeight);

	#elif UNITY_LINUX
	unsigned width = 0;
	unsigned height = 0;
	#if SUPPORT_X11
	NativeDisplayPtr display = OpenDisplay(NULL);
	NativeWindow rootWindow = GetRootWindow(display);
	GetWindowSize(display, rootWindow, &width, &height);
	CloseDisplay(display);
	#endif
	std::string infoScreenRes = Format("%u x %u", width, height);
	#elif UNITY_ANDROID
	// Create a less obscure OS string
	infoOS = Format ("%s API-%i", systeminfo::GetDeviceSystemName(), android::systeminfo::ApiLevel());
	// Strip 'driverLibraryString' from 'fixedVersionString', if duplicated
	StripGfxDriverVersion(infoGfxVersion, infoGfxDriver);
	// Get the raw display size (physical size of the display)
	int width, height;
	android::systeminfo::DisplaySize(&width, &height);
	std::string infoScreenRes = Format("%u x %u", width, height);
	// Use ANDROID ID/IMEI/WiFi MAC as UUID
	infoUUID = android::systeminfo::UniqueIdentifier();
	// This matches the first part of ro.build.fingerprint
	deviceModel = Format ("%s/%s/%s", android::systeminfo::Manufacturer(), android::systeminfo::Model(), android::systeminfo::Device());
	#elif UNITY_IPHONE
	std::string infoScreenRes = Format("%u x %u", GetScreenManager().GetWidth(), GetScreenManager().GetHeight());
	infoUUID = systeminfo::GetDeviceUniqueIdentifier(); // it is hash of WiFi MAC address
	deviceModel = systeminfo::GetDeviceModel();
	StripGfxDriverVersion(infoGfxVersion, infoGfxDriver);
	#elif UNITY_BLACKBERRY
	std::string infoScreenRes = Format("%u x %u", GetScreenManager().GetWidth(), GetScreenManager().GetHeight());
	// This will be EMPTY for Blackberry if permission is set false
	infoUUID = systeminfo::GetDeviceUniqueIdentifier(); // Unique ID from OS / it is hash of WiFi MAC address
	deviceModel = Format("%s/%s", "BlackBerry", systeminfo::GetDeviceModel());
	StripGfxDriverVersion(infoGfxVersion, infoGfxDriver);
	#elif UNITY_METRO
	std::string infoScreenRes = Format("%u x %u", GetScreenManager().GetWidth(), GetScreenManager().GetHeight());
	// It seems if we call GetDeviceUniqueIdentifier, this somehow distrubts the data flow, and server doesn't accept it
	infoUUID = systeminfo::GetDeviceUniqueIdentifier(); // it is hash of WiFi MAC address
	{
		// Make infoUUID different one from the Editor and encode it again
		infoUUID+="Metro";
		UInt8 md5Value[16];
		ComputeMD5Hash( reinterpret_cast<const UInt8*>(infoUUID.c_str() ), infoUUID.size(), md5Value);
		infoUUID = BytesToHexString(md5Value, 16);
	}
	deviceModel = "n/a";
	#elif UNITY_WP8
	auto& screenManager = GetScreenManager ();
	std::string infoScreenRes;	
	if (screenManager.GetScreenOrientation () == ScreenOrientation::kLandscapeLeft || screenManager.GetScreenOrientation () == ScreenOrientation::kLandscapeRight)
	{
		infoScreenRes = Format ("%u x %u", screenManager.GetHeight (), screenManager.GetWidth ());
	}
	else
	{
		infoScreenRes = Format ("%u x %u", screenManager.GetWidth (), screenManager.GetHeight ());
	}
	infoUUID = systeminfo::GetDeviceUniqueIdentifier ();
	deviceModel = systeminfo::GetDeviceName ();
    #elif UNITY_TIZEN
	std::string infoScreenRes = Format("%u x %u", GetScreenManager().GetWidth(), GetScreenManager().GetHeight());
	infoUUID = systeminfo::GetDeviceUniqueIdentifier();
	deviceModel = Format("%s/%s", "Tizen", systeminfo::GetDeviceModel());
	StripGfxDriverVersion(infoGfxVersion, infoGfxDriver);
	#else
	#error "Unsupported platform"
	#endif

	// Various flags, bits:
	// 0..3: license type
	// 4..5: metro presentation types
	// 6..17: CPU capabilities, reserved up to 23
	// 24..25: GPU capabilities
	int flags = 0;
	BuildSettings* buildSettings = &GetBuildSettings();
	if (buildSettings)
	{
		// bits 0..2: 1=Indie, 2=Pro
		flags |= buildSettings->hasPROVersion ? 2 : 1;
		// bit 3: 0=Licensed, 1=Trial
		if (!buildSettings->hasPublishingRights)
			flags |= 8;

		// Report build version used
		buildVersion = buildSettings->GetVersion();
	}

#if UNITY_METRO
	// bits 4..5: 1=XAML app, 2=D3D app
	flags |= UnityPlayer::AppCallbacks::Instance->GetAppType() == UnityPlayer::AppCallbacks::kAppTypeD3DXAML ? (1 << 4) : (1 << 5);
#endif

	// CPU capabilities flags, bits 6..17
	flags |= (1<<6); // CPU caps bits are present (to determine between earlier versions that did not send these flags at all)
	if (CPUInfo::HasSSE2Support())
		flags |= (1<<7);
	if (CPUInfo::HasSSE41Support())
		flags |= (1<<8);
	if (CPUInfo::HasSSE42Support())
		flags |= (1<<9);
	if (CPUInfo::HasAVXSupport())
		flags |= (1<<10);
	if (CPUInfo::HasNEONSupport())
		flags |= (1<<11);
	if (CPUInfo::HasSSE3Support())
		flags |= (1<<12);
	if (CPUInfo::HasSupplementalSSE3Support())
		flags |= (1<<13);
	if (CPUInfo::HasAVX2Support())
		flags |= (1<<14);
	if (CPUInfo::HasAVX512Support())
		flags |= (1<<15);
	if (CPUInfo::HasFP16CSupport())
		flags |= (1<<16);
	if (CPUInfo::HasFMASupport())
		flags |= (1<<17);

	// GPU capabilities flags, bits 24..25
	if (gGraphicsCaps.hasNativeDepthTexture)
		flags |= (1<<24);
	if (gGraphicsCaps.hasNativeShadowMap)
		flags |= (1<<25);


	// Send a hash some fields plus a salt as a cheap way to prevent "spam" on hardware
	// report script.
	std::string stringToHash;
	stringToHash = std::string("KonfiguracijosReportoDruska-") + infoUUID + infoOS + infoCPU + infoGfxName + infoGfxVendor + infoGfxVersion;

	UInt8 md5Value[16];
	ComputeMD5Hash( reinterpret_cast<const UInt8*>( stringToHash.c_str() ), stringToHash.size(), md5Value );
	std::string md5String = BytesToHexString( md5Value, 16 );

	// Construct POST request data
	std::string postData;
	postData += "os=" + EscapeFieldString(infoOS);
	postData += "&cpu=" + EscapeFieldString(infoCPU);
	postData += "&gfxname=" + EscapeFieldString(infoGfxName);
	postData += "&gfxvendor=" + EscapeFieldString(infoGfxVendor);
	postData += "&gfxversion=" + EscapeFieldString(infoGfxVersion);
	postData += "&gfxdriver=" + EscapeFieldString(infoGfxDriver);
	postData += "&cpucount=" + IntToString(infoCpuCount);
	postData += "&ram=" + IntToString(infoRAM);
	postData += "&vram=" + IntToString(infoVRAM);
	postData += "&screen=" + EscapeFieldString(infoScreenRes);
	postData += "&platform=" + IntToString(systeminfo::GetRuntimePlatform());
	postData += "&flags=" + IntToString(flags);
	postData += "&hash=" + md5String;
	postData += "&appId=" + EscapeFieldString(appId);
#if UNITY_ANDROID || UNITY_IPHONE || UNITY_BB10 || UNITY_WINRT || UNITY_TIZEN
	postData += "&uuid=" + infoUUID;
	postData += "&model=" + deviceModel;
#endif
#if UNITY_IPHONE
	if (const char *iOSAdvertisingIdentifier = UnityAdvertisingIdentifier())
		postData += "&adid=" + std::string(iOSAdvertisingIdentifier);
	postData += "&adtrack=" + std::string(UnityAdvertisingTrackingEnabled()?"1":"0");
#endif
	postData += "&unity=" + unityVersion;
	postData += "&build=" + buildVersion;

	// Initiate POST request
	std::map<std::string,std::string> headers;
	headers.insert( std::make_pair("Content-Type", "application/x-www-form-urlencoded") );

	m_InfoPost = WWW::Create( kHardwareReportURL, postData.c_str(), postData.size(), headers, false  );

	/*
	// Server accepts only 5% percent of data, so if you want to debug, use the loop below, to ensure that data reaches server
	for (int i = 0; i < 100; i++)
	{
		m_InfoPost = WWW::Create( kHardwareReportURL, postData.c_str(), postData.size(), headers, false  );
		Thread::Sleep(0.1f);
	}
	*/
#endif // ENABLE_WWW
}
