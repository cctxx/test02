#include "UnityPrefix.h"
#include "HMPoly.h"

template<class T>
inline bool contains (std::list<T>& l, const T& t)
{
	return std::find (l.begin (), l.end (), t) != l.end ();
}

template<class T>
inline void add_unique (std::list<T>& l, const T& t)
{
	if (contains (l, t))
		return;

	l.push_back (t);
}

template<typename T>
inline void remove (std::list<T>& l, const T& t)
{
	typename std::list<T>::iterator found = std::find (l.begin (), l.end (), t);
	if (found == l.end ())
		return;
	l.erase (found);
}

HMTriangle::HMTriangle ()
: idx (-1)
{
	vertex[0] = NULL;
	vertex[1] = NULL;
	vertex[2] = NULL;
}

HMTriangle::HMTriangle (HMVertex* v0, HMVertex* v1, HMVertex* v2, int inId)
: idx (inId)
{
	vertex[0] = v0;
	vertex[1] = v1;
	vertex[2] = v2;

	ComputeNormal ();

	for (int i = 0; i<3; i++)
	{
		vertex[i]->tris.push_back (this);
		for (int j = 0; j < 3; j++)
		{
			if (i != j)
				add_unique (vertex[i]->neighbor, vertex[j]);
		}
	}
}

void HMTriangle::Dispose ()
{
	for (int i = 0; i < 3; i++)
	{
		if (vertex[i])
		{
			remove (vertex[i]->tris, this);
		}
	}

	for (int i = 0; i < 3; i++)
	{
		int i2 = (i+1)%3;
		if (!vertex[i] || !vertex[i2]) continue;

		vertex[i ]->RemoveIfNonNeighbor (vertex[i2]);
		vertex[i2]->RemoveIfNonNeighbor (vertex[i ]);
	}
}


int HMTriangle::HasVertex (HMVertex* v)
{
	return (v == vertex[0] || v == vertex[1] || v == vertex[2]);
}


void HMTriangle::ComputeNormal ()
{
	Vector3f v0 = vertex[0]->pos;
	Vector3f v1 = vertex[1]->pos;
	Vector3f v2 = vertex[2]->pos;
	normal = Cross ((v1-v0), (v2-v1));

	if (Magnitude (normal) == 0)
		return;

	normal = NormalizeSafe (normal);
}


void HMTriangle::ReplaceVertex (HMVertex* vold, HMVertex* vnew)
{
	AssertIf (vold == NULL || vnew == NULL);
	AssertIf (vold != vertex[0] && vold != vertex[1] && vold != vertex[2]);
	AssertIf (vnew == vertex[0] && vnew == vertex[1] && vnew == vertex[2]);

	// Update triangle vertices.
	if (vold == vertex[0])
		vertex[0] = vnew;
	else if (vold == vertex[1])
		vertex[1] = vnew;
	else
		vertex[2] = vnew;

	// Remove vold from tris, add as part of the new tris.
	int i;
	remove (vold->tris, this);
	AssertIf (contains (vnew->tris, this));
	vnew->tris.push_back (this);

	// Break vold neighbor relation.
	for (i = 0; i < 3; i++)
	{
		vold->RemoveIfNonNeighbor (vertex[i]);
		vertex[i]->RemoveIfNonNeighbor (vold);
	}

	// Create vnew neighbor relation.
	for (i = 0; i < 3; i++)
	{
		AssertIf (!contains (vertex[i]->tris, this));
		for (int j = 0; j < 3; j++)
		{
			if (i != j)
			{
				add_unique (vertex[i]->neighbor, vertex[j]);
			}
		}
	}

	ComputeNormal ();
}

HMVertex::HMVertex ()
: idx (-1)
, collapseCost (1000000.0f)
, collapseCandidate (NULL)
{
}


HMVertex::HMVertex (Vector3f v, int inId)
: pos (v)
, idx (inId)
, collapseCost (1000000.0f)
, collapseCandidate (NULL)
{
}


void HMVertex::Dispose ()
{
	while (!neighbor.empty ())
	{
		remove ((*neighbor.begin ())->neighbor, this);
		remove (neighbor, (*neighbor.begin ()));
	}
}


void HMVertex::RemoveIfNonNeighbor (HMVertex* n)
{
	// removes n from neighbor l if n isn't a neighbor.
	if (!contains (neighbor, n))
		return;

	for (std::list<HMTriangle*>::iterator it = tris.begin (); it != tris.end (); ++it)
	{
		if ((*it)->HasVertex (n))
			return;
	}

	remove (neighbor, n);
}

#include "HMPolyTests.h"
