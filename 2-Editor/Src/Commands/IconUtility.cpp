#include "UnityPrefix.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Editor/Src/Utility/BuildPlayerUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Src/EditorBuildSettings.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"

#ifndef BITFIELDS_BIGENDIAN
#define BITFIELDS_BIGENDIAN 0
#endif

#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN 0
#endif

using namespace std;

static const char* kIconFileOSX = "Contents/Resources/UnityPlayer.icns";
static const char* kIconFileLinux = "Resources/UnityPlayer.png";
static const char* kStagingArea = "Temp/StagingArea";
static const char* kSplashScreeniPhone = "Default.png";
static const char* kSplashScreeniPhone2x = "Default@2x.png";
static const char* kSplashScreeniPhoneTall2x = "Default-568h@2x.png";
static const char* kSplashScreeniPadPortrait = "Default-Portrait.png";
static const char* kSplashScreeniPadPortrait2x = "Default-Portrait@2x.png";
static const char* kSplashScreeniPadLandscape = "Default-Landscape.png";
static const char* kSplashScreeniPadLandscape2x = "Default-Landscape@2x.png";
static const char* kIconiPhone = "Icon.png";
static const char* kIconiPhone2x = "Icon@2x.png";
static const char* kIconiPhoneiOS7 = "Icon-120.png";
static const char* kIconiPad = "Icon-72.png";
static const char* kIconiPadiOS7 = "Icon-76.png";
static const char* kIconiPad2x = "Icon-144.png";
static const char* kIconiPad2xiOS7 = "Icon-152.png";
static const char* kIconNaCl = "icon128.png";
static const char* kIconNaClSmall = "icon48.png";
static const char* kIconNaClTiny = "icon16.png";
static const char* kSplashScreenAndroid = "app_splash.png";
static const char* kIconAndroidLDPI = "res.drawable-ldpi.app_icon.png";
static const char* kIconAndroidMDPI = "res.drawable-mdpi.app_icon.png";
static const char* kIconAndroidHDPI = "res.drawable-hdpi.app_icon.png";
static const char* kIconAndroidXHDPI = "res.drawable-xhdpi.app_icon.png";
static const char* kIconAndroidXXHDPI = "res.drawable-xxhdpi.app_icon.png";
static const char* kSplashScreenBB10Wide = "app_splash_landscape.png";
static const char* kSplashScreenBB10Tall = "app_splash_portrait.png";
static const char* kSplashScreenBB10Square = "app_splash_square.png";
static const char* kIconBB10 = "app_icon.png";
static const char* kIconTizen = "app_icon.png";

static const int kiPhoneResHigh = 480;
static const int kiPhoneResLow = 320;
static const int kiPhoneResHigh2x = 480 * 2;
static const int kiPhoneResLow2x = 320 * 2;
static const int kiPhoneTallResHigh2x = 568 * 2;
static const int kiPhoneTallResLow2x = 320 * 2;
static const int kiPadResHigh = 1024;
static const int kiPadResLow = 768;
static const int kiPadResHigh2x = 1024 * 2;
static const int kiPadResLow2x = 768 * 2;

// These sizes should match what the function PlayerSettings::GetPlatformIconSizes (ProjectSettings.cpp) returns.
static const int kiPhoneIconRes = 57;
static const int kiPhoneIconRes2x = 114;
static const int kiPhoneIconResiOS7 = 120;
static const int kiPadIconRes = 72;
static const int kiPadIconResiOS7 = 76;
static const int kiPadIconRes2x = 144;
static const int kiPadIconRes2xiOS7 = 152;

static const int kiNaClIconRes = 128;
static const int kiNaClIconResSmall = 48;
static const int kiNaClIconResTiny = 16;

static const int kAndroidIconResLDPI = 36;
static const int kAndroidIconResMDPI = 48;
static const int kAndroidIconResHDPI = 72;
static const int kAndroidIconResXHDPI = 96;
static const int kAndroidIconResXXHDPI = 144;

static const int kBB10ResHigh = 1280;
static const int kBB10ResLow = 720;
static const int kBB10IconRes = 114;

static const int kTizenIconRes = 117;

/*
BEFORE REMOVING: Is this code better optimized than the GetPlatformIconForSize function in ProjectSettings.cpp?
Apply optimization to that function, then remove this one.

Texture2D *GetIconForSize(int width, int height)
{
	Texture2D *bestMatch = NULL;
	int minDist = 0x7fffffff;
	
	std::vector<PPtr<Texture2D> > &icons = GetPlayerSettings ().icons;
	for (std::vector<PPtr<Texture2D> >::iterator i = icons.begin(); i!=icons.end(); i++)
	{
		int dist = Abs(width - (**i).GetDataWidth()) + Abs(height - (**i).GetDataHeight());
		
		// scaling down looks better then scaling up. so first look for bigger icons or exact fits.
		if ((dist < minDist || bestMatch == NULL) && (width <= (**i).GetDataWidth()) && (height <= (**i).GetDataHeight()))
		{
			bestMatch = *i;
			minDist = dist;
		}
	}

	if (bestMatch == NULL)
	{
		for (std::vector<PPtr<Texture2D> >::iterator i = icons.begin(); i!=icons.end(); i++)
		{
			int dist = Abs(width - (**i).GetDataWidth()) + Abs(height - (**i).GetDataHeight());
			if (dist < minDist || bestMatch == NULL)
			{
				bestMatch = *i;
				minDist = dist;
			}
		}
	}
	return bestMatch;
}*/

