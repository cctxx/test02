#include "UnityPrefix.h"
#include "Runtime/GfxDevice/d3d11/GfxDeviceD3D11.h"
#include "Runtime/GfxDevice/d3d11/StreamOutSkinnedMesh.h"
#include "Runtime/GfxDevice/d3d11/D3D11Context.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/GfxDevice/d3d11/InternalShaders/builtin.h"
#include "Runtime/GfxDevice/d3d11/D3D11VBO.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "Runtime/GfxDevice/threaded/ThreadedDeviceStates.h"
#include "Runtime/GfxDevice/threaded/ThreadedVBO.h"

struct ShaderPair
{
	ID3D11GeometryShader*	m_GeometryShader;
	ID3D11VertexShader*		m_VertexShader;
	ID3D11InputLayout*		m_InputLayout;
};

typedef UInt32 InputSpec; // shaderChannelsMap + (bonesPerVertex << 16) + (maxBonesBits << 19)

typedef std::map< InputSpec, ShaderPair> StreamOutShaderMap;

struct SkinningGlobalsD3D11
{
	SkinningGlobalsD3D11()
		: m_BoundSP(0)
		, m_OldGS(0)
		, m_OldPS(0)
		, m_OldVS(0)
		, m_OldTopo(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		, m_DSState(0)
		, m_OldCB(0)
	{
	}

	StreamOutShaderMap m_ShaderMap;
	ShaderPair* m_BoundSP;
	ID3D11GeometryShader*	m_OldGS;
	ID3D11PixelShader*		m_OldPS;
	ID3D11VertexShader*		m_OldVS;
	D3D11_PRIMITIVE_TOPOLOGY m_OldTopo;
	ID3D11DepthStencilState* m_DSState;
	ID3D11Buffer*			m_OldCB;
};
static SkinningGlobalsD3D11 s_SkinningGlobals;

enum MemExShaderChannel
{
	kMEXC_Position		= VERTEX_FORMAT1(Vertex),
	kMEXC_Normal		= VERTEX_FORMAT1(Normal),
	kMEXC_Tangent		= VERTEX_FORMAT1(Tangent),
};

void StreamOutSkinningInfo::CleanUp()
{
	StreamOutShaderMap::iterator it;
	for (it = s_SkinningGlobals.m_ShaderMap.begin(); it != s_SkinningGlobals.m_ShaderMap.end(); ++it)
	{
		if (it->second.m_GeometryShader)
		{
			ULONG refCount = it->second.m_GeometryShader->Release();
			AssertIf(refCount != 0);
		}
		if (it->second.m_VertexShader)
		{
			ULONG refCount = it->second.m_VertexShader->Release();
			AssertIf(refCount != 0);
		}
	}
	s_SkinningGlobals.m_ShaderMap.clear();

	s_SkinningGlobals.m_BoundSP = NULL;
	SAFE_RELEASE(s_SkinningGlobals.m_OldVS);
	SAFE_RELEASE(s_SkinningGlobals.m_OldGS);
	SAFE_RELEASE(s_SkinningGlobals.m_OldPS);
	SAFE_RELEASE(s_SkinningGlobals.m_OldCB);
	s_SkinningGlobals.m_OldTopo = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	SAFE_RELEASE(s_SkinningGlobals.m_DSState);
}

static UInt32 roundUpToNextPowerOf2(UInt32 in)
{
	// Round up to nearest power of 2
	// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	in--;
	in |= in >> 1;
	in |= in >> 2;
	in |= in >> 4;
	in |= in >> 8;
	in |= in >> 16;
	in++;
	return in;
}
// Get the bones bit index based on bone count. Assumes bonecount is power of 2
static int getBonesBits(UInt32 boneCount)
{
	// Calculate ln2
	// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn

	static const int MultiplyDeBruijnBitPosition2[32] = 
	{
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	UInt32 res = MultiplyDeBruijnBitPosition2[(UInt32)(boneCount * 0x077CB531U) >> 27];

	if(res < 5) // Minimum size is 32 (= 0)
		return 0;

	return res-5; // Adjust so that 32 = 0, 64 = 1 etc.
}


// maxBonesBits == Max bone count: 0 = 32, 1 = 64, etc until 5 = 1024

static const BYTE* GetMemExShaderCode(UInt32 shaderChannelsMap, int bonesPerVertex, UInt32 maxBonesBits, UInt32& bytecodeSize)
{
	int channelIdx = 0;
	switch(shaderChannelsMap)
	{
	case kMEXC_Position:							channelIdx = 0; break;
	case kMEXC_Position|kMEXC_Normal:				channelIdx = 1; break;
	case kMEXC_Position|kMEXC_Normal|kMEXC_Tangent: channelIdx = 2; break;
	case kMEXC_Position|kMEXC_Tangent:				channelIdx = 3; break;
	default:
		Assert(0 && "Unsupported vertex format for GPU skinning.");
		bytecodeSize = 0;
		return 0;
	};

	if(maxBonesBits > 5)
		maxBonesBits = 5; // TODO: Alert of too many bones per vertex

	// Adjust bonespervertex to dense array (1, 2, 4 => 0, 1, 2)
	if(bonesPerVertex == 4)
		bonesPerVertex--;
	bonesPerVertex--;

	bytecodeSize = g_StreamOutShaderSizes[channelIdx][maxBonesBits][bonesPerVertex];
	return g_StreamOutShaders[channelIdx][maxBonesBits][bonesPerVertex];
}

static ShaderPair* GetMemExShader(UInt32 shaderChannelsMap, UInt32 bonesPerVertex, UInt32 maxBonesBits)
{
	// Already have vertex shader for this format?
	InputSpec is = shaderChannelsMap + (bonesPerVertex << 16) + (maxBonesBits << 19);

	StreamOutShaderMap::iterator it = s_SkinningGlobals.m_ShaderMap.find(is);
	if (it != s_SkinningGlobals.m_ShaderMap.end())
		return &it->second;

	UInt32 codeSize = 0;
	const BYTE* code = GetMemExShaderCode(shaderChannelsMap, bonesPerVertex, maxBonesBits, codeSize);
	if (!code)
		return NULL;

	D3D11_SO_DECLARATION_ENTRY entries[16] = {0};
	D3D11_SO_DECLARATION_ENTRY* entry = entries;

	
	entry->SemanticName = "POSITION";
	entry->SemanticIndex = 0;
	entry->StartComponent = 0;
	entry->ComponentCount = 3;
	entry->OutputSlot = 0;
	entry++;
	
	UINT soStride = sizeof(float) * 3;
	UInt32 semanticIndex = 0; 
	
	if(shaderChannelsMap & kMEXC_Normal)
	{
		entry->SemanticName = "TEXCOORD";
		entry->SemanticIndex = semanticIndex++;
		entry->StartComponent = 0;
		entry->ComponentCount = 3;
		entry->OutputSlot = 0;
		entry++;

		soStride += 3 * sizeof(float);
	}

	if(shaderChannelsMap & kMEXC_Tangent)
	{
		entry->SemanticName = "TEXCOORD";
		entry->SemanticIndex = semanticIndex++;
		entry->StartComponent = 0;
		entry->ComponentCount = 4;
		entry->OutputSlot = 0;
		entry++;

		soStride += 4 * sizeof(float);
	}

	ShaderPair sp;
	GetD3D11Device()->CreateVertexShader( 
		code, 
		codeSize, 
		0, &sp.m_VertexShader);

	const DX11FeatureLevel fl = gGraphicsCaps.d3d11.featureLevel;

	GetD3D11Device()->CreateGeometryShaderWithStreamOutput( 
		code, 
		codeSize, 
		entries, 
		(entry-entries), 
		&soStride, 
		1, 
		(fl >= kDX11Level11_0) ? D3D11_SO_NO_RASTERIZED_STREAM : 0, 
		0, 
		&sp.m_GeometryShader);

	sp.m_InputLayout = GetD3D11GfxDevice().GetVertexDecls().GetVertexDecl(shaderChannelsMap, (void*)code, codeSize, true, bonesPerVertex);

	s_SkinningGlobals.m_ShaderMap.insert(std::make_pair(is, sp));

	return &s_SkinningGlobals.m_ShaderMap.find( is )->second;
}

// Note: we might not support all formats all the time.
static bool DoesVertexFormatQualifyForMemExport(UInt32 shaderChannelsMap)
{
	// 1. Channel filter
	UInt32 unsupported = ~(kMEXC_Position | kMEXC_Normal | kMEXC_Tangent); // TODO: support more channels
	//UInt32 unsupported = ~(kMEXC_Position | kMEXC_Normal | kMEXC_Color | kMEXC_UV0 | kMEXC_UV1 | kMEXC_Tangent);
	bool qualify = ((shaderChannelsMap & unsupported) == 0);
	// 3. Must have position
	qualify &= (shaderChannelsMap & kMEXC_Position) != 0;

	return qualify;

}

void StreamOutSkinningInfo::SkinMesh(bool last)
{
//	Assert(DoesVertexFormatQualifyForMemExport(info.channelMap));
	if (!DoesVertexFormatQualifyForMemExport(GetChannelMap()))
		return;

	// Get the vertex shader
	//...
	ShaderPair* sp = GetMemExShader(GetChannelMap(), GetBonesPerVertex(), getBonesBits(m_BoneCount));
	
	UINT offsets[] = {0};
	D3D11VBO* vbo = (D3D11VBO*)GetDestVBO();
	vbo->BindToStreamOutput();

	ID3D11DeviceContext* ctx = GetD3D11Context();
	
	if (s_SkinningGlobals.m_BoundSP == 0)
	{
		ctx->VSGetShader( &s_SkinningGlobals.m_OldVS, 0, 0 );
		ctx->GSGetShader( &s_SkinningGlobals.m_OldGS, 0, 0 );
		ctx->PSGetShader( &s_SkinningGlobals.m_OldPS, 0, 0 );
		ctx->IAGetPrimitiveTopology(&s_SkinningGlobals.m_OldTopo);
		ctx->VSGetConstantBuffers(0, 1, &s_SkinningGlobals.m_OldCB);
	}

	if (s_SkinningGlobals.m_BoundSP != sp)
	{
		ctx->VSSetShader( sp->m_VertexShader, 0, 0 );
		ctx->GSSetShader( sp->m_GeometryShader, 0, 0 );
		ctx->PSSetShader( 0, 0, 0 );
		s_SkinningGlobals.m_BoundSP = sp;
	}
	ID3D11Buffer* const cbs[] = { m_SourceBones };
	ctx->VSSetConstantBuffers(0, 1, cbs);
	
	UInt32 skinStride = 4; // For 1 bone, just the index
	if(GetBonesPerVertex() == 2)
		skinStride *= 4; // 2 indices, 2 weights
	else if(GetBonesPerVertex() == 4)
		skinStride *= 8;

	ID3D11Buffer* const vsStreams[] = { m_SourceVBO, m_SourceSkin };
	const UINT streamStrides[]={ GetStride(), skinStride };
	const UINT streamOffsets[] = { 0,0 };

	ctx->IASetVertexBuffers(0, 2, vsStreams, streamStrides, streamOffsets);

	// Call our own wrapper for DX11 input layout that does redundant layout change
	// tracking.
	extern void SetInputLayoutD3D11 (ID3D11DeviceContext* ctx, ID3D11InputLayout* layout);
	SetInputLayoutD3D11(ctx, sp->m_InputLayout);
	
	if (s_SkinningGlobals.m_DSState == 0)
	{
		D3D11_DEPTH_STENCIL_DESC dsstatedesc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
		dsstatedesc.DepthEnable = FALSE;
		dsstatedesc.StencilEnable = FALSE;
		GetD3D11Device()->CreateDepthStencilState(&dsstatedesc, &s_SkinningGlobals.m_DSState);
	}
	
	ID3D11DepthStencilState* bdsstate = 0;
	UINT bref;
	ctx->OMGetDepthStencilState(&bdsstate, &bref);
	ctx->OMSetDepthStencilState(s_SkinningGlobals.m_DSState, 0);

	ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
	ctx->Draw(GetVertexCount(), 0);

	ctx->OMSetDepthStencilState(bdsstate, bref);
	SAFE_RELEASE(bdsstate);

	if ( last )
	{
		ctx->VSSetShader( s_SkinningGlobals.m_OldVS, 0, 0 );
		ctx->GSSetShader( s_SkinningGlobals.m_OldGS, 0, 0 );
		ctx->PSSetShader( s_SkinningGlobals.m_OldPS, 0, 0 );
		ctx->IASetPrimitiveTopology(s_SkinningGlobals.m_OldTopo);
		ctx->VSSetConstantBuffers( 0, 1, &s_SkinningGlobals.m_OldCB );
		SAFE_RELEASE(s_SkinningGlobals.m_OldVS);
		SAFE_RELEASE(s_SkinningGlobals.m_OldGS);
		SAFE_RELEASE(s_SkinningGlobals.m_OldPS);
		SAFE_RELEASE(s_SkinningGlobals.m_OldCB);
		s_SkinningGlobals.m_BoundSP = 0;		
	}
	vbo->UnbindFromStreamOutput();
	//TODO
}

// ----------------------------------------------------------------------------
static ID3D11Buffer* UpdateStaticVBO(ID3D11Buffer* vbo, const size_t size, const void* data, bool dirty)
{
	D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	if (!vbo)
	{	
		GetD3D11Device()->CreateBuffer(&desc, 0, &vbo);
		dirty = true;
	}
	else 
	{
		D3D11_BUFFER_DESC curDesc;
		vbo->GetDesc(&curDesc);
		if (curDesc.ByteWidth != size)
		{
			vbo->Release();
			GetD3D11Device()->CreateBuffer(&desc, 0, &vbo);
			dirty = true;
		}
	}

	if (dirty)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		GetD3D11Context()->Map(vbo, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, data, size);
		GetD3D11Context()->Unmap(vbo, 0);
	}

	return vbo;
}

StreamOutSkinningInfo::~StreamOutSkinningInfo()
{
	if(m_SourceSkin)
		m_SourceSkin->Release();
	if(m_SourceVBO)
		m_SourceVBO->Release();
	if(m_SourceBones)
		m_SourceBones->Release();	
}

void StreamOutSkinningInfo::UpdateSourceData(const void *vertData, const BoneInfluence *skinData, bool dirty)
{
	// If only one bone, it's just one int per vertex
	UINT32 skinSize = GetVertexCount() * sizeof(UInt32);
	if(GetBonesPerVertex() == 2)
		skinSize *= 4; // 2 weights, 2 indices
	else if(GetBonesPerVertex() == 4)
		skinSize *= 8; // 4 weights, 4 indices

	m_SourceVBO = UpdateStaticVBO(m_SourceVBO, GetVertexCount() * GetStride(), vertData, dirty);
	m_SourceSkin = UpdateStaticVBO(m_SourceSkin, skinSize, skinData, dirty);
}


void StreamOutSkinningInfo::UpdateSourceBones(const int boneCount, const Matrix4x4f* cachedPose)
{
	Assert(boneCount > 0);
	
	UInt32 uBoneCount = std::min(1024, boneCount);
	uBoneCount = std::max(32, boneCount); // Clamp to 32 - 1024, as that's what our shaders expect.
	
	if (m_BoneCount < roundUpToNextPowerOf2(uBoneCount) && m_SourceBones)
	{
		// Bone count changed, resize the bone constant buffer
		m_SourceBones->Release();
		m_SourceBones = NULL;
	}
	m_BoneCount = roundUpToNextPowerOf2(uBoneCount);

	if (!m_SourceBones)
	{	
		const UInt32 bufSize = m_BoneCount * 48; // Must match the max bone count in internalshaders.hlsl
		D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(bufSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		GetD3D11Device()->CreateBuffer(&desc, 0, &m_SourceBones);
	}

	D3D11_MAPPED_SUBRESOURCE map;
	GetD3D11Context()->Map(m_SourceBones, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	UInt8* dest = (UInt8*)map.pData;

	for (int i = 0; i < uBoneCount; ++i)
	{
		Matrix4x4f mat = cachedPose[i];
		mat.Transpose();
		memcpy(dest + i * 48, &mat, 48);
	}
	
	GetD3D11Context()->Unmap(m_SourceBones, 0);
}

