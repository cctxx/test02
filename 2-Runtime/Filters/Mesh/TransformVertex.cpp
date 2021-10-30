#include "UnityPrefix.h"
#include "TransformVertex.h"

#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Color.h"

#include "Runtime/Misc/CPUInfo.h"

void
TransformVerticesStridedREF( StrideIterator<Vector3f> inPos, StrideIterator<Vector3f> inNormal,
							 StrideIterator<ColorRGBA32> inColor, StrideIterator<Vector2f> inTexCoord0, StrideIterator<Vector2f> inTexCoord1,
							 StrideIterator<Vector4f> inTangent,
							 UInt8* dstData, const Matrix4x4f& m, unsigned vertexCount, bool multiStream )
{
	// NOTE: kill this code once all shaders normalize normals & tangents!
	//
	// We batch uniformly scaled objects, so derive the "normal matrix" here by scaling world matrix axes.
	// On reference code seems much cheaper than full normalization of normal/tangent vectors.
	// Test with scene of 200k vertices on Core i7 2600K: no handling of scale 3.77ms, normalization 8.00ms,
	// using scaled normal matrix 3.80ms.
	//
	// Note that ARM NEON/VFP transformation code does not handle this, but it's not needed on GLES platforms
	// since shaders always normalize normal & tangent. Might be needed on WinRT+ARM though (or just disable
	// dynamic batching with tangents there).
	Matrix4x4f nm;
	CopyMatrix(m.GetPtr(), nm.GetPtr());
	const float axisLen = Magnitude (m.GetAxisX());
	float scale = axisLen > 1.0e-6f ? 1.0f / axisLen : 1.0f;
	nm.Get (0, 0) *= scale;
	nm.Get (1, 0) *= scale;
	nm.Get (2, 0) *= scale;
	nm.Get (0, 1) *= scale;
	nm.Get (1, 1) *= scale;
	nm.Get (2, 1) *= scale;
	nm.Get (0, 2) *= scale;
	nm.Get (1, 2) *= scale;
	nm.Get (2, 2) *= scale;

	while (vertexCount --> 0)
	{
		Vector3f* outPos = reinterpret_cast<Vector3f*> (dstData);
		m.MultiplyPoint3(*inPos, *outPos);
		dstData += sizeof(Vector3f);
		++inPos;

		if (inNormal.GetPointer())
		{
			Vector3f* outNormal = reinterpret_cast<Vector3f*> (dstData);
			nm.MultiplyVector3(*inNormal, *outNormal);
			dstData += sizeof(Vector3f);
			++inNormal;
		}

		if (inColor.GetPointer())
		{
			memcpy(dstData, inColor.GetPointer(), sizeof(ColorRGBA32));
			dstData += sizeof(ColorRGBA32);
			++inColor;
		}

		if (inTexCoord0.GetPointer())
		{
			memcpy(dstData, inTexCoord0.GetPointer(), sizeof(Vector2f));
			dstData += sizeof(Vector2f);
			++inTexCoord0;
		}

		if (inTexCoord1.GetPointer())
		{
			memcpy(dstData, inTexCoord1.GetPointer(), sizeof(Vector2f));
			dstData += sizeof(Vector2f);
			++inTexCoord1;
		}

		if (inTangent.GetPointer())
		{
			Vector4f* outTangent = reinterpret_cast<Vector4f*> (dstData);
			Vector3f* outTangentXYZ = reinterpret_cast<Vector3f*> (outTangent);
			nm.MultiplyVector3(reinterpret_cast<const Vector3f&>(*inTangent), *outTangentXYZ);
			outTangent->w = inTangent->w;
			dstData += sizeof(Vector4f);
			++inTangent;
		}
	}
}



#if (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING) || UNITY_SUPPORTS_VFP

typedef void (*TransformFunc)( const void*, const void*, const void*, const float*, void*, int );
typedef void (*TransformFuncWithTangents)( const void*, const void*, const void*, const float*, void*, int, const void* );


#if UNITY_SUPPORTS_NEON
namespace TransformNEON
{
	#define TRANSFORM_FUNC(prefix, addData)		s_TransformVertices_Strided_##prefix##_##addData##_NEON

	TransformFunc TransformXYZ[] =
	{
		TRANSFORM_FUNC(XYZ,0), TRANSFORM_FUNC(XYZ,1), TRANSFORM_FUNC(XYZ,2), TRANSFORM_FUNC(XYZ,3), TRANSFORM_FUNC(XYZ,4), TRANSFORM_FUNC(XYZ,5)
	};

	TransformFunc TransformXYZN[] =
	{
		TRANSFORM_FUNC(XYZN,0), TRANSFORM_FUNC(XYZN,1), TRANSFORM_FUNC(XYZN,2), TRANSFORM_FUNC(XYZN,3), TRANSFORM_FUNC(XYZN,4), TRANSFORM_FUNC(XYZN,5)
	};

