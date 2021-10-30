#ifndef PROJECTSETTINGS_H
#define PROJECTSETTINGS_H

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Camera/RenderLoops/RenderLoopEnums.h"
#include "Runtime/Math/Vector2.h"

#if UNITY_EDITOR
#include "Editor/Src/EditorOnlyPlayerSettings.h"
#endif

class Texture2D;



enum { kDisableResolutionDialog = 0, kShowResolutionDialog = 1, kShowResolutionDialogDefaultNone = 2 };

enum ResolutionAspect
{
	kAspectOthers = 0,
	kAspect4by3,
	kAspect5by4,
	kAspect16by10,
	kAspect16by9,
	kAspectCount
};

enum MacFullscreenMode
{
	kCaptureDisplay,
	kFullscreenWindow,
	kFullscreenWindowWithMenuBarAndDock,
};

struct AspectRatios
{
	DECLARE_SERIALIZE_NO_PPTR (AspectRatios)

	bool	m_Ratios[kAspectCount];

	AspectRatios () { Reset (); }

	void Reset()
	{
		for (int i = 0; i < kAspectCount; ++i)
			m_Ratios[i] = true;
	}
};

class PlayerSettings : public GlobalGameManager
{
public:

	DECLARE_OBJECT_SERIALIZE (PlayerSettings)
	REGISTER_DERIVED_CLASS (PlayerSettings, GlobalGameManager)
	PlayerSettings (MemLabelId label, ObjectCreationMode mode);
	// ~PlayerSettings (); declared-by-macro

	virtual void CheckConsistency ();
	void AwakeFromLoad (AwakeFromLoadMode mode);

	bool DoesSupportResolution (int width, int height) const;
	void InitDefaultCursors ();

	static void InitializeClass ();
	static void PostInitializeClass ();
	static void CleanupClass () {  }

public:

	UnityStr    companyName;
	UnityStr    productName;
	UnityStr    version;

	PPtr<Texture2D> defaultCursor;
	Vector2f cursorHotspot;

	bool		androidProfiler;
	int			defaultScreenOrientation; ///< enum { Portrait = 0, Portrait Upside Down = 1, Landscape Right = 2, Landscape Left = 3, Auto Rotation = 4 }
	int			targetDevice; ///< enum { iPhone Only = 0, iPad Only = 1, iPhone + iPad = 2 }
	int 		targetGlesGraphics; ///< enum { OpenGL ES 2.0 = 1, OpenGL ES 3.0 = 2 }
	int         targetResolution; ///< enum { Native (default device resolution) = 0, Auto (Best Performance) = 3, Auto (Best Quality) = 4, 320p (iPhone) = 5, 640p (iPhone Retina Display) = 6, 768p (iPad) = 7 }
	int			accelerometerFrequency; ///< enum { Disabled = 0, 15 Hz = 15, 30 Hz = 30, 60 Hz = 60, 100 Hz = 100 }

	// General
	int         defaultScreenWidth;
	int         defaultScreenHeight;

	int         defaultWebScreenWidth;
	int         defaultWebScreenHeight;

	int         displayResolutionDialog;///< enum { Disabled = 0, Enabled = 1, Hidden By Default = 2 }
	AspectRatios	m_SupportedAspectRatios;

	int		m_RenderingPath; 		///< enum { Vertex Lit=0, Forward=1, Deferred Lighting=2 } Rendering path to use.
	int 	m_MobileRenderingPath;	///< enum { Vertex Lit=0, Forward=1, Deferred Lighting=2 } Rendering path to use on Mobiles.
	int 	m_ActiveColorSpace; 	///< enum { Gamma, Linear }
	bool 	m_MTRendering;
	bool	m_MobileMTRendering;
	bool 	m_UseDX11;

