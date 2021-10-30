#ifndef CUBEMAPTEXTURE_H
#define CUBEMAPTEXTURE_H

#include "Texture2D.h"


class Cubemap : public Texture2D
{
public:
	REGISTER_DERIVED_CLASS (Cubemap, Texture2D)
	DECLARE_OBJECT_SERIALIZE (Cubemap)

	Cubemap (MemLabelId label, ObjectCreationMode mode);
	
	virtual bool InitTexture (int width, int height, TextureFormat format, int flags = kMipmapMask, int imageCount = 1);
	virtual TextureDimension GetDimension () const { return kTexDimCUBE; }
	virtual void UploadTexture (bool dontUseSubImage);
	
	virtual void RebuildMipMap ();
	
	void FixupEdges (int fixupWidthInPixels = 1);
	
	void SetSourceTexture (CubemapFace face, PPtr<Texture2D> tex);
	PPtr<Texture2D> GetSourceTexture (CubemapFace face) const;
		
private:
	std::vector<PPtr<Texture2D> >	m_SourceTextures;
};

#endif
