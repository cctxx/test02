#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "TextureFormat.h"
#include "TextureSettings.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Modules/ExportModules.h"

#define TRACK_TEXTURE_SIZES (UNITY_EDITOR || ENABLE_PROFILER)

class ImageReference;
namespace ShaderLab { class TexEnv; }

class EXPORT_COREMODULE Texture : public NamedObject
{
protected:
	TextureSettings m_TextureSettings;
	TextureID		m_TexID;

	int		        m_UsageMode;

	int				m_ColorSpace;

	// Used by movie textures so that 0..1 range covers only the
	// movie portion. For all other textures this is (1,1).
	float		m_UVScaleX, m_UVScaleY;

	// Texel size. This is 1/size for all textures.
	float		m_TexelSizeX, m_TexelSizeY;

	// TexEnvs that use this texture
	dynamic_array<ShaderLab::TexEnv*> m_TexEnvUsers;

public:
	REGISTER_DERIVED_ABSTRACT_CLASS (Texture, NamedObject)

	Texture(MemLabelId label, ObjectCreationMode mode);

	virtual bool MainThreadCleanup();
	
	virtual void Reset ();

	virtual void CheckConsistency();

	virtual TextureDimension GetDimension () const = 0;

	// Blits the textures contents into image.
	// Will use the closest matching mipmap.
	virtual bool ExtractImage (ImageReference* image, int imageIndex = 0) const = 0;

	TextureID GetTextureID() const { return m_TexID; }

	TextureUsageMode GetUsageMode() const { return static_cast<TextureUsageMode>(m_UsageMode); }
	void SetUsageMode(TextureUsageMode mode);

	// The Color space the texture is imported for.
	void SetStoredColorSpace(TextureColorSpace space);
	// Needed to avoid an odd corner case for srgb cube render textures.
	// If a render texture has the apply function called a texture id will be created and
	// it will cause an error as it will be the wrong dimension when cubemap is set
	// This function sets the stored color space without applying it. It will be applied
	// when the create on the cubemap is called.
	void SetStoredColorSpaceNoDirtyNoApply(TextureColorSpace space);
	TextureColorSpace GetStoredColorSpace() const { return static_cast<TextureColorSpace>(m_ColorSpace); }

	// The active color space of the texture
	// Depending on if we are in linear or gamma rendering mode, textures will be tagged as sRGB or non-SRGB.
	TextureColorSpace GetActiveTextureColorSpace() const { return (GetActiveColorSpace() == kLinearColorSpace) ? GetStoredColorSpace() : kTexColorSpaceLinear; }

	#if ENABLE_PROFILER
	virtual int GetStorageMemorySize() const = 0;
	#endif

	virtual TextureID GetUnscaledTextureID() const { return m_TexID; }

	// Raw (original) texture size. For NPOT textures these values are NPOT.
	virtual int GetDataWidth() const = 0;
	virtual int GetDataHeight() const = 0;

	// Size as used by GL. In most cases this matches Data width/height, except for NPOT textures
	// where this is scaled up to next power of two.
	virtual int GetGLWidth() const { return GetDataWidth(); }
	virtual int GetGLHeight() const { return GetDataHeight(); }

	virtual bool HasMipMap () const = 0;
	virtual int CountMipmaps () const = 0;

	static void SetMasterTextureLimit (int i, bool reloadTextures = true);
	static int GetMasterTextureLimit ();

	static void ReloadAll (bool unload = true, bool load = true, bool forceUnloadAll = false);

	/// this is at the wrong place
	enum { kDisableAniso = 0, kEnableAniso = 1, kForceEnableAniso = 2 };

	// Get/Set the global texture anisotrophy levels
	static void SetAnisoLimit (int aniso);
	static int GetAnisoLimit ();

	static void SetGlobalAnisoLimits(int forcedMin, int globalMax);

	static Texture* FindTextureByID (TextureID tid)
	{
		TextureIDMap::iterator it = s_TextureIDMap.find(tid);
		return it == s_TextureIDMap.end() ? NULL : it->second;
	}

	// Set the filtering mode for this texture.
	// TextureFilterMode kTexFilterNearest, kTexFilterBilinear, kTexFilterTrilinear
	void SetFilterMode( int mode );
	int GetFilterMode() const { return m_TextureSettings.m_FilterMode; }

	void SetWrapMode (int mode);
	int GetWrapMode () const { return m_TextureSettings.m_WrapMode; }

	void SetAnisoLevel (int mode);
	int GetAnisoLevel () const { return m_TextureSettings.m_Aniso; }

	float GetMipMapBias () const { return m_TextureSettings.m_MipBias; }
	void SetMipMapBias (float bias);

	const TextureSettings& GetSettings() const { return m_TextureSettings; }
	TextureSettings& GetSettings() { return m_TextureSettings; }
	virtual void ApplySettings();

	float GetUVScaleX() const { return m_UVScaleX; }
	float GetUVScaleY() const { return m_UVScaleY; }
	void SetUVScale( float x, float y ) { m_UVScaleX = x; m_UVScaleY = y; NotifyUVScaleChanged(); }
	float GetTexelSizeX() const { return m_TexelSizeX; }
	float GetTexelSizeY() const { return m_TexelSizeY; }
	void SetTexelSize( float x, float y ) { m_TexelSizeX = x; m_TexelSizeY = y; }

	void AddTexEnvUser(ShaderLab::TexEnv* texenv);
	void RemoveTexEnvUser(ShaderLab::TexEnv* texenv, size_t index);

	virtual bool ShouldIgnoreInGarbageDependencyTracking ();


#if UNITY_EDITOR
	void SetAnisoLevelNoDirty (int level);
	void SetWrapModeNoDirty (int mode);
	void SetMipMapBiasNoDirty (float mipBias);
	void SetFilterModeNoDirty (int mode);
	virtual bool IgnoreMasterTextureLimit () const;

	virtual TextureFormat GetEditorUITextureFormat () const = 0;
#endif

	void* GetNativeTexturePtr();
	UInt32 GetNativeTextureID();

protected:
	void NotifyMipBiasChanged();
	void NotifyUVScaleChanged();

	// Used when changing master texture mip limit or explicitly reloading/unloading
	// all resources.
	virtual void UnloadFromGfxDevice(bool forceUnloadAll) = 0;
	virtual void UploadToGfxDevice() = 0;

	typedef std::map<TextureID, Texture*> TextureIDMap;
	static TextureIDMap s_TextureIDMap;
};
