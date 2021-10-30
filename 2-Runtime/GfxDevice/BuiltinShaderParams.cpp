#include "UnityPrefix.h"
#include "BuiltinShaderParams.h"
#include "BuiltinShaderParamsNames.h"

BuiltinShaderParamValues::BuiltinShaderParamValues ()
{
	memset (vectorParamValues, 0, sizeof(vectorParamValues));
	memset (matrixParamValues, 0, sizeof(matrixParamValues));
	memset (instanceVectorValues, 0, sizeof(instanceVectorValues));

	// Initialize default light directions to (1,0,0,0), to avoid the case
	// when a shader with uninitialized value gets "tolight" vector of zero,
	// which returns NaN when doing normalize() on it, on GeForce FX/6/7.
	for (int i = 0; i < kMaxSupportedVertexLights; ++i)
		vectorParamValues[kShaderVecLight0Position+i].x = 1.0f;
}



bool BuiltinShaderParamIndices::CheckMatrixParam(const char* name, int index, int rowCount, int colCount, int cbID)
{
	int paramIndex;
	if (IsShaderInstanceMatrixParam(name, &paramIndex))
	{
		mat[paramIndex].gpuIndex = index;
		mat[paramIndex].rows = rowCount;
		mat[paramIndex].cols = colCount;
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		mat[paramIndex].cbID = cbID;
		#endif
		return true;
	}
	return false;
}

bool BuiltinShaderParamIndices::CheckVectorParam(const char* name, int index, int dim, int cbID)
{
	int paramIndex;
	if (IsShaderInstanceVectorParam(name, &paramIndex))
	{
		vec[paramIndex].gpuIndex = index;
		vec[paramIndex].dim = dim;
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		vec[paramIndex].cbID = cbID;
		#endif
		return true;
	}
	return false;
}

// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (BuiltinShaderParamsTests)
{
	// Having anything named _Reflection as built-in property is dangerous.
	// We do have a built-in matrix like that, but there are often shaders using that name
	// as texture. Ensure we never have anything _Reflection in built-ins.
	TEST(MakeSureNoBuiltinNamedReflection)
	{
		int index;
		CHECK (!IsVectorBuiltinParam ("_Reflection", &index));
		CHECK (!IsMatrixBuiltinParam ("_Reflection", &index));
		CHECK (!IsTexEnvBuiltinParam ("_Reflection", &index));
	}

	// Ensure initial  vector & matrix values are zero (except light directions, which should default to 1,0,0,0).
	// For some built-ins, we only setup their values when needed (e.g. when actual light is set up),
	// but a shader might reference them and get NaNs or other totally invalid values.
	TEST (BuiltinParamValuesAreInitialized)
	{
		BuiltinShaderParamValues vals;
		for (int i = 0; i < kShaderVecCount; ++i)
		{
			const Vector4f& v = vals.GetVectorParam(BuiltinShaderVectorParam(i));
			float expected = (i>=kShaderVecLight0Position && i<=kShaderVecLight7Position) ? 1.0f : 0.0f;
			CHECK_EQUAL (expected,v.x);	CHECK_EQUAL (0.0f,v.y); CHECK_EQUAL (0.0f,v.z); CHECK_EQUAL (0.0f,v.w);
		}
		for (int i = 0; i < kShaderMatCount; ++i)
		{
			const Matrix4x4f& v = vals.GetMatrixParam(BuiltinShaderMatrixParam(i));
			for (int j = 0; j < 16; ++j)
			{
				CHECK_EQUAL (0.0f, v.GetPtr()[j]);
			}
		}
	}

	// basic checks for builtin arrays recognition
	TEST(BuiltinArrays)
	{
		CHECK_EQUAL(IsBuiltinArrayName("unity_LightPosition"), true);
		CHECK_EQUAL(IsBuiltinArrayName("unity_LightPosition0"), false);
	}
} // SUITE

#endif // ENABLE_UNIT_TESTS
