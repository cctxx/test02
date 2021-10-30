#include "UnityPrefix.h"
#include "TerrainIndexGenerator.h"

#if ENABLE_TERRAIN

#include "Runtime/Graphics/TriStripper.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Terrain/Heightmap.h"

static unsigned int AddSliverTriangles (unsigned int *triangles, unsigned int index, int direction, int edgeMask);
static unsigned int AddSliverCorner (unsigned int* triangles, unsigned int index, int direction, int edgeMask);
static void FlipTriangle (unsigned int *triangles, unsigned int index);
static unsigned int AddQuad (unsigned int *triangles, unsigned int index, int xBase, int yBase);


struct CachedStrip {
	unsigned int count;
	unsigned short* triangles;
	
	CachedStrip() {count = 0; triangles = NULL;}
	~CachedStrip() { if(triangles) delete[] triangles;}
};

static CachedStrip gCachedStrips[16];

unsigned int *TerrainIndexGenerator::GetIndexBuffer (int edgeMask, unsigned int &count, int stride)
{
	unsigned int *triangles = new unsigned int[(kPatchSize) * (kPatchSize) * 6];
	unsigned int index = 0;
	int size = kPatchSize;
	
	int minX = 0;
	int minY = 0;
	int maxX = kPatchSize-1;
	int maxY = kPatchSize-1;
	
	if((edgeMask & kDirectionLeftFlag) == 0)
	{
		minX+=1;
		index = AddSliverTriangles (triangles, index, kDirectionLeft, edgeMask);
	}
	if((edgeMask & kDirectionRightFlag) == 0)
	{
		maxX-=1;
		index = AddSliverTriangles (triangles, index, kDirectionRight, edgeMask);
	}
	if((edgeMask & kDirectionUpFlag) == 0)
	{
		maxY-=1;
		index = AddSliverTriangles (triangles, index, kDirectionUp, edgeMask);
	}
	if((edgeMask & kDirectionDownFlag) == 0)
	{
		minY+=1;
		index = AddSliverTriangles (triangles, index, kDirectionDown, edgeMask);
	}

	if((edgeMask & kDirectionLeftFlag) == 0 || (edgeMask & kDirectionUpFlag) == 0)
		index = AddSliverCorner (triangles, index, kDirectionLeftUp, edgeMask);
	if((edgeMask & kDirectionRightFlag) == 0 || (edgeMask & kDirectionUpFlag) == 0)
		index = AddSliverCorner (triangles, index, kDirectionRightUp, edgeMask);
	if((edgeMask & kDirectionLeftFlag) == 0 || (edgeMask & kDirectionDownFlag) == 0)
		index = AddSliverCorner (triangles, index, kDirectionLeftDown, edgeMask);
	if((edgeMask & kDirectionRightFlag) == 0 || (edgeMask & kDirectionDownFlag) == 0)
		index = AddSliverCorner (triangles, index, kDirectionRightDown, edgeMask);
	
	for (int y=minY;y<maxY;y++)
	{
		for (int x=minX;x<maxX;x++)
		{
			// For each grid cell output two triangles
			triangles[index++] = y     + (x * size);
			triangles[index++] = (y+1) + x * size;
			triangles[index++] = (y+1) + (x + 1) * size;

			triangles[index++] = y     + x * size;
			triangles[index++] = (y+1) + (x + 1) * size;
			triangles[index++] = y     + (x + 1) * size;
		}
	}
	
	count = index;		
	return triangles;
}

unsigned short *TerrainIndexGenerator::GetOptimizedIndexStrip (int edgeMask, unsigned int &count)
{
	edgeMask &= kDirectionDirectNeighbourMask;
	if (gCachedStrips[edgeMask].triangles == NULL)
	{
		unsigned int *triangles = GetIndexBuffer (edgeMask, count, 0);
		
		Mesh::TemporaryIndexContainer newStrip;

		Stripify ((const UInt32*)triangles, count, newStrip);
		
		delete[] triangles;
		
		count = newStrip.size();
		unsigned short *strip = new unsigned short[count];
		for(int i=0;i<count;i++)
			strip[i] = newStrip[i];
		gCachedStrips[edgeMask].count = count;
		gCachedStrips[edgeMask].triangles = strip;
	}
	
	count = gCachedStrips[edgeMask].count;
	return gCachedStrips[edgeMask].triangles;
}

