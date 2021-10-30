#include "UnityPrefix.h"
#include "ShaderNameRegistry.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "External/shaderlab/Library/shaderlab.h"


ScriptMapper::ScriptMapper (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Shaders.SetObjectToDirty (this);
}

ScriptMapper::~ScriptMapper ()
{
}


void ScriptMapper::AddShader (Shader& ptr)
{
	m_Shaders.Add (ptr.GetScriptClassName (), &ptr);
}

void ScriptMapper::AddBuiltinShader (const string& shaderClassName, PPtr<Shader> ptr)
{
	m_Shaders.Add (shaderClassName, ptr);
}


static const char* kBackwardsCompatShaderNames[] = {
	" VertexLit",					"VertexLit",
	" Diffuse",						"Diffuse",
	" Glossy",						"Specular",
	" Bumped",						"Bumped Diffuse",
	" BumpedSpecular",				"Bumped Specular",
	"ParallaxBump/Diffuse",			"Parallax Diffuse",
	"ParallaxBump/Specular",		"Parallax Specular",
	
	"Alpha/VertexLit",				"Transparent/VertexLit",
	"Alpha/Diffuse",				"Transparent/Diffuse",
	"Alpha/Glossy",					"Transparent/Specular",
	"Alpha/Bumped",					"Transparent/Bumped Diffuse",
	"Alpha/BumpedSpecular",			"Transparent/Bumped Specular",
	"ParallaxBump/AlphaDiffuse",	"Transparent/Parallax Diffuse",
	"ParallaxBump/AlphaSpecular",	"Transparent/Parallax Specular",
	
	//"Reflective/VertexLit",		"Reflective/VertexLit",
	//"Reflective/Diffuse",			"Reflective/Diffuse",
	"Reflective/Glossy",			"Reflective/Specular",
	"Reflective/Bumped",			"Reflective/Bumped Diffuse",
	"Reflective/BumpedSpecular",	"Reflective/Bumped Specular",
	"ParallaxBump/ReflectDiffuse",	"Reflective/Parallax Diffuse",
	"ParallaxBump/ReflectSpecular",	"Reflective/Parallax Specular",
	
	"Lightmapped/Glossy",			"Legacy Shaders/Lightmapped/Specular",
	"Lightmapped/Bumped",			"Legacy Shaders/Lightmapped/Bumped Diffuse",
	"Lightmapped/BumpedSpecular",	"Legacy Shaders/Lightmapped/Bumped Specular",
	"Lightmapped/VertexLit",		"Legacy Shaders/Lightmapped/VertexLit",
	"Lightmapped/Diffuse",			"Legacy Shaders/Lightmapped/Diffuse",
	"Lightmapped/Specular",			"Legacy Shaders/Lightmapped/Specular",
	"Lightmapped/Bumped Diffuse",	"Legacy Shaders/Lightmapped/Bumped Diffuse",
	"Lightmapped/Bumped Specular",	"Legacy Shaders/Lightmapped/Bumped Specular",
	"Diffuse Fast",					"Legacy Shaders/Diffuse Fast",
	
	
	//"Self-Illumin/VertexLit",		"Self-Illumin/VertexLit",
	//"Self-Illumin/Diffuse",		"Self-Illumin/Diffuse",
	"Self-Illumin/Glossy",			"Self-Illumin/Specular",
	"Self-Illumin/Bumped",			"Self-Illumin/Bumped Diffuse",
	"Self-Illumin/BumpedSpecular",	"Self-Illumin/Bumped Specular",
	"ParallaxBump/IlluminDiffuse",	"Self-Illumin/Parallax Diffuse",
	"ParallaxBump/IlluminSpecular",	"Self-Illumin/Parallax Specular",
	
	" DiffuseDetail",				"Diffuse Detail",
	" Diffuse (fast)",				"Legacy Shaders/Diffuse Fast",
	" Decal",						"Decal",
	
	"Hidden/TerrainEngine/Splatmap/Lightmap-FirstPass", "Nature/Terrain/Diffuse",
};


