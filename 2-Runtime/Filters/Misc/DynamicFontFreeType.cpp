#include "UnityPrefix.h"
#include "Font.h"

#if UNITY_WIN
#include "PlatformDependent/Win/Registry.h"
#endif

#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Misc/ResourceManager.h"
#include FT_BITMAP_H


DynamicFontData::DynamicFontData()
{
}

DynamicFontData::~DynamicFontData ()
{
	for (FaceMap::iterator i=m_Faces.begin(); i!=m_Faces.end(); i++)
		FT_Done_Face(i->second);
}

static FT_Library g_ftLib;
static bool g_ftLibInit = false;
static FT_Bitmap g_bitmap8bpp;
static bool g_bitmap8bppInit = false;

struct OSFont
{
	OSFont () :
		index(0)
	{}

	OSFont (std::string _path, int _index) :
		path(_path),
		index(_index)
	{}
	
	bool operator != (const OSFont& other) const
	{
		return index != other.index || path != other.path;
	}
	
	std::string path;
	int index;
};
typedef std::map<FontRef,OSFont> OSFontMap;
static OSFontMap* gOSFontMap = NULL; // Maps family names to files


namespace DynamicFontMap
{
	void StaticInitialize ()
	{
		gOSFontMap = UNITY_NEW (OSFontMap, kMemFont);
	}
	void StaticDestroy ()
	{
		UNITY_DELETE (gOSFontMap, kMemFont);
	}
}
static float ConvertFixed26(long val)
{
	float f = (float)(val >> 6);
	val = val & 0x3F;
	f += (float)val/(2^6 - 1);
	return f;
}

static float ConvertFixed16(long val)
{
	float f = (float)(val >> 16);
	val = val & 0xFFFF;
	f += (float)val/(2^16 - 1);
	return f;
}

static long ConvertFloat26(float val)
{
	return (long)(val * (1 << 6));
}

static long ConvertFloat16(float val)
{
	return (long)(val * (1 << 16));
}

static unsigned int FreeTypeStyleToUnity (int style_flags)
{
	unsigned int style = 0;
	if (style_flags & FT_STYLE_FLAG_ITALIC)
		style |= kStyleFlagItalic;
	if (style_flags & FT_STYLE_FLAG_BOLD)
		style |= kStyleFlagBold;
	return style;
}

static bool IsExpectedFontStyle(const char* style_name, unsigned int style)
{
	switch (style)
	{
		case kStyleDefault:
			return (strcmp(style_name, "Regular") == 0);
		case kStyleFlagItalic:
			return (strcmp(style_name, "Italic") == 0);
		case kStyleFlagBold:
			return (strcmp(style_name, "Bold") == 0);
		case kStyleFlagBold | kStyleFlagItalic:
			return (strcmp(style_name, "Bold Italic") == 0);
	}
	return false;
}

bool GetFontMetadata(const std::string& path, std::string& family_name, std::string& style_name, unsigned& style_flags, unsigned& face_flags, int *faceIndex)
{
	std::string shortName = GetFileNameWithoutExtension(path);
	
	// Check, maybe we already have font metadata in our preset table
	if (GetFontMetadataPreset(shortName, family_name, style_name, style_flags, face_flags))
		return true;
	
	bool res = false;
	FT_Face face;
	if(FT_New_Face(g_ftLib, path.c_str(), *faceIndex, &face) == 0)
	{
		*faceIndex = face->num_faces;
		if(face->family_name != NULL)
		{
			//Uncomment this line when font metadata needs to be rebuilt for newer OS/hw versions
			//printf_console("\tgFontMetadata[\"%s\"] = (_FontInfo){\"%s\", \"%s\", 0x%x, 0x%x};\n", shortName.c_str(), face->family_name, face->style_name, face->style_flags, face->face_flags);
			
			family_name = face->family_name;
			style_name = face->style_name;
			style_flags = face->style_flags;
			face_flags = face->face_flags;
			res = true;
		}
		
		FT_Done_Face(face);
	}
	
	return res;
}

