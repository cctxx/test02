#pragma once

#include "D3D11Includes.h"
#include "Runtime/Filters/Mesh/VertexData.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <map>

struct InputSignatureD3D11
{
	dynamic_array<UInt8> blob;
	bool operator < (const InputSignatureD3D11& o) const
	{
		size_t sizeA = blob.size();
		size_t sizeB = o.blob.size();
		if (sizeA != sizeB)
			return sizeA < sizeB;
		int res = memcmp (blob.data(), o.blob.data(), sizeA);
		return res < 0;
	}
};

class VertexDeclarationsD3D11
{
public:
	VertexDeclarationsD3D11();
	~VertexDeclarationsD3D11();

	ID3D11InputLayout* GetVertexDecl( UInt32 shaderChannelsMap, void* vertexShaderCode, unsigned vertexShaderLength, bool streamOutSkin = false, unsigned int bonesPerVertex = 4 );
	ID3D11InputLayout* GetVertexDecl (const ChannelInfoArray& channels, const InputSignatureD3D11* inputSig, bool streamOutSkin = false, unsigned int bonesPerVertex = 4);
	ID3D11InputLayout* GetImmVertexDecl (const InputSignatureD3D11* inputSig);
	void Clear();

	const InputSignatureD3D11* GetShaderInputSignature (void* code, unsigned length);

private:
	typedef std::set<InputSignatureD3D11> InputSignatures;
	InputSignatures	m_InputSignatures;

	struct KeyType
	{
		bool operator < (const KeyType& rhs) const;
		ChannelInfoArray channels;
		const InputSignatureD3D11* inputSig;
		UInt32 extraBits;
	};
	typedef UNITY_MAP(kMemVertexData, KeyType, ID3D11InputLayout*) VertexDeclMap;
	VertexDeclMap m_VertexDeclMap;

	typedef UNITY_MAP(kMemVertexData, const InputSignatureD3D11*, ID3D11InputLayout*) ImmVertexDeclMap;
	ImmVertexDeclMap m_ImmVertexDeclMap;
};
