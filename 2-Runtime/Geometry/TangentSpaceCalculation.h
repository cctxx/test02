#ifndef TANGENTSPACECALCULATION_H
#define TANGENTSPACECALCULATION_H

class Vector3f;
class Vector2f;
class Matrix3x3f;

void CreateTangentSpaceTangents (const Vector3f* pPositions, const Vector2f* tex, const Vector3f* normals,
											Matrix3x3f* avgOrthonormalBases, int vertexCount,
											const int* indices, int faceCount);


#endif