void WriteBigEndian (vector<UInt8>& buffer, SInt32 data);

void AddIconToFamily (vector<UInt8>& family, const vector<UInt8>& icon, UInt32 type)
{
	WriteBigEndian(family, type);
	WriteBigEndian(family, icon.size() + 8);
	
	family.insert(family.end(), icon.begin(), icon.end());
	
	int totalSize = family.size();
	
	#if UNITY_LITTLE_ENDIAN
	SwapEndianBytes(totalSize);	
	#endif

	((UInt32*)&family.front())[1] = totalSize;
}

void WriteRun (vector<UInt8>& rle, vector<UInt8>& run, bool isSameRun)
{
	if (run.size() == 1)
		isSameRun = false;
	UInt8 length = isSameRun? run.size()+125 : run.size()-1;
	rle.push_back(length);
	
	if (isSameRun)
		rle.push_back (run[0]);
	else for (int i=0; i<run.size(); i++)
		rle.push_back (run[i]);
	run.clear();
}

void EncodeRLE (vector<UInt8>& rle, UInt8* data, int stride, int size)
{
	vector<UInt8> run;
	bool isSameRun = true;
	int pos = 0;
	
	while (pos < size)
	{
		run.push_back (data[pos*stride]);
		
		if (run.size() >= 128)
		{
			WriteRun(rle, run, isSameRun);
			isSameRun = true;
		}
		else if (isSameRun)
		{
			if ( run[0] != run[run.size()-1])
			{
				if (run.size() > 3)
				{
					run.pop_back ();
					WriteRun(rle, run, isSameRun);
					run.push_back (data[pos*stride]);					
				}
				else
					isSameRun = false;
			}
		}
		else
		{
			if ( run.size() >= 3 && run[run.size()-1] == run[run.size()-2] && run[run.size()-1] == run[run.size()-3])
			{
				run.pop_back ();
				run.pop_back ();
				run.pop_back ();
				WriteRun(rle, run, isSameRun);
				isSameRun = true;
				run.push_back (data[pos*stride]);					
				run.push_back (data[pos*stride]);					
				run.push_back (data[pos*stride]);					
			}
		}
		pos++;
	}
	WriteRun(rle, run, isSameRun);
}

void AddIconSizeToFamily (vector<UInt8>& family, Texture2D *texture, int size, UInt32 iconType, UInt32 maskType)
{
	Image image (size, size, kTexFormatRGBA32);
	texture->ExtractImage (&image);
	image.FlipImageY();

	int numPixels = size * size;
	
	vector<UInt8> rle;
	
	// 128x128 have a unknown 32 bit field here
	if (size == 128)
		WriteBigEndian(rle, 0);
		
	// run length encode RGB channels
	EncodeRLE(rle, image.GetImageData (), 4, numPixels);
	EncodeRLE(rle, image.GetImageData ()+1, 4, numPixels);
	EncodeRLE(rle, image.GetImageData ()+2, 4, numPixels);
	AddIconToFamily(family, rle, iconType);
	
	// Add alpha
	vector<UInt8> alpha;
	alpha.resize(numPixels);
	for (int i=0; i<numPixels; i++)
		alpha[i]=image.GetImageData ()[i*4 + 3];
	AddIconToFamily(family, alpha, maskType);
}

void AddIconSizeToFamilyPNG (vector<UInt8>& family, Texture2D *texture, int size, UInt32 iconType)
{
    Image image (size, size, kTexFormatRGBA32);
    texture->ExtractImage (&image);
	
    const string tempIconFile = "Temp/tempIcon.png";
	
    if (!SaveImageToFile (image.GetImageData (), image.GetWidth (), image.GetHeight (), kTexFormatRGBA32, tempIconFile, 'PNGf'))
        return;
	
    vector<UInt8> pngData;
    int fileLength = GetFileLength(tempIconFile);
    pngData.resize (fileLength);
    if (!ReadFromFile (tempIconFile, &pngData[0], 0, fileLength))
        return;
	
    DeleteFile(tempIconFile);
	
    AddIconToFamily(family, pngData, iconType);
}

void WriteOSXIcon (const string &path)
{		
	vector<UInt8> iconData;
	
	WriteBigEndian(iconData, 'icns');
	WriteBigEndian(iconData, 8);

	AddIconSizeToFamilyPNG (iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 256), 256, 'ic08');
	AddIconSizeToFamilyPNG (iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 512), 512, 'ic09');
	AddIconSizeToFamilyPNG (iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 1024), 1024, 'ic10');
	AddIconSizeToFamily(iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 128), 128, 'it32', 't8mk');
	AddIconSizeToFamily(iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 48), 48, 'ih32', 'h8mk');
	AddIconSizeToFamily(iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 32), 32, 'il32', 'l8mk');
	AddIconSizeToFamily(iconData, GetPlayerSettings().GetPlatformIconForSize("Standalone", 16), 16, 'is32', 's8mk');
		
	WriteBytesToFile(&iconData.front(), iconData.size(), path);
}

