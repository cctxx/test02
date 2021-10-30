#pragma once

#include "GfxDeviceTypes.h"
#include "Runtime/Math/Matrix4x4.h"
#include "BuiltinShaderParams.h"

struct TransformState
{
	enum {
		kWorldDirty			= (1<<0),
		kViewDirty			= (1<<1),
		kProjDirty			= (1<<2),

		kWorldViewDirty		= (kWorldDirty | kViewDirty),
		kViewProjDirty	= (kViewDirty | kProjDirty),
		kWorldViewProjDirty	= (kWorldDirty | kViewDirty | kProjDirty),
	};

	Matrix4x4f		worldMatrix;
	Matrix4x4f		projectionMatrixOriginal;	// Originally set from Unity code

	Matrix4x4f		texMatrices[kMaxSupportedTextureCoords];

	// cache
	mutable Matrix4x4f		worldViewMatrix;

	mutable volatile UInt32	dirtyFlags;

public:
	void Invalidate(BuiltinShaderParamValues& builtins);
	void UpdateWorldViewMatrix (const BuiltinShaderParamValues& builtins) const;
	void SetViewMatrix (const float matrix[16], BuiltinShaderParamValues& builtins);
};

inline void TransformState::Invalidate(BuiltinShaderParamValues& builtins)
{
	worldViewMatrix.SetIdentity();
	worldMatrix.SetIdentity();
	builtins.GetWritableMatrixParam(kShaderMatView).SetIdentity();
	builtins.GetWritableMatrixParam(kShaderMatProj).SetIdentity();
	builtins.GetWritableMatrixParam(kShaderMatViewProj).SetIdentity();
	projectionMatrixOriginal.SetIdentity();
	dirtyFlags = kWorldViewProjDirty;
}

inline void TransformState::UpdateWorldViewMatrix (const BuiltinShaderParamValues& builtins) const
{
	if (dirtyFlags & kWorldViewDirty)
	{
		MultiplyMatrices4x4 (&builtins.GetMatrixParam(kShaderMatView), &worldMatrix, &worldViewMatrix);
		dirtyFlags &= ~kWorldViewDirty;
	}
}

inline void TransformState::SetViewMatrix (const float matrix[16], BuiltinShaderParamValues& builtins)
{
	dirtyFlags |= TransformState::kWorldViewDirty;
	Matrix4x4f& viewMat = builtins.GetWritableMatrixParam(kShaderMatView);
	const Matrix4x4f& projMat = builtins.GetMatrixParam(kShaderMatProj);
	Matrix4x4f& viewProjMat = builtins.GetWritableMatrixParam(kShaderMatViewProj);
	CopyMatrix (matrix, viewMat.GetPtr());
	MultiplyMatrices4x4 (&projMat, &viewMat, &viewProjMat);
	worldMatrix = Matrix4x4f::identity;
}
