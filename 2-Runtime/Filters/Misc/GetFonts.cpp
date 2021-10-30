#include "UnityPrefix.h"
#include "Font.h"
#include "Runtime/Utilities/File.h"
#if UNITY_LINUX
#include <ftw.h>
#endif

#if DYNAMICFONTMODE == kDynamicFontModeFreeType || DYNAMICFONTMODE == kDynamicFontModeStb


typedef std::vector<UnityStr> FontDirs;
static FontDirs* gFontDirs = NULL;

struct _FontInfo
{
	const char* family_name;
	const char* style_name;
	unsigned style_flags;
	unsigned face_flags;
};
typedef std::map<std::string, _FontInfo> FontMetadataMap;
static FontMetadataMap* gFontMetadata = NULL;

static FontNames* gFontFallbacks = NULL;

namespace GetFontsManager
{
	void StaticInitialize()
	{
		gFontDirs = UNITY_NEW(FontDirs, kMemFont);
		gFontMetadata = UNITY_NEW(FontMetadataMap, kMemFont);
		gFontFallbacks = UNITY_NEW(FontNames, kMemFont);
	}

	void StaticDestroy()
	{
		UNITY_DELETE(gFontDirs , kMemFont);
		UNITY_DELETE(gFontMetadata, kMemFont);
		UNITY_DELETE(gFontFallbacks, kMemFont);
	}
}


FontNames &GetFallbacks ()
{
	if (gFontFallbacks->empty())
	{
		// Make Arial first fallback for consistency, as it widely available.
		gFontFallbacks->push_back("Arial"); 
		// Arial Unicode MS covers almost all unicode scripts, and is available on OS X (>=10.5).
		gFontFallbacks->push_back("Arial Unicode MS"); 
		// This should catch Unicode scripts on Windows, excluding Asian scripts
		gFontFallbacks->push_back("Microsoft Sans Serif");
		// This should catch Chinese on windows
		gFontFallbacks->push_back("Microsoft YaHei");
		// This should catch Korean on windows
		gFontFallbacks->push_back("Gulim");
		// This should catch Japanese on windows
		gFontFallbacks->push_back("MS Gothic");
#if UNITY_ANDROID
		// Android system font
		gFontFallbacks->push_back("Roboto");
		gFontFallbacks->push_back("NanumGothic");
		gFontFallbacks->push_back("Droid Sans");
		gFontFallbacks->push_back("Droid Sans Japanese");
		gFontFallbacks->push_back("Droid Sans Fallback");
#elif UNITY_IPHONE
		gFontFallbacks->push_back("Hiragino Kaku Gothic ProN");
		gFontFallbacks->push_back("Heiti TC");
		gFontFallbacks->push_back("AppleGothic");
		gFontFallbacks->push_back(".LastResort");
#elif UNITY_WP8
		gFontFallbacks->push_back("Yu Gothic");           // Japanese
		gFontFallbacks->push_back("Microsoft NeoGothic"); // Korean
		gFontFallbacks->push_back("SimSun");              // Chinese simplified
		gFontFallbacks->push_back("Microsoft Mhei");      // Chinese traditional
		gFontFallbacks->push_back("Urdu Typesetting");    // Arabic
#elif UNITY_TIZEN
		gFontFallbacks->push_back("Tizen Sans");
		gFontFallbacks->push_back("Tizen Sans Japanese");
		gFontFallbacks->push_back("Tizen Sans Fallback");
#endif
#if UNITY_LINUX
		gFontFallbacks->push_back("FreeSans");
		gFontFallbacks->push_back("WenQuanYi Micro Hei");
#endif
		// Unicode debugging fallback: http://en.wikipedia.org/wiki/Fallback_font
		gFontFallbacks->push_back("LastResort");
	}
	return *gFontFallbacks;
}

#if UNITY_LINUX
int callback(const char *fpath, const struct stat *sb, int typeflag)
{
	if (typeflag == FTW_D)
	{
		gFontDirs->push_back (fpath);
	}
	return 0;
}
#endif

