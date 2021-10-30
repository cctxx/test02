#ifndef LIGHT_H
#define LIGHT_H

#include "Lighting.h"
#include "Flare.h"
#include "ShadowSettings.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/Utilities/LinkedList.h"
#if UNITY_EDITOR
#include "Editor/Src/LightmapVisualization.h"
#endif
#include "Runtime/Math/Rect.h"

class Texture;
class GfxDevice;

class Light : public Behaviour, public ListElement
{
public:

	REGISTER_DERIVED_CLASS   (Light, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Light)

	Light (MemLabelId label, ObjectCreationMode mode);
	// ~Light(); declared-by-macro

	static void InitializeClass ();
	static void CleanupClass ();
	
	// Tag class as sealed, this makes QueryComponent faster.
	static bool IsSealedClass ()				{ return true; }
	
	virtual void Reset ();

	// System interaction
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void CheckConsistency ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	
	void TransformChanged ();
	

	LightType GetType() const { return static_cast<LightType>( m_Type ); }
	void SetType (LightType type);


	/// How to render this light.
	enum RenderMode
	{
		kRenderAuto,			///< Automatic
		kRenderImportant,		///< This light is important
		kRenderNotImportant,	///< This light is not very important
		kRenderModeCount // keep this last!
	};
	/// Get the render mode hint for this light
	int GetRenderMode() const { return m_RenderMode; }
	void SetRenderMode (int mode) 	{ m_RenderMode = mode; SetDirty(); m_GfxLightValid = false; }

	enum Lightmapping
	{
		kLightmappingRealtimeOnly = 0, // light is not baked, realtime only
		kLightmappingAuto = 1, // light is baked in the dual lightmap style
		kLightmappingBakedOnly = 2 // light is fully baked, not rendering in realtime at all
	};

	void SetLightmapping (int mode) { m_Lightmapping = mode; SetDirty(); m_GfxLightValid = false; }
	inline Lightmapping GetLightmappingForRender() const;
	Lightmapping GetLightmappingForBake() const;

	void SetActuallyLightmapped (bool v);
	bool GetActuallyLightmapped() const { return m_ActuallyLightmapped; }

	/// Get the full range of the light.
	float GetRange() const 			{ return m_Range; }
	void SetRange (float range) { m_Range = std::max(0.0F, range); SetDirty (); m_GfxLightValid = false; Precalc (); }

	/// Get the spot angle in degrees.
	float GetSpotAngle() const { return m_SpotAngle; }
	void SetSpotAngle (float angle) { m_SpotAngle = angle; CheckConsistency(); SetDirty (); m_GfxLightValid = false; Precalc (); }
	
	float GetCookieSize() const { return m_CookieSize; }
	void SetCookieSize (float size) { m_CookieSize = size; CheckConsistency(); SetDirty (); m_GfxLightValid = false; Precalc (); }
	
	// range * this value = end side length
	float GetCotanHalfSpotAngle () const { return m_CotanHalfSpotAngle; }
	// range * this value = diagonal side length
	float GetInvCosHalfSpotAngle () const { return m_InvCosHalfSpotAngle; }

	/// Get/set the cookie used for the light
	void SetCookie (Texture *tex);
	Texture *GetCookie () const { return m_Cookie; }
	
	/// Get/set the lens flare used
	void SetFlare (Flare *flare);
	Flare *GetFlare () { return m_Flare; }
		
	const ColorRGBAf& GetColor () const { return m_Color; }
	void SetColor (const ColorRGBAf& c);
	ColorRGBAf GetConvertedFinalColor() const { return m_ConvertedFinalColor; }
	
	float GetIntensity() const { return m_Intensity; }
	void SetIntensity( float i );

	float GetShadowStrength() const { return m_Shadows.m_Strength; }
	void SetShadowStrength( float f ) { m_Shadows.m_Strength = f; SetDirty(); }
	ShadowType GetShadows() const { return static_cast<ShadowType>(m_Shadows.m_Type); }
	void SetShadows( int v );
	int GetShadowResolution() const { return m_Shadows.m_Resolution; }
	void SetShadowResolution( int v ) { m_Shadows.m_Resolution = v; SetDirty(); }
	float GetShadowBias() const { return m_Shadows.m_Bias; }
	void SetShadowBias( float v ) { m_Shadows.m_Bias = v; SetDirty(); }
	float GetShadowSoftness() const { return m_Shadows.m_Softness; }
	void SetShadowSoftness (float v) { m_Shadows.m_Softness = v; SetDirty(); }
	float GetShadowSoftnessFade() const { return m_Shadows.m_SoftnessFade; }
	void SetShadowSoftnessFade (float v) { m_Shadows.m_SoftnessFade = v; SetDirty(); }
	
	int GetFinalShadowResolution() const;

	#if UNITY_EDITOR
	int GetShadowSamples() const { return m_ShadowSamples; }
	void SetShadowSamples (int samples) { m_ShadowSamples = samples; }
	float GetShadowRadius() const { return m_ShadowRadius; }
	void SetShadowRadius (float radius) { m_ShadowRadius = radius; }
	float GetShadowAngle() const { return m_ShadowAngle; }
	void SetShadowAngle (float angle) { m_ShadowAngle = angle; }
	float GetIndirectIntensity() const { return m_IndirectIntensity; }
	void SetIndirectIntensity (float intensity) { m_IndirectIntensity = intensity; }
	Vector2f GetAreaSize() const { return m_AreaSize; }
	void SetAreaSize(const Vector2f& areaSize) { m_AreaSize = areaSize; }
	#endif

