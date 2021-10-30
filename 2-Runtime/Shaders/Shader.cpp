#include "UnityPrefix.h"
#include "Shader.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Material.h"
#include "ShaderNameRegistry.h"
#include "Runtime/Utilities/Word.h"
#include "External/shaderlab/Library/ShaderParser.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/ResourceManager.h"
#include "GraphicsCaps.h"
#include "External/shaderlab/Library/SLParserData.h"

#if UNITY_EDITOR
#include "External/shaderlab/Library/ShaderWriter.h"
#endif
#include "Runtime/Profiler/MemoryProfiler.h"

using std::vector;

// To get a precompiled version of specific platform shader
// Create this shader in UnityEditor and click on "Open compiled shader"
//Shader "DefaultShader" 
//{
//	SubShader
//	{
//		Pass
//		{
//		CGPROGRAM
//
//			#pragma vertex vert
//			#pragma fragment frag
//			float4 vert (float4 pos : POSITION) : SV_POSITION { return mul(UNITY_MATRIX_MVP,pos); }
//			float4 frag () : COLOR { return float4(1,0,1,1); }
//		ENDCG
//		}
//	} 
//}



static const char *gDefaultString =
"Shader \"Default\" {"
#if UNITY_XENON
"SubShader { Pass { Cull Off \n"
"Program \"vp\" { SubProgram \"xbox360 \" { Keywords { }\n"
"	Bind \"vertex\" Vertex\n"
"	Matrix 0 [glstate_matrix_mvp]\n"
"	\"vs_360\n"
"	backbbabaaaaaalmaaaaaagmaaaaaaaaaaaaaaceaaaaaaaaaaaaaajaaaaaaaaa\n"
"	aaaaaaaaaaaaaagiaaaaaabmaaaaaaflpppoadaaaaaaaaabaaaaaabmaaaaaaaa\n"
"	aaaaaafeaaaaaadaaaacaaaaaaaeaaaaaaaaaaeeaaaaaaaaghgmhdhegbhegffp\n"
"	gngbhehcgjhifpgnhghaaaklaaadaaadaaaeaaaeaaabaaaaaaaaaaaahghdfpdd\n"
"	fpdaaadccodacodjdddcdicodaaaklklaaaaaaaaaaaaaagmaaabaaabaaaaaaaa\n"
"	aaaaaaaaaaaaaaaaaaaaaaabaaaaaaabaaaaaaaaaaaaacjaaaaaaaadbaabbaad\n"
"	aaaabcaamcaaaaaaaaaaeaaeaaaabcaameaaaaaaaaaaaaadaaaaccaaaaaaaaaa\n"
"	afpibaaaaaaaagiiaaaaaaaamiapaaaaaabliiaakbabadaamiapaaaaaamgiiaa\n"
"	klabacaamiapaaaaaalbdejeklababaamiapiadoaagmaadeklabaaaaaaaaaaaa\n"
"	aaaaaaaaaaaaaaaa\"\n"
"}}\n"
"Program \"fp\" { SubProgram \"xbox360 \" { Keywords { }\n"
"\"ps_360\n"
"	backbbaaaaaaaakaaaaaaageaaaaaaaaaaaaaaceaaaaaafiaaaaaaiaaaaaaaaa\n"
"	aaaaaaaaaaaaaadaaaaaaabmaaaaaacdppppadaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
"	aaaaaabmhahdfpddfpdaaadccodacodjdddcdicodaaaklklaaaaaaaaaaaaaaab\n"
"	aaaaaaaaaaaaaaaaaaaaaabeabpmaabaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
"	aaaaaaeaaaaaaacebaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab\n"
"	aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
"	aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaadpiaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
"	aaaaaaaabaabmeaaccaaaaaabenmmaaaaaaaaagmmcaaaappaaaaaaaaaaaaaaaa\n"
"	aaaaaaaa\"\n"
"}}\n"
"}}\n"
#elif UNITY_PS3
"SubShader { Pass {	Cull Off \n"
"Program \"vp\" { SubProgram \"ps3 \" { Keywords { }\n"
"Matrix 256 [glstate_matrix_mvp]\n"
"Bind \"vertex\" Vertex\n"
"\"sce_vp_rsx // 4 instructions using 1 registers\n"
"[Configuration]\n"
"8\n"
"0000000400010100\n"
"[Microcode]\n"
"64\n"
"401f9c6c01d0300d8106c0c360403f80401f9c6c01d0200d8106c0c360405f80\n"
"401f9c6c01d0100d8106c0c360409f80401f9c6c01d0000d8106c0c360411f81\n"
"\"\n"
"}}\n"
"Program \"fp\" { SubProgram \"ps3 \" { Keywords { }\n"
"\"sce_fp_rsx // 2 instructions using 2 registers\n"
"[Configuration]\n"
"24\n"
"ffffffff000000200000ffff000000000000840002000000\n"
"[Microcode]\n"
"32\n"
"1e81014008021c9cc8000001c800000100003f80000000000000000000000000\n"
"\"\n"
"}}\n"
"}}\n"