void GetFontPaths (std::vector<std::string> &paths)
{
	paths.clear();

	// paths should not be garbaged by the repetitive
	// content accumulation in the dirs vector
	gFontDirs->clear();

// Xbox and Wii do not have GetFolderContentsAtPath().
#if !UNITY_XENON && !UNITY_WII
#if UNITY_OSX
	gFontDirs->push_back ("/System/Library/Fonts");
	gFontDirs->push_back ("/Library/Fonts");
	string homeDir = getenv ("HOME");
	gFontDirs->push_back (homeDir + "/Library/Fonts");
	
#elif UNITY_WINRT
	gFontDirs->push_back ("C:\\Windows\\Fonts");
#elif UNITY_WIN && !UNITY_WINRT
	// It must be noted that Windows installation does not necessarily have to reside on C: disk
	std::string win_dir;
	win_dir.resize( MAX_PATH );
	UINT const dir_len = GetWindowsDirectoryA( &win_dir.front(), win_dir.size() );
	if( 0u == dir_len ) // The function has failed, so a default is as good as any other choice
	{
		gFontDirs->push_back ("C:\\Windows\\Fonts");
	}
	else
	{
		std::string::size_type old_win_dir_size = win_dir.size();
		win_dir.resize( dir_len );

		if ( dir_len > old_win_dir_size )
		{
			// Absolutely unlikely, but possible; in such a case where the previous buffer was not enough
			// to hold the path to the windows directory, we simply increase the size of the buffer
			// and try again to fetch the directory name.
			UINT const dir_len2 = GetWindowsDirectoryA( &win_dir.front(), win_dir.size() );
			if( (dir_len2 + 1u) == win_dir.size() )
			{
				win_dir.pop_back();		// Remove the embedded null terminator
			}
			else
			{
				win_dir = "C:\\Windows"; // Seriously screwed up
			}
		}

		gFontDirs->push_back( PlatformAppendPathName(win_dir, "Fonts") );
	}

#elif UNITY_LINUX
	ftw ("/usr/share/fonts", callback, 16);
#elif UNITY_ANDROID
	gFontDirs->push_back ("/system/fonts");
#elif UNITY_IPHONE
	#if TARGET_IPHONE_SIMULATOR
	gFontDirs->push_back ("/Library/Fonts");
	#else
	gFontDirs->push_back ("/System/Library/Fonts/Cache");
	#endif
#elif UNITY_TIZEN
	gFontDirs->push_back ("/usr/share/fonts");
	gFontDirs->push_back ("/usr/share/fallback_fonts");
#endif
	
	for(int i = 0; i < gFontDirs->size(); ++i)
	{
		std::set<std::string> dirPaths;
		if ( GetFolderContentsAtPath( (*gFontDirs)[i], dirPaths ) )
		{
			for (std::set<std::string>::iterator j = dirPaths.begin(); j != dirPaths.end(); j++)
			{
				std::string extension = GetPathNameExtension(*j);
				ToLowerInplace(extension);
				if (!StrCmp(extension.c_str(), "ttf") || !StrCmp(extension.c_str(), "ttc") || !StrCmp(extension.c_str(), "otf") || !StrCmp(extension.c_str(), "dfont"))
					paths.push_back(*j);
			}
		}
	}
#endif
}



static void InitFontMetadataPreset();
bool GetFontMetadataPreset(const std::string& name, std::string& family_name, std::string& style_name, unsigned& style_flags, unsigned& face_flags)
{
	if (gFontMetadata->empty())
	{
		InitFontMetadataPreset();
	}

	FontMetadataMap::iterator it = gFontMetadata->find(name);
	if (it != gFontMetadata->end())
	{
		family_name = it->second.family_name;
		style_name = it->second.style_name;
		style_flags = it->second.style_flags;
		face_flags = it->second.face_flags;
		return true;
	}

	return false;	
}

