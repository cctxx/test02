#include "UnityPrefix.h"
#include "QualitySettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/Camera/Shadows.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Utilities/Utility.h"
#if UNITY_EDITOR
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Src/LightmapVisualization.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#endif

enum { kDefaultQualitySettingCount = 6 };

QualitySettings::QualitySettings(MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
	#if UNITY_EDITOR
	m_PreviousAA = -1;
	m_PreviousVSync = -1;
	#endif
	m_StrippedMaximumLODLevel = 0;
}

QualitySettings::~QualitySettings()
{
}

// defaults to Fastest to Fantastic quality settings
QualitySettings::QualitySetting::QualitySetting()
:	name("Fastest")
,	pixelLightCount(0)
,	shadows(QualitySettings::kShadowsDisable)
,	shadowResolution(0)
,	shadowProjection(kShadowProjStableFit)
,	shadowCascades(1)
,	shadowDistance(15.0f)
,	blendWeights(1)
,	textureQuality(1)
,	anisotropicTextures(0)
,	antiAliasing(0)
,	vSyncCount(0)
,	softParticles(false)
,	softVegetation(false)
,	lodBias(0.3F)
,	maximumLODLevel(0)
,	particleRaycastBudget(4)
{
}

IMPLEMENT_CLASS_HAS_INIT (QualitySettings)
IMPLEMENT_OBJECT_SERIALIZE (QualitySettings)
GET_MANAGER (QualitySettings)
GET_MANAGER_PTR (QualitySettings)

static void InitializeDefaultQualitySettings(QualitySettings::QualitySetting* m_Quality)
{
	QualitySettings::QualitySetting qualitySetting;

	// Fastest to Fantastic quality settings
	m_Quality[0] = m_Quality[1] = m_Quality[2] = m_Quality[3] = m_Quality[4] = m_Quality[5] = qualitySetting;

	m_Quality[1].name = "Fast";
	m_Quality[1].shadowDistance = 20.0f;
	m_Quality[1].blendWeights = 2;
	m_Quality[1].textureQuality = 0;
	m_Quality[1].lodBias = 0.4F;
	m_Quality[1].particleRaycastBudget = 16;

	m_Quality[2].name = "Simple";
	m_Quality[2].pixelLightCount = 1;
	m_Quality[2].shadows = QualitySettings::kShadowsHardOnly;
	m_Quality[2].shadowDistance = 20.0f;
	m_Quality[2].blendWeights = 2;
	m_Quality[2].textureQuality = 0;
	m_Quality[2].anisotropicTextures = 1;
	m_Quality[2].lodBias = 0.70F;
	m_Quality[2].particleRaycastBudget = 64;

	m_Quality[3].name = "Good";
	m_Quality[3].pixelLightCount = 2;
	m_Quality[3].shadows = QualitySettings::kShadowsAll;
	m_Quality[3].shadowResolution = 1;
	m_Quality[3].shadowCascades = 2;
	m_Quality[3].shadowDistance = 40.0f;
	m_Quality[3].blendWeights = 2;
	m_Quality[3].textureQuality = 0;
	m_Quality[3].anisotropicTextures = 1;
	m_Quality[3].vSyncCount = 1;
	m_Quality[3].softVegetation = true;
	m_Quality[3].lodBias = 1.0F;
	m_Quality[3].particleRaycastBudget = 256;

	m_Quality[4].name = "Beautiful";
	m_Quality[4].pixelLightCount = 3;
	m_Quality[4].shadows = QualitySettings::kShadowsAll;
	m_Quality[4].shadowResolution = 2;
	m_Quality[4].shadowCascades = 2;
	m_Quality[4].shadowDistance = 70.0f;
	m_Quality[4].blendWeights = 4;
	m_Quality[4].textureQuality = 0;
	m_Quality[4].anisotropicTextures = 2;
	m_Quality[4].antiAliasing = 2;
	m_Quality[4].vSyncCount = 1;
	m_Quality[4].softParticles = true;
	m_Quality[4].softVegetation = true;
	m_Quality[4].lodBias = 1.5F;
	m_Quality[4].particleRaycastBudget = 1024;

	m_Quality[5].name = "Fantastic";
	m_Quality[5].pixelLightCount = 4;
	m_Quality[5].shadows = QualitySettings::kShadowsAll;
	m_Quality[5].shadowResolution = 2;
	m_Quality[5].shadowCascades = 4;
	m_Quality[5].shadowDistance = 150.0f;
	m_Quality[5].blendWeights = 4;
	m_Quality[5].textureQuality = 0;
	m_Quality[5].anisotropicTextures = 2;
	m_Quality[5].antiAliasing = 2;
	m_Quality[5].vSyncCount = 1;
	m_Quality[5].softParticles = true;
	m_Quality[5].softVegetation = true;
	m_Quality[5].lodBias = 2.0F;
	m_Quality[5].particleRaycastBudget = 4096;
}