	// TODO: unify probably with ios, though we will ne handle it specifically with EnumPopup or something on editor side
	// TODO: if someone from editor team wants it - go ahead
	// values are taken from android.R.attr docs
	int			androidShowActivityIndicatorOnLoading; ///< enum { Don't Show=-1, Large=0, Inversed Large=1, Small=2, Inversed Small=3 }
	int			iosShowActivityIndicatorOnLoading; ///< enum { Don't Show=-1, White Large=0, White=1, Gray=2 }

	// auto-rotation handling. Expose bools on editor side
	bool		uiAutoRotateToPortrait;
	bool		uiAutoRotateToPortraitUpsideDown;
	bool		uiAutoRotateToLandscapeRight;
	bool		uiAutoRotateToLandscapeLeft;
	bool		uiUseAnimatedAutoRotation;

	bool		uiUse32BitDisplayBuffer;
	bool		uiUse24BitDepthBuffer;

	bool        defaultIsFullScreen;
	bool        defaultIsNativeResolution;
	bool		runInBackground;
	bool		captureSingleScreen;

	bool        overrideIPodMusic; ///< Force mixing behavior allowing iPod audio to play with FMOD even if the audio session doesn't usually permit this.
	bool		prepareIOSForRecording; ///< If enabled, the AudioSession of iOS will be initialized in recording mode, thus avoiding delays when initializing the Microphone object.

	bool		enableHWStatistics;

	bool		usePlayerLog;
	bool		stripPhysics;
	bool		useMacAppStoreValidation;
	int			macFullscreenMode; ///< enum {CaptureDisplay=0, FullscreenWindow=1, FullscreenWindowWithMenuBarAndDock=2}
	bool        forceSingleInstance;
	bool		resizableWindow;
	bool		gpuSkinning;
	bool		xboxPIXTextureCapture; // Xbox360
	bool        xboxEnableAvatar; // Xbox360
	bool		xboxEnableKinect; // Xbox360 Kinect
	bool		xboxEnableKinectAutoTracking; // Xbox360 Kinect
	UInt32		xboxSpeechDB;
	bool		xboxEnableFitness;
	bool		xboxEnableHeadOrientation;
	bool		xboxEnableGuest;

	int			wiiHio2Usage; ///< enum { None=-1, Profiler=0, Automation=1 } Wii Hio2 Usage.
	int			wiiLoadingScreenRectPlacement; ///< enum {TopLeft = 0, TopCenter = 1, TopRight = 2, MiddleLeft = 3, MiddleCenter = 4, MiddlerRight = 5, BottomLeft = 6, BottomCenter = 7, BottomRight = 8} Placement.
	ColorRGBAf  wiiLoadingScreenBackground;
	int			wiiLoadingScreenPeriod;
	UnityStr    wiiLoadingScreenFileName;
	Rectf		wiiLoadingScreenRect;
	int         firstStreamedLevelWithResources;

	UnityStr    absoluteURL;// Web player
	UnityStr    srcValue;// Web player

	UnityStr    iPhoneBundleIdentifier;

	UnityStr    AndroidLicensePublicKey;

	#if UNITY_EDITOR
	EditorOnlyPlayerSettings              m_EditorOnly;
	EditorOnlyPlayerSettingsNotPersistent m_EditorOnlyNotPersistent;

	////////@TODO: Move this into a seperate manager!
	int         unityRebuildLibraryVersion;
	int         unityForwardCompatibleVersion;
	int         unityStandardAssetsVersion;
	#endif

public:

	std::string GetCompanyName () const { return companyName; }
	void SetCompanyName (std::string value) { companyName = value; SetDirty(); }

	std::string GetProductName () const { return productName; }
	void SetProductName (std::string value) { productName = value; SetDirty();}

	int GetTargetDevice () const { return targetDevice; }
	void SetTargetDevice (int value) { targetDevice = value; SetDirty();}

	PPtr<Texture2D> GetDefaultCursor () const { return defaultCursor; }
	void SetDefaultCursor (PPtr<Texture2D> value) { defaultCursor = value; SetDirty();}

