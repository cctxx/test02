#ifndef _NAVMESHTILECONVERSION_H_INCLUDED_
#define _NAVMESHTILECONVERSION_H_INCLUDED_

struct dtMeshTile;
class DynamicMesh;
class Vector3f;

bool TileToDynamicMesh (const dtMeshTile* tile, DynamicMesh& mesh, const Vector3f& tileOffset);
unsigned char* DynamicMeshToTile (int* dataSize, const DynamicMesh& mesh, const dtMeshTile* sourceTile, const Vector3f& tileOffset);

#endif
