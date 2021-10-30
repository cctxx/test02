#include "UnityPrefix.h"
#include "ImportMesh.h"
#include "External/Unwrap/include/UnwrapParam.hpp"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

using namespace std;


ImportNode::ImportNode () : cameraIndex(-1), lightIndex(-1)
{
	position = Vector3f::zero;
	rotation = Quaternionf::identity ();
	scale = Vector3f::one;
	meshIndex = -1;
	meshTransform.SetIdentity ();
	instantiatedGameObject = NULL;
	cimportnode = NULL;
}

ImportTexture::ImportTexture()
:	offset(0, 0)
,	scale(1, 1)
{
}

ImportMaterial::ImportMaterial () :
	diffuse(1.0F, 1.0F, 1.0F, 1.0F),
	ambient(1.0F, 1.0F, 1.0F, 1.0F),
	hasTransparencyTexture(false)
{}

unsigned ImportMesh::AdviseVertexFormat () const
{
	unsigned vertexFormat = 0;
	if (!vertices.empty()) vertexFormat |= VERTEX_FORMAT1(Vertex);
	if (!normals.empty()) vertexFormat |= VERTEX_FORMAT1(Normal);
	if (!colors.empty()) vertexFormat |= VERTEX_FORMAT1(Color);
	if (!uvs[0].empty()) vertexFormat |= VERTEX_FORMAT1(TexCoord0);
	if (!uvs[1].empty()) vertexFormat |= VERTEX_FORMAT1(TexCoord1);
	if (!tangents.empty()) vertexFormat |= VERTEX_FORMAT1(Tangent);
	return vertexFormat;
}

void ImportMesh::Reserve (int vertexCount, int faceCount, const ImportMesh* src)
{
	if (src)
	{
		if (!src->polygons.empty()) polygons.reserve (faceCount*3);
		if (!src->polygonSizes.empty()) polygonSizes.reserve (faceCount);
		if (!src->materials.empty()) materials.reserve (faceCount);

		if (!src->vertices.empty()) vertices.reserve (vertexCount);
		if (!src->skin.empty()) skin.reserve (vertexCount);
		if (!src->normals.empty()) normals.reserve (vertexCount);
		if (!src->tangents.empty()) tangents.reserve (vertexCount);
		if (!src->colors.empty()) colors.reserve (vertexCount);
		if (!src->uvs[0].empty()) uvs[0].reserve (vertexCount);
		if (!src->uvs[1].empty()) uvs[1].reserve (vertexCount);
	}
	else
	{
		polygons.reserve (faceCount*3);
		polygonSizes.reserve (faceCount);
		materials.reserve (faceCount);

		vertices.reserve (vertexCount);
			skin.reserve (vertexCount);
		normals.reserve (vertexCount);
			tangents.reserve (vertexCount);
		colors.reserve (vertexCount);
		uvs[0].reserve (vertexCount);
			uvs[1].reserve (vertexCount);
	}
}

ImportMeshSettings::ImportMeshSettings() :
	optimizeMesh(true),
	weldVertices(true),
	invertWinding(false),
	swapUVChannels(false),
	generateSecondaryUV(false),
	normalImportMode(kTangentSpaceOptionsImport),
	tangentImportMode(kTangentSpaceOptionsCalculate),
	normalSmoothAngle(60.0F),
	splitTangentsAcrossUV(true)
{
	UnwrapParam defaultUnwrapParam;
	defaultUnwrapParam.Reset();

	secondaryUVAngleDistortion	= 100.0f  * defaultUnwrapParam.angleDistortionThreshold;
	secondaryUVAreaDistortion	= 100.0f  * defaultUnwrapParam.areaDistortionThreshold;
	secondaryUVHardAngle		= defaultUnwrapParam.hardAngle;
	secondaryUVPackMargin		= 1024.0f * defaultUnwrapParam.packMargin;
}
