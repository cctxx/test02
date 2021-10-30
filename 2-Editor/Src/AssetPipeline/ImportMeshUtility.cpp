#include "UnityPrefix.h"
#include "ImportMeshUtility.h"
#include "Runtime/Filters/Mesh/MeshOptimizer.h"
#include "ImportMeshTriangulate.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Utilities/Utility.h"
#include <vector>
#include <sstream>
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Geometry/TangentSpaceCalculation.h"
#include "Runtime/Geometry/Plane.h"
#include "TangentSpace.h"
#include "Runtime/Utilities/vector_map.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "External/Tristripper/UnityAdjacency.h"
#include "Editor/Src/Utility/UnwrapImpl.h"


using namespace std;

static bool CheckFaceIndices (const ImportMesh& mesh)
{
	int vertexCount = mesh.vertices.size ();
	for (int i=0;i<mesh.polygons.size ();i++)
	{
		if (mesh.polygons[i] >= vertexCount)
			return false;
	}
	return true;
}

namespace
{
	void AddVertexByIndex(const ImportMesh& srcMesh, ImportMesh& dstMesh, int srcVertexIndex)
	{
		dstMesh.vertices.push_back (srcMesh.vertices[srcVertexIndex]);
		for (int i = 0; i < srcMesh.shapes.size(); ++i)
			dstMesh.shapes[i].vertices.push_back(srcMesh.shapes[i].vertices[srcVertexIndex]);

		if (!srcMesh.skin.empty ())
			dstMesh.skin.push_back (srcMesh.skin[srcVertexIndex]);
	}

	void AddPolygonAttribute(const ImportMesh& srcMesh, ImportMesh& dstMesh, int srcAttributeIndex)
	{
		if (!srcMesh.normals.empty ())
			dstMesh.normals.push_back (srcMesh.normals[srcAttributeIndex]);
		if (!srcMesh.colors.empty ())
			dstMesh.colors.push_back (srcMesh.colors[srcAttributeIndex]);
		if (!srcMesh.uvs[0].empty ())
			dstMesh.uvs[0].push_back (srcMesh.uvs[0][srcAttributeIndex]);
		if (!srcMesh.uvs[1].empty ())
			dstMesh.uvs[1].push_back (srcMesh.uvs[1][srcAttributeIndex]);
		if (!srcMesh.tangents.empty ())
			dstMesh.tangents.push_back (srcMesh.tangents[srcAttributeIndex]);

		for (size_t i = 0; i < srcMesh.shapes.size(); ++i)
		{
			const ImportBlendShape& srcShape = srcMesh.shapes[i];
			ImportBlendShape& dstShape = dstMesh.shapes[i];

			if (!srcShape.normals.empty())
				dstShape.normals.push_back(srcShape.normals[srcAttributeIndex]);
			if (!srcShape.tangents.empty())
				dstShape.tangents.push_back(srcShape.tangents[srcAttributeIndex]);
		}
	}
}

struct SplitMeshImplementation
{
	float normalDotAngle;
	float uvEpsilon;
	const ImportMesh& srcMesh;
	ImportMesh& dstMesh;

	SplitMeshImplementation (const ImportMesh& src, ImportMesh& dst, float splitAngle)
		: srcMesh (src),
		  dstMesh (dst)
	{
		uvEpsilon = 0.001F;
		normalDotAngle = cos (Deg2Rad (splitAngle)) - 0.001F; // NOTE: subtract is for more consistent results across platforms
		PerformSplit ();
	}

	inline void CopyVertexAttributes (int wedgeIndex, int dstIndex)
	{
		if (!srcMesh.normals.empty ())
			dstMesh.normals[dstIndex] = srcMesh.normals[wedgeIndex];
		if (!srcMesh.colors.empty ())
			dstMesh.colors[dstIndex] = srcMesh.colors[wedgeIndex];
		if (!srcMesh.uvs[0].empty ())
			dstMesh.uvs[0][dstIndex] = srcMesh.uvs[0][wedgeIndex];
		if (!srcMesh.uvs[1].empty ())
			dstMesh.uvs[1][dstIndex] = srcMesh.uvs[1][wedgeIndex];
		if (!srcMesh.tangents.empty ())
			dstMesh.tangents[dstIndex] = srcMesh.tangents[wedgeIndex];

		for (size_t i = 0; i < srcMesh.shapes.size(); ++i)
		{
			const ImportBlendShape& srcShape = srcMesh.shapes[i];
			ImportBlendShape& dstShape = dstMesh.shapes[i];

			if (!srcShape.normals.empty())
				dstShape.normals[dstIndex] = srcShape.normals[wedgeIndex];
			if (!srcShape.tangents.empty())
				dstShape.tangents[dstIndex] = srcShape.tangents[wedgeIndex];
	}
	}

	inline void AddVertex (int srcVertexIndex, int srcAttributeIndex)
	{
		AddVertexByIndex(srcMesh, dstMesh, srcVertexIndex);
		AddPolygonAttribute(srcMesh, dstMesh, srcAttributeIndex);
	}

	inline bool NeedTangentSplit (const Vector4f& lhs, const Vector4f& rhs)
	{
		if (Dot (Vector3f(lhs.x, lhs.y, lhs.z), Vector3f(rhs.x, rhs.y, rhs.z)) < normalDotAngle)
			return true;
		return !CompareApproximately (lhs.w, rhs.w);
	}

	inline bool NeedsSplitAttributes (int srcIndex, int dstIndex)
	{
		if (!srcMesh.normals.empty () && Dot (srcMesh.normals[srcIndex], dstMesh.normals[dstIndex]) < normalDotAngle)
			return true;
		if (!srcMesh.colors.empty () && srcMesh.colors[srcIndex] != dstMesh.colors[dstIndex])
			return true;
		if (!srcMesh.uvs[0].empty () && !CompareApproximately (srcMesh.uvs[0][srcIndex], dstMesh.uvs[0][dstIndex], uvEpsilon))
			return true;
		if (!srcMesh.uvs[1].empty () && !CompareApproximately (srcMesh.uvs[1][srcIndex], dstMesh.uvs[1][dstIndex], uvEpsilon))
			return true;
		if (!srcMesh.tangents.empty () && NeedTangentSplit (srcMesh.tangents[srcIndex], dstMesh.tangents[dstIndex]))
			return true;

		for (size_t i = 0; i < srcMesh.shapes.size(); ++i)
		{
			const ImportBlendShape& srcShape = srcMesh.shapes[i];
			const ImportBlendShape& dstShape = dstMesh.shapes[i];

			if (!srcShape.normals.empty() && Dot(srcShape.normals[srcIndex], dstShape.normals[dstIndex]) < normalDotAngle)
				return true;

			if (!srcShape.tangents.empty() && Dot(srcShape.tangents[srcIndex], dstShape.tangents[dstIndex]) < normalDotAngle)
				return true;
		}

		return false;
	}

	// Wedge = a per face vertex. The source mesh has all uvs, normals etc. stored as wedges. Thus srcMesh.normals.size = srcMesh.polygons.size.

	void PerformSplit ()
	{
		const int attributeCount = srcMesh.polygons.size ();
		dstMesh.Reserve (attributeCount, srcMesh.polygonSizes.size(), &srcMesh);

		// Initialize faces to source faces
		dstMesh.polygons = srcMesh.polygons;
		dstMesh.polygonSizes = srcMesh.polygonSizes;
		dstMesh.hasAnyQuads = srcMesh.hasAnyQuads;

		// Initialize other data
		dstMesh.vertices = srcMesh.vertices;
		dstMesh.skin = srcMesh.skin;
		dstMesh.materials = srcMesh.materials;
		dstMesh.name = srcMesh.name;
		dstMesh.shapeChannels = srcMesh.shapeChannels;

		// Initialize attributes to some sane default values
		if (!srcMesh.normals.empty ())
			dstMesh.normals.resize (srcMesh.vertices.size (), Vector3f (1.0F,1.0F,1.0F));
		if (!srcMesh.colors.empty ())
			dstMesh.colors.resize (srcMesh.vertices.size (), ColorRGBA32 (0xFFFFFFFF));
		if (!srcMesh.uvs[0].empty ())
			dstMesh.uvs[0].resize (srcMesh.vertices.size (), Vector2f (0.0F,0.0F));
		if (!srcMesh.uvs[1].empty ())
			dstMesh.uvs[1].resize (srcMesh.vertices.size (), Vector2f (0.0F,0.0F));
		if (!srcMesh.tangents.empty ())
			dstMesh.tangents.resize(srcMesh.vertices.size (), Vector4f (1.0f, 0.0f, 0.0f, 0.0F));

		dstMesh.shapes.resize(srcMesh.shapes.size());
		for (size_t i = 0; i < srcMesh.shapes.size(); ++i)
		{
			const ImportBlendShape& srcShape = srcMesh.shapes[i];
			ImportBlendShape& dstShape = dstMesh.shapes[i];

			dstShape.targetWeight = srcShape.targetWeight;
			dstShape.vertices = srcShape.vertices;
			if (!srcShape.normals.empty ())
				dstShape.normals.resize (srcShape.vertices.size(), Vector3f::zAxis);
			if (!srcShape.tangents.empty ())
				dstShape.tangents.resize (srcShape.vertices.size(), Vector3f::xAxis);
		}

		typedef list<int> AlreadySplitVertices;
		vector<AlreadySplitVertices> splitVertices;
		splitVertices.resize (srcMesh.vertices.size ());

		UInt32* indices = &dstMesh.polygons[0];
		int indexCount = dstMesh.polygons.size ();
		for (int i = 0; i < indexCount; ++i)
		{
			int vertexIndex = indices[i];

			// Go through the list of already assigned vertices and find the vertex with the same attributes
			int bestVertexSplitIndex = -1;
			AlreadySplitVertices& possibilities = splitVertices[vertexIndex];
			for (AlreadySplitVertices::iterator s=possibilities.begin ();s != possibilities.end ();s++)
			{
				if (!NeedsSplitAttributes (i, *s))
				{
					bestVertexSplitIndex = *s;
				}
			}

			// No vertex was found that could be reused!
			if (bestVertexSplitIndex == -1)
			{
				// We haven't visited this vertex at all!
				// Use the original vertex and replace the vertex attribute's with the current wedge attributes
				if (possibilities.empty ())
				{
					bestVertexSplitIndex = vertexIndex;
					CopyVertexAttributes (i, vertexIndex);
				}
				// We need to add a new vertex
				else
				{
					bestVertexSplitIndex = dstMesh.vertices.size ();
					AddVertex (vertexIndex, i);
				}

				// Add the vertex to the possible vertices which other wedges can share!
				possibilities.push_back (bestVertexSplitIndex);
			}

			indices[i] = bestVertexSplitIndex;
		}
	}
};

