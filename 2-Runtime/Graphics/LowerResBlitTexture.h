#ifndef LowerResBlitTexture_h
#define LowerResBlitTexture_h

#include "Runtime/Graphics/Texture.h"
#include "Runtime/GfxDevice/TextureIdMap.h"

// This is used by the OSX/Linux/iOS standalones for scaling up the viewport to the screen resolution.
class LowerResBlitTexture
: public Texture
{
public:

	LowerResBlitTexture(MemLabelId label, ObjectCreationMode mode) : Texture(label, mode) {}

	unsigned w, h;

	void Create(unsigned tex, unsigned w_, unsigned h_)
	{
	#if ENABLE_TEXTUREID_MAP
		TextureIdMap::UpdateTexture(m_TexID, tex);
	#else
		m_TexID = TextureID(tex);
	#endif



		w = w_; h = h_;
		m_TexelSizeX = 1.0f / w_;
		m_TexelSizeY = 1.0f / h_;
	}

	virtual TextureDimension GetDimension () const 									{ return kTexDim2D; }
	virtual bool ExtractImage (ImageReference* /*image*/, int /*imageIndex*/) const { return false; }
	virtual int GetStorageMemorySize() const										{ return 0; }

	virtual int GetDataWidth() const 	{ return w; }
	virtual int GetDataHeight() const 	{ return h; }

	virtual bool HasMipMap () const 	{ return false; }
	virtual int CountMipmaps () const 	{ return 1; }

	virtual void UnloadFromGfxDevice(bool forceUnloadAll) {}
	virtual void UploadToGfxDevice() {}
	virtual void ApplySettings () 		{}
};


static LowerResBlitTexture* CreateBlitTexture()
{
	static LowerResBlitTexture* _BlitTex = 0;
	if(_BlitTex == 0)
	{
		_BlitTex = CreateObjectFromCode<LowerResBlitTexture>();
		_BlitTex->SetHideFlags(Object::kHideAndDontSave);
	}

	return _BlitTex;
}


#endif
