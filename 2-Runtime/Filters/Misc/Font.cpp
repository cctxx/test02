#include "UnityPrefix.h"
#include "Font.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Shaders/Material.h"
#include "TextMesh.h"
#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/TrueTypeFontImporter.h"
#endif
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Scripting.h"

using namespace std;

// on android we can recreate gles context and loose gpu-side texture copy
// on editor we want to have it to write it later on build
#define NEEDS_SYSTEM_MEM_COPY UNITY_ANDROID || UNITY_EDITOR

PROFILER_INFORMATION(gFontTextureCacheProfile, "Font.CacheFontForText", kProfilerRender)

unsigned int Font::s_FrameCount = 0;

Font::Font(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
	// dynamic font stuff
	m_TexWidth = 256;
	m_TexHeight = 256;
	m_SubImageSize = 1;
	m_CharacterSpacing = 1;
	m_CharacterPadding = 0;
	m_FontSize = 0;
	m_Ascent = 0.0f;
	m_DefaultStyle = kStyleDefault;
	m_FontRenderingMode = kFontRenderingModeSmooth;
	
	m_PixelScale = 0.1f;

	m_AsciiCharacterRects.clear();
	m_AsciiCharacterRects.resize (256);

	m_TexturePositions.insert (TexturePosition (0, 0));
	m_TexturePositionsSearchPosition = m_TexturePositions.begin();
}

Font::~Font ()
{
}

void Font::Reset ()
{
	Super::Reset();

	m_Kerning = 1.0F;
	m_LineSpacing = 0.1F;
	m_AsciiStartOffset = 0;
	m_ConvertCase = 0;
}

unsigned int Font::GetGlyphNo (unsigned int charCode) const
{
	if (m_ConvertCase == kUpperCase)
		return ToUpper ((char)charCode) - m_AsciiStartOffset;
	else if (m_ConvertCase == kLowerCase)
		return ToLower ((char)charCode) - m_AsciiStartOffset;
	else
		return charCode - m_AsciiStartOffset;
}

void Font::GetCharacterRenderInfo( unsigned int charCode, Rectf& verts, Rectf& uvs, bool &flipped ) const
{
	GetCharacterRenderInfo(charCode, 0, 0, verts, uvs, flipped);
}

void Font::GetCharacterRenderInfo( unsigned int charCode, int size, unsigned int style, Rectf& verts, Rectf& uvs, bool &flipped ) const
{
	unsigned int charNo = GetGlyphNo (charCode);
	if (size == m_FontSize)
		size = 0;

	if (m_ConvertCase != kDynamicFont && (size != 0 || style != kStyleDefault))
	{
		ErrorString ("Font size and style overrides are only supported for dynamic fonts.");
		size  = 0;
		style = kStyleDefault;
	}

	if (charNo < 256 && size == 0 && style == kStyleDefault)
	{
		verts = m_AsciiCharacterRects[charNo].vert;
		uvs = m_AsciiCharacterRects[charNo].uv;
		flipped = m_AsciiCharacterRects[charNo].flipped;
	}
	else
	{
		CharacterInfo proxy;
		proxy.index = charNo;
		proxy.size = size;
		proxy.style = style;
		vector_set<CharacterInfo>::const_iterator found = m_UnicodeCharacterRects.find(proxy);
		if (found != m_UnicodeCharacterRects.end())
		{
			verts = found->vert;
			uvs = found->uv;
			flipped = found->flipped;
		}
		else
		{
			verts = Rectf( 0, 0, 0, 0 );
			uvs = Rectf( 0, 0, 0, 0 );
			flipped = false;
		}
	}
}

