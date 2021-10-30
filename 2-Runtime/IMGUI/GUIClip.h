#pragma once

#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Misc/InputEvent.h"

#include "Runtime/GfxDevice/GfxDevice.h"

class GUIClip
{
	public:
	
	GUIClip () {}
	
	
	/// The rectangle of this clipping rect.
	/// This is stored absolute coordinates
	Rectf physicalRect;
	
	// The rectangle of the clip in screen space
	Rectf screenRect;
	
	/// physical scrolling offset for coordinates - this is relative to parent.
	Vector2f scrollOffset;
	/// This is absolute
	Vector2f globalScrollOffset;
	
	/// rendering offset. This is the global GUIClip->buffer coordinates
	Vector2f renderOffset;

	/// constructor
	GUIClip (const Rectf& iscreenRect, const Rectf& iphysicalRect, const Vector2f& iscrollOffset, const Vector2f& irenderOffset, const Vector2f& iglobalScrollOffset);
};

// MUST BYTE-MATCH CORRESPONDING STRUCT IN GUISTATEMONO
struct GUIClipState
{
	typedef std::vector<GUIClip> ClipStack;	
	ClipStack m_GUIClips;
	
  private:
	Matrix4x4f m_Matrix;
	Matrix4x4f m_InverseMatrix;
	
	// Where is the mouse onscreen, and where was it last frame (so we can calculate deltas correctly)
	Vector2f m_AbsoluteMousePosition;
	Vector2f m_AbsoluteLastMousePosition;

	Rectf m_VisibleRect;
	
	// Should we disable events?
	int m_Enabled;

  public:	
	GUIClipState ();
	~GUIClipState ();
	
	Rectf GetTopMostPhysicalRect ();
	
	/// Set up the clip rect to contain the entire display.
	/// called by GUI.Begin to initialize the view.
	void BeginOnGUI (InputEvent& ievent);
	void EndOnGUI (InputEvent& ievent);
	void EndThroughException ();
	
	/// This is the low-level function for doing clipping rectangles. Unless you're working with embedded temporary render buffers, this is most likely not what you want.
	/// /absoluteRect/ is the absolute device coordinates this element will be mapped to.
	/// /scrollOffset/ is a scrolling offset to apply.
	/// /renderOffset/ is the rendering offset of the absoluteRect from source to destination. Used to map from an on-screen rectangle to a destination inside a render buffer.
	void Push (InputEvent& ievent, const Rectf& screenRect, Vector2f scrollOffset, const Vector2f& renderOffset, bool resetOffset);
	/// Removes the topmost clipping rectangle, undoing the effect of the latest GUIClip.Push
	void Pop (InputEvent& ievent);
	
	/// Unclips /pos/ to physical device coordinates.
	Vector2f Unclip (const Vector2f& pos);
	Rectf Unclip (const Rectf& rect);
	
	/// Clips /absolutePos/ to drawing coordinates
	Vector2f Clip (const Vector2f& absolutePos);
	Rectf Clip (const Rectf& absoluteRect);
	
	// Return the rect for the topmost clip in screen space
	Rectf GetTopRect();
	
	
	/// Reapply the clipping info. Call this after switching render buffers.
	void Reapply (InputEvent& ievent);
	// Set the GUIMatrix. This is here as this class handles all coordinate transforms anyways.
	const Matrix4x4f& GetMatrix () { return m_Matrix; }
	void SetMatrix (InputEvent& ievent, const Matrix4x4f& m);

	Vector2f GetAbsoluteMousePosition () { return m_AbsoluteMousePosition; }	
	Rectf GetVisibleRect () { return m_VisibleRect; }
	
	bool GetEnabled () const { return m_Enabled; }
  private:
	// Recalculate the mouse values from the absolute screen position into local GUI coordinates, taking cliprects & all into account.
	void CalculateMouseValues (InputEvent& ievent);
	static void LoadPixelMatrix(float left, float right, float bottom, float top, const Matrix4x4f& mat);
	void SetAbsoluteMousePosition (const Vector2f& absoluteMousePosition);
	
	
	// Apply the current clip rect to event pointer positions & render settings for culling, etc.
	void Apply (InputEvent& ievent, GUIClip &topmost);
};