static const char* GetNonLegacyShaderName(const std::string& name)
{
	const int kShaderNameCount = sizeof(kBackwardsCompatShaderNames) / sizeof(kBackwardsCompatShaderNames[0]) / 2;
	for (int i = 0; i < kShaderNameCount; ++i)
	{
		if (strcmp( kBackwardsCompatShaderNames[i*2+0], name.c_str()) == 0)
			return kBackwardsCompatShaderNames[i*2+1];
	}
	return NULL;
}


Shader *ScriptMapper::FindShader (const string &name)
{
	Shader* result = m_Shaders.Find (name);
	if( result != NULL )
		return result;
	
	// If shader is not found, try old names (before 2.0) of built-in shaders
	const char* otherName = GetNonLegacyShaderName(name);
	if (otherName)
		return m_Shaders.Find(otherName);
	
	return NULL;
}



template<class TransferFunction>
void ScriptMapper::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	AssertIf (transfer.GetFlags() & kPerformUnloadDependencyTracking);
	transfer.Transfer (m_Shaders, "m_Shaders");
}

bool ScriptMapper::ShouldIgnoreInGarbageDependencyTracking ()
{	
	return true;
}
IMPLEMENT_CLASS (ScriptMapper)
IMPLEMENT_OBJECT_SERIALIZE (ScriptMapper)
GET_MANAGER (ScriptMapper)
GET_MANAGER_PTR(ScriptMapper)


static void ParseShaderIfNeeded (Shader* shader)
{
	Assert (!shader->GetNeedsParsing() || !shader->GetShaderLabShader());
	if (!shader->GetNeedsParsing())
		return;

	const Shader::ScriptString& script = shader->GetScript();
	// Save current shader name around shader parsing
	std::string saveShaderName = g_LastParsedShaderName;
	shader->ParseAndPostLoad (script.c_str(), script.size());
	g_LastParsedShaderName = saveShaderName;
}


Shader* FindShaderLabShader (const std::string& name, ShaderPtrVector& shaderLookup, bool useLookup)
{
	Assert(GetScriptMapperPtr());

	// Finding shaders can load new ones. So save and
	// restore the currently parsed shader name.
	std::string saveShaderName = g_LastParsedShaderName;

	Shader* shader = NULL;
	if (useLookup)
	{
		for (size_t i = 0; i < shaderLookup.size(); ++i)
		{
			Shader* s = shaderLookup[i];
			if (s == NULL)
				continue;
			ParseShaderIfNeeded (s);
			const std::string& shaderName = s->GetScriptClassName();
			if (shaderName == name)
			{
				shader = s;
				break;
			}
			// also try non-legacy name of the shader we're searching for,
			// in case someone does " Diffuse" but we're meant
			// to find "Diffuse" etc.
			const char* nonLegacyShaderName = GetNonLegacyShaderName(name.c_str());
			if (nonLegacyShaderName && !strcmp(shaderName.c_str(), nonLegacyShaderName))
			{
				shader = s;
				break;
			}
		}
	}
	else
	{
		shader = GetScriptMapper().FindShader (name);
	}

	g_LastParsedShaderName = saveShaderName;


	if (shader)
	{
		ParseShaderIfNeeded (shader);

		if (!useLookup)
		{
			PPtr<Shader> shaderPtr(shader);
			if (std::find(shaderLookup.begin(), shaderLookup.end(), shaderPtr) == shaderLookup.end())
				shaderLookup.push_back (shaderPtr);
		}

		return shader;
	}
	
	return NULL;
}



// -------------------------------------------------------------------



#if ENABLE_UNIT_TESTS

#include "External/shaderlab/Library/SLParserData.h"


static void TestCanFindLegacyShaderNamesWhenUsingLookup ()
{
	const char* oldName = "Lightmapped/VertexLit"; // legacy name
	ScriptMapper& sm = GetScriptMapper();
	Shader* shader1 = sm.FindShader(oldName);
	Assert (shader1 != NULL);

	// now check if FindShaderLabShader can find it when using lookup table
	ShaderPtrVector lookup;
	lookup.push_back(shader1);
	Shader* shader2 = FindShaderLabShader (oldName, lookup, true);
	Assert (shader2 == shader1);
}


void TestHighLevelShaderNameRegistryTests()
{
	TestCanFindLegacyShaderNamesWhenUsingLookup();
}

#endif // #if ENABLE_UNIT_TESTS