#define IMAGE_DOS_SIGNATURE    0x5A4D     /* MZ */
#define IMAGE_NT_SIGNATURE     0x00004550 /* PE00 */
#define	IMAGE_DIRECTORY_ENTRY_RESOURCE		2
#define	IMAGE_RESOURCE_NAME_IS_STRING		0x80000000
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA	0x00000080
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16
#define IMAGE_SIZEOF_SHORT_NAME 8
#define IMAGE_RT_ICON          3
#define IMAGE_MAGIC_PE32 0x10b
#define IMAGE_MAGIC_PE64 0x20b


typedef struct {
    UInt16 magic;
    UInt16 cblp;
    UInt16 cp;
    UInt16 crlc;
    UInt16 cparhdr;
    UInt16 minalloc;
    UInt16 maxalloc;
    UInt16 ss;
    UInt16 sp;
    UInt16 csum;
    UInt16 ip;
    UInt16 cs;
    UInt16 lfarlc;
    UInt16 ovno;
    UInt16 res[4];
    UInt16 oemid;
    UInt16 oeminfo;
    UInt16 res2[10];
    UInt32 lfanew;
} DOSImageHeader;

typedef struct {
    UInt16 machine;
    UInt16 number_of_sections;
    UInt32 time_date_stamp;
    UInt32 pointer_to_symbol_table;
    UInt32 number_of_symbols;
    UInt16 size_of_optional_header;
    UInt16 characteristics;
} ImageFileHeader;

typedef struct {
    UInt32 virtual_address;
    UInt32 size;
} ImageDataDirectory;

typedef struct {
    UInt16 magic;
    UInt8 major_linker_version;
    UInt8 minor_linker_version;
    UInt32 size_of_code;
    UInt32 size_of_initialized_data;
    UInt32 size_of_uninitialized_data;
    UInt32 address_of_entry_point;
    UInt32 base_of_code;
    UInt32 base_of_data;
    UInt32 image_base;
    UInt32 section_alignment;
    UInt32 file_alignment;
    UInt16  major_operating_system_version;
    UInt16  minor_operating_system_version;
    UInt16  major_image_version;
    UInt16  minor_image_version;
    UInt16  major_subsystem_version;
    UInt16  minor_subsystem_version;
    UInt32 _version_value;
    UInt32 size_of_image;
    UInt32 size_of_headers;
    UInt32 checksum;
    UInt16 subsystem;
    UInt16 dll_characteristics;
    UInt32 size_of_stack_reserve;
    UInt32 size_of_stack_commit;
    UInt32 size_of_heap_reserve;
    UInt32 size_of_heap_commit;
    UInt32 loader_flags;
    UInt32 number_of_rva_and_sizes;
    ImageDataDirectory data_directory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} ImageOptionalHeader;

typedef struct {
    UInt16 magic;
    UInt8 major_linker_version;
    UInt8 minor_linker_version;
    UInt32 size_of_code;
    UInt32 size_of_initialized_data;
    UInt32 size_of_uninitialized_data;
    UInt32 address_of_entry_point;
    UInt32 base_of_code;
    UInt64 image_base;
    UInt32 section_alignment;
    UInt32 file_alignment;
    UInt16  major_operating_system_version;
    UInt16  minor_operating_system_version;
    UInt16  major_image_version;
    UInt16  minor_image_version;
    UInt16  major_subsystem_version;
    UInt16  minor_subsystem_version;
    UInt32 _version_value;
    UInt32 size_of_image;
    UInt32 size_of_headers;
    UInt32 checksum;
    UInt16 subsystem;
    UInt16 dll_characteristics;
    UInt64 size_of_stack_reserve;
    UInt64 size_of_stack_commit;
    UInt64 size_of_heap_reserve;
    UInt64 size_of_heap_commit;
    UInt32 loader_flags;
    UInt32 number_of_rva_and_sizes;
    ImageDataDirectory data_directory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} ImageOptionalHeader64;

typedef struct {
    UInt32 signature;
    ImageFileHeader file_header;
	union {
		ImageOptionalHeader pe32;
		ImageOptionalHeader64 pe64;
	} optional_header;
} ImageNTHeaders;

typedef struct {
    union {
    	struct {
    	    #if BITFIELDS_BIGENDIAN
    	    unsigned name_is_string:1;
    	    unsigned name_offset:31;
    	    #else
    	    unsigned name_offset:31;
    	    unsigned name_is_string:1;
    	    #endif
    	} s1;
    	UInt32 name;
    	struct {
    	    #if WORDS_BIGENDIAN
    	    UInt16 __pad;
    	    UInt16 id;
    	    #else
    	    UInt16 id;
    	    UInt16 __pad;
    	    #endif
    	} s2;
    } u1;
    union {
    	UInt32 offset_to_data;
    	struct {
    	    #if BITFIELDS_BIGENDIAN
    	    unsigned data_is_directory:1;
    	    unsigned offset_to_directory:31;
    	    #else
    	    unsigned offset_to_directory:31;
    	    unsigned data_is_directory:1;
    	    #endif
    	} s;
    } u2;
} ImageResourceDirectoryEntry;

