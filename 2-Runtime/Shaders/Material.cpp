#include "UnityPrefix.h"
#include "Material.h"
#include "Shader.h"
#include "ShaderNameRegistry.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/ResourceManager.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/ProceduralMaterial.h"
#include "Runtime/GfxDevice/GfxDisplayList.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Profiler/Profiler.h"

#if UNITY_EDITOR
#include "External/shaderlab/Library/SLParserData.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"

extern ShaderLab::ShaderState* g_EditorPixelShaderOverride;
#endif


PROFILER_INFORMATION(gSetShadowCasterPass, "Shader.SetShadowCasterPass", kProfilerRender);
PROFILER_INFORMATION(gSetShadowCollectorPass, "Shader.SetShadowCollectorPass", kProfilerRender);


using namespace std;

namespace Unity
{

template<class TransferFunc>
void Material::Transfer (TransferFunc& transfer)
{
	transfer.SetVersion (3);
	Super::Transfer (transfer);
	transfer.Transfer (m_Shader, "m_Shader");
	transfer.Transfer (m_ShaderKeywords, "m_ShaderKeywords");
	transfer.Transfer (m_CustomRenderQueue, "m_CustomRenderQueue");
	
#if UNITY_EDITOR
	if (transfer.IsBuildingTargetPlatform(kBuild_Android) && m_Shader && m_Shader->HasClip())
	{
		DebugStringToFile (	"Shader is using clip instruction (usually caused by alpha test). It might cause problems on some Qualcomm/Adreno drivers.",
							0, __FILE__, __LINE__, kAssetImportWarning, GetInstanceID()
						  );

	}
#endif

	// Cull unused properties when making build!
	#if UNITY_EDITOR
	if ((transfer.GetFlags () & kBuildPlayerOnlySerializeBuildProperties))
	{
		Shader *shader = m_Shader;
		if (shader)
		{
			UnityPropertySheet tempProps = m_SavedProperties;
			tempProps.CullUnusedProperties (shader->GetParsedForm());
			transfer.Transfer (tempProps, "m_SavedProperties");
		}
		else
		{
			UnityPropertySheet tempProps;
			transfer.Transfer (tempProps, "m_SavedProperties");
		}
	}
	else
	#endif
	{
		TRANSFER (m_SavedProperties);
	}
}


void Material::Reset()
{
	Super::Reset();

	m_CustomRenderQueue = -1;

	ClearProperties();
	m_SavedProperties = UnityPropertySheet();

	Shader *shader = m_Shader;
	if( !shader )
		shader = Shader::GetDefault();
	
	BuildShaderKeywordSet (); // build keywords before properties, so that pass hashing gets correct keywords
	BuildProperties ();
#if UNITY_EDITOR
	ResetDefaultTextures (true);
	if (GetCachedScriptingObject())
		ApplyMaterialPropertyDrawers();
#endif
}


int Material::GetActualRenderQueue() const
{
	// Use custom render queue if set (case 548478)
	if (m_CustomRenderQueue >= 0)
		return m_CustomRenderQueue;

	const Shader* shader = GetShader();
	Assert (shader);
	return shader->GetShaderLabShader()->GetRenderQueue();
}


void Material::BuildProperties ()
{
	SET_ALLOC_OWNER(NULL);
	SAFE_RELEASE_LABEL(m_Properties,kMemShader);
	Shader *shader = m_Shader;
	if( !shader )
		shader = Shader::GetDefault();
	
	// This happens on Flash when loading some materials - at AwakeFromLoad
	// time, the shader is not parsed yet (?). Just return in that case, the
	// properties will be build later on demand. Worst that can happen is that
	// for one frame shadow casters might get batched slightly wrong.
	if (!shader->GetShaderLabShader())
		return;
	
	// Build the shaderlab property sheet.
	m_Properties = shader->MakeProperties();
	m_Properties->SetOwnerMaterial(this);

	// Make sure our serialized properties (m_SavedProperties) have
	// all the properties from the shader's properties block.
	// (we might not be up-to-date if the shader has changed and added more
	// properties).
	m_SavedProperties.AddNewShaderlabProps (*shader->GetShaderLabShader()->GetDefaultProperties());

	/// Get the properties from the material.
	m_SavedProperties.AssignDefinedPropertiesTo (*m_Properties);
	
	if( m_Shader )
		m_Shader->AddMaterialUser( m_ShaderUserNode );

	UpdateHashes();
}


void Material::UpdateHashesOnPropertyChange (ShaderLab::FastPropertyName name)
{
	Shader* shader = m_Shader;
	if (!shader || !m_Properties)
		return;
	ShaderLab::Pass* passCaster = shader->GetShadowCasterPass();
	if (passCaster)
	{
		if (passCaster->IsPropertyAffectingPass(name))
			m_ShadowCasterHash = passCaster->ComputePassValuesHash (m_ShaderKeywordSet, m_Properties);
	}
	ShaderLab::Pass* passCollector = shader->GetShadowCollectorPass();
	if (passCollector)
	{
		if (passCollector->IsPropertyAffectingPass(name))
			m_ShadowCollectorHash = passCollector->ComputePassValuesHash (m_ShaderKeywordSet, m_Properties);
	}

	const dynamic_array<int>& propsAffectingBlocks = shader->GetShaderLabShader()->GetPropsAffectingStateBlocks();
	if (std::find(propsAffectingBlocks.begin(), propsAffectingBlocks.end(), name.index) != propsAffectingBlocks.end())
	{
		m_StateKeyHash = ShaderLab::ComputeStateBlockValuesHash (propsAffectingBlocks, m_Properties);
		shader->GetShaderLabShader()->CreateStateBlocksForKey (m_StateKeyHash, m_Properties);
	}
}

void Material::UpdateHashes ()
{
	m_ShadowCasterHash = 0;
	m_ShadowCollectorHash = 0;
	m_StateKeyHash = 0;
	Shader* shader = m_Shader;
	if (!shader || !m_Properties)
		return;
	ShaderLab::Pass* passCaster = shader->GetShadowCasterPass();
	if (passCaster)
		m_ShadowCasterHash = passCaster->ComputePassValuesHash (m_ShaderKeywordSet, m_Properties);
	ShaderLab::Pass* passCollector = shader->GetShadowCollectorPass();
	if (passCollector)
		m_ShadowCollectorHash = passCollector->ComputePassValuesHash (m_ShaderKeywordSet, m_Properties);

	m_StateKeyHash = ShaderLab::ComputeStateBlockValuesHash (shader->GetShaderLabShader()->GetPropsAffectingStateBlocks(), m_Properties);
	shader->GetShaderLabShader()->CreateStateBlocksForKey (m_StateKeyHash, m_Properties);
}


void Material::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	BuildShaderKeywordSet (); // build keywords before properties, so that pass hashing gets correct keywords