#else

#if GFX_SUPPORTS_D3D11
"SubShader { Pass {\n"
"Program \"vp\" {\n"
"SubProgram \"d3d11 \" { Bind \"vertex\" Vertex ConstBuffer \"UnityPerDraw\" 336 Matrix 0 [glstate_matrix_mvp] 4 BindCB \"UnityPerDraw\" 0\n"
"\"vs_dx11\n"
"eefiecedijhpljdppnfhjnjaadaickkmhicpkjbcabaaaaaaheabaaaaadaaaaaa\n"
"cmaaaaaagaaaaaaajeaaaaaaejfdeheocmaaaaaaabaaaaaaaiaaaaaacaaaaaaa\n"
"aaaaaaaaaaaaaaaaadaaaaaaaaaaaaaaapapaaaafaepfdejfeejepeoaaklklkl\n"
"epfdeheocmaaaaaaabaaaaaaaiaaaaaacaaaaaaaaaaaaaaaabaaaaaaadaaaaaa\n"
"aaaaaaaaapaaaaaafdfgfpfaepfdejfeejepeoaafdeieefcniaaaaaaeaaaabaa\n"
"dgaaaaaafjaaaaaeegiocaaaaaaaaaaaaeaaaaaafpaaaaadpcbabaaaaaaaaaaa\n"
"ghaaaaaepccabaaaaaaaaaaaabaaaaaagiaaaaacabaaaaaadiaaaaaipcaabaaa\n"
"aaaaaaaafgbfbaaaaaaaaaaaegiocaaaaaaaaaaaabaaaaaadcaaaaakpcaabaaa\n"
"aaaaaaaaegiocaaaaaaaaaaaaaaaaaaaagbabaaaaaaaaaaaegaobaaaaaaaaaaa\n"
"dcaaaaakpcaabaaaaaaaaaaaegiocaaaaaaaaaaaacaaaaaakgbkbaaaaaaaaaaa\n"
"egaobaaaaaaaaaaadcaaaaakpccabaaaaaaaaaaaegiocaaaaaaaaaaaadaaaaaa\n"
"pgbpbaaaaaaaaaaaegaobaaaaaaaaaaadoaaaaab\"\n"
"}\n"
"SubProgram \"d3d11_9x \" {\n"
"Bind \"vertex\" Vertex\n"
"ConstBuffer \"UnityPerDraw\" 336 \n"
"Matrix 0 [glstate_matrix_mvp] 4\n"
"BindCB \"UnityPerDraw\" 0\n"
"\"vs_4_0_level_9_1\n"
"eefieceddggiiplcpkoeljnckhkjahapknjdfpkhabaaaaaadeacaaaaaeaaaaaa\n"
"daaaaaaaomaaaaaammabaaaaaaacaaaaebgpgodjleaaaaaaleaaaaaaaaacpopp\n"
"iaaaaaaadeaaaaaaabaaceaaaaaadaaaaaaadaaaaaaaceaaabaadaaaaaaaaaaa\n"
"aeaaabaaaaaaaaaaaaaaaaaaaaacpoppbpaaaaacafaaaaiaaaaaapjaafaaaaad\n"
"aaaaapiaaaaaffjaacaaoekaaeaaaaaeaaaaapiaabaaoekaaaaaaajaaaaaoeia\n"
"aeaaaaaeaaaaapiaadaaoekaaaaakkjaaaaaoeiaaeaaaaaeaaaaapiaaeaaoeka\n"
"aaaappjaaaaaoeiaaeaaaaaeaaaaadmaaaaappiaaaaaoekaaaaaoeiaabaaaaac\n"
"aaaaammaaaaaoeiappppaaaafdeieefcniaaaaaaeaaaabaadgaaaaaafjaaaaae\n"
"egiocaaaaaaaaaaaaeaaaaaafpaaaaadpcbabaaaaaaaaaaaghaaaaaepccabaaa\n"
"aaaaaaaaabaaaaaagiaaaaacabaaaaaadiaaaaaipcaabaaaaaaaaaaafgbfbaaa\n"
"aaaaaaaaegiocaaaaaaaaaaaabaaaaaadcaaaaakpcaabaaaaaaaaaaaegiocaaa\n"
"aaaaaaaaaaaaaaaaagbabaaaaaaaaaaaegaobaaaaaaaaaaadcaaaaakpcaabaaa\n"
"aaaaaaaaegiocaaaaaaaaaaaacaaaaaakgbkbaaaaaaaaaaaegaobaaaaaaaaaaa\n"
"dcaaaaakpccabaaaaaaaaaaaegiocaaaaaaaaaaaadaaaaaapgbpbaaaaaaaaaaa\n"
"egaobaaaaaaaaaaadoaaaaabejfdeheocmaaaaaaabaaaaaaaiaaaaaacaaaaaaa\n"
"aaaaaaaaaaaaaaaaadaaaaaaaaaaaaaaapapaaaafaepfdejfeejepeoaaklklkl\n"
"epfdeheocmaaaaaaabaaaaaaaiaaaaaacaaaaaaaaaaaaaaaabaaaaaaadaaaaaa\n"
"aaaaaaaaapaaaaaafdfgfpfaepfdejfeejepeoaa\"\n"
"}\n"
"}\n"
"Program \"fp\" {\n"
"SubProgram \"d3d11 \" {\n"
"\"ps_dx11\n"
"eefiecedlfbokalingbkfpbcgbelaibibjjpegjnabaaaaaalaaaaaaaadaaaaaa\n"
"cmaaaaaadmaaaaaahaaaaaaaejfdeheoaiaaaaaaaaaaaaaaaiaaaaaaepfdeheo\n"
"cmaaaaaaabaaaaaaaiaaaaaacaaaaaaaaaaaaaaaaaaaaaaaadaaaaaaaaaaaaaa\n"
"apaaaaaafdfgfpfegbhcghgfheaaklklfdeieefcdiaaaaaaeaaaaaaaaoaaaaaa\n"
"gfaaaaadpccabaaaaaaaaaaadgaaaaaipccabaaaaaaaaaaaaceaaaaaaaaaiadp\n"
"aaaaaaaaaaaaiadpaaaaiadpdoaaaaab\"\n"
"}\n"
"SubProgram \"d3d11_9x \" {\n"
"\"ps_4_0_level_9_1\n"
"eefiecednclpehpkkhodapchnemhifoohfkahfmoabaaaaaaamabaaaaaeaaaaaa\n"
"daaaaaaaiiaaaaaamiaaaaaaniaaaaaaebgpgodjfaaaaaaafaaaaaaaaaacpppp\n"
"cmaaaaaaceaaaaaaaaaaceaaaaaaceaaaaaaceaaaaaaceaaaaaaceaaaaacpppp\n"
"fbaaaaafaaaaapkaaaaaiadpaaaaaaaaaaaaiadpaaaaiadpabaaaaacaaaiapia\n"
"aaaaoekappppaaaafdeieefcdiaaaaaaeaaaaaaaaoaaaaaagfaaaaadpccabaaa\n"
"aaaaaaaadgaaaaaipccabaaaaaaaaaaaaceaaaaaaaaaiadpaaaaaaaaaaaaiadp\n"
"aaaaiadpdoaaaaabejfdeheoaiaaaaaaaaaaaaaaaiaaaaaaepfdeheocmaaaaaa\n"
"abaaaaaaaiaaaaaacaaaaaaaaaaaaaaaaaaaaaaaadaaaaaaaaaaaaaaapaaaaaa\n"
"fdfgfpfegbhcghgfheaaklkl\"\n"
"}\n"
"}\n"
"}}"
#endif

