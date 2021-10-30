#ifndef FONT_H
#define FONT_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Misc/UTF8.h"
#include "Runtime/IMGUI/TextFormatting.h"

class Font;

typedef UNITY_VECTOR(kMemFont,UnityStr) FontNames;
typedef UNITY_VECTOR (kMemFont, PPtr<Font> ) FontFallbacks;

#include "DynamicFontFreeType.h"

namespace Unity { class Material; }
using namespace Unity;

enum {
	kFontRenderingModeSmooth,
	kFontRenderingModeHintedSmooth,
	kFontRenderingModeHintedRaster,
	kFontRenderingModeOSDefault
};

class Texture;

class Font : public NamedObject
{
 public:
 
	struct KerningCompare : std::binary_function<std::pair<char, char>, std::pair<char, char>, std::size_t>
	{
		bool operator()(const std::pair<char, char> lhs, const std::pair<char, char> rhs) const
		{
			if (lhs.first != rhs.first)
				return lhs.first < rhs.first;
			else
				return lhs.second < rhs.second;
		}
	};

	// TrueType fonts: information for a character.
	struct CharacterInfo {
		unsigned int index;
		Rectf		uv;			///< UV coordinates for this glyph.
		Rectf		vert;		///< Rectangle for where to render the glyph.
		float		width;
		int			size;
		unsigned int style;
		unsigned int lastUsedInFrame;
		bool		flipped;
		DECLARE_SERIALIZE_NO_PPTR (CharacterInfo)
		
		CharacterInfo() :
		vert(0.0F,0.0F,0.0F,0.0F),
		uv(0.0F,0.0F,0.0F,0.0F),
		index (-1),
		width (0.0F),
		size (0),
		style (kStyleDefault),
		lastUsedInFrame (0),
		flipped (false)
		{  }
		
		friend bool operator < (const CharacterInfo& lhs, const CharacterInfo& rhs) 
		{
			if (lhs.index == rhs.index)
			{
				if (lhs.size < rhs.size)
					return true;
				else if (lhs.size > rhs.size)
					return false;
				return lhs.style < rhs.style;
			}
			return lhs.index < rhs.index; 
		}
	};	
	typedef UNITY_VECTOR(kMemFont,CharacterInfo) CharacterInfos;

 
 	REGISTER_DERIVED_CLASS (Font, NamedObject)
	DECLARE_OBJECT_SERIALIZE (Font)

	Font (MemLabelId label, ObjectCreationMode mode);
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Reset();

	static void InitializeClass ();
	static void CleanupClass ();

	/// Get the kerning of the font.
	/// Kerning is letter-spacing
	float GetKerning () const { return m_Kerning; }
	
	/// Get the uv & vertex info for unicode character charCode into 'verts' & 'uvs'
	void GetCharacterRenderInfo( unsigned int charCode, Rectf& verts, Rectf& uvs, bool &flipped ) const;
	void GetCharacterRenderInfo( unsigned int charCode, int size, unsigned int style, Rectf& verts, Rectf& uvs, bool &flipped ) const;
	

	/// Get the width of a character
	float GetCharacterWidth (unsigned int charCode, int size = 0, unsigned int style = kStyleDefault) const;
	
	/// Get the width of the tab character
	float GetTabWidth () const;
	
	/// Get the material of this font.
	PPtr<Material> GetMaterial () const { return m_DefaultMaterial; }
	
	/// Set the default material
	void SetMaterial (PPtr<Material> material) {m_DefaultMaterial = material;}
	
	/// Get the texture of this font... 
	PPtr<Texture> GetTexture () const { return m_Texture; }
	
	/// Set the texture
	void SetTexture(PPtr<Texture> texture) {m_Texture = texture;}
	
	/// Get the line spacing of this font
	float GetLineSpacing (int size = 0) const;

	/// Get the line spacing of this font
	void SetLineSpacing (float spacing) { m_LineSpacing = spacing; }

