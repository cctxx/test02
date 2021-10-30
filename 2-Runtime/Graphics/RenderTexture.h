#pragma once

#include "Texture.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Modules/ExportModules.h"

namespace ShaderLab {
	struct FastPropertyName;
	class TexEnv;
}

enum {
	kScreenOffscreen = -1,
};


class EXPORT_COREMODULE RenderTexture : public Texture
{
public:

	DECLARE_OBJECT_SERIALIZE (RenderTexture)
	REGISTER_DERIVED_CLASS (RenderTexture, Texture)

	RenderTexture (MemLabelId label, ObjectCreationMode mode);
	// virtual ~RenderTexture (); declared-by-macro

	void SetWidth (int width);
	int GetWidth () const { return m_Width; }

	void SetHeight (int height);
	int GetHeight () const { return m_Height; }

	void SetDepthFormat( DepthBufferFormat depth );
	DepthBufferFormat GetDepthFormat() const { return static_cast<DepthBufferFormat>(m_DepthFormat); }

	bool GetIsPowerOfTwo () const { return IsPowerOfTwo(m_Width) && IsPowerOfTwo(m_Height); }

	void SetDimension (TextureDimension dim);
	virtual TextureDimension GetDimension() const { return m_Dimension; }

	void SetVolumeDepth(int v);
	int GetVolumeDepth() const { return m_VolumeDepth; }

	void SetAntiAliasing(int aa);
	int GetAntiAliasing() const { return m_AntiAliasing; }
	bool IsAntiAliased() const { return m_AntiAliasing > 1; }

	void SetMipMap( bool mipmap );
	bool GetMipMap() const { return m_MipMap; }

	void SetGenerateMips (bool v);
	bool GetGenerateMips() const { return m_GenerateMips; }

	void SetSRGBReadWrite(bool sRGB);
	bool GetSRGBReadWrite() const { return m_SRGB; }

	void SetEnableRandomWrite(bool v);
	bool GetEnableRandomWrite() const { return m_EnableRandomWrite; }

	void SetCreatedFromScript( bool fromScript ) { m_CreatedFromScript = fromScript; }
	bool GetCreatedFromScript() const { return m_CreatedFromScript; }

	void SetSampleOnlyDepth( bool sampleOnly ) { m_SampleOnlyDepth = sampleOnly; }
	bool GetSampleOnlyDepth() const { return m_SampleOnlyDepth; }

	RenderTextureFormat GetColorFormat() const { return static_cast<RenderTextureFormat>(m_ColorFormat); }
	void SetColorFormat( RenderTextureFormat format );

	virtual bool HasMipMap () const { return m_MipMap; }
	virtual int CountMipmaps() const { return 1; } //@TODO ?

	enum {
		kFlagDontSetViewport  = (1<<0),
		kFlagForceResolve     = (1<<1),    // Force resolve to texture: Used by MSAA targets and Xbox 360
		kFlagDontRestoreColor = (1<<2),    // Xbox 360 specific: do not restore old contents to EDRAM
		kFlagDontRestoreDepth = (1<<3),    // Xbox 360 specific: do not restore old contents to EDRAM
		kFlagDontRestore      = kFlagDontRestoreColor | kFlagDontRestoreDepth,
	};

	static void FindAndSetSRGBWrite( RenderTexture* newActive );

	// Makes the render texture the current render target. If texture is NULL the back buffer is activated.
	static void SetActive( RenderTexture* texture, int mipLevel = 0, CubemapFace face = kCubeFaceUnknown, UInt32 flags = 0 );
	static bool SetActive(int count, RenderSurfaceHandle* colors, RenderSurfaceHandle depth, RenderTexture* rt, int mipLevel = 0, CubemapFace face=kCubeFaceUnknown, UInt32 flags=0);

	// Returns the active render texture.
	// NULL means the main window is active.
	static RenderTexture* GetActive();

	// Does card support RTs, and built with Unity Pro, and not manually disabled?
	static bool IsEnabled ();
	// this should not be used... it's there only to support RenderTexture.enabled in scripts, in case anyone uses it.
	static void SetEnabled (bool enable);

	// Destroys all render textures created
	static void ReleaseAll ();

	// Creates the render texture.
	// Create is automatically called inside Activate the first time.
	// Create can fail if RenderTexture::IsEnabled is false
	bool Create ();

	// Destroys the render texture
	void Release ();

