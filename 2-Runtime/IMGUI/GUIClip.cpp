#include "UnityPrefix.h"
#include "Runtime/IMGUI/GUIClip.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Math/Quaternion.h"

static const float ko1 = -10000, ko2 = 10000, ko3= 0, ko4 = 0;

GUIClipState::GUIClipState()
{ 
	m_Enabled = 0;
}

GUIClipState::~GUIClipState ()
{
}


/// Push a clip rect to the stack with pixel offsets.
/// This is the low-level function for doing clipping rectangles. Unless you're working with embedded
/// temporary render buffers, this is most likely not what you want.
/// /absoluteRect/ is the absolute device coordinates this element will be mapped to.
/// /scrollOffset/ is a scrolling offset to apply.
/// /renderOffset/ is the rendering offset of the absoluteRect from source to destination. Used to map from an on-screen rectangle to a 
/// destination inside a render buffer.
void GUIClipState::Push (InputEvent& event, const Rectf& screenRect, Vector2f scrollOffset, const Vector2f& renderOffset, bool resetOffset)
{
	if (m_GUIClips.empty())
	{
		ErrorString("GUIClip pushing empty stack not allowed.");
		return;
	}
	
	GUIClip& topmost = m_GUIClips.back ();
	// build absolute offsets by adding parent's position & scroll to the screenRect's positions
	float physicalxMin = screenRect.x + topmost.physicalRect.x + topmost.scrollOffset.x;
	float physicalxMax = screenRect.GetXMax() + topmost.physicalRect.x + topmost.scrollOffset.x;
	float physicalyMin = screenRect.y + topmost.physicalRect.y + topmost.scrollOffset.y;
	float physicalyMax = screenRect.GetYMax() + topmost.physicalRect.y + topmost.scrollOffset.y;
	
	// If the user tries to push a GUIClip with an xMin that goes outside the parent's clipping, we cannot allow that
	// so we move the xMin in and use scrollOffset to hide this.
	if (physicalxMin < topmost.physicalRect.x)
	{ 
		scrollOffset.x += physicalxMin - topmost.physicalRect.x;
		physicalxMin = topmost.physicalRect.x;
	}
	
	// Clip top side (yMin) to the parent as well.
	if (physicalyMin < topmost.physicalRect.y)
	{
		scrollOffset.y += physicalyMin - topmost.physicalRect.y;
		physicalyMin = topmost.physicalRect.y;
	}
	
	// Clip right side (xMax) as well.
	if (physicalxMax > topmost.physicalRect.GetXMax())
	{
		physicalxMax = topmost.physicalRect.GetXMax();
	}
	
	// Clip bottom side (yMax) as well.
	if (physicalyMax > topmost.physicalRect.GetYMax())
	{
		physicalyMax = topmost.physicalRect.GetYMax();
	}
	
	// if the new GUIClip is completely outside parent, sizes can get negative.
	// We just make them be 0, so no rendering is performed.
	if (physicalxMax <= physicalxMin)
		physicalxMax = physicalxMin;
	if (physicalyMax <= physicalyMin)
		physicalyMax = physicalyMin;
	
	// Build the rect straight away
	Rectf absoluteRect = MinMaxRect (physicalxMin, physicalyMin, physicalxMax, physicalyMax);
	
	if (!resetOffset)
	{
		// Maintian global render offset
		m_GUIClips.push_back(GUIClip (screenRect, absoluteRect, scrollOffset, topmost.renderOffset + renderOffset, topmost.globalScrollOffset + scrollOffset));
	}
	else
	{
		// Maintian global scroll offset
		m_GUIClips.push_back (GUIClip (screenRect, absoluteRect, scrollOffset, 
									 Vector2f(absoluteRect.x + scrollOffset.x + renderOffset.x, absoluteRect.y + scrollOffset.y + renderOffset.y),
									 topmost.globalScrollOffset + scrollOffset));
	}
		
	Apply(event, m_GUIClips.back());		
}
		
/// Removes the topmost clipping rectangle, undoing the effect of the latest GUIClip.Push
void GUIClipState::Pop (InputEvent& event)
{
	if (m_GUIClips.size() < 2)
	{
		ErrorString("Invalid GUIClip stack popping");
		return;
	}
	
	m_GUIClips.pop_back();
	Apply(event, m_GUIClips.back());		
}
	