"	SubShader { Tags { \"ForceSupported\" = \"True\" } Pass{ Cull Off SetTexture[_Dummy] { constantColor (1,0,1,1) combine constant }}}	 \n"
#endif
"}";

int*							gShaderLabContainer = NULL;

static ShaderLab::IntShader*	gDefaultShaderLabShader = NULL;
static Shader*					gDefaultShader = NULL;
static Shader*					s_ClearShader = NULL;
static void MakeDefaultShaderLabShader ();

Shader::Shader(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
, m_ShaderIsBaked(false)
{
	MakeDefaultShaderLabShader ();
	m_Shader = gDefaultShaderLabShader;
	m_ShadowCasterPass = NULL;
	m_ShadowCollectorPass = NULL;
	m_NeedsParsing = false;

	#if UNITY_EDITOR
	m_ParsedForm = NULL;
	#endif

	if (mode != kCreateObjectFromNonMainThread)
		PostLoad ();
}

Shader *Shader::GetDefault ()
{
	if (!gDefaultShader) {
		SET_ALLOC_OWNER(gShaderLabContainer);
		gDefaultShader = NEW_OBJECT(Shader);
		gDefaultShader->Reset();
		#if !UNITY_RELEASE
		// in Shader's AwakeFromLoad it will try to parse script and call PostLoad
		// we basically doing the same, but we assign default shader directly (w/o script)
		gDefaultShader->HackSetAwakeWasCalled();
		#endif

		gDefaultShader->m_Shader = gDefaultShaderLabShader;
		gDefaultShader->PostLoad();
		gDefaultShader->SetHideFlags (kHideAndDontSave);
	}
	return gDefaultShader;
}

Shader* Shader::GetScreenClearShader ()
{
	Assert (s_ClearShader);
	return s_ClearShader;
}


void Shader::LoadDefaultShaders ()
{
	GetDefault ();

	#if GFX_SUPPORTS_D3D11 || GFX_SUPPORTS_XENON
	if (IsGfxDevice())
	{
		Assert (!s_ClearShader);
		s_ClearShader = GetBuiltinResource<Shader> ("Internal-Clear.shader");
	}
	#endif
}


Shader::~Shader ()
{
	if (m_Shader != gDefaultShaderLabShader)
		UNITY_DELETE(m_Shader, kMemShader);

	if (this == gDefaultShader)
		gDefaultShader = NULL;
	
	#if UNITY_EDITOR
	UNITY_DELETE( m_ParsedForm, kMemShader);
	#endif
}

bool Shader::MainThreadCleanup ()
{
	if (m_Shader != gDefaultShaderLabShader)
	{
		m_Shader->MainThreadCleanup ();
	}
	
	return true;
}

ShaderLab::PropertySheet *Shader::MakeProperties () const
{
	return m_Shader->MakeProperties ();
}

#if UNITY_EDITOR

UInt32 Shader::CalculateUsedVertexComponents (bool lightmapped)
{
	if (m_Shader == NULL)
		return 0;

	UInt32 mask = 0;

	const ShaderLab::IntShader::SubShaders& subshaders = m_Shader->GetSubShaders();
	for (int i=0;i<subshaders.size();i++)
	{
		int npasses = subshaders[i]->GetTotalPassCount();
		for (int p = 0; p < npasses; ++p)
		{
			mask |= subshaders[i]->GetPass(p)->CalculateUsedVertexComponents(lightmapped);
		}
	}

	return mask;
}

int Shader::GetPropertyCount () const
{
	if (!m_ParsedForm)
		return 0;
	return m_ParsedForm->m_PropInfo.GetPropertyCount();
}

const ShaderLab::ParserProperty* Shader::GetPropertyInfo (int propertyNo) const
{
	if (!m_ParsedForm)
		return NULL;
	return m_ParsedForm->m_PropInfo.GetPropertyInfo (propertyNo);
}

bool Shader::HasClip() const
{
	return m_ParsedForm ? m_ParsedForm->HasClip() : false;
}

char const* Shader::GetCustomEditorName() const
{
	if (!m_Shader)
			return NULL;
	return m_Shader->GetCustomEditor ().c_str ();
}

#endif

PROFILER_INFORMATION(gSetPassProfile, "Shader.SetPass", kProfilerRender);

const ChannelAssigns* Shader::SetPass( int subshaderIndex, int passNo, UInt32 stateKey, const ShaderLab::PropertySheet* props )
{
	PROFILER_AUTO(gSetPassProfile, this)
	return m_Shader->GetSubShader(subshaderIndex).SetPass (passNo, stateKey, props);
}

bool Shader::CanPassBeRecorded(int subshaderIndex, int passNo) const
{
	const ShaderLab::Pass* pass = m_Shader->GetSubShader(subshaderIndex).GetPass(passNo);
	return pass->CanPassBeRecorded();
}

ShaderLab::Pass* Shader::GetShadowCasterPassToUse(int subshaderIndex)
{
	ShaderLab::SubShader& subShader = m_Shader->GetSubShader(subshaderIndex);
	if (!subShader.ShadowCasterPassEnabled())
		return NULL;
	int ssPass = subShader.GetShadowCasterPassIndex();
	if (ssPass >= 0)
		return subShader.GetPass(ssPass);
	return m_ShadowCasterPass;
}

ShaderLab::Pass* Shader::GetShadowCollectorPassToUse(int subshaderIndex)
{
	ShaderLab::SubShader& subShader = m_Shader->GetSubShader(subshaderIndex);
	int ssPass = subShader.GetShadowCollectorPassIndex();
	if (ssPass >= 0)
		return subShader.GetPass(ssPass);
	return m_ShadowCollectorPass;
}


int Shader::GetActiveSubShaderIndex () const
{
	return GetShaderLabShader()->GetActiveSubShaderIndex();
}

bool Shader::SetScript (const TextAsset::ScriptString& script)
{
	Super::SetScriptDontDirty (script);

	m_ShadowCasterPass = NULL;
	m_ShadowCollectorPass = NULL;

	ParseAndPostLoad( script.c_str(), script.size() );

	return m_Shader != gDefaultShaderLabShader;
}


namespace ShaderLab {
extern const char* kPassLightModeTypeNames[kShaderPassTypeCount];
}


static ShaderLab::Pass* FindLightModePass( ShaderLab::IntShader* shader, ShaderPassType type )
{
	using ShaderLab::TagMap;
	typedef ShaderLab::IntShader::SubShaders SubShaders;

	static const int lightModeTagID = ShaderLab::GetShaderTagID("LIGHTMODE");

	const SubShaders& subshaders = shader->GetSubShaders();
	for( SubShaders::const_iterator it = subshaders.begin(); it != subshaders.end(); ++it )
	{
		ShaderLab::SubShader& ss = **it;
		for( int i = 0; i < ss.GetValidPassCount (); ++i )
		{
			const TagMap& tags = ss.GetPass(i)->GetTags();
			TagMap::const_iterator found = tags.find (lightModeTagID);
			if( found != tags.end() && (StrICmp( ShaderLab::GetShaderTagName(found->second), ShaderLab::kPassLightModeTypeNames[type] ) == 0 ) )
			{
				return ss.GetPass(i);
			}
		}
	}

	return NULL;
}

void Shader::PostLoad ()
{
	m_Shader->PostLoad();

	// Find out shadow caster/collector passes
	Assert (m_ShadowCasterPass == NULL && m_ShadowCollectorPass == NULL);
	m_ShadowCasterPass = FindLightModePass( m_Shader, kPassShadowCaster );
	m_ShadowCollectorPass = FindLightModePass( m_Shader, kPassShadowCollector );
}

void Shader::AddMaterialUser( ListNode<Unity::Material>& node )
{
	m_Users.push_back( node );
}

PROFILER_INFORMATION(gShaderParseProfile, "Shader.Parse", kProfilerRender);

void Shader::Parse (const char *str, size_t strLength)
{
	PROFILER_AUTO(gShaderParseProfile, this);
	SET_ALLOC_OWNER(this);

	m_NeedsParsing = false;
	if( m_Shader != gDefaultShaderLabShader )
		UNITY_DELETE(m_Shader, kMemShader);

	#if UNITY_EDITOR
	UNITY_DELETE( m_ParsedForm, kMemShader);
	m_ParsedForm = NULL;
	ShaderErrors& errors = m_Errors;
	#else
	ShaderErrors errors;
	#endif

	ShaderLab::ParserShader* parsedForm = NULL;
	m_Shader = ParseShader (str, strLength, m_Dependencies, m_ShaderIsBaked, &parsedForm, errors, GetGfxDevice().GetRenderer());
	// In editor, keep the parsed form so we can serialize shaders out specific for a platform. In players, delete parsed form now.
	#if UNITY_EDITOR
	m_ParsedForm = parsedForm;
	#else
	UNITY_DELETE( parsedForm, kMemShader);
	#endif


	m_ShadowCasterPass = NULL;
	m_ShadowCollectorPass = NULL;
	m_ShaderName = g_LastParsedShaderName;

	// For no-fixed function hardware and shaders that ended up having only shadow related passes
	// (like VertexLit), insert the pass from default shader. Otherwise the shader is supported, but
	// has no passes suitable for any rendering path, hence all objects with it are not rendered at all.
	if (!gGraphicsCaps.hasFixedFunction && m_Shader && gDefaultShaderLabShader)
	{
		ShaderLab::SubShader& ss = m_Shader->GetActiveSubShader();
		if (ss.GetHasOnlyShadowPasses()) {
			printf_console("Unsupported: %s\n", m_ShaderName.c_str());
			ss.InsertDefaultPass (gDefaultShaderLabShader->GetActiveSubShader().GetPass(0));
		}
	}

	if( !m_Shader )
	{
		printf_console("Unsupported: %s\n", m_ShaderName.c_str());
		m_Shader = gDefaultShaderLabShader;
		return;
	}

	if (m_Shader->HasNoSubShaders())
	{
		char buf[256];
		snprintf(buf, 255, "No valid subshaders in '%s'.shader", GetScriptClassName().c_str());
		ErrorStringObject (buf, this);
		UNITY_DELETE(m_Shader, kMemShader);
		m_Shader = gDefaultShaderLabShader;
	}
}


void Shader::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	SET_ALLOC_OWNER(this);
	Super::AwakeFromLoad (awakeMode);
	const ScriptString& s = GetScript();
	ParseAndPostLoad( s.c_str(), s.size() );
}

