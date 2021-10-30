#include "UnityPrefix.h"
#include "ImportMesh.h"
#include "ImportMeshTriangulate.h"
#include "Runtime/GfxDevice/opengl/UnityGL.h"

#if UNITY_WIN
#include <GL/glu.h>
#define TESS_FUNCTION_CALLCONV CALLBACK
#elif UNITY_LINUX
#include <GL/glu.h>
#define TESS_FUNCTION_CALLCONV
#else
#include <OpenGL/glu.h>
#define TESS_FUNCTION_CALLCONV
#endif


void TESS_FUNCTION_CALLCONV DummyEmitEdgeFlag (GLboolean flag);
void TESS_FUNCTION_CALLCONV EmitVertex (void* vertexData, void* userData);


struct TriangulationData
{
	const ImportMesh* input;
	ImportMesh* output;
};


bool Triangulate (const ImportMesh& input, ImportMesh& output, bool allowQuads)
{
	// We have a triangulated mesh.
	if (input.polygonSizes.empty())
	{
		output = input;
		output.polygonSizes.resize (input.polygons.size() / 3, 3); // set all polygon sizes to 3
		output.hasAnyQuads = false;
		return output.polygons.size() % 3 == 0;
	}
	
	{
		// calculate and reserve as much as it actually will actually need
		int faceCount = 0;
		int vertexCount = 0;
		for (int i = 0, size = input.polygonSizes.size(); i < size; ++i)
		{
			// we will skip all polygons which have less than 3 vertices in the next loop
			int faceSize = input.polygonSizes[i];
			if (allowQuads && faceSize == 4)
			{
				++faceCount;
				vertexCount += 4;
			}
			else if (faceSize >= 3)
			{
				faceCount += faceSize - 2;
				vertexCount += (faceSize - 2) * 3;
			}
		}
		Assert (faceCount >= 0);

		output.polygons.reserve(vertexCount);
		if (!input.normals.empty())
			output.normals.reserve(vertexCount);
		if (!input.tangents.empty())
			output.tangents.reserve(vertexCount);
		if (!input.uvs[0].empty())
			output.uvs[0].reserve(vertexCount);
		if (!input.uvs[1].empty())
			output.uvs[1].reserve(vertexCount);
		if (!input.colors.empty())
			output.colors.reserve(vertexCount);
		if (!input.materials.empty())
			output.materials.reserve(faceCount);
		output.polygonSizes.reserve(faceCount);
		output.hasAnyQuads = false;

		output.shapes.resize(input.shapes.size());
		output.shapeChannels = input.shapeChannels;
		for (size_t i = 0; i < input.shapes.size(); ++i)
		{
			const ImportBlendShape& inputShape = input.shapes[i];
			ImportBlendShape& outputShape = output.shapes[i];

			outputShape.vertices = inputShape.vertices;
			outputShape.targetWeight = inputShape.targetWeight;

			if (!inputShape.normals.empty())
				outputShape.normals.reserve(vertexCount);
			if (!inputShape.tangents.empty())
				outputShape.tangents.reserve(vertexCount);
		}
	}
	
	TriangulationData data;
	data.input = &input;
	data.output = &output;
	
	// Generate tesselator
	GLUtesselator* tesselator = gluNewTess();
	#  if defined(__GNUC__) && !defined(MAC_OS_X_VERSION_10_5)
	gluTessCallback(tesselator, GLU_TESS_EDGE_FLAG, reinterpret_cast<GLvoid(*)(...)> (DummyEmitEdgeFlag));
	gluTessCallback(tesselator, GLU_TESS_VERTEX_DATA, reinterpret_cast<GLvoid(*)(...)> (EmitVertex));
	#elif UNITY_WIN
	gluTessCallback(tesselator, GLU_TESS_EDGE_FLAG, reinterpret_cast<GLvoid(CALLBACK *)()> (DummyEmitEdgeFlag));
	gluTessCallback(tesselator, GLU_TESS_VERTEX_DATA, reinterpret_cast<GLvoid(CALLBACK *)()> (EmitVertex));
	#else
	gluTessCallback(tesselator, GLU_TESS_EDGE_FLAG, reinterpret_cast<GLvoid(*)()> (DummyEmitEdgeFlag));
	gluTessCallback(tesselator, GLU_TESS_VERTEX_DATA, reinterpret_cast<GLvoid(*)()> (EmitVertex));
	#endif
	
	
	int index = 0;
	int faceIndex = 0;
	for (int p=0;p<input.polygonSizes.size();p++)
	{
		int polygonSize = input.polygonSizes[p];
		if (index + polygonSize > input.polygons.size())
			return false;

		int nextFaceIndex = faceIndex;
		
		if (polygonSize == 3)
		{
			EmitVertex ((void*)index, &data); index++;
			EmitVertex ((void*)index, &data); index++;
			EmitVertex ((void*)index, &data); index++;
			nextFaceIndex += 1;
			output.polygonSizes.push_back (3);
		}
		else if (allowQuads && polygonSize == 4)
		{
			EmitVertex ((void*)index, &data); index++;
			EmitVertex ((void*)index, &data); index++;
			EmitVertex ((void*)index, &data); index++;
			EmitVertex ((void*)index, &data); index++;
			nextFaceIndex += 1;
			output.polygonSizes.push_back (4);
			output.hasAnyQuads = true;
		}
		else if (polygonSize == 4)
		{
			int curIndex = index;
			/*
			curIndex = index + 0;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 1;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 3;
			EmitVertex ((void*)curIndex, &data);
		
			curIndex = index + 2;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 3;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 1;
			EmitVertex ((void*)curIndex, &data);
			*/

			curIndex = index + 0;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 1;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 2;
			EmitVertex ((void*)curIndex, &data);
		
			curIndex = index + 0;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 2;
			EmitVertex ((void*)curIndex, &data);
			curIndex = index + 3;
			EmitVertex ((void*)curIndex, &data);
			
			
			index += 4;
			nextFaceIndex += 2;
			output.polygonSizes.push_back (3);
			output.polygonSizes.push_back (3);
		}
		else
		{
			size_t curPolySize = output.polygons.size();
			gluTessBeginPolygon(tesselator, &data);
			gluTessBeginContour(tesselator);

			for (int v=0;v<polygonSize;v++)
			{
				int vertexIndex = input.polygons[index];
				Vector3f vertex = input.vertices[vertexIndex];
				double gluVertex[3] = { vertex.x, vertex.y, vertex.z };
				gluTessVertex(tesselator, gluVertex, (void*)index);
				index++;
			}

			gluTessEndContour(tesselator);
			gluTessEndPolygon(tesselator);

			int newTris = (output.polygons.size() - curPolySize) / 3;
			for (int i = 0; i < newTris; ++i)
				output.polygonSizes.push_back (3);
			nextFaceIndex += newTris;
		}

		// Fill up material
		if (!input.materials.empty())
		{
			for (int i = faceIndex; i < nextFaceIndex; ++i)
				output.materials.push_back(input.materials[p]);
		}

		faceIndex = nextFaceIndex;
	}
	
	gluDeleteTess(tesselator);
	
	output.vertices = input.vertices;
	output.skin = input.skin;
	output.bones = input.bones;
	output.name = input.name;
	
	return true;
}

