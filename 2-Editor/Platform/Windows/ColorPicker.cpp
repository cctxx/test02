#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ColorPicker.h"
#include "Runtime/Math/Color.h"

unsigned char* ReadScreenPixels (int x, int y, int sizex, int sizey, int* outBPP)
{
	*outBPP = 4;

	static ColorRGBA32 singlePixel = ColorRGBA32(0xFFFFFFFF);
	if( sizex == 1 && sizey == 1 ) {
		HDC hdcScreen = GetDC(NULL);
		COLORREF c = GetPixel(hdcScreen, x, y);
		singlePixel.r = GetRValue(c);
		singlePixel.g = GetGValue(c);
		singlePixel.b = GetBValue(c);
		ReleaseDC(NULL,hdcScreen);
		return singlePixel.GetPtr();
	}

	HDC hdcScreen = GetDC(NULL);
	HDC hdcCompatible = CreateCompatibleDC(hdcScreen);

	HBITMAP hbmScreen = CreateBitmap( sizex, sizey, 1, 32, NULL );
	if (hbmScreen == 0) {
		ReleaseDC(NULL,hdcScreen);
		DeleteObject(hdcCompatible);
		return NULL;
	}

	SelectObject(hdcCompatible, hbmScreen);
	if( !BitBlt( hdcCompatible, 0,0,sizex, sizey, hdcScreen, x,y, SRCCOPY) ) 
	{
		DeleteObject(hbmScreen);
		ReleaseDC(NULL,hdcScreen);
		DeleteObject(hdcCompatible);
		return NULL;
	}

	// debug
	//BitBlt( hdcScreen, 0, 0, sizex, sizey, hdcCompatible, 0, 0, SRCCOPY );

	static std::vector<ColorRGBA32> s_Colors;
	s_Colors.resize( sizex * sizey );
	BITMAPINFO bi;
	memset( &bi, 0, sizeof(bi) );
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = sizex;
	bi.bmiHeader.biHeight = sizey;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	int res = GetDIBits( hdcCompatible, hbmScreen, 0, sizey, &s_Colors[0], &bi, DIB_RGB_COLORS );

	for( size_t i = 0; i < s_Colors.size(); ++i ) {
		s_Colors[i] = s_Colors[i].SwizzleToBGRA();
	}

	DeleteObject(hbmScreen);
	ReleaseDC(NULL,hdcScreen);
	DeleteObject(hdcCompatible);

	return (unsigned char*)&s_Colors[0];
}
