#include "UnityPrefix.h"
#include "TextureAtlas.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Shaders/GraphicsCaps.h"

using namespace std;

// Just implemented from this article:
// http://www.blackpawn.com/texts/lightmaps/default.html

bool PackTextureAtlas( Texture2D* atlas, int atlasMaximumSize, int textureCount, Texture2D** textures, Rectf* outRects, int padding, int textureMode );

struct Node
{
	Node() : taken(false) { child[0] = NULL; child[1] = NULL; }
	~Node() { delete child[0]; delete child[1]; }
	
	void Reset()
	{
		delete child[0]; delete child[1];
		child[0] = NULL; child[1] = NULL;
		taken = false;
	}

	Node* Insert( float width, float height, float padding, bool use4PixelBoundaries );

	Node*	child[2];
	Rectf	rect;
	bool	taken;
};


Node* Node::Insert( float width, float height, float padding, bool use4PixelBoundaries )
{
	// if we're not leaf, try inserting into children
	if( child[0] )
	{
		Node* newNode = child[0]->Insert( width, height, padding, use4PixelBoundaries );
		if( newNode )
			return newNode;
		return child[1]->Insert( width, height, padding, use4PixelBoundaries );
	}

	// we are leaf

	if( taken )
		return NULL; // already taken

	// will it fit?
	// 0.5 float error margin and we don't care about sub-texel overlaps anyway
	if( width > rect.Width() - padding + 0.5f || height > rect.Height() - padding + 0.5f )
		return NULL; // won't fit

	// if this a perfect or nearly perfect fit, take it
	float dw = rect.Width() - width;
	float dh = rect.Height() - height;
	if( dw <= padding*2 && dh <= padding*2 )
	{
		taken = true;
		return this;
	}
	if( use4PixelBoundaries && dw < 4 && dh < 4 )
	{
		taken = true;
		return this;
	}

	// split the node
	child[0] = new Node();
	child[1] = new Node();

	// decide which way to split
	if( dw > dh )
	{
		// horizontal children
		int split = int(width + padding);
		if( use4PixelBoundaries )
			split = (split + 3) & (~3);
		child[0]->rect = MinMaxRect( rect.x, rect.y, rect.x+width+padding, rect.GetBottom() );
		child[1]->rect = MinMaxRect( rect.x+split, rect.y, rect.GetRight(), rect.GetBottom() );
	}
	else
	{
		// vertical children
		int split = int(height + padding);
		if( use4PixelBoundaries )
			split = (split + 3) & (~3);
		child[0]->rect = MinMaxRect ( rect.x, rect.y, rect.GetRight(), rect.y+height+padding );
		child[1]->rect = MinMaxRect ( rect.x, rect.y+split, rect.GetRight(), rect.GetBottom() );
	}

	// insert into first child
	return child[0]->Insert( width, height, padding, use4PixelBoundaries );
}

typedef std::pair<int,int> IntPair;
typedef std::vector<IntPair> TextureSizes;

struct IndexSorter {
	bool operator()( int a, int b ) const
	{
		return sizes[a].first * sizes[a].second > sizes[b].first * sizes[b].second;
	}
	IndexSorter( const TextureSizes& s ) : sizes(s) { }
	const TextureSizes& sizes;
};

void PackAtlases (dynamic_array<Vector2f>& sizes, const int maxAtlasSize, const float padding, dynamic_array<Vector2f>& outOffsets, dynamic_array<int>& outIndices, int& atlasCount)
{
	const int count = sizes.size ();
	const bool use4PixelBoundaries = false;

	dynamic_array<Node> atlases;
	outOffsets.resize_uninitialized (count);
	outIndices.resize_uninitialized (count);

	for (int i = 0; i < count; ++i)
	{
		Node* node = NULL;
		int atlasIndex = -1;
		while (!node)
		{
			atlasIndex++;
			Vector2f& size = sizes[i];
			if (atlasIndex == atlases.size ())
			{
				Node atlas;
				atlas.rect.Set (0, 0, maxAtlasSize, maxAtlasSize);
				atlases.push_back (atlas);
				node = atlases[atlasIndex].Insert (size.x, size.y, padding, use4PixelBoundaries);
				if (!node)
				{
					// We just tried inserting into an empty atlas. If that didn't succeed, we need to make the current rect smaller to fit maxAtlasSize
					if (size.x > size.y)
					{
						size.y *= ((float)maxAtlasSize) / size.x;
						size.x = maxAtlasSize;
					}
					else
					{
						size.x *= ((float)maxAtlasSize) / size.y;
						size.y = maxAtlasSize;
					}
					node = atlases[atlasIndex].Insert (size.x, size.y, 0.0f, use4PixelBoundaries);
					DebugAssert (node);
				}
			}
			else
			{
				node = atlases[atlasIndex].Insert (size.x, size.y, padding, use4PixelBoundaries);
			}
		}
		outOffsets[i].Set (node->rect.x, node->rect.y);
		outIndices[i] = atlasIndex;
	}

	atlasCount = atlases.size ();

	// deallocate all the trees
	for (int i = 0; i < atlases.size (); ++i)
		atlases[i].Reset ();
}

