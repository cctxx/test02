#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Math/SphericalHarmonics.h"
#if UNITY_WIN && !UNITY_WINRT
#include "External/DirectX/builds/dx9include/d3dx9.h"
#endif
#include <vector>


SUITE (MathTests)
{

#if UNITY_WIN
#pragma warning( disable : 4723 ) //required for the divide by 0 that's happening in this test.
#endif

TEST (Math_Nan)
{
	struct
	{
		inline bool IsNANNew (float A)
		{
			// A NAN has an exponent of 255 (shifted left 23 positions) and a non-zero mantissa.
			int exp = *(int*)&A & 0x7F800000;
			int mantissa = *(int*)&A & 0x007FFFFF;
			return exp == 0x7F800000 && mantissa != 0;
		}

		inline void operator () (bool expect, float A)
		{
			CHECK_EQUAL (IsNANNew (A), IsNAN (A));
			CHECK_EQUAL (expect, IsNAN (A));
		}
	} CheckNAN;

	float f = 0.0F;
	float f0 = 0.0F;

	f = f / f0;

	CheckNAN (true, f);

	CheckNAN (true, std::numeric_limits<float>::quiet_NaN ());
	CheckNAN (true, std::numeric_limits<float>::signaling_NaN ());
	CheckNAN (false, 1.0F);
	CheckNAN (false, 0.0F);
}

#if UNITY_WIN
#pragma warning( default : 4723 )
#endif

TEST (Math_Matrix)
{
	Matrix4x4f m0, m1, m2, m6;

	for (int i = 0; i < 16; ++i)
	{
		m0.m_Data[i] = (float)i;
		m1.m_Data[15 - i] = (float)i;
	}

	MultiplyMatrices4x4REF (&m0, &m1, &m2);
	MultiplyMatrices4x4 (&m0, &m1, &m6);
	CHECK_EQUAL (0, memcmp (m2.m_Data, m6.m_Data, sizeof(Matrix4x4f)));

	TransposeMatrix4x4REF(&m0, &m2);
	TransposeMatrix4x4(&m0, &m6);
	CHECK_EQUAL (0, memcmp (m2.m_Data, m6.m_Data, sizeof(Matrix4x4f)));

	Vector3f v (2.0F, 5.0F, 2.0F);
	Vector3f res (2.0F, 5.0F, -2.0F);

	Quaternionf q;
	Quaternionf backConvertedQ;
	Matrix3x3f m3;
	Vector3f tempResult;

	q = AxisAngleToQuaternion (Vector3f::yAxis, kPI / 2.0F);
	QuaternionToMatrix (q, m3);

	CHECK_EQUAL (true, CompareApproximately (RotateVectorByQuat(q, v), res));
	CHECK_EQUAL (true, CompareApproximately (m3.MultiplyPoint3 (v), res));

	MatrixToQuaternion (m3, backConvertedQ);
	CHECK_EQUAL (true, CompareApproximately (backConvertedQ, q));

	Vector3f axis;
	float roll;

	QuaternionToAxisAngle (backConvertedQ, &axis, &roll);
	CHECK_EQUAL (true, CompareApproximately (axis, Vector3f::yAxis));
	CHECK_CLOSE (kPI / 2.0F, roll, 0.000001F);

	q = Inverse (q);
	m3.Invert ();
	MatrixToQuaternion (m3, backConvertedQ);
	CHECK_EQUAL (true, CompareApproximately (backConvertedQ, q));

	tempResult = RotateVectorByQuat (q, res);
	CHECK_EQUAL (true, CompareApproximately (tempResult, v));
	tempResult = RotateVectorByQuat (backConvertedQ, res);
	CHECK_EQUAL (true, CompareApproximately (tempResult, v));
	tempResult = m3.MultiplyPoint3 (res);
	CHECK_EQUAL (true, CompareApproximately (tempResult, v));
}

	

TEST (Math_NormalizeFastTest)
{
	Vector3f input[] = { Vector3f (0.0f, 0.1f, 0.0f), Vector3f (0.0f, 0.0f, 0.0f), Vector3f (-0.0f, -0.0f, -0.0f) };
	Vector3f output[] = { Vector3f (0.0f, 1.f, 0.0f), Vector3f (0.0f, 0.0f, 0.0f), Vector3f (-0.0f, -0.0f, -0.0f) };
	
	for (int i=0;i<3;i++)
	{
		Vector3f normalized = NormalizeFast(input[i]);
		CHECK (CompareApproximately (output[i] , normalized, 0.0001f));
	}
}
	
	
	
TEST (Math_MatrixQuaternionConversion)
{
	Rand rand (GetTimeSinceStartup ());
	for (int i = 0; i < 500; ++i)
	{
		Quaternionf rot = RandomQuaternion (rand);
		Quaternionf outq, outq2;
		Matrix3x3f m, outm;
		QuaternionToMatrix (rot, m);
		Vector3f angle;

		MatrixToEuler (m, angle);
		EulerToMatrix (angle, outm);
		outq2 = EulerToQuaternion (angle);

		MatrixToQuaternion (outm, outq);
		CHECK (CompareApproximately (m , outm, 0.1f));
		CHECK_CLOSE (1, Abs (Dot (outq, rot)), 0.01f);
		CHECK_CLOSE (1, Abs (Dot (outq2, rot)), 0.01f);
	}
}


TEST (Math_EulerAngles)
{
	struct
	{
		void operator() (float x, float y, float z)
		{
			Quaternionf quat = EulerToQuaternion (Vector3f (Deg2Rad (x), Deg2Rad (y), Deg2Rad (z)));
			Matrix3x3f quatM;
			QuaternionToMatrix (quat, quatM);

			Vector3f euler = QuaternionToEuler (quat);
			Vector3f eulerDeg (Rad2Deg (euler.x), Rad2Deg (euler.y), Rad2Deg (euler.z));
			Quaternionf newquat = EulerToQuaternion (euler);

			CHECK_CLOSE (Abs (Dot (newquat, quat)), 1, 0.01f);
		}
	} TestEulerAngles;

	TestEulerAngles ( 90.0f, 45.0f, 0.0f);
	TestEulerAngles ( 90.0f, 90.0f, 0.0f);
	TestEulerAngles (270.0f,  0.0f, 0.0f);
	TestEulerAngles (270.0f, 40.0f, 0.0f);
}

TEST (Math_EulerAnglesMatchAxisAngle)
{
	Quaternionf quat = AxisAngleToQuaternion(Vector3f::yAxis, Deg2Rad(20.0F));
	Vector3f euler = QuaternionToEuler(quat);
	CHECK_EQUAL (true, CompareApproximately (0, euler.x));
	CHECK_EQUAL (true, CompareApproximately (Deg2Rad(20.0F), euler.y));
	CHECK_EQUAL (true, CompareApproximately (0, euler.z));
}

// This test fails with the current version of QuaternionToEuler.  The angles
// being close to gimbal lock will snap to 90 degree increments.
#if 0
    
TEST (Math_QuaternionToEulerHandlesGimbalLock)
{
    Quaternionf quat = EulerToQuaternion (Vector3f (Deg2Rad (269.5f), 0.f, 0.f));
//    printf( "%f, %f, %f, %f\n", quat.x, quat.y, quat.z, quat.w);
    Vector3f euler = QuaternionToEuler (quat);
//    printf( "%f, %f, %f\n", Rad2Deg(euler.x), Rad2Deg(euler.y), Rad2Deg(euler.z));
    Quaternionf quat1 = EulerToQuaternion (euler);
//    printf( "%f, %f, %f, %f\n", quat1.x, quat1.y, quat1.z, quat1.w);
    
    CHECK_CLOSE (269.5f, Rad2Deg (euler.x), 0.01f);
    CHECK_CLOSE (0.f, euler.y, 0.01f);
    CHECK_CLOSE (0.f, euler.z, 0.01f);
    
    quat = EulerToQuaternion (Vector3f (Deg2Rad (89.5f), 0.f, 0.f));
    euler = QuaternionToEuler (quat);
    
    CHECK_CLOSE (89.5f, Rad2Deg (euler.x), 0.01f);
    CHECK_CLOSE (0.f, euler.y, 0.01f);
    CHECK_CLOSE (0.f, euler.z, 0.01f);
    
    quat = EulerToQuaternion (Vector3f (Deg2Rad (89.0f), 0.f, 0.f));
    euler = QuaternionToEuler (quat);
    
    CHECK_CLOSE (89.0f, Rad2Deg (euler.x), 0.01f);
    CHECK_CLOSE (0.f, euler.y, 0.01f);
    CHECK_CLOSE (0.f, euler.z, 0.01f);
    
    quat = EulerToQuaternion (Vector3f (Deg2Rad (88.5f), 0.f, 0.f));
    euler = QuaternionToEuler (quat);
    
//    printf( "%f, %f, %f\n", Rad2Deg(euler.x), Rad2Deg(euler.y), Rad2Deg(euler.z));
    
    CHECK_CLOSE (88.5f, Rad2Deg (euler.x), 0.01f);
    CHECK_CLOSE (0.f, euler.y, 0.01f);
    CHECK_CLOSE (0.f, euler.z, 0.01f);
}
    
#endif
    
TEST (Math_ColorRGBA32Lerp)
{
#if UNITY_LINUX
#warning Investigate/fix ColorRGBA32 Tests!
#else
	ColorRGBA32 c0, c1, res;

	c0 = ColorRGBA32 (100, 150, 255, 0);
	c1 = ColorRGBA32 (200, 100, 0, 255);

	res = Lerp (c0, c1, 0);
	CHECK (ColorRGBA32 (100, 150, 255, 0) == res);

	res = Lerp (c0, c1, 90);
	CHECK (ColorRGBA32 (135, 132,165,89) == res);

	res = Lerp (c0, c1, 200);
	CHECK (ColorRGBA32 (178, 110,55,199) == res);

	res = Lerp (c0, c1, 255);
	CHECK (ColorRGBA32 (199, 100, 0, 254) == res);
#endif
}


TEST (Math_ColorRGBA32Scale)
{
#if UNITY_LINUX
#warning Investigate/fix ColorRGBA32 Tests!
#else
	ColorRGBA32 c0, res;

	c0 = ColorRGBA32 (100, 150, 255, 150);

	res = c0 * 0;
	CHECK (ColorRGBA32 (0, 0, 0, 0) == res);

	res = c0 * 20;
	CHECK (ColorRGBA32 (8, 12, 20, 12) == res);

	res = c0 * 150;
	CHECK (ColorRGBA32 (58, 88, 150, 88) == res);

	res = c0 * 255;
	CHECK (ColorRGBA32 (100, 150, 255, 150) == res);
#endif
}

void TestMultiplyColorRGBA32(const ColorRGBA32 input0, const ColorRGBA32 input1, int tolerance)
{
	ColorRGBA32 expected;
	ColorRGBA32 actual;
	expected.r = (input0.r * input1.r) / 255;
	expected.g = (input0.g * input1.g) / 255;
	expected.b = (input0.b * input1.b) / 255;
	expected.a = (input0.a * input1.a) / 255;
	actual = input0*input1;
	
	CHECK_CLOSE((int)expected.r, (int)actual.r, tolerance);
	CHECK_CLOSE((int)expected.g, (int)actual.g, tolerance);
	CHECK_CLOSE((int)expected.b, (int)actual.b, tolerance);
	CHECK_CLOSE((int)expected.a, (int)actual.a, tolerance);
}

TEST (Math_ColorRGBA32Muliply)
{
	for(int i = 0; i < 256; i+=4)
	{
		TestMultiplyColorRGBA32(ColorRGBA32(0,0,0,0), ColorRGBA32(i+0,i+1,i+2,i+3), 0);
		TestMultiplyColorRGBA32(ColorRGBA32(i+0,i+1,i+2,i+3), ColorRGBA32(0,0,0,0), 0);
		TestMultiplyColorRGBA32(ColorRGBA32(i+0,i+1,i+2,i+3), ColorRGBA32(0xff,0xff,0xff,0xff), 0);
		TestMultiplyColorRGBA32(ColorRGBA32(0xff,0xff,0xff,0xff), ColorRGBA32(i+0,i+1,i+2,i+3), 0);
	}

	for(int i = 0; i < 256; i+=4)
		for(int j = i; j < 256; j+=4)
			TestMultiplyColorRGBA32(ColorRGBA32(j+0,j+1,j+2,j+3), ColorRGBA32(i+0,i+1,i+2,i+3), 1);
}

// Reference Implementation: D3DX; thus only test on Windows
#if UNITY_WIN && !UNITY_WINRT
TEST (Math_SphericalHarmonics)
{
	Rand r;

	for (int i = 0; i < 10000; ++i)
	{
		float x = r.GetFloat () * 2.0f - 1.0f;
		float y = r.GetFloat () * 2.0f - 1.0f;
		float z = r.GetFloat () * 2.0f - 1.0f;
		float sh[9], d3dxsh[9];

		SHEvalDirection9 (x, y, z, sh);
		D3DXSHEvalDirection (d3dxsh, 3, &D3DXVECTOR3 (x, y, z));
		for (int j = 0; j < 9; ++j)
		{
			CHECK_CLOSE (sh[j], d3dxsh[j], 0.000001f);
		}

		float shR[9], shG[9], shB[9];
		float d3dxshR[9], d3dxshG[9], d3dxshB[9];
		SHEvalDirectionalLight9 (x, y, z, 0.1f, 0.2f, 0.3f, shR, shG, shB);
		D3DXSHEvalDirectionalLight (3, &D3DXVECTOR3(x,y,z), 0.1f, 0.2f, 0.3f, d3dxshR, d3dxshG, d3dxshB);
		for (int j = 0; j < 9; ++j)
		{
			CHECK_CLOSE (shR[j], d3dxshR[j], 0.000001f);
			CHECK_CLOSE (shG[j], d3dxshG[j], 0.000001f);
			CHECK_CLOSE (shB[j], d3dxshB[j], 0.000001f);
		}
	}
}
#endif


void FabsPerformance ();

TEST (Math_Repeat)
{
	CHECK_EQUAL (15.0F, Repeat (-5.0F, 20.0F));
	CHECK_EQUAL ( 5.0F, Repeat (5.0F, 20.0F));
	CHECK_EQUAL ( 5.0F, Repeat (25.0F, 20.0F));
	CHECK_EQUAL ( 0.0F, Repeat (20.0F, 20.0F));
	CHECK_EQUAL ( 0.0F, Repeat (0.0F, 20.0F));
	CHECK_EQUAL (19.9F, Repeat (-0.1F, 20.0F));
	CHECK_EQUAL (10.0F, Repeat (-10.0F, 20.0F));
	CHECK_CLOSE (0.139999F, Repeat (0.699999F, 0.14F), 1e-5f);
	//CHECK (Repeat (0.69999999F, 0.14F) >= 0.0f) // This fails! Revisit for the next breaking version.

	// Our Repeat inverts when in negative space
	CHECK_CLOSE ( 3.0F, Repeat ( 3.0F,  5.0F), 1e-5f);
	CHECK_CLOSE (-2.0F, Repeat ( 3.0F, -5.0F), 1e-5f);
	CHECK_CLOSE (-3.0F, Repeat (-3.0F, -5.0F), 1e-5f);
	CHECK_CLOSE ( 2.0F, Repeat (-3.0F,  5.0F), 1e-5f);
	CHECK_CLOSE ( 0.0F, Repeat ( 0.0F, -1.0F), 1e-5f);
	CHECK_CLOSE ( 0.0F, Repeat ( 0.0F,  1.0F), 1e-5f);
	
	CHECK_CLOSE ( 1.0F, Repeat (-59.0F, 30.0F), 1e-5f);
	CHECK_CLOSE ( 0.0F, Repeat (-60.0F, 30.0F), 1e-5f);
	CHECK_CLOSE (29.0F, Repeat (-61.0F, 30.0F), 1e-5f);
}

TEST (Math_DeltaAngleRad)
{
	CHECK_EQUAL (0, DeltaAngleRad (12345.67890F, 12345.67890F));

	CHECK_EQUAL (kPI, DeltaAngleRad (0, -kPI));
	CHECK_EQUAL (kPI, DeltaAngleRad (0, kPI));
	CHECK_EQUAL (kPI, DeltaAngleRad (kPI, 0));

	CHECK_EQUAL (0, DeltaAngleRad (1.0F, 1.0F+2*kPI));
	CHECK_EQUAL (0, DeltaAngleRad (1.0F+2*kPI, 1.0F));

	CHECK_CLOSE ( kPI/2, DeltaAngleRad (0, 5*kPI/2), 1e-5f);
	CHECK_CLOSE (-kPI/2, DeltaAngleRad (0, 7*kPI/2), 1e-5f);
}

/*
TEST (Math_RoundFunctions)
{
	struct
	{
		void operator() (float t, int floor, int ceil, int round)
		{
			CHECK_EQUAL (floor, std::floor(t));
			CHECK_EQUAL (ceil, std::ceil(t));

			CHECK_EQUAL (round, RoundfToInt(t));
			CHECK_EQUAL (round, Roundf(t));

			CHECK_EQUAL (floor, Floorf(t));
			CHECK_EQUAL (floor, FloorfToInt(t));

			CHECK_EQUAL (ceil, Ceilf(t));
			CHECK_EQUAL (ceil, CeilfToInt(t));

			if (t >= 0.0F)
			{
				CHECK_EQUAL (RoundfToIntPos(t), round);
				CHECK_EQUAL (FloorfToIntPos(t), floor);
				CHECK_EQUAL (CeilfToIntPos(t), ceil);
			}
		}
	} TestRoundFunctions;

	CHECK_EQUAL (64, NextPowerOfTwo (33));
	CHECK_EQUAL (32, NextPowerOfTwo (32));
	CHECK_EQUAL (32, NextPowerOfTwo (31));

	TestRoundFunctions (15.1F, 15, 16, 15);
	TestRoundFunctions (0.9F, 0, 1, 1);

	TestRoundFunctions (1.0F, 1, 1, 1);
	TestRoundFunctions (2.0F, 2, 2, 2);

	TestRoundFunctions (5.9F, 5, 6, 6);
	TestRoundFunctions (7.1F, 7, 8, 7);
	TestRoundFunctions (7.6F, 7, 8, 8);
	TestRoundFunctions (0.49F, 0, 1, 0);
	TestRoundFunctions (120000.51F, 120000, 120001, 120001);

	TestRoundFunctions (-19.7F, -20, -19, -20);
	TestRoundFunctions (-16.01F, -17, -16, -16);
	TestRoundFunctions (-25.0F, -25, -25, -25);
	TestRoundFunctions (-25.501F, -26, -25, -26);
	TestRoundFunctions (-5.9F, -6, -5, -6);
	TestRoundFunctions (-7.1F, -8, -7, -7);
	TestRoundFunctions (-7.6F, -8, -7, -8);
	TestRoundFunctions (-0.1F, -1, 0, 0);
	TestRoundFunctions (-0.0000011F, -1, 0, 0);
	TestRoundFunctions (-0.25F, -1, 0, 0);
	TestRoundFunctions (-0.49F, -1, 0, 0);
	TestRoundFunctions (-0.51F, -1, 0, -1);
	TestRoundFunctions (-0.6F, -1, 0, -1);
	TestRoundFunctions (-1.0F, -1, -1, -1);
	TestRoundFunctions (-2.0F, -2, -2, -2);
	TestRoundFunctions (-1.01F, -2, -1, -1);
	TestRoundFunctions (-100000.49F, -100001, -100000, -100000);

	CHECK_EQUAL (1, RoundfToInt(0.5F));
	CHECK_EQUAL (2, RoundfToInt(1.5F));
	CHECK_EQUAL (0, RoundfToInt(-0.5F));
	CHECK_EQUAL (-1, RoundfToInt(-1.5F));

	// Rounding up or down, doesn't have to match floor / ceil function. Pick fastest
	//ErrorIf (TestFloor (120000.51F, 120000, 120000, 120000));

	CHECK_EQUAL (15, FloorfToIntPos (15.1F));
	CHECK_EQUAL (0, FloorfToIntPos (0.9F));

	CHECK_EQUAL (16, CeilfToIntPos (15.1F));
	CHECK_EQUAL (1, CeilfToIntPos (0.9F));

	CHECK_EQUAL (15, RoundfToIntPos (15.1F));
	CHECK_EQUAL (1, RoundfToIntPos (0.9F));
}
*/

TEST (Math_TransformPoints)
{
	Vector3f v (1, 0, 0);
	Matrix4x4f tr;
	tr.SetTR (Vector3f(10,0,0), AxisAngleToQuaternion (Vector3f::zAxis, Deg2Rad (90)));

	//Must ignore the translation, and work when input and output are the same.
	TransformPoints3x3 (tr, &v, &v, 1);

	CHECK (CompareApproximately (v, Vector3f(0, 1, 0)));
}


TEST (TypeSizes)
{
	CHECK_EQUAL (4, sizeof(SInt32));
	CHECK_EQUAL (4, sizeof(UInt32));

	CHECK_EQUAL (2, sizeof(SInt16));
	CHECK_EQUAL (2, sizeof(UInt16));

	CHECK_EQUAL (1, sizeof(UInt8));
	CHECK_EQUAL (1, sizeof(SInt8));

	CHECK_EQUAL (8, sizeof(UInt64));
	CHECK_EQUAL (8, sizeof(SInt64));
}


TEST (Math_QuaternionMatrixEquivalence)
{
	Matrix3x3f m;
	EulerToMatrix (Vector3f (Deg2Rad (15.0F), Deg2Rad (20.0F), Deg2Rad (64.0F)), m);

	Quaternionf q;
	MatrixToQuaternion (m, q);

	Vector3f v (25.3F, 27.14F, 34.2F);

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));
		Vector3f quatRes  = RotateVectorByQuat (q, v);

		CHECK (CompareApproximately (matrixRes, quatRes));
	}

	{
		Matrix3x3f m2 (m);
		m2.Scale (Vector3f (1.0F, 1.0F, -1.0F));

		CHECK (CompareApproximately (m2.GetDeterminant (), -1.0F));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));
		Vector3f quatRes = RotateVectorByQuat (q, Vector3f (v.x, v.y, v.z));

		CHECK (CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= -1.0F;
		modQ.y *= -1.0F;
		modQ.z *= -1.0F;
		modQ.w *= -1.0F;
		Vector3f quatRes  = RotateVectorByQuat(modQ, v);

		CHECK (CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= 1.0F;
		modQ.y *= -1.0F;
		modQ.z *= -1.0F;
		modQ.w *= -1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= 1.0F;
		modQ.y *= 1.0F;
		modQ.z *= -1.0F;
		modQ.w *= -1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= 1.0F;
		modQ.y *= 1.0F;
		modQ.z *= 1.0F;
		modQ.w *= -1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= -1.0F;
		modQ.y *= -1.0F;
		modQ.z *= 1.0F;
		modQ.w *= -1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= -1.0F;
		modQ.y *= -1.0F;
		modQ.z *= 1.0F;
		modQ.w *= 1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= 1.0F;
		modQ.y *= 1.0F;
		modQ.z *= -1.0F;
		modQ.w *= 1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}

	{
		Vector3f matrixRes (m.MultiplyPoint3 (v));

		Quaternionf modQ = q;
		modQ.x *= -1.0F;
		modQ.y *= 1.0F;
		modQ.z *= -1.0F;
		modQ.w *= 1.0F;

		Vector3f quatRes = RotateVectorByQuat(modQ, v);

		CHECK (!CompareApproximately (matrixRes, quatRes));
	}
}