/// Does this font have a definition for a specific character?
bool Font::HasCharacterInTexture (unsigned int unicodeChar, int size, unsigned int style)
{
	unsigned int charNo = GetGlyphNo (unicodeChar);
	if (size == m_FontSize)
		size = 0;

	if (m_ConvertCase != kDynamicFont && (size != 0 || style != kStyleDefault))
	{
		ErrorString ("Font size and style overrides are only supported for dynamic fonts.");
		size  = 0;
		style = kStyleDefault;
	}

	// This uses the character advancement - We're making the assumption that all characters have a width
	if (charNo < 256 && size == 0 && style == kStyleDefault)
	{
		if (m_AsciiCharacterRects[charNo].width != 0.0f)
		{
			m_AsciiCharacterRects[charNo].lastUsedInFrame = s_FrameCount;
			return true;
		}
	}

	CharacterInfo proxy;
	proxy.index = charNo;
	proxy.size = size;
	proxy.style = style;
	vector_set<CharacterInfo>::iterator found = m_UnicodeCharacterRects.find(proxy);
	if (found != m_UnicodeCharacterRects.end())
	{
		found->lastUsedInFrame = s_FrameCount;
		return true;
	}
	return false;
}

bool Font::HasCharacter (unsigned int unicodeChar, int size, unsigned int style)
{
	if (m_ConvertCase == kDynamicFont)
		return HasCharacterDynamic (unicodeChar);
	else
		return HasCharacterInTexture (unicodeChar, size, style);
}

float Font::GetCharacterWidth( unsigned int charCode, int size, unsigned int style ) const
{
	if (size == m_FontSize)
		size = 0;

	if (m_ConvertCase != kDynamicFont && (size != 0 || style != kStyleDefault))
	{
		ErrorString ("Font size and style overrides are only supported for dynamic fonts.");
		size  = 0;
		style = kStyleDefault;
	}

	unsigned int charNo = GetGlyphNo (charCode);
	if (charNo < 256 && size == 0 && style == kStyleDefault)
		return m_AsciiCharacterRects[charNo].width * m_Kerning;
	else
	{
		CharacterInfo proxy;
		proxy.index = charNo;
		proxy.size = size;
		proxy.style = style;
		vector_set<CharacterInfo>::const_iterator found = m_UnicodeCharacterRects.find(proxy);
		if (found != m_UnicodeCharacterRects.end())
			return found->width * m_Kerning;
		else
			return 0.0F;
	}
}

float Font::GetTabWidth() const
{
	return GetCharacterWidth(' ');
}

template<class TransferFunction> inline
void Font::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (4);

	TRANSFER(m_AsciiStartOffset);
	TRANSFER(m_Kerning);
	TRANSFER(m_LineSpacing);
	TRANSFER(m_CharacterSpacing);
	TRANSFER(m_CharacterPadding);
	TRANSFER(m_ConvertCase);
	TRANSFER(m_DefaultMaterial);
	
	if (m_ConvertCase != kDynamicFont)
	{
		transfer.Transfer(m_CharacterRects, "m_CharacterRects");
	}
	else
	{
		// These are generated dynamically for dynamic fonts.
		UNITY_TEMP_VECTOR(CharacterInfo) emptyCharacterInfo;
		transfer.Transfer(emptyCharacterInfo, "m_CharacterRects");
	}

	transfer.Transfer(m_Texture, "m_Texture", kHideInEditorMask);

	transfer.Transfer(m_KerningValues, "m_KerningValues", kHideInEditorMask);

	// In version 1.5.0 line spacing is multiplicative instead of additive
	if (transfer.IsOldVersion(1))
	{
		m_LineSpacing = 1.0F + m_LineSpacing;
	}

	transfer.Transfer(m_PixelScale, "m_PixelScale", kHideInEditorMask);

	// Legacy Grid Font support
	if (transfer.IsVersionSmallerOrEqual(3))
	{
		bool gridFont;
		transfer.Transfer(gridFont, "m_GridFont");

		if (gridFont)
		{
			int fontCountX;
			int fontCountY;
			transfer.Transfer(fontCountX, "m_FontCountX");
			transfer.Transfer(fontCountY, "m_FontCountY");

			m_PixelScale = -fontCountX;

			PerCharacterKerning perCharacterKerning;

			transfer.Transfer(perCharacterKerning, "m_PerCharacterKerning");

			for (int charNo=0; charNo< fontCountX*fontCountY; charNo++ )
			{
				CharacterInfo info;
				info.index = charNo;

				info.vert = Rectf(0.0F, 0.0F, 1.0, -1.0);
				short charCol = charNo % fontCountX;
				short charRow = charNo / fontCountX;
				float charUVSizeX = 1.0F / (float)fontCountX;
				float charUVSizeY = 1.0F / (float)fontCountY;
				Vector2f charUVOffset = Vector2f((float)charCol * charUVSizeX, (float)charRow * charUVSizeY);
				info.uv = MinMaxRect ( charUVOffset.x, 1.0F - charUVOffset.y - charUVSizeY, charUVOffset.x + charUVSizeX, 1.0F - charUVOffset.y );
				info.width = 1.0;
				for (PerCharacterKerning::iterator i=perCharacterKerning.begin ();i != perCharacterKerning.end ();i++)
				{
					if (i->first - m_AsciiStartOffset == charNo)
						info.width = i->second;
				}
				m_CharacterRects.push_back(info);
			}
		}
	}

	transfer.Align();
	transfer.Transfer(m_FontData, "m_FontData", kHideInEditorMask);
	transfer.Align();
	float fontSize = m_FontSize;
	transfer.Transfer(fontSize, "m_FontSize", kHideInEditorMask);
	m_FontSize = (int)fontSize;
	transfer.Transfer(m_Ascent, "m_Ascent", kHideInEditorMask);
	transfer.Transfer(m_DefaultStyle, "m_DefaultStyle", kHideInEditorMask);
	transfer.Transfer(m_FontNames, "m_FontNames", kHideInEditorMask);