typedef struct {
    UInt32 offset_to_data;
    UInt32 size;
    UInt32 code_page;
    UInt32 resource_handle;
} ImageResourceDataEntry;

typedef struct {
    UInt32 characteristics;
    UInt32 time_date_stamp;
    UInt16 major_version;
    UInt16 minor_version;
    UInt16 number_of_named_entries;
    UInt16 number_of_id_entries;
} ImageResourceDirectory;

typedef struct  {
    UInt8 name[IMAGE_SIZEOF_SHORT_NAME];
    union {
	UInt32 physical_address;
	UInt32 virtual_size;
    } misc;
    UInt32 virtual_address;
    UInt32 size_of_raw_data;
    UInt32 pointer_to_raw_data;
    UInt32 pointer_to_relocations;
    UInt32 pointer_to_linenumbers;
    UInt16 number_of_relocations;
    UInt16 number_of_linenumbers;
    UInt32 characteristics;
} ImageSectionHeader;

typedef struct {
    UInt32 size;
    SInt32 width;
    SInt32 height;
    UInt16 planes;
    UInt16 bit_count;
    UInt32 compression;
    UInt32 size_image;
    SInt32 x_pels_per_meter;
    SInt32 y_pels_per_meter;
    UInt32 clr_used;
    UInt32 clr_important;
} BitmapInfoHeader;

typedef struct {
    UInt8 blue;
    UInt8 green;
    UInt8 red;
    UInt8 reserved;
} RGBQuad;

void SetWindowsIconDataPNG (ImageResourceDataEntry *dataent, UInt8* imageData)
{
	const int pngSize = 256; // we just assume this, so adding 512x512 icons would not just work, though adding them this way they'd need to take 1MB
	const string tempIconFile = "Temp/tempIcon.png";
	Texture2D* tex = GetPlayerSettings().GetPlatformIconForSize("Standalone", pngSize);
		
	Image image (pngSize, pngSize, kTexFormatRGBA32);
	tex->ExtractImage (&image);

	if (!SaveImageToFile (image.GetImageData (), image.GetWidth (), image.GetHeight (), kTexFormatRGBA32, tempIconFile, 'PNGf'))
		return;

	int fileLength = GetFileLength(tempIconFile);

	if (!ReadFromFile (tempIconFile, imageData, 0, fileLength))
		return;

	DeleteFile(tempIconFile);

	dataent->size = fileLength;
}

void SetWindowsIconDataICO (ImageResourceDataEntry *dataent, UInt8* iconData)
{
	BitmapInfoHeader* iconHeader = (BitmapInfoHeader*)iconData;

	if (iconHeader->size != sizeof(BitmapInfoHeader)
		|| iconHeader->compression != 0
		|| iconHeader->planes != 1
		|| iconHeader->bit_count != 32 )
	{
		// 8-Bit Windows 2000 icons not yet supported - they need palette color mode.
		return;
	}
	
    int width   = iconHeader->width;
    int height  = iconHeader->height / 2;
    int imgSize = width * height * 4;

    char* img  = (char*)iconData + sizeof(BitmapInfoHeader);
    char* mask = img + imgSize;

    // writing image data
    {
        ImageReference image (width, height, width*4, kTexFormatARGB32, img);
	GetPlayerSettings().GetPlatformIconForSize("Standalone", width)->ExtractImage (&image);
	
        UInt32* dst = (UInt32*)img;
        for( int i=0 ; i<width*height ; ++i )
		SwapEndianBytes(dst[i]);
}

    // writing transparency 1bpp mask
    // line should be padded to multiple of dword
    // transparent pixels should be set to full black
    // we patch correct icons - ant their size is multiple of 8 always, so we can have it easy there

    {
        const unsigned lineByteCount    = width / 8;
        const unsigned lineByteDwordPad = ((lineByteCount+3) & ~3) - lineByteCount;

        for(unsigned row = 0 ; row < height ; ++row)
        {
            for( unsigned pack = 0 ; pack < lineByteCount ; ++pack, ++mask )
            {
                unsigned packMask = 0;
                for( unsigned pix = 0 ; pix < 8 ; ++pix, img += 4 )
                {
                    static const unsigned _TransparentThreshold = 64;
                    if(img[3] < _TransparentThreshold)
                    {
                        img[0] = img[1] = img[2] = img[3] = 0;
                        packMask |= (1<<(7-pix));
                    }
                }
                *mask = packMask;
            }

            for(unsigned i = 0 ; i < lineByteDwordPad ; ++i, ++mask)
                *mask = ~0;
        }
    }
}