TEST (Math_ColorMisc)
{
	{
		ColorRGBA32 c0 (100, 150, 100, 0);
		ColorRGBA32 c1 (200, 100, 0, 200);
		ColorRGBA32 res = c0 + c1;
		CHECK (res == ColorRGBA32 (255, 250, 100, 200));
	}

	{
		ColorRGBA32 res = ColorRGBA32 (150, 150, 150, 150) + ColorRGBA32 (150, 150, 150, 150);
		CHECK (res == ColorRGBA32 (255, 255, 255, 255));
	}
}


TEST (Math_TransformAABB)
{
	Matrix4x4f m;

	for (int i = 0; i < 16; ++i)
		m.m_Data[i] = (float)(7-i);

	AABB aabb(Vector3f(1,2,3), Vector3f(4,5,6));

	AABB aabbSlow;
	TransformAABBSlow(aabb, m, aabbSlow);

	AABB aabbRef;
	TransformAABB(aabb, m, aabbRef);

	CHECK (CompareApproximately (aabbSlow.m_Center, aabbRef.m_Center));
	CHECK (CompareApproximately (aabbSlow.m_Extent, aabbRef.m_Extent));
}

TEST (Math_BitsInMask)
{
	CHECK_EQUAL (0, BitsInMask(0x0));
	CHECK_EQUAL (32, BitsInMask(0xFFFFFFFF));
	CHECK_EQUAL (1, BitsInMask(0x1));
	CHECK_EQUAL (1, BitsInMask(0x80000000));
	CHECK_EQUAL (2, BitsInMask(0x5));
	CHECK_EQUAL (3, BitsInMask(0x7));
	CHECK_EQUAL (24, BitsInMask(0xDEADBEEF));
	CHECK_EQUAL (19, BitsInMask(0xCAFE1337));
}

