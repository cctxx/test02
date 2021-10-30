#include "UnityPrefix.h"
#include "Runtime/Misc/MeshWelding.h"
#include "Editor/Src/AssetPipeline/ImportMesh.h"


inline UInt32 GetVector3HashValue (const Vector3f& value)
{
	const UInt32* h = (const UInt32*)(&value);
	UInt32 f = (h[0]+h[1]*11-(h[2]*17))&0x7fffffff; // avoid problems with +-0
	return (f>>22)^(f>>12)^(f);
}

/*
 In 12 operations, this code computes the next highest power of 2 for a 32-bit integer. The result may be expressed by the formula 1U << (lg(v - 1) + 1).
 It would be faster by 2 operations to use the formula and the log base 2 methed that uses a lookup table, but in some situations, 
 lookup tables are not suitable, so the above code may be best. 
 */

inline int nextPowerOfTwo (UInt32 v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v + (v==0);
}

inline bool CompareBone(const BoneInfluence& lhs, const BoneInfluence& rhs)
{
	for (int i=0;i<4;i++)
	{
		if (!CompareApproximately(lhs.weight[0], rhs.weight[0]) || lhs.boneIndex[i] != rhs.boneIndex[i])
			return false;
	}
	return true;
}


#if UNITY_EDITOR

/*-----------------------------------------------------------------------//*
 !
 * \brief    an array of vertex positions
 * \param   p   Array of vertex positions (will be modified!)
 * \param   N   Number of vertices in array
 * \return  Number of vertices after the welding operation
 * \note    The unique vertices are stored into the beginning of array
 *          'p'.
 * \note    This welder is "bit-exact", i.e. only duplicate vertices are
 *          removed. For distance-based welding a somewhat more complicated
 *          algorithm needs to be used.
 
 *-------------------------------------------------------------------------*/

int weld (std::vector<Vector3f>& vertices, dynamic_array<BoneInfluence>& skin, std::vector<ImportBlendShape>& shapes, std::vector<int>& remap)
{
    const int NIL       = -1;                               // linked list terminator symbol
    int     outputCount = 0;                                // # of output vertices
    int     hashSize    = nextPowerOfTwo(vertices.size());                // size of the hash table
    int*    hashTable   = new int[hashSize + vertices.size()];            // hash table + linked list
    int*    next        = hashTable + hashSize;             // use bottom part as linked list
    
	remap.resize(vertices.size());
    
    memset (hashTable, NIL, (hashSize) * sizeof(int));      // init hash table (NIL = 0xFFFFFFFF so memset works)
    
    for (int i = 0; i < vertices.size(); i++)
    {
        const Vector3f&  v           = vertices[i];
        UInt32          hashValue   = GetVector3HashValue(v) & (hashSize-1);
        int             offset      = hashTable[hashValue];
        while (offset != NIL)
        {
        	Assert (offset < i);
        	bool euqals = (vertices[offset] == v);

			if (euqals && !skin.empty() && !CompareBone(skin[i], skin[offset]))
    	    	euqals = false;

			if (euqals && !shapes.empty())
			{
				for (int j = 0; j < shapes.size(); ++j)
				{
					if (shapes[j].vertices[i] != shapes[j].vertices[offset])
					{
						euqals = false;
						break;
					}
				}
			}
        	
			if (euqals)
				break;

            offset = next[offset];
        }
        
        if (offset == NIL)                                  // no match found - copy vertex & add to hash
        {
        	remap[i]                = outputCount;
            vertices[outputCount]   = v;                    // copy vertex
            if (!skin.empty())
            	skin[outputCount]   = skin[i];
			for (int j = 0; j < shapes.size(); ++j)
				shapes[j].vertices[outputCount] = shapes[j].vertices[i];
            
            next[outputCount]       = hashTable[hashValue]; // link to hash table
            hashTable[hashValue]    = outputCount++;        // update hash heads and increase output counter
        }
        else
        {
        	Assert (offset < i);
	        remap[i] = offset;
        }
    }
    
    delete[] hashTable;                                     // cleanup
	if (outputCount < vertices.size())
	{
		vertices.resize(outputCount);
		if (!skin.empty())
			skin.resize_initialized(outputCount);
		for (int j = 0; j < shapes.size(); ++j)
			shapes[j].vertices.resize(outputCount);
		return true;
	}
	else
		return false;
}

