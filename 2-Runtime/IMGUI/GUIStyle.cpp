#include "UnityPrefix.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Filters/Misc/Font.h"
#include "TextMeshGenerator2.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Misc/ResourceManager.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Graphics/GeneratedTextures.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "Runtime/Graphics/TextureGenerator.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/GUIContent.h"
#include "Runtime/IMGUI/IMGUIUtils.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/AssetBundle.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/OptimizedGUIBlock.h"
#include "Editor/Src/TooltipManager.h"
#include "Editor/Src/Highlighter/HighlighterCore.h"
#include "Editor/Src/EditorResources.h"
#include "Editor/Src/EditorHelper.h"
#endif

const float s_TabSize = 16;
using namespace std;
static Rectf s_GUIClipRect (0,0,1,1);

namespace GUIStyle_Static
{
	PPtr<Font> s_DefaultFont = NULL, s_BuiltinFont = NULL;
} // namespace GUIStyle_Static

// if these are set to 0 icons scale to fit the text
float s_GUIStyleIconSizeX = 0;
float s_GUIStyleIconSizeY = 0;


// A minimal (4x4) texture is not enough because of limited subtexel precision on some cards.
// With large clipping rectangles, one texel in clipping texture can span lots of pixels on screen,
// and because of limited precision there can be one pixel errors.
// E.g. Radeon HD 3850 seems to need at least 16x16 texture.
const float kGUIClipTextureSize = 16.0f;

static Material *kGUITextMaterial = NULL;
static Material *kBlendMaterial = NULL;
static Material *kBlitMaterial = NULL;

inline void GUIClipTexture (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) 
{
	if (x == 0 || y == 0 || x == maxX - 1 || y == maxY - 1) 
		*data = 0;
	else
		*data = 255;
}

static Texture2D *gGUIClipTexture = NULL;

Texture2D *GUIStyle::GetClipTexture ()
{
	return gGUIClipTexture;
}

void InitializeGUIClipTexture () 
{
	if (gGUIClipTexture)
		return;
	
	gGUIClipTexture = BuildTexture <unsigned char> (int(kGUIClipTextureSize), int(kGUIClipTextureSize), kTexFormatAlpha8, &GUIClipTexture);
	gGUIClipTexture->SetFilterMode (kTexFilterNearest);
	gGUIClipTexture->SetWrapMode(kTexWrapClamp);
	ShaderLab::PropertySheet *props = ShaderLab::g_GlobalProperties;
	ShaderLab::TexEnv *te = props->SetTexture (ShaderLab::Property ("_GUIClipTexture"),gGUIClipTexture);
	te->SetMatrixName (ShaderLab::Property("_GUIClipTextureMatrix"));
	te->SetTexGen (kTexGenEyeLinear);
}

Material* GetGUIBlitMaterial ()
{
	if (!kBlitMaterial) 
	{
		Shader* shader = GetBuiltinResource<Shader> ("Internal-GUITextureBlit.shader");
		kBlitMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
		InitializeGUIClipTexture();
	}
	
	return kBlitMaterial;
}

Material* GetGUIBlendMaterial ()
{
	if (!kBlendMaterial) 
	{
		Shader* shader = GetBuiltinResource<Shader> ("Internal-GUITextureClip.shader");
		kBlendMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
		InitializeGUIClipTexture();
	}
	
	return kBlendMaterial;
}

static Material* GetGUITextMaterial () 
{
	if (!kGUITextMaterial) 
	{
		Shader* shader = GetBuiltinResource<Shader> ("Internal-GUITextureClipText.shader");
		kGUITextMaterial = Material::CreateMaterial(*shader, Object::kHideAndDontSave);
		InitializeGUIClipTexture();
	}
	
	return kGUITextMaterial;
}

GUIStyle::GUIStyle()
{
	m_Alignment = 0;
	m_RichText = true;
	m_WordWrap = 0;
	m_Clipping = 0;
	m_ImagePosition = 0;
	m_ContentOffset = Vector2f(0.0f, 0.0f);
	m_ClipOffset = Vector2f(0.0f, 0.0f);
	m_FixedWidth = 0;
	m_FixedHeight = 0;
	m_FontSize = 0;
	m_FontStyle = 0;
	m_StretchWidth = true;
	m_StretchHeight = false;
	
}

GUIStyle::GUIStyle (const GUIStyle &other)
{
	m_Name = other.m_Name;
	m_Normal = other.m_Normal;
	m_Hover = other.m_Hover;
	m_Active = other.m_Active;
	m_Focused = other.m_Focused;
	m_OnNormal = other.m_OnNormal;
	m_OnHover = other.m_OnHover;
	m_OnActive = other.m_OnActive;
	m_OnFocused = other.m_OnFocused;
	m_Border = other.m_Border;
	m_Margin = other.m_Margin;
	m_Padding = other.m_Padding;
	m_Overflow = other.m_Overflow;
	m_Font = other.m_Font;
	m_Alignment = other.m_Alignment;
	m_RichText = other.m_RichText;
	m_WordWrap = other.m_WordWrap;
	m_Clipping = other.m_Clipping;
	m_ImagePosition = other.m_ImagePosition;
	m_ContentOffset = other.m_ContentOffset;
	m_ClipOffset = other.m_ClipOffset;
	m_FixedWidth = other.m_FixedWidth;
	m_FixedHeight = other.m_FixedHeight;
	m_FontSize = other.m_FontSize;
	m_FontStyle = other.m_FontStyle;
	m_StretchWidth = other.m_StretchWidth;
	m_StretchHeight = other.m_StretchHeight;
}


void GUIStyle::SetDefaultFont (Font *font) 
{
	using namespace GUIStyle_Static;
	if (font != NULL)
		s_DefaultFont = font;
	else 
		s_DefaultFont = GetBuiltinResource<Font> (kDefaultFontName);
}
Font*GUIStyle::GetDefaultFont ()
{
	return GUIStyle_Static::s_DefaultFont;
}

