#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "MeshBlendShape.h"
#include "Runtime/mecanim/generic/crc32.h"

static const float kVertexDeltaEpsilon = 1e-5f;
static const float kNormalDeltaEpsilon = 1e-5f;

void SetBlendShapeVertices(const std::vector<Vector3f>& deltaVertices, const std::vector<Vector3f>& deltaNormals, const std::vector<Vector3f>& deltaTangents, BlendShapeVertices& sharedSparceVertices, BlendShape& frame)
{
	Assert(deltaNormals.empty() || deltaVertices.size() == deltaNormals.size());
	Assert(deltaTangents.empty() || deltaVertices.size() == deltaTangents.size());

	frame.firstVertex = sharedSparceVertices.size();

	// Converting blend shape in to sparse blend shape
	sharedSparceVertices.reserve(sharedSparceVertices.size() + deltaVertices.size());

	frame.hasNormals = frame.hasTangents = false;

	for (int j = 0; j < deltaVertices.size(); ++j)
	{
		const bool vertexHasNormal = (!deltaNormals.empty() && Magnitude(deltaNormals[j]) > kNormalDeltaEpsilon);
		const bool vertexHasTangent = (!deltaTangents.empty() && Magnitude(deltaTangents[j]) > kNormalDeltaEpsilon);

		frame.hasNormals = frame.hasNormals || vertexHasNormal;
		frame.hasTangents = frame.hasTangents || vertexHasTangent;

		if (Magnitude(deltaVertices[j]) > kVertexDeltaEpsilon || vertexHasNormal || vertexHasTangent)
		{
			BlendShapeVertex v;			

			v.vertex = deltaVertices[j];
			if (!deltaNormals.empty())
				v.normal = deltaNormals[j];
			if (!deltaTangents.empty())
				v.tangent = deltaTangents[j];

			v.index = j;
			sharedSparceVertices.push_back(v);
		}				
	}

	frame.vertexCount = sharedSparceVertices.size() - frame.firstVertex;
}

void BlendShape::UpdateFlags(const BlendShapeVertices& sharedSparceVertices)
{
	hasNormals = hasTangents = false;

	for (int j = 0; j < vertexCount; ++j)
	{
		const BlendShapeVertex& v = sharedSparceVertices[firstVertex + j];
		const bool vertexHasNormal = Magnitude(v.normal) > kNormalDeltaEpsilon;
		const bool vertexHasTangent = Magnitude(v.tangent) > kNormalDeltaEpsilon;

		hasNormals = hasNormals || vertexHasNormal;
		hasTangents = hasTangents || vertexHasTangent;
	}
}

void InitializeChannel (const UnityStr& inName, int frameIndex, int frameCount, BlendShapeChannel& channel)
{
	channel.name.assign(inName.c_str(), kMemGeometry);
	channel.nameHash = mecanim::processCRC32(inName.c_str());
	channel.frameIndex = frameIndex;
	channel.frameCount = frameCount;
}

const char* GetChannelName (const BlendShapeData& data, int index)
{
	return data.channels[index].name.c_str();
}

int GetChannelIndex (const BlendShapeData& data, const char* name)
{
	for (int i=0;i<data.channels.size();i++)
	{
		if (name == data.channels[i].name)
			return i;
	}
	return -1;
}

int GetChannelIndex (const BlendShapeData& data, BindingHash name)
{
	for (int i=0;i<data.channels.size();i++)
	{
		if (name == data.channels[i].nameHash)
			return i;
	}
	return -1;
}

void ClearBlendShapes (BlendShapeData& data)
{
	data.vertices.clear();
	data.shapes.clear();
	data.channels.clear();
	data.fullWeights.clear();
}