void Shader::AwakeFromLoadThreaded ()
{
	Super::AwakeFromLoadThreaded ();

	// In some cases a shader is constructed (on the loading thread, has shader pointing
	// to the default one, and m_NeedsParsing flag set to false). Later on
	// actual shader is being loaded, but no one resets the m_NeedsParsing flag.
	// This seems to happen only in a very rare combination of shaders loaded via fallbacks
	// combined with shaders being referenced from scripts only; and even then it
	// highly depends on the order the shaders end up being loaded. Couldn't
	// make an automated repro case.
	if (m_Shader == gDefaultShaderLabShader)
	{
		m_Shader = NULL;
		m_ShadowCasterPass = NULL;
		m_ShadowCollectorPass = NULL;
		m_NeedsParsing = true;
	}
}

template<class TransferFunc>
void Shader::Transfer (TransferFunc& transfer)
{
	#if UNITY_EDITOR
	if (transfer.IsBuildingTargetPlatform(kBuildAnyPlayerData))
	{
		UnityStr newSource = ProduceTargetSpecificShader (m_Script, m_ParsedForm, transfer.GetBuildingTarget());

		Super::Super::Transfer (transfer);
		transfer.Transfer (newSource, "m_Script", kHideInEditorMask);
		transfer.Transfer (m_PathName, "m_PathName", kHideInEditorMask);
	}
	else
		Super::Transfer (transfer);
	#else
	Super::Transfer (transfer);
	#endif

	transfer.Transfer (m_Dependencies, "m_Dependencies");

	// Shaders in player data files and asset bundles should get "shader is baked" flag,
	// so they get their dependencies directly instead of through script mapper.
	// Need to set this flag to true when building the data files, but properly
	// reset it back afterwards.
	if (transfer.IsWritingGameReleaseData())
	{
		bool shaderIsBaked = true;
		transfer.Transfer (shaderIsBaked, "m_ShaderIsBaked");
	}
	else
	{
		transfer.Transfer (m_ShaderIsBaked, "m_ShaderIsBaked");
	}

	transfer.Align();

	#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease() && !transfer.IsRemapPPtrTransfer ())
	{
		transfer.Transfer (m_Errors.GetErrors(), "errors");
		transfer.Transfer (m_DefaultTextures, "m_DefaultTextures", kHideInEditorMask);
	}
	#endif

}