void SetWindowsIconData (ImageResourceDataEntry *dataent, UInt8* imageVirtualSpace)
{
	const int pngMagic = 0x474E5089;
	UInt8* imageData = imageVirtualSpace + dataent->offset_to_data;

	if (pngMagic == *(int*)imageData)
		SetWindowsIconDataPNG(dataent, imageData);
	else
		SetWindowsIconDataICO(dataent, imageData);
}

void SetWindowsIconResources (ImageResourceDirectory* base, ImageResourceDirectory* resDir, int level, UInt8* imageVirtualSpace)
{
	int resCount = resDir->number_of_named_entries + resDir->number_of_id_entries;
	ImageResourceDirectoryEntry *entries = (ImageResourceDirectoryEntry*) (resDir+1);
	
	for (int i=0;i<resCount;i++)
	{
		bool isDirectory = entries[i].u2.s.data_is_directory;
		int name = entries[i].u1.name;
		
		if (level == 0 && name != IMAGE_RT_ICON)
			continue;
		
		UInt8* data = (UInt8*)base+entries[i].u2.s.offset_to_directory;
		if (isDirectory)
			SetWindowsIconResources(base, (ImageResourceDirectory*)data, level+1, imageVirtualSpace);
		else
		{
			// SetIconData calls GetIconForSize
			SetWindowsIconData ((ImageResourceDataEntry*)data, imageVirtualSpace);
		}
	}
}

void AddIconToWindowsExecutable (const string& executablePath)
{
	File exe;
	exe.Open(executablePath, File::kReadWritePermission);
	int offset = 0;
	
	DOSImageHeader dosHeader;
	exe.Read(offset, &dosHeader, sizeof(DOSImageHeader));
	if (dosHeader.magic == IMAGE_DOS_SIGNATURE)
	{
		if (dosHeader.lfanew < sizeof(DOSImageHeader))
		{
			ErrorString("Unknown binary format");
			return;
		}
		offset = dosHeader.lfanew;
	}
	
	ImageNTHeaders ntHeader;
	exe.Read(offset, &ntHeader, sizeof(ImageNTHeaders));
	if (ntHeader.signature != IMAGE_NT_SIGNATURE)
	{
		ErrorString("Unknown binary format");
		return;
	}
	offset += sizeof(ImageFileHeader) + sizeof(UInt32) + ntHeader.file_header.size_of_optional_header;
	
	int segCount = ntHeader.file_header.number_of_sections;

	ImageSectionHeader *sectionHeaders;
	sectionHeaders = new ImageSectionHeader[segCount];
	exe.Read(offset, sectionHeaders, sizeof(ImageSectionHeader) * segCount);
		
	for (int i=0;i<segCount;i++)
	{
		if (!(sectionHeaders[i].characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA))
		{
			if (strcmp((const char*)sectionHeaders[i].name,".rsrc") == 0)
			{
				UInt32 imageSize = sectionHeaders[i].virtual_address + sectionHeaders[i].size_of_raw_data;
				UInt8* imageVirtualSpace = new UInt8[imageSize];
				
				exe.Read(sectionHeaders[i].pointer_to_raw_data, imageVirtualSpace+sectionHeaders[i].virtual_address, sectionHeaders[i].size_of_raw_data);
				
				ImageDataDirectory* dir;
				if (ntHeader.optional_header.pe32.magic == IMAGE_MAGIC_PE32)
					dir = ntHeader.optional_header.pe32.data_directory + IMAGE_DIRECTORY_ENTRY_RESOURCE;			
				else
					dir = ntHeader.optional_header.pe64.data_directory + IMAGE_DIRECTORY_ENTRY_RESOURCE;			
				ImageResourceDirectory* resDir = (ImageResourceDirectory*) (imageVirtualSpace + dir->virtual_address);
				
				// SetWindowsIconResources calls GetIconForSize
				SetWindowsIconResources (resDir, resDir, 0, imageVirtualSpace);

				exe.Write(sectionHeaders[i].pointer_to_raw_data, imageVirtualSpace+sectionHeaders[i].virtual_address, sectionHeaders[i].size_of_raw_data);

				delete[] imageVirtualSpace;
			}
		}
	}
	
	delete[] sectionHeaders;
	exe.Close();
}

// Returns pixel address for specified image. ARGB32 & RGB24 formats supported
inline UInt8* GetPixelAddress(Image& img, int x, int y, const TextureFormat& tex_format)
{
	UInt8* pixel = img.GetRowPtr(y) + x * (tex_format == kTexFormatARGB32 ? 4 : 3);
	return pixel;
}

static void ExportPNGImage(Texture2D* texture, int resx, int resy, string path, 
						   ScreenOrientation desiredOrientation = kScreenOrientationUnknown, bool use_alpha = false);
