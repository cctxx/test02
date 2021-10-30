#ifndef RUNTIME_TRI_REDUCTION
#define RUNTIME_TRI_REDUCTION

#include "HMPoly.h"

// Collapse all triangles that doesn't alter the geometry.
void tri_reduction_collapse_no_cost (
	HMPoly& poly		// The polygon to collapse for which to collapse triangles.
);
void tri_reduction_collapse_no_cost (
	TRVerts& vertices,
	TRTris& triangles);

// Remove non-walkable surfaces
void tri_reduction_remove_non_walkable (
	TRVerts& vertices,
	TRTris& triangles,
	dynamic_array<bool>& triWalkableFlags);

// Convert an HMPoly to a HMVertex / HMTriangle representation allowing
// to use HMTriangle Reduction algorithm.
void tri_reduction_convert (
	HMPoly& poly,
	TRVerts& vertices,
	TRTris& triangles);

void tri_reduction_convert_back (
	TRVerts& vertices,
	TRTris& triangles,
	HMPoly& poly);

void tri_reduction_clean_up (
	TRVerts& vertices,
	TRTris& triangles);

#endif // RUNTIME_TRI_REDUCTION