char const* Shader::GetName () const {
	// shader name is the name of the shader, not the filename
	return GetScriptClassName().c_str();
}


bool Shader::IsSupported () const
{
	return m_Shader != gDefaultShaderLabShader;
}

int Shader::GetSubShaderWithTagValue (int tagNameID, int tagValueID) const
{
	using ShaderLab::TagMap;
	typedef ShaderLab::IntShader::SubShaders SubShaders;

	const SubShaders& ss = m_Shader->GetSubShaders();
	int idx = 0;
	for( SubShaders::const_iterator i = ss.begin(); i != ss.end(); ++i )
	{
		const TagMap& tags = (**i).GetTags();
		TagMap::const_iterator j = tags.find(tagNameID);
		if (j != tags.end () && j->second == tagValueID)
			return idx;
		idx++;
	}
	return -1;
}

bool Shader::IsDependentOn (PPtr<Shader> shader) const
{
	for (int i = 0; i < m_Dependencies.size (); ++i)
		if (m_Dependencies[i] == shader ||
			m_Dependencies[i]->IsDependentOn (shader))
			return true;

	return false;
}

Shader* Shader::GetDependency (const std::string& name)
{
	if (!m_Shader)
		return NULL;
	const std::string* dependencyName = m_Shader->GetDependency (name);
	if (!dependencyName || dependencyName->empty())
		return NULL;
	Shader* res = FindShaderLabShader (*dependencyName, m_Dependencies, m_ShaderIsBaked);
	return res;
}