	ClearProperties ();
	
	// Enforce that everything is preloaded in the player, reduces hiccups at runtime
	// Don't do that in the editor because it can easily run out of memory and increase build times.
	#if !UNITY_EDITOR
	BuildProperties();
	#endif	
}


Material::Material(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_ShaderUserNode(this)
{
	m_Properties = NULL;
	m_PropertiesDirty = false;
	m_Shader = 0;
	m_CustomRenderQueue = -1;
	m_ShadowCasterHash = 0;
	m_ShadowCollectorHash = 0;
	m_StateKeyHash = 0;
}

Material::~Material()
{
	SAFE_RELEASE_LABEL(m_Properties, kMemShader);
	InvalidateDisplayLists();
}

void Material::SetShader (Shader *s)
{
	m_ShaderUserNode.RemoveFromList(); // remove ourselves from old shader's users

	m_Shader = s; 
	if( !s )
	{
		SetDirty ();
		return;
	}

	BuildProperties ();

	SetDirty ();
	InvalidateDisplayLists();
}

Material *Material::CreateMaterial (const char *shaderStr, int hideFlags, bool scriptingObjectIsBeingCreated)
{
	// Create temporary shader
	Shader *shader = NEW_OBJECT (Shader);
	shader->Reset();

	shader->SetHideFlags(hideFlags);
	shader->SetScript (shaderStr);
	#if UNITY_EDITOR
	shader->GetErrors().LogErrors (shader->GetName(), shader->GetNamedObjectName(), shader, shader->GetInstanceID());
	#endif

	shader->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
	
	// TODO: in editor parse shaderlab errors to console
	//shader->ParseShaderErrorsToConsole();
	return CreateMaterial(*shader, hideFlags, scriptingObjectIsBeingCreated);
}

Material *Material::CreateMaterial (Shader& shader, int hideFlags, bool scriptingObjectIsBeingCreated)
{
	// Create temporary material
	Material *mat = CreateObjectFromCode<Material>();

	mat->SetHideFlags(hideFlags);
	mat->SetName(shader.GetName ());
	mat->m_Shader = &shader;

	mat->BuildProperties ();

#if UNITY_EDITOR
	mat->ResetDefaultTextures (true);
	if (!scriptingObjectIsBeingCreated)
		mat->ApplyMaterialPropertyDrawers();
#endif

	return mat;
}

Material *Material::CreateMaterial (const Material& material, int hideFlags, bool scriptingObjectIsBeingCreated)
{
	// Create temporary material
	Material *mat = CreateObjectFromCode<Material>();

	mat->SetHideFlags(hideFlags);
	mat->SetName(material.GetName ());

	mat->m_Shader = material.m_Shader;
	mat->m_SavedProperties.AddNewSerializedProps (material.m_SavedProperties);
	mat->BuildProperties ();

	return mat;
}

Material *Material::GetDefault ()
{
	static Material* s_DefaultMaterial = NULL;
	if (!s_DefaultMaterial) {
		s_DefaultMaterial = CreateObjectFromCode<Material>();
		s_DefaultMaterial->SetHideFlags (kHideAndDontSave);
	}
	return s_DefaultMaterial;
}

Material *Material::GetDefaultDiffuseMaterial ()
{
	#if WEBPLUG
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion_OldWebResourcesAdded))
		return GetBuiltinOldWebResource<Material> ("Default-Diffuse.mat");
	#endif

	#if UNITY_EDITOR
	// In the editor, important to return an asset here: e.g. if we create a cube,
	// then we want to turn that into a prefab with a proper material.
	return GetBuiltinExtraResource<Material> ("Default-Diffuse.mat");
	#endif

	static PPtr<Material> s_DefaultDiffuseMaterial;
	if (s_DefaultDiffuseMaterial.IsNull())
	{
		Shader* shader = GetScriptMapper().FindShader("Diffuse");
		if (!shader)
			shader = Shader::GetDefault();
		s_DefaultDiffuseMaterial = CreateMaterial(*shader, kHideAndDontSave);
	}
	return s_DefaultDiffuseMaterial;
}