Vector2f GUIClipState::Unclip (const Vector2f& pos)
{
	if (!m_GUIClips.empty())
	{
		GUIClip& topmost = m_GUIClips.back();
		Vector3f res; 
		m_Matrix.PerspectiveMultiplyPoint3 (Vector3f (pos.x, pos.y, 0.0F), res);
		return Vector2f(res.x, res.y) + topmost.scrollOffset + Vector2f (topmost.physicalRect.x, topmost.physicalRect.y);
	}
	else
	{
		return Vector2f (0,0);
	}
}

Rectf GUIClipState::Unclip (const Rectf& rect)
{
	if (!m_GUIClips.empty())
	{
		GUIClip& topmost = m_GUIClips.back();

		return Rectf (rect.x + topmost.scrollOffset.x + topmost.physicalRect.x,
		              rect.y + topmost.scrollOffset.y + topmost.physicalRect.y,
					  rect.width, rect.height);
	}
	else
	{
		return Rectf (0,0,0,0);
	}
}
	
/// Clips /absolutePos/ to drawing coordinates
/// Used for reconverting values calculated from ::ref::Unclip
Vector2f GUIClipState::Clip (const Vector2f& absolutePos)
{
	if (!m_GUIClips.empty())
	{
		GUIClip& topmost = m_GUIClips.back();
		
		Vector3f transformedPoint;
		m_InverseMatrix.PerspectiveMultiplyPoint3(Vector3f(absolutePos.x, absolutePos.y, 0.0F), transformedPoint);
		
		//	return (Vector2)s_InverseMatrix.MultiplyPoint (absolutePos) - topmost.globalScrollOffset - new Vector2 (topmost.physicalRect.x, topmost.physicalRect.y);
		Vector2f res = Vector2f(transformedPoint.x, transformedPoint.y) - topmost.scrollOffset - Vector2f (topmost.physicalRect.x, topmost.physicalRect.y);		
		return Vector2f(res.x, res.y);
	}
	else
	{
		return Vector2f (0,0);
	}
}

/// Convert /absoluteRect/ to drawing coordinates
/// Used for reconverting values calculated from ::ref::Unclip
Rectf GUIClipState::Clip (const Rectf& absoluteRect)
{
	if (!m_GUIClips.empty())
	{
		GUIClip& topmost = m_GUIClips.back();
		
		return Rectf (absoluteRect.x - topmost.globalScrollOffset.x - topmost.physicalRect.x,
					  absoluteRect.y - topmost.globalScrollOffset.y - topmost.physicalRect.y,
					  absoluteRect.width, absoluteRect.height);
	}
	else
	{
		return Rectf (0,0,0,0);
	}
}

// Return the rect for the topmost clip in screen space
Rectf GUIClipState::GetTopRect ()
{
	if (!m_GUIClips.empty())
	{
		GUIClip& topmost = m_GUIClips.back();
		return topmost.screenRect;
	}
	else
	{
		return Rectf (0,0,0,0);
	}
}


/// Reapply the clipping info.
/// Call this after switching render buffers.
void GUIClipState::Reapply (InputEvent& event)
{
	if (!m_GUIClips.empty())
		Apply (event, m_GUIClips.back());
}


void GUIClipState::SetMatrix (InputEvent& event, const Matrix4x4f& m)
{ 
	m_Matrix = m;
	
	Matrix4x4f inverse;
	bool success = Matrix4x4f::Invert_Full(m, inverse);
	if (!success)
	{
		ErrorString ("Ignoring invalid matrix assinged to GUI.matrix - the matrix needs to be invertible. Did you scale by 0 on Z-axis?");
		return;
	}
	
	m_Matrix = m;	// Store the value
	m_InverseMatrix = inverse;
	Reapply (event);		// Reapply the toplevel cliprect.
}

