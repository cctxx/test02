#include "UnityPrefix.h"
#include "SplatDatabase.h"

#if ENABLE_TERRAIN

#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Graphics/Image.h"
#include "TerrainData.h"
#include "Runtime/Scripting/Scripting.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#endif

#define PRINT_BASEMAP_TIME 0

#if PRINT_BASEMAP_TIME
double GetTimeSinceStartup();
#endif


using namespace std;

SplatPrototype::SplatPrototype ()
:	tileSize(15, 15),
	tileOffset(0, 0)
{	
	
}

SplatDatabase::SplatDatabase (TerrainData *owner)
:	m_BaseMap(NULL)
,	m_AlphamapResolution(512)
,	m_BaseMapResolution(512)
,   m_BaseMapDirty (true)
{
	m_TerrainData = owner;
}

SplatDatabase::~SplatDatabase()
{
	if( m_BaseMap )
		DestroySingleObject( m_BaseMap );
}

void SplatDatabase::UploadBasemap()
{
	if (!m_BaseMap->IsInstanceIDCreated())
	{
		Object::AllocateAndAssignInstanceID(m_BaseMap);
		m_BaseMap->SetHideFlags (Object::kHideAndDontSave);
		m_BaseMap->SetWrapMode( kTexWrapClamp );
	}
	m_BaseMap->AwakeFromLoad(kDefaultAwakeFromLoad);
}


// Typical time spent in recalculate base maps (core duo w/ gcc):
// 6 splats, basemap size 2048:
//   0.53 seconds (0.072 getting the mips, 0.031 getting alpha map, 0.268 blending, 0.16 uploading texture)
// 6 splats, basemap size 1024:
//   0.21 seconds (0.072 getting the mips, 0.031 getting alpha map, 0.067 blending, 0.04 uploading texture)
//
// Here we operate on ColorRGBA32 colors. The first quick implementation used ColorRGBAf,
// that was about 40% slower.

