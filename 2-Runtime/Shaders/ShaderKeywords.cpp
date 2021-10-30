#include "UnityPrefix.h"
#include "ShaderKeywords.h"
#include <map>

typedef std::map<std::string, ShaderKeyword> KeywordMap;

static KeywordMap* s_KeywordMap;
ShaderKeywordSet g_ShaderKeywords;


void keywords::Initialize()
{
	Assert(!s_KeywordMap);
	s_KeywordMap = UNITY_NEW( KeywordMap, kMemShader);
	SET_ALLOC_OWNER(NULL);
	// precreate common keywords
	
	// Important: these must go first
	Create( "SPOT" );
	Create( "DIRECTIONAL" );
	Create( "DIRECTIONAL_COOKIE" );
	Create( "POINT" );
	Create( "POINT_COOKIE" );
	
	Create( "SHADOWS_OFF" );
	Create( "SHADOWS_DEPTH" );
	Create( "SHADOWS_SCREEN" );
	Create( "SHADOWS_CUBE" );
	Create( "SHADOWS_SOFT" );
	Create( "SHADOWS_SPLIT_SPHERES" );
	Create( "SHADOWS_NATIVE" );
	
	Create( "LIGHTMAP_OFF" );
	Create( "LIGHTMAP_ON" );
	
	Create( "DIRLIGHTMAP_OFF" );
	Create( "DIRLIGHTMAP_ON" );

	Create( "VERTEXLIGHT_ON" );
	Create( "SOFTPARTICLES_ON" );
	
	Create( "HDR_LIGHT_PREPASS_ON" );
}

void keywords::Cleanup()
{
	UNITY_DELETE( s_KeywordMap, kMemShader);
	s_KeywordMap = NULL;
}

ShaderKeyword keywords::Create( const std::string& name )
{
	if( !s_KeywordMap ) {
		Initialize();
		ANALYSIS_ASSUME(s_KeywordMap);
	}
	
	KeywordMap::const_iterator it = s_KeywordMap->find( name );
	if( it != s_KeywordMap->end() )
		return it->second;
	
	ShaderKeyword key = s_KeywordMap->size();
	if( key >= kMaxShaderKeywords )
	{
		AssertString( Format("Maximum number (%i) of shader keywords exceeded, keyword %s will be ignored", kMaxShaderKeywords, name.c_str()) );
	}
	else
	{
		SET_ALLOC_OWNER(NULL);
		s_KeywordMap->insert( std::make_pair( name, key ) );
	}
	return key;
}

#if UNITY_EDITOR
std::string keywords::GetKeywordName (ShaderKeyword k)
{
	if (!s_KeywordMap) {
		Initialize();
		ANALYSIS_ASSUME(s_KeywordMap);
	}
	for (KeywordMap::const_iterator it = s_KeywordMap->begin(); it != s_KeywordMap->end(); ++it)
	{
		if (it->second == k)
			return it->first;
	}
	AssertString ("Requesting non existing shader keyword");
	return "";
}
#endif