static void FlipTriangle (unsigned int *triangles, unsigned int index)
{
	int temp = triangles[index];
	triangles[index] = triangles[index+1];
	triangles[index+1] = temp;
}
	
static unsigned int AddQuad (unsigned int *triangles, unsigned int index, int xBase, int yBase)
{
	triangles[index++] = (xBase +  0) * kPatchSize + (yBase + 0);
	triangles[index++] = (xBase +  0) * kPatchSize + (yBase + 1);
	triangles[index++] = (xBase +  1) * kPatchSize + (yBase + 1);

	triangles[index++] = (xBase +  0) * kPatchSize + (yBase + 0);
	triangles[index++] = (xBase +  1) * kPatchSize + (yBase + 1);
	triangles[index++] = (xBase +  1) * kPatchSize + (yBase + 0);
	
	return index;
}
	
static unsigned int AddSliverCorner (unsigned int* triangles, unsigned int index, int direction, int edgeMask)
{
	int xBase, yBase, ox, oy;
	bool flip = false;
	
	int vMask = 0;
	int hMask = 0;

	if (direction == kDirectionLeftDown)
	{
		xBase = 1;
		yBase = 1;
		ox = 1;
		oy = 1;
		flip = false;

		hMask = 1 << kDirectionLeft;
		vMask = 1 << kDirectionDown;
	}
	else if (direction == kDirectionRightDown)
	{
		xBase = kPatchSize-2;
		yBase = 1;
		ox = -1;
		oy = 1;
		flip = true;

		hMask = 1 << kDirectionRight;
		vMask = 1 << kDirectionDown;
	}
	else if (direction == kDirectionLeftUp)
	{
		xBase = 1;
		yBase = kPatchSize-2;
		ox = 1;
		oy = -1;
		flip = true;

		hMask = 1 << kDirectionLeft;
		vMask = 1 << kDirectionUp;
	}
	else
	{
		xBase = kPatchSize-2;
		yBase = kPatchSize-2;
		ox = -1;
		oy = -1;
		flip = false;

		hMask = 1 << kDirectionRight;
		vMask = 1 << kDirectionUp;
	}

	int mask = 0;
	if ((hMask & edgeMask) != 0)
		mask |= 1;
	if ((vMask & edgeMask) != 0)
		mask |= 2;

	// Both edges are tesselated
	// Vertical edge is tesselated
	if (mask == 1)
	{
		// Stitch big down and small up
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase + 0);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase + 0);
		
		// rigth up small
		triangles[index++] = (xBase +  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase +  ox) * kPatchSize + (yBase + 0);
		
		// Down Big span
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase +  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase - oy);
		
		if (flip)
		{
			FlipTriangle(triangles, index - 9);
			FlipTriangle(triangles, index - 6);
			FlipTriangle(triangles, index - 3);
		}

	}
	// Horizontal edge is tesselated
	else if (mask == 2)
	{
		// Left up small
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase + oy);
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase + oy);

		// Left Big span
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase + oy);

		// Stitch right-down and big left span
		triangles[index++] = (xBase - ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase +  0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase +  0) * kPatchSize + (yBase - oy);
		
		if (flip)
		{
			FlipTriangle(triangles, index - 9);
			FlipTriangle(triangles, index - 6);
			FlipTriangle(triangles, index - 3);
		}

	}
	// Nothing tesselated
	else
	{
		// Left up small
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase + oy);
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase + oy);
		// right up small
		triangles[index++] = (xBase +  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase +  ox) * kPatchSize + (yBase + 0);
		// Left Big span
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase + oy);
		// Down Big span
		triangles[index++] = (xBase +   0) * kPatchSize + (yBase +  0);
		triangles[index++] = (xBase +  ox) * kPatchSize + (yBase - oy);
		triangles[index++] = (xBase -  ox) * kPatchSize + (yBase - oy);
		
		if (flip)
		{
			FlipTriangle(triangles, index - 12);
			FlipTriangle(triangles, index - 9);
			FlipTriangle(triangles, index - 6);
			FlipTriangle(triangles, index - 3);
		}
	}

	return index;		
}
		
