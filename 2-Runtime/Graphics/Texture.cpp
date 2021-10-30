#include "UnityPrefix.h"
#include "Texture.h"
#include "Image.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Graphics/ProceduralMaterial.h"

using namespace std;

static int gTextureBaseLevel = 0;
static int gAnisoSetting = Texture::kEnableAniso;

static const int kForceAnisoMinLevelDefault = 9;
static const int kAnisoMaxLevelDefault 		= 16;

static int gForceAnisoMinLevel	= kForceAnisoMinLevelDefault;
static int gAnisoMaxLevel		= kAnisoMaxLevelDefault;


Texture::TextureIDMap Texture::s_TextureIDMap;


///@todo: texture should not allocate memory based on texture base level.

Texture::Texture(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode), m_UVScaleX(1.0f), m_UVScaleY(1.0f), m_TexelSizeX(1.0f), m_TexelSizeY(1.0f)
{
	// We use unchecked version since we may not be on the main thread
	// This means CreateTextureID() implementation must be thread safe!
	m_TexID = GetUncheckedGfxDevice().CreateTextureID();
	m_UsageMode = kTexUsageNone;
	m_ColorSpace = kTexColorSpaceLinear;
}

Texture::~Texture ()
{
	MainThreadCleanup ();
}

bool Texture::MainThreadCleanup ()
{
	Texture::s_TextureIDMap.erase (m_TexID);

	// FreeTextureID() implementation must be thread safe!
	GetUncheckedGfxDevice().FreeTextureID(m_TexID);
	m_TexID = TextureID();
	
	// Notify TexEnvs using this texture
	for( size_t i = 0; i < m_TexEnvUsers.size(); ++i )
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		m_TexEnvUsers[i]->SetUsedTexture(NULL, 0);
	}
	m_TexEnvUsers.clear();
	return true;
}


void Texture::CheckConsistency()
{
	Super::CheckConsistency();
	m_TextureSettings.CheckConsistency ();
}

void Texture::Reset ()
{
	Super::Reset ();
	m_UsageMode = kTexUsageNone;
	m_ColorSpace = kTexColorSpaceLinear;
}

void Texture::SetMasterTextureLimit (int i, bool reloadTextures /*= true*/)
{
	if (gTextureBaseLevel == i)
		return;

	gTextureBaseLevel = i;
	if (!reloadTextures)
		return;

	vector<SInt32> objects;
	Object::FindAllDerivedObjects (ClassID (Texture), &objects);
	for( size_t j = 0; j < objects.size(); ++j )
	{
		Texture& tex = *PPtr<Texture> (objects[j]);
#if UNITY_EDITOR
		if (tex.IgnoreMasterTextureLimit ())
			continue;
#endif
		tex.UnloadFromGfxDevice(false);
		tex.UploadToGfxDevice();
	}

	ProceduralMaterial::ReloadAll();
}

void Texture::ReloadAll (bool unload, bool load, bool forceUnloadAll)
{
	vector<SInt32> objects;
	Object::FindAllDerivedObjects (ClassID (Texture), &objects, true);
	for (size_t i=0;i<objects.size ();++i)
	{
		Texture& tex = *PPtr<Texture> (objects[i]);

		if (unload)
			tex.UnloadFromGfxDevice(forceUnloadAll);
		if (load)
			tex.UploadToGfxDevice();
	}

	ProceduralMaterial::ReloadAll(unload, load);
}

static void SetAnisoLimitEnumImpl(int aniso, bool forced = false)
{
	if (aniso == gAnisoSetting && !forced)
		return;

	gAnisoSetting = aniso;
	if (gAnisoSetting == Texture::kDisableAniso)
		TextureSettings::SetAnisoLimits (1, 1);
	else if (gAnisoSetting == Texture::kForceEnableAniso)
		TextureSettings::SetAnisoLimits (gForceAnisoMinLevel, gAnisoMaxLevel);
	else
		TextureSettings::SetAnisoLimits (1, gAnisoMaxLevel);

	vector<Texture*> objects;
	Object::FindObjectsOfType (&objects);
	for (int i=0;i<objects.size ();i++)
		objects[i]->ApplySettings ();
}

void Texture::SetAnisoLimit (int aniso)
{
	SetAnisoLimitEnumImpl(aniso, false);
}


void Texture::SetGlobalAnisoLimits(int forcedMin, int globalMax)
{
	if(forcedMin == -1)
		forcedMin = kForceAnisoMinLevelDefault;
	if(globalMax == -1)
		globalMax = kAnisoMaxLevelDefault;

	if(gForceAnisoMinLevel == forcedMin && gAnisoMaxLevel == globalMax)
		return;

	gForceAnisoMinLevel = forcedMin;
	gAnisoMaxLevel 		= globalMax;

	SetAnisoLimitEnumImpl(gAnisoSetting, true);
}