void SplitMesh (const ImportMesh& src, ImportMesh& dst, float splitAngle)
{
	SplitMeshImplementation split (src, dst, splitAngle);
}

// A quick and dirty implementation of mesh splitting that only splits across UV charts; and still puts all
// attributes per each vertex per each face
struct SplitMeshImplementationChart
{
	float uvEpsilon;
	const ImportMesh& srcMesh;
	ImportMesh& dstMesh;

	SplitMeshImplementationChart (const ImportMesh& src, ImportMesh& dst)
		:	srcMesh (src),
			dstMesh (dst)
	{
		uvEpsilon = 0.001F;
		PerformSplit ();
	}

	inline void AddVertex (int srcVertexIndex)
	{
		AddVertexByIndex(srcMesh, dstMesh, srcVertexIndex);
	}

	inline bool NeedsSplitAttributes (int index1, int index2)
	{
		if (!srcMesh.uvs[0].empty () && !CompareApproximately (srcMesh.uvs[0][index1], srcMesh.uvs[0][index2], uvEpsilon))
			return true;
		if (!srcMesh.uvs[1].empty () && !CompareApproximately (srcMesh.uvs[1][index2], srcMesh.uvs[1][index2], uvEpsilon))
			return true;

		return false;
	}


	void PerformSplit ()
	{
		const int attributeCount = srcMesh.polygons.size ();
		dstMesh.Reserve (attributeCount, srcMesh.polygonSizes.size(), &srcMesh);

		dstMesh.polygons = srcMesh.polygons;
		dstMesh.polygonSizes = srcMesh.polygonSizes;
		dstMesh.hasAnyQuads = srcMesh.hasAnyQuads;

		dstMesh.vertices = srcMesh.vertices;
		dstMesh.skin = srcMesh.skin;
		dstMesh.materials = srcMesh.materials;
		dstMesh.name = srcMesh.name;

		dstMesh.normals = srcMesh.normals;
		dstMesh.colors = srcMesh.colors;
		dstMesh.uvs[0] = srcMesh.uvs[0];
		dstMesh.uvs[1] = srcMesh.uvs[1];
		dstMesh.tangents = srcMesh.tangents;
		dstMesh.shapes = srcMesh.shapes;
		dstMesh.shapeChannels = srcMesh.shapeChannels;

		typedef list<int> AlreadySplitVertices;
		vector<AlreadySplitVertices> splitVertices;
		splitVertices.resize (srcMesh.vertices.size ());

		UInt32* indices = &dstMesh.polygons[0];
		int indexCount = dstMesh.polygons.size ();
		for (int i=0;i<indexCount;i++)
		{
			int vertexIndex = indices[i];

			// Go through the list of already assigned vertices and find the vertex with the attributes closest to the wedge!
			int bestVertexSplitIndex = -1;
			AlreadySplitVertices& possibilities = splitVertices[vertexIndex];
			for (AlreadySplitVertices::iterator s = possibilities.begin (); s != possibilities.end (); s++)
			{
				if (!NeedsSplitAttributes (i, *s))
					bestVertexSplitIndex = indices[*s];
			}

			// No vertex was found that could be reused!
			if (bestVertexSplitIndex == -1)
			{
				// We haven't visited this vertex at all!
				// Use the original vertex and replace the vertex attribute's with the current wedge attributes
				if (possibilities.empty ())
				{
					bestVertexSplitIndex = vertexIndex;
				}
				// We need to add a new vertex
				else
				{
					bestVertexSplitIndex = dstMesh.vertices.size ();
					AddVertex (vertexIndex);
				}

				// Add the vertex to the possible vertices which other wedges can share!
				possibilities.push_back (i);
			}

			indices[i] = bestVertexSplitIndex;
		}
	}
};

void SplitMeshChart (const ImportMesh& src, ImportMesh& dst)
{
	SplitMeshImplementationChart split (src, dst);
}


namespace
{
	void AddVertex(const ImportMesh& srcMesh, ImportMesh& dstMesh, const int srcVertexIndex, std::vector<int>& srcToDstRemap)
	{
		int dstVertexIndex = srcToDstRemap[srcVertexIndex];

		if (dstVertexIndex < 0)
		{
			AddVertexByIndex(srcMesh, dstMesh, srcVertexIndex);
			AddPolygonAttribute(srcMesh, dstMesh, srcVertexIndex);

			dstVertexIndex = srcToDstRemap[srcVertexIndex] = dstMesh.vertices.size() - 1;
		}

		dstMesh.polygons.push_back(dstVertexIndex);
	}

	bool ValidateShapes(const ImportMesh& mesh)
	{
		for (int i = 0; i < mesh.shapes.size(); ++i)
		{
			const ImportBlendShape& shape = mesh.shapes[i];

			Assert(mesh.vertices.size() == shape.vertices.size());
			Assert(shape.normals.empty() || mesh.normals.size() == shape.normals.size());
			Assert(shape.tangents.empty() || mesh.tangents.size() == shape.tangents.size());
		}

		return true;
	}

	void ValidateMesh(const ImportMesh& src, const bool validateIndexLimits)
	{
		Assert(src.normals.empty() || src.vertices.size() == src.normals.size());
		Assert(src.tangents.empty() || src.vertices.size() == src.tangents.size());
		Assert(src.colors.empty() || src.vertices.size() == src.colors.size());
		Assert(src.uvs[0].empty() || src.vertices.size() == src.uvs[0].size());
		Assert(src.uvs[1].empty() || src.vertices.size() == src.uvs[1].size());

		Assert(src.materials.empty() || src.polygonSizes.size() == src.materials.size());
		Assert(ValidateShapes(src));

		if (validateIndexLimits)
		{
			for (int i = 0; i < src.polygons.size(); ++i)
			{
				if (src.polygons[i] >= std::numeric_limits<UInt16>::max())
					ErrorString("Meshes may not have more than 65000 vertices at the moment");
			}
			AssertMsg(src.polygons.size() < std::numeric_limits<UInt16>::max() * 3, "Meshes may not have more than 65000 triangles at the moment");
		}
	}

	const int kMaxVertexCount = std::numeric_limits<UInt16>::max();
	//const int kMaxVertexCount = 500;
	const int kMaxIndexCount = std::numeric_limits<UInt16>::max() * 3;

	ImportMesh* InstantiateSplitMesh(const ImportMesh& src, std::vector<ImportMesh>& dstMeshes, const int currentMesh)
	{
		dstMeshes.push_back(ImportMesh());
		ImportMesh* dst = &dstMeshes[currentMesh];

		dst->Reserve(kMaxVertexCount, kMaxIndexCount / 3, &src);

		dst->name = src.name;
		dst->bones = src.bones;

		return dst;
	}

	enum MeshLimitResult { kMeshLimitNone, kMeshLimitDidSplit, kMeshLimitCantSplit };