static unsigned int AddSliverTriangles (unsigned int *triangles, unsigned int index, int direction, int edgeMask)
{
	int directionMask = 1 << direction;
	if ((edgeMask & directionMask) != 0)
	{
		for (int y=2;y<kPatchSize-3;y++)
		{
			if (direction == kDirectionLeft)
				index = AddQuad(triangles, index, 0, y);	
			else if (direction == kDirectionRight)
				index = AddQuad(triangles, index, kPatchSize - 2, y);	
			else if (direction == kDirectionUp)
				index = AddQuad(triangles, index, y, kPatchSize - 2);	
			else if (direction == kDirectionDown)
				index = AddQuad(triangles, index, y, 0);	
		}
	}
	else
	{
		for (int i=2;i<kPatchSize-3;i+=2)
		{
			if (direction == kDirectionLeft)
			{
				int x = 0;
				int y = i;

				// fixup bottom
				triangles[index++] = (x +  1) * kPatchSize + (y + 0);
				triangles[index++] = (x +  0) * kPatchSize + (y +  0);
				triangles[index++] = (x +  1) * kPatchSize + (y + 1);
				
				// Big span
				triangles[index++] = (x +   0) * kPatchSize + (y +  0);
				triangles[index++] = (x +   0) * kPatchSize + (y +  2);
				triangles[index++] = (x +   1) * kPatchSize + (y +  1);

				// fixup top
				triangles[index++] = (x +   0) * kPatchSize + (y +  2);
				triangles[index++] = (x +   1) * kPatchSize + (y +  2);
				triangles[index++] = (x +   1) * kPatchSize + (y + 1);
			}
			else if (direction == kDirectionRight)
			{
				int x = kPatchSize - 1;
				int y = i;
				
				// fixup bottom
				triangles[index++] = (x -  1) * kPatchSize + (y + 0);
				triangles[index++] = (x -  1) * kPatchSize + (y + 1);
				triangles[index++] = (x -   0) * kPatchSize + (y +  0);
				
				// Big span
				triangles[index++] = (x -   0) * kPatchSize + (y +  0);
				triangles[index++] = (x -   1) * kPatchSize + (y +  1);
				triangles[index++] = (x -   0) * kPatchSize + (y +  2);

				// fixup top
				triangles[index++] = (x -   0) * kPatchSize + (y +  2);
				triangles[index++] = (x -  1) * kPatchSize + (y + 1);
				triangles[index++] = (x -  1) * kPatchSize + (y +  2);
			}				
			else if (direction == kDirectionDown)
			{
				int x = i;
				int y = 0;

				// fixup bottom
				triangles[index++] = (x +  0) * kPatchSize + (y + 0);
				triangles[index++] = (x +  0) * kPatchSize + (y +  1);
				triangles[index++] = (x +  1) * kPatchSize + (y + 1);
				
				// Big span
				triangles[index++] = (x +  1) * kPatchSize + (y + 1);
				triangles[index++] = (x +  2) * kPatchSize + (y +  0);
				triangles[index++] = (x +  0) * kPatchSize + (y +  0);
				// fixup top
				triangles[index++] = (x +  2) * kPatchSize + (y +  0);
				triangles[index++] = (x +  1) * kPatchSize + (y +  1);
				triangles[index++] = (x +  2) * kPatchSize + (y +  1);
			}
			else
			{
				int x = i;
				int y = kPatchSize - 1;

				// fixup bottom
				triangles[index++] = (x +  0) * kPatchSize + (y - 0);
				triangles[index++] = (x +  1) * kPatchSize + (y - 1);
				triangles[index++] = (x +  0) * kPatchSize + (y -  1);
				
				// Big span
				triangles[index++] = (x +  1) * kPatchSize + (y - 1);
				triangles[index++] = (x +  0) * kPatchSize + (y -  0);
				triangles[index++] = (x +  2) * kPatchSize + (y -  0);
				// fixup top
				triangles[index++] = (x +  2) * kPatchSize + (y -  0);
				triangles[index++] = (x +  2) * kPatchSize + (y -  1);
				triangles[index++] = (x +  1) * kPatchSize + (y -  1);
			}
		}
	}
	return index;
}

#endif // ENABLE_TERRAIN
