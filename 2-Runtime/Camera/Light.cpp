#include "UnityPrefix.h"
#include "Light.h"
#include "Shadows.h"
#include "RenderSettings.h"
#include "HaloManager.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/CubemapTexture.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderKeywords.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Camera/LightManager.h"
#if UNITY_EDITOR
#include "Runtime/Misc/BuildSettings.h"
#endif



using namespace Unity;


static SHADERPROP (LightTexture0);

const UInt64 kAllLightKeywordsMask = 0x1F;


// constants for opengl attenuation
static const float kConstantFac = 1.000f;
static const float kQuadraticFac = 25.0f;
// where the falloff down to zero should start
static const float kToZeroFadeStart = 0.8f * 0.8f;

float Light::CalcQuadFac (float range)
{
	return kQuadraticFac / (range * range);
}

float Light::AttenuateNormalized(float distSqr)
{
	// match the vertex lighting falloff
	float atten = 1 / (kConstantFac + CalcQuadFac (1.0f) * distSqr);

	// ...but vertex one does not falloff to zero at light's range; it falls off to 1/26 which
	// is then doubled by our shaders, resulting in 19/255 difference!
	// So force it to falloff to zero at the edges.
	if( distSqr >= kToZeroFadeStart )
	{
		if( distSqr > 1 )
			atten = 0;
		else
			atten *= 1 - (distSqr - kToZeroFadeStart) / (1 - kToZeroFadeStart);
	}

	return atten;
}