#if UNITY_EDITOR
static void InitializePerPlatformDefaults(QualitySettings::PerPlatformDefaultQuality* platformSettings)
{
	platformSettings->clear ();
	for (int i=1; i<kBuildPlayerTypeCount; i++)
	{
		string target = GetBuildTargetGroupName((BuildTargetPlatform)i, false);

		if (target == "")
			continue;

		QualitySettings::PerPlatformDefaultQuality::iterator found = platformSettings->find(target);
		if (found == platformSettings->end())
		{
			if (target == "iPhone" || target == "Android" || target == "BlackBerry" || target == "Tizen")
				platformSettings->insert (pair<string,int>(target, 2));
			else
				platformSettings->insert (pair<string,int>(target, 3));
		}
	}
}
#endif

void QualitySettings::Reset()
{
	SET_ALLOC_OWNER(this);
	Super::Reset();

	QualitySetting tempQuality[kDefaultQualitySettingCount];
	InitializeDefaultQualitySettings(tempQuality);
#if UNITY_EDITOR
	InitializePerPlatformDefaults (&m_PerPlatformDefaultQuality);
#endif
	m_QualitySettings.assign(tempQuality, tempQuality + kDefaultQualitySettingCount);

	m_CurrentQuality = 3;
}

void QualitySettings::CheckConsistency ()
{
	// Ensure that there is always at least one quality setting.
	if (m_QualitySettings.empty())
	{
		QualitySetting tempQuality[kDefaultQualitySettingCount];
		InitializeDefaultQualitySettings(tempQuality);
#if UNITY_EDITOR
		InitializePerPlatformDefaults (&m_PerPlatformDefaultQuality);
#endif
		m_QualitySettings.push_back(tempQuality[3]);
	}

	for( int i = 0; i < m_QualitySettings.size(); ++i )
	{
		QualitySetting &q (m_QualitySettings[i]);
		q.pixelLightCount = std::max (q.pixelLightCount, 0);
		q.shadows = clamp<int>( q.shadows, 0, kShadowQualityCount-1 );
		q.shadowResolution = clamp<int>( q.shadowResolution, 0, kShadowResolutionCount-1 );
		q.shadowProjection = clamp<int>( q.shadowProjection, 0, kShadowProjectionCount-1 );
		q.shadowCascades = clamp<int>( q.shadowCascades, 1, 4 );
		if (q.shadowCascades == 3)
			q.shadowCascades = 2;
		if (q.antiAliasing < 2)
			q.antiAliasing = 0;
		else if (q.antiAliasing < 4)
			q.antiAliasing = 2;
		else if (q.antiAliasing < 8)
			q.antiAliasing = 4;
		else
			q.antiAliasing = 8;
		q.shadowDistance = std::max ( q.shadowDistance, 0.0f );
		q.blendWeights = std::min (std::max (q.blendWeights, 1), 4);
		if( q.blendWeights == 3 )
			q.blendWeights = 2;

		// in editor inspector we allow set [0..3], but during play we allow any level you want ;-)
		// set 10 as max in that case as it is lowest mip for 1024 texture - seems enough 8)
		{
		#if UNITY_EDITOR
			if(!IsWorldPlaying())
				q.textureQuality = std::min (std::max (q.textureQuality, 0), 3);
			else
		#endif
			q.textureQuality = std::min (std::max (q.textureQuality, 0), 10);
		}

		q.anisotropicTextures = std::min (std::max (q.anisotropicTextures, 0), 3);
		q.vSyncCount = std::min (std::max (q.vSyncCount, 0), 2);
		q.lodBias = max(0.0F, q.lodBias);
		q.maximumLODLevel = clamp(q.maximumLODLevel, 0, 7);
	}

	m_CurrentQuality = clamp<int>( m_CurrentQuality, 0, m_QualitySettings.size()-1 );
}

