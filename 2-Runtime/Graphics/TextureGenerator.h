#ifndef TEXTUREGENERATOR_H
#define TEXTUREGENERATOR_H

#include "TextureFormat.h"
#include "Texture3D.h"
#include "Texture.h"
#include "Runtime/Math/Vector3.h"
#include "CubemapTexture.h"

template<typename T, class ColorFunctor>
void GenerateTexture (Texture2D *tex, const ColorFunctor &fun) {
	AssertIf (!tex);
	int maxX = tex->GetGLWidth (), maxY = tex->GetGLHeight();
	T *data = (T*)tex->GetRawImageData();
	int bpp = GetBytesFromTextureFormat (tex->GetTextureFormat()) / sizeof(T);
	for (int y = 0; y < maxY; y++)
		for (int x = 0; x < maxX; x++) {
			fun (tex, data, x, y, maxX, maxY);
			data+=bpp;
		}
}

template<typename T, class ColorFunctor>
void Generate3DTexture (Texture3D *tex, const ColorFunctor &fun) {
	AssertIf (!tex);
	int maxX = tex->GetGLWidth (), maxY = tex->GetGLHeight(), maxZ = tex->GetDepth();
	T *data = (T*)tex->GetImageDataPointer();
	int bpp = GetBytesFromTextureFormat (tex->GetTextureFormat()) / sizeof(T);
	for (int z = 0; z < maxZ; z++) {
		for (int y = 0; y < maxY; y++) {
			for (int x = 0; x < maxX; x++) {
				fun (data, x, y, z, maxX, maxY, maxZ);
				data += bpp;
			}
		}
	}
}

template<typename T, class ColorFunctor>
void GenerateCubeTexture (Cubemap *cubemap, const ColorFunctor &fun) {
	AssertIf (!cubemap);
	for (int direction = 0; direction < 6; direction++) {
		const int kCubeXRemap[6] = { 2, 2, 0, 0, 0, 0 };
		const int kCubeYRemap[6] = { 1, 1, 2, 2, 1, 1 };
		const int kCubeZRemap[6] = { 0, 0, 1, 1, 2, 2 };
		const float kCubeXSign[6] = { -1.0F,  1.0F,  1.0F,  1.0F,  1.0F, -1.0F };
		const float kCubeYSign[6] = { -1.0F, -1.0F,  1.0F, -1.0F, -1.0F, -1.0F };
		const float kCubeZSign[6] = {  1.0F, -1.0F,  1.0F, -1.0F,  1.0F, -1.0F };

		int width = cubemap->GetGLWidth ();
		int height = cubemap->GetGLHeight ();

		// do the sign scale according to the cubemap specs then flip y sign
		Vector3f signScale = Vector3f (kCubeXSign[direction], kCubeYSign[direction], kCubeZSign[direction]);
		int byteSize = GetBytesFromTextureFormat (cubemap->GetTextureFormat ());

		Vector2f invSize (1.0F / (float)width, 1.0F / (float)height);
		UInt8* dest = cubemap->GetRawImageData (direction);
		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				Vector2f uv = Scale (Vector2f (x, y), invSize) * 2.0F - Vector2f (1.0F, 1.0F);
				Vector3f uvDir = Vector3f (uv.x, uv.y, 1.0F);
				
				uvDir.Scale (signScale);
				Vector3f worldDir;
				// Rotate the uv to the world direction using a table lookup
				worldDir[kCubeXRemap[direction]] = uvDir[0];
				worldDir[kCubeYRemap[direction]] = uvDir[1];
				worldDir[kCubeZRemap[direction]] = uvDir[2];

				fun ((T*)dest, worldDir);
				dest += byteSize;
			}
		}
	}
}

template<typename T, typename ColorFunctor>
Texture2D  *BuildTexture (int width, int height, TextureFormat format, const ColorFunctor &fun, bool mipmaps = false) {
	Texture2D* tex = CreateObjectFromCode<Texture2D>();
	tex->SetHideFlags(Object::kHideAndDontSave);
	tex->InitTexture (width, height, format, mipmaps ? Texture2D::kMipmapMask : Texture2D::kNoMipmap, 1);
	tex->GetSettings().m_Aniso = 0; // disable aniso
	GenerateTexture<T> (tex, fun);
	mipmaps ? tex->UpdateImageData() : tex->UpdateImageDataDontTouchMipmap();
	return tex;
}

template<typename T, class ColorFunctor>
Texture3D *Build3DTexture (int width, int height, int depth, TextureFormat format, const ColorFunctor &fun) {
	Texture3D *tex = CreateObjectFromCode<Texture3D>();
	tex->SetHideFlags(Object::kHideAndDontSave);
	tex->InitTexture (width, height, depth, format, false);
	Generate3DTexture<T> (tex, fun);
	tex->UpdateImageData(false);
	return tex;
}

template<typename T, class ColorFunctor>
Cubemap *BuildCubeTexture (int size, TextureFormat format, const ColorFunctor &fun) {
	Cubemap *tex = CreateObjectFromCode<Cubemap>();
	tex->SetHideFlags(Object::kHideAndDontSave);
	tex->InitTexture (size, size, format, 0, 6);
	GenerateCubeTexture<T> (tex, fun);
	tex->UpdateImageDataDontTouchMipmap();
	tex->GetSettings ().m_WrapMode = kTexWrapClamp;
	tex->ApplySettings ();
	return tex;	
}


#endif
