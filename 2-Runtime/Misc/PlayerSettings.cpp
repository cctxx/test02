#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "PlayerSettings.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/BaseClasses/Cursor.h"

#if UNITY_EDITOR
#include "Editor/Src/ColorSpaceLiveSwitch.h"
#endif


// ----------------------------------------------------------------------


// ----------------------------------------------------------------------

static const char* kAspectRatioSerializeNames[kAspectCount] = {
	"Others",
	"4:3",
	"5:4",
	"16:10",
	"16:9",
};

static float kAspectRatioValues[kAspectCount] = {
	0.0f,
	4.0f/3.0f,
	5.0f/4.0f,
	16.0f/10.0f,
	16.0f/9.0f,
};

static bool DoesMatchAspectRatio (int width, int height, ResolutionAspect aspect)
{
	AssertIf (aspect == kAspectOthers);
	if (width == 0 || height == 0)
		return false;
	float val = float(width) / float(height);
	return (Abs (val - kAspectRatioValues[aspect]) < 0.05f);
}


template<class TransferFunc>
void AspectRatios::Transfer (TransferFunc& transfer)
{
	// Others is at index 0 (so we can easily add more ratios later),
	// but we want to display it last in the UI. So serialize all first, then
	// "Others" one
	for (int i = 1; i < kAspectCount; ++i)
	{
		transfer.Transfer (m_Ratios[i], kAspectRatioSerializeNames[i]);
	}
	transfer.Transfer (m_Ratios[0], kAspectRatioSerializeNames[0]);
	transfer.Align ();
}


bool PlayerSettings::DoesSupportResolution (int width, int height) const
{
	// check if matches any enabled ratio except "Others"
	bool didSupportAnyRatio = false;
	for (int i = 1; i < kAspectCount; ++i)
	{
		if (DoesMatchAspectRatio (width, height, static_cast<ResolutionAspect>(i)))
		{
			didSupportAnyRatio = true;
			if (m_SupportedAspectRatios.m_Ratios[i])
				return true;
		}
	}
	// if "Others" is enabled and we did not support any of standard aspects -
	// then we support this weird aspect
	if (!didSupportAnyRatio && m_SupportedAspectRatios.m_Ratios[kAspectOthers])
		return true;

	return false;
}


// ----------------------------------------------------------------------

PlayerSettings::PlayerSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_RenderingPath(kRenderPathForward)
,	m_MobileRenderingPath(kRenderPathForward)
,	m_ActiveColorSpace(kGammaColorSpace)
,	m_MTRendering(true)
,	m_MobileMTRendering(false)
,	m_UseDX11(false)
{
	#if UNITY_EDITOR
	companyName = GetDefaultCompanyName ();
	productName = GetDefaultProductName ();
	unityRebuildLibraryVersion = 0;
	unityForwardCompatibleVersion = UNITY_FORWARD_COMPATIBLE_VERSION;
	unityStandardAssetsVersion = 0;
	m_EditorOnlyNotPersistent.CurrentColorSpace = (ColorSpace) m_ActiveColorSpace;
	#endif

	iPhoneBundleIdentifier = "com.Company.ProductName";

	defaultIsFullScreen = true;
	defaultIsNativeResolution = true;
	displayResolutionDialog = kShowResolutionDialog;
	defaultScreenWidth = 1024;
	defaultScreenHeight = 768;
	defaultWebScreenWidth = 960;
	defaultWebScreenHeight = 600;
	firstStreamedLevelWithResources = 0;
	androidProfiler = false;

	defaultScreenOrientation = 0;
	uiAutoRotateToPortrait = uiAutoRotateToPortraitUpsideDown = uiAutoRotateToLandscapeLeft = uiAutoRotateToLandscapeRight = true;
	uiUseAnimatedAutoRotation = true;
	uiUse32BitDisplayBuffer = true;
	uiUse24BitDepthBuffer = true;
	iosShowActivityIndicatorOnLoading = -1;
	androidShowActivityIndicatorOnLoading = -1;

	runInBackground = false;
	captureSingleScreen = false;
	targetDevice = 2;
	targetGlesGraphics = 1;
	targetResolution = 0;
	accelerometerFrequency = 60;
	overrideIPodMusic =  false;
	prepareIOSForRecording = false;
	enableHWStatistics = true;
	usePlayerLog = true;
	stripPhysics = false;
	forceSingleInstance = false;
	resizableWindow = false;
	useMacAppStoreValidation = false;
	macFullscreenMode = kFullscreenWindowWithMenuBarAndDock;
	gpuSkinning = false;
	xboxPIXTextureCapture = false;
	xboxEnableAvatar = false;
	xboxEnableKinect = false;
	xboxEnableKinectAutoTracking = false;
	xboxSpeechDB = 0;
	xboxEnableFitness = false;
	xboxEnableHeadOrientation = false;
	xboxEnableGuest = false;

	wiiHio2Usage = -1;
	wiiLoadingScreenRectPlacement = 0;
	wiiLoadingScreenBackground = ColorRGBAf(1.0f, 1.0f, 1.0f, 1.0f);
	wiiLoadingScreenPeriod = 1000;
	wiiLoadingScreenFileName = "";
	wiiLoadingScreenRect = Rectf(0.0f, 0.0f, 0.0f, 0.0f);
}

