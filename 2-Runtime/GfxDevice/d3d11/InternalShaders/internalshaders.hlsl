//--------------------------------------------------------------------------------------
// Vertex shaders for stream-out GPU skinning.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer cbBones : register ( b0 )
{
	float4x3 bones[BONECOUNT];
}

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct VS_IN_MEX
{
	// Stream 0
    float3  Position        : POSITION;
    float3	Normal			: NORMAL;
    float4	Tangent			: TANGENT;

	// Stream 1
#if BONESPERVERTEX == 4
    float4  BoneWeights     : BLENDWEIGHT;
	int4  BoneIndices     : BLENDINDICES;

#elif BONESPERVERTEX == 2
    float2  BoneWeights     : BLENDWEIGHT;
	int2  BoneIndices     : BLENDINDICES;

#else // 1 bone per vertex
	int  BoneIndices     : BONEINDEX;

#endif
};

struct VS_OUTPUT
{
	float3 vPosition	: POSITION;
	float3 vNormal		: TEXCOORD0;
	float4 vTangent		: TEXCOORD1;
};

// --- Bones ---
inline float4x3 FetchBoneMatrix( int boneIndex )
{   
	return (float4x3)(bones[boneIndex]);
}
//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain( VS_IN_MEX Input, uniform bool useNormal, uniform bool useTangent)
{
	VS_OUTPUT Output = (VS_OUTPUT)0;
	
#if BONESPERVERTEX == 4
	float boneWeights[4] = (float[4])Input.BoneWeights;  
    int boneIndices[4] = (int[4])Input.BoneIndices;
	
	float4x3 localToWorldMatrix = boneWeights[0] * FetchBoneMatrix(boneIndices[0]) +
				boneWeights[1] * FetchBoneMatrix(boneIndices[1]) +
				boneWeights[2] * FetchBoneMatrix(boneIndices[2]) +
				boneWeights[3] * FetchBoneMatrix(boneIndices[3]);

#elif BONESPERVERTEX == 2
	float boneWeights[2] = (float[2])Input.BoneWeights;  
    int boneIndices[2] = (int[2])Input.BoneIndices;
	
	float4x3 localToWorldMatrix = boneWeights[0] * FetchBoneMatrix(boneIndices[0]) +
				boneWeights[1] * FetchBoneMatrix(boneIndices[1]);


#else // 1
    int boneIndex = Input.BoneIndices;
	
	float4x3 localToWorldMatrix = FetchBoneMatrix(boneIndex);
#endif

	// Position
	Output.vPosition = mul( float4(Input.Position.xyz, 1.0), localToWorldMatrix ).xyz;
	if (useNormal)
    {
		Output.vNormal = normalize( mul( float4(Input.Normal.xyz,0.0f), localToWorldMatrix ) ).xyz;
	}
	
	// Tangent
	if (useTangent)
	{
		float3 outTangent3 = normalize( mul( float4(Input.Tangent.xyz,0.0f), localToWorldMatrix ) ).xyz;
		Output.vTangent = float4(outTangent3, Input.Tangent.w);
	}
	
	return Output;
}

// Functions are named StreamOutSkinVS_<components>_<bonespervertex>_<maxbonecount>

#define MERGE(a, b, c, delim) a##delim##b##delim##c

VS_OUTPUT MERGE(StreamOutSkinVS_Position,BONESPERVERTEX,BONECOUNT,_)(VS_IN_MEX Input)
{
	return VSMain(Input, false, false);
}

VS_OUTPUT MERGE(StreamOutSkinVS_Position_Normal,BONESPERVERTEX,BONECOUNT,_)(VS_IN_MEX Input)
{
	return VSMain(Input, true, false);
}

VS_OUTPUT MERGE(StreamOutSkinVS_Position_Normal_Tangent,BONESPERVERTEX,BONECOUNT,_)(VS_IN_MEX Input)
{
	return VSMain(Input, true, true);
}

VS_OUTPUT MERGE(StreamOutSkinVS_Position_Tangent,BONESPERVERTEX,BONECOUNT,_)(VS_IN_MEX Input)
{
	return VSMain(Input, false, true);
}
