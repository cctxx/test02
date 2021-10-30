#include "UnityPrefix.h"
#include "External/shaderlab/Library/properties.h"
#include "Texture.h"
#include "External/shaderlab/Library/texenv.h"
#include "GeneratedTextures.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Math/Random/Random.h"
#include "TextureGenerator.h"
#include "Runtime/GfxDevice/BuiltinShaderParams.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#include <limits>

static PPtr<Texture2D> gWhiteTex = NULL;
static PPtr<Texture2D> gBlackTex = NULL;
static PPtr<Texture2D> gAttenuationTex = NULL;
static PPtr<Texture2D> gHaloTex = NULL;
static PPtr<Texture3D> gDitherMaskTex = NULL;
static PPtr<Texture2D> s_RandomRotationTex;
static PPtr<Texture2D> s_NormalMapTex;
static PPtr<Texture2D> s_RedTex;
static PPtr<Texture2D> s_GrayTex;
static PPtr<Texture2D> s_GrayRampTex;


static TextureID gDefaultTextures[kTexDimCount];

static Rand gRandomSeed = 0;

using namespace ShaderLab;
using namespace std;



inline void EmptyNormalMap (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	// make "empty normal map" work in both plain encoding and DXT5nm
	data[0] = 127; // X=0.5 for plain
	data[1] = 127; // Y=0.5 for plain & DXT5nm
	data[2] = 255; // Z=1.0 for plain
	data[3] = 127; // X=0.5 for DXT5nm
}

inline void White (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
}

inline void GrayscaleRamp (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	data[0] = x;
	data[1] = x;
	data[2] = x;
	data[3] = x;
}

inline void Black (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
}

inline void Red (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	data[0] = 255;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
}

inline void Gray (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	data[0] = 127;
	data[1] = 127;
	data[2] = 127;
	data[3] = 127;
}

inline float ToUnsigned(const float s)
{
	return 0.5f * s + 0.5f;
}

inline void RandomRotation (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	float randAngle = 2.0f * kPI * Random01(gRandomSeed);
	data[0] = NormalizedToByte (ToUnsigned ( Cos (randAngle)));
	data[1] = NormalizedToByte (ToUnsigned (-Sin (randAngle)));
	data[2] = NormalizedToByte (ToUnsigned ( Sin (randAngle)));
	data[3] = NormalizedToByte (ToUnsigned ( Cos (randAngle)));
}

template <typename T>
inline void LightAttenuation (Texture2D *tex, T *data, int x, int y, int maxX, int maxY)
{
	float sqrRange = (float)x / (float)maxX;
	float val = Light::AttenuateNormalized(sqrRange);
	T b = RoundfToInt( val * std::numeric_limits<T>::max() );
	*data = b;
}

inline void HaloTex (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY)
{
	maxX >>= 1;
	maxY >>= 1;
	float xFac = (float)(x - maxX) / (float)(maxX);
	float yFac = (float)(y - maxY) / (float)(maxY);
	float sqrRange = xFac * xFac + yFac * yFac;
	if (sqrRange > 1.0f)
		sqrRange = 1.0f;
	*data = RoundfToInt((1.0f - sqrRange) * 255.0f);
	return;
}

inline void WhiteTex (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	*data = 255;
}

inline void Empty1D (Texture2D *tex, unsigned char *data, int x, int maxX) {
	data[0] = data[1] = data[2] = data[3] = 128;
}
inline void Empty2D (Texture2D *tex, unsigned char *data, int x, int y, int maxX, int maxY) {
	data[0] = data[1] = data[2] = data[3] = 128;
}
inline void Empty3D( unsigned char *data, int x, int y, int z, int maxX, int maxY, int maxZ ) {
	data[0] = data[1] = data[2] = data[3] = 128;
}
inline void EmptyCube( unsigned char *data, Vector3f normal ) {
	data[0] = data[1] = data[2] = data[3] = 128;
}

