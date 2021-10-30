#pragma once 

#include "Runtime/Animation/Animator.h"
#include "Runtime/Graphics/Texture2D.h"

///@TODO: This file should be renamed to 2DBlendTreePreviewUtility
class AnimatorUtility
{
public:	
	static void		GetRootBlendTreeChildWeights (Animator& animator, int layerIndex, int stateHash, float* weightArray);
	static void		CalculateRootBlendTreeChildWeights (Animator& animator, int layerIndex, int stateHash, float* weightArray, float blendX, float blendY);
	static void		CalculateBlendTexture (Animator& animator, int layerIndex, int stateHash, Texture2D* blendTexture, vector<Texture2D*> weightTextures, float minX, float minY, float maxX, float maxY);
};