#if UNITY_EDITOR
	if (transfer.IsWritingGameReleaseData ())
	{
		// Make sure we have references to all other fonts in the project
		// which may be used as fallbacks in the build.
		TrueTypeFontImporter::GetFallbackFontReferences (this);
	}
#endif

	transfer.Transfer(m_FallbackFonts, "m_FallbackFonts", kHideInEditorMask);
	transfer.Align();
	TRANSFER(m_FontRenderingMode);
}

template<class TransferFunction> inline
void Font::CharacterInfo::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion(2);
	TRANSFER(index);
	TRANSFER(uv);
	TRANSFER(vert);
	TRANSFER(width);
	TRANSFER(flipped);
	transfer.Align();
	if( !transfer.IsCurrentVersion() )
		width = vert.Width();
}

void ApplyToMeshes ()
{
	vector<TextMesh*> meshes;
	Object::FindObjectsOfType (&meshes);
	for (int i=0;i<meshes.size ();i++)
	{
		meshes[i]->ApplyToMesh ();
	}
}

void Font::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

#if UNITY_EDITOR
	// Make sure we have references to all other fonts in the project
	// which may be used as fallbacks in the build.
	TrueTypeFontImporter::GetFallbackFontReferences (this);

	if ((awakeMode & kDidLoadFromDisk) && (m_ConvertCase == kDynamicFont) && !m_Texture.IsValid())
		ErrorStringObject(Format("Font texture for dynamic font %s is missing. Please reimport the Font. All dynamic fonts created with earlier Unity 3.0 betas need to be reimported.", GetName()), this);
#endif

	// m_PixelScale is set to -fontCountX for legacy gridfonts
	if(m_PixelScale < 0.f)
	{
		// Load related material or texture to apply the scaling to the CharacterRects
		Texture *tex = GetTexture ();
		if (!tex) {
			Material *mat = GetMaterial();
			if (mat)
				tex = mat->GetTexture (ShaderLab::Property("_MainTex"));
		}
		if (tex)
			m_PixelScale = -m_PixelScale / tex->GetDataWidth ();
		else
			m_PixelScale = 1.0f;
		
		for (int charNo=0; charNo< m_CharacterRects.size(); charNo++ )
		{
			CharacterInfo& info = m_CharacterRects[charNo];
			info.vert = Rectf(0.0F, 0.0F, 1.0/m_PixelScale, -1.0/m_PixelScale);
			info.width /= m_PixelScale;
		}
		m_LineSpacing /= m_PixelScale;
	}

	CacheRects();

	if (m_ConvertCase == kDynamicFont)
	{
		if (m_FontNames.empty())
		{
			ErrorString ("Font does not contain font names!");
			m_FontNames.push_back("Arial");
		}
		SetupDynamicFont ();
		ResetCachedTexture();
	}

	if ((awakeMode & kDidLoadFromDisk) == 0)
		ApplyToMeshes();
}