void GUIStyle::Draw (GUIState &state, const Rectf &screenRect, GUIContent &content, int controlID, bool on) const 
{
	InputEvent &evt (*state.m_CurrentEvent);
	int hot = IMGUI::GetHotControl (state);	
	bool enabled = state.m_OnGUIState.m_Enabled;
#if ENABLE_NEW_EVENT_SYSTEM
	bool mouseOver = screenRect.Contains (evt.touch.pos);
#else
	bool mouseOver = screenRect.Contains (evt.mousePosition);
#endif
	bool isHover = mouseOver && state.m_CanvasGUIState.m_GUIClipState.GetEnabled();
	bool isHover2 = isHover && (hot == controlID || hot == 0);
	
	if (isHover) 
		state.m_CanvasGUIState.m_IsMouseUsed = true;
	
	bool isActive = (controlID == hot) && enabled && mouseOver;
	bool hasKey = state.m_MultiFrameGUIState.m_KeyboardControl == controlID && enabled && state.m_OnGUIState.m_ShowKeyboardControl;
	
	Draw (state, screenRect, content, isHover2, isActive, on, hasKey);	
	
	
	//	#if UNITY_GUI_SUPPORT_TOOLTIP
	if (content.m_Tooltip.text != NULL && content.m_Tooltip.length != 0)
	{
		if (isHover || isActive || hot == controlID)
		{
			SetMouseTooltip (state, content.m_Tooltip, screenRect);
		}
		if (hasKey)
			state.m_OnGUIState.SetKeyTooltip (content.m_Tooltip);
	}
	//	#endif 
}

void GUIStyle::SetMouseTooltip (GUIState& state, const UTF16String& tooltip, const Rectf& screenRect)
{
	state.m_OnGUIState.SetMouseTooltip (tooltip);
#if UNITY_EDITOR
	Vector2f v = GetGUIManager().GetGUIPixelOffset();
	v += state.m_CanvasGUIState.m_GUIClipState.Unclip (Vector2f (screenRect.x, screenRect.y));
	GetTooltipManager().SetTooltip (tooltip, Rectf (v.x, v.y, screenRect.width, screenRect.height));
#endif
	
}