/// constructor
GUIClip::GUIClip (const Rectf& iscreenRect, const Rectf& iphysicalRect, const Vector2f& iscrollOffset, const Vector2f& irenderOffset, const Vector2f& iglobalScrollOffset)
{
	//		Debug.Log ("GUIClipping: " + physicalRect + scrollOffset + renderOffset + globalScrollOffset);
	
	screenRect = iscreenRect;
	physicalRect = iphysicalRect;
	scrollOffset = iscrollOffset;
	renderOffset = irenderOffset;
	globalScrollOffset = iglobalScrollOffset;
}

// Recalculate the mouse values from the absolute screen position into local GUI coordinates, taking cliprects & all into account.
void GUIClipState::CalculateMouseValues (InputEvent& event)
{
	if (!m_GUIClips.empty())
	{
#if ENABLE_NEW_EVENT_SYSTEM
		event.touch.pos = Clip (m_AbsoluteMousePosition);
#else
		event.mousePosition = Clip (m_AbsoluteMousePosition);
#endif
		
		// Check if we're outside the cliprect & set the event ignore flag
		Vector3f res;
		m_InverseMatrix.PerspectiveMultiplyPoint3 (Vector3f(m_AbsoluteMousePosition.x, m_AbsoluteMousePosition.y, 0.0F), res);

		GUIClip& topmost = m_GUIClips.back();
		m_Enabled = topmost.physicalRect.Contains (res.x, res.y) ? -1 : 0;
		
		// scrollwheel is a specialcase
		if (event.type != InputEvent::kScrollWheel)
		{
#if ENABLE_NEW_EVENT_SYSTEM
			event.touch.deltaPos = event.touch.pos - Clip (m_AbsoluteLastMousePosition);
#else
			event.delta = event.mousePosition - Clip (m_AbsoluteLastMousePosition);
#endif
		}
	}
}

/// Apply the current clip rect to OpenGL's viewport & scissor rects.
void GUIClipState::Apply (InputEvent& event, GUIClip &topmost)
{
	// Warp the current event to the correct place in the new coordinate space
	
	CalculateMouseValues (event);

	m_VisibleRect = Rectf (-topmost.scrollOffset.x, -topmost.scrollOffset.y, topmost.physicalRect.width, topmost.physicalRect.height);

	// From here on out, we're only setting up OpenGL, so abort if we're not repainting
	if (event.type != InputEvent::kRepaint)
		return;
	
	// Calculate the viewport (where we end up on screen)
	Rectf r = topmost.physicalRect;
	
	if (r.width < 0) r.width = 0;
	if (r.height < 0) r.height = 0;
	
	r.x -= topmost.renderOffset.x;
	r.y -= topmost.renderOffset.y;
	
	
	r.x = RoundfToInt(r.x);
	r.y = RoundfToInt (r.y);
	r.width = RoundfToIntPos (r.width);
	r.height = RoundfToIntPos (r.height);
	
	Matrix4x4f viewportMatrix;
	viewportMatrix.SetIdentity();
	
	float width, height;
	RenderTexture *rTex = RenderTexture::GetActive ();
	if (rTex)
	{
		width = rTex->GetWidth ();
		height = rTex->GetHeight ();
	}
	else
	{
		width = GetScreenManager ().GetWidth ();
		height = GetScreenManager ().GetHeight (); 
	}
		
	Vector3f scaleFac = Vector3f (r.width / width, r.height / height,1);
	Vector3f move;
	m_Matrix.PerspectiveMultiplyPoint3 (Vector3f (r.x, r.y,0), move);
	
	///@TODO: OPTIMIZE non rotation
	viewportMatrix.SetTRS(Vector3f(move.x * scaleFac.x, move.y * scaleFac.y, 0.0F), Quaternionf::identity(), scaleFac);
	
	SetGLViewport (Rectf (0,0, width, height));
	
	// The ortho bounds passed to LoadPixelMatrix gets multiplied by the matrix as well
	// We need to counter that for the scroll offsets - so stuff like scale doesn't apply to scrolling
	Vector3f sOffset;
	m_Matrix.PerspectiveMultiplyPoint3 (Vector3f(-topmost.scrollOffset.x, -topmost.scrollOffset.y, 0.0F), sOffset);
	sOffset.x *= scaleFac.x;
	sOffset.y *= scaleFac.y;
	
	// Scale X & Y
	// Upload the client coordinate system.
	// Here we multiply in the scaleFac on the scrollOffsets - its something with Matrix ordering - a case of "iterative debugging": I kept chanigng it (at random) until it worked...
	Matrix4x4f pixelMatrix;
	MultiplyMatrices4x4 (&viewportMatrix, &m_Matrix, &pixelMatrix);
	LoadPixelMatrix(
					sOffset.x, Roundf (topmost.physicalRect.width) + sOffset.x, 
					Roundf (topmost.physicalRect.height) + sOffset.y, sOffset.y,
					pixelMatrix
					);
	GUIStyle::SetGUIClipRect(m_VisibleRect);
}

