#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ColorPicker.h"
#include <QuickTime/QuickTime.h>
#include <Cocoa/Cocoa.h>

static void memswap(void *m1, void *m2, size_t n)
{
	char *p = (char*)m1;
	char *q = (char*)m2;

	while ( n-- ) {
		char t = *p;
		*p = *q;
		*q = t;
		p++; q++;
	}
}

// Ugh. Can we move away from 10.4 sdk soon? Then all this won't be needed.
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
enum {
  kCGWindowListOptionOnScreenOnly = (1 << 0),
  kCGWindowImageDefault = 0,
};

typedef uint32_t CGWindowID;
typedef uint32_t CGWindowListOption;
typedef uint32_t CGWindowImageOption;

#define kCGNullWindowID ((CGWindowID)0)
typedef int NSInteger;
typedef unsigned int NSUInteger;


CFBundleRef LoadSystemBundle(const char* frameworkName);
CGImageRef CGWindowListCreateImage(CGRect screenBounds,
  CGWindowListOption listOption, CGWindowID windowID,
  CGWindowImageOption imageOption)
{
	typedef CGImageRef (*fpCGWindowListCreateImage) (CGRect screenBounds,
	  CGWindowListOption listOption, CGWindowID windowID,
	  CGWindowImageOption imageOption);

	//We can't hardlink these functions, since we use 10.4 API. So, get them dynamically
	static fpCGWindowListCreateImage _CGWindowListCreateImage = NULL;
	if (_CGWindowListCreateImage == NULL)
	{
		CFBundleRef cgBundle = LoadSystemBundle("ApplicationServices.framework:Frameworks:CoreGraphics.framework");
		if (cgBundle == NULL)
			return false;
			
		_CGWindowListCreateImage = (fpCGWindowListCreateImage)CFBundleGetFunctionPointerForName (cgBundle, CFSTR("CGWindowListCreateImage"));
		if (_CGWindowListCreateImage == NULL)
			return false;
	}
	return _CGWindowListCreateImage (screenBounds, listOption, windowID, imageOption);
}
#endif


unsigned char* ReadScreenPixels (int x, int y, int sizex, int sizey, int* outBPP)
{	
	static std::vector<unsigned char> rawData;

	CGRect area = CGRectMake(x, y, sizex, sizey);
	CGImageRef image = CGWindowListCreateImage(area, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	NSUInteger bytesPerPixel = 4;
	rawData.resize (sizey * sizex * bytesPerPixel);
	*outBPP = bytesPerPixel;
	NSUInteger bytesPerRow = bytesPerPixel * sizex;
	NSUInteger bitsPerComponent = 8;
	CGContextRef context = CGBitmapContextCreate(&rawData[0], sizex, sizey, bitsPerComponent, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
	CGColorSpaceRelease(colorSpace);

	CGContextDrawImage(context, CGRectMake(0, 0, sizex, sizey), image);
	CGContextRelease(context);
	
	// Image data has first row as top row, but we expect that first row should be bottom row,
	// so we reverse the order of the rows.
	unsigned char tmp;
	for (int j=0; j<sizey/2; j++)
	{
		for (int i=0; i<bytesPerRow; i++)
		{
			tmp = rawData[i + j*bytesPerRow];
			rawData[i + j*bytesPerRow] = rawData[i + (sizey-1-j)*bytesPerRow];
			rawData[i + (sizey-1-j)*bytesPerRow] = tmp;
		}
	}
	
	return &rawData[0];
}


void OSColorPickerShow(bool showAlpha)
{
	NSColorPanel *cp = [NSColorPanel sharedColorPanel];
	[cp makeKeyAndOrderFront: NULL];
	[cp setContinuous: YES];
	[cp setShowsAlpha: showAlpha];
}

void OSColorPickerClose()
{
	if(OSColorPickerIsVisible())
	{
		NSColorPanel *cp = [NSColorPanel sharedColorPanel];
		[cp close];
	}
}

bool OSColorPickerIsVisible()
{
	if(![NSColorPanel sharedColorPanelExists])
		return false;
	return [[NSColorPanel sharedColorPanel] isVisible];
}

ColorRGBAf OSColorPickerGetColor()
{
	NSColor *color = [[[NSColorPanel sharedColorPanel] color] colorUsingColorSpaceName: NSCalibratedRGBColorSpace];
	return ColorRGBAf([color redComponent], [color greenComponent], [color blueComponent], [color alphaComponent]);
}

void OSColorPickerSetColor(const ColorRGBAf &color)
{
	[[NSColorPanel sharedColorPanel] setColor: [NSColor colorWithCalibratedRed: color.r green: color.g blue: color.b alpha: color.a]];
}