#include "UnityPrefix.h"
#include "Renderqueue.h"
#include "BaseRenderer.h"
#include "Runtime/Graphics/Transform.h"
#include "Camera.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderstate.h"
#include "UnityScene.h"
#include "RenderSettings.h"
#include "RenderManager.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Shaders/VBO.h"
#include "CameraUtil.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Profiler/Profiler.h"
#if UNITY_EDITOR
#include "Editor/Src/LightmapVisualization.h"
#endif

PROFILER_INFORMATION(gWarmupShadersProfile, "Shader.WarmupAllShaders", kProfilerRender)


static ShaderKeyword kLightmapOffKeyword = keywords::Create("LIGHTMAP_OFF");
static ShaderKeyword kLightmapOnKeyword = keywords::Create("LIGHTMAP_ON");

static ShaderKeyword kDirLightmapOffKeyword = keywords::Create("DIRLIGHTMAP_OFF");
static ShaderKeyword kDirLightmapOnKeyword = keywords::Create("DIRLIGHTMAP_ON");


void SetupObjectMatrix (const Matrix4x4f& m, int transformType)
{
	GfxDevice& device = GetGfxDevice();
	device.SetWorldMatrixAndType( m.GetPtr(), TransformType(transformType) );
}


UInt32 GetCurrentRenderOptions ()
{
	UInt32 currentRenderOptions = 0;
	if( GetQualitySettings().GetCurrent().softVegetation )
		currentRenderOptions |= (1 << kShaderOptionSoftVegetation);
	return currentRenderOptions;
}

bool CheckShouldRenderPass (int pass, Material& material)
{
	UInt32 currentRenderOptions = GetCurrentRenderOptions ();

	ShaderPassType passType;
	UInt32 passRenderOptions;
	ShaderLab::Pass* slpass = material.GetShader()->GetShaderLabShader()->GetActiveSubShader().GetPass(pass);
	slpass->GetPassOptions (passType, passRenderOptions);

	// All options that a pass requires must be on
	if( (currentRenderOptions & passRenderOptions) != passRenderOptions )
		return false; // some options are off, skip this pass

	return true;
}

bool SetupObjectLightmaps (const LightmapSettings& lightmapper, UInt32 lightmapIndex, const Vector4f& lightmapST, bool setMatrix)
{
	// setup object's lightmap info
	LightmapSettings::TextureTriple lmTextures = lightmapper.GetLightmapTexture (lightmapIndex);
#if UNITY_EDITOR
	if (lmTextures.first.m_ID && GetLightmapVisualization().GetUseLightmapsForRendering())
#else
	if (lmTextures.first.m_ID)
#endif
	{
		g_ShaderKeywords.Enable( kLightmapOnKeyword );
		g_ShaderKeywords.Disable( kLightmapOffKeyword );
		
		if (lightmapper.GetLightmapsMode() == LightmapSettings::kDirectionalLightmapsMode)
		{
			g_ShaderKeywords.Enable( kDirLightmapOnKeyword );
			g_ShaderKeywords.Disable( kDirLightmapOffKeyword );
		}
		else
		{
			g_ShaderKeywords.Enable( kDirLightmapOffKeyword );
			g_ShaderKeywords.Disable( kDirLightmapOnKeyword );
		}

		
		#define INIT_LIGHTMAP_TEX(texI, tex) GetGfxDevice().GetBuiltinParamValues().GetWritableTexEnvParam(texI).SetTextureInfo(tex, kTexDim2D, NULL)
		
		//@TODO: optimize
		TextureID indTex = lmTextures.second.m_ID ? lmTextures.second : builtintex::GetBlackTextureID();
		TextureID thirdTex = lmTextures.third.m_ID ? lmTextures.third : builtintex::GetBlackTextureID();
		INIT_LIGHTMAP_TEX(kShaderTexEnvUnityLightmap, lmTextures.first);
		INIT_LIGHTMAP_TEX(kShaderTexEnvUnityLightmapInd, indTex);
		INIT_LIGHTMAP_TEX(kShaderTexEnvUnityLightmapThird, thirdTex);
		
		#undef INIT_LIGHTMAP_TEX

		GetGfxDevice().GetBuiltinParamValues().SetVectorParam(kShaderVecUnityLightmapST, lightmapST);
		if (setMatrix)
		{
			Matrix4x4f mat;
			mat.SetIdentity();
			mat.Get(0,0) = lightmapST[0];
			mat.Get(1,1) = lightmapST[1];
			mat.Get(0,3) = lightmapST[2];
			mat.Get(1,3) = lightmapST[3];
			GetGfxDevice().GetBuiltinParamValues().SetMatrixParam(kShaderMatLightmapMatrix, mat);
		}
		return true;
	}
	else
	{
		g_ShaderKeywords.Enable( kLightmapOffKeyword );
		g_ShaderKeywords.Disable( kLightmapOnKeyword );
		
		g_ShaderKeywords.Enable( kDirLightmapOffKeyword );
		g_ShaderKeywords.Disable( kDirLightmapOnKeyword );
		return false;
	}
}