void Font::AddCharacterInfoEntry( const Rectf& uv, const Rectf& vert, float width, int character, bool flipped, int size, unsigned int style)
{
	character -= m_AsciiStartOffset;
	AssertIf( character < 0 );

	CharacterInfo inf;
	inf.uv = uv;
	inf.vert = vert;
	inf.width = width;
	inf.index = character;
	if (size == m_FontSize)
		inf.size = 0;
	else
		inf.size = size;
	inf.style = style;
	inf.lastUsedInFrame = s_FrameCount;
	inf.flipped = flipped;
	m_CharacterRects.push_back(inf);
	AddRectToCache (inf);
}

void Font::AddRectToCache(CharacterInfo& info)
{
	// We cache ascii characters into a direct lookup buffer, for optimal performance on the most common characters
	if (info.index < 256 && info.size == 0 && info.style == kStyleDefault)
		m_AsciiCharacterRects[info.index] = info;
	// And a set of characters for the rest.
	else
		m_UnicodeCharacterRects.insert(info);
}

void Font::CacheRects ()
{
	m_AsciiCharacterRects.clear();
	m_AsciiCharacterRects.resize (256);
	m_UnicodeCharacterRects.clear();

	for (int i=0;i<m_CharacterRects.size();i++)
	{
		CharacterInfo& info = m_CharacterRects[i];
		// Older version didn't have the index. So derive it from i
		if (info.index == -1)
			info.index = i;

		AddRectToCache(info);
	}
}



// Dynamic font stuff
// ==================

static bool Equals (FontNames& list1, FontNames& list2)
{
	if (list1.size() != list2.size())
		return false;

	for (unsigned i=0; i<list1.size (); ++i)
		if (list1[i] != list2[i])
			return false;

	return true;
}

void Font::SetFontNames (FontNames &names)
{
	if (m_ConvertCase == kDynamicFont)
	{
		if (Equals (names, m_FontNames))
			return;

		m_FontNames = names;
// TODO!
//		DestroyDynamicFont ();
//		SetupDynamicFont ();
		ResetCachedTexture();
	}
	else
		ErrorString ("Font.names can only be set for dynamic fonts.");
}

void Font::ResetPackingData ()
{
	m_TexturePositions.clear();
	m_IntRects.clear();
	m_TexturePositions.insert (TexturePosition (0, 0));
	m_TexturePositionsSearchPosition = m_TexturePositions.begin();
}

bool Font::ResetCachedTexture ()
{
	if (m_ConvertCase != kDynamicFont)
		return true; // nothing to do for static fonts

	m_CharacterRects.clear();
	CacheRects ();

	int maxSize = gGraphicsCaps.maxTextureSize;

	// Some windows setups apparently crash when allocating textures > 4096^2. So, don't do it.
	maxSize = std::min(maxSize, 4096);
	
	if (m_TexWidth >  maxSize || m_TexHeight > maxSize)
	{
		ErrorString ("Failed to generate dynamic font texture, because all the needed characters do not fit onto a single texture. Try using less text or a smaller font size.");
		m_TexWidth = maxSize;
		m_TexHeight = maxSize;
		return false;
	}

	Texture2D *tex;
	if (!GetTexture().IsValid())
		return false;
	else
		tex = dynamic_pptr_cast<Texture2D*>(GetTexture());

	if (gGraphicsCaps.disableSubTextureUpload || (NEEDS_SYSTEM_MEM_COPY && !UNITY_EDITOR))
		tex->SetIsReadable(true);
	else
		tex->SetIsUnreloadable (true);

#if UNITY_EDITOR
	tex->SetEditorDontWriteTextureData(true);
#endif

	if (tex->GetDataWidth() != m_TexWidth || tex->GetDataHeight() != m_TexHeight || !tex->GetIsUploaded())
	{
		if (!tex->InitTexture (m_TexWidth, m_TexHeight, kTexFormatAlpha8, Texture2D::kNoMipmap))
			return false;
		tex->UpdateImageData ();
	}

	{
		UInt8* texData;
		ALLOC_TEMP(texData, UInt8, m_TexWidth * m_TexHeight);
		memset (texData, 0, m_TexWidth * m_TexHeight);
		int dataSize = m_TexWidth * m_TexHeight;

		if (!gGraphicsCaps.disableSubTextureUpload)
			GetGfxDevice().UploadTextureSubData2D( tex->GetTextureID(), texData, dataSize, 0, 0, 0, m_TexWidth, m_TexHeight, kTexFormatAlpha8, tex->GetActiveTextureColorSpace() );
		if (gGraphicsCaps.disableSubTextureUpload || NEEDS_SYSTEM_MEM_COPY)
		{
			ImageReference texImg;
			if (tex->GetWriteImageReference ( &texImg, 0, 0 ))
			{
				ImageReference data (m_TexWidth, m_TexHeight, m_TexWidth, kTexFormatAlpha8, texData);
				texImg.BlitImage( data );
			}
			if (gGraphicsCaps.disableSubTextureUpload)
				tex->UpdateImageData();
		}
	}

	ResetPackingData ();

	m_SubImageIndex = 0;
	m_SubImageSize = std::max(m_SubImageSize, (unsigned int)NextPowerOfTwo(8 * m_FontSize));
	m_SubImageSize = std::min(m_SubImageSize, m_TexWidth);
	
	return true;
}