void PlayerSettings::InitializeClass()
{
	// 2.x -> 3.0 compatibility
	RegisterAllowNameConversion("PlayerSettings", "defaultWebScreenWidth", "defaultScreenWidthWeb");
	RegisterAllowNameConversion("PlayerSettings", "defaultWebScreenHeight", "defaultScreenHeightWeb");
}

void PlayerSettings::PostInitializeClass ()
{
	// This will help reset the cursors during a dynamic reload.
	// For most platforms, this will happen only during initialization,
	// the manager pointer will be null and this will no-op.
	if (GetPlayerSettingsPtr ())
		GetPlayerSettings ().InitDefaultCursors();
}


PlayerSettings::~PlayerSettings ()
{
}

void PlayerSettings::CheckConsistency ()
{
	Super::CheckConsistency ();

	m_RenderingPath = clamp (m_RenderingPath, 0, kRenderPathCount-1);
	m_ActiveColorSpace = clamp (m_ActiveColorSpace, 0, kMaxColorSpace-1);
}


bool PlayerSettings::GetAutoRotationAllowed(int orientation) const
{
	switch(orientation)
	{
		case 0:	return uiAutoRotateToPortrait;
		case 1: return uiAutoRotateToPortraitUpsideDown;
		case 2: return uiAutoRotateToLandscapeRight;
		case 3: return uiAutoRotateToLandscapeLeft;

		default:
			ErrorString("orientation out of range");
			return false;
	}
}

void PlayerSettings::SetAutoRotationAllowed(int orientation, bool enabled)
{
	switch(orientation)
	{
		case 0:	uiAutoRotateToPortrait = enabled;			break;
		case 1: uiAutoRotateToPortraitUpsideDown = enabled;	break;
		case 2: uiAutoRotateToLandscapeRight = enabled;		break;
		case 3: uiAutoRotateToLandscapeLeft = enabled;		break;

		default:
			ErrorString("orientation out of range, ignoring");
			break;
	}

	bool somethingAllowed =    uiAutoRotateToPortrait
							|| uiAutoRotateToPortraitUpsideDown
							|| uiAutoRotateToLandscapeRight
							|| uiAutoRotateToLandscapeLeft;

	if( defaultScreenOrientation == 4 && !somethingAllowed )
	{
		ErrorString("all orientations are disabled for auto-rotation. Enabling Portrait");
		uiAutoRotateToPortrait = true;
	}
}

