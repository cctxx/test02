#pragma once

#include "GfxDeviceObjects.h"
#include "BuiltinShaderParamsNames.h"
#include "Runtime/Utilities/fixed_bitset.h"
#include "Runtime/Utilities/NonCopyable.h"

#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Matrix4x4.h"
#include "External/shaderlab/Library/texenv.h"


struct BuiltinShaderParamIndices
{
	struct MatrixParamData {
		MatrixParamData() : gpuIndex(-1), rows(0), cols(0)
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
			, cbID(-1)
		#endif
		{ }
		int     gpuIndex;
		UInt16  rows;
		UInt16  cols;
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		int		cbID;
		#endif
	};
	struct VectorParamData {
		VectorParamData() : gpuIndex(-1), dim(0)
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
			, cbID(-1)
		#endif
		{ }
		int     gpuIndex;
		SInt16	dim;
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		int		cbID;
		#endif
	};

	MatrixParamData	mat[kShaderInstanceMatCount];
	VectorParamData vec[kShaderInstanceVecCount];

	bool CheckMatrixParam (const char* name, int index, int rowCount, int colCount, int cbID);
	bool CheckVectorParam (const char* name, int index, int dim, int cbID);
};


class BuiltinShaderParamValues : public NonCopyable
{
public:
	BuiltinShaderParamValues ();

	FORCE_INLINE const Vector4f& 			GetVectorParam(BuiltinShaderVectorParam param) const	 { DebugAssert(param >= 0 && param < kShaderVecCount); return vectorParamValues[param]; }
	FORCE_INLINE const Matrix4x4f&			GetMatrixParam(BuiltinShaderMatrixParam param) const	 { DebugAssert(param >= 0 && param < kShaderMatCount); return matrixParamValues[param]; }
	FORCE_INLINE const ShaderLab::TexEnv&	GetTexEnvParam(BuiltinShaderTexEnvParam param) const	 { DebugAssert(param >= 0 && param < kShaderTexEnvCount); return texEnvParamValues[param]; }
	FORCE_INLINE const Vector4f& 			GetInstanceVectorParam(ShaderBuiltinInstanceVectorParam param) const { DebugAssert(param >= 0 && param < kShaderInstanceVecCount); return instanceVectorValues[param]; }

	FORCE_INLINE Vector4f& 			GetWritableVectorParam(BuiltinShaderVectorParam param)			 { DebugAssert(param >= 0 && param < kShaderVecCount); return vectorParamValues[param]; }
	FORCE_INLINE Matrix4x4f&		GetWritableMatrixParam(BuiltinShaderMatrixParam param)			 { DebugAssert(param >= 0 && param < kShaderMatCount); return matrixParamValues[param]; }
	FORCE_INLINE ShaderLab::TexEnv&	GetWritableTexEnvParam(BuiltinShaderTexEnvParam param)			 { DebugAssert(param >= 0 && param < kShaderTexEnvCount); return texEnvParamValues[param]; }

	FORCE_INLINE void	SetVectorParam(BuiltinShaderVectorParam param, const Vector4f& val)          { GetWritableVectorParam(param) = val; }
	FORCE_INLINE void	SetMatrixParam(BuiltinShaderMatrixParam param, const Matrix4x4f& val)        { GetWritableMatrixParam(param) = val; }
	FORCE_INLINE void	SetTexEnvParam(BuiltinShaderTexEnvParam param, const ShaderLab::TexEnv& val) { GetWritableTexEnvParam(param) = val; }
	FORCE_INLINE void	SetInstanceVectorParam(ShaderBuiltinInstanceVectorParam param, const Vector4f& val) { DebugAssert(param >= 0 && param < kShaderInstanceVecCount); instanceVectorValues[param] = val; }

private:
	Vector4f			vectorParamValues[kShaderVecCount];
	Matrix4x4f			matrixParamValues[kShaderMatCount];
	ShaderLab::TexEnv	texEnvParamValues[kShaderTexEnvCount];
	Vector4f			instanceVectorValues[kShaderInstanceVecCount];
};