bool Font::IsRectFree(const IntRect &r) const
{
	if (r.x < 0 || r.y < 0 || r.x + r.width > m_SubImageSize || r.y+r.height > m_SubImageSize)
		return false;

	for (UNITY_VECTOR(kMemFont,IntRect)::const_iterator i = m_IntRects.begin(); i != m_IntRects.end(); i++)
	{
		if (r.Intersects(*i))
			return false;
	}

	return true;
}

bool Font::AddCharacterToTexture (unsigned int unicodeChar, int size, unsigned int style)
{
	Rectf vert;

	unsigned int charWidth = 0;
	unsigned int charHeight = 0;
	unsigned int bufferWidth = 0;
	float advance = 0;
	// returns a pointer to static vector data
	UInt8* bitmap = GetCharacterBitmap (charWidth, charHeight, bufferWidth, vert, advance, unicodeChar, size, style | m_DefaultStyle);
	UNITY_TEMP_VECTOR(UInt8) flippedBitmap;
	bool flipped = false;

	if (bitmap == NULL && charHeight*charWidth != 0)
	{
		charWidth = 0;
		charHeight = 0;
		advance = 0;
	}

	if (charWidth > charHeight)
	{
		// flip glyphs with >1 aspect ratios for better packing results.
		flipped = true;
		flippedBitmap.resize (charWidth * charHeight);
		for (int x = 0; x<charWidth; x++)
		{
			for (int y = 0; y<charHeight; y++)
				flippedBitmap[charHeight-1-y + (charWidth-1-x)*charHeight] = bitmap[x + y*bufferWidth];
		}
		bitmap = &flippedBitmap[0];
		bufferWidth = charHeight;
		charHeight = charWidth;
		charWidth = bufferWidth;
	}
	else if (bufferWidth > charWidth)
	{
		flippedBitmap.resize (charWidth * charHeight);
		for (int x = 0; x<charWidth; x++)
		{
			for (int y = 0; y<charHeight; y++)
				flippedBitmap[x + y*charWidth] = bitmap[x + y*bufferWidth];
		}
		bitmap = &flippedBitmap[0];
		bufferWidth = charWidth;
	}

	vert.x -= m_CharacterPadding;
	vert.y += m_CharacterPadding;
	vert.width += 2*m_CharacterPadding;
	vert.height -= 2*m_CharacterPadding;

	while (true)
	{
		for (UNITY_SET(kMemFont,TexturePosition)::iterator i = m_TexturePositionsSearchPosition; i != m_TexturePositions.end(); i++)
		{
			IntRect r (i->x, i->y, charWidth+m_CharacterSpacing+2*m_CharacterPadding, charHeight+m_CharacterSpacing+2*m_CharacterPadding);
			if (IsRectFree (r))
			{
				IntRect r2 = r;
				r2.x--;
				while (IsRectFree (r2) && r2.x > 0)
				{
					r = r2;
					r2.x--;
				}
				r2 = r;
				r2.y--;
				while (IsRectFree (r2) && r2.y > 0)
				{
					r = r2;
					r2.y--;
				}

				m_IntRects.push_back(r);
				m_TexturePositionsSearchPosition = i;
				m_TexturePositionsSearchPosition++;
				m_TexturePositions.erase(i);
				m_TexturePositions.insert( TexturePosition(r.x + r.width, r.y));
				m_TexturePositions.insert( TexturePosition(r.x, r.y + r.height));

				// Offset sub image position to get actual texture position
				int subImagePos = m_SubImageIndex * m_SubImageSize;
				r.x += subImagePos % m_TexWidth;
				r.y += (subImagePos / m_TexWidth) * m_SubImageSize;

				if (bitmap)
				{
					int dataSize = bufferWidth * charHeight;
					Texture2D *tex = dynamic_pptr_cast<Texture2D*>(GetTexture());

					if (!gGraphicsCaps.disableSubTextureUpload)
						GetGfxDevice().UploadTextureSubData2D( tex->GetTextureID(), bitmap, dataSize, 0, r.x+m_CharacterPadding, r.y+m_CharacterPadding, bufferWidth, charHeight, kTexFormatAlpha8, tex->GetActiveTextureColorSpace() );
					if (gGraphicsCaps.disableSubTextureUpload || NEEDS_SYSTEM_MEM_COPY)
					{
						ImageReference texImg;
						if (tex->GetWriteImageReference ( &texImg, 0, 0 ))
						{
							ImageReference destRect = texImg.ClipImage( r.x, r.y, bufferWidth, charHeight );

							ImageReference data (bufferWidth, charHeight, bufferWidth, kTexFormatAlpha8, bitmap);
							destRect.BlitImage( data );
						}
					}
				}

				float width = m_TexWidth;
				float height = m_TexHeight;

				Rectf uv (r.x/width, (r.y+charHeight+2*m_CharacterPadding)/height, (charWidth+2*m_CharacterPadding)/width, -((charHeight+2*m_CharacterPadding)/height));
				AddCharacterInfoEntry (uv, vert, advance, unicodeChar, flipped, size, style);
				return true;
			}
		}
		if (m_TexturePositionsSearchPosition != m_TexturePositions.begin())
			m_TexturePositionsSearchPosition = m_TexturePositions.begin();
		else
		{
			if (m_SubImageIndex+1 < (m_TexWidth/m_SubImageSize) * (m_TexHeight/m_SubImageSize))
			{
				// This sub image is full. move to the next one.
				m_SubImageIndex++;
				ResetPackingData ();
			}
			else
				return false;
		}
	}
	return false;
}