void Shader::UnloadDefaultShaderLabShader()
{
	UNITY_DELETE(gDefaultShaderLabShader, kMemShader);
	gDefaultShaderLabShader = NULL;
	if (gDefaultShader)
		gDefaultShader->m_Shader = NULL;
}

void Shader::LoadDefaultShaderLabShader()
{
	ShaderLab::ParserShader* parsedForm = NULL;
	ShaderErrors errors;
	ShaderPtrVector dependencies;
	gDefaultShaderLabShader = ParseShader (gDefaultString, strlen(gDefaultString), dependencies, false, &parsedForm, errors, GetGfxDevice().GetRenderer());
	if (errors.HasErrorsOrWarnings()) {
		ErrorString ("Default Shader has errors! Something is wrong.");
	}
	UNITY_DELETE(parsedForm, kMemShader);

	if (gDefaultShader)
	{
		gDefaultShader->m_Shader = gDefaultShaderLabShader;
		gDefaultShader->PostLoad();
	}
}

static void MakeDefaultShaderLabShader ()
{
	if (gDefaultShaderLabShader)
		return;
	gShaderLabContainer = UNITY_NEW_AS_ROOT(int, kMemShader, "ShaderLab", "");
	SET_ALLOC_OWNER(gShaderLabContainer);
	ShaderLab::InitShaderLab ();

	Shader::LoadDefaultShaderLabShader();
}

