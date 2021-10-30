#include "UnityPrefix.h"
#include "TrueTypeFontImporter.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "TextureImporter.h"
#include "Runtime/GameCode/CloneObject.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Utilities/BitUtility.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "Runtime/Graphics/Texture2D.h"
#include "AssetDatabase.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Src/AssetPipeline/AssetImportState.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/Argv.h"

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kTrueTypeFontImporterVersion = 4 };

// A list of the chars we want in the texture - Might be refactored later
// If we only wish to use lower or upper chars, we use the respective string :)
const char* kCharsAll = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
const char* kCharsLower = " !\"#$%&'()*+,-./0123456789:;<=>?@abcdefghijklmnopqrstuvwxyz[\\]^_`{|}~";
const char* kCharsUpper = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`{|}~";

enum { kAntialiasLight = 0, AntialiasStrong = 1, kNoAntialias = 2 };

FT_Library TrueTypeFontImporter::ms_FTLibrary = NULL;

static int CanLoadPathName (const string& pathName, int* order)
{
	string ext = ToLower (GetPathNameExtension (pathName));
	
	if (ext == "ttf" || ext == "dfont" || ext == "otf")
	{
		*order = 2;
		return kCopyAsset;
	}
	else
		return 0;		
}

void TrueTypeFontImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (TrueTypeFontImporter), kTrueTypeFontImporterVersion);

	RegisterAllowNameConversion("TrueTypeFontImporter", "size", "m_FontSize");
	RegisterAllowNameConversion("TrueTypeFontImporter", "case", "m_ForceTextureCase");
}

TrueTypeFontImporter::TrueTypeFontImporter(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_FontSize = 16;
	m_ForceTextureCase = Font::kDynamicFont;
	m_CharacterSpacing = 1;
	m_CharacterPadding = 0;
	m_IncludeFontData = true;
	m_Use2xBehaviour = false;
	m_CustomCharacters = "";
	m_Output.hasEmptyFontData = true;
	m_FontRenderingMode = kFontRenderingModeSmooth;
}

TrueTypeFontImporter::~TrueTypeFontImporter ()
{}

float TrueTypeFontImporter::GetCharacterAscent(FT_Face face, UnicodeChar ch)
{
   	FT_UInt charIndex = FT_Get_Char_Index(face, ch);
	if (charIndex == 0)
		return 0;

	// Render the current glyph.
	FT_Load_Glyph(face, charIndex, FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING);
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

	return face->glyph->bitmap_top;
}

// In 2.x we got the ascent by measuring all characters the font uses, instead of querying it from
// FreeType. This gives the wrong value for fonts with diacritic characters extending over the ascent value.
float TrueTypeFontImporter::GetAscent(FT_Face face, const dynamic_array<UnicodeChar>& chars) 
{
	if (m_Use2xBehaviour)
	{
		float maxAscent = 0;
		for (int i = 0; i < chars.size(); i++)
			maxAscent = std::max(GetCharacterAscent(face, chars[i]), maxAscent);
		return maxAscent;
	}
	else
		return (float)face->ascender * (float)face->size->metrics.y_ppem / (float)face->units_per_EM;		
}


void TrueTypeFontImporter::SetupKerningValues(FT_Face face, Font& font, const dynamic_array<UnicodeChar>& chars) 
{
  	FT_UInt   glyphIndex, nextGlyphIndex; 
  	FT_Vector  delta;
	for (int i = 0; i < chars.size(); i++)
	{
		// Since this code has O^2 complexity, if we have an asian font with kerning information,
		// this can quickly take a very long time, so display a progress bar, or people think it has crashed.
		GetUpdateProgressCallback() (i/(float)chars.size(), -1.0, GetAssetPathName());
		
    	glyphIndex = FT_Get_Char_Index(face, chars[i]);
		if (glyphIndex == 0)
			continue;
			
    	for(int j = 0; j < chars.size(); j++) 
    	{
	    	nextGlyphIndex = FT_Get_Char_Index(face, chars[j]);
			if (nextGlyphIndex == 0)
				continue;
			
			FT_Get_Kerning( face, glyphIndex, nextGlyphIndex, FT_KERNING_DEFAULT, &delta );
			delta.x = delta.x >> 6;
			if(delta.x != 0) 
			{
				std::pair<UnicodeChar, UnicodeChar> kerningPair = std::pair<UnicodeChar, UnicodeChar>(chars[i], chars[j]);
				font.GetKerningValues()[kerningPair] = delta.x;
			}
    	}
	}
}

bool TrueTypeFontImporter::ReadFontDataFromFile(const string& path, UNITY_VECTOR(kMemFont,char) &fontData)
{
	int fileSize = GetFileLength (path);

	if (0 == fileSize)
	{
#if UNITY_OSX
		FSRef  ref;
		SInt16 res;
		
		HFSUniStr255 str;
		FSGetResourceForkName(&str);
		
		if ( FSPathMakeRef( (const UInt8*)path.c_str(), &ref, FALSE ) != noErr )
			return false;

		if ( FSOpenResourceFile( &ref, str.length, str.unicode, fsRdPerm, &res ) != noErr )
			return false;

		UseResFile(res);
		
		Handle data = Get1IndResource('sfnt', 1);
		
		if (data == NULL)
		{
			ErrorString ("Could not load ttf font from resource fork font");
			CloseResFile( res );
			return false;
		}
		
		size_t size = GetHandleSize(data);
		fontData.resize (size);
		memcpy (&fontData[0] , *data, size);

		CloseResFile( res );
		
		return true;
#else
		return false;
#endif
	}
	else
	{
		fontData.resize (fileSize);
		return ReadFromFile (path, &fontData[0], 0, fileSize);
	}
}

int TrueTypeFontImporter::OpenFreeTypeFace (const string& path, FT_Face& outFace, FontData& outMemory)
{
	// Read file into memory.  Doing this rather than letting FT_New_Face read the
	// file itself works around the fact that FreeType doesn't support Unicode file names.
	// Otherwise, if we have multi-byte UTF-8 characters in the path, FT_New_Face will fail.
	if (!ReadFontDataFromFile (path, outMemory))
		return FT_Err_Cannot_Open_Resource;

	// Open.
	return FT_New_Memory_Face (ms_FTLibrary, reinterpret_cast<const FT_Byte*> (outMemory.data ()), outMemory.size(), 0, &outFace);
}


std::string TrueTypeFontImporter::GetFontNameFromTTFData (std::string path)
{
	if (!LoadFontLibrary())
		return "";
	
	std::string fontName;
	FT_Face face;
	FontData data;
	int err = OpenFreeTypeFace (path, face, data);
	if (err == 0)
	{
		if (face->family_name)
			fontName = face->family_name;
		FT_Done_Face(face);
	}

	return fontName;
}

std::string TrueTypeFontImporter::GetFontNameFromTTFData ()
{
	return GetFontNameFromTTFData(GetAbsoluteAssetPathName());
}

void TrueTypeFontImporter::GenerateFont (Font &font, dynamic_array<UnicodeChar> &chars, Texture2D &texture, bool includeFontData)
{				
	std::string absolutePath = GetAbsoluteAssetPathName();
	
	UNITY_VECTOR(kMemFont,char) &fontData = font.GetFontData ();
	ReadFontDataFromFile(absolutePath, fontData);
	
	font.GetFontNames() = m_FontNames;

	if (font.GetFontNames().empty() || font.GetFontNames()[0].empty())
	{
		font.GetFontNames().clear ();
		font.GetFontNames().push_back(GetFontNameFromTTFData());
	}
	
	string ext = ToLower (GetPathNameExtension (absolutePath));
	
	int textureCase = font.GetConvertCase();
	if (textureCase == Font::kDynamicFont && !includeFontData)
		fontData.resize (0);
	
	font.SetFontSize(m_FontSize);
	font.SetCharacterSpacing(m_CharacterSpacing);
	font.SetCharacterPadding(m_CharacterPadding);
	font.SetFontRenderingMode(m_FontRenderingMode);
	
	font.SetTexture(&texture);
	
	if (textureCase != Font::kDynamicFont)
	{
		if (m_FontRenderingMode == kFontRenderingModeOSDefault)
		{
			WarningString ("FontRenderingMode.OSDefault is only supported for dynamic fonts. Importing using FontRenderingMode.Smooth");
			font.SetFontRenderingMode(kFontRenderingModeSmooth);
		}
		
		// make static font textures as small as possible (but <16 gives errors from GL).
		// For dynamic textures, we keep the default minimum of 256 to avoid unneeded texture resizing.
		font.SetMinimalFontTextureSize (16);
		
		// temporarily set font mode to dynamic to render the characters into it.
		font.SetConvertCase(Font::kDynamicFont);
		font.AwakeFromLoad(kDefaultAwakeFromLoad);
		
		// render characters
		font.CacheFontForText(&chars[0], chars.size());
		
		// revert font mode
		font.SetConvertCase(textureCase);
		font.AwakeFromLoad(kDefaultAwakeFromLoad);
		
		// set texture properties, and make sure it is written
		texture.SetEditorDontWriteTextureData(false);
		texture.SetIsUnreloadable (false);
		texture.UpdateImageDataDontTouchMipmap ();
		
		// clear font data, not needed any more
		fontData.resize (0);
	}
	else
	{
		texture.InitTexture (1, 1, kTexFormatAlpha8, Texture2D::kNoMipmap, 1);
		texture.SetIsUnreloadable (true);
		texture.SetEditorDontWriteTextureData (true);
	}

	texture.SetIsReadable (false);
}

bool TrueTypeFontImporter::LoadFontLibrary()
{
	// Freetype code here
	if (ms_FTLibrary == NULL)
	{
		FT_Error error = FT_Init_FreeType( &ms_FTLibrary );
		if ( error )
		{
			ErrorString("Freetype could not be initialized!");
			return false;
		}
	}
	return true;
}

bool TrueTypeFontImporter::IsFormatSupported()
{
	if (!LoadFontLibrary())
		return false;

	FT_Face face;
	FontData data;
	int err = OpenFreeTypeFace (GetAbsoluteAssetPathName (), face, data);
	bool supported = err != FT_Err_Unknown_File_Format && face->family_name != NULL;
	if (err == 0)
		FT_Done_Face(face);
	return supported;
}

PPtr<Font> TrueTypeFontImporter::GenerateEditableFont (string path)
{
	string basePath = DeletePathNameExtension(path);
	
	Font *font = dynamic_pptr_cast<Font*>(GetMainAsset(GetAssetPathName()));
	if (!font)
	{
		ErrorString ("Could not load font.");
		return NULL;
	}
	
	Font *clonedFont = dynamic_pptr_cast<Font*>(&CloneObject(*font));
	if (!clonedFont)
	{
		ErrorString ("Could not copy font asset.");
		return NULL;
	}
		
	Material *material = font->GetMaterial();
	if (!material)
	{
		ErrorString ("Could not get font material.");
		return NULL;
	}
	
	Material *clonedMaterial = dynamic_pptr_cast<Material*>(&CloneObject(*material));
	if (!clonedMaterial)
	{
		ErrorString ("Could not copy font material.");
		return NULL;
	}
	
	clonedFont->SetMaterial(clonedMaterial);
	
	Texture2D *tex = dynamic_pptr_cast<Texture2D*>(font->GetTexture());
	if (!tex)
	{
		ErrorString ("Could not get font texture.");
		return NULL;
	}
	
	dynamic_array<UInt8> buffer;
	string texPath = basePath+".png";
	if( !tex->EncodeToPNG( buffer ) )
	{
		ErrorString ("Could not extract font texture.");
		return NULL;
	}
	
	if (!WriteBytesToFile (&buffer[0], buffer.size(), texPath))
	{
	   ErrorString ("Could not write font texture.");
	   return NULL;
	}

	AssetInterface::Get().Refresh();

	TextureImporter *texImporter = dynamic_pptr_cast<TextureImporter*>(FindAssetImporterAtAssetPath (texPath));
	if (!texImporter)
	{
		ErrorString ("Could not get texture importer");
		return NULL;
	}
	texImporter->SetTextureFormat(kTexFormatAlpha8);
	texImporter->SetMipmapEnabled(false);

	AssetInterface::Get().ImportAtPath (texPath);

	Texture2D* clonedTex = dynamic_pptr_cast<Texture2D*>(GetMainAsset(texPath));
	if (!clonedTex)
	{
		ErrorString ("Could not get copied texture");
		return NULL;
	}

	clonedFont->SetTexture(clonedTex);
	clonedMaterial->SetTexture(ShaderLab::Property("_MainTex"), clonedTex);

	if (!AssetInterface::Get ().CreateSerializedAsset (*clonedMaterial, basePath+".mat", AssetInterface::kDeleteExistingAssets | AssetInterface::kWriteAndImport | AssetInterface::kForceImportImmediate))
	{
		ErrorString ("Could not write font material.");
		return NULL;
	}
	
	if (!AssetInterface::Get ().CreateSerializedAsset (*clonedFont, path, AssetInterface::kDeleteExistingAssets | AssetInterface::kWriteAndImport | AssetInterface::kForceImportImmediate))
	{
		ErrorString ("Could not write font asset.");
		return NULL;
	}

	return clonedFont;
}

void TrueTypeFontImporter::GenerateAssetData ()
{
	BuildTargetPlatform targetPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();	
	
	GetAssetImportState().SetDidImportAssetForTarget (targetPlatform);
	const bool buildingBuiltinResources = HasARGV("buildBuiltinUnityResources");
	
	bool includeFontData = m_IncludeFontData;
	if (!HasBuildTargetOSFonts(targetPlatform) && m_ForceTextureCase == Font::kDynamicFont && !includeFontData)
	{
		// Don't show dynamic fonts warning when builtin builtin resources. We want no warnings / errors when building those.
		bool showWarning = !buildingBuiltinResources;
		if (showWarning)
		{
			WarningString(Format("The target platform does not have access to any system fonts. Including Font data even though the importer is set not to.", GetAssetPathName().c_str()));
		}
		includeFontData = true;
	}

	// Our built-in Arial.ttf font is not actually present on some platforms (like Android), which
	// would make the default texts look different than on other platforms. So just include the
	// font data for the built-in fonts, when building for them. If/when we have per-platform
	// "include font data" settings on fonts, then this hack can be removed
	// and the font setting set appropriately.
	if (!HasBuildTargetDefaultUnityFonts(targetPlatform) && buildingBuiltinResources)
		includeFontData = true;

	Font& font = ProduceAssetObject<Font> ("font");
	
  	// Store font information in the font object (UVs, Kerning and so on)
	font.SetAsciiStartOffset(0);

	if (!LoadFontLibrary())
		return;
	
	std::string absolutePath = GetAbsoluteAssetPathName();

	#if !UNITY_OSX
	// Some fonts coming from OS X have all data in resource fork, and zero bytes in the actual file. Check this case
	// and notify the user.
	int fileSize = GetFileLength( absolutePath );
	if( fileSize == 0 )
	{
		font.AwakeFromLoad(kDefaultAwakeFromLoad);
		ErrorString(Format("The font %s could not be imported because the file is empty.\nIf the file comes from Mac OS X, it might be that all data is in 'resource fork'. Change the file there to have data in the file.", GetAssetPathName().c_str()));
		return;
	}
	#endif

	FT_Face face;
	FontData data;
	int error = OpenFreeTypeFace (absolutePath, face, data);
 	if ( error == FT_Err_Unknown_File_Format )
	{
		font.AwakeFromLoad(kDefaultAwakeFromLoad);
    	ErrorString(Format("The font %s could not be imported because the  format is unsupported", GetAssetPathName().c_str()));
		return;
  	}
  	else if ( error )
  	{
		font.AwakeFromLoad(kDefaultAwakeFromLoad);
    	ErrorString(Format("The font %s could not be imported because it couldn't be read", GetAssetPathName().c_str()));
		return;
  	}
	if (face->family_name == NULL)
	{
		font.AwakeFromLoad(kDefaultAwakeFromLoad);
    	ErrorString(Format("The font %s could not be imported because the TrueType name cannot be parsed.", GetAssetPathName().c_str()));
		return;		
	}
	
  	// Set the font size
  	FT_Set_Pixel_Sizes(face, m_FontSize, 0);

  	dynamic_array<UnicodeChar> chars;
	
	if (m_ForceTextureCase == Font::kUnicodeSet || m_ForceTextureCase == Font::kDynamicFont)
	{
		FT_ULong  charcode;
		FT_UInt   gindex;
		std::set<UnicodeChar> sortedChars;
		charcode = FT_Get_First_Char( face, &gindex );
	 	while ( gindex != 0 )
		{
			if (charcode < 1 << 16)
				sortedChars.insert(charcode);
			charcode = FT_Get_Next_Char( face, charcode, &gindex );
		}

		for(std::set<UnicodeChar>::const_iterator it = sortedChars.begin(); it != sortedChars.end(); ++it)
			chars.push_back(*it);
	}
	else
	{
		string asciiDefaultChars;
		// Set the right char array, depending on weather the user wants all cases or just upper/lower.
		if (m_ForceTextureCase == Font::kUpperCase)
			asciiDefaultChars = kCharsUpper;	
		else if (m_ForceTextureCase == Font::kLowerCase)
			asciiDefaultChars = kCharsLower;
		else if (m_ForceTextureCase == Font::kCustomSet)
			asciiDefaultChars = m_CustomCharacters;
		else
			asciiDefaultChars = kCharsAll;

		// generate unicode chars from extra chars and ascii chars	
		ConvertUTF8toUTF16(asciiDefaultChars, chars);
	}
	font.SetConvertCase(m_ForceTextureCase);
	font.SetAscent(GetAscent(face, chars));
 	
  	// Store kerning information by creating a map of all kerning values that are different from 0
  	// Or so small that they are not visible.
  	bool hasKerning = FT_HAS_KERNING(face);
  	if(hasKerning)
	  	SetupKerningValues(face, font, chars);		

  	// Build new material and make sure to retain the color of the material!
	Material& fontMaterial = ProduceAssetObject<Material> ("Font Material");
	fontMaterial.AwakeFromLoad(kDefaultAwakeFromLoad);
	
	Shader* fontShader = NULL;
	
	// One asks, what the hell is this??? I know this is a fucked up hack, but this is the least painful way,
	// When building builtin resources, we allow no external dependencies. Thus we need to make sure the fonts
	// reference the shader in the project and not in the builtin resources.
	if (IsDeveloperBuild(false) && StrICmp (GetAssetPathName(), string("Assets/DefaultResources/") + kDefaultFontName) == 0)
	{
		AssertIf(GetMainAsset("Assets/DefaultResources/Font.shader") == NULL);
		fontShader = dynamic_pptr_cast<Shader*> (GetMainAsset("Assets/DefaultResources/Font.shader"));
	}
	
	if (fontShader == NULL)
		fontShader = GetBuiltinResource<Shader> ("Font.shader");
	
	fontMaterial.SetShader(fontShader);

	font.SetMaterial(&fontMaterial);
			
	Texture2D& texture = ProduceAssetObject<Texture2D> ("Font Texture");
	texture.AwakeFromLoad(kDefaultAwakeFromLoad);

	GenerateFont(font, chars, texture, includeFontData);

  	// Build new material and make sure to retain the color of the material!
	font.GetMaterial()->SetTexture(ShaderLab::Property("_MainTex"), &texture);

	float pixelHeight = (float)face->height * (float)face->size->metrics.y_ppem / (float)face->units_per_EM;
  	font.SetLineSpacing(pixelHeight);
	font.AwakeFromLoad(kDefaultAwakeFromLoad);

	// GetFallbackFontReferences, when called during import, may generate a reference to the previous version of
	// the imported font, which causes errors. Check for that and remove it if needed.
	FontFallbacks &fallbacks = font.GetFontFallbacks();
	for (FontFallbacks::iterator i = fallbacks.begin(); i!= fallbacks.end(); i++)
	{
		if (GetAssetPathFromObject(*i) == GetAssetPathName())
		{
			fallbacks.erase(i);
			break;
		}
	}
	
	// Remove any cached text strings, as we may have some that reference the font with the old settings
	TextMeshGenerator2::Flush ();

	FT_Done_Face(face);

	
	m_Output.hasEmptyFontData = font.GetFontData().empty();
}

void TrueTypeFontImporter::GetFallbackFontReferences (Font* font)
{
	if (font->GetConvertCase() == Font::kDynamicFont)
	{
		string fontPath = GetAssetPathFromObject(font);
		FontNames& fallbackNames = font->GetFontNames(); 
		FontFallbacks &fallbacks = font->GetFontFallbacks();
		fallbacks.clear();
		
		Assets::const_iterator end, i;
		i = AssetDatabase::Get().begin();
		end = AssetDatabase::Get().end();
		for (;i != end;i++)
		{
			if (i->second.importerClassId == ClassID(TrueTypeFontImporter))
			{
				string path = GetAssetPathFromGUID(i->first);
				string metaDataPath = GetMetaDataPathFromAssetPath (path);
				TrueTypeFontImporter* importer = dynamic_pptr_cast<TrueTypeFontImporter*> (FindAssetImporterAtPath (metaDataPath));
				if (importer->m_ForceTextureCase == Font::kDynamicFont)
				{
					string name = GetFontNameFromTTFData (path);
					if (!name.empty())
					{
						for (FontNames::iterator fallback = fallbackNames.begin();fallback != fallbackNames.end();fallback++)
						{
							if (*fallback == name && i->second.mainRepresentation.object.GetInstanceID() != font->GetInstanceID())
							{
								Font *fallbackFont = dynamic_pptr_cast<Font*>(i->second.mainRepresentation.object);
								fallbacks.push_back(fallbackFont);
							}
						}
					}
				}
			}
		}
		font->ResetCachedTexture();
	}
}

/// Reimport for current platform
bool TrueTypeFontImporter::DoesAssetNeedReimport (const string& path, BuildTargetPlatform targetPlatform, bool unload)
{
	int order;
	if (!CanLoadPathName (path, &order))
		return false;

	bool requireReimport = false;

	string metaDataPath = GetMetaDataPathFromAssetPath (path);
	TrueTypeFontImporter* importer = dynamic_pptr_cast<TrueTypeFontImporter*> (FindAssetImporterAtPath (metaDataPath));
	
	if (importer != NULL)
	{
		// Only some platforms support stripping out the font data and using system fonts instead.
		// Make sure platform switching triggers a reimports if it doesn't match up.
		if (importer->m_ForceTextureCase == Font::kDynamicFont && !importer->m_IncludeFontData)
		{
			bool expectEmptyFontData = HasBuildTargetOSFonts(targetPlatform);
			if (!HasBuildTargetDefaultUnityFonts(targetPlatform) && HasARGV("buildBuiltinUnityResources"))
				expectEmptyFontData = false;

			requireReimport = (importer->m_Output.hasEmptyFontData != expectEmptyFontData);
		}
	}
	
	if (unload)
	{
		if (importer && !importer->IsPersistentDirty()) 
			UnloadObject(importer);

		GetPersistentManager().UnloadStream(metaDataPath);
	}
	
	
	return requireReimport;
}

template<class T>
void TrueTypeFontImporter::Transfer (T& transfer) 
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	TRANSFER(m_FontSize);
	TRANSFER(m_ForceTextureCase);
	TRANSFER(m_CharacterSpacing);
	TRANSFER(m_CharacterPadding);
	TRANSFER(m_IncludeFontData);
	TRANSFER(m_Use2xBehaviour);
	transfer.Align();
	TRANSFER(m_FontNames);
	TRANSFER(m_CustomCharacters);
	TRANSFER(m_FontRenderingMode);
	
	// the old ttf .meta format did not have the distinction between versions 1 and 2. 
	// so ignore this on meta import.
	if (transfer.IsOldVersion(1) && transfer.IsReading() && !transfer.AssetMetaDataOnly())
		m_Use2xBehaviour = true;
	
	if (!transfer.AssetMetaDataOnly())
	{
		TRANSFER(m_Output);
	}	
	
	PostTransfer (transfer);
}


template<class T>
void TrueTypeFontImporter::Output::Transfer (T& transfer) 
{
	TRANSFER(hasEmptyFontData);
}


IMPLEMENT_CLASS_HAS_INIT (TrueTypeFontImporter)
IMPLEMENT_OBJECT_SERIALIZE (TrueTypeFontImporter)
