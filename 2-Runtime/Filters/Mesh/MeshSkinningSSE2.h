#if UNITY_SUPPORTS_SSE && !UNITY_64

#if UNITY_OSX || UNITY_LINUX
#define __cdecl
#endif

#define SKIN_SSE2_PARAMS \
    const void* inVertices, \
    void* outVertices, \
    int numVertices, \
    const void* boneMatrices, \
    const void* weightsAndIndices, \
    int inputStride, \
    int outputStride

typedef void (__cdecl *SkinSSE2_Function)(SKIN_SSE2_PARAMS);

extern "C"
{
    void __cdecl SkinSSE2_1Bone_Pos(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_2Bones_Pos(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_4Bones_Pos(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_1Bone_PosNormal(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_2Bones_PosNormal(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_4Bones_PosNormal(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_1Bone_PosNormalTan(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_2Bones_PosNormalTan(SKIN_SSE2_PARAMS);
    void __cdecl SkinSSE2_4Bones_PosNormalTan(SKIN_SSE2_PARAMS);
}


bool SkinMeshOptimizedSSE2(SkinMeshInfo& info)
{
	if (!CPUInfo::HasSSE2Support())
    {
        return false;
    }

	SkinSSE2_Function skinFunc = NULL;

	if (!info.skinNormals && !info.skinTangents)
	{
		switch (info.bonesPerVertex)
		{
			DebugAssert(info.inStride == sizeof(Vector3f));
			case 1:
				skinFunc = &SkinSSE2_1Bone_Pos;
				break;
			case 2:
				skinFunc = &SkinSSE2_2Bones_Pos;
				break;
			case 4:
				skinFunc = &SkinSSE2_4Bones_Pos;
				break;
				
		}
	}
	else if (info.skinNormals && !info.skinTangents)
	{
		DebugAssert(info.inStride == sizeof(Vector3f) + sizeof(Vector3f));
		switch (info.bonesPerVertex)
		{
			case 1:
				skinFunc = &SkinSSE2_1Bone_PosNormal;
				break;
			case 2:
				skinFunc = &SkinSSE2_2Bones_PosNormal;
				break;
			case 4:
				skinFunc = &SkinSSE2_4Bones_PosNormal;
				break;
				
		}
	}
	else if (info.skinNormals && info.skinTangents)
    {
		DebugAssert(info.inStride == sizeof(Vector3f) + sizeof(Vector3f) + sizeof(Vector4f));
		switch (info.bonesPerVertex)
        {
			case 1:
				skinFunc = &SkinSSE2_1Bone_PosNormalTan;
				break;
			case 2:
				skinFunc = &SkinSSE2_2Bones_PosNormalTan;
				break;
			case 4:
				skinFunc = &SkinSSE2_4Bones_PosNormalTan;
				break;
				
		}
	}
	
	if (skinFunc == NULL)
		return false;
	
	// Skin all vertices apart from last one!
	if (info.vertexCount > 1)
	{
		(*skinFunc)(info.inVertices, info.outVertices, info.vertexCount - 1,info.cachedPose, info.compactSkin, info.inStride, info.outStride);
	}
	// Copy last vertex to stack to avoid reading/writing past end of buffer
	if (info.vertexCount > 0)
	{
		const int maxStride = 2 * sizeof(Vector3f) + sizeof(Vector4f) + 4;
		Assert(info.inStride <= maxStride && info.outStride <= maxStride);
		// Need 4 bytes padding to access Vec3 as Vec4
		char vertexCopyIn[maxStride + 4];
		char vertexCopyOut[maxStride + 4];
		int skinStride = (info.bonesPerVertex == 4) ? sizeof(BoneInfluence) :
			(info.bonesPerVertex == 2) ? sizeof(BoneInfluence2) : 
			(info.bonesPerVertex == 1) ? sizeof(int) : 0;
		Assert(skinStride != 0);
		int index = info.vertexCount - 1;
		const char* compactSkin = static_cast<const char*>(info.compactSkin) + index * skinStride;
		const char* inVertex = static_cast<const char*>(info.inVertices) + index * info.inStride;
		char* outVertex = static_cast<char*>(info.outVertices) + index * info.outStride;
		memcpy(vertexCopyIn, inVertex, info.inStride);
		(*skinFunc)(vertexCopyIn, vertexCopyOut, 1, info.cachedPose, compactSkin, info.inStride, info.outStride);
		memcpy(outVertex, vertexCopyOut, info.outStride);
	}
	
    return true;
}
#else
inline bool SkinMeshOptimizedSSE2(SkinMeshInfo& info)
{
	return false;
}
#endif
