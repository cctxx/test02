#ifndef DYNAMICFONTFREETYPE_H
#define DYNAMICFONTFREETYPE_H

#if DYNAMICFONTMODE == kDynamicFontModeFreeType

#include "External/freetype2/include/ft2build.h"
#include FT_FREETYPE_H
//#include FT_GLYPH_H

namespace DynamicFontMap
{
	void StaticInitialize();
	void StaticDestroy();
}

struct FontRef
{
	std::string family;
	unsigned int style;
	
	FontRef(const std::string& _family, unsigned int _style) : family(_family), style(_style) {}
	
	bool operator < (const FontRef& other) const {
		if (family < other.family)
			return true;
		else if (family > other.family)
			return false;
		return style < other.style;
	}
};

typedef std::map<FontRef, FT_Face> FaceMap;

struct DynamicFontData {
	DynamicFontData();
	~DynamicFontData();
	
	FT_Face GetFaceForFontRef (FontRef &r, FontFallbacks &fallbacks);
	FT_Face GetFaceForCharacter (FontNames &fonts, FontFallbacks &fallbacks, unsigned int style, unsigned int unicodeChar);
	FT_Face GetFaceForCharacterIfAvailableInFont (FontRef &r, FontFallbacks &fallbacks, unsigned int unicodeChar);
	
	FaceMap m_Faces; 
};

#endif

#endif