	MeshLimitResult LimitMeshSize(const ImportMesh& src, std::vector<ImportMesh>& dstMeshes)
	{
		bool needsSplit = src.polygons.size() >= kMaxIndexCount;

		if (!needsSplit)
		{
			for (int i = 0; i < src.polygons.size(); ++i)
				if (src.polygons[i] >= kMaxVertexCount)
				{
					needsSplit = true;
					break;
				}
		}

		if (needsSplit && src.hasAnyQuads)
		{
			// Can not split meshes with quads yet
			return kMeshLimitCantSplit;
		}

		if (!needsSplit)
		{
			return kMeshLimitNone;
		}
		else
		{
			Assert(!src.polygons.empty());
			Assert(!src.hasAnyQuads);
			Adjacencies adjacienies(NULL, &src.polygons[0], src.polygons.size() / 3);

			// We use two structures for queuing polygons:
			// * queuedPolygons is used as a queue
			// * queuedPolygonSet is used to mark what is in queuedPolygons, so we don't get duplicates
			std::vector<char> queuedPolygonSet(src.polygons.size() / 3, 0);
			std::vector<UInt32> queuedPolygons;
			queuedPolygons.reserve(queuedPolygonSet.size());
			queuedPolygons.push_back(0);
			queuedPolygonSet[0] = 1;

			// currentQueuedPolygon is used for iterating queuedPolygons, so we don't have to do pop_front
			int currentQueuedPolygon = 0;
			int firstNonQueuedPolygon = 0;

			dstMeshes.clear();

			ValidateMesh(src, false);

			const int estimatedCount = src.polygons.size() / kMaxIndexCount + 1;
			dstMeshes.reserve(estimatedCount * 2);

			int currentMesh = 0;
			ImportMesh* dst = InstantiateSplitMesh(src, dstMeshes, currentMesh);

			std::vector<int> vertexRemap(src.vertices.size(), -1);

			while (currentQueuedPolygon < queuedPolygons.size())
			{
				if (dst->polygons.size() + 3 >= kMaxIndexCount ||
					dst->vertices.size() + 3 >= kMaxVertexCount)
				{
					dst = InstantiateSplitMesh(src, dstMeshes, ++currentMesh);

					// clearing up vertexRemap
					std::fill(vertexRemap.begin(), vertexRemap.end(), -1);
				}

				const int currentPolygon = queuedPolygons[currentQueuedPolygon++];

				AddVertex(src, *dst, src.polygons[currentPolygon*3+0], vertexRemap);
				AddVertex(src, *dst, src.polygons[currentPolygon*3+1], vertexRemap);
				AddVertex(src, *dst, src.polygons[currentPolygon*3+2], vertexRemap);
				dst->polygonSizes.push_back (3);

				if (!src.materials.empty())
					dst->materials.push_back(src.materials[currentPolygon]);

				// adding adjacent polygons into queuedPolygons list
				for (int i = 0; i < 3; ++i)
				{
					int adjacentPolygon = adjacienies.mFaces->ATri[i];
					if (IS_BOUNDARY(adjacentPolygon))
						continue;

					adjacentPolygon = MAKE_ADJ_TRI(adjacentPolygon);
					Assert(adjacentPolygon >= 0 && adjacentPolygon < queuedPolygonSet.size());

					if (!queuedPolygonSet[adjacentPolygon])
					{
						queuedPolygonSet[adjacentPolygon] = 1;
						queuedPolygons.push_back(adjacentPolygon);
					}
				}

				// if we ran out of adjacent polygons then just find first unconnected polygon
				if (currentQueuedPolygon == queuedPolygons.size())
				{
					// firstNonQueuedPolygon - start search where we finished last time
					for (int i = firstNonQueuedPolygon; i < queuedPolygonSet.size(); ++i)
						if (!queuedPolygonSet[i])
						{
							queuedPolygons.push_back(i);
							queuedPolygonSet[i] = 1;
							firstNonQueuedPolygon = i + 1;
							break;
						}
				}
			}

			for (int i = 0; i < dstMeshes.size(); ++i)
				ValidateMesh(dstMeshes[i], true);

			return kMeshLimitDidSplit;
		}
	}

	void ValidateInputMesh(const ImportMesh& mesh)
	{
		AssertIf (!mesh.normals.empty () && mesh.normals.size () != mesh.polygons.size ());
		AssertIf (!mesh.colors.empty () && mesh.colors.size () != mesh.polygons.size ());
		AssertIf (!mesh.uvs[0].empty () && mesh.uvs[0].size () != mesh.polygons.size ());
		AssertIf (!mesh.uvs[1].empty () && mesh.uvs[1].size () != mesh.polygons.size ());
		AssertIf (!mesh.materials.empty () && mesh.polygonSizes.empty () && mesh.materials.size () != mesh.polygons.size ()/3);
		AssertIf (!mesh.materials.empty () && !mesh.polygonSizes.empty () && mesh.materials.size () != mesh.polygonSizes.size ());
		AssertIf (!mesh.skin.empty () && mesh.skin.size () != mesh.vertices.size ());

		Assert(ValidateShapes(mesh));
	}
}


#define DEBUG_UNWRAPPER_DETERMINISM 0

void GenerateSecondaryUVSetImportMesh(ImportMesh& originalMesh, const ImportMeshSettings& settings, std::vector<std::string>& warnings, std::string& error)
{
	// Generate secondary UV set - used for lightmapping
	if (settings.generateSecondaryUV && originalMesh.polygons.size())
	{
		if (originalMesh.hasAnyQuads)
		{
			warnings.push_back(Format("Can't generate secondary UV for meshes with quads (mesh '%s')\n", originalMesh.name.c_str()));
		}
		else
		{
			if (originalMesh.uvs[1].size() != originalMesh.polygons.size())
				originalMesh.uvs[1].resize(originalMesh.polygons.size());

			UnwrapParam param;
			param.angleDistortionThreshold	= settings.secondaryUVAngleDistortion / 100.0f;
			param.areaDistortionThreshold	= settings.secondaryUVAreaDistortion / 100.0f;
			param.hardAngle					= settings.secondaryUVHardAngle;
			param.packMargin				= settings.secondaryUVPackMargin / 1024.0f;

			// unwrapper is implemented as dll - so we can't just pass std::string
			static const unsigned	kErrorBufferSize = 1024;
			char					unwrapErrorBuffer[kErrorBufferSize];

			#if DEBUG_UNWRAPPER_DETERMINISM
				MdFourGenerator input;
				input.Feed(reinterpret_cast<const char*> (&param), sizeof(param));
				input.Feed(reinterpret_cast<const char*> (&originalMesh.vertices[0].x), originalMesh.vertices.size() * sizeof(originalMesh.vertices[0]));
				input.Feed(reinterpret_cast<const char*> (&originalMesh.normals[0].x), originalMesh.normals.size() * sizeof(originalMesh.normals[0]));
				input.Feed(reinterpret_cast<const char*> (&originalMesh.polygons[0]), originalMesh.polygons.size() * sizeof(originalMesh.polygons[0]));
				input.Feed(reinterpret_cast<const char*> (&originalMesh.uvs[0][0].x), originalMesh.uvs[0].size() * sizeof(originalMesh.uvs[0][0]));

				printf_console("Generate Secondary UV Input hash: %s\n", MdFourToString(input.Finish()).c_str());
			#endif

			bool unwrapOk = UnwrapImpl::GenerateSecondaryUVSet( &originalMesh.vertices[0].x, originalMesh.vertices.size(),
												   &originalMesh.normals[0].x, &originalMesh.uvs[0][0].x,
												   &originalMesh.polygons[0], originalMesh.polygons.size()/3,
												   &originalMesh.uvs[1][0].x, param,
												   unwrapErrorBuffer, kErrorBufferSize
												   );

			#if DEBUG_UNWRAPPER_DETERMINISM
				MdFourGenerator output;
				output.Feed(reinterpret_cast<const char*> (&originalMesh.uvs[1][0].x), originalMesh.uvs[1].size() * sizeof(originalMesh.uvs[1][0]));

				printf_console("Generate Secondary UV Output hash: %s\n", MdFourToString(output.Finish()).c_str());
			#endif

			if( !unwrapOk )
				error += unwrapErrorBuffer;
		}
	}
}

void CreateTangentSpaceTangentsUnsplit(const ImportMesh& mesh, Vector4f* outTangents, float normalSmoothingAngle);
void CreateTangentSpaceTangentsUnsplit(const ImportMesh& mesh, const Vector3f* vertices, const Vector3f* normals, Vector4f* outTangents, const float normalSmoothingAngle);
void GenerateNormals (const ImportMesh& mesh, const std::vector<Vector3f>& vertices, std::vector<Vector3f>& normals, const float hardAngle);

bool GenerateMeshData(const string& meshName, const ImportMesh& constantMesh, const Matrix4x4f& transform, const ImportMeshSettings& settings, std::vector<std::string>& warnings, ImportMesh& splitMesh, std::string& error)
{
	ValidateInputMesh(constantMesh);
	error = "";

	// Make sure no face indices are out of range!
	if (!CheckFaceIndices (constantMesh))
	{
		error = Format("Mesh indices of %s are out of range!\n", meshName.c_str());
		return true;
	}

	const ImportMesh* currentMesh = &constantMesh;

	// Triangulate
	ImportMesh originalMesh;
	if (!Triangulate(*currentMesh, originalMesh, false))
	{
		error = Format("The mesh %s couldn't be triangulated. Try cleaning and triangulating the mesh before exporting.\n Please report this bug!\n", meshName.c_str());
		return true;
	}

	// Invert winding
	if (settings.invertWinding)
		InvertWinding (originalMesh);

	// Weld vertices
	if (settings.weldVertices)
		WeldVertices (originalMesh);

	// Remove unused vertices
	if (settings.optimizeMesh)
		RemoveUnusedVertices (originalMesh);

	// Swap uv sets
	if (settings.swapUVChannels && !originalMesh.uvs[1].empty())
		swap(originalMesh.uvs[0], originalMesh.uvs[1]);

	// Remove degenerate faces
	RemoveDegenerateFaces (originalMesh);

	// Generate secondary UV set - used for lightmapping
	GenerateSecondaryUVSetImportMesh(originalMesh, settings, warnings, error);

	if (settings.normalImportMode == kTangentSpaceOptionsNone)
	{
		if (!originalMesh.normals.empty())
		{
			AssertString(Format("Mesh %s should have no normals", meshName.c_str()));
			originalMesh.normals.clear();
		}
	}
	else
	{
		const bool normalCountMismatch = originalMesh.normals.size() != originalMesh.polygons.size();

		if (settings.normalImportMode == kTangentSpaceOptionsCalculate || normalCountMismatch)
		{
			if (settings.normalImportMode != kTangentSpaceOptionsCalculate && normalCountMismatch)
			{
				if (originalMesh.normals.empty())
					warnings.push_back(Format("Mesh '%s' has no normals. Recalculating normals.\n", originalMesh.name.c_str()));
				else
				{
					warnings.push_back(Format("Normal count (%u) doesn't match polygon count (%u) in mesh '%s'. Recalculating normals.\n",
						(int)originalMesh.normals.size(), (int)originalMesh.polygons.size(), originalMesh.name.c_str()));
				}
			}

			GenerateNormals(originalMesh, originalMesh.vertices, originalMesh.normals, settings.normalSmoothAngle);

			// Generate normals for blendShapes
			for (int i = 0; i < originalMesh.shapes.size(); ++i)
			{
				ImportBlendShape& shape = originalMesh.shapes[i];
				if (!shape.vertices.empty())
					GenerateNormals (originalMesh, shape.vertices, shape.normals, settings.normalSmoothAngle);
			}
		}
	}

	// Transform vertices and normals
	TransformMesh (originalMesh, transform);

	// Most often we want to split mesh by UV charts before calculating tangents (i.e. don't ever merge
	// tangents across UV seams).

	ImportMesh chartedMesh;
	//if (settings.tangentImportMode == kTangentSpaceOptionsCalculate && settings.splitTangentsAcrossUV)
    if (settings.splitTangentsAcrossUV)
		SplitMeshChart (originalMesh, chartedMesh);
	else
		chartedMesh = originalMesh;


	// Create tangents
	if (settings.tangentImportMode == kTangentSpaceOptionsCalculate)
	{
		if (originalMesh.normals.empty() && !chartedMesh.polygons.empty())
		{
			warnings.push_back(Format("Can't calculate tangents, because mesh '%s' doesn't contain normals.\n", originalMesh.name.c_str()));
		}
		else
		{
			// Create tangents with uv's
			if (chartedMesh.uvs[0].empty ())
				chartedMesh.uvs[0].resize (chartedMesh.polygons.size(), Vector2f (0.0F, 0.0F));

			chartedMesh.tangents.resize(chartedMesh.polygons.size());

			CreateTangentSpaceTangentsUnsplit (chartedMesh, &chartedMesh.tangents[0], settings.normalSmoothAngle);

			// Generate tangents&binormals for blendShapes
			for (int i = 0; i < chartedMesh.shapes.size(); ++i)
			{
				ImportBlendShape& shape = chartedMesh.shapes[i];
				if (!shape.vertices.empty() && !shape.normals.empty())
				{
					std::vector<Vector4f> tangents(chartedMesh.polygons.size());

					CreateTangentSpaceTangentsUnsplit (chartedMesh, &shape.vertices[0], &shape.normals[0], &tangents[0], settings.normalSmoothAngle);

					shape.tangents.resize(tangents.size());
					for (int j = 0; j < tangents.size(); ++j)
					{
						const Vector4f t = tangents[j];
						shape.tangents[j].Set(t.x, t.y, t.z);
					}
				}
			}
		}
	}

	// Final splitting of the mesh (because of mesh discontinuity in any attributes)
	SplitMesh (chartedMesh, splitMesh);

	return error != "";
}

