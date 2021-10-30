#ifndef __CHANNELASSIGNS_H__
#define __CHANNELASSIGNS_H__

#include <string>
#include "GfxDeviceTypes.h"

namespace ShaderLab {
struct ParserBindChannels;
}

// Tracks which vertex components should be sourced from which shader channels
// TODO: It gets serialized a lot for multithreading, can we make it more compact?
class ChannelAssigns {
public:
	ChannelAssigns();
	void FromParsedChannels (const ShaderLab::ParserBindChannels& parsed); // ShaderParser.cpp

	void Bind( ShaderChannel source, VertexComponent target );
	void Unbind( VertexComponent target );
	void MergeWith( const ChannelAssigns& additional );

	UInt32 GetTargetMap() const { return m_TargetMap; }
	UInt32 GetSourceMap() const { return m_SourceMap; }
	bool IsEmpty() const { return m_TargetMap == 0; }

	// if and only if all source channels directly map to target components
	//   src.Vertex -> dst.Vertex, src.Normal -> dst.Normal, etc
	//   there is NO cross-bar connections like: src.TexCoord0 -> dst.TexCoord1
	bool IsDirectlyWired() const { return m_DirectlyWired; }

	ShaderChannel GetSourceForTarget( VertexComponent target ) const { return ShaderChannel(m_Channels[target]); }

	bool operator== (const ChannelAssigns& other) const;

private:
	void RecalculateIsDirectlyWired ();

private:
	UInt32	m_TargetMap; // bitfield of which vertex components are sourced
	UInt32	m_SourceMap; // bitfield of which source channels are used
	SInt8	m_Channels[kVertexCompCount]; // for each vertex component: from which channel it is sourced
	bool	m_DirectlyWired;

	// Friends for serialization
	friend struct GfxRet_ChannelAssigns;
};


ShaderChannel GetShaderChannelFromName( const std::string& name );


#endif