typedef std::set<const void*> PointerSet;


static int WarmupOnePass (ShaderLab::Pass& pass, const ShaderLab::PropertySheet* props, DynamicVBO& vbo, PointerSet& passes, PointerSet& programs)
{
	if (pass.GetPassType() != ShaderLab::Pass::kPassNormal)
		return 0;
	
	// check if this pass already done (we clone quite a few passes via Fallback/UsePass)
	if (passes.find(&pass) != passes.end())
		return 0;
	passes.insert (&pass);
	

	// If we have vertex or fragment program, pick the one with larger permutation count.
	const ShaderLab::Program* vp = pass.GetState().GetProgram(kShaderVertex);
	const ShaderLab::Program* fp = pass.GetState().GetProgram(kShaderFragment);

	const ShaderLab::Program* prog = NULL;
	if (vp && fp)
		prog = vp->GetSubProgramCount() > fp->GetSubProgramCount() ? vp : fp;
	else if (vp)
		prog = vp;
	else if (fp)
		prog = fp;

	if (!prog)
	{
		// Fixed function; just apply shader state
		const ChannelAssigns* channels = pass.ApplyPass (0, props);
		vbo.DrawChunk (*channels);
		GPU_TIMESTAMP();
		return 1;
	}
	else
	{
		// Check if this program already done (we cache identical programs)
		if (programs.find(prog) != programs.end())
			return 0;
		programs.insert (prog);
		
		// Go over all shader permutations in the program
		int combos = 0;
		for (int i = 0; i < prog->GetSubProgramCount(); ++i)
		{
			g_ShaderKeywords = prog->GetSubProgramKeywords (i);
			const ChannelAssigns* channels = pass.ApplyPass (0, props);
			vbo.DrawChunk (*channels);
			GPU_TIMESTAMP();
			++combos;
		}
		return combos;
	}
}

static int WarmupOneShader (ShaderLab::IntShader& shader, DynamicVBO& vbo, PointerSet& passes, PointerSet& programs)
{
	ShaderKeywordSet savedKeywords = g_ShaderKeywords;
	
	int combos = 0;

	const ShaderLab::PropertySheet* props = shader.GetDefaultProperties();
	for (size_t is = 0; is < shader.GetSubShaders().size(); ++is)
	{
		ShaderLab::SubShader& ss = shader.GetSubShader(is);
		for (size_t ip = 0; ip < ss.GetValidPassCount(); ++ip)
		{
			combos += WarmupOnePass (*ss.GetPass(ip), props, vbo, passes, programs);
		}
	}

	g_ShaderKeywords = savedKeywords;
	
	return combos;
}

void WarmupAllShaders ()
{
	PROFILER_AUTO(gWarmupShadersProfile, NULL);
	
	double t0 = GetTimeSinceStartup();
	int shaderCount = 0;
	int comboCount = 0;

	// Get a dynamic VBO chunk with one triangle
	struct FullVertex {
		Vector3f vert;
		Vector3f normal;
		ColorRGBA32	color;
		Vector2f uv1;
		Vector2f uv2;
		Vector4f tangent;
	};

	GfxDevice& device = GetGfxDevice ();
	DynamicVBO& vbo = device.GetDynamicVBO();
	FullVertex* vbPtr;
	if (!vbo.GetChunk(
		(1<<kShaderChannelVertex) |
		(1<<kShaderChannelNormal) |
		(1<<kShaderChannelColor) |
		(1<<kShaderChannelTexCoord0) |
		(1<<kShaderChannelTexCoord1) |
		(1<<kShaderChannelTangent),
		3, 0,
		DynamicVBO::kDrawTriangleStrip,
		(void**)&vbPtr, NULL))
	{
		return;
	}
	// Set all vertex data to zero; this will ensure nothing is changed on screen
	// but is enough for the driver to actually issue a draw call with a fully
	// prepared shader/state.
	memset (vbPtr, 0, 3*sizeof(FullVertex));
	vbo.ReleaseChunk (3, 0);

	DeviceMVPMatricesState saveMVPMatrices;
	LoadFullScreenOrthoMatrix ();
	
	PointerSet passes, programs;

	// Go over all currently loaded shaders
	std::vector<SInt32> allShaderObjects;
	Object::FindAllDerivedObjects (ClassID (Shader), &allShaderObjects);
	for (std::vector<SInt32>::iterator i = allShaderObjects.begin(); i != allShaderObjects.end(); ++i)
	{
		Shader* s = PPtr<Shader>(*i);
		if (!s)
			continue;
		ShaderLab::IntShader* shader = s->GetShaderLabShader();
		if (!shader)
			continue;

		int combos = WarmupOneShader (*shader, vbo, passes, programs);
		++shaderCount;
		comboCount += combos;
	}
	
	double t1 = GetTimeSinceStartup();
	printf_console ("Shader warmup: %i shaders %i combinations %.3fs\n", shaderCount, comboCount, (t1-t0));
}
