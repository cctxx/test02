#include "UnityPrefix.h"
#include "VertexDeclarationsD3D11.h"
#include "D3D11Context.h"
#include "D3D11ByteCode.h"
#include "D3D11Utils.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Shaders/VBO.h"


extern ID3D11InputLayout* g_ActiveInputLayoutD3D11;


bool VertexDeclarationsD3D11::KeyType::operator < (const KeyType& rhs) const
{
	if (inputSig != rhs.inputSig)
		return inputSig < rhs.inputSig;
	if (extraBits != rhs.extraBits)
		return extraBits < rhs.extraBits;		
	return memcmp(channels, rhs.channels, sizeof(channels)) < 0;
}



VertexDeclarationsD3D11::VertexDeclarationsD3D11()
{
}

VertexDeclarationsD3D11::~VertexDeclarationsD3D11()
{
	Clear();
}

static D3D11_INPUT_ELEMENT_DESC kChannelVertexElems[kShaderChannelCount] = {
	// semantic name, semantic index, format, input slot, aligned byte offset, input slot class, instance data step rate
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
static const int kDefaultChannelSizes[kShaderChannelCount] = {
	12,	// position
	12,	// normal
	4,	// color
	8,	// uv
	8,	// uv2
	16,	// tangent
};

static const int kChannelSkinningCount = 2;
static D3D11_INPUT_ELEMENT_DESC kChannelSkinning4[kChannelSkinningCount] = {
	// Stream 1
	{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BLENDINDICES",  0, DXGI_FORMAT_R32G32B32A32_SINT,  1, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static D3D11_INPUT_ELEMENT_DESC kChannelSkinning2[kChannelSkinningCount] = {
	// Stream 1
	{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32_FLOAT, 1,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BLENDINDICES",  0, DXGI_FORMAT_R32G32_SINT,  1, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static D3D11_INPUT_ELEMENT_DESC kChannelSkinning1[kChannelSkinningCount] = {
	// Stream 1
	{ "BONEINDEX",  0, DXGI_FORMAT_R32_SINT,  1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static const D3D11_INPUT_ELEMENT_DESC kImmChannelVertexElems[] = {
	// semantic name, semantic index, format, input slot, aligned byte offset, input slot class, instance data step rate
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 3, DXGI_FORMAT_R32G32B32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 4, DXGI_FORMAT_R32G32B32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 5, DXGI_FORMAT_R32G32B32_FLOAT, 0, 88, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 6, DXGI_FORMAT_R32G32B32_FLOAT, 0,100, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 7, DXGI_FORMAT_R32G32B32_FLOAT, 0,112, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	// feed position as tangent0 data, just in case we use shaders that pretend to want tangents
	{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static FORCE_INLINE DXGI_FORMAT GetD3D11VertexDeclType(const ChannelInfo& info)
{
	switch (info.format)
	{
	case kChannelFormatFloat:
		{
			switch (info.dimension)
			{
			case 1: return DXGI_FORMAT_R32_FLOAT;
			case 2: return DXGI_FORMAT_R32G32_FLOAT;
			case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
			case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			break;
		}
	case kChannelFormatFloat16:
		{
			switch (info.dimension)
			{
			case 2: return DXGI_FORMAT_R16G16_FLOAT;
			case 4: return DXGI_FORMAT_R16G16B16A16_FLOAT;
			}
			break;
		}
	case kChannelFormatColor:
		{
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	}
	Assert("No matching D3D11 vertex decl type!");
	return DXGI_FORMAT_UNKNOWN;
}

ID3D11InputLayout* VertexDeclarationsD3D11::GetVertexDecl( UInt32 shaderChannelsMap, void* vertexShaderCode, unsigned vertexShaderLength, bool memExportSkin, unsigned int bonesPerVertex)
{
	ChannelInfoArray channels;
	int offset = 0;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		ChannelInfo& info = channels[i];
		if (shaderChannelsMap & (1 << i))
		{
			info.stream = 0;
			info.offset = offset;
			info.format = VBO::GetDefaultChannelFormat( i );
			info.dimension = VBO::GetDefaultChannelDimension( i );
			offset += VBO::GetDefaultChannelByteSize( i );
		}
		else
			info.Reset();
	}
	return GetVertexDecl( channels, GetShaderInputSignature(vertexShaderCode, vertexShaderLength), memExportSkin, bonesPerVertex );
}


ID3D11InputLayout* VertexDeclarationsD3D11::GetVertexDecl (const ChannelInfoArray& channels, const InputSignatureD3D11* inputSig, bool streamOutSkin, unsigned int bonesPerVertex)
{
	if (!inputSig)
	{
		AssertString("DX11 shader input signature is null");
		return NULL;
	}

	KeyType key;
	memcpy(key.channels, channels, sizeof(key.channels));
	key.extraBits = streamOutSkin ? bonesPerVertex : 0; // Set bones-per-vertex count for memExportSkin
	key.inputSig = inputSig;

	// already have vertex declaration for this format/shader?
	VertexDeclMap::iterator it = m_VertexDeclMap.find (key);
	if( it != m_VertexDeclMap.end() )
		return it->second;

	// don't have this declaration yet - create one
	D3D11_INPUT_ELEMENT_DESC elements[kShaderChannelCount + kChannelSkinningCount];
	int elIndex = 0;
	for( int chan = 0; chan < kShaderChannelCount; chan++ )
	{
		DebugAssert(elIndex < kShaderChannelCount);
		if (!channels[chan].IsValid() )
		{
			///@TODO: for now, hack in all shader channels to pretend to be there
			elements[elIndex] = kChannelVertexElems[chan];
			elements[elIndex].AlignedByteOffset = 0;
			++elIndex;
			continue;
		}
		elements[elIndex] = kChannelVertexElems[chan];
		elements[elIndex].InputSlot = channels[chan].stream;
		elements[elIndex].AlignedByteOffset = channels[chan].offset;
		elements[elIndex].Format = GetD3D11VertexDeclType(channels[chan]);
		++elIndex;
	}

	if (streamOutSkin) // Append extra elements required for streamout
	{
		switch(bonesPerVertex)
		{
		default:
		case 1:
			elements[elIndex] = kChannelSkinning1[0];
			++elIndex;
			break;
		case 2:
			for (int i = 0; i < kChannelSkinningCount; ++i)
			{
				elements[elIndex] = kChannelSkinning2[i];
				++elIndex;
			}
			break;
		case 4:
			for (int i = 0; i < kChannelSkinningCount; ++i)
			{
				elements[elIndex] = kChannelSkinning4[i];
				++elIndex;
			}
			break;
		}
	}
	
	ID3D11InputLayout* decl = NULL;
	HRESULT hr = GetD3D11Device()->CreateInputLayout (elements, elIndex, inputSig->blob.data(), inputSig->blob.size(), &decl );
	if( FAILED(hr) ) {
		AssertString ("Failed to create vertex declaration\n");
		// TODO: error!
	}
	SetDebugNameD3D11 (decl, Format("InputLayout-%d", elIndex));
	m_VertexDeclMap.insert( std::make_pair( key, decl ) );

	return decl;
}

ID3D11InputLayout* VertexDeclarationsD3D11::GetImmVertexDecl (const InputSignatureD3D11* inputSig)
{
	if (!inputSig)
	{
		AssertString("DX11 shader input signature is null");
		return NULL;
	}

	// already have vertex declaration for this shader?
	ImmVertexDeclMap::iterator it = m_ImmVertexDeclMap.find (inputSig);
	if (it != m_ImmVertexDeclMap.end())
		return it->second;

	// don't have this declaration yet - create one
	ID3D11InputLayout* decl = NULL;
	HRESULT hr = GetD3D11Device()->CreateInputLayout (kImmChannelVertexElems,ARRAY_SIZE(kImmChannelVertexElems), inputSig->blob.data(), inputSig->blob.size(), &decl);
	if (FAILED(hr))
	{
		AssertString ("Failed to create vertex declaration for GL.Begin\n");
		// TODO: error!
	}
	SetDebugNameD3D11 (decl, "InputLayoutImmediate");
	m_ImmVertexDeclMap.insert(std::make_pair(inputSig, decl));

	return decl;
}

void VertexDeclarationsD3D11::Clear()
{
	g_ActiveInputLayoutD3D11 = NULL;

	for (VertexDeclMap::iterator it = m_VertexDeclMap.begin(); it != m_VertexDeclMap.end(); ++it)
	{
		if (it->second) {
			ULONG refCount = it->second->Release();
			AssertIf( refCount != 0 );
		}
	}
	m_VertexDeclMap.clear();

	for (ImmVertexDeclMap::iterator it = m_ImmVertexDeclMap.begin(); it != m_ImmVertexDeclMap.end(); ++it)
	{
		if (it->second) {
			ULONG refCount = it->second->Release();
			AssertIf( refCount != 0 );
		}
	}
	m_ImmVertexDeclMap.clear();
}


const InputSignatureD3D11* VertexDeclarationsD3D11::GetShaderInputSignature (void* code, unsigned length)
{
	DXBCChunkHeader* isigChunk = dxbc_find_chunk (code, length, kFOURCC_ISGN);
	DebugAssert (isigChunk);		
	if (!isigChunk)
		return NULL;

	InputSignatureD3D11 isig;
	dxbc_create (&isigChunk, 1, isig.blob);

	InputSignatures::iterator it = m_InputSignatures.insert(isig).first;
	return &(*it);
}
