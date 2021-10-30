#ifndef TEXTMESH
#define TEXTMESH

#include "TextUtil.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Math/Rect.h"

class ChannelAssigns;

#if UNITY_LINUX
class Font;
#endif

//#include "DLAllocator.h"

// Pixel-correct textmesh generator
class TextMeshGenerator2 {
 public: 
//	DECLARE_FORWARD_MAINTHREAD_ALLOCATOR_MEMBER_NEW_DELETE
		
	struct Vertex {
		enum { kFormat = VERTEX_FORMAT3(Vertex, Color, TexCoord0) };
		Vector3f vert;
		ColorRGBA32 color;
		Vector2f uv;
	};

	// The global getter function
	static TextMeshGenerator2 &Get (const UTF16String &text, Font *font, TextAnchor anchor, TextAlignment alignment, float wordWrapWidth, float tabSize, float lineSpacing, bool richText, bool pixelcorrect, ColorRGBA32 color, int fontSize = 0, int fontStyle = 0);

	/// Global update function - this is called once per frame & cleans up any meshes not used since then.
	static void GarbageCollect ();

	/// Delete all text meshes
	static void Flush ();

	static void CleanCache (const UTF16String &text);
	// Individual textmesh objects here on down:

	/// Render this text thing inside Rect.
	// See this code for the correct stup instructions for rendering text with RenderRaw
	void Render (const Rectf &rect, const ChannelAssigns& channels);

	/// Just call the rendering on the GPU - don't set up a matrix to position the text or any such crap. Just submit the mesh.
	void RenderRaw (const ChannelAssigns& channels);
	
	/// Get the modelview offset to position this object's text within rect.
	Vector2f GetTextOffset (const Rectf &rect);
	// Get the font to use - this gets the default font if none set
	Font *GetFont () const;
	// Get the g
	Mesh *GetMesh () const { return m_Mesh; }

	/// Get the cursor position
	Vector2f GetCursorPosition (const Rectf &screenRect, int cursor);

	int GetCursorIndexAtPosition (const Rectf &screenRect, const Vector2f &pos);

	/// Get the size.
	Vector2f GetSize () const { return Vector2f (m_Rect.width, m_Rect.height); }
	
 private:
		
	TextMeshGenerator2 (const UTF16String &text, Font *font, TextAnchor anchor, TextAlignment alignment, float wordWrapWidth, float tabSize, float lineSpacing, bool richText, bool pixelcorrect, ColorRGBA32 color, int fontSize, int fontStyle);
	~TextMeshGenerator2 ();
	void FixLineOffset (float lineWidth, Vertex *firstChar, Vector2f *firstCursor, int count);

	// Generate the mesh stuff from the stored values
	void Generate ();

	float Roundf (float x);
	
	// Width to word-wrap against. If 0, no word-wrapping is applied.
	float m_WordWrapWidth;
		
	TextAnchor m_Anchor;		//< The text anchor.
	TextAlignment m_Alignment;
	float m_LineSpacing;
	float m_TabSize;			//< The pixel tab size
	bool m_TabUsed;			//< Is the tab used in the string?
	bool m_RichText;
	PPtr<Font> m_Font;			//< font to use
	int m_FontSize;
	int m_FontStyle;
	ColorRGBA32 m_Color;
	bool m_PixelCorrect;
	Rectf m_Rect;
	UTF16String m_UTF16Text;
	
	Mesh* m_Mesh;			//< The mesh that contains the generated characters
	std::vector<Vector2f/*, main_thread_allocator<Vector2f>*/ > m_CursorPos;	//< Cursor positions for each character
	int m_LastUsedFrame;			//< Has this textmeshgenerator been used this frame
	
	friend class TextMeshGenerationData;
};
#endif
