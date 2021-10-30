#ifndef RUNTIME_HEIGHT_MESH
#define RUNTIME_HEIGHT_MESH

#include "HMPoly.h"

class HeightMesh
{
public:
	struct HMTile
	{
		HMPolys	detailPolys;
		HMPolys heightMeshPolys;
	};

	HeightMesh ();
	~HeightMesh ();

	HMTile& AddTile ();
	const HMTile& GetTile (int tileID) const;
	void CompleteTile ();

private:

	std::vector<HMTile> m_tiles;
};

#endif
