#pragma once

#include "D3D9Includes.h"
#include "External/shaderlab/Library/shadertypes.h"

namespace ShaderLab {
	struct TextureBinding;
	class TexEnv;
}


const int kMaxD3DTextureStages = 8;
const int kMaxD3DTextureStagesForPS = 4;

struct D3DTextureStage
{
	D3DTEXTUREOP	colorOp;
	int				colorArgs[3];
	D3DTEXTUREOP	alphaOp;
	int				alphaArgs[3];
};

struct TextureCombinersD3D
{
	static TextureCombinersD3D* Create( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular );
	static void CleanupCombinerCache();

	D3DTextureStage	stages[kMaxD3DTextureStages+1];
	int envCount, stageCount; // these might be different!
	IDirect3DPixelShader9*	pixelShader;
	const ShaderLab::TextureBinding* texEnvs;

	int	 textureFactorIndex;
	bool textureFactorUsed;

	int		uniqueID;
};
