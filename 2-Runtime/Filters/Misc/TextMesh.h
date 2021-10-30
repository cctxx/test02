#ifndef TEXTMESH_H
#define TEXTMESH_H

#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Geometry/AABB.h"
#include <string>
#include <vector>

using std::vector;
class Font;
class Mesh;




class TextMesh : public Unity::Component {
 public:
	REGISTER_DERIVED_CLASS (TextMesh, Component)
	DECLARE_OBJECT_SERIALIZE (TextMesh)

	TextMesh (MemLabelId label, ObjectCreationMode mode);
	// ~TextMesh (); declared-by-macro
	
	const UnityStr& GetText () const { return m_Text; }
	void SetText (const std::string& text);

	void SetFont (PPtr<Font> font);
	Font * GetFont () const;
	
	void SetFontSize (int size);
	int GetFontSize() const { return m_FontSize; }

	void SetFontStyle (int style);
	int GetFontStyle() const { return m_FontStyle; }

	void SetOffsetZ(float offset);
	float GetOffsetZ(){ return m_OffsetZ; }

	void SetAlignment(short alignment);
	short GetAlignment(){ return m_Alignment; }

	void SetAnchor(short anchor);
	short GetAnchor(){ return m_Anchor; }

	void SetCharacterSize(float characterSize);
	float GetCharacterSize(){ return m_CharacterSize; }

	void SetLineSpacing(float lineSpacing);
	float GetLineSpacing(){ return m_LineSpacing; }

	void SetTabSize(float tabSize);
	float GetTabSize(){ return m_TabSize; }	

	void SetRichText(bool richText);
	bool GetRichText() { return m_RichText; }	

	void SetColor(const ColorRGBA32 color);
	ColorRGBA32 GetColor() const { return m_Color; } 
	
	void AwakeFromLoad(AwakeFromLoadMode mode);
	
	virtual void Reset ();
	void DidAddComponent ();

	static void InitializeClass();	
	static void CleanupClass() {}
	
	void ApplyToMesh ();
 private:	
	
	void SetupMeshRenderer();
	
	UnityStr m_Text;
	
	PPtr<Font> m_Font;
	
	float m_OffsetZ;   ///< How much to offset the generated mesh from the Z-position=0.
	short m_Alignment; ///< enum { left, center, right }
	short m_Anchor;    ///< Where the text-mesh is anchored related to local origo. enum { upper left, upper center, upper right, middle left, middle center, middle right, lower left, lower center, lower right }
	float m_CharacterSize; ///< Size of one character (as its height, since Aspect may change its width)
	float m_LineSpacing;   ///< Spacing between lines as multiplum of height of a character.
	float m_TabSize;   ///< Length of one tab

	int m_FontSize; ///<The font size to use. Set to 0 to use default font size. Only applicable for dynamic fonts.
	int m_FontStyle;	///<The font style to use. Only applicable for dynamic fonts. enum { Normal, Bold, Italic, Bold and Italic }
	
	ColorRGBA32 m_Color;
	bool m_RichText;
	
	Mesh* GetMesh ();
	
	Mesh* m_Mesh;
};

#endif
