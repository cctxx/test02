#ifndef _NAVMESHTILECARVING_H_INCLUDED_
#define _NAVMESHTILECARVING_H_INCLUDED_

struct dtMeshTile;
class dtNavMesh;
class Matrix4x4f;
class Vector3f;
class MinMaxAABB;

void CarveNavMeshTile (const dtMeshTile* tile, dtNavMesh* detourNavMesh, size_t count, const Matrix4x4f* transforms, const Vector3f* sizes, const MinMaxAABB* aabbs);

#endif
