#pragma once

#include "D3D9Includes.h"
#include "Runtime\Filters\Mesh\VertexData.h"
#include <map>


class VertexDeclarations
{
public:
	VertexDeclarations();
	~VertexDeclarations();

	IDirect3DVertexDeclaration9* GetVertexDecl( const ChannelInfoArray channels );
	void Clear();

private:
	struct KeyType
	{
		bool operator < (const KeyType& rhs) const;
		ChannelInfoArray channels;
	};

	typedef UNITY_MAP(kMemVertexData, KeyType, IDirect3DVertexDeclaration9*) VertexDeclMap;
	VertexDeclMap m_VertexDeclMap;
};