struct ApplyKeywords
{
	ShaderKeywordSet prev;
	ApplyKeywords (ShaderKeywordSet mask) {
		prev = g_ShaderKeywords;
		g_ShaderKeywords.SetMask (prev.GetMask () | mask.GetMask ());
	}
	~ApplyKeywords () {
		g_ShaderKeywords = prev;
	}
};

const ChannelAssigns* Material::SetPass (int passNo, int subshaderIndex, bool allowRecording)
{
	ApplyKeywords applyKeywords (m_ShaderKeywordSet);
	
	Shader *shader = m_Shader;
	if (!shader)
		shader = Shader::GetDefault();
	EnsurePropertiesExist();

	if (m_PropertiesDirty)
		InvalidateDisplayLists();

#if GFX_SUPPORTS_DISPLAY_LISTS

	// editor shader override won't work with recorded display lists
	#if UNITY_EDITOR
	if (g_EditorPixelShaderOverride)
		allowRecording = false;
	#endif

	CachedShaderPass* cachedPass = NULL;
	if (allowRecording && !m_PropertiesDirty)
	{
		if (subshaderIndex >= m_CachedSubShaders.size())
			m_CachedSubShaders.resize(subshaderIndex + 1);	
		CachedSubShader& subshader = m_CachedSubShaders[subshaderIndex];
		if (passNo >= subshader.passes.size())
			subshader.passes.resize_initialized(passNo + 1);
		cachedPass = &subshader.passes[passNo];
	}

	GfxDevice& device = GetGfxDevice();

	UInt64 shaderKeywords = g_ShaderKeywords.GetMask();
	using ShaderLab::g_GlobalFogMode;
	bool recording = false;
	if (cachedPass)
	{
		if (cachedPass->displayList &&
			cachedPass->shaderKeywords == shaderKeywords &&
			cachedPass->globalFogMode == g_GlobalFogMode)
		{
			cachedPass->displayList->Call();
			return cachedPass->channelAssigns;
		}
		SAFE_RELEASE(cachedPass->displayList);
		if (shader->CanPassBeRecorded(subshaderIndex, passNo))
			recording = device.BeginRecording();
	}

#endif

	const ChannelAssigns* channels;
	channels = shader->SetPass(subshaderIndex, passNo, m_StateKeyHash, &GetProperties());

#if GFX_SUPPORTS_DISPLAY_LISTS
	if (recording && device.EndRecording(&cachedPass->displayList))
	{
		cachedPass->channelAssigns = channels;
		cachedPass->shaderKeywords = shaderKeywords;
		cachedPass->globalFogMode = g_GlobalFogMode;
	}
#endif
	return channels;
}
	
const ChannelAssigns* Material::SetPassWithShader( int passNo, Shader* shader, int subshaderIndex )
{
	if (shader == m_Shader)
		return SetPass(passNo, subshaderIndex);

	ApplyKeywords applyKeywords (m_ShaderKeywordSet);	
	Assert(shader != NULL);
	return shader->SetPass( subshaderIndex, passNo, m_StateKeyHash, &GetProperties() );
}