void GenerateBlendShapeData (const ImportMesh& importMesh, BlendShapeData& shapeData)
{
	Assert(GetBlendShapeChannelCount(shapeData) == 0);
	Assert(importMesh.shapeChannels.size() != 0);
	
	BlendShapeVertices& vertices = shapeData.vertices;
	shapeData.shapes.resize_uninitialized(importMesh.shapes.size());
	shapeData.fullWeights.resize_uninitialized(importMesh.shapes.size());
	
	const size_t count = importMesh.vertices.size();
	
	///@TODO: Tangents is without w component...
	std::vector<Vector3f> deltaVertices(count), deltaNormals, deltaTangents;

	for (int i = 0; i < shapeData.shapes.size(); ++i)
	{
		const ImportBlendShape& inShape = importMesh.shapes[i];
		
		Assert(inShape.vertices.size() == count);

		deltaNormals.resize(inShape.normals.size());
		deltaTangents.resize(inShape.tangents.size());

		for (int j = 0; j < count; ++j)
		{
			deltaVertices[j] = inShape.vertices[j] - importMesh.vertices[j];
			if (!inShape.normals.empty())
				deltaNormals[j] = inShape.normals[j] - importMesh.normals[j];
			if (!inShape.tangents.empty())
			{
				const Vector4f& t = importMesh.tangents[j];
				deltaTangents[j] = inShape.tangents[j] - Vector3f(t.x, t.y, t.z);
			}
		}

		SetBlendShapeVertices(deltaVertices, deltaNormals, deltaTangents, vertices, shapeData.shapes[i]);
		
		shapeData.fullWeights[i] = inShape.targetWeight;
	}
	
	shapeData.channels.resize(importMesh.shapeChannels.size());
	for (int i=0;i<importMesh.shapeChannels.size();i++)
	{
		const ImportBlendShapeChannel& inChannel = importMesh.shapeChannels[i];
		InitializeChannel (inChannel.name, inChannel.frameIndex, inChannel.frameCount, shapeData.channels[i]);
	}
}

void GenerateBlendShapeData (const ImportMesh& importMesh, Mesh& mesh)
{
	if (importMesh.shapeChannels.empty())
		return;
	
	GenerateBlendShapeData(importMesh, mesh.GetWriteBlendShapeDataInternal ());
}

void FillLodMeshData(const ImportMesh& splitMesh, Mesh& lodMesh, const ImportMeshSettings& settings, std::vector<int>& lodMeshMaterials)
{
	// Assign vertex data to lodmesh
	unsigned vertexFormat = splitMesh.AdviseVertexFormat ();

	if (settings.tangentImportMode == kTangentSpaceOptionsNone)
		vertexFormat &= ~VERTEX_FORMAT1(Tangent);
	if (settings.normalImportMode == kTangentSpaceOptionsNone)
		vertexFormat &= ~VERTEX_FORMAT1(Normal);

	lodMesh.ResizeVertices (splitMesh.vertices.size (), vertexFormat);

	std::copy (splitMesh.vertices.begin(), splitMesh.vertices.end(), lodMesh.GetVertexBegin ());
	std::copy (splitMesh.uvs[0].begin(), splitMesh.uvs[0].end(), lodMesh.GetUvBegin (0));
	std::copy (splitMesh.uvs[1].begin(), splitMesh.uvs[1].end(), lodMesh.GetUvBegin (1));
	std::copy (splitMesh.colors.begin(), splitMesh.colors.end(), lodMesh.GetColorBegin ());
	if (settings.tangentImportMode != kTangentSpaceOptionsNone)
		std::copy (splitMesh.tangents.begin(), splitMesh.tangents.end(), lodMesh.GetTangentBegin ());
	if (settings.normalImportMode != kTangentSpaceOptionsNone)
		std::copy (splitMesh.normals.begin(), splitMesh.normals.end(), lodMesh.GetNormalBegin ());

	GenerateBlendShapeData(splitMesh, lodMesh);

	// Assign skin
	AssertIf (!splitMesh.skin.empty () && splitMesh.skin.size () != splitMesh.vertices.size ());
	lodMesh.GetSkin().assign( splitMesh.skin.begin(), splitMesh.skin.end() );

	// splitMesh.materials might have changed from original, because we do face reduction,
	// that's why lodMeshMaterials have to be calculated here
	lodMeshMaterials.clear();

	// TODO : this might end up with mesh which has 0 polygons (due to degeneration)
	// maybe remove these meshes altogether?

	// Assign mesh
	if (!splitMesh.materials.empty ())
	{
		// <material,topology> to mesh subset index lookup
		typedef std::pair<int,GfxPrimitiveType> SubsetKey;
		typedef vector_map<SubsetKey, int> SubsetLookup;
		SubsetLookup subsets;

		// Figure out different materials and subsets we have
		Assert (splitMesh.materials.size () == splitMesh.polygonSizes.size());
		for (int i = 0; i < splitMesh.polygonSizes.size (); ++i)
		{
			const int materialIndex = splitMesh.materials[i];
			GfxPrimitiveType topo = splitMesh.polygonSizes[i] == 4 ? kPrimitiveQuads : kPrimitiveTriangles;
			SubsetKey key (materialIndex, topo);
			if (subsets.insert (make_pair (key, subsets.size())).second)
				lodMeshMaterials.push_back (materialIndex);
		}

		const int subsetCount = subsets.size();

		// Allocate split faces
		vector<vector<UInt32> > splitFaces;
		splitFaces.resize (subsetCount);
		for (int i=0;i<splitFaces.size ();i++)
			splitFaces[i].reserve (splitMesh.polygons.size ());

		// Put faces into the separate per subset array
		vector<GfxPrimitiveType> subsetTopology(subsetCount);
		Assert (splitMesh.polygonSizes.size() == splitMesh.materials.size());
		for (int i = 0, idx = 0; i < splitMesh.polygonSizes.size(); ++i)
		{
			const int fs = splitMesh.polygonSizes[i];

			GfxPrimitiveType topo = splitMesh.polygonSizes[i] == 4 ? kPrimitiveQuads : kPrimitiveTriangles;
			SubsetKey key (splitMesh.materials[i], topo);
			int subsetIndex = subsets.find (key)->second;
			subsetTopology[subsetIndex] = topo;
			vector<UInt32>& buffer = splitFaces[subsetIndex];
			for (int j = 0; j < fs; ++j)
				buffer.push_back (splitMesh.polygons[idx+j]);
			idx += fs;
		}

		// Assign separate faces to mesh
		lodMesh.SetSubMeshCount (subsetCount);
		for (int i = 0; i < subsetCount; ++i)
			lodMesh.SetIndices (&splitFaces[i][0], splitFaces[i].size(), i, subsetTopology[i]);
	}
	else
	{
		lodMesh.SetSubMeshCount(1);
		lodMesh.SetIndices(&splitMesh.polygons[0], splitMesh.polygons.size(), 0, kPrimitiveTriangles);
	}
	AssertIf (!CheckFaceIndices (splitMesh));

	// currently mesh optimization can't handle quads and/or changes vertex orderings
	const bool canDoOptimization = !splitMesh.hasAnyQuads;

	if (canDoOptimization)
	{
		OptimizeIndexBuffers(lodMesh);
		if (settings.optimizeMesh)
		{
			OptimizeReorderVertexBuffer(lodMesh);
		}
	}
}