void Font::GrowTexture (int maxFontSize)
{
	// If we couldn't fit all characters by repainting the texture, enlarge it.
	if (m_TexWidth < m_TexHeight)
		m_TexWidth *= 2;
	else
		m_TexHeight *= 2;
	// Make sure that we fit the largest characters in the string into a single sub image.
	m_SubImageSize = std::max(m_SubImageSize, (unsigned int)NextPowerOfTwo(4 * maxFontSize));
	m_SubImageSize = std::min(m_SubImageSize, m_TexWidth);
}

UInt16 *Font::CollectAllUsedCharacters (UInt16 *chars, int &length, int *&sizes, unsigned int *&styles)
{
	// We have to create a new texture. Make sure to add all characters which have been used in this frame.
	int usedThisFrame = 0;
	for (UNITY_VECTOR(kMemFont,CharacterInfo)::iterator ch = m_CharacterRects.begin(); ch != m_CharacterRects.end(); ch++)
	{
		if (ch->lastUsedInFrame == s_FrameCount)
			usedThisFrame++;
	}
	UInt16 *newchars = new UInt16[usedThisFrame + length];
	sizes = new int[usedThisFrame + length];
	styles = new unsigned int[usedThisFrame + length];
	// string being currently cached; without size/style override
	for (int ch=0; ch<length; ch++)
	{
		newchars[ch] = chars[ch];
		sizes[ch] = -1;
		styles[ch] = -1;
	}
	// put in characters used in this frame, with their original size/style
	for (UNITY_VECTOR(kMemFont,CharacterInfo)::iterator ch = m_CharacterRects.begin(); ch != m_CharacterRects.end(); ch++)
	{
		if (ch->lastUsedInFrame == s_FrameCount)
		{
			newchars[length] = ch->index;
			sizes[length] = ch->size;
			styles[length] = ch->style;
			length++;
		}
	}
	return newchars;
}