void QualitySettings::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	#if !UNITY_EDITOR
		// player
		// careful to not change current quality setting if player prefs entry does not exist
		m_CurrentQuality = clamp<int>( PlayerPrefs::GetInt("UnityGraphicsQuality", m_CurrentQuality), 0, m_QualitySettings.size()-1 );
		ApplySettings ();
	#else
		// editor
		PlayerPrefs::SetInt("UnityGraphicsQuality", m_CurrentQuality);
		ApplySettings (-1, (awakeMode & kDidLoadFromDisk)==0);
	#endif

}

std::vector<std::string> QualitySettings::GetQualitySettingsNames ()
{
	std::vector<std::string> settingNames;
	for( int i = 0; i < m_QualitySettings.size(); ++i )
	{
		settingNames.push_back (m_QualitySettings[i].name);
	}
	return settingNames;
}

void QualitySettings::SetCurrentIndex (int setting, bool applyExpensiveChanges)
{
	setting = clamp<int>( setting, 0, m_QualitySettings.size()-1 );

	SetDirty ();
	int oldIndex = m_CurrentQuality;
	m_CurrentQuality = setting;
	PlayerPrefs::SetInt("UnityGraphicsQuality", m_CurrentQuality);

	ApplySettings( oldIndex, applyExpensiveChanges );
}

bool QualitySettings::SetCurrent (string setting, bool applyExpensiveChanges)
{
	for( int i = 0; i < m_QualitySettings.size(); ++i )
	{
		if (m_QualitySettings[i].name == setting)
		{
			SetCurrentIndex (i, applyExpensiveChanges);
			return true;
		}
	}
	return false;
}

void QualitySettings::ApplySettings( int previousIndex, bool applyExpensiveChanges )
{
	const QualitySetting &q = GetCurrent();

	Texture::SetAnisoLimit (q.anisotropicTextures);
	Texture::SetMasterTextureLimit (q.textureQuality);
	if (GetLODGroupManagerPtr())
		GetLODGroupManager().SetLODBias (q.lodBias);

	// When playing we have to clamp to the stripped maximum level
	int lodLevel = q.maximumLODLevel;
	if (IsWorldPlaying())
		lodLevel = max(m_StrippedMaximumLODLevel, lodLevel);
	if (GetLODGroupManagerPtr())
		GetLODGroupManager().SetMaximumLODLevel (lodLevel);

	bool hasChanged = false;
	if (applyExpensiveChanges)
	{
		#if UNITY_EDITOR
		if( m_PreviousAA != q.antiAliasing || m_PreviousVSync != q.vSyncCount )
			hasChanged = true;
		#else
		const QualitySetting& old = GetQualityForIndex(previousIndex);
		if( old.antiAliasing != q.antiAliasing || old.vSyncCount != q.vSyncCount )
			hasChanged = true;
		#endif
	}

	if (hasChanged)
	{
		ApplyExpensiveSettings();
	}
}

