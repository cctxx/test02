#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS && UNITY_SUPPORTS_SSE && !UNITY_64

#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Math/Matrix4x4.h"

bool SkinMeshOptimizedSSE2(SkinMeshInfo& info);
void SkinMesh(SkinMeshInfo& info);

Vector3f RandomVector3InUnitBox(Rand& rnd)
{
    return Vector3f(rnd.GetSignedFloat(),
                    rnd.GetSignedFloat(),
                    rnd.GetSignedFloat());
}

SUITE (MeshSkinningTests)
{
TEST(MeshSkinning_AllFeatures)
{
    int failedPositions = 0;
    int failedNormals = 0;
    int failedTangents = 0;
    int failedTangentSigns = 0;
    int failedVertexCopies = 0;

    const int minVertices = 1;
    const int maxVertices = 100;
    const int positionSize = 3*sizeof(float);
    const int normalSize = 3*sizeof(float);
    const int tangentSize = 4*sizeof(float);
	const int maxStride = positionSize + normalSize + tangentSize;
    const int trailingBytes = 128;

    UInt8 inVertices[maxVertices * maxStride];
    UInt8 outVerticesRef[maxVertices * maxStride + trailingBytes];
    UInt8 outVerticesSimd[maxVertices * maxStride + trailingBytes];

    SkinMeshInfo info;
    memset(&info, 0, sizeof(info));
    info.inVertices = inVertices;
    info.vertexCount = minVertices;
	info.normalOffset = positionSize;
    info.tangentOffset = positionSize + normalSize;

	// Try a large offset so AABBs don't contain (0,0,0)
	Vector3f posOffset(-2000, 0, 2000);

    const int numBones = 64;
	Matrix4x4f *cachedPose;
    ALLOC_TEMP_ALIGNED(cachedPose, Matrix4x4f, numBones, 32);
	info.cachedPose = cachedPose;
    for (int i = 0; i < numBones; i++)
    {
        Matrix4x4f mat;
        mat.SetScale(Vector3f(1.0 + 0.5f*sin(i*0.3f),
                              1.0 + 0.5f*sin(i*0.5f),
                              1.0 + 0.5f*sin(i*0.7f)));
        mat.SetPosition(Vector3f(100.0f*sin(i*1.0f),
                                 100.0f*sin(i*2.5f),
                                 100.0f*sin(i*3.3f)) + posOffset);
        cachedPose[i] = mat;
    }
    info.boneCount = numBones;

    Rand rnd(123);

    int boneIndices[maxVertices];
    BoneInfluence2 boneInfl2[maxVertices];
    BoneInfluence boneInfl4[maxVertices];
    for (int i = 0; i < maxVertices; i++)
    {
        boneIndices[i] = i%numBones;

        BoneInfluence2& b2 = boneInfl2[i];
        b2.boneIndex[0] = (i)%numBones;
        b2.boneIndex[1] = (i/2+10)%numBones;
	    b2.weight[0] = rnd.GetFloat();
        b2.weight[1] = 1.0f - b2.weight[0];

        BoneInfluence& b4 = boneInfl4[i];
        b4.boneIndex[0] = (i)%numBones;
        b4.boneIndex[1] = (i/2+10)%numBones;
        b4.boneIndex[2] = (i/3+20)%numBones;
        b4.boneIndex[3] = (i/4+30)%numBones;
        float weightLeft = 1.0f;
        for (int j=0; j<3; j++)
        {
            b4.weight[j] = weightLeft * rnd.GetFloat();
            weightLeft -= b4.weight[j];
        }
        b4.weight[3] = weightLeft;
    }

    for (info.bonesPerVertex = 1; info.bonesPerVertex <= 4; info.bonesPerVertex++)
    {
        if (info.bonesPerVertex == 3) continue;

        switch (info.bonesPerVertex)
        {
            case 1:
                info.compactSkin = boneIndices;
                break;
            case 2:
                info.compactSkin = boneInfl2;
                break;
            case 4:
                info.compactSkin = boneInfl4;
                break;
        }

        for (int skinNormals = 0; skinNormals <= 1; skinNormals++)
        {
            info.skinNormals = (skinNormals != 0);

            for (int skinTangents = 0; skinTangents <= 1; skinTangents++)
            {
                if (!skinNormals && skinTangents) continue;
                info.skinTangents = (skinTangents != 0);

                // Randomize vertex count and stride
                info.vertexCount += 7;
                while (info.vertexCount > maxVertices) info.vertexCount -= (maxVertices - minVertices);
                info.inStride = positionSize;
				info.inStride += skinNormals ? normalSize : 0;
				info.inStride += skinTangents ? tangentSize : 0;
				info.outStride = info.inStride;

                UInt8* inVert = inVertices;
                for (int i = 0; i < info.vertexCount; i++)
                {
                    Vector3f* nextVec = (Vector3f*)inVert;
                    Vector3f pos = RandomVector3InUnitBox(rnd);
                    pos *= 1000.0f;
                    *nextVec++ = pos;
                    if (info.skinNormals)
                    {
                        Vector3f normal = RandomVector3InUnitBox(rnd);
                        normal = NormalizeSafe(normal);
                        *nextVec++ = normal;
                    }

                    if (info.skinTangents)
                    {
                        Vector3f tangent = RandomVector3InUnitBox(rnd);
                        tangent = NormalizeSafe(tangent);
                        *nextVec++ = tangent;
                        float* tangentSign = (float*)nextVec;
                        *tangentSign = (rnd.GetSignedFloat() < 0.0f) ? -1.0f : 1.0f;
                    }
                    inVert += info.inStride;
                }

                int outSize = info.vertexCount * info.outStride;
                memset(outVerticesRef, 0xcc, outSize + trailingBytes);
                memset(outVerticesSimd, 0xdd, outSize + trailingBytes);

                info.outVertices = outVerticesRef;
                SkinMesh(info);

                info.outVertices = outVerticesSimd;
                bool successSimd = SkinMeshOptimizedSSE2(info);
                CHECK(successSimd);

                // Check if we wrote past end of buffer
                for (int i = 0; i < trailingBytes; i++)
                {
                    CHECK_EQUAL(0xcc, outVerticesRef[outSize + i]);
                    CHECK_EQUAL(0xdd, outVerticesSimd[outSize + i]);
                }

                inVert = inVertices;
                UInt8* vertRef = outVerticesRef;
                UInt8* vertSimd = outVerticesSimd;
                for (int i = 0; i < info.vertexCount; i++)
                {
                    Vector3f* posRef = (Vector3f*)vertRef;
                    Vector3f* posSimd = (Vector3f*)vertRef;
                    if (!CompareApproximately(*posRef, *posSimd))
                    {
                        failedPositions++;
                    }
                    if (info.skinNormals)
                    {
                        Vector3f* normalRef = (Vector3f*)(vertRef + info.normalOffset);
                        Vector3f* normalSimd = (Vector3f*)(vertRef + info.normalOffset);
                        if (!CompareApproximately(*normalRef, *normalSimd))
                        {
                            failedNormals++;
                        }
                    }
                    if (info.skinTangents)
                    {
                        Vector3f* tangentRef = (Vector3f*)(vertRef + info.tangentOffset);
                        Vector3f* tangentSimd = (Vector3f*)(vertRef + info.tangentOffset);
                        if (!CompareApproximately(*tangentRef, *tangentSimd))
                        {
                            failedTangents++;
                        }
                        float* tangentSignRef = (float*)(vertRef + info.tangentOffset + sizeof(Vector3f));
                        float* tangentSignSimd = (float*)(vertRef + info.tangentOffset + sizeof(Vector3f));
                        if (*tangentSignRef != *tangentSignSimd)
                        {
                            failedTangentSigns++;
                        }
                    }

                    inVert += info.inStride;
                    vertRef += info.outStride;
                    vertSimd += info.outStride;
                }
            }
        }
    }

    CHECK_EQUAL(0, failedPositions);
    CHECK_EQUAL(0, failedNormals);
    CHECK_EQUAL(0, failedTangents);
    CHECK_EQUAL(0, failedTangentSigns);
    CHECK_EQUAL(0, failedVertexCopies);
}
}
#endif