const ChannelAssigns* Material::SetShadowCasterPassWithShader(Shader* shader, int subshaderIndex)
{
	if (shader == m_Shader)
		return SetShadowCasterPass(subshaderIndex);

	ApplyKeywords applyKeywords (m_ShaderKeywordSet);
	PROFILER_AUTO(gSetShadowCasterPass, this)
	ShaderLab::Pass* pass = shader->GetShadowCasterPassToUse(subshaderIndex);
	return pass->ApplyPass(m_StateKeyHash, &GetProperties());
}

const ChannelAssigns* Material::SetShadowCasterPass(int subshaderIndex)
{
	PROFILER_AUTO(gSetShadowCasterPass, this)

	ApplyKeywords applyKeywords (m_ShaderKeywordSet);
	Shader *shader = GetShader();
	ShaderLab::Pass* pass = shader->GetShadowCasterPassToUse(subshaderIndex);
	EnsurePropertiesExist();
	
#if GFX_SUPPORTS_DISPLAY_LISTS
	if (m_PropertiesDirty)
		InvalidateDisplayLists();

	UInt64 shaderKeywords = g_ShaderKeywords.GetMask();
	bool recording = false;
	bool allowRecording = true;

	// editor shader override won't work with recorded display lists
	#if UNITY_EDITOR
	if (g_EditorPixelShaderOverride)
		allowRecording = false;
	#endif

	GfxDevice& device = GetGfxDevice();
	CachedShaderPass* cachedPass = NULL;
	if (!m_PropertiesDirty && allowRecording)
	{
		if (subshaderIndex >= m_CachedSubShaders.size())
			m_CachedSubShaders.resize(subshaderIndex + 1);	
		
		CachedSubShader& subshader = m_CachedSubShaders[subshaderIndex];
		cachedPass = &subshader.shadowCasterPass;
		Assert (cachedPass != NULL);

		if (cachedPass->displayList &&
			cachedPass->shaderKeywords == shaderKeywords)
		{
			cachedPass->displayList->Call();
			return cachedPass->channelAssigns;
		}
		SAFE_RELEASE(cachedPass->displayList);
		if (pass->CanPassBeRecorded())
			recording = device.BeginRecording();
	}
#endif

	const ChannelAssigns* channels;
	channels = pass->ApplyPass(m_StateKeyHash, &GetProperties());

#if GFX_SUPPORTS_DISPLAY_LISTS
	if (recording && device.EndRecording(&cachedPass->displayList))
	{
		cachedPass->channelAssigns = channels;
		cachedPass->shaderKeywords = shaderKeywords;
	}
#endif

	return channels;
}
	
const ChannelAssigns* Material::SetShadowCollectorPassWithShader(Shader* shader, int subshaderIndex)
{
	if (shader == m_Shader)
		return SetShadowCollectorPass(subshaderIndex);

	ApplyKeywords applyKeywords (m_ShaderKeywordSet);
	PROFILER_AUTO(gSetShadowCollectorPass, this)
	ShaderLab::Pass* pass = shader->GetShadowCollectorPassToUse(subshaderIndex);
	return pass->ApplyPass(m_StateKeyHash, &GetProperties());
}

const ChannelAssigns* Material::SetShadowCollectorPass(int subshaderIndex)
{
	PROFILER_AUTO(gSetShadowCollectorPass, this)
	ApplyKeywords applyKeywords (m_ShaderKeywordSet);	
	Shader *shader = GetShader();
	ShaderLab::Pass* pass = shader->GetShadowCollectorPassToUse(subshaderIndex);
	EnsurePropertiesExist();

#if GFX_SUPPORTS_DISPLAY_LISTS
	if (m_PropertiesDirty)
		InvalidateDisplayLists();

	UInt64 shaderKeywords = g_ShaderKeywords.GetMask();
	bool recording = false;
	bool allowRecording = true;

	// editor shader override won't work with recorded display lists
#if UNITY_EDITOR
	if (g_EditorPixelShaderOverride)
		allowRecording = false;
#endif

	GfxDevice& device = GetGfxDevice();
	CachedShaderPass* cachedPass = NULL;
	if (!m_PropertiesDirty && allowRecording)
	{
		if (subshaderIndex >= m_CachedSubShaders.size())
			m_CachedSubShaders.resize(subshaderIndex + 1);	

		CachedSubShader& subshader = m_CachedSubShaders[subshaderIndex];
		cachedPass = &subshader.shadowCollectorPass;
		Assert (cachedPass != NULL);

		if (cachedPass->displayList &&
			cachedPass->shaderKeywords == shaderKeywords)
		{
			cachedPass->displayList->Call();
			return cachedPass->channelAssigns;
		}
		SAFE_RELEASE(cachedPass->displayList);
		if (pass->CanPassBeRecorded())
			recording = device.BeginRecording();
	}
#endif

	const ChannelAssigns* channels;
	channels = pass->ApplyPass(m_StateKeyHash, &GetProperties());

#if GFX_SUPPORTS_DISPLAY_LISTS
	if (recording && device.EndRecording(&cachedPass->displayList))
	{
		cachedPass->channelAssigns = channels;
		cachedPass->shaderKeywords = shaderKeywords;
	}
#endif
	return channels;
}	