static void ExportPNGImage(Texture2D* texture, int resx, int resy, string path, 
						   ScreenOrientation desiredOrientation, bool use_alpha)
{
	const TextureFormat tex_format = use_alpha ? kTexFormatRGBA32 : kTexFormatRGB24;
	int srcresx = texture->GetDataWidth();
	int srcresy = texture->GetDataHeight();
	
	TextureImporter* ti = dynamic_pptr_cast<TextureImporter*>(FindAssetImporterForObject(texture->GetInstanceID()));
	
	if (ti)
	{
		srcresx = ti->GetSourceTextureInformation().width;
		srcresy = ti->GetSourceTextureInformation().height;
	}

	Image image (resx, resy, tex_format);
	
	// We don't care about rotation
	if (desiredOrientation == kScreenOrientationUnknown || (resx - resy)*(srcresx - srcresy) >= 0)
	{
		texture->ExtractImage (&image);	
	}
	else
	{
		// scale image to final size (before rotation)
		Image tmpImage(resy, resx, tex_format);
		texture->ExtractImage(&tmpImage);
		
		for (int i = 0; i < tmpImage.GetHeight(); i++)
		{
			for (int j = 0; j < tmpImage.GetWidth(); j++)
			{
				UInt8* src = GetPixelAddress(tmpImage, j, i, tex_format);
				UInt8* dst = NULL;
				
				// Image is landscape, but we need portrait, let's rotate it
				if (resx - resy < 0 && srcresx - srcresy > 0)
				{
					if (desiredOrientation == kLandscapeLeft)
						dst = GetPixelAddress(image, i, resy - j - 1, tex_format);
					else 
						dst = GetPixelAddress(image, resx - i - 1, j, tex_format);

				}
				// Image is portrait, but we need landscape
				else 
				{
					dst = GetPixelAddress(image, resx - i - 1, j, tex_format);
				}
				
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				
				if (tex_format == kTexFormatRGBA32)
					dst[3] = src[3];
			}
		}
	}

	SaveImageToFile (image.GetImageData (), image.GetWidth (), image.GetHeight (), tex_format, path, 'PNGf');
}

void WarnIfCompressed(Texture2D* img, bool icon = true)
{
	const char* iconorsplash = icon ? "icon" : "splash screen";
	if (IsAnyCompressedTextureFormat(img->GetTextureFormat()))
		WarningString(Format("Compressed texture %s is used as %s. This might compromise visual quality of the final image. Uncompressed format might be considered as better import option.", img->GetName(), iconorsplash));
}

bool ValidateSplashImage(const PPtr<Texture2D>& img)
{
	if (!img.IsValid())
		return false;
	
	WarnIfCompressed(img, false);
	return true;
}

