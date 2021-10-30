#include "UnityPrefix.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/File.h"
#import <Cocoa/Cocoa.h>

static std::map<std::string, Image>* gExtensionToIcon = NULL;

static Image ImageFromNSImage (NSImage* image)
{
	AssertIf (image == NULL);
	// Find bitmap image rep
	NSArray* reps = [image representations];
	NSBitmapImageRep* imageRep = NULL;
	if ([reps count] == 0)
	{
		AssertString ("NSImage didn't have is invalid");
		return Image (1, 1, kTexFormatRGBA32);
	}
	
	for (int i=0;i<[reps count];i++)
	{
		//NSLog ([[reps objectAtIndex: i] description]);
		if ([[reps objectAtIndex: i] isKindOfClass: [NSBitmapImageRep class]])
		{
			imageRep = [reps objectAtIndex: i];
			break;
		}
	}
	
	if (imageRep == NULL)
		imageRep = [[NSBitmapImageRep alloc]initWithData: [image TIFFRepresentation]];

	[imageRep setBitsPerSample: 8];
	
	if ([imageRep bitsPerPixel] == 24)
	{
		Image output ([imageRep pixelsWide], [imageRep pixelsHigh], [imageRep bytesPerRow], kTexFormatRGB24, [imageRep bitmapData]);
		output.FlipImageY ();
		return output;
	}
	else if ([imageRep bitsPerPixel] == 32)
	{
		Image output ([imageRep pixelsWide], [imageRep pixelsHigh], [imageRep bytesPerRow], kTexFormatRGBA32, [imageRep bitmapData]);
		output.FlipImageY ();
		Premultiply(output);
		return output;
	}
	else
	{
		AssertString ("Unsupported image when converting from NSImage");
		return Image ([imageRep pixelsWide], [imageRep pixelsHigh], kTexFormatRGBA32);
	}
}

Image ImageForIconAtPath (const std::string& path)
{
	std::string ext = GetPathNameExtension(path);

	if (gExtensionToIcon == NULL)
		gExtensionToIcon = new std::map<std::string, Image>();
	

	std::map<std::string, Image>::iterator result = gExtensionToIcon->find(ext);
	if (result != gExtensionToIcon->end())
		return result->second;
		
	std::string absolute = PathToAbsolutePath (path);
	NSImage* icon = [[NSWorkspace sharedWorkspace]iconForFile: [NSString stringWithUTF8String: absolute.c_str ()]];
	Image img = ImageFromNSImage (icon);

	gExtensionToIcon->insert(std::make_pair(ext, img));
	return img;
}