	Vector2f GetCursorHotspot () const { return cursorHotspot; }
	void SetCursorHotspot (Vector2f value) { cursorHotspot = value; SetDirty();}

	int GetTargetGlesGraphics () const { return targetGlesGraphics; }
	void SetTargetGlesGraphics (int value) { targetGlesGraphics = value; SetDirty(); }

	int GetTargetResolution () const { return targetResolution; }
	void SetTargetResolution (int value) { targetResolution = value; SetDirty();}

	int GetAccelerometerFrequency () const { return accelerometerFrequency; }
	void SetAccelerometerFrequency (int value) { accelerometerFrequency = value; SetDirty();}

	int GetDefaultScreenOrientation () const { return defaultScreenOrientation; }
	void SetDefaultScreenOrientation (int value) { defaultScreenOrientation = value; SetDirty();}

	bool GetUseAnimatedAutoRotation () const { return uiUseAnimatedAutoRotation; }
	void SetUseAnimatedAutoRotation (bool value) { uiUseAnimatedAutoRotation = value; SetDirty();}

	bool GetAutoRotationAllowed(int orientation) const;
	void SetAutoRotationAllowed(int orientation, bool enabled);

	bool GetUse32BitDisplayBuffer() const { return uiUse32BitDisplayBuffer; }
	void SetUse32BitDisplayBuffer(bool use) { uiUse32BitDisplayBuffer = use; }

	bool GetUse24BitDepthBuffer() const { return uiUse24BitDepthBuffer; }
	void SetUse24BitDepthBuffer(bool use) { uiUse24BitDepthBuffer = use; }

	int GetIOSShowActivityIndicatorOnLoading() const { return iosShowActivityIndicatorOnLoading; }
	void SetIOSShowActivityIndicatorOnLoading(int mode) { iosShowActivityIndicatorOnLoading = mode; SetDirty();}
	int GetAndroidShowActivityIndicatorOnLoading() const { return androidShowActivityIndicatorOnLoading; }
	void SetAndroidShowActivityIndicatorOnLoading(int mode) { androidShowActivityIndicatorOnLoading = mode; SetDirty();}

	int GetDefaultScreenWidth () const { return defaultScreenWidth; }
	void SetDefaultScreenWidth (int value) { defaultScreenWidth = value; SetDirty();}

	int GetDefaultScreenHeight () const { return defaultScreenHeight; }
	void SetDefaultScreenHeight (int value) { defaultScreenHeight = value; SetDirty();}

	int GetDefaultWebScreenWidth () const { return defaultWebScreenWidth; }
	void SetDefaultWebScreenWidth (int value) { defaultWebScreenWidth = value; SetDirty();}

	int GetDefaultWebScreenHeight () const { return defaultWebScreenHeight; }
	void SetDefaultWebScreenHeight (int value) { this->defaultWebScreenHeight = value; SetDirty();}

	int GetDisplayResolutionDialog () const { return displayResolutionDialog; }
	void SetDisplayResolutionDialog (int value) { this->displayResolutionDialog = value; SetDirty();}

	bool AspectRatioEnabled (int aspectRatio) const { return m_SupportedAspectRatios.m_Ratios [aspectRatio]; }
	void SetAspectRatio (int aspectRatio, bool enabled) { m_SupportedAspectRatios.m_Ratios[aspectRatio] = enabled; SetDirty();}

	bool GetDefaultIsFullScreen () const { return defaultIsFullScreen; }
	void SetDefaultIsFullScreen (bool value) { defaultIsFullScreen = value; SetDirty(); }

	bool GetDefaultIsNativeResolution () const { return defaultIsNativeResolution; }
	void SetDefaultIsNativeResolution (bool value) { defaultIsNativeResolution = value; SetDirty(); }

	bool GetRunInBackground () const { return runInBackground; }
	void SetRunInBackground (bool value) { runInBackground = value; SetDirty(); }

	bool GetStripPhysics () const { return stripPhysics; }
	void SetStripPhysics (bool value) { stripPhysics = value; SetDirty(); }