	/// Adds character info (only for non grid fonts)
	void AddCharacterInfoEntry( const Rectf& uv, const Rectf& vert, float width, int character, bool flipped, int size = 0, unsigned int style = kStyleDefault);
	
	/// Set Ascii Start Offset
	void SetAsciiStartOffset(short val) {m_AsciiStartOffset = val;}
	
	// Old "Grid fonts" where scaled differently when using non-pixel correct rendering then
	// normal fonts. So we need this to emulate the effect.
	float GetDeprecatedPixelScale () const { return m_PixelScale; }
	
	/// Set the Convert Case property
	enum { kDynamicFont = -2, kUnicodeSet = -1, kDontConvertCase = 0, kUpperCase = 1, kLowerCase = 2, kCustomSet = 3 };
	void SetConvertCase(int val) {m_ConvertCase = val;}
	int GetConvertCase() { return m_ConvertCase; }

	typedef vector_map<std::pair<UnicodeChar, UnicodeChar>, float, KerningCompare> KerningValues;
	/// Get the kerning values for modifying in place
	KerningValues &GetKerningValues() { return  m_KerningValues; }
	
	/// Set the font size, called by the truetype font importer
	void SetFontSize(int val) { m_FontSize = val; }
	
	/// Get the font size, used to find pixel height of the chars in the font
	int GetFontSize() const { return m_FontSize; }

	void SetAscent(float val) { m_Ascent = val; }
	float GetAscent() const { return m_Ascent; }
	
	void SetCharacterSpacing (int val) { m_CharacterSpacing = val; }
	void SetCharacterPadding (int val) { m_CharacterPadding = val; }
	
	/// Does this font have a definition for a specific character?
	bool HasCharacter (unsigned int unicodeChar, int size = 0, unsigned int style = kStyleDefault);

	bool HasCharacterInTexture (unsigned int unicodeChar, int size, unsigned int style);

	/// Dynamic Font stuff:
	bool CacheFontForText (UInt16 *chars, int length, int size = 0, unsigned int style = kStyleDefault, std::vector<TextFormatChange> format = std::vector<TextFormatChange>());

	FontNames &GetFontNames () { return m_FontNames; }
	FontFallbacks &GetFontFallbacks () { return m_FallbackFonts; }
	void SetFontNames (FontNames &names);

	UNITY_VECTOR(kMemFont,char) &GetFontData () { return m_FontData; }
	void SetFontDefaultStyle (int style) { m_DefaultStyle = style; }
	static void FrameComplete () { s_FrameCount++; }
	bool ResetCachedTexture ();

	const CharacterInfos &GetCharacterInfos() { return m_CharacterRects; }
	void SetCharacterInfos (CharacterInfos &infos) { m_CharacterRects = infos; CacheRects(); }
	
	void SetFontRenderingMode(int val) { m_FontRenderingMode = val; }
	int GetFontRenderingMode() const { return m_FontRenderingMode; }
#if UNITY_EDITOR
	void SetMinimalFontTextureSize (int size) { m_TexWidth = size; m_TexHeight = size; }
#endif
	
protected:

	void AddRectToCache(CharacterInfo& info);
	void CacheRects();

	/// Helper: Get the glyph code from a character, remapping cases, etc...
	unsigned int GetGlyphNo( unsigned int charCode ) const;

	// The kerning map
	KerningValues m_KerningValues;
	
	// These are only used in grid fonts
	typedef std::pair<int, float> IntFloatPair;
	typedef UNITY_VECTOR(kMemFont,IntFloatPair) PerCharacterKerning;

	float m_Kerning;///< Kerning of space between characters (Smaller than 1.0 pulls them together, Larger pushes them out)
	float m_LineSpacing; ///< Spacing between lines as multiplum of height of a character.
	int   m_CharacterSpacing;
	int	  m_CharacterPadding;
	int   m_AsciiStartOffset; ///< What is the first ascii character in the texture.
	int   m_FontSize;

