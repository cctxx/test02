#ifndef RUNTIME_HMPOLY
#define RUNTIME_HMPOLY

#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"

// Structure representing a polygon. Based on rcPolyMeshDetail data structure, but dynamic
// to ease morphing of polygons.
struct HMPoly
{
	DECLARE_SERIALIZE (HMPoly)

	HMPoly () : polyIdx (0), ntris (0), nverts (0) {}
	void Reset ();
	void ResetTris ();

	dynamic_array<float>			vertices;		// 3 float per vertices.
	dynamic_array<unsigned short>	triIndices;		// As per Recast, 4 indices per triangle (fourth is junk). They are stored as unsigned short.
	int								polyIdx;		// Index of the polygon this detail poly is the representation of.
	int								ntris;			// Number of triangles.
	int								nverts;			// Number of vertices.
};
typedef std::vector<HMPoly> HMPolys;

// HMVertex representation used to reduce polygons.
struct HMTriangle;
struct HMVertex
{
	HMVertex ();
	HMVertex (Vector3f v,int inId);
	~HMVertex () {};
	void Dispose ();

	void RemoveIfNonNeighbor (HMVertex *n);

	Vector3f					pos;				// Location of point in euclidean space
	std::list<HMVertex*>		neighbor;			// Adjacent vertices
	std::list<HMTriangle*>		tris;				// Triangles this vertex is part of
	int							idx;				// Index in global vertices list.
	float						collapseCost;		// Cached cost of collapsing edge
	HMVertex*					collapseCandidate;	// Candidate vertex for collapseCandidate
};
typedef std::vector<HMVertex*> TRVerts;

// HMTriangle representation used to reduce polygons.
struct HMTriangle
{
	HMTriangle ();
	HMTriangle (HMVertex *v0, HMVertex *v1, HMVertex *v2, int id);
	~HMTriangle () {};
	void Dispose ();

	void        ComputeNormal ();
	void        ReplaceVertex (HMVertex *vold,HMVertex *vnew);
	int         HasVertex (HMVertex *v);

	HMVertex*	vertex[3];
	Vector3f	normal;
	int			idx;				// Index in global triangles list.
};
typedef std::vector<HMTriangle*> TRTris;

inline void HMPoly::Reset ()
{
	ResetTris ();
	polyIdx = -1;
}

inline void HMPoly::ResetTris ()
{
	vertices.clear ();
	triIndices.clear ();
	ntris = 0;
	nverts = 0;
}

template<class TransferFunction>
void HMPoly::Transfer (TransferFunction& transfer)
{
	TRANSFER (vertices);
	TRANSFER (triIndices);

	TRANSFER (polyIdx);
	TRANSFER (ntris);
	TRANSFER (nverts);
}

#endif	//RUNTIME_HMPOLY