void CleanupShaders ()
{
	Shader::UnloadDefaultShaderLabShader();
	ShaderLab::CleanupShaderLab();
	// still some allocations left in shaderlab, so we don't want to delete this one before someone has cleaned up shaderlab
//	UNITY_DELETE(gShaderLabContainer, kMemShader);
}

void Shader::DeleteAllShaders( std::vector<SInt32>& outShaderObjects )
{
	outShaderObjects.clear();
	Object::FindAllDerivedObjects (ClassID (Shader), &outShaderObjects);
	for (std::vector<SInt32>::iterator i = outShaderObjects.begin(); i != outShaderObjects.end(); i++)
	{
		Shader *s = PPtr<Shader> (*i);
		for (MaterialList::iterator sit = s->m_Users.begin(); sit != s->m_Users.end(); ++sit)
		{
			(*sit)->InvalidateDisplayLists();
		}
		if (s->m_Shader != gDefaultShaderLabShader)
		{
			UNITY_DELETE(s->m_Shader, kMemShader);
			s->m_Shader = NULL;
			s->m_ShadowCasterPass = NULL;
			s->m_ShadowCollectorPass = NULL;
			s->m_NeedsParsing = true;
		}
	}
}

void Shader::RecreateAllShaders( const std::vector<SInt32>& shaderObjects )
{
	for( std::vector<SInt32>::const_iterator i = shaderObjects.begin(); i != shaderObjects.end(); ++i )
	{
		Shader *s = PPtr<Shader> (*i);
		if( s != gDefaultShader )
		{
			const ScriptString& script = s->GetScript();
			s->ParseAndPostLoad( script.c_str(), script.size() );
		}
	}
}


