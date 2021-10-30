#include "UnityPrefix.h"
#include "MeshBlendShaping.h"
#include "MeshSkinning.h"
#include "MeshBlendShape.h"

template<bool skinNormal, bool skinTangent>
void ApplyBlendShapeTmpl (const BlendShapeVertex* vertices, size_t vertexCount, size_t dstVertexCount, float weight, int normalOffset, int tangentOffset, int inStride, UInt8* dst)
{
	for (int i = 0; i < vertexCount; ++i)
	{
		const BlendShapeVertex& blendShapeVertex = vertices[i];
		
		int offset = inStride * blendShapeVertex.index;

		*reinterpret_cast<Vector3f*>(dst + offset) += blendShapeVertex.vertex * weight;
		if (skinNormal)
		{
			DebugAssert (offset + normalOffset < inStride * dstVertexCount);
			*reinterpret_cast<Vector3f*>(dst + offset + normalOffset) += blendShapeVertex.normal * weight;
		}
		if (skinTangent)
		{
			DebugAssert (offset + tangentOffset < inStride * dstVertexCount);
			*reinterpret_cast<Vector3f*>(dst + offset + tangentOffset) += blendShapeVertex.tangent * weight;
		}				
	}
}


void ApplyBlendShape (const BlendShape& target, const BlendShapeVertices& vertices, float weight, const SkinMeshInfo& info, UInt8* dst)
{
	if (!HasValidWeight(weight))
		return;
	
	weight = std::min(weight, 1.0F);
	
	const BlendShapeVertex* v = vertices.begin() + target.firstVertex;
	
	if (info.skinNormals && info.skinTangents && target.hasNormals && target.hasTangents)
		ApplyBlendShapeTmpl<true, true> (v, target.vertexCount, info.vertexCount, weight, info.normalOffset, info.tangentOffset, info.inStride, dst);
	else if (info.skinNormals && target.hasNormals)
		ApplyBlendShapeTmpl<true, false> (v, target.vertexCount, info.vertexCount, weight, info.normalOffset, info.tangentOffset, info.inStride, dst);
	else
		ApplyBlendShapeTmpl<false, false> (v, target.vertexCount, info.vertexCount, weight, info.normalOffset, info.tangentOffset, info.inStride, dst);
}

static int FindFrame (const float* weights, size_t count, float targetWeight)
{
	// Find frame (left index)
	int frame = 0;
	while (frame < count-1 && targetWeight > weights[frame+1])
		frame++;
	
	return frame;
}

void ApplyBlendShapes (SkinMeshInfo& info, UInt8* dst)
{
	DebugAssert (info.blendshapeCount != 0);
	Assert (info.inStride == info.outStride);
	const int inStride = info.inStride;
	const int count = info.vertexCount;

	Assert (dst);
	memcpy (dst, info.inVertices, inStride * count);		

	const BlendShapeData& blendShapeData = *info.blendshapes;
	
	for (int c = 0; c < info.blendshapeCount; ++c)
	{
		const float targetWeight = info.blendshapeWeights[c];

		if (!HasValidWeight (targetWeight))
			continue;

		const BlendShapeChannel& channel = blendShapeData.channels[c];
		Assert(channel.frameCount != 0);

		const BlendShape* blendShapeFrames = &blendShapeData.shapes[channel.frameIndex];
		const float* weights = &blendShapeData.fullWeights[channel.frameIndex];
		
		// The first blendshape does not need to do any blending. Just fade it in.
		if (targetWeight < weights[0] || channel.frameCount == 1)
		{
			float lhsShapeWeight = weights[0];
			ApplyBlendShape (blendShapeFrames[0], blendShapeData.vertices, targetWeight / lhsShapeWeight, info, dst);
		}
		// We are blending with two frames
		else
		{
			// Find the frame we are blending with
			int frame = FindFrame(weights, channel.frameCount, targetWeight);
			
			float lhsShapeWeight = weights[frame + 0];
			float rhsShapeWeight = weights[frame + 1];
			
			float relativeWeight = (targetWeight - lhsShapeWeight) / (rhsShapeWeight - lhsShapeWeight);
			
			ApplyBlendShape (blendShapeFrames[frame + 0], blendShapeData.vertices, 1.0F - relativeWeight, info, dst);
			ApplyBlendShape (blendShapeFrames[frame + 1], blendShapeData.vertices, relativeWeight, info, dst);
		}
	}
}

///@TODO: How do we deal with resizing vertex count once mesh blendshapes have been created???

/*
 template<bool skinNormal, bool skinTangent>
 static void ApplyBlendShapesTmpl (SkinMeshInfo& info, UInt8* dst)
 {
 DebugAssert (info.blendshapeCount != 0);
 Assert (info.inStride == info.outStride);
 const int inStride = info.inStride;
 const int count = info.vertexCount;
 
 Assert (dst);
 memcpy (dst, info.inVertices, inStride * count);		
 
 const int normalOffset = info.normalOffset;
 const int tangentOffset = info.tangentOffset;
 
 #if BLEND_DIRECT_NORMALS
 if (skinNormal)
 { // figure out how what fraction of original normal should be used
 float totalBlendshapeWeight = 0.0f;
 for (int i = 0; i < info.blendshapeCount; ++i)
 totalBlendshapeWeight += info.blendshapeWeights[i];
 Assert (totalBlendshapeWeight >= 0.0f);
 if (totalBlendshapeWeight > 0.0f)
 {
 for (int i = 0; i < count; ++i)
 *reinterpret_cast<Vector3f*>(dst + i*inStride + normalOffset) *= max(0.0f, (1.0f - totalBlendshapeWeight));
 }
 }
 
 bool atLeastOneSparseBlendshape = false;
 #endif
 for (int bs = 0; bs < info.blendshapeCount; ++bs)
 {
 const float w = info.blendshapeWeights[bs];
 
 if (HasWeight(w))
 {
 const MeshBlendShape& blendShape = info.blendshapes[bs];
 
 const BlendShapeVertex* vertices = info.blendshapesVertices + blendShape.firstVertex;
 for (int i = 0; i < blendShape.vertexCount; ++i)
 {
 const BlendShapeVertex& blendShapeVertex = vertices[i];
 
 int offset = inStride * blendShapeVertex.index;
 Assert (offset < inStride * count);
 *reinterpret_cast<Vector3f*>(dst + offset) += blendShapeVertex.vertex * w;
 if (skinNormal)
 {
 Assert (offset + normalOffset < inStride * count);
 *reinterpret_cast<Vector3f*>(dst + offset + normalOffset) += blendShapeVertex.normal * w;
 }
 if (skinTangent)
 {
 Assert (offset + tangentOffset < inStride * count);
 *reinterpret_cast<Vector3f*>(dst + offset + tangentOffset) += blendShapeVertex.tangent * w;
 }				
 }
 
 #if BLEND_DIRECT_NORMALS
 if (vertices.size () < count)
 atLeastOneSparseBlendshape = true;
 #endif
 }
 }
 
 #if BLEND_DIRECT_NORMALS
 if (atLeastOneSparseBlendshape && skinNormal) // we might need to take larger fraction from original normal
 for (int i = 0; i < count; ++i)
 {	
 Vector3f const& srcNormal = *reinterpret_cast<Vector3f*>((UInt8*)info.inVertices + i*inStride + normalOffset);
 Vector3f* dstNormal = reinterpret_cast<Vector3f*>(dst + i*inStride + normalOffset);
 const float missingFractionOfNormal = max (0.0f, 1.0f - Magnitude (*dstNormal));
 *dstNormal += srcNormal * missingFractionOfNormal;
 }
 #endif
 }
*/