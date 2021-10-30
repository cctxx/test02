#ifndef LINEBUILDER_H
#define LINEBUILDER_H

class MinMaxAABB;

#include "Runtime/Math/Gradient.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Color.h"

enum { k3DLineGradientSize = 5 };

struct LineVertex {
	Vector3f vert;
	ColorRGBA32 color;
	Vector2f uv;
};

// Settings for the Build3DLine function
// Instead of passing lots of parameters, make one of these and use that instead.
struct LineParameters
{
	DECLARE_SERIALIZE (LineParameters)
	
	LineVertex* outVertices;	// Output vertices; 2 * input vertices size
	class MinMaxAABB *outAABB;	// AABB to be generated
	
	// ptr to the gradient used for color generation
	GradientDeprecated<k3DLineGradientSize> *gradient;
	ColorRGBA32 color1;
	ColorRGBA32 color2;
	Matrix4x4f cameraTransform;

	float startWidth;				///< The width (in worldspace) at the line start.
	float endWidth;					///< The width (in worldspace) at the line end.

	LineParameters () : 
		outVertices (NULL), outAABB (NULL),
			gradient (NULL), startWidth (1), endWidth (1),
			color1 (0), color2 (0) { cameraTransform = Matrix4x4f::identity; }
};

template<class TransferFunction>
inline void LineParameters::Transfer (TransferFunction& transfer) {
	TRANSFER_SIMPLE (startWidth);
	TRANSFER_SIMPLE (endWidth);
	transfer.Transfer (color1, "m_StartColor", kSimpleEditorMask);
	transfer.Transfer (color2, "m_EndColor", kSimpleEditorMask);
}

/// build the mesh for a 3D line segement seen from the current camera
/// @param param		generation parameters.
/// @param in 	ptr to the input vertices
/// @param vertexCount	the number of vertices in inVertices
void Build3DLine (LineParameters *param, const Vector3f *in, int vertexCount);

/// Calculates the 2D line extrusion, so that the line is halfWidth * 2 wide and always faces the viewer.
/// The start point is p0, the endpoint is p0 + delta
/// The points are expected to be in camera space.
inline Vector2f Calculate2DLineExtrusion (const Vector3f& p0, const Vector3f& delta, float halfWidth)
{
	#if 1
	Vector2f dif;
	dif.x = p0.y * delta.z - p0.z * delta.y;
	dif.y = p0.z * delta.x - p0.x * delta.z;
	
	dif = NormalizeFast(dif);
	
	dif.x *= halfWidth;
	dif.y *= halfWidth;

	return dif;

	#else

	Vector3f dif = Cross (p0, delta);
	dif = NormalizeFast (dif);
	
	dif.x *= halfWidth;
	dif.y *= halfWidth;

	return Vector2f (dif.x, dif.y);

	#endif
}

#endif