void GenerateDitherTextures()
{
	const int width = 4;
	const int height = 4;
	const int numSlices = 16;
	const int numPixelsPerSlice = width*height;

	const unsigned char mask [numPixelsPerSlice] = 
	{
		0,9,3,9,
		9,4,9,7,
		2,9,1,9,
		9,6,9,5,
	};

	gDitherMaskTex = CreateObjectFromCode<Texture3D>();
	gDitherMaskTex->SetHideFlags(Object::kHideAndDontSave);
	gDitherMaskTex->InitTexture (width, height, 16, kTexFormatAlpha8, false);
	gDitherMaskTex->GetSettings().m_Aniso = 0; // disable aniso
	gDitherMaskTex->GetSettings().m_FilterMode = 0; // disable aniso
	
	gDitherMaskTex->ApplySettings ();

	unsigned char* data = (unsigned char*)gDitherMaskTex->GetImageDataPointer();
	for(int slice = 0; slice < numSlices/2; slice++)
	{
		int index = slice;
		int invIndex = numSlices-1-slice;
		unsigned char* sliceData0 = data + numPixelsPerSlice * index;
		unsigned char* sliceData1 = data + numPixelsPerSlice * invIndex;
		for(int i = 0; i < numPixelsPerSlice; i++)
		{
			unsigned char maskValue = mask[i] >= slice ? 0x00 : 0xff;
			sliceData0[i] = maskValue;
			sliceData1[i] = ~maskValue;
		}
	}
	gDitherMaskTex->UpdateImageData(false);
}


void builtintex::ReinitBuiltinTextures()
{
#	define INIT_BUILTIN_TEX(texI, tsI, tex) GetGfxDevice().GetBuiltinParamValues().GetWritableTexEnvParam(texI).InitFromTexture(tex, NULL, tsI)

	INIT_BUILTIN_TEX(kShaderTexEnvWhite, kShaderVecWhiteTexelSize, gWhiteTex);
	INIT_BUILTIN_TEX(kShaderTexEnvBlack, kShaderVecBlackTexelSize, gBlackTex);
	INIT_BUILTIN_TEX(kShaderTexEnvRed, kShaderVecRedTexelSize, s_RedTex);
	INIT_BUILTIN_TEX(kShaderTexEnvGray, kShaderVecGrayTexelSize, s_GrayTex);
	INIT_BUILTIN_TEX(kShaderTexEnvGrey, kShaderVecGreyTexelSize, s_GrayTex);
	INIT_BUILTIN_TEX(kShaderTexEnvGrayscaleRamp, kShaderVecGrayscaleRampTexelSize, s_GrayRampTex);
	INIT_BUILTIN_TEX(kShaderTexEnvGreyscaleRamp, kShaderVecGreyscaleRampTexelSize, s_GrayRampTex);
	INIT_BUILTIN_TEX(kShaderTexEnvBump, kShaderVecBumpTexelSize, s_NormalMapTex);
	INIT_BUILTIN_TEX(kShaderTexEnvLightmap, kShaderVecLightmapTexelSize, gBlackTex);
	INIT_BUILTIN_TEX(kShaderTexEnvUnityLightmap, kShaderVecUnityLightmapTexelSize, gBlackTex);
	INIT_BUILTIN_TEX(kShaderTexEnvUnityLightmapInd, kShaderVecUnityLightmapIndTexelSize, gBlackTex);
	INIT_BUILTIN_TEX(kShaderTexEnvUnityLightmapThird, kShaderVecUnityLightmapThirdTexelSize, gBlackTex);
	INIT_BUILTIN_TEX(kShaderTexEnvDitherMaskLOD, kShaderVecDitherMaskLODSize, gDitherMaskTex);
	INIT_BUILTIN_TEX(kShaderTexEnvRandomRotation, kShaderVecRandomRotationTexelSize, s_RandomRotationTex);

#	undef INIT_BUILTIN_TEX
}


