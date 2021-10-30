#include "UnityPrefix.h"
#include "LineBuilder.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"

inline Vector2f Calculate2DLineExtrusionAverage (const Vector3f& p0, const Vector3f& delta, const Vector3f& delta2, float halfWidth)
{
	Vector2f dif;
	dif.x = p0.y * delta.z - p0.z * delta.y;
	dif.y = p0.z * delta.x - p0.x * delta.z;
//	dif = NormalizeFast(dif);

	Vector2f dif2;
	dif2.x = p0.y * delta2.z - p0.z * delta2.y;
	dif2.y = p0.z * delta2.x - p0.x * delta2.z;
//	dif2 = NormalizeFast(dif2);
	
	dif += dif2;
	dif = NormalizeFast(dif);
	
	dif.x *= halfWidth;
	dif.y *= halfWidth;

	return dif;
/*	Vector2f dif;
	dif.x = p0.y * delta.z - p0.z * delta.y;
	dif.y = p0.z * delta.x - p0.x * delta.z;
*/	
}

/// \todo have optional input lengths for speed
/// \todo optimize the dif cross product (z is unused)
void Build3DLine( LineParameters *param, const Vector3f *inVertices, int vertexCount )
{
	Assert(vertexCount > 1);
	Assert(param->outVertices && param->outAABB);
	
	LineVertex *outVertices = param->outVertices;
	Matrix4x4f matrix = param->cameraTransform;

	// As Gradient->GetFixed() needs an unnormalized position in 16.16 format
	// (upper 16 color index, lower 16 - how far between), the max value is
	// (nr of gradient colors - 1)*2^16 - 1
	float fixedMult = (float)(((k3DLineGradientSize - 1) << 16) - 1);

	GfxDevice& device = GetGfxDevice();

	// Skip last vertex
	Vector3f delta = matrix.MultiplyPoint3 (inVertices[0]) - matrix.MultiplyPoint3 (inVertices[1]);
	for (int i=0;i<vertexCount;i++)
	{
		// Don't accumulate by adding deltaU, as the rounding error accumulates as well.
		// Calculate u each time anew instead.
		float u = i/(float)(vertexCount - 1);
		
		// Calculate width and figure a cross section that faces the camera
		Vector3f p0 = matrix.MultiplyPoint3 (inVertices[i]);
		
		if (i+1 != vertexCount)
		{
			Vector3f p1 = matrix.MultiplyPoint3 (inVertices[i+1]);
			delta = p0 - p1;
		}
		
		float width = Lerp(param->startWidth, param->endWidth, u);
		Vector2f dif = Calculate2DLineExtrusion (p0, delta, width * 0.5F);

		ColorRGBA32 color;
		if(param->gradient)
			color = param->gradient->GetFixed (UInt32(u*fixedMult));
		else
			// TODO: rewrite Gradient, so that we can elegantly use it here as well
			color = Lerp((ColorRGBAf)param->color1, (ColorRGBAf)param->color2, u);
		
		// Swizzle color of the renderer requires it
		color = device.ConvertToDeviceVertexColor(color);
		
		// One vertex
		outVertices->vert.Set( p0.x - dif.x, p0.y - dif.y, p0.z );
		outVertices->color = color;
		outVertices->uv.Set( u, 1.0f );
		++outVertices;
		
		// And another vertex
		outVertices->vert.Set( p0.x + dif.x, p0.y + dif.y, p0.z );
		outVertices->color = color;
		outVertices->uv.Set( u, 0.0f );
		++outVertices;
		
		param->outAABB->Encapsulate (inVertices[i]);
	}
	param->outAABB->Encapsulate (inVertices[vertexCount-1]);
}
