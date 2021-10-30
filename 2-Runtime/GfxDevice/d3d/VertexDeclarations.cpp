#include "UnityPrefix.h"
#include "VertexDeclarations.h"
#include "D3D9Context.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

bool VertexDeclarations::KeyType::operator < (const KeyType& rhs) const
{
	return memcmp(channels, rhs.channels, sizeof(channels)) < 0;
}

VertexDeclarations::VertexDeclarations()
{
}

VertexDeclarations::~VertexDeclarations()
{
	Clear();
}

struct D3DVertexSemantics
{
	UInt8 usage;
	UInt8 index;
};

static D3DVertexSemantics kChannelVertexSemantics[kShaderChannelCount] =
{
	{ D3DDECLUSAGE_POSITION, 0 }, // position
	{ D3DDECLUSAGE_NORMAL,   0 }, // normal
	{ D3DDECLUSAGE_COLOR,    0 }, // color
	{ D3DDECLUSAGE_TEXCOORD, 0 }, // uv
	{ D3DDECLUSAGE_TEXCOORD, 1 }, // uv2
	{ D3DDECLUSAGE_TANGENT,  0 }, // tangent
};

static FORCE_INLINE D3DDECLTYPE GetD3DVertexDeclType(const ChannelInfo& info)
{
	switch (info.format)
	{
		case kChannelFormatFloat:
		{
			switch (info.dimension)
			{
				case 1: return D3DDECLTYPE_FLOAT1;
				case 2: return D3DDECLTYPE_FLOAT2;
				case 3: return D3DDECLTYPE_FLOAT3;
				case 4: return D3DDECLTYPE_FLOAT4;
			}
			break;
		}
		case kChannelFormatFloat16:
		{
			switch (info.dimension)
			{
				case 2: return D3DDECLTYPE_FLOAT16_2;
				case 4: return D3DDECLTYPE_FLOAT16_4;
			}
			break;
		}
		case kChannelFormatColor:
		{
			return D3DDECLTYPE_D3DCOLOR;
		}
	}
	Assert("No matching D3D vertex decl type!");
	return D3DDECLTYPE_UNUSED;
}

IDirect3DVertexDeclaration9* VertexDeclarations::GetVertexDecl( const ChannelInfoArray channels )
{
	KeyType key;
	memcpy(key.channels, channels, sizeof(key.channels));

	// already have vertex declaration for these formats?
	VertexDeclMap::iterator it = m_VertexDeclMap.find( key );
	if( it != m_VertexDeclMap.end() )
		return it->second;

	// don't have this declaration yet - create one
	// KD: not sure if elements need to be ordered by stream, playing it safe
	D3DVERTEXELEMENT9 elements[kShaderChannelCount+1];
	int elIndex = 0;
	for( int stream = 0; stream < kMaxVertexStreams; stream++ )
	{
		for( int chan = 0; chan < kShaderChannelCount; chan++ )
		{
			if( channels[chan].stream == stream && channels[chan].IsValid() )
			{
				DebugAssert(elIndex < kShaderChannelCount);
				D3DVERTEXELEMENT9& elem = elements[elIndex];
				elem.Stream = stream;
				elem.Offset = channels[chan].offset;
				elem.Type = GetD3DVertexDeclType(channels[chan]);
				elem.Method = D3DDECLMETHOD_DEFAULT;
				elem.Usage = kChannelVertexSemantics[chan].usage;
				elem.UsageIndex = kChannelVertexSemantics[chan].index;
				++elIndex;
			}
		}
	}
	D3DVERTEXELEMENT9 declEnd = D3DDECL_END();
	elements[elIndex] = declEnd;

	IDirect3DVertexDeclaration9* decl = NULL;
	HRESULT hr = GetD3DDevice()->CreateVertexDeclaration( elements, &decl );
	if( FAILED(hr) ) {
		// TODO: error!
	}
	m_VertexDeclMap[key] = decl;
	return decl;
}

void VertexDeclarations::Clear()
{
	VertexDeclMap::iterator it;
	for( it = m_VertexDeclMap.begin(); it != m_VertexDeclMap.end(); ++it )
	{
		if( it->second ) {
			ULONG refCount = it->second->Release();
			AssertIf( refCount != 0 );
		}
	}
	m_VertexDeclMap.clear();
}
