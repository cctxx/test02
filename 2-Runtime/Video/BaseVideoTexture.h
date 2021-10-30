#ifndef VIDEO_TEXTURE
#define VIDEO_TEXTURE

#include "Runtime/Graphics/Texture.h"

struct YuvFrame
{
	unsigned char* y;
	unsigned char* u;
	unsigned char* v;
	int width;
	int height;
	int y_stride;
	int uv_stride;
	int offset_x;
	int offset_y;
	int uv_step;
};

class BaseVideoTexture: public Texture
{
protected:
	virtual ~BaseVideoTexture();
public:
	BaseVideoTexture (MemLabelId label, ObjectCreationMode mode);

	void InitVideoMemory(int width, int height);
	void ReleaseVideoMemory ();

	virtual void UnloadFromGfxDevice(bool forceUnloadAll) { }
	virtual void UploadToGfxDevice() { }

	UInt32 *GetImageBuffer() const {return m_ImageBuffer;}

	virtual int GetDataWidth() const {return m_VideoWidth; }
	virtual int GetDataHeight() const {return m_VideoHeight; }

	int GetPaddedHeight() const {return m_PaddedHeight; }
	int GetPaddedWidth() const {return m_PaddedWidth; }

	int GetTextureHeight() const {return m_TextureHeight; }
	int GetTextureWidth() const {return m_TextureWidth; }

	virtual bool HasMipMap () const { return false; }
	virtual int CountMipmaps() const { return 1; }

	bool	IsReadable() const	{ return m_IsReadable; }
	void	SetReadable(bool readable);

	void UploadTextureData();

	virtual void Update() = 0;
	virtual void Play() { m_EnableUpdates = true; }
	virtual void Pause() { m_EnableUpdates = false; }
	virtual void Stop() { m_EnableUpdates = false; }
	virtual bool IsPlaying() const { return m_EnableUpdates; }
	virtual void Suspend() {} 
	virtual void Resume() {}

	static void UpdateVideoTextures();
	static void PauseVideoTextures();
	static void StopVideoTextures();

	// Useful for platforms like WinRT that can lose DX device on app switch
	static void SuspendVideoTextures();
	static void ResumeVideoTextures();

	virtual TextureDimension GetDimension () const { return kTexDim2D; }

	virtual int GetRuntimeMemorySize() const { return m_TextureWidth * m_TextureHeight * 4; }
	#if UNITY_EDITOR
	virtual int GetStorageMemorySize() const { return 0; }
	virtual TextureFormat GetEditorUITextureFormat () const { return kTexFormatARGB32; }
	#endif

	virtual bool ExtractImage (ImageReference* image, int imageIndex = 0) const;

	bool DidUpdateThisFrame () const { return m_DidUpdateThisFrame; };

	void YuvToRgb (const YuvFrame *yuv);
	void YUYVToRGBA (UInt16 *const src);
	void YUYVToRGBA (UInt16 *const src, int srcStride);

    virtual int GetVideoRotationAngle() const 		{ return 0; }
    virtual bool IsVideoVerticallyMirrored() const	{ return false; }

private:
	UInt32*	m_ImageBuffer;					//texture image buffer

	int m_VideoWidth, m_VideoHeight;		//height and width of video source
	int m_TextureWidth, m_TextureHeight;	//power-of-two texture dimensions
	int m_PaddedWidth, m_PaddedHeight;		//movie size padded by 1 if non-power-of-two in order to fix clamping

	bool m_EnableUpdates;
	bool m_DidUpdateThisFrame;
	bool m_IsReadable;

private:
	void UploadGfxTextureBuffer(UInt32* imgBuf);

protected:
	void CreateGfxTextureAndUploadData(bool uploadCurrentBuffer);

	virtual TextureFormat	GetBufferTextureFormat() const 		{ return kTexFormatARGB32; }
	// by default we support only readable textures, as we create mem buffer anyway
	virtual bool			CanSetReadable(bool readable) const	{ return readable ? true : false; }
};


#endif
