#include "UnityPrefix.h"
#include "DrawSplashScreenAndWatermarks.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Misc/ReproductionLog.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Misc/SystemInfo.h"

#if WEBPLUG
double gDisplayFullscreenEscapeTimeout = -1.0F;
#endif

static bool ConsumeVersionNumber(const char *&version, int &number)
{
	number = 0;

	for (int i = 0; i < 8; ++i)	// limit to 8 digits
	{
		char c = *version;

		if ((c < '0') || (c > '9'))
		{
			return (i > 0);
		}

		++version;

		number *= 10;
		number += (c - '0');
	}

	return false;
}

static bool ConsumeVersionSeparator(const char *&version)
{
	if ('.' == *version)
	{
		++version;
		return true;
	}

	return false;
}

bool IsContentBuiltWithBetaVersion()
{
	if (GetBuildSettingsPtr() == NULL)
		return false;

	const char* version = GetBuildSettings().GetVersion().c_str();

	int temp;

	if (ConsumeVersionNumber(version, temp) && ConsumeVersionSeparator(version))	// major
	{
		if (ConsumeVersionNumber(version, temp) && ConsumeVersionSeparator(version))	// minor
		{
			if (ConsumeVersionNumber(version, temp))	// fix
			{
				char c = *version;

				if (('d' == c) || ('D' == c) ||	// development
					('a' == c) || ('A' == c) ||	// alpha
					('b' == c) || ('B' == c))	// beta
				{
					return true;
				}
			}
		}
	}

	return false;
}

void DrawWaterMark( bool alwaysDisplay )
{
	Texture2D* watermark = GetBuiltinResource<Texture2D>("UnityWaterMark-small.png");
	if( !watermark )
		return;

#if !UNITY_EDITOR

	const float kSlideInDelay = 1.5f;
	const float kSlideInTime = 1.0f;
	const float kStayTime = 5.0f;
	const float kSlideOutTime = 1.5f;

	float t = GetTimeManager().GetRealtime();
	if( !alwaysDisplay && t > kSlideInDelay+kSlideInTime+kStayTime+kSlideOutTime )
		return;

	float a;
	if( t < kSlideInDelay+kSlideInTime )
		a = (t-kSlideInDelay) / kSlideInTime;
	else if( !alwaysDisplay )
		a = 1.0f - (t-(kSlideInDelay+kSlideInTime+kStayTime)) / kSlideOutTime;
	else
		a = 1.0f;

#else
	float a = 1.0f;
#endif

	float pos = SmoothStep( 0.0f, 128.0f, a );

	const Rectf& windowRect = GetRenderManager().GetWindowRect();
	DeviceMVPMatricesState preserveMVP;
	SetupPixelCorrectCoordinates();

	DrawGUITexture (Rectf (windowRect.width-pos, 62, 128, -58), watermark, ColorRGBAf(0.5f, 0.5f, 0.5f, 0.5f));
}

// (0, 0) is at the lower left corner. x increments to the left, y - upwards.
// specify -(x + 1) to align to the right and -(y + 1) to align to the top
static int DrawSimpleWatermark(const string &name, float x, float y, const ColorRGBAf& color)
{
	Texture2D* texture = GetBuiltinResource<Texture2D>(name);
	if (!texture)
		return 0;

	const Rectf& windowRect = GetRenderManager().GetWindowRect();
	DeviceMVPMatricesState preserveMVP;
	SetupPixelCorrectCoordinates();

	float width = texture->GetDataWidth();
	float height = -texture->GetDataHeight();

	if (x < 0)
	{
		x = -(x + 1);
		x = (windowRect.width - width - x);
	}

	if (y < 0)
	{
		y = -(y + 1);
		y = (windowRect.height + height - y);
	}

	y -= height;

	DrawGUITexture (Rectf (x, y, width, height), texture, color);

	return texture->GetDataHeight();
}

static int DrawSimpleWatermark(const string &name, float x, float y)
{
	return DrawSimpleWatermark(name, x, y, ColorRGBAf(0.5f, 0.5f, 0.5f, 0.5f));
}

static void DrawTrialWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWaterMark-trial.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}

static void DrawEducationalWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWaterMark-edu.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}

static void DrawPrototypingWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWaterMark-proto.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}

static void DrawDeveloperWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWaterMark-dev.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}

#if UNITY_FLASH
static void DrawDebugFlashPlayerWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWatermark-DebugFlashPlayer.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}
#endif

#if WEBPLUG


static void DrawContentBetaWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWaterMark-beta.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}

static void DrawPluginBetaWatermark (int& watermarkoffset)
{
	int height = DrawSimpleWatermark("UnityWaterMarkPlugin-beta.png", -(1 + 1), watermarkoffset);
	watermarkoffset += height + 3;
}

void ShowFullscreenEscapeWarning ()
{
	gDisplayFullscreenEscapeTimeout = GetTimeSinceStartup();
}

