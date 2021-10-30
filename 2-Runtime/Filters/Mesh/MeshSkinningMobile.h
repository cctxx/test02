#if UNITY_SUPPORTS_VFP

#if UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
#define s_SkinVertices_VFP								_s_SkinVertices_VFP
#define s_SkinVertices_NoNormals_VFP					_s_SkinVertices_NoNormals_VFP
#define s_SkinVertices_Tangents_VFP						_s_SkinVertices_Tangents_VFP

#define s_SkinVertices2Bones_VFP						_s_SkinVertices2Bones_VFP
#define s_SkinVertices2Bones_NoNormals_VFP				_s_SkinVertices2Bones_NoNormals_VFP
#define s_SkinVertices2Bones_Tangents_VFP				_s_SkinVertices2Bones_Tangents_VFP

#define s_SkinVertices4Bones_VFP						_s_SkinVertices4Bones_VFP
#define s_SkinVertices4Bones_NoNormals_VFP				_s_SkinVertices4Bones_NoNormals_VFP
#define s_SkinVertices4Bones_Tangents_VFP				_s_SkinVertices4Bones_Tangents_VFP
#endif // UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN

extern "C"
{
	void s_SkinVertices_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence1, void* dstVertData);
	void s_SkinVertices_NoNormals_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence1, void* dstVertData);
	void s_SkinVertices_Tangents_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence1, void* dstVertData);

	void s_SkinVertices2Bones_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence2, void* dstVertData);
	void s_SkinVertices2Bones_NoNormals_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence2, void* dstVertData);
	void s_SkinVertices2Bones_Tangents_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence2, void* dstVertData);

	void s_SkinVertices4Bones_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence4, void* dstVertData);
	void s_SkinVertices4Bones_NoNormals_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence4, void* dstVertData);
	void s_SkinVertices4Bones_Tangents_VFP(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const void* srcBoneInfluence4, void* dstVertData);
}
#endif

#if (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING)

#if UNITY_ANDROID || UNITY_WINRT || UNITY_BB10 || UNITY_TIZEN
#define s_SkinVertices_NEON								_s_SkinVertices_NEON
#define s_SkinVertices_NoNormals_NEON					_s_SkinVertices_NoNormals_NEON
#define s_SkinVertices_Tangents_NEON					_s_SkinVertices_Tangents_NEON

#define s_SkinVertices2Bones_NEON						_s_SkinVertices2Bones_NEON
#define s_SkinVertices2Bones_NoNormals_NEON				_s_SkinVertices2Bones_NoNormals_NEON
#define s_SkinVertices2Bones_Tangents_NEON				_s_SkinVertices2Bones_Tangents_NEON

#define s_SkinVertices4Bones_NEON						_s_SkinVertices4Bones_NEON
#define s_SkinVertices4Bones_NoNormals_NEON				_s_SkinVertices4Bones_NoNormals_NEON
#define s_SkinVertices4Bones_Tangents_NEON				_s_SkinVertices4Bones_Tangents_NEON

#endif // UNITY_ANDROID || UNITY_WINRT || UNITY_BB10 || UNITY_TIZEN

extern "C"
{
	void s_SkinVertices_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const int* srcBoneInfluence1, void* dstVertData);
	void s_SkinVertices_NoNormals_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const int* srcBoneInfluence1, void* dstVertData);
	void s_SkinVertices_Tangents_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const int* srcBoneInfluence1, void* dstVertData);

	void s_SkinVertices2Bones_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const BoneInfluence2* srcBoneInfluence2, void* dstVertData);
	void s_SkinVertices2Bones_NoNormals_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const BoneInfluence2* srcBoneInfluence2, void* dstVertData);
	void s_SkinVertices2Bones_Tangents_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const BoneInfluence2* srcBoneInfluence2, void* dstVertData);

	void s_SkinVertices4Bones_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const BoneInfluence* srcBoneInfluences, void* dstVertData);
	void s_SkinVertices4Bones_NoNormals_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const BoneInfluence* srcBoneInfluences, void* dstVertData);
	void s_SkinVertices4Bones_Tangents_NEON(const Matrix4x4f* bones4x4, const void* srcVertData, const void* srcVertDataEnd, const BoneInfluence* srcBoneInfluences, void* dstVertData);
}
#endif

