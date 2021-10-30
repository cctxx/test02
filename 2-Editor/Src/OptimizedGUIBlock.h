#ifndef OPTIMIZEDGUIBLOCK_H
#define OPTIMIZEDGUIBLOCK_H

#include "Runtime/Math/Rect.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Editor/Platform/Interface/EditorWindows.h"


class OptimizedGUIBlock 
{
public:
	void QueueText (const Rectf &guiRect, TextMeshGenerator2 *tmGen, const Rectf &clipRect);
	void QueueTexture (const Rectf &guiRect, Texture *texture, const ColorRGBAf &color, const Rectf &clipRect);
	void QueueBackground (const Rectf &guiRect, Texture2D *texture, const RectOffset &border, const ColorRGBAf &color, const Rectf &clipRect);
	void QueueCursorRect (const Rectf &cursorRect, GUIView::MouseCursor cursor);

	void Execute ();
	void Clear ();
	void ClearForReuse ();

	~OptimizedGUIBlock ();
	
	// Terminate curent GUIBlock recording from an exception.
	static void Abandon ();
private:
	struct TextDrawCall 
	{
		// Add the text to this drawcall.
		void AddText (const Rectf &guiRect, TextMeshGenerator2 *tmgen, const Rectf &clipRect);

		Mesh* m_Mesh;
	};
	typedef std::map<PPtr<Font>, TextDrawCall> TextDrawCallContainer;
	TextDrawCallContainer m_TextDrawCalls;
	
	struct ImageDrawCall 
	{
		PPtr<Texture> m_Image;
		Rectf m_ScreenRect;
		RectOffset m_Border;
		Rectf m_ClipRect;
		ColorRGBAf m_Color;
		ImageDrawCall (Texture *image, Rectf screenRect, const RectOffset &border, const ColorRGBAf &color, const Rectf &clipRect);
	};
	std::vector <ImageDrawCall> m_Backgrounds, m_Images;
	void DrawTextures (std::vector <ImageDrawCall> &texturesToDraw);

	struct CursorRect
	{
		Rectf m_CursorRect;
		GUIView::MouseCursor m_Cursor;
		CursorRect (const Rectf cursorRect, GUIView::MouseCursor cursor);
	};
	std::vector<CursorRect> m_CursorRects;
	void ApplyCursorRects ();
};
OptimizedGUIBlock *GetCaptureGUIBlock ();
void SetCaptureGUIBlock (OptimizedGUIBlock* block);

#endif
