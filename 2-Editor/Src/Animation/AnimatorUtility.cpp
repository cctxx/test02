#include "UnityPrefix.h"

#include "AnimatorUtility.h"

#include "Runtime/mecanim/animation/blendtree.h"


void AnimatorUtility::GetRootBlendTreeChildWeights (Animator& animator, int layerIndex, int stateHash, float* weightArray)
{
	mecanim::animation::BlendTreeNodeConstant const* nodeConstant = NULL;
	mecanim::animation::BlendTreeWorkspace* workspace = NULL;
	animator.GetRootBlendTreeConstantAndWorkspace (layerIndex, stateHash, nodeConstant, workspace);
	if (nodeConstant == NULL)
		return;
	int childCount = nodeConstant->m_ChildCount;
	for (int j=0; j<childCount; j++)
	{
		int childNodeIndex = nodeConstant->m_ChildIndices[j];
		weightArray[j] = workspace->m_BlendArray[childNodeIndex];
	}
}

void AnimatorUtility::CalculateRootBlendTreeChildWeights (Animator& animator, int layerIndex, int stateHash, float* weightArray, float blendX, float blendY)
{
	mecanim::animation::BlendTreeNodeConstant const* nodeConstant = NULL;
	mecanim::animation::BlendTreeWorkspace* workspace = NULL;
	animator.GetRootBlendTreeConstantAndWorkspace (layerIndex, stateHash, nodeConstant, workspace);
	if (nodeConstant == NULL)
		return;
	mecanim::animation::GetWeights (*nodeConstant, *workspace, weightArray, blendX, blendY);
}

void AnimatorUtility::CalculateBlendTexture (Animator& animator, int layerIndex, int stateHash, Texture2D* blendTexture, vector<Texture2D*> weightTextures, float minX, float minY, float maxX, float maxY)
{
	mecanim::animation::BlendTreeNodeConstant const* nodeConstant = NULL;
	mecanim::animation::BlendTreeWorkspace* workspace = NULL;
	animator.GetRootBlendTreeConstantAndWorkspace (layerIndex, stateHash, nodeConstant, workspace);
	if (nodeConstant == NULL)
		return;
	
	int width = blendTexture->GetDataWidth ();
	int height = blendTexture->GetDataHeight ();
	int childCount = nodeConstant->m_ChildCount;
	
	float* weights;
	ALLOC_TEMP (weights, float, childCount);
	ColorRGBA32* bData;
	ALLOC_TEMP (bData, ColorRGBA32, width * height);
	ColorRGBA32* wData;
	ALLOC_TEMP (wData, ColorRGBA32, width * height * childCount);
	
	for (int i=0; i<width; i++)
	{
		for (int j=0; j<height; j++)
		{
			int p = i + width * j;
			mecanim::animation::GetWeights (*nodeConstant, *workspace, weights, minX + (maxX-minX) * i / (float)width, minY + (maxY-minY) * j / (float)height);
			
			float blend = 0;
			for (int c=0; c<childCount; c++)
			{
				int offset = c * width * height;
				wData[p + offset] = ColorRGBA32 (255, 255, 255, (int)(weights[c] * 255));
				blend += weights[c] * weights[c];
			}
			bData[p] = ColorRGBA32 (255, 255, 255, (int)(blend * 255));
		}
	}
	
	blendTexture->SetPixels32 (0, bData, width * height);
	blendTexture->UpdateImageData ();
	for (int c=0; c<childCount; c++)
	{
		if (weightTextures[c] == NULL)
			continue;
		int offset = c * width * height;
		weightTextures[c]->SetPixels32 (0, &wData[offset], width * height);
		weightTextures[c]->UpdateImageData ();
	}
}
