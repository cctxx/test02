#ifndef COMPUTIONALGEOMETRY_H
#define COMPUTIONALGEOMETRY_H

class Vector2f;
class Vector3f;
class Plane;

float SignedTriangleArea2D (const Vector2f& a, const Vector2f& b, const Vector2f& c);
float SignedTriangleArea2D (const Vector3f* v);
float TriangleArea3D (const Vector3f& a, const Vector3f& b, const Vector3f& c);
float CalculateProjectedBoxArea2D (const Vector3f* v);
float CalculateTriangleAreaRotationless (Vector3f* v, float inFovy, float inScreenWidth, float inScreenHeight, Vector3f& viewPoint);
float CalculateTriangleAreaRadialFrustum (Vector3f* v, float inFovy, float inNear, float inFar, float inScreenHeight);
float CalculateBoxAreaRadialFrustum (Vector3f* v, float inFovy, float inNear, float inFar, float inScreenHeight);
float CalculateBoxAreaRadialFrustum2 (Vector3f* v, float a, float b, float c);
float CalculateTriangleAreaRadialFrustum2 (Vector3f* v, float a, float b, float c);
int ClipPolygonAgainstPlane(int vertexCount, const Vector3f *vertex, const Plane& plane, char *location, Vector3f *result);

#endif
