#include "UnityPrefix.h"
#include "HeightMesh.h"

HeightMesh::HeightMesh ()
{
}

HeightMesh::~HeightMesh ()
{
	m_tiles.clear ();
}

HeightMesh::HMTile& HeightMesh::AddTile ()
{
	m_tiles.push_back (HMTile ());
	return m_tiles.back ();
}

const HeightMesh::HMTile& HeightMesh::GetTile (int tileID) const
{
	return m_tiles[tileID];
}

void HeightMesh::CompleteTile ()
{
	// Comment this to keep the detail mesh around. Useful to draw it when debugging.
	for (int i = 0; i < m_tiles.size (); ++i)
	{
		m_tiles[i].detailPolys.clear ();
	}
}
