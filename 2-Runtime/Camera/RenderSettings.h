#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Graphics/GeneratedTextures.h"
#include "Runtime/Modules/ExportModules.h"

class Texture2D;

class EXPORT_COREMODULE RenderSettings : public LevelGameManager
{
public:
	RenderSettings (MemLabelId label, ObjectCreationMode mode);
	// virtual ~RenderSettings(); declared-by-macro
   	REGISTER_DERIVED_CLASS (RenderSettings, LevelGameManager)
	DECLARE_OBJECT_SERIALIZE (RenderSettings)

	void CheckConsistency ();
	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	/// Get the scene ambient light
	const ColorRGBAf& GetAmbientLight () const { return m_AmbientLight; }
	
	/// Get the ambient light in active color space
	ColorRGBAf GetAmbientLightInActiveColorSpace () const { return GammaToActiveColorSpace(m_AmbientLight); }

	void SetAmbientLight (const ColorRGBAf &col);
	#if UNITY_EDITOR
	void SetAmbientLightNoDirty (const ColorRGBAf &col);
	#endif

	/// Get the default spotlight cookie.
	/// This is used if no cookies have been assigned to a spotlight.
	Texture2D *GetDefaultSpotCookie();
	
	void SetupAmbient () const;

	// Fog
	const ColorRGBAf& GetFogColor() const { return m_FogColor; }
	void SetFogColor (const ColorRGBAf& color);
	bool GetUseFog () const { return m_Fog; } 
	void SetUseFog (bool fog);
	void SetUseFogNoDirty (bool fog);
	FogMode GetFogMode () const { return static_cast<FogMode>(m_FogMode); }
	void SetFogMode (FogMode fm);
	float GetFogDensity () const { return m_FogDensity; } 
	void SetFogDensity (float fog);
	float GetLinearFogStart() const { return m_LinearFogStart; }
	void SetLinearFogStart (float d);
	float GetLinearFogEnd() const { return m_LinearFogEnd; }
	void SetLinearFogEnd (float d);
		
	Material *GetSkyboxMaterial () const { return m_SkyboxMaterial; }
	void SetSkyboxMaterial (Material *mat); 
	float GetHaloStrength () const { return m_HaloStrength; }
	void SetHaloStrength (float halo);

	float GetFlareStrength () const { return m_FlareStrength; }
	void SetFlareStrength (float flare);
	float GetFlareFadeSpeed () const { return m_FlareFadeSpeed; }
	void SetFlareFadeSpeed (float flare);

	float CalcFogFactor (float distance) const;
 	
	static void InitializeClass ();
	static void PostInitializeClass ();
	static void CleanupClass ();
	
private:
	void ApplyFlareAndHaloStrength ();
	void ApplyHaloTexture();
	void ApplyFog ();

private:
	ColorRGBAf m_AmbientLight;   		///< Scene ambient color.
	float m_HaloStrength;				///< Strength of light halos range {0, 1}
	float m_FlareStrength;				///< Strength of light flares range {0, 1}
	float m_FlareFadeSpeed;				///< Fade time for a flare
	bool m_Fog;						///< Use fog in the scene?
	int m_FogMode;					///< enum { Linear=1, Exponential, Exp2 } Fog mode to use.
	ColorRGBAf m_FogColor;			///< Fog color.
	float m_LinearFogStart;					///< Starting distance for linear fog.
	float m_LinearFogEnd; 					///< End distance for linear fog.
	float m_FogDensity;					///< Density for exponential fog.
	PPtr<Texture2D> m_SpotCookie;		///< The default spotlight cookie
	PPtr<Texture2D> m_HaloTexture;		///< The light halo texture
	PPtr<Material> m_HaloMaterial;
	PPtr<Material> m_SkyboxMaterial;	///< The material used to render the skybox
};

EXPORT_COREMODULE RenderSettings& GetRenderSettings ();