void ReadFontFiles()
{
	DynamicFontMap::StaticInitialize();

	std::vector<std::string> paths;
	GetFontPaths(paths);

	for(int folderIndex = 0; folderIndex < paths.size(); ++folderIndex)
	{
		
		std::string &path = paths[folderIndex];
		
		std::string family_name, style_name;
		unsigned style_flags, face_flags;
		int numFaces = 1;
		for (int fontIndex=0; fontIndex<numFaces; fontIndex++)
		{
			int faceIndex = fontIndex;
			if (GetFontMetadata(path, family_name, style_name, style_flags, face_flags, &faceIndex))
			{
				numFaces = faceIndex;
				if((face_flags & FT_FACE_FLAG_SCALABLE) == 0)
					continue;
				
				FontRef r(family_name.c_str(), FreeTypeStyleToUnity(style_flags));
				OSFont font (path, fontIndex);
				OSFontMap::iterator i = gOSFontMap->find(r);
				if (i != gOSFontMap->end())
				{
					// There already is a font for this family and style
					if (i->second != font)
					{
						// Try to pick the one with the expected style name.
						// This way we don't accidentally catch fonts like "Arial Black" when we want "Arial",
						// as both will show with family_name=="Arial" and style_flags==0. In such cases, pick
						// the one with the style name matching what we'd expect from the style flags, if possible.
						if (IsExpectedFontStyle(style_name.c_str(), r.style))
							(*gOSFontMap)[r] = font;
					}
				}
				else
					(*gOSFontMap)[r] = font;
			}
		}
	}
}

static OSFont SelectFont(FontRef &r)
{
	// Initialize font map.  Do this on demand here
	// as it takes a significant amount of time to go through
	// all of the system's fonts.
	if (!gOSFontMap)
	{
		ReadFontFiles();
		Assert (gOSFontMap);
	}

	OSFontMap::iterator i = gOSFontMap->find(r);
	if (i != gOSFontMap->end())
		return i->second;
	return OSFont();
}

FT_Face DynamicFontData::GetFaceForFontRef (FontRef &r, FontFallbacks &fallbacks)
{
	FaceMap::iterator i = m_Faces.find(r);
	if (i != m_Faces.end())
		return (i->second);
	else
	{
		// First see if any of the fallback fonts in the project contain the needed font.
		for (FontFallbacks::iterator j=fallbacks.begin(); j != fallbacks.end(); j++)
		{
			if (j->IsValid())
			{
				i = (**j).m_DynamicData.m_Faces.find(r);
				if (i != (**j).m_DynamicData.m_Faces.end())
					return (i->second);
			}
		}
			
		// Then look at the system font files.
		OSFont font = SelectFont(r);
		if (!font.path.empty())
		{
			FT_New_Face(g_ftLib, font.path.c_str(), font.index, &m_Faces[r]);
			return m_Faces[r];
		}
	}
	return NULL;
}

FT_Face DynamicFontData::GetFaceForCharacterIfAvailableInFont (FontRef &r, FontFallbacks &fallbacks, unsigned int unicodeChar)
{
	// Do we have a font for the requested name and style which has the character?
	FT_Face face = GetFaceForFontRef(r, fallbacks);
	if (face != NULL)
	{
		if (FT_Get_Char_Index(face, unicodeChar))
			return face;
	}

	// If not, try with default style (if we had something else requested).
	if (r.style)
	{
		FontRef r2 = r;
		r2.style = 0;
		face = GetFaceForFontRef(r2, fallbacks);
		if (face != NULL)
		{
			if (FT_Get_Char_Index(face, unicodeChar))
				return face;
		}
	}
	return NULL;
}