void AddIcon(const string& buildpath, BuildTargetPlatform platform)
{
	const EditorOnlyPlayerSettings& settings = GetPlayerSettings().GetEditorOnly();

	if (kBuild_iPhone == platform)
	{
		ScreenOrientation desiredOrientation = PlayerSettingsToScreenOrientation(GetPlayerSettings().GetDefaultScreenOrientation());
		if (desiredOrientation == kAutoRotation)
		{
			// if autorotation is selected then choose ANY of allowed orientations as splashscreen orientation
			if (GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kLandscapeLeft)))
				desiredOrientation = kLandscapeLeft;
			else if (GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kLandscapeRight)))
				desiredOrientation = kLandscapeRight;
			else if (GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kPortraitUpsideDown)))
				desiredOrientation = kPortraitUpsideDown;
			else
				desiredOrientation = kPortrait;
		}
		
		if (ValidateSplashImage(settings.iPhoneSplashScreen))
		{
			ExportPNGImage(settings.iPhoneSplashScreen, kiPhoneResLow, kiPhoneResHigh, 
						   AppendPathName (kStagingArea, kSplashScreeniPhone), desiredOrientation);
		}

        if (ValidateSplashImage(settings.iPhoneHighResSplashScreen))
		{
            ExportPNGImage(settings.iPhoneHighResSplashScreen, kiPhoneResLow2x, kiPhoneResHigh2x, 
						   AppendPathName (kStagingArea, kSplashScreeniPhone2x), desiredOrientation);
		}
		
		if (ValidateSplashImage(settings.iPhoneTallHighResSplashScreen))
		{
            ExportPNGImage(settings.iPhoneTallHighResSplashScreen, kiPhoneTallResLow2x, kiPhoneTallResHigh2x,
						   AppendPathName (kStagingArea, kSplashScreeniPhoneTall2x), desiredOrientation);
		}
		
		if (ValidateSplashImage(settings.iPadPortraitSplashScreen))
			ExportPNGImage(settings.iPadPortraitSplashScreen, kiPadResLow, kiPadResHigh, AppendPathName (kStagingArea, kSplashScreeniPadPortrait));
		
		if (ValidateSplashImage(settings.iPadHighResPortraitSplashScreen))
			ExportPNGImage(settings.iPadHighResPortraitSplashScreen, kiPadResLow2x, kiPadResHigh2x, AppendPathName (kStagingArea, kSplashScreeniPadPortrait2x));
		
		if (ValidateSplashImage(settings.iPadLandscapeSplashScreen))
			ExportPNGImage(settings.iPadLandscapeSplashScreen, kiPadResHigh, kiPadResLow, AppendPathName (kStagingArea, kSplashScreeniPadLandscape));
		
		if (ValidateSplashImage(settings.iPadHighResLandscapeSplashScreen))
			ExportPNGImage(settings.iPadHighResLandscapeSplashScreen, kiPadResHigh2x, kiPadResLow2x, AppendPathName (kStagingArea, kSplashScreeniPadLandscape2x));
		
		if (GetPlayerSettings().GetPlatformIconForSize("iPhone", 0) != NULL)
		{
			Texture2D* texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPhoneIconRes);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiPhoneIconRes, kiPhoneIconRes, AppendPathName (kStagingArea, kIconiPhone));

            texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPhoneIconResiOS7);
            WarnIfCompressed(texture);
            ExportPNGImage(texture, kiPhoneIconResiOS7, kiPhoneIconResiOS7, AppendPathName (kStagingArea, kIconiPhoneiOS7));
			
			texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPhoneIconRes2x);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiPhoneIconRes2x, kiPhoneIconRes2x, AppendPathName (kStagingArea, kIconiPhone2x));
			
			texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPadIconRes);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiPadIconRes, kiPadIconRes, AppendPathName (kStagingArea, kIconiPad));

            texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPadIconResiOS7);
            WarnIfCompressed(texture);
            ExportPNGImage(texture, kiPadIconResiOS7, kiPadIconResiOS7, AppendPathName (kStagingArea, kIconiPadiOS7));
			
			texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPadIconRes2x);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiPadIconRes2x, kiPadIconRes2x, AppendPathName (kStagingArea, kIconiPad2x));

            texture = GetPlayerSettings ().GetPlatformIconForSize("iPhone", kiPadIconRes2xiOS7);
            WarnIfCompressed(texture);
            ExportPNGImage(texture, kiPadIconRes2xiOS7, kiPadIconRes2xiOS7, AppendPathName (kStagingArea, kIconiPad2xiOS7));
		}
	}
	else if (kBuild_Android == platform)
	{
		if (ValidateSplashImage(settings.iPhoneSplashScreen))
		{
			Texture2D* texture = settings.iPhoneSplashScreen;
			ExportPNGImage(texture, texture->GetDataWidth(), texture->GetDataHeight(), AppendPathName ("Temp/StagingArea", kSplashScreenAndroid));
		}
		
		Texture2D* texture_ldpi = GetPlayerSettings().GetPlatformIconForSize("Android", kAndroidIconResLDPI);
		Texture2D* texture_mdpi = GetPlayerSettings().GetPlatformIconForSize("Android", kAndroidIconResMDPI);
		Texture2D* texture_hdpi = GetPlayerSettings().GetPlatformIconForSize("Android", kAndroidIconResHDPI);
		Texture2D* texture_xhdpi = GetPlayerSettings().GetPlatformIconForSize("Android", kAndroidIconResXHDPI);
		Texture2D* texture_xxhdpi = GetPlayerSettings().GetPlatformIconForSize("Android", kAndroidIconResXXHDPI);
		if (texture_ldpi != NULL)
		{
			WarnIfCompressed(texture_mdpi);
			ExportPNGImage(texture_ldpi, kAndroidIconResLDPI, kAndroidIconResLDPI,
						   AppendPathName ("Temp/StagingArea", kIconAndroidLDPI), kScreenOrientationUnknown, true);
		}
		if (texture_mdpi != NULL)
		{
			WarnIfCompressed(texture_mdpi);
			ExportPNGImage(texture_mdpi, kAndroidIconResMDPI, kAndroidIconResMDPI, 
						   AppendPathName ("Temp/StagingArea", kIconAndroidMDPI), kScreenOrientationUnknown, true);
		}
		if (texture_hdpi != NULL)
		{
			WarnIfCompressed(texture_hdpi);
			ExportPNGImage(texture_hdpi, kAndroidIconResHDPI, kAndroidIconResHDPI, 
						   AppendPathName ("Temp/StagingArea", kIconAndroidHDPI), kScreenOrientationUnknown, true);
		}
		if (texture_xhdpi != NULL)
		{
			WarnIfCompressed(texture_xhdpi);
			ExportPNGImage(texture_xhdpi, kAndroidIconResXHDPI, kAndroidIconResXHDPI,
						   AppendPathName ("Temp/StagingArea", kIconAndroidXHDPI), kScreenOrientationUnknown, true);
		}
		if (texture_xxhdpi != NULL)
		{
			WarnIfCompressed(texture_xxhdpi);
			ExportPNGImage(texture_xxhdpi, kAndroidIconResXXHDPI, kAndroidIconResXXHDPI,
						   AppendPathName ("Temp/StagingArea", kIconAndroidXXHDPI), kScreenOrientationUnknown, true);
		}
	}
	else if (kBuildNaCl == platform)
	{
		if (GetPlayerSettings().GetPlatformIconForSize("Web", 0) != NULL)
		{
			Texture2D* texture = GetPlayerSettings ().GetPlatformIconForSize("Web", kiNaClIconRes);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiNaClIconRes, kiNaClIconRes, AppendPathName (buildpath, kIconNaCl), kScreenOrientationUnknown, true);
			
			texture = GetPlayerSettings ().GetPlatformIconForSize("Web", kiNaClIconResSmall);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiNaClIconResSmall, kiNaClIconResSmall, AppendPathName (buildpath, kIconNaClSmall), kScreenOrientationUnknown, true);
			
			texture = GetPlayerSettings ().GetPlatformIconForSize("Web", kiNaClIconResTiny);
			WarnIfCompressed(texture);
			ExportPNGImage(texture, kiNaClIconResTiny, kiNaClIconResTiny, AppendPathName (buildpath, kIconNaClTiny), kScreenOrientationUnknown, true);
		}
		else
		{
			Texture2D* texture = GetEditorAssetBundle()->Get<Texture2D> ("Icons/UnityLogoLarge.png");	
			if (texture != NULL)
			{
				ExportPNGImage(texture, kiNaClIconRes, kiNaClIconRes, AppendPathName (buildpath, kIconNaCl), kScreenOrientationUnknown, true);
				ExportPNGImage(texture, kiNaClIconResSmall, kiNaClIconResSmall, AppendPathName (buildpath, kIconNaClSmall), kScreenOrientationUnknown, true);
				ExportPNGImage(texture, kiNaClIconResTiny, kiNaClIconResTiny, AppendPathName (buildpath, kIconNaClTiny), kScreenOrientationUnknown, true);
			}
		}
	}
	else if(kBuildBB10 == platform)
	{
		ScreenOrientation desiredOrientation = PlayerSettingsToScreenOrientation(GetPlayerSettings().GetDefaultScreenOrientation());
		if (desiredOrientation == kAutoRotation)
		{
			// if autorotation is selected then choose ANY of allowed orientations as splashscreen orientation
			if (GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kLandscapeLeft)))
				desiredOrientation = kLandscapeLeft;
			else if (GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kLandscapeRight)))
				desiredOrientation = kLandscapeRight;
			else if (GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kPortraitUpsideDown)))
				desiredOrientation = kPortraitUpsideDown;
			else
				desiredOrientation = kPortrait;
		}

		if (ValidateSplashImage(settings.blackberryLandscapeSplashScreen))
		{
            ExportPNGImage(settings.blackberryLandscapeSplashScreen, kBB10ResHigh, kBB10ResLow,
						   AppendPathName (kStagingArea, kSplashScreenBB10Wide), desiredOrientation);
		}

        if (ValidateSplashImage(settings.blackberryPortraitSplashScreen))
		{
            ExportPNGImage(settings.blackberryPortraitSplashScreen, kBB10ResLow, kBB10ResHigh,
						   AppendPathName (kStagingArea, kSplashScreenBB10Tall), desiredOrientation);
		}

		if (ValidateSplashImage(settings.blackberrySquareSplashScreen))
		{
            ExportPNGImage(settings.blackberrySquareSplashScreen, kBB10ResLow, kBB10ResLow,
						   AppendPathName (kStagingArea, kSplashScreenBB10Square), desiredOrientation);
		}

		if (GetPlayerSettings().GetPlatformIconForSize("BlackBerry", 0) != NULL)
		{
			Texture2D* texture = GetPlayerSettings ().GetPlatformIconForSize("BlackBerry", kBB10IconRes);
			ExportPNGImage(texture, kBB10IconRes, kBB10IconRes, AppendPathName (kStagingArea, kIconBB10), kScreenOrientationUnknown, true);
		}
	}
	else if(kBuildTizen == platform)
	{
		if (GetPlayerSettings().GetPlatformIconForSize("Tizen", 0) != NULL)
		{
			Texture2D* texture = GetPlayerSettings ().GetPlatformIconForSize("Tizen", kTizenIconRes);
			ExportPNGImage(texture, kTizenIconRes, kTizenIconRes, AppendPathName (kStagingArea, kIconTizen), kScreenOrientationUnknown, true);
		}
	}
	
	if (GetPlayerSettings().GetPlatformIconForSize("Standalone", 0) != NULL)
	{
		if (IsPlatformOSXStandalone (platform) )
		{
			string path = AppendPathName (buildpath, kIconFileOSX);
			WriteOSXIcon (path);
		}
		else if (platform == kBuildStandaloneWinPlayer || platform == kBuildStandaloneWin64Player)
		{
			// what windows player icon sizes are included depends on what image sizes icon in the windows stub player has
			// windows player has default Unity icons for all supported sizes that are replaced with custom ones when they are selected
			AddIconToWindowsExecutable (buildpath);
		}
		else if (platform == kBuildStandaloneLinux || platform == kBuildStandaloneLinux64 || platform == kBuildStandaloneLinuxUniversal)
		{
			string path = AppendPathName (DeletePathNameExtension (buildpath) + "_Data", kIconFileLinux);
			ExportPNGImage (GetPlayerSettings().GetPlatformIconForSize("Standalone", 128), 128, 128, path, kScreenOrientationUnknown, true);
		}
	}
}