void GUIStyle::Draw (GUIState &state, const Rectf &screenRect, GUIContent &content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
{
	#if UNITY_EDITOR
	if (HighlighterCore::s_SearchMode == kHighlightByContent || HighlighterCore::s_SearchMode == kHighlightAuto)
		HighlighterCore::Handle (state, screenRect, content.m_Text);
	#endif

	// always draw of pixel coordinates
	Rectf rounded = ClampRect (screenRect);
	float right = Roundf (rounded.x + rounded.width);
	float bottom = Roundf (rounded.y + rounded.height);
	rounded.x = Roundf(rounded.x);
	rounded.y = Roundf(rounded.y);
	rounded.width = right - rounded.x;
	rounded.height = bottom - rounded.y;
	
	Rectf visibleRect = state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect();
	
	if (bottom < visibleRect.y || rounded.y > visibleRect.GetBottom())
		return;
	
	isHover = state.m_EternalGUIState->m_AllowHover && isHover;
	const GUIStyleState *gss = GetGUIStyleState (state, isHover, isActive, on, hasKeyboardFocus);
	DrawBackground (state, rounded, gss);
	DrawContent (state, rounded, content, gss);
}

void GUIStyle::DrawWithTextSelection (GUIState &state, const Rectf &screenRect, GUIContent &content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus, bool drawSelectionAsComposition, int cursorFirst, int cursorLast, const ColorRGBAf &cursorColor, const ColorRGBAf &selectionColor) const {
	// always draw of pixel coordinates
	Rectf rounded = ClampRect (screenRect);
	rounded.x = Roundf(rounded.x);
	rounded.y = Roundf(rounded.y);
	rounded.width = Roundf(rounded.width);
	rounded.height = Roundf(rounded.height);
	
	const GUIStyleState *gss = GetGUIStyleState (state, isHover, isActive, on, hasKeyboardFocus);
	DrawBackground (state, rounded, gss);
	if (hasKeyboardFocus)
	{
		if (drawSelectionAsComposition)
		{
			DrawTextUnderline (state, rounded, content, cursorFirst, cursorLast, gss);
			// draw cursor.
			DrawTextSelection (state, rounded, content, cursorLast, cursorLast, cursorColor, selectionColor);
		}
		else
			DrawTextSelection (state, rounded, content, cursorFirst, cursorLast, cursorColor, selectionColor);
	}
	DrawContent (state, rounded, content, gss);
}

void GUIStyle::CalcMinMaxWidth (GUIContent &content, float *minWidth, float *maxWidth) const {
	// If we have a fixed width, that becoms both min and max
	if (m_FixedWidth != 0) {
		*minWidth = *maxWidth = m_FixedWidth;
		return;
	}
	
	TextMeshGenerator2 &tmgen = TextMeshGenerator2::Get (content.m_Text, &GetCurrentFont(), (TextAnchor)m_Alignment, kAuto, 0, s_TabSize, 1.0f, m_RichText, true, ColorRGBA32(0xffffffff), m_Font ? m_FontSize : 0, m_Font ? m_FontStyle : 0);
	Vector2f size = tmgen.GetSize ();
	// If we're word wrapping, we'll never go below 32 pixels.
	// Ideally, we should get the size of the largest word, but that is just too painful.
	*maxWidth = size.x;
	if (m_WordWrap)
		*minWidth = min (32.0f, size.x);
	else
		*minWidth = size.x;
	
	if (content.m_Image) {
		float iWidth = content.m_Image->GetDataWidth();
		switch (m_ImagePosition) {
			case kImageLeft:
				*minWidth += iWidth;
				*maxWidth += iWidth;
				break;
			case kImageOnly:
				*minWidth = *maxWidth = iWidth;
				break;
			case kImageAbove:
				*minWidth = max (iWidth, *minWidth);
				*maxWidth = max (iWidth, *maxWidth);
				break;
			case kTextOnly:
				break;
		}
	}
	
	*minWidth += m_Padding.left + m_Padding.right;
	*maxWidth += m_Padding.left + m_Padding.right;
}

Vector2f GUIStyle::GetCursorPixelPosition (const Rectf &screenRect, GUIContent &content, int cursorStringIndex) const {
	TextMeshGenerator2 *tmgen = GetGenerator (screenRect, content);
	if (tmgen)
		return tmgen->GetCursorPosition(m_Padding.Remove (screenRect), cursorStringIndex) + (m_ContentOffset + m_ClipOffset);
	else
		return Vector2f (0,0);
}

int GUIStyle::GetCursorStringIndex  (const Rectf &screenRect, GUIContent &content, const Vector2f &cursorPixelPosition) const {
	TextMeshGenerator2 *tmgen = GetGenerator (screenRect, content);
	if (tmgen)
		return tmgen->GetCursorIndexAtPosition(m_Padding.Remove (screenRect), cursorPixelPosition - (m_ContentOffset + m_ClipOffset));
	return 0;
}

Font& GUIStyle::GetCurrentFont () const
{
	using namespace GUIStyle_Static;
	Font* thisFont = m_Font;
	if (thisFont != NULL)
		return *thisFont;
	
	Font* defaultFont = s_DefaultFont;
	if (defaultFont != NULL)
		return *defaultFont;
	
	return GetBuiltinFont ();
}

Font &GUIStyle::GetBuiltinFont ()
{
	Font* builtinFont = GUIStyle_Static::s_BuiltinFont;
	if (builtinFont != NULL)
		return *builtinFont;
	
	GUIStyle_Static::s_BuiltinFont = builtinFont = GetBuiltinResource<Font> (kDefaultFontName);
	if (builtinFont == NULL)
	{
		LogString ("Couldn't load default font or font material!");
	}
	
	return *builtinFont;
}

float GUIStyle::GetLineHeight () const {
	Font& f = GetCurrentFont();
	return f.GetLineSpacing (m_FontSize);
}

int GUIStyle::GetNumCharactersThatFitWithinWidth (const UTF16String &text, float width) const
{
	Font &f = GetCurrentFont();
	
	// Call before GetCharacterWidth to ensure character data has been setup
	f.CacheFontForText (text.text, text.length);

	width -= m_Padding.left + m_Padding.right;
	
	float currentWidth = 0;
	unsigned stringlen = text.length;
	int numChars = stringlen;
	for (int i=0; i<stringlen; i++)
	{
		int c = text[i];
		float characterWidth = f.GetCharacterWidth (c, m_FontSize, m_FontStyle);
		if (characterWidth == 0.f)
		{
			return -1; // failed to get character width
		}

		currentWidth += characterWidth;
		if (currentWidth > width)
		{
			numChars = i;
			break;
		}
	}

	return numChars;
}

float GUIStyle::CalcHeight (GUIContent &content, float width) const {
	if (m_FixedHeight != 0.0f)
		return m_FixedHeight;
	
	Vector2f imageSize (0,0), textSize (0,0);
	Texture *image = content.m_Image;
	if (image != NULL) 
		imageSize = Vector2f (image->GetDataWidth(), image->GetDataHeight());
	
	float contentHeight = 0;
	
	TextMeshGenerator2 *tmgen = GetGenerator (Rectf (0,0,width, 1000), content);
	if (tmgen)
		textSize = tmgen->GetSize ();
	switch (m_ImagePosition) {
		case kImageLeft: {
			contentHeight = max (textSize.y, imageSize.y);
			break;
		}
		case kImageAbove: {
			contentHeight = textSize.y + imageSize.y;
			break;
		}
		case kImageOnly:
			contentHeight = imageSize.y;
			break;
		case kTextOnly: {
			contentHeight = textSize.y;
			break;
		}
	}
	
	return contentHeight + m_Padding.top + m_Padding.bottom;
}

Vector2f GUIStyle::CalcSize (GUIContent &content) const {
	Texture *image = content.m_Image;
	
	if (m_FixedHeight != 0.0f && m_FixedWidth != 0.0f)
		return Vector2f (m_FixedWidth, m_FixedHeight);
	
	Vector2f textSize (0,0), imageSize (0,0);
	if (content.m_Text.length != 0 && m_ImagePosition != kImageOnly) 
		textSize = GetGenerator(Rectf (0,0,0,0), content)->GetSize ();
	if (image != NULL && m_ImagePosition != kTextOnly) 
		imageSize = Vector2f (image->GetDataWidth(), image->GetDataHeight());
	
	Vector2f size (0,0);
	switch (m_ImagePosition) {
		case kImageLeft:
			if (imageSize.x > 0) 
			{
				if ((s_GUIStyleIconSizeX != 0) && (s_GUIStyleIconSizeY != 0))
				{
					imageSize.x = s_GUIStyleIconSizeX;
					imageSize.y = s_GUIStyleIconSizeY;
				}
			}
			size = Vector2f (textSize.x + imageSize.x, max (textSize.y, imageSize.y));
			break;
		case kImageAbove:
			size = Vector2f (max (textSize.x, imageSize.x), textSize.y + imageSize.y);
			break;
		case kImageOnly:
			size = imageSize;
			break;
		case kTextOnly:
			size = textSize;
			break;
	}
	
	// If we 
	if (content.m_Text.length == 0 && image == NULL && m_ImagePosition != kImageOnly)
	{
		size.y = GetCurrentFont().GetLineSpacing(m_FontSize);
	}
	size+= m_Padding.Size ();
	if (m_FixedWidth != 0.0f)
		size.x = m_FixedWidth;
	if (m_FixedHeight != 0.0f)
		size.y = m_FixedHeight;
	return size;
}

const GUIStyleState *GUIStyle::GetGUIStyleState (GUIState &state, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
{
	const GUIStyleState *t = NULL;
	if (!on) {
		// If the GUI is disabled, return the normal style
		if (isHover)
			t = m_Hover.background ? &m_Hover : t;
		if (hasKeyboardFocus)
			t = m_Focused.background ? &m_Focused : (m_Hover.background ? &m_Hover : t);
		if (isActive && isHover)
			t = m_Active.background ? &m_Active : t;
		if (!state.m_OnGUIState.m_Enabled) 
			t = &m_Normal;
	} else {
		// If the GUI is disabled, return the normal style
		if (isHover)
			t = m_OnHover.background ? &m_OnHover : t;
		if (hasKeyboardFocus)
			t = m_OnFocused.background ? &m_OnFocused : (m_OnHover.background ? &m_OnHover : t);
		if (isActive && isHover)
			t = m_OnActive.background ? &m_OnActive : t;
		if (!state.m_OnGUIState.m_Enabled) 
			t = &m_Normal;
		
		if (t == NULL || !t->background || !state.m_OnGUIState.m_Enabled)
			t = &m_OnNormal;
	}
	
	if (t == NULL || !t->background)
		t = &m_Normal;
	return t;
}

static void DrawClippedTexture (const Rectf &screenRect, Texture *image, float leftBorder, float rightBorder, float topBorder, float bottomBorder, const ColorRGBAf &color)
{
	DrawGUITexture (screenRect, image, int(leftBorder), int(rightBorder), int(topBorder), int(bottomBorder), color, GetGUIBlendMaterial());
}


void GUIStyle::DrawBackground (GUIState &state, const Rectf &screenRect, const GUIStyleState *gss) const {
#if ENABLE_RETAINEDGUI	
	if (state.m_OnGUIState.m_CaptureBlock)
	{
		if (gss->background)
		{
			GUIVertexData* data = new GUIVertexData (&GUIVertexDataFormat::vtxPosColorUV0UV1);
			GUIUtils::BuildImage (m_Overflow.Add (screenRect), m_Border, gss->background, Vector4f(1.0f, 1.0f, 0.0f, 0.0f), *data);
			GUIClipRegion temp (state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect());
			GUIUtils::BuildClipCoords (&temp, *data);
			
			ColorRGBAf backgroundColor = state.m_OnGUIState.m_Color * state.m_OnGUIState.m_BackgroundColor;
			if (!state.m_OnGUIState.m_Enabled)
				backgroundColor.a *= .5f;
			GUIUtils::BuildColor(backgroundColor, *data);
			
			state.m_OnGUIState.m_CaptureBlock->push_back (GUIGraphicsCacheBlock (data, GetGUIBlendMaterial()));
		}
		return;
	}
#endif
	
	Rectf visibleRect = state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect();
	SetGUIClipRect(visibleRect);
	
	if (gss->background) {
		ColorRGBAf backgroundColor = state.m_OnGUIState.m_Color * state.m_OnGUIState.m_BackgroundColor;
		if (!state.m_OnGUIState.m_Enabled)
			backgroundColor *= ColorRGBAf (1,1,1,.5f);
		
#if UNITY_EDITOR
		OptimizedGUIBlock *block = GetCaptureGUIBlock ();
		if (block)
		{
			block->QueueBackground (m_Overflow.Add (screenRect), gss->background, m_Border, backgroundColor, state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect());
			return;
		} 
#endif		
		
		DrawClippedTexture (m_Overflow.Add (screenRect), gss->background, m_Border.left, m_Border.right, m_Border.top, m_Border.bottom, backgroundColor);
	} 
}

void GUIStyle::RenderText (const Rectf &screenRect, TextMeshGenerator2 &tmgen, ColorRGBAf color) const {
	// Configure the shaders
	Font& currentFont = GetCurrentFont();
	
	Material *material = GetGUITextMaterial ();
	
	// now we set color using TextMeshGenerator vertices, rather then using the material, so set material color to white.
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
		color = ColorRGBA32(0xffffffff);
	
	static SHADERPROP (Color);
	static SHADERPROP (MainTex);
	ShaderLab::PropertySheet& properties = material->GetWritableProperties();
	
	properties.SetVector(kSLPropColor, color.GetPtr());
	
	Texture* fontTexture = currentFont.GetTexture();
	
	// In the case when font is a custum font, GetTexture() will return NULL, so in this case take the main texture from font's material
	if (fontTexture == NULL && currentFont.GetMaterial())
	{
		fontTexture = currentFont.GetMaterial()->GetTexture(kSLPropMainTex);
	}
	
	properties.SetTexture(kSLPropMainTex, fontTexture);
	
	GfxDevice &device = GetGfxDevice ();
	
	float matWorld[16], matView[16];
	
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	
	Matrix4x4f textMatrix;
	
	Vector2f offset = tmgen.GetTextOffset (screenRect);
	
	textMatrix.SetTranslate (Vector3f (offset.x, offset.y, 0.0f));
	
	device.SetViewMatrix (textMatrix.GetPtr());
	
	int passCount = material->GetPassCount ();	
	for (int i=0;i < passCount ;i++)
	{
		const ChannelAssigns* channels = material->SetPass (i);
		tmgen.RenderRaw (*channels);
	}
	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

TextMeshGenerator2* GUIStyle::GetGenerator (const Rectf &screenRect, GUIContent &content, ColorRGBA32 color) const
{
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
		color = 0xffffffff;
	return IMGUI::GetGenerator (m_Padding.Remove (screenRect), content, GetCurrentFont(), (TextAnchor)m_Alignment, m_WordWrap, m_RichText, color, m_FontSize, m_FontStyle, (ImagePosition)m_ImagePosition);
}

TextMeshGenerator2* GUIStyle::GetGenerator (const Rectf &screenRect, GUIContent &content) const
{
	ColorRGBA32 color = 0xffffffff;
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
	{
		GUIState &state = GetGUIState();
		ColorRGBAf imageColor = state.m_OnGUIState.m_Color * state.m_OnGUIState.m_ContentColor;
		ColorRGBAf textColor = m_Normal.textColor * imageColor;
		if (!state.m_OnGUIState.m_Enabled) {
			textColor.a *= .5f;
		}
		color = textColor;
	}
	return IMGUI::GetGenerator (m_Padding.Remove (screenRect), content, GetCurrentFont(), (TextAnchor)m_Alignment, m_WordWrap, m_RichText, color, m_FontSize, m_FontStyle, (ImagePosition)m_ImagePosition);
}


void GUIStyle::CalcContentRects (const Rectf &contentRect, Vector2f imageSize, Vector2f textSize, Rectf &imageRect, Rectf &textRect, float &width, float &height, int imagePosition, int alignment, Vector2f contentOffset)
{
	// Sum them up depending on contents layout.
	width = 0;
	height = 0;
	switch (imagePosition) {
		case kImageLeft:
			// Scale the image down if we need the room
			if (imageSize.x > 0) 
			{
				if ((s_GUIStyleIconSizeX == 0) || (s_GUIStyleIconSizeY == 0))
				{
					float imageScale = clamp (min ((contentRect.Width() - textSize.x) / imageSize.x, contentRect.Height() / imageSize.y), 0.0f, 1.0f);
					
					imageSize.x = Roundf (imageSize.x * imageScale);
					imageSize.y = Roundf (imageSize.y * imageScale);
				}
				else
				{
					imageSize.x = s_GUIStyleIconSizeX;
					imageSize.y = s_GUIStyleIconSizeY;
				}
			}
			
			width = imageSize.x + textSize.x;
			height = max (imageSize.y, textSize.y);
			break;
		case kImageAbove:
			// Scale the image down if we need the room
			if (imageSize.x > 0) 
			{
				if ((s_GUIStyleIconSizeX == 0) || (s_GUIStyleIconSizeY == 0))
				{
					float imageScale = clamp (min ((contentRect.Height() - textSize.y) / imageSize.y, contentRect.Width() / imageSize.x), 0.0f, 1.0f);
					imageSize.x = Roundf (imageSize.x * imageScale);
					imageSize.y = Roundf (imageSize.y * imageScale);
				}
				else
				{
					imageSize.x = s_GUIStyleIconSizeX;
					imageSize.y = s_GUIStyleIconSizeY;
				}
			}
			
			width = max (imageSize.x, textSize.x);
			height = imageSize.y + textSize.y;
			break;
		case kImageOnly:
			// Scale the image down if we need the room
			if (imageSize.x > 0) 
			{
				if ((s_GUIStyleIconSizeX == 0) || (s_GUIStyleIconSizeY == 0))
				{
					float imageScale = min (min (contentRect.Width() / imageSize.x, contentRect.Height() / imageSize.y), 1.0f);
					imageSize.x = Roundf (imageSize.x * imageScale);
					imageSize.y = Roundf (imageSize.y * imageScale);
				}
				else
				{
					imageSize.x = s_GUIStyleIconSizeX;
					imageSize.y = s_GUIStyleIconSizeY;
				}
			}
			
			width = imageSize.x;
			height = imageSize.y;
			break;
		case kTextOnly:
			width = textSize.x;
			height = textSize.y;
			break;
	}
	
	// We now have the combined sizes - need to know where to put them.
	float xAlign = 0, yAlign = 0;
	switch (alignment) {
		case kUpperLeft:
			xAlign = 0; yAlign = 0; break;
		case kUpperCenter:
			xAlign = .5f; yAlign = 0; break;
		case kUpperRight:
			xAlign = 1; yAlign = 0; break;
		case kMiddleLeft:	
			xAlign = 0; yAlign = .5f; break;
		case kMiddleCenter:
			xAlign = .5f; yAlign = .5f; break;
		case kMiddleRight:
			xAlign = 1; yAlign = .5f; break;
		case kLowerLeft:
			xAlign = 0; yAlign = 1; break;
		case kLowerCenter:
			xAlign = .5f; yAlign = 1; break;
		case kLowerRight:
			xAlign = 1; yAlign = 1; break;
	}
	// We need to round as centering the text can position it on half a pixel
	float x = Roundf (contentRect.x + (contentRect.width - width) * xAlign + contentOffset.x);
	float y = Roundf (contentRect.y + (contentRect.height - height) * yAlign + contentOffset.y);
	
	switch (imagePosition) {
		case kImageLeft:
			if (imageSize.x > 0.0f)
				imageRect = Rectf (x, y + (height - imageSize.y) * .5f, imageSize.x, imageSize.y);
			if (textSize.x > 0.0f && imageSize.x > 0.0f)
				textRect = Rectf (x + imageSize.x + 1, y + (height - textSize.y) * .5f, textSize.x, textSize.y);
			else if (textSize.x > 0.0f)
				textRect = Rectf (x, y + (height - textSize.y) * .5f, textSize.x, textSize.y);
			break;
		case kImageAbove:
			if (imageSize.x > 0.0f)
				imageRect = Rectf (Roundf (x + (width - imageSize.x) * .5f), y, imageSize.x, imageSize.y);
			if (textSize.x > 0.0f)
				textRect = Rectf (Roundf (x + (width - textSize.x) * .5f), y + imageSize.y, textSize.x, textSize.y);
			
			break;
		case kImageOnly:
			if (imageSize.x > 0.0f)
				imageRect = Rectf (Roundf (x + (width - imageSize.x) * .5f), y, imageSize.x, imageSize.y);
			break;
		case kTextOnly:
			if (textSize.x > 0.0f)
				textRect = Rectf (x, y, textSize.x, textSize.y);
			break;
	}
}

void GUIStyle::DrawContent (GUIState &state, const Rectf &screenRect, GUIContent &content, const GUIStyleState *gss) const 
{
	Vector2f textSize (0,0), imageSize (0,0);
	TextMeshGenerator2 *tmgen = NULL;
	
	// we now have the location of the contents in the rects. Now to draw it.
	// Setup the color
	ColorRGBAf imageColor = state.m_OnGUIState.m_Color * state.m_OnGUIState.m_ContentColor;
	ColorRGBAf textColor = gss->textColor * imageColor;
	if (!state.m_OnGUIState.m_Enabled) {
		textColor.a *= .5f;
		imageColor.a *= .5f;
	}

	if (m_ImagePosition != kImageOnly && content.m_Text.length != 0)
	{
		tmgen = GetGenerator (screenRect, content, textColor);
		textSize = tmgen->GetSize();
	}
	
	// TODO: If we use word wrapping and imageLeft we should somehow subtract the width...
	Texture *image = content.m_Image;
	if (image != NULL && m_ImagePosition != kTextOnly)
		imageSize = Vector2f (image->GetDataWidth(), image->GetDataHeight());	
	
	Rectf imageRect (0,0,0,0), textRect (0,0,0,0);
	float width, height;
	Rectf contentRect = m_Padding.Remove (screenRect);
	CalcContentRects (contentRect, imageSize, textSize, imageRect, textRect, width, height, m_ImagePosition, m_Alignment, m_ContentOffset);
	
#if ENABLE_RETAINEDGUI
	if (state.m_OnGUIState.m_CaptureBlock)
	{
		GUIVertexData* data = new GUIVertexData (&GUIVertexDataFormat::vtxPosColorUV0UV1);
		GUIUtils::BuildText(textRect, *tmgen, *data);
		
		GUIClipRegion temp (state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect());
		GUIUtils::BuildClipCoords (&temp, *data);
		
		GUIUtils::BuildColor(textColor, *data);
		
		state.m_OnGUIState.m_CaptureBlock->push_back (GUIGraphicsCacheBlock (data, GetGUITextMaterial()));
		return;
	}
#endif
	// We know where the various components go, now we draw them (with lots of clipping shit)
	Rectf visibleRect = state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect();
	
#if UNITY_EDITOR
	OptimizedGUIBlock *block = GetCaptureGUIBlock ();
	if (block)
	{
		Rectf newClipRect, clipRect = visibleRect;
		if (m_Clipping != kOverflow && (width > contentRect.width || height > contentRect.height)) 
		{
			newClipRect = contentRect;
			newClipRect.x += (m_ContentOffset.x + m_ClipOffset.x);
			newClipRect.y += (m_ContentOffset.y + m_ClipOffset.y);
			newClipRect.Clamp (clipRect);
		} else {
			newClipRect = clipRect;	
		}
		
		// Queue the text for later processing
		if (textRect.width != 0.0f) 
		{
			block->QueueText (textRect, tmgen, newClipRect);
		}
		// Render image
		if (imageRect.width != 0.0f)  
			block->QueueTexture (imageRect, image, imageColor, clipRect);
		return;
	} 
#endif
	
	// Do clipping of content
	bool needClipping = false;
	Rectf newClipRect;
	if (m_Clipping != kOverflow && (width > contentRect.width || height > contentRect.height)) {
		needClipping = true;
		newClipRect = contentRect;
		newClipRect.x += (m_ContentOffset.x + m_ClipOffset.x);
		newClipRect.y += (m_ContentOffset.y + m_ClipOffset.y);
		newClipRect.Clamp (visibleRect);
		if (newClipRect.width == 0.0f || newClipRect.height == 0.0f)
			return;
		SetGUIClipRect (newClipRect);
	} else {
		SetGUIClipRect(visibleRect);
	}
	
	// Render text
	if (textRect.width != 0.0f && tmgen != NULL)
	{
		RenderText (textRect, *tmgen, textColor);
	}
	// Render image
	if (imageRect.width != 0.0f)  
		DrawClippedTexture (imageRect, image, 0,0,0,0,imageColor);
	
	// If we used custom clipping, restore it
	if (needClipping)
		SetGUIClipRect (visibleRect);
	
}

Rectf GUIStyle::ClampRect (const Rectf &screenRect) const
{
	return Rectf (screenRect.x, screenRect.y, m_FixedWidth ? m_FixedWidth : screenRect.Width(), m_FixedHeight ? m_FixedHeight : screenRect.Height());
}

void GUIStyle::DrawCursor(GUIState &state, const Rectf &screenRect, GUIContent &content, int position, const ColorRGBAf &cursorColor) const
{
	if (!state.m_OnGUIState.m_Enabled)
		return;
	
	Texture *tex = builtintex::GetWhiteTexture();
	Font& currentFont = GetCurrentFont();
	
	float lineHeight = currentFont.GetLineSpacing (m_FontSize);
	Material *material = GetGUIBlendMaterial();
	
	ColorRGBA32 textureColor = cursorColor * state.m_OnGUIState.m_Color;
	Vector2f cursorPos = GetCursorPixelPosition (screenRect, content, position) - m_ClipOffset;
	DrawGUITexture(Rectf (cursorPos.x, cursorPos.y, 1, lineHeight),tex,textureColor,material);
}

void GUIStyle::DrawTextSelection (GUIState &state, const Rectf &screenRect, GUIContent &content, int first, int last, const ColorRGBAf &cursorColor, const ColorRGBAf &selectionColor) const {
	if (!state.m_OnGUIState.m_Enabled)
		return;
	Texture *tex = builtintex::GetWhiteTexture();
	
	Font& currentFont = GetCurrentFont();
	
	float lineHeight = currentFont.GetLineSpacing (m_FontSize);
	Material *material = GetGUIBlendMaterial();
	
	Rectf visibleRect = state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect();
	SetGUIClipRect(visibleRect);
	
	// If the style uses clipping, just apply it - there's only ever one of these anyways
	Rectf innerRect = m_Padding.Remove (screenRect);
	Rectf clipRect;
	if (m_Clipping) {
		clipRect = visibleRect;
		innerRect.Clamp (visibleRect);
		innerRect.x += (m_ContentOffset.x + m_ClipOffset.x);
		innerRect.y += (m_ContentOffset.y + m_ClipOffset.y);
		
		SetGUIClipRect (innerRect);
	}
	
	// We don't have a selection, only a cursor position
	if (first == last) {
		ColorRGBA32 textureColor = cursorColor * state.m_OnGUIState.m_Color;
		Vector2f cursorPos = GetCursorPixelPosition (screenRect, content, first) - m_ClipOffset;	
		
		// if we auto-size the GUIStyle to the text, and the cursor is after the last character, it always gets clipped.
		// Move it one pixel to the left, so it does not clip
		if (first == content.m_Text.length && cursorPos.x >= screenRect.x + screenRect.width)
			cursorPos.x--;
		
#if UNITY_EDITOR
		OptimizedGUIBlock *block = GetCaptureGUIBlock ();
		if (block)
		{
			block->QueueTexture (Rectf (cursorPos.x, cursorPos.y, 1, lineHeight),tex,textureColor,visibleRect);			
		} else {
			DrawGUITexture(Rectf (cursorPos.x, cursorPos.y, 1, lineHeight),tex,textureColor,material);			
		}
#else
		DrawGUITexture(Rectf (cursorPos.x, cursorPos.y, 1, lineHeight),tex,textureColor,material);
#endif	
	} else {
		ColorRGBA32 textureColor = selectionColor * state.m_OnGUIState.m_Color;
		
		int min = first < last ? first : last;
		int max = first > last ? first : last;
		
		Vector2f minPos = GetCursorPixelPosition (screenRect, content, min) - m_ClipOffset;
		Vector2f maxPos = GetCursorPixelPosition (screenRect, content, max) - m_ClipOffset;
		
		if (minPos.y == maxPos.y) 
		{
#if UNITY_EDITOR
			OptimizedGUIBlock *block = GetCaptureGUIBlock ();
			if (block)
			{
				block->QueueTexture (Rectf (minPos.x, minPos.y, maxPos.x - minPos.x + 1, lineHeight), tex, textureColor, visibleRect);
			} else {
#endif	
				DrawGUITexture (Rectf (minPos.x, minPos.y, maxPos.x - minPos.x + 1, lineHeight), tex, textureColor, material);
#if UNITY_EDITOR
			}
#endif
		} else {
#if UNITY_EDITOR
			OptimizedGUIBlock *block = GetCaptureGUIBlock ();
			if (block)
			{
				// draw the first line - including end part
				block->QueueTexture (Rectf (minPos.x, minPos.y, innerRect.GetRight() - minPos.x, lineHeight), tex, textureColor, visibleRect);
				// Draw the middle part
				block->QueueTexture (Rectf (innerRect.x, minPos.y + lineHeight, innerRect.Width(), maxPos.y - minPos.y - lineHeight), tex, textureColor, visibleRect);
				// Draw the bottom line - up to selection
				if (maxPos.x != innerRect.x)		// Don't draw silly 1px line at the left when a newline is selected
					block->QueueTexture (Rectf (innerRect.x, maxPos.y, maxPos.x - innerRect.x + 1, lineHeight), tex, textureColor, visibleRect);
			} else {
#endif	
				// draw the first line - including end part
				DrawGUITexture (Rectf (minPos.x, minPos.y, innerRect.GetRight() - minPos.x, lineHeight), tex, textureColor, material);
				// Draw the middle part
				DrawGUITexture (Rectf (innerRect.x, minPos.y + lineHeight, innerRect.Width(), maxPos.y - minPos.y - lineHeight), tex, textureColor, material);
				// Draw the bottom line - up to selection
				if (maxPos.x != innerRect.x)		// Don't draw silly 1px line at the left when a newline is selected
					DrawGUITexture (Rectf (innerRect.x, maxPos.y, maxPos.x - innerRect.x + 1, lineHeight), tex, textureColor, material);
#if UNITY_EDITOR
			}
#endif
		}
	}
	if (m_Clipping) 
		SetGUIClipRect (clipRect);
	
}

void GUIStyle::DrawTextUnderline (GUIState &state, const Rectf &screenRect, GUIContent &content, int first, int last, const GUIStyleState *gss) const
{
	if (!state.m_OnGUIState.m_Enabled)
		return;
	
	Rectf visibleRect = state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect();
	
	SetGUIClipRect(visibleRect);
	
	Texture *tex = builtintex::GetWhiteTexture();
	Font& currentFont = GetCurrentFont();
	
	float lineHeight = currentFont.GetLineSpacing (m_FontSize);
	Material *material = GetGUIBlendMaterial();
	
	// If the style uses clipping, just apply it - there's only ever one of these anyways
	Rectf innerRect = m_Padding.Remove (screenRect);
	Rectf clipRect;
	
	if (m_Clipping) {
		clipRect = visibleRect;
		innerRect.Clamp (visibleRect);
		innerRect.x += (m_ContentOffset.x + m_ClipOffset.x);
		innerRect.y += (m_ContentOffset.y + m_ClipOffset.y);
		
		SetGUIClipRect (innerRect);
	}
	
	{
		ColorRGBA32 textColor = gss->textColor * state.m_OnGUIState.m_Color * state.m_OnGUIState.m_ContentColor;
		
		int min = first < last ? first : last;
		int max = first > last ? first : last;
		
		Vector2f pos = GetCursorPixelPosition (screenRect, content, min) - m_ClipOffset;
		Vector2f maxPos = GetCursorPixelPosition (screenRect, content, max) - m_ClipOffset;
		
		float underlineSize = std::max(1.0f, lineHeight*0.03f);
		float underlineOffset = lineHeight*0.95f-underlineSize;
		
		while(pos.y < maxPos.y - 0.01)
		{
#if UNITY_EDITOR
			OptimizedGUIBlock *block = GetCaptureGUIBlock ();
			if (block)
			{
				block->QueueTexture (Rectf (pos.x, pos.y+underlineOffset, innerRect.GetRight() - pos.x + 1, underlineSize), tex, textColor, visibleRect);
			} else
#endif	
				DrawGUITexture (Rectf (pos.x, pos.y+underlineOffset, innerRect.GetRight() - pos.x + 1, underlineSize), tex, textColor, material);
			pos.y += ceilf(lineHeight);
			pos.x = innerRect.x;
		}
#if UNITY_EDITOR
		OptimizedGUIBlock *block = GetCaptureGUIBlock ();
		if (block)
		{
			block->QueueTexture (Rectf (pos.x, pos.y+underlineOffset, maxPos.x - pos.x + 1, underlineSize), tex, textColor, visibleRect);
		} else
#endif	
			DrawGUITexture (Rectf (pos.x, pos.y+underlineOffset, maxPos.x - pos.x + 1, underlineSize), tex, textColor, material);
	}
	if (m_Clipping) 
		SetGUIClipRect (clipRect);
}

void GUIStyle::SetStyleState (int stateIndex, ColorRGBAf textColor, Texture2D *background) {
	GUIStyleState *gss = &m_Normal;
	gss += stateIndex;
	gss->background = background;
	gss->textColor = textColor;
}

void GUIStyle::SetGUIClipRect (const Rectf &screenRect)
{
	s_GUIClipRect = screenRect;
	Matrix4x4f clipMatrix;
	clipMatrix.SetIdentity();
	// In a divide-by-zero case, set these to infinity, so we don't render anything.
	if (screenRect.width > 0.0f)
		clipMatrix.Get (0,0) = (kGUIClipTextureSize-2.0f)/kGUIClipTextureSize / screenRect.width;
	else 
		clipMatrix.Get (0,0) = numeric_limits<float>::infinity ();
	if (screenRect.height > 0.0f)
		clipMatrix.Get (1,1) = (kGUIClipTextureSize-2.0f)/kGUIClipTextureSize / screenRect.height;
	else 
		clipMatrix.Get (1,1) = numeric_limits<float>::infinity ();
	clipMatrix.Get (0,3) = -screenRect.x * clipMatrix.Get (0,0) + 1.0f/kGUIClipTextureSize;
	clipMatrix.Get (1,3) = -screenRect.y * clipMatrix.Get (1,1) + 1.0f/kGUIClipTextureSize;
	clipMatrix.Get (2,2) = clipMatrix.Get (3,3) = 0;		// Kill all perspective/depth: Just put 1 in ZW texcoords
	clipMatrix.Get (2,3) = clipMatrix.Get (3,3) = 1;		// Kill all perspective/depth: Just put 1 in ZW texcoords
	GetGfxDevice().GetBuiltinParamValues().SetMatrixParam(kShaderMatGUIClip, clipMatrix);
}

Rectf GUIStyle::GetGUIClipRect ()
{
	return s_GUIClipRect;
}

MonoBehaviour* GetBuiltinSkin (int skin)
{
	// 0 == Game, 1 == Editor Light, 2 = Editor Dark
	static PPtr<MonoBehaviour> skinObject[3] = { NULL, NULL, NULL };

#if UNITY_EDITOR
		// Load skins (editor version)
		if (!skinObject[0] || !skinObject[1] || !skinObject[2])
		{
			// If this is a devel buildwe want to try and load the skins from the opened project
			// (super useful when skinning the app).
			if (IsDeveloperBuild ()) 
			{
				// Try to load the skins from the current project
				skinObject[0] = dynamic_pptr_cast<MonoBehaviour*> (GetMainAsset ("Assets/DefaultResources/GameSkin/GameSkin.GUISkin"));
				skinObject[1] = dynamic_pptr_cast<MonoBehaviour*> (GetMainAsset (AppendPathName ("Assets/Editor Default Resources/", EditorResources::kLightSkinPath)));
				skinObject[2] = dynamic_pptr_cast<MonoBehaviour*> (GetMainAsset (AppendPathName ("Assets/Editor Default Resources/", EditorResources::kDarkSkinPath)));
			}
			
			// Load the game skin.
			// We can not mark this object as dont save, because that will make it not be unloaded when unloading the player.
			// When that happens in the player the serialized state will be lost. So instead we let it be unloaded and on next
			// load it will be reloaded from disk.
			if (!skinObject[0]) 
			{
				Object *obj = GetBuiltinResourceManager ().GetResource (ClassID(MonoBehaviour), "GameSkin/GameSkin.guiskin");
				skinObject[0] = static_cast<MonoBehaviour*> (obj);
			}
			
			// Load the light inspector skin.
			if (!skinObject[1])
				skinObject[1] = GetEditorAssetBundle()->Get<MonoBehaviour>(EditorResources::kLightSkinPath);
			
			// Load the dark inspector skin.
			if (!skinObject[2])
				skinObject[2] = GetEditorAssetBundle()->Get<MonoBehaviour>(EditorResources::kDarkSkinPath);
		}
#else
		// Players are much easier: we just load the game skin.
		if (!skinObject[0]) 
		{
			Object *obj = GetBuiltinResourceManager ().GetResource (ClassID(MonoBehaviour), "GameSkin/GameSkin.guiskin");
			skinObject[0] = static_cast<MonoBehaviour*> (obj);
		}
#endif

	return skinObject[skin];
}

MonoBehaviour* GetDefaultSkin (int skinMode)
{
#if UNITY_EDITOR
	if (skinMode == 0)
		return GetBuiltinSkin (0);
	return GetBuiltinSkin (GetEditorResources().GetSkinIdx () + 1);
#else
	return GetBuiltinSkin (0);
#endif
}