void QualitySettings::ApplyExpensiveSettings()
{
#if UNITY_EDITOR
	GUIView::RecreateAllOnAAChange();
	m_PreviousAA = GetCurrent().antiAliasing;
	m_PreviousVSync = GetCurrent().vSyncCount;
#else
	// Make sure to use "as requested" values for everything
	// Otherwise we might lose pending changes (case 559015)
	ScreenManager& screen = GetScreenManager();
	screen.RequestResolution(
		screen.GetWidthAsRequested(), screen.GetHeightAsRequested(),
		screen.GetFullScreenAsRequested(), screen.GetRefreshRateAsRequested());
#endif
}

template<class TransferFunc>
void QualitySettings::QualitySetting::Transfer (TransferFunc& transfer)
{
	transfer.SetVersion(2);
	TRANSFER_SIMPLE (name);
	TRANSFER_SIMPLE (pixelLightCount);
	TRANSFER_SIMPLE (shadows);
	TRANSFER_SIMPLE (shadowResolution);
	TRANSFER_SIMPLE (shadowProjection);
	TRANSFER_SIMPLE (shadowCascades);
	TRANSFER_SIMPLE (shadowDistance);
	TRANSFER_SIMPLE (blendWeights);
	TRANSFER_SIMPLE (textureQuality);
	TRANSFER_SIMPLE (anisotropicTextures);
	TRANSFER_SIMPLE (antiAliasing);
	TRANSFER_SIMPLE (softParticles);
	transfer.Transfer (softVegetation, "softVegetation", kSimpleEditorMask | kHideInEditorMask);
	transfer.Align();
	TRANSFER_SIMPLE (vSyncCount);
	TRANSFER_SIMPLE (lodBias);
	TRANSFER_SIMPLE (maximumLODLevel);
	TRANSFER_SIMPLE (particleRaycastBudget);

	// Unity 3.4 introduced vSyncCount instead of syncToVBL.
	if(transfer.IsVersionSmallerOrEqual(1))
	{
		bool syncToVBL;
		transfer.Transfer (syncToVBL, "syncToVBL", kSimpleEditorMask | kHideInEditorMask);
		vSyncCount = (syncToVBL?1:0);
	}
	transfer.Align();

	TRANSFER_EDITOR_ONLY (excludedTargetPlatforms);
}