std::string GenerateMeshData (AssetImporter& assetImporter, const ImportMesh& constantMesh, const Matrix4x4f& transform, const ImportMeshSettings& settings, const std::string& lodMeshName, std::vector<Mesh*>& lodMesh, std::vector< std::vector<int> >& lodMeshMaterials, std::vector<std::string>& warnings)
{
	ImportMesh splitMesh;
	std::string error;
	if (GenerateMeshData(lodMeshName, constantMesh, transform, settings, warnings, splitMesh, error))
	{
		// if it fails we must to return 1 empty mesh
		lodMesh.resize(1);
		lodMeshMaterials.resize(1);
		lodMesh[0] = &assetImporter.ProduceAssetObject<Mesh>(lodMeshName);
		lodMesh[0]->Clear(false);

		return error;
	}
	ValidateMesh(splitMesh, false);

	std::vector<ImportMesh> dstMeshes;

	const MeshLimitResult meshLimit = LimitMeshSize(splitMesh, dstMeshes);
	if (meshLimit == kMeshLimitDidSplit)
	{
		Assert(dstMeshes.size() >= 1);
		lodMesh.resize(dstMeshes.size());
		lodMeshMaterials.resize(dstMeshes.size());

		std::ostringstream warning;
		warning << "Meshes may not have more than " << kMaxVertexCount << " vertices at the moment. "
			<< "Mesh '" << lodMeshName << "' will be split into " << dstMeshes.size() << " parts: ";

		for (int i = 0; i < dstMeshes.size(); ++i)
		{
			std::string subMeshName = lodMeshName + "_MeshPart" + IntToString(i);
			lodMesh[i] = &assetImporter.ProduceAssetObject<Mesh>(subMeshName);

			warning << "'" << subMeshName << "'";
			if (i + 1 < dstMeshes.size())
				warning << ", ";

			FillLodMeshData(dstMeshes[i], *lodMesh[i], settings, lodMeshMaterials[i]);
		}

		warning << ".";
		warnings.push_back(warning.str().c_str());
	}
	else if (meshLimit == kMeshLimitNone)
	{
		lodMesh.resize(1);
		lodMeshMaterials.resize(1);
		lodMesh[0] = &assetImporter.ProduceAssetObject<Mesh>(lodMeshName);

		FillLodMeshData(splitMesh, *lodMesh[0], settings, lodMeshMaterials[0]);
	}
	else if (meshLimit == kMeshLimitCantSplit)
	{
		// we must to return 1 empty mesh
		lodMesh.resize(1);
		lodMeshMaterials.resize(1);
		lodMesh[0] = &assetImporter.ProduceAssetObject<Mesh>(lodMeshName);
		lodMesh[0]->Clear(false);
		return "Meshes with quads can not have more than 65000 vertices or faces";
	}

	return "";
}


enum DegenFace { kDegenNone, kDegenFull, kDegenToTri };
static inline DegenFace IsDegenerateFace (const UInt32* f, const int faceSize)
{
	if (faceSize == 3)
	{
		// triangle
		return (f[0]==f[1] || f[0]==f[2] || f[1]==f[2]) ? kDegenFull : kDegenNone;
	}
	else if (faceSize == 4)
	{
		// quad

		// non-neighbor indices are the same: quad is fully degenerate (produces no output)
		if (f[0]==f[2] || f[1]==f[3])
			return kDegenFull;
		// two opposing sides are collapsed: fully degenerate
		if ((f[0]==f[1] && f[2]==f[3]) || (f[1]==f[2] && f[3]==f[0]))
			return kDegenFull;
		// just one side is collapsed: degenerate to triangle
		if (f[0]==f[1] || f[1]==f[2] || f[2]==f[3] || f[3]==f[0])
			return kDegenToTri;
		// otherwise, regular quad
		return kDegenNone;
	}
	else
	{
		AssertString ("unsupported face size");
		return kDegenFull;
	}
}

template <typename T>
static void AddFace (const typename std::vector<T>& src, typename std::vector<T>& dst, int idx, const UInt32* indices, int faceSize)
{
	if (src.empty())
		return;
	for (int i = 0; i < faceSize; ++i)
		dst.push_back (src[idx+indices[i]]);
}

void RemoveDegenerateFaces (ImportMesh& mesh)
{
	// Check if we have any degenerate faces
	const int faceCount = mesh.polygonSizes.size();
	int i, idx;
	for (i = 0, idx = 0; i < faceCount; ++i)
	{
		const int fs = mesh.polygonSizes[i];
		if (IsDegenerateFace(&mesh.polygons[idx], fs) != kDegenNone)
			break;
		idx += fs;
	}
	// No degenerate faces, return
	if (i == faceCount)
		return;

	ImportMesh temp;
	const int indexCount = mesh.polygons.size();
	temp.polygons.reserve (indexCount);
	temp.materials.reserve (faceCount);
	temp.polygonSizes.reserve (faceCount);

	temp.tangents.reserve (indexCount);
	temp.normals.reserve (indexCount);
	temp.colors.reserve (indexCount);
	temp.uvs[0].reserve (indexCount);
	temp.uvs[1].reserve (indexCount);

	temp.shapes.resize(mesh.shapes.size());
	for (size_t i = 0; i < mesh.shapes.size(); ++i)
	{
		ImportBlendShape& tempShape = temp.shapes[i];
		tempShape.normals.reserve(indexCount);
		tempShape.tangents.reserve(indexCount);
	}

	mesh.hasAnyQuads = false;

	// Build result and remove degenerates on the way.
	for (i = 0, idx = 0; i < faceCount; ++i)
	{
		const int fs = mesh.polygonSizes[i];
		const UInt32* face = &mesh.polygons[idx];
		DegenFace degen = IsDegenerateFace (face, fs);

		int addFaceSize = fs;
		UInt32 addIndices[4] = {0, 1, 2, 3};
		if (degen == kDegenToTri)
		{
			DebugAssert (fs == 4);
			if (face[0]==face[1])
				addIndices[0] = 1, addIndices[1] = 2, addIndices[2] = 3;
			else if (face[1]==face[2])
				addIndices[0] = 0, addIndices[1] = 2, addIndices[2] = 3;
			else if (face[2]==face[3])
				addIndices[0] = 0, addIndices[1] = 1, addIndices[2] = 3;
			else if (face[3]==face[0])
				addIndices[0] = 0, addIndices[1] = 1, addIndices[2] = 2;
			else {
				DebugAssert (false);
			}

			degen = kDegenNone;
			addFaceSize = 3;
		}

		if (degen == kDegenNone)
		{
			if (!mesh.materials.empty ())
				temp.materials.push_back (mesh.materials[i]);
			temp.polygonSizes.push_back (addFaceSize);

			AddFace (mesh.polygons, temp.polygons, idx, addIndices, addFaceSize);
			AddFace (mesh.normals, temp.normals, idx, addIndices, addFaceSize);
			AddFace (mesh.tangents, temp.tangents, idx, addIndices, addFaceSize);
			AddFace (mesh.colors, temp.colors, idx, addIndices, addFaceSize);
			AddFace (mesh.uvs[0], temp.uvs[0], idx, addIndices, addFaceSize);
			AddFace (mesh.uvs[1], temp.uvs[1], idx, addIndices, addFaceSize);
			if (addFaceSize == 4)
				mesh.hasAnyQuads = true;

			for (size_t s = 0; s < mesh.shapes.size(); ++s)
			{
				ImportBlendShape& meshShape = mesh.shapes[s];
				ImportBlendShape& tempShape = temp.shapes[s];
				AddFace (meshShape.normals, tempShape.normals, idx, addIndices, addFaceSize);
				AddFace (meshShape.tangents, tempShape.tangents, idx, addIndices, addFaceSize);
			}
		}
		idx += fs;
	}

	mesh.polygons.swap (temp.polygons);
	mesh.materials.swap (temp.materials);
	mesh.polygonSizes.swap (temp.polygonSizes);
	mesh.normals.swap (temp.normals);
	mesh.tangents.swap (temp.tangents);
	mesh.colors.swap (temp.colors);
	mesh.uvs[0].swap (temp.uvs[0]);
	mesh.uvs[1].swap (temp.uvs[1]);

	for (size_t i = 0; i < mesh.shapes.size(); ++i)
	{
		ImportBlendShape& meshShape = mesh.shapes[i];
		ImportBlendShape& tempShape = temp.shapes[i];
		meshShape.normals.swap (tempShape.normals);
		meshShape.tangents.swap (tempShape.tangents);
	}
}

// Creates a vertex -> connected faces list
// Every vertex contains a list of faces that use it
class ConnectedMesh
{
public:
	struct Vertex
	{
		UInt32* faces;
		int  faceCount;
	};

	vector<Vertex> vertices;

	ConnectedMesh (int vertexCount, const UInt32* indices, int indexCount, const UInt32* faceSizes, int faceCount);

	~ConnectedMesh ()
	{
		delete[] m_FaceAllocator;
	}

private:
	UInt32* m_FaceAllocator;
};