const Shader *Material::GetShader() const
{
	const Shader *shader = m_Shader;
	if (shader)
		return shader;
	else
		return Shader::GetDefault();
}

PPtr<Shader> Material::GetShaderPPtr() const
{
	return m_Shader;
}


Shader *Material::GetShader()
{
	Shader *shader = m_Shader;
	if (shader)
		return shader;
	else
		return Shader::GetDefault();
}
std::string Material::GetTag( const string& tag, bool currentSubShaderOnly, const string& defaultValue ) const
{
	Shader* shader = m_Shader;
	if (!shader)
		return defaultValue;
	int tagValueID = shader->GetShaderLabShader()->GetTag (ShaderLab::GetShaderTagID(tag), currentSubShaderOnly);
	if (tagValueID < 0)
		return defaultValue;
	return ShaderLab::GetShaderTagName(tagValueID);
}

int Material::GetPassCount()
{
	Shader* shader = m_Shader;
	if( !shader )
		shader = Shader::GetDefault();
	return shader->GetShaderLabShader()->GetActiveSubShader().GetValidPassCount();
}

Material& Material::GetInstantiatedMaterial (Material* material, Object& renderer, bool allowInEditMode)
{
	if (material == NULL)
		material = GetDefaultDiffuseMaterial();

	if (material->m_Owner == PPtr<Object> (&renderer))
		return *material;
	else
	{
		if (!allowInEditMode && !IsWorldPlaying())
			ErrorStringObject("Instantiating material due to calling renderer.material during edit mode. This will leak materials into the scene. You most likely want to use renderer.sharedMaterial instead.", &renderer);

		Material* instance;        
        if (material->GetClassID()==ProceduralMaterial::GetClassIDStatic())
        {
            instance = static_cast<ProceduralMaterial*>(material)->Clone();
        }
        else
        {
            instance = CreateObjectFromCode<Material>();
        }

		instance->SetNameCpp (Append (material->GetName (), " (Instance)"));
		instance->m_Shader = material->m_Shader;
		instance->m_Owner = &renderer;
		if (material->m_Properties)
		{
			SET_ALLOC_OWNER(instance);
			SAFE_RELEASE_LABEL(instance->m_Properties, kMemShader);
			instance->m_Properties = UNITY_NEW(ShaderLab::PropertySheet (*material->m_Properties), kMemShader);
			instance->m_Properties->SetOwnerMaterial(instance);
		}
		instance->m_CustomRenderQueue = material->m_CustomRenderQueue;
		instance->m_SavedProperties = material->m_SavedProperties;
		if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
		{
			instance->m_ShaderKeywords = material->m_ShaderKeywords;
			instance->m_ShaderKeywordSet = material->m_ShaderKeywordSet;
		}
		instance->m_ShadowCollectorHash = material->m_ShadowCollectorHash;
		instance->m_ShadowCasterHash = material->m_ShadowCasterHash;
		instance->m_StateKeyHash = material->m_StateKeyHash;
		if( instance->m_Shader )
			instance->m_Shader->AddMaterialUser( instance->m_ShaderUserNode );
		return *instance;
	}
}



#if UNITY_EDITOR
inline void EmitWarningAboutSettingBuiltinParam(const char* name)
{
	static const std::string kWarningPrefix = std::string("Trying to set builtin parameter \"");
	static const std::string kWarningSuffix = std::string("\". Will be ignored.");

	WarningString( kWarningPrefix + name + kWarningSuffix );
}
#endif


void Material::SetColor (ShaderLab::FastPropertyName name, const ColorRGBAf &col)
{
#if UNITY_EDITOR
	if( IsVectorBuiltinParam(name.GetName()) )
		EmitWarningAboutSettingBuiltinParam(name.GetName());
#endif

	UnityPropertySheet::ColorMap::iterator i = m_SavedProperties.m_Colors.find (name);
	if( i != m_SavedProperties.m_Colors.end() )
	{
		#if UNITY_EDITOR
		if (i->second.NotEquals(col))
		#endif
		{
			i->second = col;
			SetDirty();
		}
	}
	
	ShaderLab::PropertySheet& properties = GetWritableProperties();
	if (properties.GetColorTag(name))
		properties.SetVector (name, GammaToActiveColorSpace(col).GetPtr());
	else
		properties.SetVector (name, col.GetPtr());

	UpdateHashesOnPropertyChange (name);
}