bool Font::CacheFontForText (UInt16 *chars, int length, int size, unsigned int style, std::vector<TextFormatChange> format)
{
	if (m_ConvertCase != kDynamicFont)
		return true;

	PROFILER_AUTO(gFontTextureCacheProfile, NULL)

	if (!GetTexture().IsValid() && !ResetCachedTexture ())
		return false;

	bool didAdd = false;
	int *sizes = NULL;
	unsigned int *styles = NULL;
	int maxFontSize = 0;
	bool didNotFit = false;
	do {
		didNotFit = false;
		FormatStack formatStack(0xffffffff, size, style);
		int formatChange = 0;
		for (int i=0; i<length; i++)
		{
			while (formatChange < format.size() && i >= format[formatChange].startPosition)
			{
				i += format[formatChange].skipCharacters;
				formatStack.PushFormat(format[formatChange]);
				formatChange++;
			}
			// Recheck range after skipping format changes
			if (i >= length)
				break;

			int thisSize = formatStack.Current().size;
			int thisStyle = formatStack.Current().style;
			if (sizes && sizes[i] != -1)
			{
				// Normally, we just add characters with the computed size/style.
				// But when we need to recreate the texture, then we need to make sure all used characters
				// with all used sizes are in there, so we index size & style from an array.
				thisSize = sizes[i];
				thisStyle = styles[i];
			}
			if (thisSize == 0)
				thisSize = m_FontSize;
			if (thisSize > maxFontSize)
				maxFontSize = thisSize;
			UInt16 thisChar = chars[i];
			if (HasCharacterDynamic (thisChar) && !HasCharacterInTexture (chars[i], thisSize, thisStyle))
			{
				if (!AddCharacterToTexture (thisChar, thisSize, thisStyle))
				{
					if (sizes != NULL)
						GrowTexture(maxFontSize);
					else
						chars = CollectAllUsedCharacters (chars, length, sizes, styles);
					didNotFit = true;
					if (!ResetCachedTexture())
						return false;
					break;
				}
				didAdd = true;
			}
		}
	} while(didNotFit);

	if (didAdd && gGraphicsCaps.disableSubTextureUpload)
		dynamic_pptr_cast<Texture2D*>(GetTexture())->UpdateImageData();

	if (sizes != NULL)
	{
		delete[] chars;
		delete[] sizes;
		delete[] styles;
		
		#if ENABLE_SCRIPTING
		// Make sure we don't call InvokeFontTextureRebuildCallback_Internal repeatedly due to
		// ApplyToMeshes adding characters.
		static int recursionDepth = 0;
		recursionDepth++;
		TextMeshGenerator2::Flush();
		
		ApplyToMeshes();
		recursionDepth--;

		if (recursionDepth == 0)
		{
			ScriptingObjectPtr instance = Scripting::ScriptingWrapperFor(this);
			if (instance)
			{
				ScriptingInvocation invocation(GetScriptingManager().GetCommonClasses().font_InvokeFontTextureRebuildCallback_Internal);
				invocation.object = instance;
				invocation.Invoke();
			}
		}
		#endif
	}

	return true;
}

float Font::GetLineSpacing (int size) const
{
	if (size == 0 || m_FontSize == 0)
		return m_LineSpacing;
	else
		return m_LineSpacing * (float)size/m_FontSize;
}

IMPLEMENT_CLASS_HAS_INIT (Font)
IMPLEMENT_OBJECT_SERIALIZE (Font)
