#ifndef TRANSFORM_VERTEX_H_
#define TRANSFORM_VERTEX_H_

#include "Configuration/PrefixConfigure.h"
#include "Runtime/Utilities/StrideIterator.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Color.h"

class Matrix4x4f;


//==============================================================================

#define DECL_TRANSFORM_VERTICES_STRIDED(code, num, postfix)																				\
	void s_TransformVertices_Strided_##code##_##num##_##postfix( const void* srcData, const void* srcDataEnd, const void* addData,		\
																 const float* xform, void* outData, int stride							\
															   );

#define DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(num, postfix)																			\
	void s_TransformVertices_Strided_XYZNT_##num##_##postfix(	const void* srcData, const void* srcDataEnd, const void* addData,		\
																const float* xform, void* outData, int stride, const void* srcTangent	\
															   );


#if UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING

extern "C"
{
#if UNITY_ANDROID || UNITY_WINRT || UNITY_BB10 || UNITY_TIZEN
	#define s_TransformVertices_Strided_XYZ_0_NEON		_s_TransformVertices_Strided_XYZ_0_NEON
	#define s_TransformVertices_Strided_XYZ_1_NEON		_s_TransformVertices_Strided_XYZ_1_NEON
	#define s_TransformVertices_Strided_XYZ_2_NEON		_s_TransformVertices_Strided_XYZ_2_NEON
	#define s_TransformVertices_Strided_XYZ_3_NEON		_s_TransformVertices_Strided_XYZ_3_NEON
	#define s_TransformVertices_Strided_XYZ_4_NEON		_s_TransformVertices_Strided_XYZ_4_NEON
	#define s_TransformVertices_Strided_XYZ_5_NEON		_s_TransformVertices_Strided_XYZ_5_NEON
	
	#define s_TransformVertices_Strided_XYZN_0_NEON		_s_TransformVertices_Strided_XYZN_0_NEON
	#define s_TransformVertices_Strided_XYZN_1_NEON		_s_TransformVertices_Strided_XYZN_1_NEON
	#define s_TransformVertices_Strided_XYZN_2_NEON		_s_TransformVertices_Strided_XYZN_2_NEON
	#define s_TransformVertices_Strided_XYZN_3_NEON		_s_TransformVertices_Strided_XYZN_3_NEON
	#define s_TransformVertices_Strided_XYZN_4_NEON		_s_TransformVertices_Strided_XYZN_4_NEON
	#define s_TransformVertices_Strided_XYZN_5_NEON		_s_TransformVertices_Strided_XYZN_5_NEON
	
	#define s_TransformVertices_Strided_XYZNT_0_NEON	_s_TransformVertices_Strided_XYZNT_0_NEON
	#define s_TransformVertices_Strided_XYZNT_1_NEON	_s_TransformVertices_Strided_XYZNT_1_NEON
	#define s_TransformVertices_Strided_XYZNT_2_NEON	_s_TransformVertices_Strided_XYZNT_2_NEON
	#define s_TransformVertices_Strided_XYZNT_3_NEON	_s_TransformVertices_Strided_XYZNT_3_NEON
	#define s_TransformVertices_Strided_XYZNT_4_NEON	_s_TransformVertices_Strided_XYZNT_4_NEON
	#define s_TransformVertices_Strided_XYZNT_5_NEON	_s_TransformVertices_Strided_XYZNT_5_NEON
#if ENABLE_SPRITES
#define		s_TransformVertices_Sprite_NEON             _s_TransformVertices_Sprite_NEON
#endif
	
#endif // UNITY_ANDROID || UNITY_WINRT || UNITY_BB10 || UNITY_TIZEN

	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,0,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,1,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,2,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,3,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,4,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,5,NEON);

	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,0,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,1,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,2,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,3,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,4,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,5,NEON);
	
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(0,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(1,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(2,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(3,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(4,NEON);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(5,NEON);
#if ENABLE_SPRITES
	void s_TransformVertices_Sprite_NEON(const void* srcData, const void* srcDataEnd, const void* addData, const float* xform, void* outData, int stride, unsigned int color);
#endif
}

#endif


#if UNITY_SUPPORTS_VFP

extern "C"
{
#if UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
	#define s_TransformVertices_Strided_XYZ_0_VFP		_s_TransformVertices_Strided_XYZ_0_VFP
	#define s_TransformVertices_Strided_XYZ_1_VFP		_s_TransformVertices_Strided_XYZ_1_VFP
	#define s_TransformVertices_Strided_XYZ_2_VFP		_s_TransformVertices_Strided_XYZ_2_VFP
	#define s_TransformVertices_Strided_XYZ_3_VFP		_s_TransformVertices_Strided_XYZ_3_VFP
	#define s_TransformVertices_Strided_XYZ_4_VFP		_s_TransformVertices_Strided_XYZ_4_VFP
	#define s_TransformVertices_Strided_XYZ_5_VFP		_s_TransformVertices_Strided_XYZ_5_VFP
	
	#define s_TransformVertices_Strided_XYZN_0_VFP		_s_TransformVertices_Strided_XYZN_0_VFP
	#define s_TransformVertices_Strided_XYZN_1_VFP		_s_TransformVertices_Strided_XYZN_1_VFP
	#define s_TransformVertices_Strided_XYZN_2_VFP		_s_TransformVertices_Strided_XYZN_2_VFP
	#define s_TransformVertices_Strided_XYZN_3_VFP		_s_TransformVertices_Strided_XYZN_3_VFP
	#define s_TransformVertices_Strided_XYZN_4_VFP		_s_TransformVertices_Strided_XYZN_4_VFP
	#define s_TransformVertices_Strided_XYZN_5_VFP		_s_TransformVertices_Strided_XYZN_5_VFP
	
	#define s_TransformVertices_Strided_XYZNT_0_VFP		_s_TransformVertices_Strided_XYZNT_0_VFP
	#define s_TransformVertices_Strided_XYZNT_1_VFP		_s_TransformVertices_Strided_XYZNT_1_VFP
	#define s_TransformVertices_Strided_XYZNT_2_VFP		_s_TransformVertices_Strided_XYZNT_2_VFP
	#define s_TransformVertices_Strided_XYZNT_3_VFP		_s_TransformVertices_Strided_XYZNT_3_VFP
	#define s_TransformVertices_Strided_XYZNT_4_VFP		_s_TransformVertices_Strided_XYZNT_4_VFP
	#define s_TransformVertices_Strided_XYZNT_5_VFP		_s_TransformVertices_Strided_XYZNT_5_VFP
#if ENABLE_SPRITES
	#define s_TransformVertices_Sprite_VFP				_s_TransformVertices_Sprite_VFP
#endif
#endif // UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN


	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,0,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,1,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,2,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,3,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,4,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZ,5,VFP);
	
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,0,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,1,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,2,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,3,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,4,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED(XYZN,5,VFP);
	
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(0,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(1,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(2,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(3,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(4,VFP);
	DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS(5,VFP);
#if ENABLE_SPRITES
	void s_TransformVertices_Sprite_VFP (const void* srcData, const void* srcDataEnd, const void* addData, const float* xform, void* outData, int stride, unsigned int color);
#endif
}

#endif 


#undef DECL_TRANSFORM_VERTICES_STRIDED_TANGENTS
#undef DECL_TRANSFORM_VERTICES_STRIDED


//==============================================================================

void
TransformVerticesStridedREF( StrideIterator<Vector3f> inPos, StrideIterator<Vector3f> inNormal,
							 StrideIterator<ColorRGBA32> inColor, StrideIterator<Vector2f> inTexCoord0, StrideIterator<Vector2f> inTexCoord1,
							 StrideIterator<Vector4f> inTangent,
							 UInt8* dstData, const Matrix4x4f& m, unsigned vertexCount, bool multiStream );

#if (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING) || UNITY_SUPPORTS_VFP
void
TransformVerticesStridedARM( StrideIterator<Vector3f> inPos, StrideIterator<Vector3f> inNormal,
							 StrideIterator<ColorRGBA32> inColor, StrideIterator<Vector2f> inTexCoord0, StrideIterator<Vector2f> inTexCoord1,
							 StrideIterator<Vector4f> inTangent,
							 UInt8* dstData, const Matrix4x4f& m, unsigned vertexCount, bool multiStream );
#endif


#if (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING) || UNITY_SUPPORTS_VFP
	#define TransformVerticesStrided TransformVerticesStridedARM
#else
	#define TransformVerticesStrided TransformVerticesStridedREF
#endif


//==============================================================================

#endif // TRANSFORM_VERTEX_H_