void Shader::ReloadAllShaders()
{
	std::vector<SInt32> derivedObjects;
	DeleteAllShaders( derivedObjects );
	RecreateAllShaders( derivedObjects );
}

void Shader::SetGLobalMaximumShaderLOD( int lod )
{
	if( ShaderLab::g_GlobalMaximumShaderLOD == lod )
		return;
	ShaderLab::g_GlobalMaximumShaderLOD = lod;

	// notify all shaders of global LOD change
	std::vector<SInt32> shaders;
	Object::FindAllDerivedObjects (ClassID (Shader), &shaders);
	for (std::vector<SInt32>::iterator i = shaders.begin(); i != shaders.end(); ++i)
	{
		Shader* s = PPtr<Shader> (*i);
		s->m_Shader->PostLoad();
	}
}

int Shader::GetGlobalMaximumShaderLOD()
{
	return ShaderLab::g_GlobalMaximumShaderLOD;
}

void Shader::SetMaximumShaderLOD( int lod )
{
	if(GetMaximumShaderLOD() == lod)
		return;

	GetShaderLabShader()->SetMaximumShaderLOD(lod);
}

int Shader::GetMaximumShaderLOD( ) const
{
	return GetShaderLabShader()->GetMaximumShaderLOD();
}


IMPLEMENT_CLASS (Shader)
IMPLEMENT_OBJECT_SERIALIZE (Shader)