	int   m_ConvertCase; ///< enum { Don't change case, Convert to upper case characters, Convert to lower case characters }
	PPtr<Material> m_DefaultMaterial;
	PPtr<Texture> m_Texture;
	
	// Legacy Grid font support
	float m_PixelScale;

	struct TexturePosition {
		int x, y;
		
		TexturePosition (int _x, int _y) : x(_x), y(_y) {}

		friend bool operator < (const TexturePosition& lhs, const TexturePosition& rhs) 
		{
			if (lhs.x + lhs.y != rhs.x + rhs.y)
				return lhs.x + lhs.y < rhs.x + rhs.y; 
			else
				return lhs.x < rhs.x;
		}
	};

	struct IntRect {
		int x, y, width, height;
		
		IntRect (int _x, int _y, int _width, int _height) : x(_x), y(_y), width(_width), height(_height) {}
		
		inline bool Intersects (const IntRect &r) const
		{
			return r.x+r.width > x && r.y+r.height > y && r.x < x + width && r.y < y + height;
		}
	};
					
	UNITY_VECTOR(kMemFont,CharacterInfo)		m_CharacterRects;
	vector_set<CharacterInfo>		m_UnicodeCharacterRects;
	UNITY_VECTOR(kMemFont,CharacterInfo)		m_AsciiCharacterRects;

	// dynamic font stuff:
	void ResetPackingData ();
	void GrowTexture (int maxFontSize);
	UInt16 *CollectAllUsedCharacters (UInt16 *chars, int &length, int *&sizes, unsigned int *&styles);
	void SetupDynamicFont ();
	bool HasCharacterDynamic (unsigned int unicodeChar);

	bool AddCharacterToTexture (unsigned int unicodeChar, int size, unsigned int style);
	bool IsRectFree (const IntRect &r) const;
	UInt8 *GetCharacterBitmap(unsigned int &charWidth, unsigned int &charHeight, unsigned int &bufferWidth, Rectf &vert, float &advance, unsigned int unicodeChar, int size, unsigned int style);

	UNITY_VECTOR(kMemFont,char) m_FontData;
	FontNames m_FontNames;
	FontFallbacks m_FallbackFonts;
	UNITY_VECTOR(kMemFont,IntRect) m_IntRects;
	UNITY_SET(kMemFont,TexturePosition) m_TexturePositions;
	UNITY_SET(kMemFont,TexturePosition)::iterator m_TexturePositionsSearchPosition;
		
	unsigned int m_TexWidth;
	unsigned int m_TexHeight;
	unsigned int m_TexMargin;
	unsigned int m_SubImageSize;
	unsigned int m_SubImageIndex;
	static unsigned int s_FrameCount;
	unsigned int m_DefaultStyle;
	float m_Ascent;
	int m_FontRenderingMode;
				
	DynamicFontData m_DynamicData;
	
	friend struct DynamicFontData;
};

namespace GetFontsManager
{
	void StaticInitialize();
	void StaticDestroy();
}

void GetFontPaths (std::vector<std::string> &paths);
FontNames &GetFallbacks ();
bool GetFontMetadataPreset(const std::string& name, std::string& family_name, std::string& style_name, unsigned& style_flags, unsigned& face_flags);


struct ScriptingCharacterInfo 
{
	int index;
	Rectf uv, vert;
	float width;
	int size, style;
	bool flipped;

	void CopyFrom(const Font::CharacterInfo& inData)
	{
		index = inData.index;
		uv = inData.uv;
		vert = inData.vert;
		width = inData.width;
		size = inData.size;
		style = inData.style;
		flipped = inData.flipped;
	}
	void CopyTo(Font::CharacterInfo& outData)
	{
		outData.index = index;
		outData.uv = uv;
		outData.vert = vert;
		outData.width = width;
		outData.size = size;
		outData.style = style;
		outData.flipped = flipped;
	}
};

#endif