// Append one vertex to the output mesh
void TESS_FUNCTION_CALLCONV EmitVertex (void* vertexData, void* userData)
{
	int index = (int)vertexData;
	TriangulationData* meshes = (TriangulationData*)userData;
	const ImportMesh& input = *meshes->input;
	ImportMesh& output = *meshes->output;
	
	output.polygons.push_back (input.polygons[index]);
	
	// Add wedge data	
	if (!input.normals.empty())
		output.normals.push_back(input.normals[index]);
	if (!input.tangents.empty())
		output.tangents.push_back(input.tangents[index]);
	if (!input.uvs[0].empty())
		output.uvs[0].push_back(input.uvs[0][index]);
	if (!input.uvs[1].empty())
		output.uvs[1].push_back(input.uvs[1][index]);
	if (!input.colors.empty())
		output.colors.push_back(input.colors[index]);

	for (size_t i = 0; i < input.shapes.size(); ++i)
	{
		const ImportBlendShape& inputShape = input.shapes[i];
		ImportBlendShape& outputShape = output.shapes[i];

		if (!inputShape.normals.empty())
			outputShape.normals.push_back(inputShape.normals[index]);
		if (!inputShape.tangents.empty())
			outputShape.tangents.push_back(inputShape.tangents[index]);
	}
}


// This forces output to be triangles
void TESS_FUNCTION_CALLCONV DummyEmitEdgeFlag (GLboolean flag) { }