Light::Light(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_GfxLightValid(false)
,	m_ActuallyLightmapped(false)
{
	m_KeywordMode = kLightKeywordDirectional;
	m_HaloHandle = 0;
	m_FlareHandle = -1;
	m_World2Local = Matrix4x4f::identity;
	m_WorldPosition = Vector3f::zero;
}

Light::~Light ()
{
}

void Light::InitializeClass () {
	REGISTER_MESSAGE_VOID (Light, kTransformChanged, TransformChanged);
}

void Light::CleanupClass ()
{
}

float Light::AttenuateApprox (float sqrDist) const
{
	return 1.0f / (kConstantFac + CalcQuadFac(m_Range) * sqrDist);
}


void Light::TransformChanged ()
{
	if (IsAddedToManager ())
	{
		const Transform& transform = GetComponent(Transform);
		m_World2Local = transform.GetWorldToLocalMatrixNoScale ();
		m_WorldPosition = transform.GetPosition ();
		Precalc ();
	}
	m_GfxLightValid = false;
}

Light::Lightmapping Light::GetLightmappingForBake() const
{
	if (m_Type == kLightArea)
		return kLightmappingBakedOnly;

	return static_cast<Lightmapping>(m_Lightmapping);
}

void Light::Reset ()
{
	Super::Reset();
	m_Shadows.Reset();
	
	m_Color = ColorRGBAf (1,1,1,1);
	m_Intensity = 1.0f;
	m_Range = 10.0f;
	m_SpotAngle = 30.0f;
	m_CookieSize = 10.0f;
	m_Lightmapping = kLightmappingAuto;
	UpdateSpotAngleValues ();
	m_RenderMode = kRenderAuto;
	m_DrawHalo = false;
	m_Type = kLightPoint;
	m_CullingMask.m_Bits = -1;
	#if UNITY_EDITOR
	m_ShadowSamples = 1;
	m_ShadowRadius = 0.0f;
	m_ShadowAngle = 0.0f;
	m_IndirectIntensity = 1.0f;
	m_AreaSize.Set (1,1);
	#endif
}

void Light::CheckConsistency ()
{
	Texture *cookie = m_Cookie;
	// If this is a point light and cookie is not a cubemap, remove the cookie
	if( m_Type == kLightPoint && cookie && cookie->GetClassID () != ClassID (Cubemap) )
	{
		m_Cookie = NULL;
		cookie = NULL;
	}
	
	// If this is not a point light and cookie is a cubemap, remove the cookie
	if( m_Type != kLightPoint && cookie && cookie->GetClassID () == ClassID (Cubemap) )
	{
		m_Cookie = NULL;
		cookie = NULL;
	}
	
	// I think this is to get cookie-to-cubemap working on Radeon 7000 path. Enforcing cookies
	// to be square makes constructing cubemap much easier.
	if( m_Type == kLightSpot && cookie && cookie->GetDataHeight() != cookie->GetDataWidth() )
	{
		ErrorStringObject ("Spotlight cookies must be square (width and height must be equal)", this);
		m_Cookie = 0;
	}

	m_Range = std::max (m_Range, 0.0f);
	m_SpotAngle = std::min (m_SpotAngle, 179.0f);
	m_SpotAngle = std::max (m_SpotAngle, 1.0f);	
	m_CookieSize = std::max (m_CookieSize, 0.0f);
	m_Shadows.m_Bias = clamp (m_Shadows.m_Bias, 0.0f, 10.0f);
	m_Shadows.m_Softness = clamp (m_Shadows.m_Softness, 1.0f, 8.0f);
	m_Shadows.m_SoftnessFade = clamp (m_Shadows.m_SoftnessFade, 0.1f, 5.0f);
	#if UNITY_EDITOR
	m_ShadowSamples = std::max<int> (m_ShadowSamples, 1);
	m_IndirectIntensity = std::max (m_IndirectIntensity, 0.0f);
	m_AreaSize.x = std::max(m_AreaSize.x, 0.0f);
	m_AreaSize.y = std::max(m_AreaSize.y, 0.0f);
	#endif
}

void Light::AddToManager ()
{
	DebugAssert (!IsInList());
	const Transform& transform = GetComponent(Transform);
	m_World2Local = transform.GetWorldToLocalMatrixNoScale ();
	m_WorldPosition = transform.GetPosition ();
	GetLightManager().AddLight(this);
	
	SetupHalo ();
	SetupFlare ();
}

void Light::RemoveFromManager ()
{
	if (IsInList())
	{
		GetLightManager().RemoveLight (this);
	}
	if (m_HaloHandle) {
		GetHaloManager().DeleteHalo (m_HaloHandle);
		m_HaloHandle = 0;
	}
	if (m_FlareHandle != -1)
	{
		GetFlareManager ().DeleteFlare (m_FlareHandle);
		m_FlareHandle = -1;
	}
}

void Light::Precalc ()
{	
	// setup the light cookie/attenuation textures
	Texture *cookie = m_Cookie;
	switch (m_Type)
	{
	case kLightSpot: 
		if (!cookie) 
			cookie = GetRenderSettings().GetDefaultSpotCookie();
		
		m_AttenuationTexture = cookie;
		m_AttenuationMode = kSpotCookie;
		m_KeywordMode = kLightKeywordSpot;
		break;
		
	case kLightPoint:
		if (cookie) {
			m_AttenuationTexture = cookie;
			m_AttenuationMode = kPointFalloff;
			m_KeywordMode = kLightKeywordPointCookie;
		} else {
			m_AttenuationTexture = builtintex::GetAttenuationTexture();
			m_AttenuationMode = kPointFalloff;
			m_KeywordMode = kLightKeywordPoint;
		}
		break;
		
	case kLightDirectional:
		if (cookie)
		{
			m_AttenuationTexture = cookie;
			m_AttenuationMode = kDirectionalCookie;
			m_KeywordMode = kLightKeywordDirectionalCookie;
		}
		else
		{
			m_AttenuationTexture = NULL;
			m_AttenuationMode = kUnused;
			m_KeywordMode = kLightKeywordDirectional;
		}
		break;
	}

	m_ConvertedFinalColor = GammaToActiveColorSpace (m_Color) * m_Intensity;
	
	UpdateSpotAngleValues ();
	
	SetupHalo();
	SetupFlare();
}




void Light::SetPropsToShaderLab (float blend) const
{
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();

	params.SetVectorParam(kShaderVecLightColor0, Vector4f((m_ConvertedFinalColor * blend).GetPtr()));
	Texture* attenTex = m_AttenuationTexture;
	if (attenTex)
	{
		ShaderLab::PropertySheet* probs = ShaderLab::g_GlobalProperties;
		probs->SetTexture (kSLPropLightTexture0, attenTex);
	}
}

void Light::SetLightKeyword()
{
	UInt64 mask = g_ShaderKeywords.GetMask();
	mask &= ~kAllLightKeywordsMask;
	mask |= 1ULL << m_KeywordMode;
	g_ShaderKeywords.SetMask (mask);
}

void Light::GetMatrix (const Matrix4x4f* __restrict object2light, Matrix4x4f* __restrict outMatrix) const
{
	Matrix4x4f temp1, temp2, temp3;
	float scale;
	
	switch (m_AttenuationMode) {
	case kSpotCookie:
		// we want out.w = 2.0 * in.z / m_CotanHalfSpotAngle
		// c = m_CotanHalfSpotAngle
		// 1 0 0 0
		// 0 1 0 0
		// 0 0 1 0
		// 0 0 2/c 0
		// the "2" will be used to scale .xy for the cookie as in .xy/2 + 0.5 
		temp3.SetIdentity();
		temp3.Get(3,2) = 2.0f / m_CotanHalfSpotAngle;
		temp3.Get(3,3) = 0;

		scale = 1.0f / m_Range;
		temp1.SetScale (Vector3f(scale,scale,scale));
			
		// temp3 * temp1 * object2Light
		MultiplyMatrices4x4 (&temp3, &temp1, &temp2);
		MultiplyMatrices4x4 (&temp2, object2light, outMatrix);
		break;
	case kPointFalloff:
		scale = 1.0f / m_Range;
		temp1.SetScale (Vector3f(scale,scale,scale));
		MultiplyMatrices4x4 (&temp1, object2light, outMatrix);
		break;
	case kDirectionalCookie:
		scale = 1.0f / m_CookieSize;
		temp1.SetScale (Vector3f (scale, scale, 0));
		temp2.SetTranslate (Vector3f (.5f, .5f, 0));
		// temp2 * temp1 * object2Light
		MultiplyMatrices4x4 (&temp2, &temp1, &temp3);
		MultiplyMatrices4x4 (&temp3, object2light, outMatrix);
		break;
	case kUnused:
		break;
	}
}



void Light::ComputeGfxLight (GfxVertexLight& gfxLight) const
{	
	gfxLight.type = static_cast<LightType>(m_Type);
	
	const Transform& tr = GetComponent(Transform);
	switch( m_Type ) {
		case kLightPoint:
		{
			Vector3f lightPos = tr.GetPosition();
			gfxLight.position.Set( lightPos.x, lightPos.y, lightPos.z, 1.0f );
			gfxLight.spotAngle = -1.0f;
			gfxLight.quadAtten = CalcQuadFac(m_Range);
			gfxLight.spotDirection.Set( 1.0f, 0.0f, 0.0f, 0.0f );
		}
			break;
		case kLightDirectional:
		{
			Vector3f lightDir = tr.TransformDirection( Vector3f (0,0,1) );
			gfxLight.position.Set( lightDir.x, lightDir.y, lightDir.z, 0.0f );
			gfxLight.quadAtten = 0.0f;
			gfxLight.spotAngle = -1.0f;
			gfxLight.spotDirection.Set( 1.0f, 0.0f, 0.0f, 0.0f );
		}
			break;
		case kLightSpot:
		{
			Vector3f lightPos = tr.GetPosition();
			gfxLight.position.Set( lightPos.x, lightPos.y, lightPos.z, 1.0f );
			Vector3f lightDir = tr.TransformDirection (Vector3f (0,0,1));
			gfxLight.spotDirection.Set( lightDir.x, lightDir.y, lightDir.z, 0.0f );
			gfxLight.spotAngle = m_SpotAngle;
			gfxLight.quadAtten = CalcQuadFac(m_Range);
		}
			break;
		case kLightArea:
			break;
		default:
			ErrorStringObject( "Unsupported light type", this );
	}
	
	// Light color & range
	gfxLight.color.Set( m_ConvertedFinalColor.GetPtr() );
	gfxLight.range = m_Range;
}

void Light::SetupVertexLight (int lightNo, float visibilityFade)
{
	if (!m_GfxLightValid)
	{
		ComputeGfxLight (m_CachedGfxLight);
		m_GfxLightValid = true;
	}	
	GfxDevice& device = GetGfxDevice();
	
	const ColorRGBAf color = GetConvertedFinalColor();
	Vector4f fadedColor;
	fadedColor.x = color.r * visibilityFade;
	fadedColor.y = color.g * visibilityFade;
	fadedColor.z = color.b * visibilityFade;

	m_CachedGfxLight.color = fadedColor;
	device.SetLight (lightNo, m_CachedGfxLight);
}


void Light::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	if ((awakeMode & kDidLoadFromDisk) == 0 && GetEnabled () && IsActive ())
	{
		const Transform& transform = GetComponent(Transform);
		m_World2Local = transform.GetWorldToLocalMatrixNoScale ();
		m_WorldPosition = transform.GetPosition ();
		SetupHalo ();
		SetupFlare ();
	}
	m_GfxLightValid = false;
	Precalc ();
}