void RenderFullscreenEscapeWarning ()
{
	const float kTimeout = 6.0F;
	if (GetTimeSinceStartup() < gDisplayFullscreenEscapeTimeout + kTimeout && GetScreenManager().IsFullScreen())
	{
		float time = GetTimeSinceStartup() - gDisplayFullscreenEscapeTimeout;
		time = std::max(time, 0.0F);

		Texture2D* background = GetBuiltinResource<Texture2D>("EscToExit_back.png");
		Texture2D* text = GetBuiltinResource<Texture2D>("EscToExit_text.png");
		if( background && text )
		{
			float a = SmoothStep( 0.5f, 0, (time - 3.5f) * 0.5f );
			ColorRGBA32 color = ColorRGBAf(0.5f,0.5f,0.5f,a);
			const float w = 350;
			const float h = 71;
			const float yFac = 1.0f - 0.38197f;

			const Rectf& windowRect = GetRenderManager().GetWindowRect();
			DeviceMVPMatricesState preserveMVP;
			SetupPixelCorrectCoordinates();
			DrawGUITexture( Rectf((windowRect.width-w)*0.5f, (windowRect.height-h)*yFac, w, -h), background, 36, 36, 0, 0, color );
			const float textw = 267;
			DrawGUITexture( Rectf((windowRect.width-textw)*0.5f, (windowRect.height-h)*yFac-25, textw, -20), text, color );
		}
	}
	else if (!GetScreenManager().HasFullscreenRequested())
	{
		gDisplayFullscreenEscapeTimeout = -1000.0;
	}
}
#endif


#if UNITY_CAN_SHOW_SPLASH_SCREEN

static double gSplashScreenStartTime;
static const float kSplashBeforeGameDuration = 4.5f;

void BeginSplashScreenFade()
{
	gSplashScreenStartTime = GetTimeSinceStartup();
}

bool IsSplashScreenFadeComplete()
{
	return (GetTimeSinceStartup() >= gSplashScreenStartTime + kSplashBeforeGameDuration);
}

bool GetShouldShowSplashScreen()
{
	const BuildSettings& settings = GetBuildSettings();
	bool disableWatermark = GetBuildSettings().isNoWatermarkBuild;
	if ( disableWatermark )
		return false;
	bool isTrial = !settings.hasPublishingRights;
	bool isIndie = !settings.hasPROVersion;
	return isTrial || isIndie;
}

#if DEBUGMODE 
	#define DEBUGMSG_ONCE(Message) \
	{\
		static bool once = false; \
		if (!once) { printf_console(Message); once = true; }\
	}
#else
	#define DEBUGMSG_ONCE(Message) {}
#endif

void DrawSplashScreen (bool fullDraw)
{
	AssertIf (!GetShouldShowSplashScreen());

	DEBUGMSG_ONCE("Begin showing splash screen.\n");

	const float kSplashFadeStart = 3.0f;
	const float kSplashXFadeDuration = 0.4f;

	float timeBegin = fullDraw ? 0.0f : kSplashBeforeGameDuration;
	float t = GetTimeSinceStartup() - gSplashScreenStartTime - timeBegin;
	if (!fullDraw && t > kSplashXFadeDuration)
	{
		DEBUGMSG_ONCE("End showing splash screen.\n");
		return;
	}

	Texture2D* logo = GetBuiltinResource<Texture2D>("UnitySplash.png");
	if (!logo)
		return;
	Texture2D* text = GetBuiltinResource<Texture2D>("UnitySplash2.png");
	if (!text)
		return;
	Texture2D* background = GetBuiltinResource<Texture2D>("UnitySplash3.png");
	if (!background)
		return;
	Texture2D* black = GetBuiltinResource<Texture2D>("UnitySplashBack.png");
	if (!black)
		return;

	const Rectf& windowRect = GetRenderManager().GetWindowRect();
	Vector2f basePos (windowRect.width * .5f - 119, windowRect.height *.5f - 219);

	Rectf (basePos.x, basePos.y, 276,432);

	GfxDevice& device = GetGfxDevice();
	if (fullDraw)
	{
		device.BeginFrame();
		if (!device.IsValidState())
		{
			device.HandleInvalidState();
			return;
		}
		const float kBlack[4] = {0,0,0,0};
		GraphicsHelper::Clear (kGfxClearAll, kBlack, 1.0f, 0);
	}

	DeviceMVPMatricesState preserveMVP;
	SetupPixelCorrectCoordinates();

	if (fullDraw)
	{
		float a = SmoothStep( 0.5f, 0.0f, (t - kSplashFadeStart) / (kSplashBeforeGameDuration-kSplashFadeStart));
		DrawGUITexture( Rectf (basePos.x, basePos.y + 432, 276, -432), background, ColorRGBAf(a,a,a, 0.5f) );
		DrawGUITexture( Rectf (basePos.x, basePos.y + 432, 276, -432), background, ColorRGBAf(a,a,a, 0.5f) );
		DrawGUITexture( Rectf (basePos.x + 43, basePos.y + 83, 146, -19), text, ColorRGBAf(a,a,a, 0.5f) );
		DrawGUITexture( Rectf (basePos.x, basePos.y + 330, 206, -214), logo, ColorRGBAf(a,a,a, 0.5f) );
	}
	else
	{
		float a = SmoothStep( 0.5f, 0.0f, t / kSplashXFadeDuration);
		DrawGUITexture(Rectf (0,0,windowRect.width+10, windowRect.height+10), black, ColorRGBAf(0.5f, 0.5f, 0.5f, a));
	}

	if (fullDraw)
	{
		device.EndFrame();
#if ((UNITY_LINUX && SUPPORT_X11) || UNITY_OSX) && !UNITY_EDITOR && !WEBPLUG
		GetScreenManager().SetBlitMaterial();
#endif
		device.PresentFrame();
	}
}