#if UNITY_SUPPORTS_VFP || (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING)

bool SkinMeshOptimizedMobile(SkinMeshInfo& info)
{
	static const size_t kPrefetchSizeBones  = 4096;
	static const size_t kPrefetchSizeVertex = 512;

	const int bonesPerVertexCount = info.bonesPerVertex;
	const bool skinNormal = info.skinNormals;
	const bool skinTangent = info.skinTangents;

	const int* influence1 = reinterpret_cast<const int*> (info.compactSkin);
	const BoneInfluence2* influence2 = reinterpret_cast<const BoneInfluence2*> (info.compactSkin);
	const BoneInfluence* influence4 = reinterpret_cast<const BoneInfluence*> (info.compactSkin);

	const Matrix4x4f* bones4x4 = info.cachedPose;

	const int inStride = info.inStride;
	int count = info.vertexCount;

	const UInt8* inputVertex = (const UInt8*)info.inVertices;
	UInt8* outputVertex = (UInt8*)info.outVertices;

	if (skinTangent && !skinNormal)
		return false;

	if( !UNITY_SUPPORTS_VFP && !CPUInfo::HasNEONSupport() )
	{
		ErrorString("non-NEON path not enabled!");
		return false;
	}

#if !ENABLE_MULTITHREADED_SKINNING
	PROFILER_AUTO_THREAD_SAFE(gMeshSkinningOptimized, NULL);
#endif

	Prefetch(bones4x4, std::min<size_t>(info.boneCount * sizeof(Matrix4x4f), kPrefetchSizeBones));
	Prefetch(inputVertex + inStride, std::min<size_t>(inStride * (count-1), kPrefetchSizeVertex));

#if UNITY_SUPPORTS_NEON && UNITY_SUPPORTS_VFP
#define CALL_SKIN_FUNC( name, influence )																	\
do 																											\
{																											\
if (CPUInfo::HasNEONSupport())																				\
	name##_NEON(bones4x4, inputVertex, (UInt8*)inputVertex + (inStride * count), influence, outputVertex);	\
else																										\
	name##_VFP(bones4x4, inputVertex, (UInt8*)inputVertex + (inStride * count), influence, outputVertex);	\
}																											\
while(0)
#endif
#if UNITY_SUPPORTS_NEON && !UNITY_SUPPORTS_VFP
#define CALL_SKIN_FUNC( name, influence ) name##_NEON(bones4x4, inputVertex, (UInt8*)inputVertex + (inStride * count), influence, outputVertex)
#endif
#if UNITY_SUPPORTS_VFP && !UNITY_SUPPORTS_NEON
#define CALL_SKIN_FUNC( name, influence ) name##_VFP(bones4x4, inputVertex, (UInt8*)inputVertex + (inStride * count), influence, outputVertex)
#endif

	if (bonesPerVertexCount == 1 )
	{
		if (skinNormal && skinTangent)
			CALL_SKIN_FUNC(s_SkinVertices_Tangents, influence1);
		else if( skinNormal )
			CALL_SKIN_FUNC(s_SkinVertices, influence1);
		else
			CALL_SKIN_FUNC(s_SkinVertices_NoNormals, influence1);
	}
	else if (bonesPerVertexCount == 2)
	{
		if (skinNormal && skinTangent)
			CALL_SKIN_FUNC(s_SkinVertices2Bones_Tangents, influence2);
		else if( skinNormal )
			CALL_SKIN_FUNC(s_SkinVertices2Bones, influence2);
		else
			CALL_SKIN_FUNC(s_SkinVertices2Bones_NoNormals, influence2);
	}
	else if (bonesPerVertexCount == 4)
	{
		if (skinNormal && skinTangent)
			CALL_SKIN_FUNC(s_SkinVertices4Bones_Tangents, influence4);
		else if (skinNormal)
			CALL_SKIN_FUNC(s_SkinVertices4Bones, influence4);
		else
			CALL_SKIN_FUNC(s_SkinVertices4Bones_NoNormals, influence4);
	}

	return true;
}
#else
bool SkinMeshOptimizedMobile(SkinMeshInfo& info)
{
	return false;
}
#endif // UNITY_SUPPORTS_VFP || UNITY_SUPPORTS_NEON