ConnectedMesh::ConnectedMesh (int vertexCount, const UInt32* indices, int indexCount, const UInt32* faceSizes, int faceCount)
{
	Vertex temp; temp.faces = NULL; temp.faceCount = 0;
	vertices.resize (vertexCount, temp);

	// Count faces that use each vertex
	for (int i = 0, idx = 0; i < faceCount; ++i)
	{
		const int fs = faceSizes[i];
		for (int e = 0; e < fs; ++e)
			vertices[indices[idx+e]].faceCount++;
		idx += fs;
	}

	// Assign memory to faces (reset faceCount - so we reuse it as a counter)
	m_FaceAllocator = new UInt32[indexCount];
	UInt32* memory = m_FaceAllocator;

	for (int i = 0; i < vertexCount; ++i)
	{
		vertices[i].faces = memory;
		memory += vertices[i].faceCount;
		vertices[i].faceCount = 0;
	}

	// Assign vertex->face connections
	for (int i = 0, idx = 0; i < faceCount; ++i)
	{
		const int fs = faceSizes[i];
		for (int e = 0; e < fs; ++e)
		{
			int vertexIndex = indices[idx + e];
			vertices[vertexIndex].faces[vertices[vertexIndex].faceCount] = i;
			vertices[vertexIndex].faceCount++;
		}
		idx += fs;
	}
}


void OrthogonalizeTangent (TangentInfo& tangentInfo, Vector3f normalf, Vector4f& outputTangent)
{
	TangentInfo::Vector3d normal   = { normalf.x, normalf.y, normalf.z };
	TangentInfo::Vector3d tangent  = tangentInfo.tangent;
	TangentInfo::Vector3d binormal = tangentInfo.binormal;

	// Try Gram-Schmidt orthonormalize.
	// This might fail in degenerate cases which we all handle seperately.

	double NdotT = TangentInfo::Vector3d::Dot (normal, tangent);
	TangentInfo::Vector3d newTangent =
	{
		tangent.x - NdotT * normal.x, tangent.y - NdotT * normal.y, tangent.z - NdotT * normal.z
	};

	double magT  = TangentInfo::Vector3d::Magnitude (newTangent);
	newTangent   = TangentInfo::Vector3d::Normalize (newTangent, magT);

	double NdotB = TangentInfo::Vector3d::Dot (normal, binormal);
	double TdotB = TangentInfo::Vector3d::Dot (newTangent, binormal) * magT;

	TangentInfo::Vector3d newBinormal =
	{
		binormal.x - NdotB * normal.x - TdotB * newTangent.x,
		binormal.y - NdotB * normal.y - TdotB * newTangent.y,
		binormal.z - NdotB * normal.z - TdotB * newTangent.z
	};

	double magB  = TangentInfo::Vector3d::Magnitude (newBinormal);
	newBinormal  = TangentInfo::Vector3d::Normalize (newBinormal, magB);


	Vector3f tangentf  = Vector3f( (float)newTangent.x,  (float)newTangent.y,  (float)newTangent.z );
	Vector3f binormalf = Vector3f( (float)newBinormal.x, (float)newBinormal.y, (float)newBinormal.z );


	const double kNormalizeEpsilon = 1e-6;

	if (magT <= kNormalizeEpsilon || magB <= kNormalizeEpsilon)
	{
		// Create tangent basis from scratch - we can safely use Vector3f here - no computations ;-)

		Vector3f axis1, axis2;

		float dpXN = Abs( Dot( Vector3f::xAxis, normalf ) );
		float dpYN = Abs( Dot( Vector3f::yAxis, normalf ) );
		float dpZN = Abs( Dot( Vector3f::zAxis, normalf ) );

		if ( dpXN <= dpYN && dpXN <= dpZN )
		{
			axis1 = Vector3f::xAxis;
			if( dpYN <= dpZN )	axis2 = Vector3f::yAxis;
			else				axis2 = Vector3f::zAxis;


		}
		else if ( dpYN <= dpXN && dpYN <= dpZN )
		{
			axis1 = Vector3f::yAxis;
			if( dpXN <= dpZN )	axis2 = Vector3f::xAxis;
			else				axis2 = Vector3f::zAxis;
		}
		else
		{
			axis1 = Vector3f::zAxis;
			if( dpXN <= dpYN )	axis2 = Vector3f::xAxis;
			else				axis2 = Vector3f::yAxis;
		}



		tangentf  = axis1 - Dot (normalf, axis1) * normalf;
		binormalf = axis2 - Dot (normalf, axis2) * normalf - Dot(tangentf,axis2)*NormalizeSafe(tangentf);

		tangentf  = NormalizeSafe(tangentf);
		binormalf = NormalizeSafe(binormalf);
	}

	outputTangent = Vector4f( tangentf.x, tangentf.y, tangentf.z, 0.0F);

	float dp = Dot (Cross (normalf, tangentf), binormalf);
	if ( dp > 0.0F)
		outputTangent.w = 1.0F;
	else
		outputTangent.w = -1.0F;
}

void CreateTangentSpaceTangentsUnsplit (const ImportMesh& mesh, const Vector3f* vertices, const Vector3f* normals, Vector4f* outTangents, const float normalSmoothingAngle)
{
	float smoothingAngle = max(89.0F, normalSmoothingAngle);

	const int vertexCount = mesh.vertices.size();
	const int faceCount = mesh.polygonSizes.size();
	const int indexCount = mesh.polygons.size();
	const Vector2f* tex = &mesh.uvs[0][0];
	const UInt32* indices = &mesh.polygons[0];

	// Calculate the tangent and binormal vectors of each face,
	// also compute face start offsets in the index buffer
	vector<TangentInfo> faceTangents(indexCount);
	vector<UInt32> faceOffsets(faceCount);
	for (int i = 0, idx = 0; i < faceCount; ++i)
	{
		const int fs = mesh.polygonSizes[i];
		ComputeTriangleTangentBasis (vertices, tex + idx, indices + idx, &faceTangents[idx]);
		///@TODO: is this good enough?
		if (fs == 4)
			ComputeTriangleTangentBasis (vertices, tex + idx+1, indices + idx+1, &faceTangents[idx+1]);
		faceOffsets[i] = idx;
		idx += fs;
	}

	// Average the tangents/binormals but only if they are within the smoothing angle
	vector<TangentInfo> avgTangents(indexCount);
	ConnectedMesh meshConnection (vertexCount, indices, mesh.polygons.size(), &mesh.polygonSizes[0], faceCount);
	float hardDot = cos (Deg2Rad (smoothingAngle)) - 0.001F; // NOTE: subtract is for more consistent results across platforms

	// Go through all faces, average with the connected faces
	for (int fi = 0, idx = 0; fi < faceCount; ++fi)
	{
		const int fs = mesh.polygonSizes[fi];
		for (int e = 0; e < fs; ++e)
		{
			TangentInfo::Vector3d faceTangent  = TangentInfo::Vector3d::Normalize (faceTangents[idx+e].tangent);
			TangentInfo::Vector3d faceBinormal = TangentInfo::Vector3d::Normalize (faceTangents[idx+e].binormal);

			TangentInfo::Vector3d averagedTangent  = {0,0,0};
			TangentInfo::Vector3d averagedBinormal = {0,0,0};

			ConnectedMesh::Vertex connected = meshConnection.vertices[indices[idx + e]];
			for (int i=0;i<connected.faceCount;i++)
			{
				int faceI = -1;
				const UInt32 connectedFaceOffset = faceOffsets[connected.faces[i]];
				const int connectedFaceSize = mesh.polygonSizes[connected.faces[i]];
				for (int k = 0; k < connectedFaceSize; ++k)
				{
					if (indices[connectedFaceOffset+k] == indices[idx+e])
					{
						faceI = connectedFaceOffset+k;
						break;
					}
				}
				Assert(faceI != -1);

				TangentInfo::Vector3d connectedTangent  = faceTangents[faceI].tangent;
				TangentInfo::Vector3d connectedBinormal = faceTangents[faceI].binormal;

				if ( TangentInfo::Vector3d::Dot(TangentInfo::Vector3d::Normalize(connectedTangent), faceTangent) > (double)hardDot )
				{
					averagedTangent.x += connectedTangent.x;
					averagedTangent.y += connectedTangent.y;
					averagedTangent.z += connectedTangent.z;
				}

				if ( TangentInfo::Vector3d::Dot(TangentInfo::Vector3d::Normalize(connectedBinormal), faceBinormal) > (double)hardDot )
				{
					averagedBinormal.x += connectedBinormal.x;
					averagedBinormal.y += connectedBinormal.y;
					averagedBinormal.z += connectedBinormal.z;
				}
			}

			avgTangents[idx+e].tangent  = TangentInfo::Vector3d::Normalize (averagedTangent);
			avgTangents[idx+e].binormal = TangentInfo::Vector3d::Normalize (averagedBinormal);
		}
		idx += fs;
	}

	// Orthogonalize the tangents/binormals and output them into the tangents array
	for (int i=0;i<avgTangents.size();i++)
	{
		OrthogonalizeTangent (avgTangents[i], normals[i], outTangents[i]);
	}
}

void CreateTangentSpaceTangentsUnsplit (const ImportMesh& mesh, Vector4f* outTangents, const float normalSmoothingAngle)
{
	const Vector3f* vertices = &mesh.vertices[0];
	const Vector3f* normals = &mesh.normals[0];

	CreateTangentSpaceTangentsUnsplit(mesh, vertices, normals, outTangents, normalSmoothingAngle);
}

void RemoveUnusedVertices (ImportMesh& mesh)
{
	dynamic_bitset usedVertices;
	usedVertices.resize (mesh.vertices.size ());
	for (int i=0;i<mesh.polygons.size ();i++)
		usedVertices[mesh.polygons[i]] = true;

	if (usedVertices.count () == mesh.vertices.size ())
		return;

	vector<int> remap;
	remap.resize(mesh.vertices.size ());

	int allocated = 0;
	for (int i=0;i<mesh.vertices.size ();i++)
	{
		remap[i] = allocated;
		mesh.vertices[allocated] = mesh.vertices[i];
		for (int j = 0; j < mesh.shapes.size(); ++j)
			mesh.shapes[j].vertices[allocated] = mesh.shapes[j].vertices[i];
		if (!mesh.skin.empty ())
			mesh.skin[allocated] = mesh.skin[i];

		allocated += usedVertices[i];
	}

	for (int i=0;i<mesh.polygons.size();i++)
		mesh.polygons[i] = remap[mesh.polygons[i]];
}

