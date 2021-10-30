#include "UnityPrefix.h"
#include "RenderSettings.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Light.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

static SHADERPROP (LightTextureB0);
static SHADERPROP (HaloFalloff);


RenderSettings::RenderSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_LinearFogStart = 0; m_LinearFogEnd = 300; m_FogDensity = .01f;
	m_Fog = false;
	m_FogMode = kFogExp2;
	m_FlareFadeSpeed = 3.0f;
}


void RenderSettings::Reset ()
{
	Super::Reset();

	m_FogColor.Set (.5,.5,.5,1);
	m_LinearFogStart = 0; m_LinearFogEnd = 300; m_FogDensity = .01f;
	m_Fog = false;
	m_FogMode = kFogExp2;
	m_AmbientLight.Set (.2f, .2f, .2f, 1);
	m_FlareStrength = 1.0f;
	m_FlareFadeSpeed = 3.0f;
	m_HaloStrength = .5f;
}

float RenderSettings::CalcFogFactor (float distance) const
{
	if (m_Fog)
		return 1.0F-exp(-(m_FogDensity * m_FogDensity * distance * distance));
	
	return 0.0F;
}

Texture2D* RenderSettings::GetDefaultSpotCookie()
{
	Texture2D* tex = m_SpotCookie;
	if (tex)
		return tex;
	else
	{
		static PPtr<Texture2D> fallback;
		if (!fallback)
			fallback = GetBuiltinResource<Texture2D> ("Soft.psd");
		return fallback;
	}
}

RenderSettings::~RenderSettings ()
{
}

void RenderSettings::SetupAmbient () const
{
	ColorRGBAf amb = GetAmbientLightInActiveColorSpace() * 0.5F;
	GetGfxDevice().SetAmbient( amb.GetPtr() );
}

void RenderSettings::AwakeFromLoad (AwakeFromLoadMode awakeMode) {
	Super::AwakeFromLoad (awakeMode);

	ShaderLab::g_GlobalProperties->SetTexture (kSLPropLightTextureB0, builtintex::GetAttenuationTexture());

	ApplyFog();
	ApplyHaloTexture();
	
	// disable light 0 because GL state has it on by default (or something like that)
	// This handles the wacky case where we have 0 vertex lights and a vertex lit shader (DOH!).
	GetGfxDevice().DisableLights (0);
	
	if ((awakeMode & kDidLoadFromDisk) == 0)
		ApplyFlareAndHaloStrength ();
}

void RenderSettings::ApplyFlareAndHaloStrength ()
{
	// if we are editing from inside the editor, the Halo values might have changed,
	// So all lights need to update their halos
	std::vector<SInt32> obj;
	Object::FindAllDerivedObjects(ClassID (Light), &obj);
	for (std::vector<SInt32>::iterator i = obj.begin(); i != obj.end(); i++) {
		Light *l = PPtr<Light> (*i);
		l->SetupHalo ();
		l->SetupFlare ();
	}
}

void RenderSettings::CheckConsistency () {
	m_FogDensity = std::min (std::max (m_FogDensity, 0.0f), 1.0f);
	m_HaloStrength = std::min (std::max (m_HaloStrength, 0.0f), 1.0f);
}

void RenderSettings::ApplyHaloTexture()
{
	Texture2D *tex = m_HaloTexture;	
	ShaderLab::g_GlobalProperties->SetTexture (kSLPropHaloFalloff, tex?tex:builtintex::GetHaloTexture());
}

void RenderSettings::ApplyFog ()
{
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();
	ShaderLab::g_GlobalFogMode = m_Fog ? static_cast<FogMode>(m_FogMode) : kFogDisabled;
	float fogDensity = m_FogDensity;
	float fogStart = m_LinearFogStart;
	float fogEnd = m_LinearFogEnd;
	if (ShaderLab::g_GlobalFogMode == kFogDisabled)
	{
		fogDensity = 0.0f;
		fogStart = 10000.0f;
		fogEnd = 20000.0f;
	}

	params.SetVectorParam(kShaderVecUnityFogStart, Vector4f(fogStart, fogStart, fogStart, fogStart));
	params.SetVectorParam(kShaderVecUnityFogEnd, Vector4f(fogEnd, fogEnd, fogEnd, fogEnd));
	params.SetVectorParam(kShaderVecUnityFogDensity, Vector4f(fogDensity, fogDensity, fogDensity, fogDensity));
	
	ColorRGBAf fogColor = GammaToActiveColorSpace(m_FogColor);
	params.SetVectorParam(kShaderVecUnityFogColor, Vector4f(fogColor.GetPtr()));
	
}