void Material::SetColorIndexed (ShaderLab::FastPropertyName name, int index, float value)
{
#if UNITY_EDITOR
	if( IsVectorBuiltinParam(name.GetName()) )
		EmitWarningAboutSettingBuiltinParam(name.GetName());
#endif

	UnityPropertySheet::ColorMap::iterator i = m_SavedProperties.m_Colors.find (name);
	if( i != m_SavedProperties.m_Colors.end() )
	{
		#if UNITY_EDITOR
		if (i->second.GetPtr()[index] != value)
		#endif
		{
			i->second.GetPtr()[index] = value;
			SetDirty();
		}
	}
	
	ShaderLab::PropertySheet& properties = GetWritableProperties();
	if (properties.GetColorTag(name))
		properties.SetVectorIndexed (name, index, GammaToActiveColorSpace(value));
	else
		properties.SetVectorIndexed (name, index, value);

	UpdateHashesOnPropertyChange (name);
}

ColorRGBAf Material::GetColor (ShaderLab::FastPropertyName name)
{
	EnsurePropertiesExist (); 

	const Vector4f* prop = m_Properties->FindVector(name);
	if( prop == NULL ) {
		AssertStringObject (Format ("Material doesn't have a color property '%s'", name.GetName()), this);
		return ColorRGBAf (0,0,0,0);
	}
	
	ColorRGBAf color (prop->x, prop->y, prop->z, prop->w);
	if (m_Properties->GetColorTag(name))
		return ActiveToGammaColorSpace (color);
	else	
		return color;
}


void Material::SetFloat (ShaderLab::FastPropertyName name, float val)
{
#if UNITY_EDITOR
	if( IsVectorBuiltinParam(name.GetName()) )
		EmitWarningAboutSettingBuiltinParam(name.GetName());
#endif

	UnityPropertySheet::FloatMap::iterator i = m_SavedProperties.m_Floats.find (name);
	if( i != m_SavedProperties.m_Floats.end() )
	{
		#if UNITY_EDITOR
		if (i->second != val)
		#endif
		{
			i->second = val;
			SetDirty();
		}
	}
	GetWritableProperties().SetFloat (name, val);

	UpdateHashesOnPropertyChange (name);
}

float Material::GetFloat (ShaderLab::FastPropertyName name)
{
	EnsurePropertiesExist ();
	
	const float* prop = m_Properties->FindFloat(name);
	if( prop == NULL ) {
		AssertStringObject (Format ("Material doesn't have a float or range property '%s'", name.GetName()), this);
		return 0;
	}
	return *prop;
}

bool Material::HasProperty (ShaderLab::FastPropertyName name)
{
	EnsurePropertiesExist (); 	
	return m_Properties->HasProperty(name);
}


void Material::ApplyMaterialPropertyDrawers ()
{
#	if UNITY_EDITOR
	// Don't try to apply property drawers for the very initial Reset()
	// that happens; the shader isn't set up yet anyway.
	if (!m_Shader.IsValid())
		return;
	if (!GetMonoManagerPtr())
		return;

	void* params[] = {Scripting::ScriptingWrapperFor(this)};
	CallStaticMonoMethod ("MaterialEditor", "ApplyMaterialPropertyDrawers", params);
#	endif // if UNITY_EDITOR
}


#if UNITY_EDITOR
bool Material::ActuallyHasTextureProperty (ShaderLab::FastPropertyName name) const
{
	return m_SavedProperties.m_TexEnvs.find (name) != m_SavedProperties.m_TexEnvs.end();
}


void Material::ResetDefaultTextures(bool overrideSetTextures)
{
	if( !m_Shader )
		return;

	const Shader::DefaultTexturesMap& defaultTextures = m_Shader->GetDefaultTextures();
	for (Shader::DefaultTexturesMap::const_iterator it = defaultTextures.begin(); it != defaultTextures.end(); ++it)
	{
		bool nullDefaultTexture = it->second.IsNull();
		
		for (int i = 0; i < m_Shader->GetPropertyCount (); i++)
		{
			const ShaderLab::ParserProperty* shaderProperty = m_Shader->GetPropertyInfo (i);
			
			// only assign if it is a texture where the name and dimensions match
			if (shaderProperty->m_Name != it->first.c_str()
				|| shaderProperty->m_Type != ShaderLab::ParserProperty::kTexture
				|| (!nullDefaultTexture && shaderProperty->m_DefTexture.m_TexDim != it->second->GetDimension ()))
				continue;
			
			if (!overrideSetTextures)
			{
				if (GetTexture(ShaderLab::Property(it->first)) != NULL)
					continue;
			}

			SetTexture (ShaderLab::Property(it->first), it->second);
			break;
		}
	}
}
#endif