FT_Face DynamicFontData::GetFaceForCharacter (FontNames &fonts, FontFallbacks &fallbacks, unsigned int style, unsigned int unicodeChar)
{
	// Check if any of the fonts in the fallback names serialized with the font has the character
	for (FontNames::iterator font = fonts.begin();font != fonts.end();font++)
	{
		// First check if we find the font by it's full name (as that
		// is what's used for the embedded ttf data, if any)
		unsigned int st = style;
		std::string name = *font;
		FontRef r (name, st);
		FT_Face face = GetFaceForCharacterIfAvailableInFont (r, fallbacks, unicodeChar);
		if (face != NULL)
			return face;

		// If that did not find anything, remove and parse potential style names, as OS fonts
		// are identified by family name and style flags.
		size_t pos = name.find(" Bold");
		if (pos != std::string::npos)
		{
			name = name.substr(0,pos)+name.substr(pos+5);
			st |= kStyleFlagBold;
		}
		pos = name.find(" Italic");
		if (pos != std::string::npos)
		{
			name = name.substr(0,pos)+name.substr(pos+7);
			st |= kStyleFlagItalic;
		}
		r = FontRef(name, st);
		face = GetFaceForCharacterIfAvailableInFont (r, fallbacks, unicodeChar);
		if (face != NULL)
			return face;
	}
	// If not, fall back to the global fallbacks.
	FontNames &globalFallbacks = GetFallbacks();
	for (FontNames::iterator font = globalFallbacks.begin();font != globalFallbacks.end();font++)
	{
		FontRef r (*font, style);
		FT_Face face = GetFaceForCharacterIfAvailableInFont (r, fallbacks, unicodeChar);
		if (face != NULL)
			return face;
	}
	
	return NULL;
}

bool Font::HasCharacterDynamic (unsigned int unicodeChar)
{
	return unicodeChar >= 32;
}

int GetLoadTarget (int fontsize, int mode)
{
	switch (mode)
	{
		case kFontRenderingModeSmooth:
			return FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
		case kFontRenderingModeHintedSmooth:
			return FT_LOAD_TARGET_NORMAL;
		case kFontRenderingModeHintedRaster:
			return FT_LOAD_TARGET_MONO;
		case kFontRenderingModeOSDefault:
		#if UNITY_WINRT
			return FT_LOAD_TARGET_NORMAL;
		#elif UNITY_WIN
			{
				static bool smoothing = registry::getString( "Control Panel\\Desktop", "FontSmoothing", "2" ) == "2";
				if (smoothing)
					return FT_LOAD_TARGET_NORMAL;
				else
					return FT_LOAD_TARGET_MONO;
			}
		#elif UNITY_OSX
			static int antiAliasingTreshold = -1;
			if (antiAliasingTreshold == -1)
			{
				Boolean exists;
				antiAliasingTreshold = CFPreferencesGetAppIntegerValue(CFSTR("AppleAntiAliasingThreshold"),CFSTR("Apple Global Domain"),&exists);
				if (!exists)
					antiAliasingTreshold = 4;
			}
			if (fontsize <= antiAliasingTreshold)
				return FT_LOAD_TARGET_MONO;
			else
				return FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
		#else
			return FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
		#endif
		default:
			ErrorString("Unknown font rendering mode.");
			return FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
	}
}