	// Is the render texture created?
	bool IsCreated() const { return m_ColorHandle.IsValid() || m_DepthHandle.IsValid(); }

	// Discards the contents of this render texture
	// Xbox 360: will not restore contents to EDRAM the next time this RenderTexture is set.
	// GLES: will use combo of glDiscardFramebufferEXT and glClear to avoid both resolve and restore
	// Other platforms: currently no effect.
	// NB: we have both because no-arg one had been used directly from scripts, so we keep it for b/c
	void DiscardContents();
	void DiscardContents(bool discardColor, bool discardDepth);
	void MarkRestoreExpected();

	virtual bool ExtractImage (ImageReference* /*image*/, int /*imageIndex*/ = 0) const { return false; }
	virtual void ApplySettings();

	virtual int GetRuntimeMemorySize() const;


	#if UNITY_EDITOR
	TextureFormat GetEditorUITextureFormat () const { return -1; }
	#endif

	#if ENABLE_PROFILER || UNITY_EDITOR
	virtual int GetStorageMemorySize() const { return 0; }
	static int GetCreatedRenderTextureCount ();
	static int GetCreatedRenderTextureBytes ();
	#endif


	RenderSurfaceHandle GetColorSurfaceHandle() 		{ return m_ColorHandle; }
	RenderSurfaceHandle GetResolvedColorSurfaceHandle() { return m_ResolvedColorHandle; }
	RenderSurfaceHandle GetDepthSurfaceHandle() 		{ return m_DepthHandle; }


	virtual int GetDataWidth() const { return m_Width; }
	virtual int GetDataHeight() const { return m_Height; }

	void AwakeFromLoad(AwakeFromLoadMode mode);

	virtual void UnloadFromGfxDevice(bool /*forceUnloadAll*/) { }
	virtual void UploadToGfxDevice() { }

	ShaderLab::TexEnv *SetGlobalProperty (const ShaderLab::FastPropertyName& name);

	void GrabPixels (int left, int bottom, int width, int height);

	// Flips vertical texel size to positive or negative if we're not using OpenGL coords
	void CorrectVerticalTexelSize(bool shouldBePositive);

	const TextureID& GetSecondaryTextureID() { return m_SecondaryTexID; }

	static void SetTemporarilyAllowIndieRenderTexture (bool allow);

private:
	void DestroySurfaces();
	void ResolveAntiAliasedSurface();
	void UpdateTexelSize();
	bool GetSupportedMipMapFlag(bool mipMap) const;

private:
	int		m_Width; ///< range {1, 20000}
	int		m_Height;///< range {1, 20000}
	int		m_AntiAliasing;///< enum { None = 1, 2xMSAA = 2, 4xMSAA = 4, 8xMSAA = 8 } Anti-aliasing
	int		m_VolumeDepth;///< range {1, 20000}
	int		m_ColorFormat; ///< enum { RGBA32 = 0, Depth texture = 1 } Color buffer format
	int		m_DepthFormat; ///< enum { No depth buffer = 0, 16 bit depth = 1, 24 bit depth = 2 } Depth buffer format
	TextureDimension m_Dimension;
	bool	m_MipMap;
	bool	m_GenerateMips;
	bool	m_SRGB;
	bool	m_EnableRandomWrite;
	bool	m_CreatedFromScript;
	bool	m_SampleOnlyDepth;

	TextureID m_SecondaryTexID;

	RenderSurfaceHandle m_ColorHandle;
	RenderSurfaceHandle m_ResolvedColorHandle;
	RenderSurfaceHandle m_DepthHandle;
	
	int		m_RegisteredSizeForStats;

	ListNode<RenderTexture> m_RenderTexturesNode;

	bool m_SecondaryTexIDUsed;
};

int EstimateRenderTextureSize (int width, int height, int depth, RenderTextureFormat format, DepthBufferFormat depthFormat, TextureDimension dim, bool mipMap);

// return or passed tex (but created if needed) or NULL if RT cannot be created by any reason
RenderTexture* EnsureRenderTextureIsCreated(RenderTexture* tex);

bool RenderTextureSupportsStencil(RenderTexture* rt);

void RenderTextureDiscardContents(RenderTexture* rt, bool discardColor, bool discardDepth);

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
struct ScriptingRenderBuffer
{
	int m_RenderTextureInstanceID;
	RenderSurfaceBase* m_BufferPtr;
};