// Get/Set a matrix value in the material
// We don't allow user editing of matrix props, hence they don't have a representation in the 
// UnityPropertySheet, hence we work directly with the SL properties here
void Material::SetMatrix (ShaderLab::FastPropertyName name, const Matrix4x4f &val)
{
#if UNITY_EDITOR
	if( IsMatrixBuiltinParam(name.GetName()) )
		EmitWarningAboutSettingBuiltinParam(name.GetName());
#endif

	GetWritableProperties().SetValueProp (name, 16, val.GetPtr());

	UpdateHashesOnPropertyChange (name);
}

const Matrix4x4f &Material::GetMatrix (ShaderLab::FastPropertyName name)
{
	EnsurePropertiesExist ();
	
	int count = 0;
	const float* prop = m_Properties->GetValueProp (name, &count);
	if (prop == NULL || count != 16)
	{
		AssertStringObject (Format ("Material doesn't have a matrix property '%s'", name.GetName()), this);
		return Matrix4x4f::identity;
	}
	return *reinterpret_cast<const Matrix4x4f*>(prop);
}


void Material::SetTexture (ShaderLab::FastPropertyName name, Texture *val)
{
	UnityPropertySheet::TexEnvMap::iterator i = m_SavedProperties.m_TexEnvs.find (name);
	if( i != m_SavedProperties.m_TexEnvs.end() )
	{
		#if UNITY_EDITOR
		if (i->second.m_Texture != PPtr<Texture>(val))
		#endif
		{
			i->second.m_Texture = val;
			SetDirty();
		}
	}
	else
	{
		//AssertStringObject (Format ("Material '%s' doesn't have a texture property '%s'", GetName(), name.GetName()), this);
	}
	
	GetWritableProperties().SetTexture (name, val);

	UpdateHashesOnPropertyChange (name);
}


Texture *Material::GetTexture (ShaderLab::FastPropertyName name)
{
	EnsurePropertiesExist ();
	
	UnityPropertySheet::TexEnvMap::const_iterator i = m_SavedProperties.m_TexEnvs.find (name);
	if (i == m_SavedProperties.m_TexEnvs.end())
	{
		AssertStringObject (Format ("Material doesn't have a texture property '%s'", name.GetName()), this);
		return 0;
	}
	return PPtr<Texture> (i->second.m_Texture);
}

void Material::SetTextureOffset( ShaderLab::FastPropertyName name, const Vector2f& val )
{
	UnityPropertySheet::TexEnvMap::iterator i = m_SavedProperties.m_TexEnvs.find (name);
	if( i != m_SavedProperties.m_TexEnvs.end() )
	{
		#if UNITY_EDITOR
		if (i->second.m_Offset != val)
		#endif
		{
			i->second.m_Offset.Set( val.x, val.y );
			SetDirty();
		}
	}
	GetWritableProperties().SetTextureOffset( name, val.x, val.y );

	UpdateHashesOnPropertyChange (name);
}

void Material::SetTextureScale( ShaderLab::FastPropertyName name, const Vector2f& val )
{
	UnityPropertySheet::TexEnvMap::iterator i = m_SavedProperties.m_TexEnvs.find (name);
	if( i != m_SavedProperties.m_TexEnvs.end() )
	{
		#if UNITY_EDITOR
		if (i->second.m_Scale != val)
		#endif
		{
			i->second.m_Scale.Set( val.x, val.y );
			SetDirty();
		}
	}
	GetWritableProperties().SetTextureScale( name, val.x, val.y );

	UpdateHashesOnPropertyChange (name);
}

void Material::SetTextureScaleAndOffsetIndexed( ShaderLab::FastPropertyName name, int index, float value)
{
	UnityPropertySheet::TexEnvMap::iterator i = m_SavedProperties.m_TexEnvs.find (name);
	if( i != m_SavedProperties.m_TexEnvs.end() )
	{
		if (index < 2)
		{
			i->second.m_Scale[index] = value;
		}
		else
		{
			i->second.m_Offset[index - 2] = value;
		}
		SetDirty();
	}
	GetWritableProperties().SetTextureScaleAndOffsetIndexed( name, index, value );

	UpdateHashesOnPropertyChange (name);
}