/*

STRUCT BlendShapeChannel

// BlendShape vertex class.
STRUCT Vertex
// Vertex delta.
CSRAW public Vector3 vertex;

// Normal delta.
CSRAW public Vector3 normal;

// Tangent delta.
CSRAW public Vector3 tangent;

// Index to [[Mesh]] vertex data.
CSRAW public int index;
END

// A class representing a single BlendShape (also called morph-target).
STRUCT BlendShape

// The weight of the frame 
CSRAW public float    weight;

// Sparse vertex data.
CSRAW public Vertex[] vertices;
END

// Name of the BlendShape.
CSRAW public string           name;

// The frames making up a blendshape animation.
// Each frame has a weight, based on the weight of the BlendShape in the SkinnedMeshRenderer, Unity will apply 1 or 2 frames.
CSRAW public BlendShape[] shapes;
END


C++RAW
/*
 struct MonoMeshBlendShape
 {
 ScriptingStringPtr name;
 ScriptingArrayPtr vertices;
 };
 
 void BlendShapeVertexToMono (const BlendShapeVertex &src, MonoBlendShapeVertex &dest) {
 dest.vertex = src.vertex;
 dest.normal = src.normal;
 dest.tangent = src.tangent;
 dest.index = src.index;
 }
 void BlendShapeVertexToCpp (const MonoBlendShapeVertex &src, BlendShapeVertex &dest) {
 dest.vertex = src.vertex;
 dest.normal = src.normal;
 dest.tangent = src.tangent;
 dest.index = src.index;
 }
 
 class MeshBlendShapeToMono
 {
 public:
 MeshBlendShapeToMono(const BlendShapeVertices& sharedVertices_) : sharedVertices(sharedVertices_) {}
 
 void operator() (const MeshBlendShape &src, MonoMeshBlendShape &dest)
 {
 dest.name = scripting_string_new(src.m_Name);
 const BlendShapeVertices vertices(sharedVertices.begin() + src.firstVertex, sharedVertices.begin() + src.firstVertex + src.vertexCount);
 
 ScriptingTypePtr classVertex = GetScriptingTypeRegistry().GetType("UnityEngine", "BlendShapeVertex");
 dest.vertices = VectorToScriptingStructArray<BlendShapeVertex, MonoBlendShapeVertex>(vertices, classVertex, BlendShapeVertexToMono);
 }
 
 private:
 const BlendShapeVertices& sharedVertices;
 };
 
 class MeshBlendShapeToCpp
 {
 public:
 MeshBlendShapeToCpp(int meshVertexCount_, BlendShapeVertices& sharedVertices_) : meshVertexCount(meshVertexCount_), sharedVertices(sharedVertices_) {}
 
 void operator() (MonoMeshBlendShape &src, MeshBlendShape &dest)
 {
 dest.weight = src.weight;
 
 const BlendShapeVertex* vertices = Scripting::GetScriptingArrayStart<BlendShapeVertex> (src.vertices);
 sharedVertices.insert(sharedVertices.end(), vertices, vertices + GetScriptingArraySize(src.vertices));
 
 for (BlendShapeVertices::iterator it = vertices.begin(), end = vertices.end(); it != end; ++it)
 {
 BlendShapeVertex& v = *it;
 if (v.index < 0 || v.index >= meshVertexCount)
 {
 ErrorStringMsg("Value (%d) of BlendShapeVertex.index #%d is out of bounds (Mesh vertex count: %d) on BlendShape '%s'. It will be reset to 0.", v.index, it - vertices.begin(), meshVertexCount, dest.m_Name.c_str());
 v.index = 0;
 }
 }
 
 dest.firstVertex = sharedVertices.size();
 dest.vertexCount = vertices.size();
 
 sharedVertices.insert(sharedVertices.end(), vertices.begin(), vertices.end());
 dest.UpdateFlags(sharedVertices);
 }
 
 private:
 int meshVertexCount;
 BlendShapeVertices& sharedVertices;
 };
 
 
 
 ----------------
 
 // BlendShapes for this mesh.
 CUSTOM_PROP BlendShapeChannel[] blendShapes
 {
 //		ScriptingTypePtr classBlendShape = GetScriptingTypeRegistry().GetType("UnityEngine", "MeshBlendShape");
 //		return VectorToScriptingStructArray<MeshBlendShape, MonoMeshBlendShape>(self->GetShapesVector(), classBlendShape, MeshBlendShapeToMono(self->GetShapeVertexVector()));
 return SCRIPTING_NULL;
 }
 {
 //		Mesh::MeshBlendShapeContainer shapes;
 //		self->GetShapeVertexVector().clear();
 //		ScriptingStructArrayToVector<MeshBlendShape, MonoMeshBlendShape>(value, shapes, MeshBlendShapeToCpp(self->GetVertexCount(), self->GetShapeVertexVector()));
 //		self->SwapShapesVector(shapes);
 }
 
 
 
 */