void SplatDatabase::RecalculateBasemap(bool allowUpload)
{
	//
	// create/resize base map texture
	
	int basemapSize = m_BaseMapResolution;
	if( !m_BaseMap )
	{
		m_BaseMap = NEW_OBJECT_FULL (Texture2D, kCreateObjectFromNonMainThread);
		m_BaseMap->Reset();

		// ok, just from performance standpoint we don't want to upload texture here
		// or get an assert from uninited texture, so, let's cheat
		m_BaseMap->HackSetAwakeDidLoadThreadedWasCalled();
		
		// uncomment this for proper handling (but assert will fire in current impl)
		//m_BaseMap->AwakeFromLoad(kDidLoadThreaded);

		m_BaseMap->InitTexture( basemapSize, basemapSize, kTexFormatRGBA32, Texture2D::kMipmapMask | Texture2D::kThreadedInitialize, 1 );
		
		// uncomment this for proper handling (but you will upload texture twice in current impl)
		//m_BaseMap->AwakeFromLoad(kDidLoadThreaded);
	}
	else
	{
		if( m_BaseMap->GetDataWidth() != basemapSize || m_BaseMap->GetDataHeight() != basemapSize || m_BaseMap->GetTextureFormat() != kTexFormatRGBA32 || !m_BaseMap->HasMipMap() )
			m_BaseMap->ResizeWithFormat (basemapSize, basemapSize, kTexFormatRGBA32, Texture2D::kMipmapMask);
	}
	
	Vector3f terrainSize = m_TerrainData->GetHeightmap().GetSize();

	#if PRINT_BASEMAP_TIME
	double t0 = GetTimeSinceStartup();
	#endif
	
	#define SPLAT_FIX_BITS 16	// use 16.16 fixed point
	
	// Note to optimizers: separating this struct into hot (x,y,yoffset) & cold (the rest) data
	// actually made it slower (about 20%, core duo w/ gcc), due to more housekeeping in the
	// innermost loop.
	struct SplatData
	{
		int xmask;	// 16.16 fixed point
		int ymask;	// 16.16 fixed point
		int dx;		// 16.16 fixed point
		int dy;		// 16.16 fixed point
		int x;		// 16.16 fixed point
		int y;		// 16.16 fixed point
		int xoffset;// 16.16 fixed point
		int yoffset;
		int width;
		ColorRGBA32* mip;
	};

	
	//
	// Get data needed from the splat textures.
	// This gets the optimal mip level in RGBA32 colors, and computes step sizes etc.
	
	int splatCount = m_Splats.size();
	
	SplatData* splatData = new SplatData[splatCount];
	
	// Set up sRGB state for splat maps
	bool sRGBEncountered = false;
	for( int i = 0; i < splatCount; ++i )
	{
		// figure out which mip level of the splat map to use
		Texture2D* splatTex = dynamic_pptr_cast<Texture2D*> (InstanceIDToObjectThreadSafe(m_Splats[i].texture.GetInstanceID()));
		if( !splatTex )
		{
			// if no splat texture, fill in a dummy one white pixel
			ErrorStringObject( Format ("Terrain splat %d is null.", i), m_TerrainData );
			splatData[i].mip = new ColorRGBA32[1];
			splatData[i].mip[0] = 0xFFFFFFFF;
			splatData[i].width = 1;
			splatData[i].xmask = 0;
			splatData[i].ymask = 0;
			splatData[i].dx = 0;
			splatData[i].dy = 0;
			splatData[i].x = 0;
			splatData[i].y = 0;
			continue;
		}

		if (splatTex->GetStoredColorSpace () != kTexColorSpaceLinear)
			sRGBEncountered = true;

		#if !UNITY_EDITOR
		AssertIf(!splatTex->GetIsReadable());
		#endif
		int splatWidth = splatTex->GetDataWidth();
		int splatHeight = splatTex->GetDataHeight();
		float tilesX = terrainSize.x / m_Splats[i].tileSize.x;
		float tilesY = terrainSize.z / m_Splats[i].tileSize.y;
		int splatBaseWidth = std::max( (int)(basemapSize / tilesX), 1 );
		int splatBaseHeight = std::max( (int)(basemapSize / tilesY), 1 );
		float areaRatio = (splatWidth * splatHeight) / (splatBaseWidth * splatBaseHeight);

		const float tileOffsetX = m_Splats[i].tileOffset.x / terrainSize.x * tilesX;
		const float tileOffsetY = m_Splats[i].tileOffset.y / terrainSize.z * tilesY;
		
		int mipLevel = int(0.5f * Log2( areaRatio ));
		mipLevel = clamp( mipLevel, 0, splatTex->CountDataMipmaps() - 1);
		
		// get the pixels of this mip level
		int minSplatSize = GetMinimumTextureMipSizeForFormat( splatTex->GetTextureFormat() );
		int width = std::max( splatWidth >> mipLevel, minSplatSize );
		int height = std::max( splatHeight >> mipLevel, minSplatSize );
		splatData[i].mip = new ColorRGBA32[width * height];
		if( !splatTex->GetPixels32( mipLevel, splatData[i].mip ) )
		{
			memset( splatData[i].mip, 0, width*height*sizeof(ColorRGBA32) );
			ErrorStringObject( "Failed to get pixels of splat texture", m_TerrainData );
		}
		
		// compute step sizes for this splat
		splatData[i].width = width;
		splatData[i].xmask = (width << SPLAT_FIX_BITS) - 1;
		splatData[i].ymask = (height << SPLAT_FIX_BITS) - 1;
		float dx = tilesX * width / basemapSize;
		splatData[i].dx = (int)( dx * (1<<SPLAT_FIX_BITS) );
		float dy = tilesY * height / basemapSize;
		splatData[i].dy = (int)( dy * (1<<SPLAT_FIX_BITS) );
		splatData[i].xoffset	= (int)(tileOffsetX * width * (1<<SPLAT_FIX_BITS));
		int yoffset				= (int)(tileOffsetY * height * (1<<SPLAT_FIX_BITS));
		splatData[i].x = 0;
		splatData[i].y = yoffset + (splatData[i].dy / 2) & splatData[i].ymask; // start at mid-pixel
	}

	if (sRGBEncountered)
		m_BaseMap->SetStoredColorSpaceNoDirtyNoApply (kTexColorSpaceSRGB);
	else
		m_BaseMap->SetStoredColorSpaceNoDirtyNoApply (kTexColorSpaceLinear);
	
	#if PRINT_BASEMAP_TIME
	double t1 = GetTimeSinceStartup();
	#endif
	
	//
	// get alpha map
	
	dynamic_array<UInt8> alphamap;
	GetAlphamaps( alphamap );

	#if PRINT_BASEMAP_TIME
	double t2 = GetTimeSinceStartup();
	#endif
	
	//
	// blend splats directly into texture data
	
	AssertIf( m_BaseMap->GetTextureFormat() != kTexFormatRGBA32 );
	ColorRGBA32* pix = reinterpret_cast<ColorRGBA32*>( m_BaseMap->GetRawImageData() );
	
	int alphaFixStep = (m_AlphamapResolution << SPLAT_FIX_BITS) / basemapSize; // 16.16 fixed point
	int alphaFixY = 0;
	int pixIndex = 0;
	for( int y = 0; y < basemapSize; ++y, alphaFixY += alphaFixStep )
	{
		int alphaY = (alphaFixY >> SPLAT_FIX_BITS);
		int alphaYIndex = alphaY * m_AlphamapResolution * splatCount;
		
		int alphaFixX = 0;
		for( int s = 0; s < splatCount; ++s ) {
			splatData[s].x = (splatData[s].dx / 2 + splatData[s].xoffset) & splatData[s].xmask;
			splatData[s].yoffset = (splatData[s].y >> SPLAT_FIX_BITS) * splatData[s].width;
		}
		for( int x = 0; x < basemapSize; ++x, alphaFixX += alphaFixStep, ++pixIndex )
		{
			int alphaX = (alphaFixX >> SPLAT_FIX_BITS);
			int alphaXIndex = alphaX * splatCount;
			
			// Per-pixel loop: accumulate splat textures into final color.
			// No need for pixel clipping since alphamap weights are always normalized.
			UInt32 c = 0;
			for( int s = 0; s < splatCount; ++s )
			{
				SplatData& splat = splatData[s];
				ColorRGBA32 splatCol = splat.mip[ splat.yoffset + (splat.x >> SPLAT_FIX_BITS) ];
				
				// Note for optimizers: trying to get rid of multiplies in color*byte by using
				// a 256x256 lookup table is about 2x slower (core duo w/ gcc).
				
				// We always have to set alpha to zero. Using color * byte and then setting
				// alpha to zero does not get optimized away by gcc, so use directly changed code
				// from color * byte.
				int scale = alphamap[alphaYIndex + alphaXIndex + s];
				const UInt32& u = reinterpret_cast<const UInt32&> (splatCol);
				#if UNITY_LITTLE_ENDIAN
				UInt32 lsb = (((u & 0x00ff00ff) * scale) >> 8) & 0x00ff00ff;
				UInt32 msb = (((u & 0xff00ff00) >> 8) * scale) & 0x0000ff00;
				#else
				UInt32 lsb = (((u & 0x00ff00ff) * scale) >> 8) & 0x00ff0000;
				UInt32 msb = (((u & 0xff00ff00) >> 8) * scale) & 0xff00ff00;
				#endif
				c += lsb | msb;
				
				splat.x = (splat.x + splat.dx) & splat.xmask; // next splat texel
			}
			pix[pixIndex] = ColorRGBA32(c);
			
		}
		for( int s = 0; s < splatCount; ++s )
			splatData[s].y = (splatData[s].y + splatData[s].dy) & splatData[s].ymask;
	}
	
	if (splatCount == 0)
	{
		memset(pix, 0xFFFFFFFF, basemapSize * basemapSize * GetBytesFromTextureFormat(m_BaseMap->GetTextureFormat()));
	}
	
	#if PRINT_BASEMAP_TIME
	double t3 = GetTimeSinceStartup();
	#endif

	m_BaseMap->RebuildMipMap();
	
	// Upload base map texture

	if(allowUpload)
		UploadBasemap();	
		
	// Cleanup
	for( int i = 0; i < splatCount; ++i )
		delete[] splatData[i].mip;
	delete[] splatData;
	
	#if PRINT_BASEMAP_TIME
	double t4 = GetTimeSinceStartup();
	printf_console( "basemap time calc: %.2f (%.3f mips, %.3f alpha, %.3f blend, %.2f upload)\n", (t4-t0), (t1-t0), (t2-t1), (t3-t2), (t4-t3) );
	#endif
	m_BaseMapDirty = false;
}