template<class TransferFunc>
void RenderSettings::Transfer (TransferFunc& transfer) {
	TRANSFER_SIMPLE (m_Fog);	
	transfer.Align();	
	TRANSFER_SIMPLE (m_FogColor);
	TRANSFER_SIMPLE (m_FogMode);
	TRANSFER_SIMPLE (m_FogDensity);
	TRANSFER_SIMPLE (m_LinearFogStart);
	TRANSFER_SIMPLE (m_LinearFogEnd);
	TRANSFER_SIMPLE (m_AmbientLight);
	TRANSFER_SIMPLE (m_SkyboxMaterial);
	TRANSFER (m_HaloStrength);
	TRANSFER (m_FlareStrength);
	TRANSFER (m_FlareFadeSpeed);
	TRANSFER (m_HaloTexture);
	TRANSFER (m_SpotCookie);
	Super::Transfer (transfer);
}

void RenderSettings::InitializeClass ()
{
	RegisterAllowNameConversion(GetClassStringStatic (), "m_Ambient", "m_AmbientLight");
	LightManager::InitializeClass();
}

void RenderSettings::PostInitializeClass ()
{
	builtintex::GenerateBuiltinTextures();
}

void RenderSettings::CleanupClass()
{
	LightManager::CleanupClass();
}

void RenderSettings::SetSkyboxMaterial (Material *mat) {
	m_SkyboxMaterial = mat;
	SetDirty();	
}

void RenderSettings::SetFogColor (const ColorRGBAf& color)
{
	m_FogColor = color;
	SetDirty();
	ApplyFog ();
}

void RenderSettings::SetUseFog (bool fog)
{
	m_Fog = fog;
	SetDirty();
	ApplyFog ();
} 

void RenderSettings::SetUseFogNoDirty (bool fog)
{
	m_Fog = fog;
	ApplyFog ();
} 

void RenderSettings::SetFogMode (FogMode fm)
{
	m_FogMode = fm;
	SetDirty();
	ApplyFog ();
} 

void RenderSettings::SetFogDensity (float fog)
{
	m_FogDensity = fog;
	SetDirty();
	ApplyFog ();
}

void RenderSettings::SetLinearFogStart (float d)
{
	m_LinearFogStart = d;
	SetDirty();
	ApplyFog ();
}

void RenderSettings::SetLinearFogEnd (float d)
{
	m_LinearFogEnd = d;
	SetDirty();
	ApplyFog ();
}

void RenderSettings::SetAmbientLight (const ColorRGBAf &col)
{
	m_AmbientLight = col;
	ApplyFog ();
	SetDirty();
}

#if UNITY_EDITOR
void RenderSettings::SetAmbientLightNoDirty (const ColorRGBAf &col)
{
	m_AmbientLight = col;
	ApplyFog ();
}
#endif

void RenderSettings::SetFlareStrength (float flare)
{
	m_FlareStrength = flare;
	SetDirty();
	ApplyFlareAndHaloStrength();
}

void RenderSettings::SetFlareFadeSpeed (float time)
{
	m_FlareFadeSpeed = time;
	SetDirty();
	ApplyFlareAndHaloStrength();
}

void RenderSettings::SetHaloStrength (float halo)
{
	m_HaloStrength = halo;
	SetDirty();
	ApplyFlareAndHaloStrength();
}

IMPLEMENT_CLASS_HAS_POSTINIT (RenderSettings)
IMPLEMENT_OBJECT_SERIALIZE (RenderSettings)
GET_MANAGER (RenderSettings)
