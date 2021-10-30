#ifndef GUISTYLE_H
#define GUISTYLE_H

#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Math/Color.h"
#include "Runtime/IMGUI/TextUtil.h"
#include "Runtime/Mono/MonoBehaviour.h"

class Font;
struct MonoString;
struct GUIState;
struct GUIContent;
class TextMeshGenerator2;
struct GUIGraphicsCacheBlock;
class GUIClipRegion;

struct GUIStyleState {
	DECLARE_SERIALIZE (GUIStyleState)

	/// Background image used by this style
	PPtr<Texture2D> background;
	/// The color of the text
	ColorRGBAf	     textColor;
	
	GUIStyleState () : textColor (0,0,0,1) {}
	GUIStyleState (const GUIStyleState &other) : background (other.background), textColor (other.textColor) {}
};

template<class TransferFunc>
void GUIStyleState::Transfer (TransferFunc& transfer)
{
	transfer.Transfer (background, "m_Background");
	transfer.Transfer (textColor, "m_TextColor");
}

/// Positioning of the image and the text withing a GUIStyle
enum ImagePosition {
	/// Image is to the left of the text.
	kImageLeft = 0,
	/// Image is above the text.
	kImageAbove = 1,
	/// Only the image is displayed.
	kImageOnly = 2,
	/// Only the text is displayed.
	kTextOnly = 3
};
	


struct RectOffset {
	DECLARE_SERIALIZE (RectOffset)

	int left, right, top, bottom;

	RectOffset () { left = right = top = bottom = 0; }
	RectOffset (const RectOffset &other) 
	{ 
		left = other.left;
		right = other.right;
		top = other.top;
		bottom = other.bottom; 
	}
	
	int GetHorizontal () { return left + right; }
 	int GetVertical () { return top + bottom; }

	Rectf Add (const Rectf &r) const
	{
		return MinMaxRect (r.x - left, r.y - top, r.GetRight() + right, r.GetBottom() + bottom);
	}
	
	Vector2f Size () const {
		return Vector2f (left + right, top + bottom);
	}
	Rectf Remove (const Rectf &r) const
	{
		return MinMaxRect (r.x + left, r.y + top, r.GetRight() - right, r.GetBottom() - bottom);
	}	
};

template<class TransferFunc>
void RectOffset::Transfer (TransferFunc& transfer)
{
	transfer.Transfer (left, "m_Left");
	transfer.Transfer (right, "m_Right");
	transfer.Transfer (top, "m_Top");
	transfer.Transfer (bottom, "m_Bottom");
}

class GUIStyle {
  public:
	DECLARE_SERIALIZE (GUIStyle);
	GUIStyle ();
	GUIStyle (const GUIStyle &other);
	UnityStr m_Name;

	GUIStyleState m_Normal;
	GUIStyleState m_Hover;
	GUIStyleState m_Active;
	GUIStyleState m_Focused;
	GUIStyleState m_OnNormal;
	GUIStyleState m_OnHover;
	GUIStyleState m_OnActive;
	GUIStyleState m_OnFocused;

	/// Border of the background images
	RectOffset m_Border;

	/// Spacing between this element and ones next to it
	RectOffset m_Margin;
	
	/// Distance from outer edge to contents
	RectOffset m_Padding;
	
	/// Extra size to use for the background images.
	RectOffset m_Overflow;
	
	/// The font to use. If not set, the font is read from the main GUISkin
	PPtr<Font> m_Font;

	/// Text alignment.
	int m_Alignment;				///< enum { Upper Left = 0, Upper Center = 1, Upper Right = 2, Middle Left = 3, Middle Center = 4, Middle Right = 5, Lower Left = 6, Lower Center = 7, Lower Right = 8 } How is the content placed inside the control.

	/// Word wrap the text?
	bool m_WordWrap;
	
	/// Use HTML-style markup
	bool m_RichText;
	
	/// Clipping mode to use.
	int m_Clipping;				///< enum { Overflow = 0, Clip = 1 }   What happens with content that goes outside the control
	
	/// How image and text is combined.
	int m_ImagePosition;			///< enum { Image Left = 0, Image Above = 1, Image Only = 2, Text Only = 3 }   How text and image is placed in relation to each other.

	/// Pixel offset to apply to the content of this GUIstyle.
	Vector2f m_ContentOffset;
	
	/// Clip offset
	Vector2f m_ClipOffset;

	float m_FixedWidth; ///< If non-0, that axis is always draw at the specified size.
	float m_FixedHeight; ///< If non-0, that axis is always draw at the specified size.

	/// The font size to use. Set to 0 to use default font size. Only applicable for dynamic fonts.
	int m_FontSize;

	/// The font style to use. Only applicable for dynamic fonts.
	int m_FontStyle;				///< enum { Normal = 0, Bold = 1, Italic = 2, Bold and Italic = 3 }		Only applicable for dynamic fonts.

	bool m_StretchWidth, m_StretchHeight;

