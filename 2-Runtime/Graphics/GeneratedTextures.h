#ifndef GENERATEDTEXTURES_H
#define GENERATEDTEXTURES_H

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
class Texture;
class Texture2D;
class Texture3D;

namespace builtintex {

void GenerateBuiltinTextures();
void ReinitBuiltinTextures();
	
Texture2D* GetWhiteTexture ();
Texture2D* GetBlackTexture ();
Texture3D* GetDitherMaskTexture();
Texture* GetAttenuationTexture ();
Texture* GetHaloTexture ();

// Get the default texture for a texture dimension
TextureID GetDefaultTexture( TextureDimension texDim );	
	
TextureID GetBlackTextureID ();

} // namespace


#endif