void builtintex::GenerateBuiltinTextures()
{
	static bool texturesGenerated = false;
	if( texturesGenerated )
		return;
	texturesGenerated = true;

	s_NormalMapTex = BuildTexture<unsigned char> (4, 4, kTexFormatRGBA32, &EmptyNormalMap);
	
	gWhiteTex = BuildTexture<unsigned char> (4, 4, kTexFormatRGBA32, &White);
	
	gBlackTex = BuildTexture<unsigned char> (4, 4, kTexFormatRGBA32, &Black);
	
	// Random rotation texture for rotating points. Stores unsigned cos(theta) in .r and unsigned sin(theta) in .g
	s_RandomRotationTex = BuildTexture<unsigned char> (16, 16, kTexFormatRGBA32, &RandomRotation);

	// The red texture will be created with mipmaps. The reason is a driver bug in Nvidia GTX 4xx cards
	// (Win7, driver 270.61), which makes terrain's FirstPass splat map shader flicker like mad if
	// the control texture has no mips and is not compressed. This happens on any new terrain
	// (the red texture is used there by default).
	s_RedTex = BuildTexture<unsigned char> (4, 4, kTexFormatRGBA32, &Red, true);
	
	s_GrayTex = BuildTexture<unsigned char> (4, 4, kTexFormatRGBA32, &Gray);
	
	s_GrayRampTex = BuildTexture<unsigned char> (256, 2, kTexFormatRGBA32, &GrayscaleRamp);
	s_GrayRampTex->GetSettings().m_WrapMode = kTexWrapClamp;
	s_GrayRampTex->ApplySettings ();

	gHaloTex = BuildTexture<unsigned char> (64, 64, kTexFormatAlpha8, &HaloTex);
	gHaloTex->GetSettings ().m_WrapMode = kTexWrapClamp;
	gHaloTex->ApplySettings ();

	if (gGraphicsCaps.supportsTextureFormat[kTexFormatAlphaLum16])
		gAttenuationTex = BuildTexture<unsigned short> (1024, 1, kTexFormatAlphaLum16, &LightAttenuation<unsigned short>);
	else
		gAttenuationTex = BuildTexture<unsigned char> (1024, 1, kTexFormatAlpha8, &LightAttenuation<unsigned char>);
	gAttenuationTex->GetSettings ().m_WrapMode = kTexWrapClamp;
	gAttenuationTex->ApplySettings ();

	// Initialize the textures used when no texture has been bound.
	gDefaultTextures[kTexDim2D] = BuildTexture<unsigned char> (16,16, kTexFormatRGBA32, &Empty2D)->GetTextureID();

	if (gGraphicsCaps.has3DTexture)
		gDefaultTextures[kTexDim3D] = Build3DTexture<unsigned char> (1,1,1, kTexFormatRGBA32, &Empty3D)->GetTextureID();

	gDefaultTextures[kTexDimCUBE] = BuildCubeTexture<unsigned char> (1, kTexFormatRGBA32, &EmptyCube)->GetTextureID();

	if (gGraphicsCaps.has3DTexture)
		GenerateDitherTextures();

	// Make the default for a 'any' be the same as the 2D case
	gDefaultTextures[kTexDimAny] = gDefaultTextures[kTexDim2D];

	ReinitBuiltinTextures();
}

Texture2D* builtintex::GetWhiteTexture() { return gWhiteTex; }
Texture2D* builtintex::GetBlackTexture() { return gBlackTex; }
Texture3D* builtintex::GetDitherMaskTexture() { return gDitherMaskTex; }
Texture* builtintex::GetAttenuationTexture() { return gAttenuationTex; }
Texture* builtintex::GetHaloTexture() { return gHaloTex; }
TextureID builtintex::GetDefaultTexture( TextureDimension texDim ) { return gDefaultTextures[texDim]; }
TextureID builtintex::GetBlackTextureID () { return gBlackTex->GetTextureID(); }