	bool GetUsePlayerLog () const { return usePlayerLog; }
	void SetUsePlayerLog (bool value) { usePlayerLog = value; SetDirty(); }

	bool GetEnableHWStatistics () const { return enableHWStatistics; }
	void SetEnableHWStatistics (bool value) { enableHWStatistics = value; SetDirty(); }

	bool GetUseMacAppStoreValidation () const { return useMacAppStoreValidation; }
	void SetUseMacAppStoreValidation (bool value) { useMacAppStoreValidation = value; SetDirty(); }

	int GetMacFullscreenMode () const { return macFullscreenMode; }
	void SetMacFullscreenMode (int value) { macFullscreenMode = value; SetDirty(); }

	bool GetForceSingleInstance () const { return forceSingleInstance; }
	void SetForceSingleInstance (bool value) { forceSingleInstance = value; SetDirty(); }

	bool GetResizableWindow () const { return resizableWindow; }
	void SetResizableWindow (bool value) { resizableWindow = value; SetDirty(); }

	bool GetGPUSkinning() const { return gpuSkinning; }
	void SetGPUSkinning(bool value) { gpuSkinning = value; SetDirty(); }

	bool GetXboxPIXTextureCapture() const { return xboxPIXTextureCapture; }
	void SetXboxPIXTextureCapture(bool value) { xboxPIXTextureCapture = value; SetDirty(); }

	bool GetXboxEnableAvatar() const { return xboxEnableAvatar; }
	void SetXboxEnableAvatar(bool value) { xboxEnableAvatar = value; SetDirty(); }
	bool GetXboxEnableGuest() const { return xboxEnableGuest; }
	void SetXboxEnableGuest(bool value) { xboxEnableGuest = value; SetDirty(); }

	bool GetXboxEnableKinect() const { return xboxEnableKinect; }
	void SetXboxEnableKinect(bool value) { xboxEnableKinect = value; SetDirty(); }
	bool GetXboxEnableKinectAutoTracking() const { return xboxEnableKinectAutoTracking; }
	void SetXboxEnableKinectAutoTracking(bool value) { xboxEnableKinectAutoTracking = value; SetDirty(); }
	UInt32 GetXboxSpeechDB() const { return xboxSpeechDB; }
	void SetXboxSpeechDB(UInt32 value) { xboxSpeechDB = value; SetDirty(); }
	bool GetXboxEnableFitness() const { return xboxEnableFitness; }
	void SetXboxEnableFitness(bool value) { xboxEnableFitness = value; SetDirty(); }
	bool GetXboxHeadOrientation() const { return xboxEnableHeadOrientation; }
	void SetXboxHeadOrientation(bool value) { xboxEnableHeadOrientation = value; SetDirty(); }

	bool GetCaptureSingleScreen () const { return captureSingleScreen; }
	void SetCaptureSingleScreen (bool value) { captureSingleScreen = value; SetDirty(); }

	int GetFirstStreamedLevelWithResources () const { return firstStreamedLevelWithResources; }
	void SetFirstStreamedLevelWithResources (int value) { this->firstStreamedLevelWithResources = value; SetDirty(); }

	void SetRenderingPath (RenderingPath rp) { m_RenderingPath = rp; SetDirty(); }
	RenderingPath GetRenderingPath() const { return static_cast<RenderingPath>(m_RenderingPath); }

	void SetMobileRenderingPath (RenderingPath rp) { m_MobileRenderingPath = rp; SetDirty(); }
	RenderingPath GetMobileRenderingPath() const { return static_cast<RenderingPath>(m_MobileRenderingPath); }

	RenderingPath	GetRenderingPathRuntime();
	void			SetRenderingPathRuntime(RenderingPath rp);

	void SetDesiredColorSpace (ColorSpace colorSpace) { m_ActiveColorSpace = colorSpace; SetDirty();}
	ColorSpace GetDesiredColorSpace () const { return static_cast<ColorSpace> (m_ActiveColorSpace); }
	ColorSpace GetValidatedColorSpace () const ;