TEST (Math_BitsInMask64)
{
	CHECK_EQUAL (0,  BitsInMask64(0x0000000000000000ULL));
	CHECK_EQUAL (64, BitsInMask64(0xFFFFFFFFFFFFFFFFULL));
	CHECK_EQUAL (1,  BitsInMask64(0x0000000000000001ULL));
	CHECK_EQUAL (2,  BitsInMask64(0x8000000080000000ULL));
	CHECK_EQUAL (2,  BitsInMask64(0x0000000000000005ULL));
	CHECK_EQUAL (3,  BitsInMask64(0x0000000000000007ULL));
	CHECK_EQUAL (24, BitsInMask64(0x00000000DEADBEEFULL));
	CHECK_EQUAL (19, BitsInMask64(0x00000000CAFE1337ULL));
	CHECK_EQUAL (43, BitsInMask64(0xCAFE1337DEADBEEFULL));
}

TEST (Math_Normalize)
{
	Plane p;
	p.SetABCD(0,0,0,1);
	p.NormalizeRobust();
	Vector3f n = p.GetNormal();
	CHECK (IsNormalized(n));

	p.SetABCD(2.5e-5f, 3.1e-5f, 1.2e-5f, 1.f);
	p.NormalizeRobust();
	n = p.GetNormal();
	CHECK (IsNormalized(n));

	Vector3f normal(2.3e-5f, 2.1e-5f, 3.2e-5f);
	float invOriginalLength;
	normal = NormalizeRobust(normal, invOriginalLength);
	CHECK (CompareApproximately (22394.295f, invOriginalLength));
}

}


#endif