Vector2f Material::GetTextureOffset( ShaderLab::FastPropertyName name )
{
	// get from current runtime sheet
	const ShaderLab::TexEnv* texEnv = GetProperties().GetTexEnv (name);
	if (texEnv != NULL)
		return texEnv->GetTextureOffset	();

	// if not there, get from serialized properties
	UnityPropertySheet::TexEnvMap::const_iterator it = m_SavedProperties.m_TexEnvs.find (name);
	if (it != m_SavedProperties.m_TexEnvs.end())
		return it->second.m_Offset;

	AssertStringObject (Format ("Material doesn't have a texture property '%s'", name.GetName()), this);
	return Vector2f(0,0);
}

Vector2f Material::GetTextureScale( ShaderLab::FastPropertyName name )
{
	// get from current runtime sheet
	const ShaderLab::TexEnv* texEnv = GetProperties().GetTexEnv (name);
	if (texEnv != NULL)
		return texEnv->GetTextureScale ();

	// if not there, get from serialized properties
	UnityPropertySheet::TexEnvMap::const_iterator it = m_SavedProperties.m_TexEnvs.find (name);
	if (it != m_SavedProperties.m_TexEnvs.end())
		return it->second.m_Scale;

	AssertStringObject (Format ("Material doesn't have a texture property '%s'", name.GetName()), this);
	return Vector2f(1,1);
}

void Material::SetComputeBuffer (ShaderLab::FastPropertyName name, ComputeBufferID val)
{
	GetWritableProperties().SetComputeBuffer (name, val);
}

void Material::CopyPropertiesFromMaterial(Material& other)
{
	m_SavedProperties = other.GetSavedProperties(); 
	SAFE_RELEASE_LABEL(m_Properties, kMemShader);
	{
		SET_ALLOC_OWNER(this);
		m_Properties = UNITY_NEW(ShaderLab::PropertySheet(other.GetProperties()), kMemShader);
		m_Properties->SetOwnerMaterial(this);
	}
	m_PropertiesDirty = true;

	Shader *shader = m_Shader;
	if( !shader )
		shader = Shader::GetDefault();

	UpdateHashes();
}

void Material::ClearProperties ()
{
	SAFE_RELEASE_LABEL(m_Properties, kMemShader);
	m_PropertiesDirty = true;
	m_ShaderUserNode.RemoveFromList();
}

int Material::GetRuntimeMemorySize() const
{	int size = Super::GetRuntimeMemorySize();
	if (m_Properties)	
		size += m_Properties->GetMemoryUsage();
	return size;
}

void Material::InvalidateDisplayLists()
{
	int subShaderCount = m_CachedSubShaders.size();
	for (int i = 0; i < subShaderCount; i++)
	{
		CachedSubShader& subshader = m_CachedSubShaders[i];
		int passCount = subshader.passes.size();
		for (int j = 0; j < passCount; j++)
		{
			CachedShaderPass& pass = subshader.passes[j];
			SAFE_RELEASE(pass.displayList);
		}
		SAFE_RELEASE(subshader.shadowCasterPass.displayList);
		SAFE_RELEASE(subshader.shadowCollectorPass.displayList);
	}
	m_PropertiesDirty = false;
}

void Material::BuildShaderKeywordSet ()
{
	m_ShaderKeywordSet.Reset ();
	for (size_t q = 0; q < m_ShaderKeywords.size (); ++q)
		m_ShaderKeywordSet.Enable (keywords::Create (m_ShaderKeywords[q]));
}
	
void Material::SetShaderKeywords (const ShaderKeywordsT& keywords)
{
	m_ShaderKeywords = keywords;
	BuildShaderKeywordSet ();
	UpdateHashes();
	SetDirty();
}

void Material::EnableKeyword (const std::string& key)
{
	// already set -> do nothing
	if (std::find(m_ShaderKeywords.begin(), m_ShaderKeywords.end(), key) != m_ShaderKeywords.end())
		return;
	m_ShaderKeywords.push_back (key);
	BuildShaderKeywordSet ();
	UpdateHashes();
	SetDirty();
}

void Material::DisableKeyword (const std::string& key)
{
	// no keyword -> do nothing
	ShaderKeywordsT::iterator it = std::find(m_ShaderKeywords.begin(), m_ShaderKeywords.end(), key);
	if (it == m_ShaderKeywords.end())
		return;

	m_ShaderKeywords.erase (it);
	BuildShaderKeywordSet ();
	UpdateHashes();
	SetDirty();
}


}

IMPLEMENT_CLASS (Material)
IMPLEMENT_OBJECT_SERIALIZE (Material)
INSTANTIATE_TEMPLATE_TRANSFER (Material)
