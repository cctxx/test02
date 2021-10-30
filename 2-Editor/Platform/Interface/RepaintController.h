#pragma once

#include "Runtime/Math/Rect.h"

#ifdef __OBJC__
@class NSView;
@class NSEvent;
void AddSceneRepaintView (NSView *view);
void RemoveSceneRepaintView (NSView *view);
void HandleGameViewInput (NSEvent* event);
#endif
#if UNITY_WIN
void AddSceneRepaintView (HWND window);
void RemoveSceneRepaintView (HWND window);
#endif

void* GetScreenParamsFromGameView(bool updateScreenManager, bool setActiveView, bool* outHasFocus, Rectf* outGUIRect, Rectf* outCameraRect);
void UpdateScreenManagerAndInput();
void RepaintAllSceneRepaintViews ();
void RepaintView (void * view);
void RenderGameViewCameras(const Rectf& inputRect, bool gizmos, bool gui);

/// Request a repaint of the currently "active" view.
void RepaintCurrentView ();
void SetCurrentView (/*NSView*/ void *currentView);
