#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Shaders/GraphicsCaps.h"

#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/ShaderParser.h"
#include "External/shaderlab/Library/SLParserData.h"
#include "External/shaderlab/Library/ShaderParserInterm.h"
#include "External/shaderlab/Library/ShaderParser.h"

#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Utilities/Word.h"

using namespace ShaderLab;

static void CheckPassAffectingProps_TestHelper (const char* desc, const ShaderLab::Pass* pass, const char* propName, ...)
{
	const PropertyNamesSet& names = pass->GetAffectingProps();

	vector_set<int> namesFound;
	bool error = false;

	// See if properties we expect do in fact affect the pass
	const char* currName = propName;
	va_list ap;
	va_start (ap, propName);
	while (currName != NULL)
	{
		ShaderLab::FastPropertyName currProp = ShaderLab::Property(currName);
		if (names.names.find(currProp.index) == names.names.end())
		{
			printf_console("ShaderTests '%s': property %s expected to affect it, but it doesn't!\n", desc, currName);
			error = true;
		}
		else
			namesFound.insert(currProp.index);
		currName = va_arg (ap, const char*);
	};
	va_end (ap);

	// Now print all properties that are affecting the pass, but we didn't expect them to
	for (vector_set<int>::const_iterator it = names.names.begin(); it != names.names.end(); ++it)
	{
		if (namesFound.find(*it) == namesFound.end())
		{
			ShaderLab::FastPropertyName prop;
			prop.index = *it;
			printf_console("Shader test %s: property %s is affecting shader, but is not expected to!\n", desc, prop.GetName());
			error = true;
		}
	}

	// If we had errors, print out all properties that are affecting the pass
	if (error)
	{
		std::string msg = "..shader test failed, properties that are affecting: ";
		for (vector_set<int>::const_iterator it = names.names.begin(); it != names.names.end(); ++it)
		{
			ShaderLab::FastPropertyName name;
			name.index = *it;
			msg += name.GetName();
			msg += " ";
		}
		printf_console("%s\n\n", msg.c_str());
		printf_console ("ShaderTests: Shader Pass affecting properties test failed, check output log");
		CHECK (false);
	}
}

struct ParseTestFixture
{
	ShaderErrors errors;
	bool ParseTest (const char* text)
	{
		errors.Clear();
		ShaderLab::ParserShader* parsedForm = PreParseShader (text, strlen(text), kGfxRendererNull, errors);
		if (!parsedForm)
			return false;
		UNITY_DELETE(parsedForm,kMemShader);
		return true;
	}
};