#endif // UNITY_CAN_SHOW_SPLASH_SCREEN


#if UNITY_FLASH
static bool isDebugFlashPlayer;
static bool haveSetDebugFlashPlayer=false;

static bool IsRunningInDebugFlashPlayer()
{
	if (!haveSetDebugFlashPlayer)
	{
		__asm("%0 = flash.system.Capabilities.isDebugger ? 1 : 0;" : "=r"(isDebugFlashPlayer));
		//__asm("trace('Debugger!?!?!');");
		//__asm("trace(flash.system.Capabilities.isDebugger ? 1 : 0);");
		haveSetDebugFlashPlayer = true;
	}
	return isDebugFlashPlayer;
}
#endif

void DrawSplashAndWatermarks()
{
#if SUPPORT_REPRODUCE_LOG
	if (GetReproduceMode() != kNormalPlayback)
		return;
#endif
	// draw splash / watermark
	RuntimePlatform platform = systeminfo::GetRuntimePlatform();
	bool disableWatermark = GetBuildSettings().isNoWatermarkBuild;
	bool isEducational = GetBuildSettings().isEducationalBuild && !UNITY_EDITOR;
	bool isPrototyping = GetBuildSettings().isPrototypingBuild && !UNITY_EDITOR;
	bool isTrial = !GetBuildSettings().hasPublishingRights && !UNITY_EDITOR;
	bool isIndie = !GetBuildSettings().hasPROVersion;
	bool isIndieAndWebPlayer = isIndie && systeminfo::IsPlatformWebPlayer(platform);
	bool isDeveloperPlayerBuild = !UNITY_EDITOR && GetBuildSettings().isDebugBuild;

	if (isIndieAndWebPlayer)
		DrawWaterMark (false);

	int waterMarkOffset = 3;

#if UNITY_FLASH
	if (IsRunningInDebugFlashPlayer())
		DrawDebugFlashPlayerWatermark(waterMarkOffset);
#endif

	const bool isDeveloperBuild = IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1) && isDeveloperPlayerBuild || (UNITY_DEVELOPER_BUILD && !UNITY_FLASH);

	if (isDeveloperBuild)
		DrawDeveloperWatermark(waterMarkOffset);

#if !UNITY_FLASH && !UNITY_WP8
	// Time based licenses will still look like "trial" licenses, so check for educational and prototyping flags first
	if ( !disableWatermark )
	{
		if (isEducational)
			DrawEducationalWatermark (waterMarkOffset);
		else if (isPrototyping)
			DrawPrototypingWatermark (waterMarkOffset);
		else if (isTrial)
			DrawTrialWatermark (waterMarkOffset);
	}
#endif

#if UNITY_ANDROID || UNITY_BB10
 	// it will be drawn completely transparent, so no need to worry ;-)
	if(		gGraphicsCaps.gles20.needToRenderWatermarkDueToDrawFromMemoryBuggy
		&&	GetGfxDevice().GetRenderer() == kGfxRendererOpenGLES20Mobile
		&&	!isDeveloperBuild
	  )
	{
 		DrawSimpleWatermark("UnityWaterMark-dev.png", -(1 + 1), waterMarkOffset, ColorRGBAf(0,0,0,0));
	}
#endif

#if WEBPLUG && !UNITY_PEPPER && !UNITY_FLASH

	// Jonas Echterhoff suggested [2012.07.23 18:21:35] to show only one of the messages,
	// i.e., either plugin beta or content beta, because if you have some unity beta installed
	// you will always have three watermarks: Made with beta, Running on beta and Development player
	#if UNITY_IS_BETA
		DrawPluginBetaWatermark (waterMarkOffset);
	#else
		if( IsContentBuiltWithBetaVersion ())
			DrawContentBetaWatermark (waterMarkOffset);
	#endif // UNITY_IS_BETA
#endif

#if UNITY_CAN_SHOW_SPLASH_SCREEN
	if (GetShouldShowSplashScreen())
		DrawSplashScreen (false);
#endif
}