bool PackTextureAtlasSimple( Texture2D* atlas, int atlasMaximumSize, int textureCount, Texture2D** textures, Rectf* outRects, int padding, bool upload, bool markNoLongerReadable )
{
	atlasMaximumSize = min(gGraphicsCaps.maxTextureSize, atlasMaximumSize);

	// Cleanup the texture set.
	// * Remove duplicate textures
	// * Remove null textures	
	vector<int>              remap;
	remap.resize(textureCount);
	vector<Texture2D*>  uniqueTextures; 
	for (int i=0;i<textureCount;i++)
	{
		// Completely ignore null textures
		if (textures[i] == NULL)
		{
			*outRects = Rectf (0,0,0,0);
			remap[i] = -1;
			continue;
		}
		
		// Find duplicate texture and update remap
		vector<Texture2D*> ::iterator found = find(uniqueTextures.begin(), uniqueTextures.end(), textures[i]);
		if (found != uniqueTextures.end())
		{
			remap[i] = distance(uniqueTextures.begin(), found);
		}
		else
		{
			remap[i] = uniqueTextures.size();
			uniqueTextures.push_back(textures[i]);
		}
	}

	if (!uniqueTextures.empty())
	{
		vector<Rectf> uniqueRects;
		uniqueRects.resize(uniqueTextures.size());

		// Do the heavy lifting
		if (!PackTextureAtlas(atlas, atlasMaximumSize, uniqueTextures.size(), &uniqueTextures[0], &uniqueRects[0], padding, upload ? 0 : Texture2D::kThreadedInitialize ))
			return false;
			
		// Copy out rects from unique
		for (int i=0;i<textureCount;i++)
		{
			if (remap[i] != -1)
				outRects[i] = uniqueRects[remap[i]];
		}
	}

	if (upload)
	{
		if (!IsAnyCompressedTextureFormat(atlas->GetTextureFormat()))
			atlas->RebuildMipMap ();

		if( markNoLongerReadable )
		{
			atlas->SetIsReadable(false);
			atlas->SetIsUnreloadable(false);
		}			

		atlas->AwakeFromLoad(kDefaultAwakeFromLoad);
	}
	return true;
}

enum PackingFormat {
	kPackingUncompressed,
	kPackingDXT1,
	kPackingDXT5,
};