static void InitFontMetadataPreset()
{
#if UNITY_IPHONE
	// Reading font metada on iOS devices might take few seconds when missing OS cache, so keeping preset known font table there
	// iOS 4.3
	(*gFontMetadata)["AppleColorEmoji"] = (_FontInfo){"Apple Color Emoji", "Regular", 0x0, 0x19};
	(*gFontMetadata)["AppleGothic"] = (_FontInfo){"AppleGothic", "Regular", 0x0, 0x39};
	(*gFontMetadata)["Arial"] = (_FontInfo){"Arial", "Regular", 0x0, 0x59};
	(*gFontMetadata)["ArialBold"] = (_FontInfo){"Arial", "Bold", 0x2, 0x59};
	(*gFontMetadata)["ArialBoldItalic"] = (_FontInfo){"Arial", "Bold Italic", 0x3, 0x59};
	(*gFontMetadata)["ArialHB"] = (_FontInfo){"Arial Hebrew", "Regular", 0x0, 0x19};
	(*gFontMetadata)["ArialHBBold"] = (_FontInfo){"Arial Hebrew", "Bold", 0x2, 0x19};
	(*gFontMetadata)["ArialItalic"] = (_FontInfo){"Arial", "Italic", 0x1, 0x59};
	(*gFontMetadata)["ArialRoundedMTBold"] = (_FontInfo){"Arial Rounded MT Bold", "Regular", 0x0, 0x59};
	(*gFontMetadata)["BanglaSangamMN"] = (_FontInfo){"Bangla Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["CourierNew"] = (_FontInfo){"Courier New", "Regular", 0x0, 0x1f};
	(*gFontMetadata)["CourierNewBold"] = (_FontInfo){"Courier New", "Bold", 0x2, 0x1d};
	(*gFontMetadata)["CourierNewBoldItalic"] = (_FontInfo){"Courier New", "Bold Italic", 0x3, 0x1d};
	(*gFontMetadata)["CourierNewItalic"] = (_FontInfo){"Courier New", "Italic", 0x1, 0x1d};
	(*gFontMetadata)["DB_LCD_Temp-Black"] = (_FontInfo){"DB LCD Temp", "Black", 0x0, 0x19};
	(*gFontMetadata)["DevanagariSangamMN"] = (_FontInfo){"Devanagari Sangam MN", "Regular", 0x0, 0x59};
	(*gFontMetadata)["Fallback"] = (_FontInfo){".PhoneFallback", "Regular", 0x0, 0x19};
	(*gFontMetadata)["GeezaPro"] = (_FontInfo){"Geeza Pro", "Regular", 0x0, 0x19};
	(*gFontMetadata)["GeezaProBold"] = (_FontInfo){"Geeza Pro", "Bold", 0x0, 0x19};
	(*gFontMetadata)["Georgia"] = (_FontInfo){"Georgia", "Regular", 0x0, 0x19};
	(*gFontMetadata)["GeorgiaBold"] = (_FontInfo){"Georgia", "Bold", 0x2, 0x19};
	(*gFontMetadata)["GeorgiaBoldItalic"] = (_FontInfo){"Georgia", "Bold Italic", 0x3, 0x19};
	(*gFontMetadata)["GeorgiaItalic"] = (_FontInfo){"Georgia", "Italic", 0x1, 0x19};
	(*gFontMetadata)["GujaratiSangamMN"] = (_FontInfo){"Gujarati Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["GurmukhiMN"] = (_FontInfo){"Gurmukhi MN", "Regular", 0x0, 0x59};
	(*gFontMetadata)["HKGPW3UI"] = (_FontInfo){".HKGPW3UI", "Regular", 0x0, 0x59};
	(*gFontMetadata)["HiraginoKakuGothicProNW3"] = (_FontInfo){"Hiragino Kaku Gothic ProN", "W3", 0x0, 0x39};
	(*gFontMetadata)["HiraginoKakuGothicProNW6"] = (_FontInfo){"Hiragino Kaku Gothic ProN", "W6", 0x2, 0x39};
	(*gFontMetadata)["Kailasa"] = (_FontInfo){"Kailasa", "Bold", 0x0, 0x59};
	(*gFontMetadata)["KannadaSangamMN"] = (_FontInfo){"Kannada Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["LastResort"] = (_FontInfo){".LastResort", "Regular", 0x0, 0x19};
	(*gFontMetadata)["LockClock"] = (_FontInfo){".Lock Clock", "Light", 0x0, 0x59};
	(*gFontMetadata)["MalayalamSangamMN"] = (_FontInfo){"Malayalam Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["OriyaSangamMN"] = (_FontInfo){"Oriya Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["PhoneKeyCaps"] = (_FontInfo){".PhoneKeyCaps", "Regular", 0x2, 0x59};
	(*gFontMetadata)["PhoneKeyCapsTwo"] = (_FontInfo){".PhoneKeyCapsTwo", "Regular", 0x2, 0x59};
	(*gFontMetadata)["PhonepadTwo"] = (_FontInfo){".PhonepadTwo", "Regular", 0x0, 0x59};
	(*gFontMetadata)["STHeiti-Light"] = (_FontInfo){"Heiti TC", "Light", 0x0, 0x1b};
	(*gFontMetadata)["STHeiti-Medium"] = (_FontInfo){"Heiti TC", "Medium", 0x2, 0x19};
	(*gFontMetadata)["SinhalaSangamMN"] = (_FontInfo){"Sinhala Sangam MN", "Regular", 0x0, 0x59};
	(*gFontMetadata)["TamilSangamMN"] = (_FontInfo){"Tamil Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["TeluguSangamMN"] = (_FontInfo){"Telugu Sangam MN", "Regular", 0x0, 0x19};
	(*gFontMetadata)["Thonburi"] = (_FontInfo){"Thonburi", "Regular", 0x0, 0x19};
	(*gFontMetadata)["ThonburiBold"] = (_FontInfo){"Thonburi", "Bold", 0x2, 0x19};
	(*gFontMetadata)["TimesNewRoman"] = (_FontInfo){"Times New Roman", "Regular", 0x0, 0x59};
	(*gFontMetadata)["TimesNewRomanBold"] = (_FontInfo){"Times New Roman", "Bold", 0x2, 0x59};
	(*gFontMetadata)["TimesNewRomanBoldItalic"] = (_FontInfo){"Times New Roman", "Bold Italic", 0x3, 0x59};
	(*gFontMetadata)["TimesNewRomanItalic"] = (_FontInfo){"Times New Roman", "Italic", 0x1, 0x59};
	(*gFontMetadata)["TrebuchetMS"] = (_FontInfo){"Trebuchet MS", "Regular", 0x0, 0x59};
	(*gFontMetadata)["TrebuchetMSBold"] = (_FontInfo){"Trebuchet MS", "Bold", 0x2, 0x59};
	(*gFontMetadata)["TrebuchetMSBoldItalic"] = (_FontInfo){"Trebuchet MS", "Bold Italic", 0x3, 0x59};
	(*gFontMetadata)["TrebuchetMSItalic"] = (_FontInfo){"Trebuchet MS", "Italic", 0x1, 0x59};
	(*gFontMetadata)["Verdana"] = (_FontInfo){"Verdana", "Regular", 0x0, 0x59};
	(*gFontMetadata)["VerdanaBold"] = (_FontInfo){"Verdana", "Bold", 0x2, 0x19};
	(*gFontMetadata)["VerdanaBoldItalic"] = (_FontInfo){"Verdana", "Bold Italic", 0x3, 0x19};
	(*gFontMetadata)["VerdanaItalic"] = (_FontInfo){"Verdana", "Italic", 0x1, 0x19};
	(*gFontMetadata)["Zapfino"] = (_FontInfo){"Zapfino", "Regular", 0x1, 0x19};
	(*gFontMetadata)["_H_AmericanTypewriter"] = (_FontInfo){"American Typewriter", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_Baskerville"] = (_FontInfo){"Baskerville", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_ChalkboardSE"] = (_FontInfo){"Chalkboard SE", "Light", 0x0, 0x59};
	(*gFontMetadata)["_H_Cochin"] = (_FontInfo){"Cochin", "Regular", 0x0, 0x5b};
	(*gFontMetadata)["_H_Courier"] = (_FontInfo){"Courier", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_Futura"] = (_FontInfo){"Futura", "Medium", 0x0, 0x19};
	(*gFontMetadata)["_H_Helvetica"] = (_FontInfo){"Helvetica", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_HelveticaNeue"] = (_FontInfo){"Helvetica Neue", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_HelveticaNeueExtras"] = (_FontInfo){"Helvetica Neue", "Light", 0x0, 0x19};
	(*gFontMetadata)["_H_MarkerFeltThin"] = (_FontInfo){"Marker Felt", "Thin", 0x0, 0x59};
	(*gFontMetadata)["_H_MarkerFeltWide"] = (_FontInfo){"Marker Felt", "Wide", 0x2, 0x59};
	(*gFontMetadata)["_H_Noteworthy"] = (_FontInfo){"Noteworthy", "Light", 0x0, 0x59};
	(*gFontMetadata)["_H_Palatino"] = (_FontInfo){"Palatino", "Regular", 0x0, 0x59};
	(*gFontMetadata)["_H_SnellRoundhand"] = (_FontInfo){"Snell Roundhand", "Regular", 0x0, 0x59};
	(*gFontMetadata)["_H__PO_Bodoni-Ornaments"] = (_FontInfo){"Bodoni Ornaments", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_Bodoni72-Book-SmallCaps"] = (_FontInfo){"Bodoni 72 Smallcaps", "Book", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_Bodoni72-OldStyle"] = (_FontInfo){"Bodoni 72 Oldstyle", "Book", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_Bodoni72"] = (_FontInfo){"Bodoni 72", "Book", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_BradleyHand-Bold"] = (_FontInfo){"Bradley Hand", "Bold", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_Didot"] = (_FontInfo){"Didot", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_GillSans"] = (_FontInfo){"Gill Sans", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_HoeflerText"] = (_FontInfo){"Hoefler Text", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_Optima"] = (_FontInfo){"Optima", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_PartyLET"] = (_FontInfo){"Party LET", "Plain", 0x0, 0x19};
	(*gFontMetadata)["_H__PO_ZapfDingbats"] = (_FontInfo){"Zapf Dingbats", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_PO_AcademyEngraved"] = (_FontInfo){"Academy Engraved LET", "Plain", 0x0, 0x19};
	(*gFontMetadata)["_PO_Chalkduster"] = (_FontInfo){"Chalkduster", "Regular", 0x0, 0x59};
	(*gFontMetadata)["_PO_Copperplate"] = (_FontInfo){"Copperplate", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_PO_HiraginoMinchoProNW3"] = (_FontInfo){"Hiragino Mincho ProN", "W3", 0x0, 0x39};
	(*gFontMetadata)["_PO_HiraginoMinchoProNW6"] = (_FontInfo){"Hiragino Mincho ProN", "W6", 0x2, 0x39};
	(*gFontMetadata)["_PO_Papyrus"] = (_FontInfo){"Papyrus", "Regular", 0x0, 0x19};
// iOS 5.0
	(*gFontMetadata)["AcademyEngraved"] = (_FontInfo){"Academy Engraved LET", "Plain", 0x0, 0x19};
	(*gFontMetadata)["Chalkduster"] = (_FontInfo){"Chalkduster", "Regular", 0x0, 0x59};
	(*gFontMetadata)["Copperplate"] = (_FontInfo){"Copperplate", "Regular", 0x0, 0x19};
	(*gFontMetadata)["EuphemiaCAS"] = (_FontInfo){"Euphemia UCAS", "Regular", 0x0, 0x19};
	(*gFontMetadata)["HiraginoMinchoProNW3"] = (_FontInfo){"Hiragino Mincho ProN", "W3", 0x0, 0x39};
	(*gFontMetadata)["HiraginoMinchoProNW6"] = (_FontInfo){"Hiragino Mincho ProN", "W6", 0x2, 0x39};
	(*gFontMetadata)["Papyrus"] = (_FontInfo){"Papyrus", "Regular", 0x0, 0x19};
	(*gFontMetadata)["STFangsongCore"] = (_FontInfo){".STFangsongCore", "Regular", 0x0, 0x39};
	(*gFontMetadata)["STKaitiCore"] = (_FontInfo){".STKaitiCore", "Regular", 0x0, 0x39};
	(*gFontMetadata)["STSongCore"] = (_FontInfo){".STSongCore", "Regular", 0x0, 0x39};
	(*gFontMetadata)["_H_Bodoni-Ornaments"] = (_FontInfo){"Bodoni Ornaments", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_Bodoni72-Book-SmallCaps"] = (_FontInfo){"Bodoni 72 Smallcaps", "Book", 0x0, 0x19};
	(*gFontMetadata)["_H_Bodoni72-OldStyle"] = (_FontInfo){"Bodoni 72 Oldstyle", "Book", 0x0, 0x19};
	(*gFontMetadata)["_H_Bodoni72"] = (_FontInfo){"Bodoni 72", "Book", 0x0, 0x19};
	(*gFontMetadata)["_H_BradleyHand-Bold"] = (_FontInfo){"Bradley Hand", "Bold", 0x0, 0x19};
	(*gFontMetadata)["_H_Didot"] = (_FontInfo){"Didot", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_GillSans"] = (_FontInfo){"Gill Sans", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_HoeflerText"] = (_FontInfo){"Hoefler Text", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_Marion"] = (_FontInfo){"Marion", "Regular", 0x0, 0x59};
	(*gFontMetadata)["_H_Optima"] = (_FontInfo){"Optima", "Regular", 0x0, 0x19};
	(*gFontMetadata)["_H_PartyLET"] = (_FontInfo){"Party LET", "Plain", 0x0, 0x19};
	(*gFontMetadata)["_H_ZapfDingbats"] = (_FontInfo){"Zapf Dingbats", "Regular", 0x0, 0x19};
// iOS 6.0
	(*gFontMetadata)["AppleColorEmoji@2x"] = (_FontInfo){"Apple Color Emoji", "Regular", 0x0, 0x39};
	(*gFontMetadata)["AppleSDGothicNeoBold"] = (_FontInfo){"Apple SD Gothic Neo", "Bold", 0x0, 0x39};
	(*gFontMetadata)["AppleSDGothicNeoMedium"] = (_FontInfo){"Apple SD Gothic Neo", "Medium", 0x0, 0x39};
	(*gFontMetadata)["Symbol"] = (_FontInfo){"Symbol", "Regular", 0x0, 0x1b};
	(*gFontMetadata)["_H_Avenir"] = (_FontInfo){"Avenir", "Book", 0x0, 0x59};
	(*gFontMetadata)["_H_AvenirNext"] = (_FontInfo){"Avenir Next", "Bold", 0x2, 0x59};
	(*gFontMetadata)["_H_AvenirNextCondensed"] = (_FontInfo){"Avenir Next Condensed", "Bold", 0x2, 0x59};

#endif	
}

#endif
