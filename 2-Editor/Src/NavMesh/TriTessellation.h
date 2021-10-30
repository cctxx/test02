#ifndef RUNTIME_TRI_TESSELLATION
#define RUNTIME_TRI_TESSELLATION

#include "HMPoly.h"

// Identify planar surfaces and re-tessellate them.
// Returns true if the re-tessellation works.
bool tri_tessellation_planar_surface_retessallation (
	TRVerts& vertices,		// Vertices.
	TRTris& triangles,		// Triangles.
	HMPoly& out_poly);		// Polygon representation in which the tessellated surface will be returned.
							// Only populated if re-tessellation succeeded.

#endif // RUNTIME_TRI_TESSELLATION
