#ifndef QUALITYSETTINGS_H
#define QUALITYSETTINGS_H

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/dynamic_array.h"

enum ShadowProjection
{
	kShadowProjCloseFit = 0,
	kShadowProjStableFit,
	kShadowProjectionCount
};


class QualitySettings  : public GlobalGameManager {
public:
	REGISTER_DERIVED_CLASS (QualitySettings, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (QualitySettings)

	QualitySettings(MemLabelId label, ObjectCreationMode mode);

	virtual void Reset ();

	int GetCurrentIndex () const { return m_CurrentQuality;}
	bool SetCurrent (string setting, bool applyExpensiveChanges);
	void SetCurrentIndex (int setting, bool applyExpensiveChanges);

	std::vector<std::string> GetQualitySettingsNames ();

	virtual void CheckConsistency ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	void ApplySettings( int previousIndex = -1, bool applyExpensiveChanges = false );

 	struct QualitySetting
	{
		DECLARE_SERIALIZE (QualitySetting)
		QualitySetting();
	
		UnityStr name;

		int   pixelLightCount;			///< Number of pixel lights to use
		int   shadows;					///< enum { Disable Shadows = 0, Hard Shadows Only = 1, Hard and Soft Shadows = 2 } Shadow quality
		int   shadowResolution;			///< enum { Low Resolution = 0, Medium Resolution = 1, High Resolution = 2, Very High Resolution = 3 } Shadow resolution
		int   shadowProjection;			///< enum { Close Fit = 0, Stable Fit = 1 } Shadow projection
		int	  shadowCascades;			///< enum { No Cascades = 1, Two Cascades = 2, Four Cascades = 4 } Number of cascades for directional light shadows
		float shadowDistance;			///< Shadow drawing distance
		int   blendWeights;				///< enum { 1 Bone=1, 2 Bones=2, 4 Bones = 4 } Bone count for mesh skinning
		int   textureQuality;			///< enum { Full Res = 0, Half Res = 1, Quarter Res = 2, Eighth Res = 3} Base texture level
		int   anisotropicTextures;		///< enum { Disabled = 0, Per Texture = 1, Forced On = 2 } When to enable anisotropic texturing
		int   antiAliasing;				///< enum { Disabled = 0, 2x Multi Sampling = 2, 4x Multi Sampling = 4, 8x Multi Sampling = 8 } Screen anti aliasing
		int   vSyncCount;				///< enum { Don't Sync = 0, Every VBlank = 1, Every Second VBlank = 2 } Limit refresh rate to avoid tearing
		bool  softParticles;			///< Use soft blending for particles?
		bool  softVegetation;			///< Use soft shading for terrain vegetation?
		float lodBias;
		int   maximumLODLevel;
		int   particleRaycastBudget;	///< Number of rays to cast for approximate world collisions

		#if UNITY_EDITOR
		std::vector<UnityStr> excludedTargetPlatforms;
		#endif
	};

	enum ShadowQuality
	{
		kShadowsDisable = 0,
		kShadowsHardOnly,
		kShadowsAll,
		kShadowQualityCount
	};
	enum
	{
		kShadowResolutionCount = 4,
	};

	const QualitySetting& GetCurrent() const { return m_QualitySettings[m_CurrentQuality]; }
	const QualitySetting& GetQualityForIndex(int index) const { Assert ( index < m_QualitySettings.size() ); return m_QualitySettings[index]; }
	int GetQualitySettingsCount() const { return m_QualitySettings.size(); }

	void SetPixelLightCount(int light);
	void SetShadowProjection(ShadowProjection shadowProjection);
	void SetShadowCascades(int cascades);
	void SetShadowDistance(float shadowDistance);
	void SetShadowDistanceTemporarily(float shadowDistance);
	void SetSoftVegetation (bool softVegetation);
	void SetSoftParticles (bool soft);
	void SetVSyncCount (int count);
	void SetAntiAliasing (int aaSettings);
	void SetAnisotropicTextures (int aniso);
	void SetLODBias (float lodBias);
	void SetMaximumLODLevel (int maximumLODLevel);
	void SetParticleRaycastBudget (int particleRaycastBudget);
	void SetBlendWeights(int weights);
	void SetMasterTextureLimit(int limit);

	/// The stripped maximum LOD level is calculated from the supported quality settings for this platform when entering playmode & building a player.
	/// It always acts as the absolute minimum for LOD's
	void SetStrippedMaximumLODLevel (int maximumLODLevel) { m_StrippedMaximumLODLevel = maximumLODLevel; }
	int  GetStrippedMaximumLODLevel () { return m_StrippedMaximumLODLevel; }

	#if UNITY_EDITOR
	typedef std::map<UnityStr, int> PerPlatformDefaultQuality;
	void SetQualitySettings(const QualitySetting* settings, int size, int defaultQuality);
	void DeletePerPlatformDefaultQuality ();
	int GetDefaultQualityForPlatform (const std::string& platformName);
	#endif

	static float GetShadowDistanceForRendering();

	static void InitializeClass();
	static void CleanupClass() {}

private:

	std::vector<QualitySetting> m_QualitySettings;

	int m_StrippedMaximumLODLevel;
	int m_CurrentQuality;

	void ApplyExpensiveSettings();

	#if UNITY_EDITOR
	PerPlatformDefaultQuality m_PerPlatformDefaultQuality;
	int m_PreviousAA;

	int m_PreviousVSync;
	#endif
};
QualitySettings& GetQualitySettings ();
QualitySettings* GetQualitySettingsPtr ();


#endif