	void SetMTRendering (bool mtRendering) { m_MTRendering = mtRendering; SetDirty(); }
	bool GetMTRendering () const { return m_MTRendering; }

	void SetMobileMTRendering (bool mtRendering) { m_MobileMTRendering = mtRendering; SetDirty(); }
	bool GetMobileMTRendering () const { return m_MobileMTRendering; }

	bool GetMTRenderingRuntime();
	void SetMTRenderingRuntime(bool mtRendering);


	void SetUseDX11 (bool v) { m_UseDX11 = v; SetDirty(); }
	bool GetUseDX11 () const { return m_UseDX11; }

	std::string GetiPhoneBundleIdentifier () const { return iPhoneBundleIdentifier; }
	void SetiPhoneBundleIdentifier (const std::string& value) { iPhoneBundleIdentifier = value; SetDirty();}

	#if UNITY_EDITOR

	int GetAPICompatibilityLevel () const { return m_EditorOnly.apiCompatibilityLevel; }
	void SetAPICompatibilityLevel (int value) { m_EditorOnly.apiCompatibilityLevel = value; SetDirty();}

	bool GetStripUnusedMeshComponents() const { return m_EditorOnly.stripUnusedMeshComponents; }
	void SetStripUnusedMeshComponents (bool v) { m_EditorOnly.stripUnusedMeshComponents = v; SetDirty(); }

	std::string GetAotOptions () const { return m_EditorOnly.aotOptions; }
	void SetAotOptions (std::string value) { m_EditorOnly.aotOptions = value; SetDirty();}

	std::string GetAndroidKeystoreName () const { return m_EditorOnly.AndroidKeystoreName; }
	void SetAndroidKeystoreName (std::string value) { m_EditorOnly.AndroidKeystoreName = value; SetDirty();}

	std::string GetAndroidKeystorePass () const { return m_EditorOnlyNotPersistent.AndroidKeystorePass; }
	void SetAndroidKeystorePass (std::string value) { m_EditorOnlyNotPersistent.AndroidKeystorePass = value; SetDirty();}

	std::string GetAndroidKeyaliasName () const { return m_EditorOnly.AndroidKeyaliasName; }
	void SetAndroidKeyaliasName (std::string value) { m_EditorOnly.AndroidKeyaliasName = value; SetDirty();}

	std::string GetAndroidKeyaliasPass () const { return m_EditorOnlyNotPersistent.AndroidKeyaliasPass; }
	void SetAndroidKeyaliasPass (std::string value) { m_EditorOnlyNotPersistent.AndroidKeyaliasPass = value; SetDirty();}

	bool GetAndroidLicenseVerification () const { return !AndroidLicensePublicKey.empty(); }

	int GetUseAPKExpansionFiles () const { return m_EditorOnly.enableAndroidExpansionFiles; }
	void SetUseAPKExpansionFiles (int value) { m_EditorOnly.enableAndroidExpansionFiles = value; SetDirty();}

	std::string GetiPhoneBundleVersion () const { return m_EditorOnly.iPhoneBundleVersion; }
	void SetiPhoneBundleVersion (std::string value) { m_EditorOnly.iPhoneBundleVersion = value; SetDirty();}

	int GetiPhoneStrippingLevel () const { return m_EditorOnly.iPhoneStrippingLevel; }
	void SetiPhoneStrippingLevel (int value) { m_EditorOnly.iPhoneStrippingLevel = value; SetDirty();}

	int GetAndroidBundleVersionCode () const { return m_EditorOnly.AndroidBundleVersionCode; }
	void SetAndroidBundleVersionCode (int value) { m_EditorOnly.AndroidBundleVersionCode = value; SetDirty();}