int Texture::GetAnisoLimit ()
{
	return gAnisoSetting;
}

bool Texture::HasMipMap () const {return false;}

void Texture::ApplySettings()
{
	m_TextureSettings.Apply( GetTextureID(), GetDimension(), HasMipMap(), GetActiveTextureColorSpace() );
	NotifyMipBiasChanged();
}

void Texture::AddTexEnvUser(ShaderLab::TexEnv* texenv)
{
	SET_ALLOC_OWNER(this);
	size_t index = m_TexEnvUsers.size();
	texenv->SetUsedTexture(this, index);
	m_TexEnvUsers.push_back(texenv);
}

void Texture::RemoveTexEnvUser(ShaderLab::TexEnv* texenv, size_t index)
{
	DebugAssert(m_TexEnvUsers[index] == texenv);
	// Swap with last element and pop
	m_TexEnvUsers[index] = m_TexEnvUsers.back();
	m_TexEnvUsers[index]->SetUsedTexture(this, index);
	m_TexEnvUsers.pop_back();
	texenv->SetUsedTexture(NULL, 0);
}

void Texture::NotifyMipBiasChanged()
{
	float mipBias = m_TextureSettings.m_MipBias;
	for( size_t i = 0; i < m_TexEnvUsers.size(); ++i )
	{
		ShaderLab::TexEnv* te = m_TexEnvUsers[i];
		te->TextureMipBiasChanged( mipBias );
	}
}

void Texture::NotifyUVScaleChanged()
{
	float x = m_UVScaleX;
	float y = m_UVScaleY;
	for( size_t i = 0; i < m_TexEnvUsers.size(); ++i )
	{
		ShaderLab::TexEnv* te = m_TexEnvUsers[i];
		te->TextureUVScaleChanged( x, y );
	}
}


void Texture::SetFilterMode (int mode)
{
	if (m_TextureSettings.m_FilterMode != mode) {
		m_TextureSettings.m_FilterMode = mode;
		ApplySettings();
		SetDirty();
	}
}

void Texture::SetUsageMode ( TextureUsageMode mode)
{
	if (m_UsageMode != mode) {
		m_UsageMode = mode;
		ApplySettings();
		SetDirty();
	}
}

void Texture::SetStoredColorSpace(TextureColorSpace space)
{
	if (m_ColorSpace != space) {
		m_ColorSpace = space;
		ApplySettings();
		SetDirty();
	}
}

void Texture::SetStoredColorSpaceNoDirtyNoApply(TextureColorSpace space)
{
	if (m_ColorSpace != space) {
		m_ColorSpace = space;
	}
}

void Texture::SetWrapMode (int mode)
{
	if (m_TextureSettings.m_WrapMode != mode) {
		m_TextureSettings.m_WrapMode = mode;
		ApplySettings();
		SetDirty();
	}
}

void Texture::SetAnisoLevel (int level)
{
	if (m_TextureSettings.m_Aniso != level) {
		m_TextureSettings.m_Aniso = level;
		ApplySettings();
		SetDirty();
	}
}

void Texture::SetMipMapBias (float mipBias)
{
	if (m_TextureSettings.m_MipBias != mipBias)
	{
		m_TextureSettings.m_MipBias = mipBias;
		ApplySettings();
		SetDirty();
	}
}


#if UNITY_EDITOR
// Helper function so texture inspector can draw preview wrap mode
void Texture::SetWrapModeNoDirty (int mode)
{
	m_TextureSettings.m_WrapMode = mode;
	ApplySettings();
}

// Helper function so texture inspector can draw preview aniso level
void Texture::SetAnisoLevelNoDirty (int level)
{
	m_TextureSettings.m_Aniso = level;
	ApplySettings();
}

// Helper function so texture inspector can draw a specific mip of the texture
void Texture::SetMipMapBiasNoDirty (float mipBias)
{
	m_TextureSettings.m_MipBias = mipBias;
	ApplySettings();
}

// Helper function for editor so it can draw zoomed textures as point.
void Texture::SetFilterModeNoDirty (int mode)
{
	if (m_TextureSettings.m_FilterMode != mode) {
		m_TextureSettings.m_FilterMode = mode;
		ApplySettings();
	}
}

bool Texture::IgnoreMasterTextureLimit () const
{
	return false;
}

#endif

int Texture::GetMasterTextureLimit ()
{
	return gTextureBaseLevel;
}

bool Texture::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}

void* Texture::GetNativeTexturePtr()
{
	return GetGfxDevice().GetNativeTexturePointer(m_TexID);
}

UInt32 Texture::GetNativeTextureID()
{
	return GetGfxDevice().GetNativeTextureID(m_TexID);
}


IMPLEMENT_CLASS (Texture)