	/// Calculate the quadratic attenuation factor for a light with a specified range
	/// @param range the range of the light
	/// @return the quadratic attenuation factor
	static float CalcQuadFac (float range);

	// Attenuation function: inverse quadratic. distSqr is over 0..1 range
	static float AttenuateNormalized(float distSqr);

	// Get the result of attenuation for a squared distance from the light
	float AttenuateApprox (float sqrDist) const;

	// Set up this light as GfxDevice fixed function per-vertex light.
	// IMPORTANT: Assumes the modelview matrix has been set up to be the world2camera matrix.
	void SetupVertexLight (int lightNo, float visibilityFade);

	void SetLightKeyword();
	void SetPropsToShaderLab (float blend) const;
	

	// Set up the halo for the light.
	void SetupHalo ();
	// Set up the flare for the light.
	void SetupFlare ();
	
	void SetCullingMask (int mask) { m_CullingMask.m_Bits = mask; SetDirty(); }
	int GetCullingMask () const { return m_CullingMask.m_Bits; }

	// Precalc all non-changing shaderlab values.
	void Precalc ();

	const Vector3f& GetWorldPosition() const { return m_WorldPosition; }
	const Matrix4x4f& GetWorldToLocalMatrix() const { return m_World2Local; }

	enum AttenuationMode
	{
		kSpotCookie,		// 2D cookie projected in spot light's cone
		kPointFalloff,		// Attenuation for a point light
		kDirectionalCookie,	// Cookie projected from a directional light
		kUnused				// The attenuation texture is not used
	};
	void GetMatrix (const Matrix4x4f* __restrict object2light, Matrix4x4f* __restrict outMatrix) const;

	bool IsValidToRender() const;

private:
	void UpdateSpotAngleValues ()
	{
		float halfSpotRad = Deg2Rad(m_SpotAngle * 0.5f);
		float cs = cosf(halfSpotRad);
		float ss = sinf(halfSpotRad);
		m_CotanHalfSpotAngle = cs / ss;
		m_InvCosHalfSpotAngle = 1.0f / cs;
	}

	void ComputeGfxLight (GfxVertexLight& gfxLight) const;

private:
	Matrix4x4f		m_World2Local;
	Vector3f		m_WorldPosition;
	ShadowSettings	m_Shadows;			///< Shadow settings.
	
	ColorRGBAf m_Color;
	ColorRGBAf m_ConvertedFinalColor;
	
	PPtr<Flare> m_Flare;				///< Does the light have a flare?
	PPtr<Texture> m_Cookie;				///< Custom cookie (optional). 
	BitField  m_CullingMask;            ///< The mask used for selectively lighting objects in the scene.
	float	m_Intensity;				///< Light intensity range {0.0, 8.0}
	float	m_Range; 					///< Light range
	float	m_SpotAngle;				///< Angle of the spotlight cone.
	float	m_CookieSize;				///< Cookie size for directional lights.
	float	m_CotanHalfSpotAngle; // cotangent of half of the spot angle
	float	m_InvCosHalfSpotAngle; // 1/cos of half of the spot angle
	int		m_RenderMode;				///< enum { Auto, Important, Not Important } Rendering mode for the light.
	int		m_Lightmapping;				///< enum { RealtimeOnly, Auto, BakedOnly } Is light baked into lightmaps?
	int		m_Type;						///< enum { Spot, Directional, Point, Area (baked only) } Light type
	bool    m_DrawHalo;					///< Does the light have a halo?
	bool	m_ActuallyLightmapped; // Is it actually lightmapped already?
	#if UNITY_EDITOR
	int		m_ShadowSamples;			// number of samples for lightmapper shadow calculations
	float	m_ShadowRadius;				// radius of the light source for lightmapper shadow calculations (point and spot lights)
	float	m_ShadowAngle;				// angle of the cone for lightmapper shadow rays (directional lights)
	float	m_IndirectIntensity;		// aka bounce intensity - a multiplier for the indirect light
	Vector2f m_AreaSize;				// size of area light's rectangle
	#endif
	
	PPtr<Texture>	m_AttenuationTexture;
	AttenuationMode m_AttenuationMode;
	LightKeywordMode	m_KeywordMode; // The current keyword used by the light
		
	int m_HaloHandle, m_FlareHandle;
	
	GfxVertexLight m_CachedGfxLight;
	bool	m_GfxLightValid;
};

void SetupVertexLights(const std::vector<Light*>& lights);
void SetLightScissorRect (const Rectf& lightRect, const Rectf& viewPort, bool intoRT, GfxDevice& device);
void ClearScissorRect (bool oldScissor, const int oldRect[4], GfxDevice& device);


// Inline this; called a lot in light culling.
inline Light::Lightmapping Light::GetLightmappingForRender() const 
{
	if (m_Type == kLightArea)
		return kLightmappingBakedOnly;

	// all lights behave as realtime only if no lightmaps have been baked yet or if lightmaps are disabled
	return m_ActuallyLightmapped
		#if UNITY_EDITOR
		&& GetLightmapVisualization().GetUseLightmapsForRendering()
		#endif
		? static_cast<Lightmapping>(m_Lightmapping) : kLightmappingRealtimeOnly; 
}


#endif