void Light::SetFlare (Flare *flare)
{
	if (m_Flare == PPtr<Flare> (flare))
		return;
	m_Flare = flare;
	if (GetEnabled () && IsActive ())
		SetupFlare();
}

void Light::SetType (LightType type)
{
	m_Type = type;
	SetDirty(); m_GfxLightValid = false; Precalc();
}

void Light::SetColor (const ColorRGBAf& c)
{
	m_Color = c;
	SetDirty(); m_GfxLightValid = false; Precalc();
}

void Light::SetIntensity( float i )
{
	m_Intensity = clamp(i, 0.0f, 8.0f);
	SetDirty(); m_GfxLightValid = false; Precalc();
}

int Light::GetFinalShadowResolution() const
{
	int lightShadowResolution = GetShadowResolution();
	if (lightShadowResolution == -1) // use global resolution?
	{
		const QualitySettings::QualitySetting& quality = GetQualitySettings().GetCurrent();
		lightShadowResolution = quality.shadowResolution;;
	}
	return lightShadowResolution;
}

void Light::SetShadows( int v )
{
	m_Shadows.m_Type = v;
	SetDirty();
}

void Light::SetActuallyLightmapped (bool v)
{
	if (m_ActuallyLightmapped != v)
	{
		m_ActuallyLightmapped = v;
		SetDirty();
		m_GfxLightValid = false;
	}
}