void GenerateFaceNormals (const ImportMesh& mesh, const std::vector<Vector3f>& vertices, Vector3f* outFaceNormals);

/*
  - Build Face normals
  - Build connected mesh (all faces connected to one vertex)
  - Add all face normals connected to the vertex together if the angle between the face normal is less than hard angle
  - Normalize the vertex normal
*/
void GenerateNormals (const ImportMesh& mesh, const std::vector<Vector3f>& vertices, std::vector<Vector3f>& normals, const float hardAngle)
{
	ConnectedMesh meshConnection (mesh.vertices.size (), &mesh.polygons[0], mesh.polygons.size(), &mesh.polygonSizes[0], mesh.polygonSizes.size());

	float hardDot = cos (Deg2Rad (hardAngle)) - 0.001F; // NOTE: subtract is for more consistent results across platforms

	const int indexCount = mesh.polygons.size();
	const int faceCount = mesh.polygonSizes.size();

	normals.resize (indexCount);

	vector<Vector3f> faceNormals;
	faceNormals.resize (faceCount);
	GenerateFaceNormals (mesh, vertices, &faceNormals[0]);

	for (int fi = 0, idx = 0; fi < faceCount; ++fi)
	{
		const int fs = mesh.polygonSizes[fi];
		const UInt32* face = &mesh.polygons[idx];
		Vector3f faceNormal = faceNormals[fi];
		for (int e = 0; e < fs; ++e)
		{
			Vector3f calculateNormal = Vector3f::zero;

			ConnectedMesh::Vertex connected = meshConnection.vertices[face[e]];
			for (int i=0;i<connected.faceCount;i++)
			{
				Vector3f connectedFaceNormal = faceNormals[connected.faces[i]];
				float dp = Dot (faceNormal, connectedFaceNormal);
				if (dp > hardDot)
				{
					calculateNormal += connectedFaceNormal;
				}
			}

			normals[idx+e] = NormalizeRobust (calculateNormal);
		}
		idx += fs;
	}
}




static inline Vector3f RobustNormalFromFace (const Vector3f* vertices, const UInt32* face, int faceSize)
{
	DebugAssert (faceSize == 3 || faceSize == 4);

	// For a triangle, cross of two edges. For a quad, cross of two diagonals.
	// So one edge is v1-(quad?v3:v0), another is v2-v0
	Vector3f ea = vertices[face[1]] - vertices[face[faceSize==4?3:0]];
	Vector3f eb = vertices[face[2]] - vertices[face[0]];
	Vector3f n = Cross (ea, eb);
	return NormalizeRobust (n);
}

void GenerateFaceNormals (const ImportMesh& mesh, const std::vector<Vector3f>& vertices, Vector3f* outFaceNormals)
{
	const int faceCount = mesh.polygonSizes.size();
	for (int i = 0, idx = 0; i < faceCount; ++i)
	{
		int fs = mesh.polygonSizes[i];
		const UInt32* face = &mesh.polygons[idx];
		Vector3f normal = RobustNormalFromFace (&vertices[0], face, fs);
		outFaceNormals[i] = normal;
		idx += fs;
	}
}

void TransformMesh (ImportMesh& mesh, const Matrix4x4f& transform)
{
	// Transform vertices
	Matrix4x4f m44 = transform;
	for (int i=0;i<mesh.vertices.size ();i++)
		mesh.vertices[i] = transform.MultiplyPoint3 (mesh.vertices[i]);

	// Transform normals
	Matrix3x3f invTranspose = Matrix3x3f(transform);
	invTranspose.InvertTranspose ();
	for (int i=0;i<mesh.normals.size ();i++)
		mesh.normals[i] = NormalizeRobust (invTranspose.MultiplyVector3 (mesh.normals[i]));

	for (int i=0;i<mesh.tangents.size ();i++)
	{
		// we transform and normalize tangent and keep sign of binormal
		const Vector4f& t4 = mesh.tangents[i];

		Vector3f t3(t4.x, t4.y, t4.z);
		t3 = NormalizeRobust(invTranspose.MultiplyVector3 (t3));

		mesh.tangents[i].Set(t3.x, t3.y, t3.z, t4.w);
	}

	for (int i = 0; i < mesh.shapes.size(); ++i)
	{
		ImportBlendShape& shape = mesh.shapes[i];
		for (size_t j = 0; j < shape.vertices.size(); ++j)
			shape.vertices[j] = transform.MultiplyVector3(shape.vertices[j]);

		for (size_t j = 0; j < shape.normals.size(); ++j)
			shape.normals[j] = NormalizeRobust(invTranspose.MultiplyVector3(shape.normals[j]));

		for (size_t j = 0; j < shape.tangents.size(); ++j)
			shape.tangents[j] = NormalizeRobust(invTranspose.MultiplyVector3(shape.tangents[j]));
	}
}


template<typename T>
static void InvertFaceWindings (typename std::vector<T>& x, const std::vector<UInt32>& polygonSizes)
{
	if (x.empty())
		return;
	const size_t faceCount = polygonSizes.size();
	for (size_t i = 0, idx = 0; i < faceCount; ++i)
	{
		int ps = polygonSizes[i];
		std::swap (x[idx+1], x[idx+ps-1]);
		idx += ps;
	}
}

void InvertWinding(ImportMesh& mesh)
{
	InvertFaceWindings(mesh.polygons, mesh.polygonSizes);
	InvertFaceWindings(mesh.normals, mesh.polygonSizes);
	InvertFaceWindings(mesh.tangents, mesh.polygonSizes);
	InvertFaceWindings(mesh.colors, mesh.polygonSizes);
	InvertFaceWindings(mesh.uvs[0], mesh.polygonSizes);
	InvertFaceWindings(mesh.uvs[1], mesh.polygonSizes);
	for (size_t i = 0; i < mesh.shapes.size(); ++i)
	{
		ImportBlendShape& shape = mesh.shapes[i];
		InvertFaceWindings(shape.normals, mesh.polygonSizes);
		InvertFaceWindings(shape.tangents, mesh.polygonSizes);
	}
}

template<class T, class InItT>
void Deindex (std::vector<T>& output, InItT const& in, size_t indexCount, UInt32 const* indices)
{
	output.resize(indexCount);
	for( unsigned i = 0; i<indexCount; ++i)
		output[i] = in[ indices[i] ];
}

void InsertVertexAttr(Mesh& src, const Vector2f* uv, const Vector2f* uv2, const ColorRGBAf* color, const unsigned* triMaterial)
{
	// WARNING: copy-pasted - unify

	ImportMesh mesh;

	Mesh::TemporaryIndexContainer tempIndex;

	const int triCount = src.CalculateTriangleCount();

	mesh.materials.resize(triCount);
	if( triMaterial )
	{
		src.GetTriangles(tempIndex);
		::memcpy(&mesh.materials[0], triMaterial, triCount*sizeof(unsigned));
	}
	else
	{
		for( unsigned matI = 0 ; matI < src.GetSubMeshCount() ; ++matI )
		{
			UInt32 startMat = tempIndex.size() / 3;
			src.AppendTriangles(tempIndex, matI);
			UInt32 endMat   = tempIndex.size() / 3;

			for( unsigned i = startMat ; i < endMat ; ++i )
				mesh.materials[i]=matI;
		}
	}

	mesh.polygons.resize(3*triCount);
	::memcpy(&mesh.polygons[0], &tempIndex[0], 3*triCount*sizeof(UInt32));

	mesh.vertices.resize(src.GetVertexCount());
	std::copy (src.GetVertexBegin(), src.GetVertexEnd(), mesh.vertices.begin ());

	size_t indexCount = 3 * triCount;
	UInt32 const* indexPointer = &*mesh.polygons.begin ();

	if( src.IsAvailable(kShaderChannelTexCoord0) || uv )
	{
		mesh.uvs[0].resize(3*triCount);

		if( uv )
			::memcpy(&mesh.uvs[0][0], uv, indexCount*sizeof(Vector2f));
		else
			Deindex (mesh.uvs[0], src.GetUvBegin (0), indexCount, indexPointer);
	}

	if( src.IsAvailable(kShaderChannelTexCoord1) || uv2 )
	{
		mesh.uvs[1].resize(indexCount);

		if( uv2 )
			::memcpy(&mesh.uvs[1][0], uv2, indexCount*sizeof(Vector2f));
		else
			Deindex (mesh.uvs[1], src.GetUvBegin (1), indexCount, indexPointer);
	}

	if( src.IsAvailable(kShaderChannelNormal) )
		Deindex (mesh.normals, src.GetNormalBegin(), indexCount, indexPointer);

	if( src.IsAvailable(kShaderChannelTangent) )
		Deindex (mesh.tangents, src.GetTangentBegin(), indexCount, indexPointer);

	if (src.IsAvailable(kShaderChannelColor))
		Deindex (mesh.colors, src.GetColorBegin(), indexCount, indexPointer);
	else if (color)
		Deindex (mesh.colors, color, indexCount, indexPointer);

	if( src.GetBoneWeights() )
	{
		mesh.skin.resize_initialized(src.GetVertexCount());
		for( unsigned i = 0 ; i < src.GetVertexCount() ; ++i )
			mesh.skin[i] = src.GetBoneWeights()[ mesh.polygons[i] ];
	}

	ImportMesh splitMesh;
	SplitMesh(mesh, splitMesh);

	// copy-paste
	src.ResizeVertices (splitMesh.vertices.size (), splitMesh.AdviseVertexFormat ());
	std::copy (splitMesh.vertices.begin (), splitMesh.vertices.end (), src.GetVertexBegin ());
	std::copy (splitMesh.uvs[0].begin (), splitMesh.uvs[0].end (), src.GetUvBegin (0));
	std::copy (splitMesh.uvs[1].begin (), splitMesh.uvs[1].end (), src.GetUvBegin (1));
	std::copy (splitMesh.colors.begin (), splitMesh.colors.end (), src.GetColorBegin ());
	std::copy (splitMesh.tangents.begin (), splitMesh.tangents.end (), src.GetTangentBegin ());
	std::copy (splitMesh.normals.begin (), splitMesh.normals.end (), src.GetNormalBegin ());
	src.GetSkin().assign( splitMesh.skin.begin(), splitMesh.skin.end() );

	// TODO : add blend shape support here....

	std::vector<int> lodMeshMaterials;

	if (!splitMesh.materials.empty ())
	{
		AssertIf (splitMesh.materials.size () != splitMesh.polygons.size ()/3);

		typedef vector_map<int, int> MaterialLookup;
		MaterialLookup materialLookup;

		// How many different materials do we have?
		for (int i=0;i<splitMesh.materials.size ();i++)
		{
			const int materialIndex = splitMesh.materials[i];
			if (materialLookup.insert (make_pair (materialIndex, materialLookup.size ())).second)
				lodMeshMaterials.push_back(materialIndex);
		}

		int materialCount = materialLookup.size ();

		// Allocate split faces!
		vector<vector<UInt32> > splitFaces;
		splitFaces.resize (materialCount);
		for (int i=0;i<splitFaces.size ();i++)
			splitFaces[i].reserve (splitMesh.polygons.size ());

		// Put faces into the seperate per material face array
		for (int i=0;i<splitMesh.materials.size ();i++)
		{
			vector<UInt32>& buffer = splitFaces[materialLookup.find (splitMesh.materials[i])->second];
			buffer.push_back (splitMesh.polygons[i*3+0]);
			buffer.push_back (splitMesh.polygons[i*3+1]);
			buffer.push_back (splitMesh.polygons[i*3+2]);
		}

		// Assign seperate faces to lodmesh
		src.SetSubMeshCount(materialCount);
		for (int i=0;i<materialCount;i++)
			src.SetIndices(&splitFaces[i][0], splitFaces[i].size(), i, kPrimitiveTriangles);
	}
	else
	{
		src.SetSubMeshCount(1);
		src.SetIndices(&splitMesh.polygons[0], splitMesh.polygons.size(), 0, kPrimitiveTriangles);
	}

	src.SetChannelsDirty(src.GetAvailableChannels(),true);
}




// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (ImportMeshUtilityTests)
{
	TEST(InvertWinding)
	{
		const UInt32 polySizes[] = { 3, 4, 3 };
		const UInt32 srcPolygons[] = { 0,1,2, 3,4,5,6, 7,8,9 };
		const UInt32 expPolygons[] = { 0,2,1, 3,6,5,4, 7,9,8 };
		ImportMesh mesh;
		mesh.polygonSizes.assign (&polySizes[0], polySizes + ARRAY_SIZE(polySizes));
		mesh.polygons.assign (&srcPolygons[0], srcPolygons + ARRAY_SIZE(srcPolygons));
		InvertWinding (mesh);
		CHECK_ARRAY_EQUAL (expPolygons, &mesh.polygons[0], ARRAY_SIZE(expPolygons));
}

	TEST(RemoveDegenerateFaces)
{
		Vector3f u(1,2,3); Vector3f v(4,5,6); Vector3f w(7,8,9);
		const UInt32 srcPolySizes[] = { 3, 3, 4, 4 };
		const UInt32 expPolySizes[] = { 3,    4 };
		const UInt32 srcPolygons[] = { 0,1,2, 0,0,0, 3,4,5,6, 3,3,4,4 };
		const UInt32 expPolygons[] = { 0,1,2,        3,4,5,6 };
		const Vector3f srcNormals[] ={ u,v,w, w,v,u, w,v,u,w, u,v,w,u };
		const Vector3f expNormals[] ={ u,v,w,        w,v,u,w };
		ImportMesh m;
		m.polygonSizes.assign (&srcPolySizes[0], srcPolySizes + ARRAY_SIZE(srcPolySizes));
		m.polygons.assign (&srcPolygons[0], srcPolygons + ARRAY_SIZE(srcPolygons));
		m.normals.assign (&srcNormals[0], srcNormals + ARRAY_SIZE(srcNormals));
		RemoveDegenerateFaces (m);
		CHECK_EQUAL (ARRAY_SIZE(expPolySizes), m.polygonSizes.size());
		CHECK_ARRAY_EQUAL (expPolySizes, &m.polygonSizes[0], ARRAY_SIZE(expPolySizes));
		CHECK_EQUAL (ARRAY_SIZE(expPolygons), m.polygons.size());
		CHECK_ARRAY_EQUAL (expPolygons, &m.polygons[0], ARRAY_SIZE(expPolygons));
		CHECK_EQUAL (ARRAY_SIZE(expNormals), m.normals.size());
		CHECK_ARRAY_EQUAL (&expNormals[0].x, &m.normals[0].x, ARRAY_SIZE(expNormals)*3);
		CHECK_EQUAL (true, m.hasAnyQuads);
}

	TEST(IsDegenerateQuad)
{
		const UInt32 qa[] = {0,1,2,3}; CHECK_EQUAL(kDegenNone, IsDegenerateFace(qa,4));

		const UInt32 qb[] = {1,1,1,1}; CHECK_EQUAL(kDegenFull, IsDegenerateFace(qb,4));
		const UInt32 qc[] = {1,2,3,2}; CHECK_EQUAL(kDegenFull, IsDegenerateFace(qc,4));
		const UInt32 qd[] = {1,1,2,2}; CHECK_EQUAL(kDegenFull, IsDegenerateFace(qd,4));

		const UInt32 qe[] = {1,1,2,3}; CHECK_EQUAL(kDegenToTri, IsDegenerateFace(qe,4));
		const UInt32 qf[] = {1,2,2,3}; CHECK_EQUAL(kDegenToTri, IsDegenerateFace(qf,4));
		const UInt32 qg[] = {1,2,3,3}; CHECK_EQUAL(kDegenToTri, IsDegenerateFace(qg,4));
		const UInt32 qh[] = {1,2,3,1}; CHECK_EQUAL(kDegenToTri, IsDegenerateFace(qh,4));
}

	TEST(RemoveDegenerateFacesQuadsDegeneratingToTriangles)
{
		Vector3f u(1,2,3); Vector3f v(4,5,6); Vector3f w(7,8,9);
		const UInt32 srcPolySizes[] = { 4, 4, 4 };
		const UInt32 expPolySizes[] = { 3, 3, 3 };
		const UInt32 srcPolygons[] = { 1,2,3,3,  5,5,6,7,  8,9,9,10 };
		const UInt32 expPolygons[] = { 1,2,  3,    5,6,7,  8,  9,10 };
		const Vector3f srcNormals[] ={ u,v,w,u,  u,v,w,u,  u,v,w,u };
		const Vector3f expNormals[] ={ u,v,  u,    v,w,u,  u,  w,u };
		ImportMesh m;
		m.polygonSizes.assign (&srcPolySizes[0], srcPolySizes + ARRAY_SIZE(srcPolySizes));
		m.polygons.assign (&srcPolygons[0], srcPolygons + ARRAY_SIZE(srcPolygons));
		m.normals.assign (&srcNormals[0], srcNormals + ARRAY_SIZE(srcNormals));
		RemoveDegenerateFaces (m);
		CHECK_EQUAL (ARRAY_SIZE(expPolySizes), m.polygonSizes.size());
		CHECK_ARRAY_EQUAL (expPolySizes, &m.polygonSizes[0], ARRAY_SIZE(expPolySizes));
		CHECK_EQUAL (ARRAY_SIZE(expPolygons), m.polygons.size());
		CHECK_ARRAY_EQUAL (expPolygons, &m.polygons[0], ARRAY_SIZE(expPolygons));
		CHECK_EQUAL (ARRAY_SIZE(expNormals), m.normals.size());
		CHECK_ARRAY_EQUAL (&expNormals[0].x, &m.normals[0].x, ARRAY_SIZE(expNormals)*3);
		CHECK_EQUAL (false, m.hasAnyQuads);
	}

	TEST(RobustNormalFromFaceTri)
	{
		const Vector3f v[] = {Vector3f(1,1,1), Vector3f(2,1,1), Vector3f(1,2,1)}; // counter clockwise tri on Z=1 plane
		const UInt32 f[] = {0,1,2};
		Vector3f n = RobustNormalFromFace(v, f, 3);
		CHECK (CompareApproximately(n, Vector3f(0,0,1))); // expect +Z normal
	}
	TEST(RobustNormalFromFaceQuad)
	{
		const Vector3f v[] = {Vector3f(1,1,1), Vector3f(2,1,1), Vector3f(2,2,1), Vector3f(1,2,1)}; // counter clockwise quad on Z=1 plane
		const UInt32 f[] = {0,1,2,3};
		Vector3f n = RobustNormalFromFace(v, f, 4);
		CHECK (CompareApproximately(n, Vector3f(0,0,1))); // expect +Z normal
	}

} // SUITE

#endif // ENABLE_UNIT_TESTS