template<class TransferFunction>
void QualitySettings::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion( 5 );

	transfer.Transfer (m_CurrentQuality, "m_CurrentQuality", kSimpleEditorMask);

	TRANSFER_SIMPLE (m_QualitySettings);
	TRANSFER_EDITOR_ONLY(m_PerPlatformDefaultQuality);

	// Read old default selected default quality settings
	if (transfer.IsVersionSmallerOrEqual(4))
	{
		int m_DefaultStandaloneQuality = 3;
		int m_DefaultWebPlayerQuality = 3;
		int m_DefaultMobileQuality = 2;

		TRANSFER_SIMPLE (m_DefaultStandaloneQuality);
		TRANSFER_SIMPLE (m_DefaultWebPlayerQuality);
		TRANSFER_SIMPLE (m_DefaultMobileQuality);
		transfer.Transfer (m_CurrentQuality, "m_EditorQuality", kSimpleEditorMask);

		#if UNITY_EDITOR
			for (int i=1; i<kBuildPlayerTypeCount; i++)
			{
				string target = GetBuildTargetGroupName((BuildTargetPlatform)i, false);

				if (target == "")
					continue;

				PerPlatformDefaultQuality::iterator found = m_PerPlatformDefaultQuality.find(target);
				if (found == m_PerPlatformDefaultQuality.end())
				{
					if (target == "Web")
						m_PerPlatformDefaultQuality.insert (pair<string,int>(target, m_DefaultWebPlayerQuality));
					else if (target == "Standalone")
						m_PerPlatformDefaultQuality.insert (pair<string,int>(target, m_DefaultStandaloneQuality));
					else if (target == "iPhone" || target == "Android" || target == "BlackBerry" || target == "Tizen")
						m_PerPlatformDefaultQuality.insert (pair<string,int>(target, m_DefaultMobileQuality));
					else
						m_PerPlatformDefaultQuality.insert (pair<string,int>(target, 3));
				}
			}
		#else
			#if WEBPLUG
				m_CurrentQuality = m_DefaultWebPlayerQuality;
			#elif ( (UNITY_ANDROID) || (UNITY_IPHONE) || (UNITY_BB10) || (UNITY_TIZEN) )
				m_CurrentQuality = m_DefaultMobileQuality;
			#else
				m_CurrentQuality = m_DefaultStandaloneQuality;
			#endif
		#endif

		if (m_QualitySettings.size () == 6)
		{
			m_QualitySettings[0].name = "Fastest";
			m_QualitySettings[1].name = "Fast";
			m_QualitySettings[2].name = "Simple";
			m_QualitySettings[3].name = "Good";
			m_QualitySettings[4].name = "Beautiful";
			m_QualitySettings[5].name = "Fantastic";
	}

	}

	if (transfer.IsVersionSmallerOrEqual(3))
	{
		QualitySetting tempQuality[kDefaultQualitySettingCount];
		InitializeDefaultQualitySettings(tempQuality);
#if UNITY_EDITOR
		InitializePerPlatformDefaults (&m_PerPlatformDefaultQuality);
#endif
		transfer.Transfer (tempQuality[0], "Fastest", kSimpleEditorMask);
		transfer.Transfer (tempQuality[1], "Fast", kSimpleEditorMask);
		transfer.Transfer (tempQuality[2], "Simple", kSimpleEditorMask);
		transfer.Transfer (tempQuality[3], "Good", kSimpleEditorMask);
		transfer.Transfer (tempQuality[4], "Beautiful", kSimpleEditorMask);
		transfer.Transfer (tempQuality[5], "Fantastic", kSimpleEditorMask);

		// Unity 3.3 only supported Close Fit shadow projection
		// In Unity 3.4 the default value is Stable Fit shadows.
		if (transfer.IsVersionSmallerOrEqual(2))
		{
			for (int i = 0; i < kDefaultQualitySettingCount; i++)
				tempQuality[i].shadowProjection = kShadowProjCloseFit;
		}

		m_QualitySettings.assign(tempQuality, tempQuality + kDefaultQualitySettingCount);
	}

	if (transfer.IsSerializingForGameRelease ())
	{
		TRANSFER(m_StrippedMaximumLODLevel);
	}
}

void QualitySettings::SetPixelLightCount(int light)
{
	m_QualitySettings[m_CurrentQuality].pixelLightCount = light;

	SetDirty();
}

void QualitySettings::SetShadowProjection(ShadowProjection shadowProjection)
{
	int projection = clamp<int>(shadowProjection, 0, kShadowProjectionCount-1);
	m_QualitySettings[m_CurrentQuality].shadowProjection = projection;
	SetDirty();
}

void QualitySettings::SetShadowCascades(int cascades)
{
	// valid cascade counts are 1, 2, 4
	cascades = clamp(cascades,1,4);
	if (cascades == 3)
		cascades = 2;

	m_QualitySettings[m_CurrentQuality].shadowCascades = cascades;
	SetDirty();
}

// Used by the SceneView in order to temporarily override the shadow distance when rendering in ortho mode
void QualitySettings::SetShadowDistanceTemporarily(float shadowDistance)
{
	shadowDistance = std::max ( shadowDistance, 0.0f );
	m_QualitySettings[m_CurrentQuality].shadowDistance = shadowDistance;
}

void QualitySettings::SetShadowDistance(float shadowDistance)
{
	shadowDistance = std::max ( shadowDistance, 0.0f );
	m_QualitySettings[m_CurrentQuality].shadowDistance = shadowDistance;
	SetDirty();
}