	/// Draw this GUI style
	/// screenRect:		Rectangle for the border
	/// content:			The text/image to stuff inside
	/// isHover:		Is the mouse over the element
	/// isActive:		Does the element have keyboard focus
	/// on:			Is the element on (as in a togglebutton)
	void Draw (GUIState &state, const Rectf &screenRect, GUIContent &content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const ;

	/// Draw this GUI style
	/// screenRect:		Rectangle for the border
	/// content:		The text/image to stuff inside
	/// id:				The controlID mouse of the element
	/// isActive:		Does the element have keyboard focus
	/// on:			Is the element on (as in a togglebutton)
	void Draw (GUIState &state, const Rectf &screenRect, GUIContent &content, int controlID, bool on) const ;
	

	/// screenRect:		Rectangle for the border
	/// content:			The text/image to stuff inside
	/// isHover:		Is the mouse over the element
	/// isActive:		Does the element have keyboard focus
	/// on:			Is the element on (as in a togglebutton)
	/// cursorFirst, last	Where is the text selection
	void DrawWithTextSelection (GUIState &state, const Rectf &screenRect, GUIContent &content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus, bool drawSelectionAsComposition, int cursorFirst, int cursorLast, const ColorRGBAf &cursorColor, const ColorRGBAf &selectionColor) const;

	/// screenRect:		Rectangle for the border
	/// text/image:			The text/image to stuff inside
	/// position		Where is the text selection
	void DrawCursor(GUIState &state, const Rectf &screenRect, GUIContent &content, int position, const ColorRGBAf &cursorColor) const;

	/// Calculate the min & max widths of this element to correctly render the content
	void CalcMinMaxWidth (GUIContent &content, float *minWidth, float *maxWidth) const;
	/// Calculate the height  of a component given a specific width
	float CalcHeight (GUIContent &content, float width) const;
	/// Calculate the size 
	Vector2f CalcSize (GUIContent &content) const;
	/// Get the position of a specific character (used for finding out where to draw the cursor)	
	Vector2f GetCursorPixelPosition (const Rectf &screenRect, GUIContent &content, int cursorStringIndex) const;
	/// Get the index of a given pixel position (used for finding out where in the string of a textfield the user clicked)
	int GetCursorStringIndex  (const Rectf &screenRect, GUIContent &content, const Vector2f &cursorPixelPosition) const;

	/// Get the height of one line, in pixels...
	float GetLineHeight () const;
	
	/// returns number of characters that can fit within width, returns -1 if fails to shorten string
	int GetNumCharactersThatFitWithinWidth (const UTF16String &text, float width) const;

	/// Set the default font (used by GUISkin)
	static void SetDefaultFont (Font *font);
	static Font*GetDefaultFont ();
	
	static Texture2D* GetClipTexture ();

	Font& GetCurrentFont () const;
	static Font &GetBuiltinFont ();

	/// Set a specific style state. Used when changing style states from mono
	void SetStyleState (int stateIndex, ColorRGBAf textColor, Texture2D *background);

	// Calculate where text and image go inside screenRect in order to fit imageSize and textSize
	static void CalcContentRects (const Rectf &contentRect, Vector2f imageSize, Vector2f textSize, Rectf &imageRect, Rectf &textRect, float &width, float &height, int imagePosition, int alignment, Vector2f contentOffset);

	// Used by OptimizedGUIBlock
	static Rectf GetGUIClipRect ();
	static void SetGUIClipRect (const Rectf& rect);
	
	static void SetMouseTooltip (GUIState& state, const UTF16String& tooltip, const Rectf& screenRect);
  private:
	
	/// Figure out which GUIStyle to use
	const GUIStyleState *GetGUIStyleState (GUIState &state, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const;
	/// Draw the background GUIStyle without any contents
	void DrawBackground (GUIState &state, const Rectf &screenRect, const GUIStyleState *gss) const;


	/// Draw the contents 
	void DrawContent (GUIState &state, const Rectf &screenRect, GUIContent &content, const GUIStyleState *gss) const;
	/// Draw the text selection highlight.
	void DrawTextSelection (GUIState &state, const Rectf &screenRect, GUIContent &content, int first, int last, const ColorRGBAf &cursorColor, const ColorRGBAf &selectionColor) const;
	void DrawTextUnderline (GUIState &state, const Rectf &screenRect, GUIContent &content, int first, int last, const GUIStyleState *gss) const;
	
	// set up all the static text-rendering vars from this settings.
	void RenderText (const Rectf &screenRect, TextMeshGenerator2 &tmgen, ColorRGBAf color) const;

	TextMeshGenerator2 *GetGenerator (const Rectf &screenRect, GUIContent &content, ColorRGBA32 color) const;
	TextMeshGenerator2 *GetGenerator (const Rectf &screenRect, GUIContent &content) const;

	// Clamp a screenRect to be set to fixedWidth & height
	Rectf ClampRect (const Rectf &screenRect) const;
	
};
template<class TransferFunc>
void GUIStyle::Transfer (TransferFunc& transfer)
{
	TRANSFER (m_Name);
	transfer.Align ();
	TRANSFER (m_Normal);
	TRANSFER (m_Hover);
	TRANSFER (m_Active);
	TRANSFER (m_Focused);
	TRANSFER (m_OnNormal);
	TRANSFER (m_OnHover);
	TRANSFER (m_OnActive);
	TRANSFER (m_OnFocused);
	
	TRANSFER (m_Border);
	TRANSFER (m_Margin);
	TRANSFER (m_Padding);
	TRANSFER (m_Overflow);
	TRANSFER (m_Font);
	TRANSFER (m_FontSize);
	TRANSFER (m_FontStyle);
	TRANSFER (m_Alignment);
	TRANSFER (m_WordWrap);
	TRANSFER (m_RichText);
	transfer.Align();
	transfer.Transfer (m_Clipping, "m_TextClipping");
	TRANSFER (m_ImagePosition);
	TRANSFER (m_ContentOffset);
	TRANSFER (m_FixedWidth);
	TRANSFER (m_FixedHeight);
	TRANSFER (m_StretchWidth);
	TRANSFER (m_StretchHeight);
	transfer.Align();
};

Material* GetGUIBlitMaterial ();
Material* GetGUIBlendMaterial ();

void InitializeGUIClipTexture();

// skin:
// Game = 0
// Light Skin = 1
// Dark Skin = 2
MonoBehaviour* GetBuiltinSkin (int skin);
// skinMode:
// Game = 0
// Editor = 1
MonoBehaviour* GetDefaultSkin (int skinMode);

#endif