void SplatDatabase::GetAlphamaps (int xBase, int yBase, int width, int height, float* buffer)
{
	int layers = GetDepth();

	ColorRGBAf *tempBuffer;
	ALLOC_TEMP(tempBuffer, ColorRGBAf, width * height);


	for (int m=0;m<m_AlphaTextures.size();m++)
	{
		int componentCount = min(layers - m * 4, 4);
		
		Texture2D* texture = m_AlphaTextures[m];
		if (texture) {
			texture->GetPixels (xBase, yBase, width, height, 0, tempBuffer);
		} else {
			ErrorStringObject (Format ("splatdatabase alphamap %d is null", m), m_TerrainData);
			memset (tempBuffer, 0, width * height * sizeof(ColorRGBAf));
		}
		
		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				float *pixel = reinterpret_cast<float*> (&tempBuffer[y*width+x]);
				for (int a=0;a<componentCount;a++) {
					int layer = m * 4+a;
					buffer[y * width * layers + x * layers + layer] = pixel[a];
				}
			}
		}
	}
}

void SplatDatabase::GetAlphamaps( dynamic_array<UInt8>& buffer )
{
	// resync the m_AlphamapResolution with textures in m_AlphaTextures array
	for (int m = 0; m < m_AlphaTextures.size(); ++m)
	{
		Texture2D* texture = dynamic_pptr_cast<Texture2D*>(InstanceIDToObjectThreadSafe(m_AlphaTextures[m].GetInstanceID()));
		if (texture == NULL)
		{
			ErrorStringObject(Format("splatdatabase alphamap %d is null", m), m_TerrainData);
			continue;
		}

		if (texture->GetDataWidth() != m_AlphamapResolution)
		{
			if (m == 0)
			{
				WarningStringObject(Format("splatdatabase alphamap %d texture size doesn't match alphamap resolution setting: please resave the terrain asset.", m), m_TerrainData);
				m_AlphamapResolution = texture->GetDataWidth();
			}
			else
			{
				ErrorStringObject(Format("splatdatabase alphamap %d texture size doesn't match to other alphamap textures.", m), m_TerrainData);
			}
		}
	}

	int size = m_AlphamapResolution;
	int pixelCount = size * size;
	int layers = GetDepth();

	buffer.resize_uninitialized( size * size * layers );
	
	ColorRGBA32 *tempBuffer;
	ALLOC_TEMP(tempBuffer, ColorRGBA32, size * size);
	
	for( int m=0;m<m_AlphaTextures.size();m++ )
	{
		int componentCount = min(layers - m * 4, 4);
		
		Texture2D* texture = dynamic_pptr_cast<Texture2D*>(InstanceIDToObjectThreadSafe(m_AlphaTextures[m].GetInstanceID()));
		if (texture != NULL && texture->GetDataWidth() == size && texture->GetDataHeight() == size)
		{
			texture->GetPixels32( 0, tempBuffer );
		}
		else
		{
			ErrorStringObject (Format ("splatdatabase alphamap %d is invalid", m), m_TerrainData);
			memset (tempBuffer, 0, pixelCount * sizeof(ColorRGBA32));
		}

		int bufferIndex = m * 4;
		for( int i = 0; i < pixelCount; ++i )
		{
			UInt8* pixel = reinterpret_cast<UInt8*>( &tempBuffer[i] );
			for( int a = 0; a < componentCount; a++ )
			{
				buffer[bufferIndex + a] = pixel[a];
			}
			bufferIndex += layers;
		}
	}
}


