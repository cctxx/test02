#include "UnityPrefix.h"
#include "ChannelAssigns.h"
//#include "ShaderLabErrors.h"

// Channel names (strings for reading from ShaderLab)
static const char * const kShaderChannelName[kShaderChannelCount] = {
	"VERTEX",
	"NORMAL",
	"COLOR",
	"TEXCOORD",
	"TEXCOORD1",
	"TANGENT",
};


ShaderChannel GetShaderChannelFromName( const std::string& name )
{
	std::string nameUpper = ToUpper(name);
	for( int i = 0; i < kShaderChannelCount; ++i )
		if( kShaderChannelName[i] == nameUpper )
			return (ShaderChannel)i;

	return kShaderChannelNone;
}
	


ChannelAssigns::ChannelAssigns()
:	m_TargetMap(0)
,	m_SourceMap(0)
,	m_DirectlyWired(true)
{
	for( int i = 0; i < kVertexCompCount; ++i )
		m_Channels[i] = kShaderChannelNone;
}

void ChannelAssigns::MergeWith( const ChannelAssigns& additional )
{
	for( int i = 0; i < kVertexCompCount; ++i )
	{
		ShaderChannel source = additional.GetSourceForTarget(VertexComponent(i));
		if( source != kShaderChannelNone )
			Bind( source, (VertexComponent)i );
	}
}

static bool IsChannelDirectlyWired (ShaderChannel source, VertexComponent target)
{
	switch (source)
	{
		case kShaderChannelVertex:
			return (target == kVertexCompVertex);
		case kShaderChannelNormal:
			return (target == kVertexCompNormal);
		case kShaderChannelColor:
			return (target == kVertexCompColor);
		case kShaderChannelTexCoord0:
			return (target == kVertexCompTexCoord0);
		case kShaderChannelTexCoord1:
			return (target == kVertexCompTexCoord1);
		case kShaderChannelTangent:
			return (target == kVertexCompTexCoord2);
		default:
			break;
	}
	return false;
}
	
void ChannelAssigns::Bind (ShaderChannel source, VertexComponent target)
{
	AssertIf( source == kShaderChannelNone );
	// TODO: skip kShaderChannelTexCoord ones here?
	// TODO: filter duplicates somehow?
	m_Channels[target] = source;
	m_TargetMap |= (1<<target);
	m_SourceMap |= (1<<source);
	
	if (m_DirectlyWired)
		m_DirectlyWired = IsChannelDirectlyWired (source, target);
}

void ChannelAssigns::Unbind( VertexComponent target )
{
	m_TargetMap &= ~(1<<target);
	m_Channels[target] = kShaderChannelNone;

	RecalculateIsDirectlyWired ();
}

void ChannelAssigns::RecalculateIsDirectlyWired ()
{
	m_DirectlyWired = true;
	for (int i = 0; i < kVertexCompCount && m_DirectlyWired; ++i)
	{
		ShaderChannel source = GetSourceForTarget(VertexComponent(i));
		if( source != kShaderChannelNone )
			m_DirectlyWired &= IsChannelDirectlyWired (source, (VertexComponent)i);
	}
}

bool ChannelAssigns::operator== (const ChannelAssigns& other) const
{
	return m_SourceMap == other.m_SourceMap &&
		   m_TargetMap == other.m_TargetMap &&
		   memcmp(&m_Channels[0], &other.m_Channels[0], sizeof(m_Channels)) == 0;
}


