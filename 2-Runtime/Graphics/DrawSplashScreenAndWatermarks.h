#pragma once

void DrawSplashAndWatermarks();
void DrawWaterMark( bool alwaysDisplay );

#if UNITY_CAN_SHOW_SPLASH_SCREEN

void BeginSplashScreenFade();
bool IsSplashScreenFadeComplete();
bool GetShouldShowSplashScreen();
void DrawSplashScreen (bool fullDraw);
#endif

#if WEBPLUG
void ShowFullscreenEscapeWarning ();
void RenderFullscreenEscapeWarning ();
extern double gDisplayFullscreenEscapeTimeout;
#endif