	int GetAndroidMinSdkVersion () const { return m_EditorOnly.AndroidMinSdkVersion; }
	void SetAndroidMinSdkVersion (int value) { m_EditorOnly.AndroidMinSdkVersion = value; SetDirty();}

	int GetAndroidPreferredInstallLocation () const { return m_EditorOnly.AndroidPreferredInstallLocation; }
	void SetAndroidPreferredInstallLocation (int value) { m_EditorOnly.AndroidPreferredInstallLocation = value; SetDirty();}

	int GetForceAndroidInternetPermission () const { return m_EditorOnly.forceAndroidInternetPermission; }
	void SetForceAndroidInternetPermission (int value) { m_EditorOnly.forceAndroidInternetPermission = value; SetDirty();}

	int GetForceAndroidSDCardPermission () const { return m_EditorOnly.forceAndroidSDCardPermission; }
	void SetForceAndroidSDCardPermission (int value) { m_EditorOnly.forceAndroidSDCardPermission = value; SetDirty();}

	int GetCreateAndroidWallpaper () const { return m_EditorOnly.createAndroidWallpaper; }
	void SetCreateAndroidWallpaper (int value) { m_EditorOnly.createAndroidWallpaper = value; SetDirty();}

	int GetAndroidTargetDevice () const { return m_EditorOnly.AndroidTargetDevice; }
	void SetAndroidTargetDevice (int value) { m_EditorOnly.AndroidTargetDevice = value; SetDirty();}

	int GetAndroidSplashScreenScale () const { return m_EditorOnly.AndroidSplashScreenScale; }
	void SetAndroidSplashScreenScale (int value) { m_EditorOnly.AndroidSplashScreenScale = value; SetDirty();}

	std::string GetBlackBerryDeviceAddress () { return m_EditorOnly.blackberryDeviceAddress; }
	void SetBlackBerryDeviceAddress (std::string value) { m_EditorOnly.blackberryDeviceAddress = value; SetDirty();}

	std::string GetBlackBerryDevicePassword () { return m_EditorOnly.blackberryDevicePassword; }
	void SetBlackBerryDevicePassword (std::string value) { m_EditorOnly.blackberryDevicePassword = value; SetDirty();}

    std::string GetTizenProductDescription () { return m_EditorOnly.tizenProductDescription; }
	void SetTizenProductDescription (std::string value) { m_EditorOnly.tizenProductDescription = value; SetDirty();}

    std::string GetTizenProductURL () { return m_EditorOnly.tizenProductURL; }
	void SetTizenProductURL (std::string value) { m_EditorOnly.tizenProductURL = value; SetDirty();}

    std::string GetTizenCertificatePath () { return m_EditorOnly.tizenCertificatePath; }
	void SetTizenCertificatePath (std::string value) { m_EditorOnly.tizenCertificatePath = value; SetDirty();}

    std::string GetTizenCertificatePassword () { return m_EditorOnly.tizenCertificatePassword; }
	void SetTizenCertificatePassword (std::string value) { m_EditorOnly.tizenCertificatePassword = value; SetDirty();}

	int GetiPhoneScriptCallOptimization () const { return m_EditorOnly.iPhoneScriptCallOptimization; }
	void SetiPhoneScriptCallOptimization (int value) { m_EditorOnly.iPhoneScriptCallOptimization = value; SetDirty();}

	int GetiPhoneSdkVersion () const { return m_EditorOnly.iPhoneSdkVersion; }
	void SetiPhoneSdkVersion (int value) { m_EditorOnly.iPhoneSdkVersion = value; SetDirty();}

	int GetiPhoneTargetOSVersion () const { return m_EditorOnly.iPhoneTargetOSVersion; }
	void SetiPhoneTargetOSVersion (int value) { m_EditorOnly.iPhoneTargetOSVersion = value; SetDirty();}

	int GetUIPrerenderedIcon () const { return m_EditorOnly.uIPrerenderedIcon; }
	void SetUIPrerenderedIcon (int value) { m_EditorOnly.uIPrerenderedIcon = value; SetDirty();}