void QualitySettings::SetSoftVegetation (bool softVegetation)
{
	m_QualitySettings[m_CurrentQuality].softVegetation = softVegetation;
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
}

void QualitySettings::SetSoftParticles (bool soft)
{
	m_QualitySettings[m_CurrentQuality].softParticles = soft;
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
}

void QualitySettings::SetVSyncCount (int count)
{
	if (m_QualitySettings[m_CurrentQuality].vSyncCount == count)
		return;
	m_QualitySettings[m_CurrentQuality].vSyncCount = count;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
	ApplyExpensiveSettings();
}

void QualitySettings::SetAntiAliasing (int aaCount)
{
	if (m_QualitySettings[m_CurrentQuality].antiAliasing == aaCount)
		return;
	m_QualitySettings[m_CurrentQuality].antiAliasing = aaCount;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
	ApplyExpensiveSettings();
}

void QualitySettings::SetAnisotropicTextures (int aniso)
{
	if (m_QualitySettings[m_CurrentQuality].anisotropicTextures == aniso)
		return;
	m_QualitySettings[m_CurrentQuality].anisotropicTextures = aniso;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
	ApplySettings();
}

void QualitySettings::SetLODBias (float lodBias)
{
	if (m_QualitySettings[m_CurrentQuality].lodBias == lodBias)
		return;
	m_QualitySettings[m_CurrentQuality].lodBias = lodBias;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
	ApplySettings();
}

void QualitySettings::SetMaximumLODLevel (int maximumLODLevel)
{
	if (m_QualitySettings[m_CurrentQuality].maximumLODLevel == maximumLODLevel)
		return;
	m_QualitySettings[m_CurrentQuality].maximumLODLevel = maximumLODLevel;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
	ApplySettings();
}

void QualitySettings::SetParticleRaycastBudget (int particleRaycastBudget)
{
	if (m_QualitySettings[m_CurrentQuality].particleRaycastBudget == particleRaycastBudget)
		return;
	m_QualitySettings[m_CurrentQuality].particleRaycastBudget = particleRaycastBudget;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty ();
	ApplySettings();
}

void QualitySettings::SetBlendWeights(int weights)
{
	if (m_QualitySettings[m_CurrentQuality].blendWeights == weights)
		return;
	m_QualitySettings[m_CurrentQuality].blendWeights = weights;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty();
	ApplySettings();
}

void QualitySettings::SetMasterTextureLimit(int limit)
{
	if (m_QualitySettings[m_CurrentQuality].textureQuality == limit)
		return;
	m_QualitySettings[m_CurrentQuality].textureQuality = limit;
	CheckConsistency();
	// Prevent repaints caused by temporary quality settings changes
	SetDirty();
	ApplySettings();
}

void QualitySettings::InitializeClass()
{
}

float QualitySettings::GetShadowDistanceForRendering()
{
#if UNITY_EDITOR
	if (GetLightmapVisualization().GetEnabled())
		return GetLightmapVisualization().GetShadowDistance();
	else
		return GetQualitySettings().GetCurrent().shadowDistance;
#else
	return GetQualitySettings().GetCurrent().shadowDistance;
#endif
}

#if UNITY_EDITOR

void QualitySettings::SetQualitySettings(const QualitySetting* settings, int size, int currentQuality)
{
	m_QualitySettings.assign(settings, settings + size);
	m_CurrentQuality = currentQuality;
	CheckConsistency();
	AwakeFromLoad(kDefaultAwakeFromLoad);
	SetDirty();
}

void QualitySettings::DeletePerPlatformDefaultQuality()
{
	m_PerPlatformDefaultQuality.clear();
}

int QualitySettings::GetDefaultQualityForPlatform (const std::string& platformName)
{
	PerPlatformDefaultQuality::iterator found = m_PerPlatformDefaultQuality.find(platformName);
	if (found == m_PerPlatformDefaultQuality.end())
		return -1;
	else
		return found->second;
}

#endif