/// Assign back the alpha map in the given area
void SplatDatabase::SetAlphamaps (int xBase, int yBase, int width, int height, float* buffer)
{
	int layers = GetDepth ();

	ColorRGBAf *tempBuffer;
	ALLOC_TEMP(tempBuffer, ColorRGBAf, width * height);
	int alphamaps = m_AlphaTextures.size();
	for (int m=0; m < alphamaps; m++)
	{
		memset (tempBuffer, 0, width * height* sizeof (ColorRGBAf)); 
		int componentCount = min(layers - m * 4, 4);

		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				float *pixel = reinterpret_cast<float*> (&tempBuffer[y*width+x]);
				for (int a=0; a<componentCount; a++) {
					int layer = m * 4 + a;
					pixel[a] = buffer[y * width * layers + x * layers + layer];
				
				}
			}
		}
		Texture2D* texture = m_AlphaTextures[m];
		if (!texture) {
			ErrorStringObject (Format ("splatdatabase alphamap %d is null", m), m_TerrainData);
			continue;
		}
		
		texture->SetPixels(xBase, yBase, width, height, width * height, tempBuffer, 0 );
		texture->UpdateImageData();
	}
	m_BaseMapDirty = true;
}

static void ClearAlphaMap (Texture2D * map, const ColorRGBAf &color)
{
	ImageReference image;
	if( !map->GetWriteImageReference(&image, 0, 0) )
	{
		ErrorString("Unable to retrieve image reference");
		return;
	}
		
	ColorRGBA32 tempColor (color);
	ColorRGBA32 colorARGB (tempColor.a, tempColor.r, tempColor.g, tempColor.b);

	int texWidth = image.GetWidth();
	int texHeight = image.GetHeight();
	for( int iy = 0; iy < texHeight; ++iy )
	{
		ColorRGBA32* pixel = (ColorRGBA32*)image.GetRowPtr(iy);
		for( int ix = 0; ix < texWidth; ++ix )
		{
			*pixel = colorARGB;
			++pixel;
		}
	}
	map->UpdateImageData();
}