SUITE (ShaderTests)
{
#if UNITY_EDITOR

	TEST (Shader_ShadowCollectorPassCorrectZWrite_EditorOnly)
	{
		if (GetGfxDevice().GetRenderer() == kGfxRendererNull)
			return;

		// Ensure the presence of the 'VertexLit' shader.
		Shader* shader = GetScriptMapper().FindShader ("VertexLit");
		CHECK (shader != NULL);

		// 'VertexLit' shader should always have shadow collector pass.
		dynamic_array<ShaderLab::Pass*> collectorPasses;
		shader->GetShaderLabShader()->GetActiveSubShader().FindNamedPasses ("SHADOWCOLLECTOR", collectorPasses);
		CHECK (!collectorPasses.empty());

		// shadow collector should use LEqual depth test!
		const DeviceDepthState* depthState = collectorPasses[0]->GetState().GetDepthState();
		CHECK (depthState);
		CHECK (depthState->sourceState.depthFunc == kFuncLEqual);

		// FindNamedPasses calls Retain(), so do not leak memory: need to release them
		for (int i = 0; i < collectorPasses.size(); ++i)
			collectorPasses[i]->Release();
	}

	TEST (Shader_RenderQueueRespectsLOD_EditorOnly)
	{
		// two subshaders with different LOD, using different render queues
		const char* kShaderText =
			"Shader \"__Foo\" {\n"
			"SubShader { LOD 300 Tags {Queue=Geometry} Pass { } }\n"
			"SubShader { LOD 100 Tags {Queue=Transparent} Pass { } }\n"
			"}\n";
		ShaderLab::ParserShader* parsedForm = NULL;
		ShaderErrors errors;
		ShaderPtrVector dependencies;
		ShaderLab::IntShader* shader = ParseShader(kShaderText, strlen(kShaderText), dependencies, false, &parsedForm, errors, kGfxRendererD3D9);
		CHECK (shader && !errors.HasErrorsOrWarnings());
		CHECK (shader->GetSubShaders().size() == 2);

		// uses first subshader, Geometry queue
		CHECK (shader->GetRenderQueue() == 2000);

		// set LOD to not use the first subshader
		shader->SetMaximumShaderLOD(200);
		// should get queue from the 2nd subshader
		CHECK (shader->GetRenderQueue() == 3000);

		UNITY_DELETE( parsedForm, kMemShader);
		UNITY_DELETE( shader, kMemShader);
	}

	TEST (Shader_DependenciesDontHaveDuplicates_EditorOnly)
	{
		// two subshaders with different LOD, using different render queues
		const char* kShaderText =
			"Shader \"__Foo\" {\n"
			"SubShader {\n"
			"UsePass \"VertexLit/SHADOWCASTER\"\n"
			"UsePass \"VertexLit/SHADOWCASTER\"\n"
			"UsePass \"VertexLit/SHADOWCASTER\"\n"
			"}\n"
			"Fallback Off\n"
			"}\n";
		ShaderLab::ParserShader* parsedForm = NULL;
		ShaderErrors errors;
		ShaderPtrVector dependencies;
		ShaderLab::IntShader* shader = ParseShader(kShaderText, strlen(kShaderText), dependencies, false, &parsedForm, errors, kGfxRendererD3D9);
		CHECK (shader && !errors.HasErrorsOrWarnings());
		CHECK (shader->GetSubShaders().size() == 1);

		CHECK (dependencies.size() == 1);

		UNITY_DELETE (parsedForm, kMemShader);
		UNITY_DELETE (shader, kMemShader);
	}

	static void UsedVertexComponents_TestHelper (const char* name, UInt32 expected, UInt32 expectedLM)
	{
		Shader* shader = GetScriptMapper().FindShader (name);
		Assert (shader);
		UInt32 channels = shader->CalculateUsedVertexComponents(false);
		Assert (channels == expected);
		channels = shader->CalculateUsedVertexComponents(true);
		Assert (channels == expectedLM);
	}

	TEST (Shader_UsedVertexComponents_EditorOnly)
	{
		if (gGraphicsCaps.GetEmulation() != GraphicsCaps::kEmulNone ||
			GetGfxDevice().GetRenderer() == kGfxRendererNull ||
			GetGfxDevice().GetRenderer() == kGfxRendererD3D11)
			return;

		UInt32 expLightingUV = (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0);
		UInt32 expLightingUV2 = (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelTexCoord1);
		UInt32 expLightingTanUV = (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelTangent);
		UInt32 expLightingTanUV2 = (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelTexCoord1) | (1<<kShaderChannelTangent);
		UInt32 expUV1 = (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0);
		UInt32 expColorUV1 = (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor);

		UsedVertexComponents_TestHelper ("VertexLit", expLightingUV, expLightingUV2);
		UsedVertexComponents_TestHelper ("Diffuse", expLightingUV, expLightingUV2);
		UsedVertexComponents_TestHelper ("Bumped Diffuse", expLightingTanUV, expLightingTanUV2);
		UsedVertexComponents_TestHelper ("Unlit/Texture", expUV1, expUV1);
		UsedVertexComponents_TestHelper ("Particles/Additive", expColorUV1, expColorUV1);
	}

	TEST (Shader_PassAffectingPropsWork_EditorOnly)
	{
		if (GetGfxDevice().GetRenderer() == kGfxRendererNull || gGraphicsCaps.shaderCaps < kShaderLevel2)
			return;

		Shader* shSpec = GetBuiltinExtraResource<Shader>("Normal-Glossy.shader");
		CHECK (shSpec != NULL);

		CheckPassAffectingProps_TestHelper("Specular ForwardBase pass", shSpec->GetShaderLabShader()->GetSubShader(0).GetPass(0), "_MainTex", "_Color", "_Shininess", "_SpecColor", "_MainTex_ST", "_ShadowMapTexture", NULL);
		CheckPassAffectingProps_TestHelper("Specular ForwardAdd pass",  shSpec->GetShaderLabShader()->GetSubShader(0).GetPass(1), "_MainTex", "_Color", "_Shininess", "_SpecColor", "_MainTex_ST", "_LightTexture0", "_LightTextureB0", NULL);

		// None of user-accessible properties are affecting shadow caster pass
		CheckPassAffectingProps_TestHelper("Specular ShadowCaster pass", shSpec->GetShadowCasterPass(), NULL);

		// Specular falls back to VertexLit, so subshader #1 is vertex lit
		CheckPassAffectingProps_TestHelper("VertexLit vertex pass", shSpec->GetShaderLabShader()->GetSubShader(1).GetPass(0), "_MainTex", "_Color", "_Shininess", "_SpecColor", "_Emission", NULL);

		Shader* shCutoutBSpec = GetBuiltinExtraResource<Shader>("AlphaTest-Bumped.shader");
		CHECK (shCutoutBSpec !=  NULL);

		CheckPassAffectingProps_TestHelper("Cutout BumpSpec ForwardBase pass", shCutoutBSpec->GetShaderLabShader()->GetSubShader(0).GetPass(0), "_MainTex", "_Cutoff", "_BumpMap", "_Color", "_MainTex_ST", "_BumpMap_ST", "_ShadowMapTexture", NULL);

		// Alpha tested shader: texture, cutoff etc. affect shadow caster pass. Bumpmap doesn't affect it.
		CheckPassAffectingProps_TestHelper("Cutout BumpSpec pass", shCutoutBSpec->GetShadowCasterPass(), "_MainTex", "_Cutoff", "_Color", "_MainTex_ST", NULL);
	}

	TEST (Shader_MaterialGetDefaultProperties_EditorOnly)
	{
		// Check that a shader with some default property values actually gets them in the material.
		const char* kShader =
			"Shader \"\" {\n"
			"Properties {\n"
			"myColor (\"\", color) = (1,2,3,4)\n"
			"myFloat (\"\", float) = 0.5\n"
			"myRange (\"\", range(0,10)) = 5.0\n"
			"myVector (\"\", vector) = (5,6,7,8)\n"
			"}\n"
			"SubShader { Pass { } }\n"
			"}";
		Material* mat = Material::CreateMaterial(kShader,Object::kHideAndDontSave);
		Shader* myShader = mat->GetShader();

		ShaderLab::FastPropertyName propShine = ShaderLab::Property("_Shininess");
		ShaderLab::FastPropertyName propMyColor = ShaderLab::Property("myColor");
		ShaderLab::FastPropertyName propMyFloat = ShaderLab::Property("myFloat");
		ShaderLab::FastPropertyName propMyRange = ShaderLab::Property("myRange");
		ShaderLab::FastPropertyName propMyVector = ShaderLab::Property("myVector");

		// Don't compare in linear space; returned value close but slightly different due to float conversions.
		if (GetActiveColorSpace() == kGammaColorSpace)
		{
			CHECK (mat->GetColor(propMyColor).Equals(ColorRGBAf(1,2,3,4)));
		}
		CHECK (mat->GetFloat(propMyFloat) == 0.5f);
		CHECK (mat->GetFloat(propMyRange) == 5.0f);
		CHECK (mat->GetColor(propMyVector).Equals(ColorRGBAf(5,6,7,8)));

		// Assign a different shader.
		Shader* vlitShader = GetScriptMapper().FindShader("VertexLit");
		CHECK (!mat->HasProperty(propShine));
		mat->SetShader (vlitShader);

		// Now we should get _Shininess, with the default value.
		float shininess = mat->GetFloat(propShine);
		CHECK (shininess == 0.7f);

		// Now, new shader doesn't have myFloat, so it shouldn't be in the runtime PropertySheet.
		CHECK (!mat->HasProperty(propMyFloat));
		// But the value should still be intact in the serialized sheet!
		CHECK (mat->GetSavedProperties().m_Floats[propMyFloat] == 0.5f);

		DestroySingleObject (mat);
		DestroySingleObject (myShader);
	}

#endif // #if UNITY_EDITOR
}

#endif // #if ENABLE_UNIT_TESTS
