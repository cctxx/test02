#ifndef GUITEXT_H
#define GUITEXT_H

#include <string>
#include "GUIElement.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector2.h"

class Font;
namespace Unity { class Material; }

/// Can be Attached to any game object in the scene.
/// Registers with GUILayer, GUILayer renders it.
/// Position comes from transform.position.x,y
/// size comes from transform.scale.x,y
class GUIText : public GUIElement
{
	public:
	
	REGISTER_DERIVED_CLASS (GUIText, GUIElement)
	DECLARE_OBJECT_SERIALIZE (GUIText)
		
	GUIText (MemLabelId label, ObjectCreationMode mode);	
	virtual void Reset ();

	const UnityStr& GetText () const { return m_Text; }
	void SetText (const std::string& text) { m_Text = text; }

	// GUIElement
	virtual void RenderGUIElement (const Rectf& cameraRect);
	virtual Rectf GetScreenRect (const Rectf& cameraRect);

	void SetFont (PPtr<Font> font);
	Font * GetFont () const;

	Material* GetMaterial ();
	void SetMaterial (Material* material);

	void SetPixelOffset (const Vector2f& offset) { m_PixelOffset = offset; SetDirty(); }
	Vector2f GetPixelOffset () { return m_PixelOffset; }

	void SetLineSpacing (float space) { m_LineSpacing = space; SetDirty(); }
	float GetLineSpacing() const { return m_LineSpacing; }

	void SetTabSize (float size) { m_TabSize = size; SetDirty(); }
	float GetTabSize() const { return m_TabSize; }

	void SetAlignment (int align) { m_Alignment = align; SetDirty(); }
	int GetAlignment() const { return m_Alignment; }

	void SetAnchor (int size) { m_Anchor = size; SetDirty(); }
	int GetAnchor() const { return m_Anchor; }

	void SetFontSize (int size) { m_FontSize = size; SetDirty(); }
	int GetFontSize() const { return m_FontSize; }

	void SetFontStyle (int style) { m_FontStyle = style; SetDirty(); }
	int GetFontStyle() const { return m_FontStyle; }

	void SetRichText (bool richText) { m_RichText = richText; SetDirty(); }
	bool GetRichText() const { return m_RichText; }

	void SetColor (ColorRGBA32 color) { m_Color = color; SetDirty(); }
	ColorRGBA32 GetColor() const { return m_Color; }

	private:

	std::pair<Font*, Material*> GetFontAndMaterial ();

	UnityStr m_Text;
	
	short m_Alignment;   ///< enum { left, center, right }
	short m_Anchor;      ///< Where the text-mesh is anchored related to local origo. enum { upper left, upper center, upper right, middle left, middle center, middle right, lower left, lower center, lower right }
	
	float m_LineSpacing; 	///< Spacing between lines as multiplum of height of a character.
	float m_TabSize;     	///< Length of one tab
	bool m_PixelCorrect; 	///< Place & scale the text to a pixel-correct position.
	bool m_RichText;///< Enable HTML-style tags for text formatting.
	Vector2f m_PixelOffset;

	int m_FontSize; ///<The font size to use. Set to 0 to use default font size. Only applicable for dynamic fonts.
	int m_FontStyle;	///<The font style to use. Only applicable for dynamic fonts. enum { Normal, Bold, Italic, Bold and Italic }

	ColorRGBA32 m_Color;

	PPtr<Font> m_Font;
	PPtr<Material> m_Material;
};


void DrawGUIText (const std::string& text, Font* font, Material* material);

#endif