Texture2D *SplatDatabase::AllocateAlphamap (const ColorRGBAf &color)
{
	Texture2D* map = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad);
	map->ResizeWithFormat(m_AlphamapResolution, m_AlphamapResolution, kTexFormatARGB32, Texture2D::kMipmapMask);
	map->SetWrapMode (kTexWrapClamp);
	
	ClearAlphaMap(map, color);
	
	map->SetName(Format ("SplatAlpha %u", (int)m_AlphaTextures.size()).c_str ());
	#if UNITY_EDITOR
	if (m_TerrainData->IsPersistent ())
		AddAssetToSameFile (*map, *m_TerrainData);
	#endif
	return map;
}

void SplatDatabase::AwakeFromLoad (AwakeFromLoadMode mode)
{
	if ((mode & kDidLoadThreaded) == 0)
	{
		// Ensure that the alpha map textures are allocated properly.
		// When removing some splat texture, that can cause an alpha map to be destroyed.
		// If later the removal is undone, we need to recreate the alpha map texture again.
		int splatCount = m_Splats.size();
		int required = (splatCount / 4) + ((splatCount % 4) != 0 ? 1 : 0);
		AssertIf( m_AlphaTextures.size() != required );
		for( int i = 0; i < required; ++i )
		{
			Texture2D* tex = m_AlphaTextures[i];
			if( tex == NULL )
			{
				ColorRGBAf color = ColorRGBAf (0,0,0,0);
				if( i == 0 )
					color.r = 1.0f;
				m_AlphaTextures[i] = AllocateAlphamap (color);
			}
		}
		
		m_BaseMapDirty = true;
	}
}