void PlayerSettings::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	#if UNITY_EDITOR
	// product name and identifier can't be empty strings
	if (companyName.empty ())
		companyName = GetDefaultCompanyName ();
	if (productName.empty () || productName == "UnityPlayer")
		productName = GetDefaultProductName ();

	// Perform color space conversion / cursor init only when applying a setting in the inspector.
	if (awakeMode == kDefaultAwakeFromLoad && GetPlayerSettingsPtr() == this)
	{
		//only perform color space conversion if it was actually changed.
		if ( GetEditorOnlyNotPersistent().CurrentColorSpace != GetValidatedColorSpace() ) {
			ColorSpaceLiveSwitch();
			GetEditorOnlyNotPersistent().CurrentColorSpace = GetValidatedColorSpace();
		}
		InitDefaultCursors ();
	}
	#else
	// Try to set the default cursor.  We can only do so if we actually have a GFX device
	// which is not the case when we are currently still starting up (the player settings
	// are read before the graphics system is initialized).
	if((awakeMode == kDidLoadThreaded || awakeMode == kDidLoadFromDisk) && IsGfxDevice ())
	{
		InitDefaultCursors();
	}
	#endif
}

void PlayerSettings::InitDefaultCursors ()
{
	Assert (IsGfxDevice ());
	Cursors::InitializeCursors (defaultCursor, cursorHotspot);
}

template<class TransferFunction>
void PlayerSettings::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);

	transfer.Transfer (androidProfiler, "AndroidProfiler");
	transfer.Align();

	transfer.Transfer (defaultScreenOrientation, "defaultScreenOrientation");

	transfer.Transfer (targetDevice, "targetDevice");
	transfer.Transfer (targetGlesGraphics, "targetGlesGraphics");

	transfer.Transfer (targetResolution, "targetResolution");
	if(targetResolution == 1)
		targetResolution = 6; // SD -> x0.5
	else if(targetResolution == 2)
		targetResolution = 0; // HD -> Native

	transfer.Transfer (accelerometerFrequency, "accelerometerFrequency");

	transfer.Align();

	transfer.Transfer (companyName, "companyName");
	transfer.Transfer (productName, "productName");

	transfer.Transfer (defaultCursor, "defaultCursor");
	transfer.Transfer (cursorHotspot, "cursorHotspot");

	transfer.Transfer (defaultScreenWidth, "defaultScreenWidth");
	transfer.Transfer (defaultScreenHeight, "defaultScreenHeight");
	transfer.Transfer (defaultWebScreenWidth, "defaultScreenWidthWeb");
	transfer.Transfer (defaultWebScreenHeight, "defaultScreenHeightWeb");
	TRANSFER (m_RenderingPath);
	TRANSFER (m_MobileRenderingPath);
	TRANSFER (m_ActiveColorSpace);
	TRANSFER (m_MTRendering);
	TRANSFER (m_MobileMTRendering);
	TRANSFER (m_UseDX11);

	transfer.Align();

	transfer.Transfer (iosShowActivityIndicatorOnLoading, "iosShowActivityIndicatorOnLoading");
	transfer.Transfer (androidShowActivityIndicatorOnLoading, "androidShowActivityIndicatorOnLoading");

	transfer.Transfer (displayResolutionDialog, "displayResolutionDialog");

	transfer.Transfer (uiAutoRotateToPortrait, "allowedAutorotateToPortrait");
	transfer.Transfer (uiAutoRotateToPortraitUpsideDown, "allowedAutorotateToPortraitUpsideDown");
	transfer.Transfer (uiAutoRotateToLandscapeRight, "allowedAutorotateToLandscapeRight");
	transfer.Transfer (uiAutoRotateToLandscapeLeft, "allowedAutorotateToLandscapeLeft");
	transfer.Transfer (uiUseAnimatedAutoRotation, "useOSAutorotation");
	transfer.Transfer (uiUse32BitDisplayBuffer, "use32BitDisplayBuffer");
	transfer.Transfer (uiUse24BitDepthBuffer, "use24BitDepthBuffer");
	transfer.Align();

	transfer.Transfer (defaultIsFullScreen, "defaultIsFullScreen");
	transfer.Transfer (defaultIsNativeResolution, "defaultIsNativeResolution");
	transfer.Transfer (runInBackground, "runInBackground");
	transfer.Transfer (captureSingleScreen, "captureSingleScreen");

	transfer.Transfer (overrideIPodMusic, "Override IPod Music");
	transfer.Transfer (prepareIOSForRecording, "Prepare IOS For Recording");
	transfer.Transfer (enableHWStatistics, "enableHWStatistics");
	TRANSFER (usePlayerLog);
	TRANSFER (stripPhysics);