void WeldVertices (ImportMesh& mesh)
{
	std::vector<int> remap;
	if (weld (mesh.vertices, mesh.skin, mesh.shapes, remap))
	{
		UInt32* indices = &mesh.polygons[0];
		for (int i=0;i<mesh.polygons.size();i++)
			indices[i] = remap[indices[i]];
	}
}

#endif

bool WeldVertexArray(dynamic_array<Vector3f>& vertices, dynamic_array<UInt16>& triangles, dynamic_array<UInt16>& remap)
{
	Mesh::BoneInfluenceContainer skin;
	return WeldVertexArray(vertices, skin, triangles, remap);
}

bool WeldVertexArray(dynamic_array<Vector3f>& vertices, Mesh::BoneInfluenceContainer& skin, dynamic_array<UInt16>& triangles, dynamic_array<UInt16>& remap)
{
	const int NIL       = -1;                               // linked list terminator symbol
	int     outputCount = 0;                                // # of output vertices
	int     hashSize    = nextPowerOfTwo(vertices.size());                // size of the hash table
	int*    hashTable   = new int[hashSize + vertices.size()];            // hash table + linked list
	int*    next        = hashTable + hashSize;             // use bottom part as linked list

	remap.resize_uninitialized(vertices.size());

	memset (hashTable, NIL, (hashSize) * sizeof(int));      // init hash table (NIL = 0xFFFFFFFF so memset works)
	
	for (int i = 0; i < vertices.size(); i++)
	{
		const Vector3f&  v           = vertices[i];
		UInt32          hashValue   = GetVector3HashValue(v) & (hashSize-1);
		int             offset      = hashTable[hashValue];
		while (offset != NIL)
		{
			Assert (offset < i);
			if (vertices[offset] == v)
			{
				if (skin.empty() || CompareBone(skin[i], skin[offset]))
					break;
			}
			offset = next[offset];
		}

		if (offset == NIL)                                  // no match found - copy vertex & add to hash
		{
			remap[i]                = outputCount;
			vertices[outputCount]   = v;                    // copy vertex
	
			if (!skin.empty())
				skin[outputCount]   = skin[i];

			next[outputCount]       = hashTable[hashValue]; // link to hash table
			hashTable[hashValue]    = outputCount++;        // update hash heads and increase output counter
		}
		else
		{
			Assert (offset < i);
			remap[i] = offset;
		}
	}

	delete[] hashTable;                                     // cleanup

	if (outputCount < vertices.size())
	{
		vertices.resize_uninitialized(outputCount);
		if (!skin.empty())
			skin.resize_uninitialized(outputCount);
		for (int i=0;i<triangles.size();i++)
			triangles[i] = remap[triangles[i]];
		return true;
	}
    return false;
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Utilities/ArrayUtility.h"
SUITE (VertexWeldingTests)
{
	
	TEST (TestVertexWelding)
	{
		Vector3f vertices[] = { Vector3f (0,0,0), Vector3f (1,0,0), Vector3f (1,0,0), Vector3f (0,0,0) };
		dynamic_array<Vector3f> dVertices; dVertices.assign(vertices, vertices + ARRAY_SIZE(vertices));
		
		UInt16 indices[] = { 0, 1, 2, 3 };
		dynamic_array<UInt16> dIndices; dIndices.assign(indices, indices + ARRAY_SIZE(indices));
		
		dynamic_array<UInt16> remap;
		
		WeldVertexArray(dVertices, dIndices, remap);
		
		CHECK_EQUAL(2, dVertices.size());
		CHECK(Vector3f(0,0,0) == dVertices[0]);
		CHECK(Vector3f(1,0,0) == dVertices[1]);
		
		CHECK(0 == dIndices[0]);
		CHECK(1 == dIndices[1]);
		CHECK(1 == dIndices[2]);
		CHECK(0 == dIndices[3]);
		
		CHECK_EQUAL(4, remap.size());
		CHECK_EQUAL(0, remap[0]);
		CHECK_EQUAL(1, remap[1]);
		CHECK_EQUAL(1, remap[2]);
		CHECK_EQUAL(0, remap[3]);
	}
}
#endif