void Light::SetCookie (Texture *tex)
{
	if (m_Cookie == PPtr<Texture> (tex))
		return;
		
	m_Cookie = tex;
	SetDirty();
	CheckConsistency ();
	Precalc ();
}

template<class TransferFunc>
void Light::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	transfer.SetVersion(3);
	
	TRANSFER_SIMPLE (m_Type);
	TRANSFER_SIMPLE (m_Color);
	TRANSFER (m_Intensity);
	TRANSFER_SIMPLE (m_Range);
	TRANSFER_SIMPLE (m_SpotAngle);
	if (transfer.IsVersionSmallerOrEqual(2))
	{
		m_CookieSize = m_SpotAngle * 2.0f;
	}
	else
	{
		TRANSFER (m_CookieSize);
	}
	
	#if UNITY_EDITOR
	if (transfer.IsVersionSmallerOrEqual(1))
	{
		transfer.Transfer(m_Shadows.m_Type, "m_Shadows");
		transfer.Transfer(m_Shadows.m_Resolution, "m_ShadowResolution");
		transfer.Transfer(m_Shadows.m_Strength, "m_ShadowStrength");
	}
	else
	{
		transfer.Transfer(m_Shadows, "m_Shadows");
	}
	#else
	transfer.Transfer(m_Shadows, "m_Shadows");
	#endif
	
	TRANSFER (m_Cookie);
	transfer.Transfer (m_DrawHalo, "m_DrawHalo", kSimpleEditorMask);
	transfer.Transfer (m_ActuallyLightmapped, "m_ActuallyLightmapped", kDontAnimate);
	transfer.Align();
	TRANSFER (m_Flare);
	TRANSFER (m_RenderMode);
	TRANSFER (m_CullingMask);
	TRANSFER (m_Lightmapping);
	
	TRANSFER_EDITOR_ONLY (m_ShadowSamples);
	TRANSFER_EDITOR_ONLY (m_ShadowRadius);
	TRANSFER_EDITOR_ONLY (m_ShadowAngle);
	TRANSFER_EDITOR_ONLY (m_IndirectIntensity);
	TRANSFER_EDITOR_ONLY (m_AreaSize);
}

void Light::SetupHalo () {
	if( m_DrawHalo && IsActive() && GetEnabled() )
	{
		float haloStr = GetRenderSettings().GetHaloStrength();
		if (!m_HaloHandle)
			m_HaloHandle = GetHaloManager().AddHalo();
		
		if (m_HaloHandle) {
			///@TODO: Handle color conversion.
			GetHaloManager().UpdateHalo( m_HaloHandle, GetComponent(Transform).GetPosition(), m_Color * (haloStr * m_Intensity * m_Color.a), haloStr * m_Range, GetGameObject().GetLayerMask() );
		} 
	} else {
		if (m_HaloHandle) {
			GetHaloManager().DeleteHalo (m_HaloHandle);
			m_HaloHandle = 0;
		}
	} 
}