	TransformFuncWithTangents TransformXYZNT[] =
	{
		TRANSFORM_FUNC(XYZNT,0), TRANSFORM_FUNC(XYZNT,1), TRANSFORM_FUNC(XYZNT,2), TRANSFORM_FUNC(XYZNT,3), TRANSFORM_FUNC(XYZNT,4), TRANSFORM_FUNC(XYZNT,5)
	};

	#undef TRANSFORM_FUNC
}
#endif // UNITY_SUPPORTS_NEON


#if UNITY_SUPPORTS_VFP
namespace TransformVFP
{
	#define TRANSFORM_FUNC(prefix, addData)		s_TransformVertices_Strided_##prefix##_##addData##_VFP

	TransformFunc TransformXYZ[] =
	{
		TRANSFORM_FUNC(XYZ,0), TRANSFORM_FUNC(XYZ,1), TRANSFORM_FUNC(XYZ,2), TRANSFORM_FUNC(XYZ,3), TRANSFORM_FUNC(XYZ,4), TRANSFORM_FUNC(XYZ,5)
	};

	TransformFunc TransformXYZN[] =
	{
		TRANSFORM_FUNC(XYZN,0), TRANSFORM_FUNC(XYZN,1), TRANSFORM_FUNC(XYZN,2), TRANSFORM_FUNC(XYZN,3), TRANSFORM_FUNC(XYZN,4), TRANSFORM_FUNC(XYZN,5)
	};

	TransformFuncWithTangents TransformXYZNT[] =
	{
		TRANSFORM_FUNC(XYZNT,0), TRANSFORM_FUNC(XYZNT,1), TRANSFORM_FUNC(XYZNT,2), TRANSFORM_FUNC(XYZNT,3), TRANSFORM_FUNC(XYZNT,4), TRANSFORM_FUNC(XYZNT,5)
	};

	#undef TRANSFORM_FUNC
}
#endif // UNITY_SUPPORTS_VFP

void
TransformVerticesStridedARM( StrideIterator<Vector3f> inPos, StrideIterator<Vector3f> inNormal,
							 StrideIterator<ColorRGBA32> inColor, StrideIterator<Vector2f> inTexCoord0, StrideIterator<Vector2f> inTexCoord1,
							 StrideIterator<Vector4f> inTangent,
							 UInt8* dstData, const Matrix4x4f& m, unsigned vertexCount, bool multiStream )
{
	int addDataSize = 0;
	if( inColor.GetPointer() )		addDataSize += 1;
	if( inTexCoord0.GetPointer() )	addDataSize += 2;
	if( inTexCoord1.GetPointer() )	addDataSize += 2;

	const void* addDataSrc  = 0;
	if( inColor.GetPointer() )			addDataSrc = inColor.GetPointer();
	else if( inTexCoord0.GetPointer() )	addDataSrc = inTexCoord0.GetPointer();
	else if( inTexCoord1.GetPointer() )	addDataSrc = inTexCoord1.GetPointer();

	// slow path determination
	if(   (inColor.GetPointer() && inTexCoord1.GetPointer() && !inTexCoord0.GetPointer())
	   || (inTangent.GetPointer() && !inNormal.GetPointer()) || multiStream )
	{
		TransformVerticesStridedREF(inPos, inNormal, inColor, inTexCoord0, inTexCoord1, inTangent, dstData, m, vertexCount, multiStream);
		return;
	}

	int stride = inPos.GetStride();
	const UInt8* inDataBegin = static_cast<const UInt8*>(inPos.GetPointer());
	const UInt8* inDataEnd = inDataBegin + vertexCount * stride;

#if UNITY_SUPPORTS_NEON
	if (CPUInfo::HasNEONSupport())
	{
		using namespace TransformNEON;
		if( inNormal.GetPointer() && inTangent.GetPointer() )
			TransformXYZNT[addDataSize]( inDataBegin, inDataEnd, addDataSrc, m.m_Data, dstData, stride, inTangent.GetPointer() );
		else if( inNormal.GetPointer() )
			TransformXYZN[addDataSize]( inDataBegin, inDataEnd, addDataSrc, m.m_Data, dstData, stride );
		else
			TransformXYZ[addDataSize]( inDataBegin, inDataEnd, addDataSrc, m.m_Data, dstData, stride );
	}
	else
#endif
#if UNITY_SUPPORTS_VFP
	{
		using namespace TransformVFP;
		if( inNormal.GetPointer() && inTangent.GetPointer() )
			TransformXYZNT[addDataSize]( inDataBegin, inDataEnd, addDataSrc, m.m_Data, dstData, stride, inTangent.GetPointer() );
		else if( inNormal.GetPointer() )
			TransformXYZN[addDataSize]( inDataBegin, inDataEnd, addDataSrc, m.m_Data, dstData, stride );
		else
			TransformXYZ[addDataSize]( inDataBegin, inDataEnd, addDataSrc, m.m_Data, dstData, stride );
	}
#else
	{
		ErrorString("non-NEON path not enabled!");
	}
#endif
}
#endif