void SplatDatabase::Init (int alphamapResolution, int basemapResolution)
{
	m_AlphamapResolution = alphamapResolution;
	m_BaseMapResolution = basemapResolution;
}

void SplatDatabase::SetAlphamapResolution (int res)
{
	m_AlphamapResolution = clamp( res, 16, 2048 );
	for (int i=0;i<m_AlphaTextures.size();i++)
	{
		Texture2D* map = m_AlphaTextures[i];
		if (map)
		{
			map->ResizeWithFormat(m_AlphamapResolution, m_AlphamapResolution, kTexFormatARGB32, Texture2D::kMipmapMask);
			ClearAlphaMap(map, i == 0 ? ColorRGBAf (1, 0, 0, 0) : ColorRGBAf (0, 0, 0, 0));
		}
	}
	RecalculateBasemap(true);
}

void SplatDatabase::SetBaseMapResolution( int res )
{
	m_BaseMapResolution = clamp( res, 16, 2048 );
	RecalculateBasemap(true);
}

bool SplatDatabase::RecalculateBasemapIfDirty()
{
	if (m_BaseMapDirty)
	{
		RecalculateBasemap(true);
		return true;
	}
	return false;
}
		
void SplatDatabase::SetSplatPrototypes (const vector<SplatPrototype> &splats )
{
	// TODO: TEST for adding & removing
	// TODO: renormalize & optionally ditch an alphatexture when removing one
	// Do we need another one
	int required = (splats.size() / 4) + ((splats.size() % 4) != 0 ? 1 : 0);
	if (m_AlphaTextures.size() < required)
	{
		for (int i = m_AlphaTextures.size(); i < required; i++)
		{
			ColorRGBAf color = ColorRGBAf (0,0,0,0);
			if (m_AlphaTextures.empty ())
				color.r = 1;
			m_AlphaTextures.push_back (AllocateAlphamap (color));
		}
	}
	else if ( m_AlphaTextures.size() > required)
	{
		for (int i = required; i < m_AlphaTextures.size(); i++)
		{
			DestroySingleObject(m_AlphaTextures[i]);
		}
		m_AlphaTextures.resize (required);
	}
	m_Splats = splats;
	RecalculateBasemap(true);
	m_TerrainData->SetDirty();
}

Texture2D * SplatDatabase::GetAlphaTexture (int index)
{
	return m_AlphaTextures[index];
}

Texture2D* SplatDatabase::GetBasemap()
{
	return m_BaseMap;
}


void SplatPrototypeToMono (const SplatPrototype &src, MonoSplatPrototype &dest) {
	dest.texture = Scripting::ScriptingWrapperFor (src.texture);
	dest.normalMap = Scripting::ScriptingWrapperFor (src.normalMap);
	dest.tileSize = src.tileSize;
	dest.tileOffset = src.tileOffset;
}
void SplatPrototypeToCpp (MonoSplatPrototype &src, SplatPrototype &dest) {
	dest.texture = ScriptingObjectToObject<Texture2D> (src.texture);
	dest.normalMap = ScriptingObjectToObject<Texture2D> (src.normalMap);
	dest.tileSize = src.tileSize;
	dest.tileOffset = src.tileOffset;
}


#endif // ENABLE_TERRAIN