bool PackTextureAtlas( Texture2D* atlas, int atlasMaximumSize, int textureCount, Texture2D** textures, Rectf* outRects, int padding, int textureOptions  )
{
	DebugAssertIf( !atlas || !textures || !outRects || textureCount <= 0 );
	const int kMinTextureSize = 4;
	const int kMinAtlasSize = 8;
	int i;
	atlasMaximumSize = max (atlasMaximumSize, kMinAtlasSize);

	PackingFormat packFormat = kPackingDXT1;
	bool packWithMipmaps = false;
	bool someInputHasNoMipmaps = false;

	// Immediately decrease input texture sizes that are too large to fit; figure out
	// result packing format and whether we'll have mipmaps.
	TextureSizes textureSizes;
	textureSizes.resize( textureCount );
	for( i = 0; i < textureCount; ++i )
	{
		IntPair& size = textureSizes[i];
		size.first = textures[i]->GetDataWidth();
		size.second = textures[i]->GetDataHeight();
		while( size.first > atlasMaximumSize && size.first > kMinTextureSize )
		{
			size.first /= 2;
			packFormat = kPackingUncompressed; // we'll have to scale down, switch to uncompressed
		}
		while( size.second > atlasMaximumSize && size.second > kMinTextureSize )
		{
			size.second /= 2;
			packFormat = kPackingUncompressed; // we'll have to scale down, switch to uncompressed
		}

		// Atlas format rules:
		// Defaults to DXT1
		// If there is a DXT5 texture, pack to DXT5 (expand DXT1 alpha to opaque)
		// If there is an uncompressed or DXT3 texture, pack to 32 bit uncompressed.
		TextureFormat texFormat = textures[i]->GetTextureFormat();
		if (texFormat == kTexFormatDXT1 || texFormat == kTexFormatDXT5)
		{
			// Incoming texture is DXT1 or DXT5
			// If currently we are packing to DXT1 and incoming is DXT5, switch to that.
			if( packFormat == kPackingDXT1 && texFormat == kTexFormatDXT5 )
				packFormat = kPackingDXT5;
		}
		else
		{
			// Incoming texture is anything else: pack to uncompressed
			packFormat = kPackingUncompressed;
		}

		// If any texture has mipmaps, then atlas will have them
		if( textures[i]->HasMipMap() )
			packWithMipmaps = true;
		else
			someInputHasNoMipmaps = true;
	}

	// If some input textures have mipmaps and some don't, then
	// pack to uncompressed atlas.
	if( packWithMipmaps && someInputHasNoMipmaps )
		packFormat = kPackingUncompressed;
	
	// Sort incoming textures by size; largest area first
	std::vector<int> sortedIndices;
	sortedIndices.resize( textureCount );
	for( i = 0; i < textureCount; ++i )
	{
		sortedIndices[i] = i;
	}
	std::sort( sortedIndices.begin(), sortedIndices.end(), IndexSorter(textureSizes) );
	
	// Calculate an initial lower bound estimate for the atlas width & height
	int totalPixels = 0;
	for( i = 0; i < textureCount; ++i )
	{
		IntPair& size = textureSizes[i];
		totalPixels += size.first * size.second;
	}	
	int atlasWidth = min<int>(NextPowerOfTwo(UInt32(Sqrt (totalPixels))), atlasMaximumSize);
	int atlasHeight = min<int>(NextPowerOfTwo(totalPixels / atlasWidth),  atlasMaximumSize);
	// Do the packing of rectangles
	bool packOk = true;
	const int kMaxPackIterations = 100;
	int packIterations = 0;
	std::vector<Node*>	textureNodes;
	textureNodes.resize( textureCount );

	// Create a tree to track occupied areas in the atlas
	Node tree;

	do {
		packOk = true;
		tree.Reset();
		tree.rect = MinMaxRect<float> ( 0, 0, atlasWidth, atlasHeight );

		bool use4PixelBoundaries = (packFormat != kPackingUncompressed);
		
		for( i = 0; i < textureCount; ++i )
		{
			int texIndex = sortedIndices[i];
			DebugAssertIf( texIndex < 0 || texIndex >= textureCount );
			int texWidth = textureSizes[texIndex].first;
			int texHeight = textureSizes[texIndex].second;
			Node* node = tree.Insert( texWidth, texHeight, padding, use4PixelBoundaries );
			textureNodes[texIndex] = node;
			if( !node )
			{
				// texture does not fit; break out, reduce sizes and repack again
				packOk = false;
				break;
			}
		}
		
		// packing failed - decrease image sizes and try again
		if( !packOk )
		{
			// First we just increase the atlas size and see if we can fit all textures in.
			if (atlasWidth != atlasMaximumSize || atlasHeight != atlasMaximumSize)
			{
				// Never increase beyond max size
				if (atlasWidth == atlasMaximumSize)
					atlasHeight *= 2;
				else if (atlasHeight == atlasMaximumSize)
					atlasWidth *= 2;
				// increase the smaller of width/height
				else if (atlasWidth < atlasHeight)
					atlasWidth *= 2;
				else
					atlasHeight *= 2;
			}
			else
			{
				// TODO: the decreasing logic can be arbitrarily more complex. E.g. decrease the largest
				// images first only, etc.
				for( i = 0; i < textureCount; ++i )
				{
					IntPair& size = textureSizes[i];
					if( size.first > kMinTextureSize && size.second > kMinTextureSize ) {
						size.first = size.first * 3 / 4;
						size.second = size.second * 3 / 4;
					}
				}
				
				// we'll scale images down, no DXT for ya
				packFormat = kPackingUncompressed;

				// Only update pack iterations, for decreasing texture size
				++packIterations;
			}
		
			AssertIf (atlasWidth > atlasMaximumSize);
			AssertIf (atlasHeight > atlasMaximumSize);
		}
	} while( !packOk && packIterations < kMaxPackIterations );

	if( !packOk )
		return false;


	// Fill out UV rectangles for the input textures
	for( i = 0; i < textureCount; ++i )
	{
		int texIndex = sortedIndices[i];
		DebugAssertIf( texIndex < 0 || texIndex >= textureCount );
		int texWidth = textureSizes[texIndex].first;
		int texHeight = textureSizes[texIndex].second;
		const Node* node = textureNodes[texIndex];
		AssertIf( !node );

		// Set the rectangle
		outRects[texIndex] = MinMaxRect (
			node->rect.x/atlasWidth,
			node->rect.y/atlasHeight,
			(node->rect.x+texWidth)/atlasWidth,
			(node->rect.y+texHeight)/atlasHeight );
	}

	
	// Initialize atlas texture
	TextureFormat atlasFormat;
	if( packFormat == kPackingDXT1 )
		atlasFormat = kTexFormatDXT1;
	else if( packFormat == kPackingDXT5 )
		atlasFormat = kTexFormatDXT5;
	else
		atlasFormat = kTexFormatARGB32;

	textureOptions |= packWithMipmaps ? (Texture2D::kMipmapMask) : (Texture2D::kNoMipmap);
	atlas->InitTexture( atlasWidth, atlasHeight, atlasFormat, textureOptions );

	// Packing into uncompressed texture atlas
	if( packFormat == kPackingUncompressed )
	{
		UInt8* atlasData = atlas->GetRawImageData();
		memset( atlasData, 0, atlas->GetRawImageData(1) - atlasData );
		
		// Blit textures into final destinations
		Image* decompressedImage = 0;
		const int numAtlasMips = atlas->CountDataMipmaps();
		
		for( i = 0; i < textureCount; ++i )
		{
			int texIndex = sortedIndices[i];
			DebugAssertIf( texIndex < 0 || texIndex >= textureCount );
			
			int texWidth = textureSizes[texIndex].first;
			int texHeight = textureSizes[texIndex].second;
			const Node* node = textureNodes[texIndex];
			AssertIf( !node );
			
			int destCoordX	= node->rect.x;
			int	destCoordY	= node->rect.y;
			int destWidth	= std::min(texWidth, std::max(1, (int)node->rect.width - padding));
			int destHeight	= std::min(texHeight, std::max(1, (int)node->rect.height - padding));
			
			int atlasMipWidth = atlasWidth;
			int atlasMipHeight = atlasHeight;
			// copy all mips to atlas
			for ( int mip=0, numMips=std::min( textures[texIndex]->CountDataMipmaps(), numAtlasMips ); mip!=numMips; ++mip )
			{
				ImageReference atlasImgRef;
				atlas->GetWriteImageReference ( &atlasImgRef, 0, mip );
				// get texture rect in atlas for current source texture
				ImageReference destRect = atlasImgRef.ClipImage( destCoordX, destCoordY, destWidth, destHeight );
				
				ImageReference srcMip;
				if ( textures[texIndex]->GetWriteImageReference( &srcMip, 0, mip ) )
				{
                    ImageReference::BlitMode blit_mode = ImageReference::BLIT_BILINEAR_SCALE; 
                    if ( destWidth==srcMip.GetWidth() && destHeight==srcMip.GetHeight() )
                        blit_mode = ImageReference::BLIT_COPY;
                    destRect.BlitImage( srcMip, blit_mode );
				}
				else
				{
					if( !decompressedImage )
						decompressedImage = new Image( texWidth, texHeight, kTexFormatRGBA32 );
					else
						decompressedImage->SetImage( texWidth, texHeight, kTexFormatRGBA32, false );
					
					textures[texIndex]->ExtractImage( decompressedImage, 0 );
                    ImageReference::BlitMode blit_mode = ImageReference::BLIT_BILINEAR_SCALE;
                    if ( destWidth==decompressedImage->GetWidth() && destHeight==decompressedImage->GetHeight() )
                        blit_mode = ImageReference::BLIT_COPY;
					destRect.BlitImage( *decompressedImage, blit_mode );
				}
				
				// Go to next mip level
				destCoordX /= 2;
				destCoordY /= 2;
				destWidth /= 2;
				destHeight /= 2;
				atlasMipWidth = std::max( atlasMipWidth/2, 1 );
				atlasMipHeight = std::max( atlasMipHeight/2, 1 );
				texWidth = std::max( texWidth/2, 1 );
				texHeight = std::max( texHeight/2, 1 );
			}
		}
		delete decompressedImage;
	}
	// Packing into compressed texture atlas
	else
	{
		UInt8* atlasData = atlas->GetRawImageData();
		bool atlasDXT1 = (atlasFormat==kTexFormatDXT1);
		int blockBytes = atlasDXT1 ? 8 : 16;
		memset( atlasData, 0, atlas->GetRawImageData(1) - atlasData );

		// Blit textures into final destinations
		for( i = 0; i < textureCount; ++i )
		{
			int texIndex = sortedIndices[i];
			DebugAssertIf( texIndex < 0 || texIndex >= textureCount );
			int texWidth = textureSizes[texIndex].first;
			int texHeight = textureSizes[texIndex].second;
			Texture2D* src = textures[texIndex];
			int mipCount = std::min( src->CountDataMipmaps(), atlas->CountDataMipmaps() );
			AssertIf( texWidth != src->GetDataWidth() || texHeight != src->GetDataHeight() );
			const Node* node = textureNodes[texIndex];
			AssertIf( !node );
			int destCoordX = int(node->rect.x), destCoordY = int(node->rect.y);
			int destWidth = int(node->rect.width), destHeight = int(node->rect.height);
			AssertIf( (destCoordX & 3) != 0 || (destCoordY & 3) != 0 );
            
            UInt8* atlasMipData = atlasData;
			int atlasMipWidth = atlasWidth;
			int atlasMipHeight = atlasHeight;
			const UInt8* srcPointer = src->GetRawImageData();

			if(srcPointer)
			{
				
				// blit mip levels while we have them
				for( int mip = 0; mip < mipCount; ++mip )
				{
					// Get pointer where should we blit texture into
					int destBlockX = destCoordX / 4;
					int destBlockY = destCoordY / 4;
					UInt8* destPointer = atlasMipData + (destBlockY * atlasMipWidth/4 + destBlockX) * blockBytes;

					TextureFormat srcFormat = src->GetTextureFormat();
					AssertIf( !IsCompressedDXTTextureFormat(srcFormat) );
					if( srcFormat == atlasFormat )
					{
						BlitCopyCompressedImage( srcFormat, srcPointer,
							texWidth, texHeight,
							destPointer, atlasMipWidth, atlasMipHeight, false );
					}
					else if( srcFormat == kTexFormatDXT1 && atlasFormat == kTexFormatDXT5 )
					{
						BlitCopyCompressedDXT1ToDXT5( srcPointer,
							texWidth, texHeight,
							destPointer, atlasMipWidth, atlasMipHeight );
					}
					else
					{
						AssertString( "Unsupported format in compressed texture atlas" );
					}

					// Go to next mip level
					srcPointer += CalculateImageSize( texWidth, texHeight, srcFormat );
					atlasMipData += CalculateImageSize( atlasMipWidth, atlasMipHeight, atlasFormat );
					destCoordX /= 2;
					destCoordY /= 2;
					destWidth /= 2;
					destHeight /= 2;
					atlasMipWidth = std::max( atlasMipWidth / 2, 4 );
					atlasMipHeight = std::max( atlasMipHeight / 2, 4 );
					texWidth = std::max( texWidth / 2, 4 );
					texHeight = std::max( texHeight / 2, 4 );

					// Stop if we begin to straddle DXT block boundaries.
					if( (destCoordX & 3) != 0 || (destCoordY & 3) != 0 )
						break;

					// Stop if we don't fit into our initial area anymore.
					if( destWidth < 4 || destHeight < 4 )
						break;
				}
			}
			else
				ErrorStringMsg ("Could not read texture data for texture '%s'. Make sure that Read/Write access is enabled in the texture importer advanced settings\n", src->GetName());
		}
	}

	return true;
}
