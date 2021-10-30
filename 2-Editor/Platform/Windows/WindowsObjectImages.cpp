#include "UnityPrefix.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include <shellapi.h>
#include "Runtime/Math/Color.h"


static std::map<std::string, Image>* gExtensionToIcon = NULL;

Image ImageForIconAtPath( const std::string& path )
{
	if (gExtensionToIcon == NULL)
		gExtensionToIcon = new std::map<std::string, Image>();

	std::string ext = GetPathNameExtension(path);

	std::map<std::string, Image>::iterator result = gExtensionToIcon->find(ext);
	if (result != gExtensionToIcon->end())
		return result->second;	

	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( PathToAbsolutePath(path).c_str(), widePath, kDefaultPathBufferSize );
	SHFILEINFOW info;
	info.hIcon = NULL;
	DWORD_PTR code = SHGetFileInfoW( widePath, 0, &info, sizeof(info), SHGFI_ICON|SHGFI_LARGEICON );
	if( info.hIcon == NULL )
		return Image();

	HDC hdcCompatible = CreateCompatibleDC(NULL);

	const int kIconSize = 32;

	HBITMAP hbmDest = CreateBitmap( kIconSize, kIconSize, 1, 32, NULL );
	if (hbmDest == 0) {
		DeleteObject(hdcCompatible);
		return Image();
	}

	SelectObject(hdcCompatible, hbmDest);
	DrawIconEx(hdcCompatible, 0, 0, info.hIcon, kIconSize, kIconSize, 0, NULL, DI_NORMAL);

	Image img( kIconSize, kIconSize, kTexFormatRGBA32 );
	BITMAPINFO bi;
	memset( &bi, 0, sizeof(bi) );
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = kIconSize;
	bi.bmiHeader.biHeight = kIconSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	int res = GetDIBits( hdcCompatible, hbmDest, 0, kIconSize, img.GetImageData(), &bi, DIB_RGB_COLORS );

	ColorRGBA32* col = reinterpret_cast<ColorRGBA32*>(img.GetImageData());
	for( size_t i = 0; i < kIconSize*kIconSize; ++i ) {
		col[i] = col[i].SwizzleToBGRA();
	}

	DeleteObject(hbmDest);
	DeleteObject(hdcCompatible);

	DestroyIcon( info.hIcon );

	gExtensionToIcon->insert(std::make_pair<std::string, Image>(ext, img));
	return img;
}
