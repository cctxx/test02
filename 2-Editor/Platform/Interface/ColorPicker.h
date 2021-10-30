#pragma once

#include "Runtime/Math/Color.h"

unsigned char* ReadScreenPixels (int x, int y, int sizex, int sizey, int* outBPP);

#if UNITY_OSX
void OSColorPickerShow(bool showAlpha);
void OSColorPickerClose();
bool OSColorPickerIsVisible();
ColorRGBAf OSColorPickerGetColor();
void OSColorPickerSetColor(const ColorRGBAf &color);
#else
inline void OSColorPickerShow(bool showAlpha) { ErrorString("Color Picker not supported on this platform!"); }
inline void OSColorPickerClose() { ErrorString("Color Picker not supported on this platform!"); }
inline bool OSColorPickerIsVisible() { ErrorString("Color Picker not supported on this platform!"); return false; }
inline ColorRGBAf OSColorPickerGetColor()  { ErrorString("Color Picker not supported on this platform!"); return ColorRGBAf(0,0,0,0); }
inline void OSColorPickerSetColor(const ColorRGBAf &color) { ErrorString("Color Picker not supported on this platform!"); }
#endif