	int GetUIRequiresPersistentWiFi () const { return m_EditorOnly.uIRequiresPersistentWiFi; }
	void SetUIRequiresPersistentWiFi (int value) { m_EditorOnly.uIRequiresPersistentWiFi = value; SetDirty();}

	int GetUIStatusBarHidden () const { return m_EditorOnly.uIStatusBarHidden; }
	void SetUIStatusBarHidden (int value) { m_EditorOnly.uIStatusBarHidden = value; SetDirty();}

	int GetUIStatusBarStyle () const { return m_EditorOnly.uIStatusBarStyle; }
	void SetUIStatusBarStyle (int value) { m_EditorOnly.uIStatusBarStyle = value; SetDirty();}

	int GetUIExitOnSuspend () const { return m_EditorOnly.uIExitOnSuspend; }
	void SetUIExitOnSuspend (int value) { m_EditorOnly.uIExitOnSuspend = value; SetDirty();}

	PPtr <Texture2D> GetResolutionDialogBanner () const { return m_EditorOnly.resolutionDialogBanner; }
	void SetResolutionDialogBanner (PPtr <Texture2D> value) { m_EditorOnly.resolutionDialogBanner = value; SetDirty(); }

	std::vector<int> GetPlatformIconSizes (const std::string& platform) { return m_EditorOnly.GetPlatformIconSizes(platform); }
	std::vector<PPtr<Texture2D> > GetPlatformIcons (const std::string& platform) { return m_EditorOnly.GetPlatformIcons(platform); }
	void SetPlatformIcons(const std::string& platform, std::vector<PPtr<Texture2D> > icons) { if (m_EditorOnly.SetPlatformIcons(platform, icons)) SetDirty(); }

	Texture2D* GetPlatformIconForSize (const std::string& platform, int size) { return m_EditorOnly.GetPlatformIconForSize(platform, size); }

	void GetPlatformBatching (BuildTargetPlatform platform, bool* outStaticBatching, bool* outDynamicBatching) const { m_EditorOnly.GetPlatformBatching(platform, outStaticBatching, outDynamicBatching); }
	void SetPlatformBatching (BuildTargetPlatform platform, bool staticBatching, bool dynamicBatching) { m_EditorOnly.SetPlatformBatching(platform, staticBatching, dynamicBatching); SetDirty(); }

	void SetWebPlayerTemplate (UnityStr value) { if (!m_EditorOnly.m_WebPlayerTemplate.compare(value)) { m_EditorOnly.m_WebPlayerTemplate = value; SetDirty (); } };
	void SetTemplateCustomKeys (std::vector<std::string> keys) { m_EditorOnly.SetTemplateCustomKeys(keys); SetDirty(); }
	void SetTemplateCustomValue (std::string key, std::string value)  { if (m_EditorOnly.SetTemplateCustomValue(key, value)) SetDirty(); }

	std::vector<std::string> GetTemplateCustomKeys () { return m_EditorOnly.GetTemplateCustomKeys(); }
	std::string GetWebPlayerTemplate () const { return m_EditorOnly.m_WebPlayerTemplate; }
	std::string GetTemplateCustomValue (std::string key)  { return m_EditorOnly.GetTemplateCustomValue(key); }

	const EditorOnlyPlayerSettings& GetEditorOnly () const { return m_EditorOnly; }

	///Always marks as dirty
	EditorOnlyPlayerSettings& GetEditorOnlyForUpdate () { SetDirty(); return m_EditorOnly; }
	EditorOnlyPlayerSettingsNotPersistent& GetEditorOnlyNotPersistent () { return m_EditorOnlyNotPersistent; }

private:
	std::vector<EditorOnlyPlayerSettings::IconWithSize> GetBestIconWithSizes (const std::string& platform);

	#endif
};

PlayerSettings& GetPlayerSettings ();
PlayerSettings* GetPlayerSettingsPtr ();

#endif