/// Load the pixel matrix and also multiply in a GUIMatrix
void GUIClipState::LoadPixelMatrix(float left, float right, float bottom, float top, const Matrix4x4f& mat)
{
	Rectf rect( left, bottom, right-left, top-bottom );
	Matrix4x4f matrix;
	CalcPixelMatrix (rect, matrix);
	
	// Important: apply half-texel offsets after multiplying the matrix!
	matrix *= mat;
	ApplyTexelOffsetsToPixelMatrix( true, matrix );
	
	GfxDevice& device = GetGfxDevice();
	device.SetProjectionMatrix(matrix);
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity
}

Rectf GUIClipState::GetTopMostPhysicalRect ()
{
	return m_GUIClips.back().physicalRect;
}

//////@TODO:
//	CSRAW public override string ToString () {
//		return System.String.Format ("GUIClip: physicalRect {0}, scrollOffset {1}, renderOffset {2}, globalScrollOffset{3}", physicalRect, scrollOffset, renderOffset, globalScrollOffset);
//	}

/// Set up the clip rect to contain the entire display.
/// called by GUI.Begin to initialize the view.
void GUIClipState::BeginOnGUI (InputEvent& event)
{	
#if ENABLE_NEW_EVENT_SYSTEM
	m_AbsoluteMousePosition = event.touch.pos;
	m_AbsoluteLastMousePosition = m_AbsoluteMousePosition - event.touch.deltaPos;
#else
	m_AbsoluteMousePosition = event.mousePosition;
	m_AbsoluteLastMousePosition = m_AbsoluteMousePosition - event.delta;
#endif
	m_Matrix.SetIdentity();
	m_InverseMatrix.SetIdentity();
	
	m_GUIClips.resize (0);
	// Push in a really large screen, so GUI.matrix scales doesn't crop the root level.
	m_GUIClips.push_back (GUIClip (Rectf (ko1, ko1, 40000,40000), Rectf (ko1, ko1, 40000,40000), Vector2f (ko2, ko2), Vector2f (ko3,ko3), Vector2f (ko4,ko4)));
	
	Apply (event, m_GUIClips.back());
}

void GUIClipState::SetAbsoluteMousePosition (const Vector2f& val)
{
	m_AbsoluteMousePosition = val;
}

/// *End the current GUI stuff.
void GUIClipState::EndOnGUI (InputEvent &event)
{
	InputEvent::Type eventType = event.type;
	if (m_GUIClips.size() != 1 && eventType != InputEvent::kIgnore && eventType != InputEvent::kUsed)
	{
		if (m_GUIClips.size() > 1)
		{
			ErrorString ("GUI Error: You are pushing more GUIClips than you are popping. Make sure they are balanced)");
		}
		else
		{
			ErrorString ("GUI Error: You are popping more GUIClips than you are pushing. Make sure they are balanced)");
			return;	
		}
	}
	m_GUIClips.pop_back();
	// Make sure we restore the mouse values. Otherwise things like GUi.Matrix's modifications to 
	// MouseEvents will seep into the next OnGUI
#if ENABLE_NEW_EVENT_SYSTEM
	event.touch.deltaPos = m_AbsoluteMousePosition - m_AbsoluteLastMousePosition;
	event.touch.pos = m_AbsoluteMousePosition;
#else
	event.delta = m_AbsoluteMousePosition - m_AbsoluteLastMousePosition;
	event.mousePosition = m_AbsoluteMousePosition;
#endif
}

void GUIClipState::EndThroughException ()
{
	m_GUIClips.resize(0);
}