#if ENABLE_SINGLE_INSTANCE_BUILD_SETTING
	TRANSFER (forceSingleInstance);
#endif
	TRANSFER (resizableWindow);
	TRANSFER (useMacAppStoreValidation);
	TRANSFER (gpuSkinning);
	TRANSFER (xboxPIXTextureCapture);
	TRANSFER (xboxEnableAvatar);
	TRANSFER (xboxEnableKinect);
	TRANSFER (xboxEnableKinectAutoTracking);
	TRANSFER (xboxEnableFitness);
	transfer.Align();
	TRANSFER (macFullscreenMode);
	TRANSFER (xboxSpeechDB);
	TRANSFER (xboxEnableHeadOrientation);
	TRANSFER (xboxEnableGuest);
	transfer.Align();

	TRANSFER (wiiHio2Usage);
	TRANSFER (wiiLoadingScreenRectPlacement);
	TRANSFER (wiiLoadingScreenBackground);
	TRANSFER (wiiLoadingScreenPeriod);
	TRANSFER (wiiLoadingScreenFileName);
	TRANSFER (wiiLoadingScreenRect);


	TRANSFER (m_SupportedAspectRatios);

	transfer.Transfer (iPhoneBundleIdentifier, "iPhoneBundleIdentifier");


#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease())
	{
		m_EditorOnly.Transfer(transfer);

		/////@TODO: GET RID OF THIS TOO

		transfer.Transfer (firstStreamedLevelWithResources, "firstStreamedLevelWithResources");
		transfer.Transfer (unityRebuildLibraryVersion, "unityRebuildLibraryVersion", kHideInEditorMask);
		transfer.Transfer (unityForwardCompatibleVersion, "unityForwardCompatibleVersion", kHideInEditorMask);
		transfer.Transfer (unityStandardAssetsVersion, "unityStandardAssetsVersion", kHideInEditorMask);
	}
#endif
}

ColorSpace PlayerSettings::GetValidatedColorSpace () const
{
	// On xBox, don't check GraphicsCaps, because we need to know the color space when setting up
	// front/back buffers, at which point GraphicsCaps are not initialized. We know that Xenon supports
	// sRGB.
	if (gGraphicsCaps.hasSRGBReadWrite || UNITY_XENON)
		return static_cast<ColorSpace>(m_ActiveColorSpace);
	else
		return kGammaColorSpace;
}

RenderingPath PlayerSettings::GetRenderingPathRuntime()
{
#if UNITY_ANDROID || UNITY_BB10 || UNITY_IPHONE || UNITY_TIZEN
	return GetMobileRenderingPath();
#else
	return GetRenderingPath();
#endif
}

void PlayerSettings::SetRenderingPathRuntime(RenderingPath rp)
{
#if UNITY_ANDROID || UNITY_BB10 || UNITY_IPHONE || UNITY_TIZEN
	SetMobileRenderingPath(rp);
#else
	SetRenderingPath(rp);
#endif
}

bool PlayerSettings::GetMTRenderingRuntime()
{
#if UNITY_ANDROID || UNITY_BB10 || UNITY_IPHONE || UNITY_TIZEN
	return GetMobileMTRendering();
#else
	return GetMTRendering();
#endif
}
void PlayerSettings::SetMTRenderingRuntime(bool mtRendering)
{
#if UNITY_ANDROID || UNITY_BB10 || UNITY_IPHONE || UNITY_TIZEN
	SetMobileMTRendering(mtRendering);
#else
	SetMTRendering(mtRendering);
#endif
}

IMPLEMENT_CLASS_HAS_POSTINIT (PlayerSettings)
IMPLEMENT_OBJECT_SERIALIZE (PlayerSettings)

GET_MANAGER (PlayerSettings)
GET_MANAGER_PTR (PlayerSettings)

