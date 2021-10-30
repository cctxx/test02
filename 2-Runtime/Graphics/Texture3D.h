#pragma once

#include "Texture.h"


class Texture3D : public Texture
{
public:
	REGISTER_DERIVED_CLASS (Texture3D, Texture)
	DECLARE_OBJECT_SERIALIZE (Texture3D)
	
	Texture3D (MemLabelId label, ObjectCreationMode mode);

	virtual bool MainThreadCleanup ();
	
	
	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	void UploadTexture (bool dontUseSubImage);
	bool InitTexture (int width, int height, int depth, TextureFormat format, bool mipMaps);
	void UpdateImageData (bool rebuildMipMaps);

	UInt8* GetImageDataPointer() { return m_Data; }
	UInt32 GetImageDataSize() const { return m_DataSize; }
	int GetDepth() const { return m_Depth; }
	TextureFormat GetTextureFormat() const { return m_Format; }

	bool GetPixels (ColorRGBAf* dest, int miplevel) const;
	void SetPixels (int pixelCount, const ColorRGBAf* pixels, int miplevel);

	// Texture
	virtual TextureDimension GetDimension () const { return kTexDim3D; }
	virtual bool ExtractImage (ImageReference* image, int imageIndex = 0) const;
	virtual int GetDataWidth() const { return m_Width; }
	virtual int GetDataHeight() const { return m_Height; }
	virtual bool HasMipMap () const { return m_MipMap; }
	virtual int CountMipmaps() const;
	#if ENABLE_PROFILER
	virtual int GetStorageMemorySize() const { return m_DataSize; }
	#endif
	#if UNITY_EDITOR
	virtual TextureFormat GetEditorUITextureFormat () const { return GetTextureFormat(); }
	#endif
	virtual int GetRuntimeMemorySize() const { return m_DataSize; }

protected:
	// Texture
	virtual void UnloadFromGfxDevice(bool forceUnloadAll);
	virtual void UploadToGfxDevice();

private:
	UInt8* AllocateTextureData(int imageSize, TextureFormat format, bool initMemory);
	void DestroyTexture();
	void RebuildMipMap ();
	void DeleteGfxTexture();
	
private:
	int				m_Width;
	int				m_Height;
	int				m_Depth;
	TextureFormat	m_Format;
	UInt8*			m_Data;
	UInt32			m_DataSize;
	bool			m_MipMap;
	bool			m_TextureUploaded;
};