void Light::SetupFlare ()
{
	Flare *flare = m_Flare;
	if (!flare || !IsActive() || !GetEnabled())
	{
		if (m_FlareHandle != -1)
		{
			GetFlareManager().DeleteFlare (m_FlareHandle);
			m_FlareHandle = -1;
		}
		return;		
	}
	
	bool inf;
	Vector3f pos;
	
	if (m_Type != kLightDirectional)
	{
		pos = GetComponent(Transform).GetPosition();
		inf = false;
	}
	else
	{
		pos = GetComponent(Transform).TransformDirection (Vector3f (0,0,1));
		inf = true; 
	}

	if (m_FlareHandle == -1)
		m_FlareHandle = GetFlareManager().AddFlare ();
	GetFlareManager().UpdateFlare(
		m_FlareHandle,
		flare,
		pos,
		inf,
		GetRenderSettings().GetFlareStrength(),
		m_ConvertedFinalColor,
		GetRenderSettings().GetFlareFadeSpeed(),
		GetGameObject().GetLayerMask(),
		kNoFXLayerMask|kIgnoreRaycastMask
	);
}


bool Light::IsValidToRender() const
{
	// Spot lights with range lower than a pretty high value of 0.001f have to be culled already,
	// as the code extracting projection planes for culling isn't smart enough to handle smaller values.
	return !((m_Type == kLightSpot && (m_Range < 0.001f || m_SpotAngle < 0.001f)) ||
			 (m_Type == kLightPoint && m_Range < 0.00000001f));
}


IMPLEMENT_CLASS_HAS_INIT (Light)
IMPLEMENT_OBJECT_SERIALIZE (Light)

/// @TODO: Hack and should be removed before 2.0
void SetupVertexLights(const std::vector<Light*>& lights)
{
	GfxDevice& device = GetGfxDevice();
	
	/// @TODO: .a is multiplied differently only here. This is very inconsistent
	/// Someone with guts please fix it or get rid of SetupVertexLights codepath completely.
	ColorRGBAf ambient = GetRenderSettings().GetAmbientLightInActiveColorSpace();
	ambient *= ColorRGBAf(0.5F, 0.5F, 0.5F, 1.0F);
	device.SetAmbient( ambient.GetPtr() );
	
	
	int lightNumber = 0;
	for (int i = 0, size = lights.size(); i < size; ++i)
	{
		Light* light = lights[i];
		if (light)
		{
			light->SetupVertexLight(lightNumber, 1.0f);	// @TODO: Visibility fade does not work with this vertex light setup path
			lightNumber++;
		}
	}
	device.DisableLights (lightNumber);
}


void SetLightScissorRect (const Rectf& lightRect, const Rectf& viewPort, bool intoRT, GfxDevice& device)
{
	Rectf rect = lightRect;
	rect.Scale (viewPort.width, viewPort.height);
	if (!intoRT)
		rect.Move (viewPort.x, viewPort.y);
	int scissorRect[4];
	RectfToViewport (rect, scissorRect);
	FlipScreenRectIfNeeded (device, scissorRect);
	device.SetScissorRect (scissorRect[0], scissorRect[1], scissorRect[2], scissorRect[3]);
}

void ClearScissorRect (bool oldScissor, const int oldRect[4], GfxDevice& device)
{
	if (oldScissor)
		device.SetScissorRect (oldRect[0],oldRect[1],oldRect[2],oldRect[3]);
	else
		device.DisableScissor();
}


// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (LightTests)
{
	TEST(LightKeywordsHaveExpectedValues)
	{
		CHECK_EQUAL (kLightKeywordSpot, keywords::Create("SPOT"));
		CHECK_EQUAL (kLightKeywordDirectional, keywords::Create("DIRECTIONAL"));
		CHECK_EQUAL (kLightKeywordDirectionalCookie, keywords::Create("DIRECTIONAL_COOKIE"));
		CHECK_EQUAL (kLightKeywordPoint, keywords::Create("POINT"));
		CHECK_EQUAL (kLightKeywordPointCookie, keywords::Create("POINT_COOKIE"));
		UInt32 mask = 0;
		for (int i = 0; i < kLightKeywordCount; ++i)
			mask += 1<<i;
		CHECK_EQUAL (kAllLightKeywordsMask, mask);
	}
} // SUITE
#endif // ENABLE_UNIT_TESTS