UInt8 *Font::GetCharacterBitmap(unsigned int &charWidth, unsigned int &charHeight, unsigned int &bufferWidth, Rectf &vert, float &advance, unsigned int unicodeChar, int size, unsigned int style)
{
	if(size == 0)
		size = m_FontSize;

	FT_Face face = m_DynamicData.GetFaceForCharacter(m_FontNames, m_FallbackFonts, style, unicodeChar);
	if (face == NULL)
	{
		// If we don't find a fallback Font in the OS, try built-in default font.
		// Needed for Platforms without access to OS fonts (like NaCl, which has no file system access).
		Font *builtinFont = GetBuiltinResource<Font> (kDefaultFontName);
		if (builtinFont)
			face = builtinFont->m_DynamicData.GetFaceForCharacter(m_FontNames, builtinFont->m_FallbackFonts, style, unicodeChar);

		if (face == NULL)
			return NULL;
	}

	unsigned int faceStyle = FreeTypeStyleToUnity(face->style_flags);
	// Perform transformations needed for bold/italic styles if the font does not natively support it.
	FT_Matrix m;
	if (!(faceStyle & kStyleFlagBold) && style & kStyleFlagBold)
		m.xx = ConvertFloat16(1.25f);
	else
		m.xx = ConvertFloat16(1);
	if (!(faceStyle & kStyleFlagItalic) && style & kStyleFlagItalic)
		m.xy = ConvertFloat16(0.25f);
	else
		m.xy = ConvertFloat16(0);
	m.yy = ConvertFloat16(1);
	m.yx = ConvertFloat16(0);
	FT_Set_Transform(face, &m, NULL);
	
	FT_Set_Char_Size(face, 0, ConvertFloat26(size), 72, 72);

	FT_UInt glyph = FT_Get_Char_Index(face, unicodeChar);
	if(glyph != 0)
	{
		int loadTarget = FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
		loadTarget = GetLoadTarget(size, m_FontRenderingMode);
		if( FT_Load_Glyph(face, glyph, loadTarget ) == 0 )
		{
			bool glyphRendered = true;
			if(face->glyph->format != FT_GLYPH_FORMAT_BITMAP )
				glyphRendered = ( FT_Render_Glyph(face->glyph, FT_LOAD_TARGET_MODE(loadTarget) ) == 0 );

			if(glyphRendered)
			{
				FT_Bitmap *srcBitmap = 0;
				FT_Bitmap& bitmap =face->glyph->bitmap;

				if(bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
				{
					if(!g_bitmap8bppInit)
					{
						FT_Bitmap_New(&g_bitmap8bpp);
						g_bitmap8bppInit = true;
					}
					FT_Bitmap_Convert(g_ftLib, &bitmap, &g_bitmap8bpp, 4);
					srcBitmap = &g_bitmap8bpp;
					if (srcBitmap->num_grays != 256)
					{
						float factor = 1.0f/(srcBitmap->num_grays-1) * 255;
						for (int i=0; i<srcBitmap->pitch*srcBitmap->rows; i++)
							srcBitmap->buffer[i] *= factor;
					}
				}
				else
				{
					srcBitmap = &bitmap;
				}

				charWidth = srcBitmap->width;
				charHeight = srcBitmap->rows;
				bufferWidth = srcBitmap->pitch;
				vert = Rectf(face->glyph->bitmap_left,
							face->glyph->bitmap_top - m_Ascent,
							 (float)charWidth,
							 -(float)charHeight);
				advance = Roundf(face->glyph->metrics.horiAdvance / 64.0);

				if (srcBitmap->width*srcBitmap->rows == 0)
					return NULL;
				
				return srcBitmap->buffer;
			}
		}
	}

	return NULL;
}

void Font::SetupDynamicFont ()
{
	Assert(g_ftLibInit);
	
	if(!m_FontData.empty())
	{
		FT_Face face = NULL;
		if(FT_New_Memory_Face(g_ftLib, (const FT_Byte*)&m_FontData[0], m_FontData.size(), 0, &face) != 0)
		{
			ErrorString("Failed to load font from memory");
		}
		else
		{
			// So we don't crash if we have a font where FreeType does not understand the name.
			// Unity 4.x will refuse to import such fonts, but it can happen with content made with 3.x,
			// which did not use FT to import the font.
			if (face->family_name == NULL)
				face->family_name = (FT_String*)"Unreadeable font name.";

			// Make sure the name of the memory font is the first in the list of fonts to use.
			if (strcmp(m_FontNames[0].c_str(), face->family_name) != 0)
				m_FontNames.insert(m_FontNames.begin(), face->family_name);

			FontRef r (face->family_name, FreeTypeStyleToUnity(face->style_flags));
			m_DynamicData.m_Faces[r] = face;
			if (r.style != 0)
			{
				r.style = 0;
				if (FT_New_Memory_Face(g_ftLib, (const FT_Byte*)&m_FontData[0], m_FontData.size(), 0, &face) == 0)
					m_DynamicData.m_Faces[r] = face;
			}
		}
	}
}

void Font::InitializeClass()
{
	GetFontsManager::StaticInitialize();
	if(FT_Init_FreeType( &g_ftLib ) != 0)
	{
		ErrorString("Could not initialize FreeType");
	}
	g_ftLibInit = true;
}

void Font::CleanupClass()
{
	if(g_bitmap8bppInit)
	{
		FT_Bitmap_Done(g_ftLib, &g_bitmap8bpp);
		g_bitmap8bppInit = false;
	}
	if(g_ftLibInit)
	{
		FT_Done_FreeType(g_ftLib);
		g_ftLibInit = false;
	}
	DynamicFontMap::StaticDestroy();
	GetFontsManager::StaticDestroy();
}

