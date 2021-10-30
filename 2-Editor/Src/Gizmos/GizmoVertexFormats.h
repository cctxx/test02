#ifndef GIZMO_VERTEX_FORMAT_H
#define GIZMO_VERTEX_FORMAT_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Color.h"

namespace gizmos
{
	struct ColorVertex
	{
		ColorVertex() { }
		ColorVertex( const Vector3f& v, const ColorRGBA32& c ) : vertex(v), color(c) { }

		Vector3f vertex;
		ColorRGBA32 color;
	};

	struct LitVertex
	{
		Vector3f vertex;
		Vector3f normal;
	};
}
	
#endif