#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "External/shaderlab/Library/SLParserData.h"
SUITE (ChanelAssignsTest)
{
TEST(IsDirectlyWired)
{
	ChannelAssigns ch;
	// usual binding
	ch.Bind(kShaderChannelVertex, kVertexCompVertex);
	CHECK(ch.IsDirectlyWired());
	ch.Bind(kShaderChannelNormal, kVertexCompNormal);
	CHECK(ch.IsDirectlyWired());
	ch.Bind(kShaderChannelColor, kVertexCompColor);
	CHECK(ch.IsDirectlyWired());
	ch.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord0);
	CHECK(ch.IsDirectlyWired());
	ch.Bind(kShaderChannelTexCoord1, kVertexCompTexCoord1);
	CHECK(ch.IsDirectlyWired());
	ch.Bind(kShaderChannelTangent, kVertexCompTexCoord2);		
	CHECK(ch.IsDirectlyWired());

	// bind twice
	ch.Bind(kShaderChannelVertex, kVertexCompVertex);
	ch.Bind(kShaderChannelNormal, kVertexCompNormal);
	ch.Bind(kShaderChannelColor, kVertexCompColor);
	ch.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord0);
	ch.Bind(kShaderChannelTexCoord1, kVertexCompTexCoord1);
	ch.Bind(kShaderChannelTangent, kVertexCompTexCoord2);		
	CHECK(ch.IsDirectlyWired());

	// cross-bar
	ch.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord2);
	CHECK(!ch.IsDirectlyWired());

	// unbinding cross-bar
	ch.Unbind(kVertexCompTexCoord2);
	CHECK(ch.IsDirectlyWired());

	// rebinding
	ch.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord0);
	ch.Bind(kShaderChannelTexCoord1, kVertexCompTexCoord1);
	ch.Bind(kShaderChannelTangent, kVertexCompTexCoord2);		
	CHECK(ch.IsDirectlyWired());

	// cross-bar 2
	ChannelAssigns ch2 = ch;
	ch2.Bind(kShaderChannelVertex, kVertexCompNormal);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelNormal, kVertexCompColor);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelColor, kVertexCompTexCoord0);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord2);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelTexCoord1, kVertexCompVertex);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelTangent, kVertexCompAttrib0);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord1);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord2);
	CHECK(!ch2.IsDirectlyWired());
	ch2 = ch;
	ch2.Bind(kShaderChannelTexCoord0, kVertexCompColor);
	CHECK(!ch2.IsDirectlyWired());

	// unbinding all
	ch.Unbind(kVertexCompVertex);
	ch.Unbind(kVertexCompNormal);
	ch.Unbind(kVertexCompColor);
	ch.Unbind(kVertexCompTexCoord0);
	ch.Unbind(kVertexCompTexCoord1);
	ch.Unbind(kVertexCompTexCoord2);		
	CHECK(ch.IsDirectlyWired());

	// check empty/1channel
	ch.Bind(kShaderChannelVertex, kVertexCompTexCoord0);
	CHECK(!ch.IsDirectlyWired());

	ch.Unbind(kVertexCompTexCoord0);
	CHECK(ch.IsDirectlyWired());
	CHECK(ch.IsEmpty());

	ch.Bind(kShaderChannelVertex, kVertexCompVertex);
	CHECK(ch.IsDirectlyWired());
}
TEST(IsDirectlyWiredXBar)
{
	ChannelAssigns ch;
	// usual binding
	ch.Bind(kShaderChannelVertex, kVertexCompVertex);
	ch.Bind(kShaderChannelNormal, kVertexCompNormal);
	CHECK(ch.IsDirectlyWired());
	ch.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord1);
	ch.Bind(kShaderChannelTexCoord1, kVertexCompTexCoord0);
	CHECK(!ch.IsDirectlyWired());
}
TEST(IsDirectlyWiredUpdatedWhenCreatingFromParserChannels)
{
	ShaderLab::ParserBindChannels pchn;
	pchn.Bind(kShaderChannelTexCoord0, kVertexCompTexCoord1, false, NULL);
	pchn.Bind(kShaderChannelTexCoord1, kVertexCompTexCoord0, false, NULL);
	ChannelAssigns ch;
	ch.FromParsedChannels (pchn);
	CHECK(!ch.IsDirectlyWired());
}
}

#endif
