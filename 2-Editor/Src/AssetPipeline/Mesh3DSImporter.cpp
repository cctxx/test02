#include "UnityPrefix.h"
#include "Mesh3DSImporter.h"
#include "External/ProphecySDK/include/primport/3ds.hpp"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"

using namespace primport;
using namespace std;

IMPLEMENT_CLASS_HAS_INIT (Mesh3DSImporter)

static int CanLoadPathName (const string& pathName, int* queue)
{
	*queue = ModelImporter::GetQueueNumber(pathName);
	string extension = GetPathNameExtension (pathName);
	return StrICmp ("3ds", extension) == 0;
}

void Mesh3DSImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (Mesh3DSImporter), kModelImporterVersion);	
}

namespace 
{
	std::string ConvertAsciiToUTF8(const std::string& str, const std::string& stringName, std::vector<std::string>& warnings)
	{
		std::string s = str;
		if (!AsciiToUTF8(s))
		{
			warnings.push_back(Format("Failed to convert ascii string (%s) '%s' to UTF8.\n", stringName.c_str(), str.c_str()));
			return "";
		}
	
		return s;
	}

	ImportMaterial Import3DSMaterial (const primport::Material3DS& material, std::vector<std::string>& warnings)
	{
		ImportMaterial importMaterial;
	
		importMaterial.ambient = ColorRGBA32 (material.ambient.r, material.ambient.g, material.ambient.b, material.ambient.a);
		importMaterial.diffuse = ColorRGBA32 (material.diffuse.r, material.diffuse.g, material.diffuse.b, material.diffuse.a);
		importMaterial.texture.path = ConvertAsciiToUTF8(std::string(material.texturemap1.filename), "texture path", warnings);	
		importMaterial.normalMap.path =  ConvertAsciiToUTF8(std::string(material.bumpmap.filename), "bumpmap path", warnings);	
		importMaterial.name =  ConvertAsciiToUTF8(std::string(material.name), "material name", warnings);	
	
		return importMaterial;
	}
}

static void Import3DSMesh (const TriMesh3DS& mesh, ImportMesh& importMesh, std::vector<std::string>& warnings)
{
	importMesh.Reserve (mesh.vertices.GetSize (), mesh.faces.GetSize (), NULL);
	importMesh.name = mesh.name;

	// Add vertices
	for (int i=0;i<mesh.vertices.GetSize ();i++)
		importMesh.vertices.push_back (Vector3f (mesh.vertices[i]));

	int NaNCount = 0;
	for (int i=0;i<mesh.faces.GetSize ();i++)
	{
		const Face3DS& face = mesh.faces[i];

		// Assign face material index
		importMesh.materials.push_back (face.material);
		
		// Add face
		importMesh.polygons.push_back (face.vertex[0]);
		importMesh.polygons.push_back (face.vertex[1]);
		importMesh.polygons.push_back (face.vertex[2]);
		
		// Add normals
		for (int e=0; e<3; ++e)
			importMesh.normals.push_back (Vector3f (&face.normal[e].x));
			
		// Add UV's (Import mesh wants face uv's, 3DS has per vertex uv's.)
		if (mesh.texcoord.GetSize () == mesh.vertices.GetSize ())
		{			
			for (int e=0; e<3; ++e)
			{
				Vector2f uv(mesh.texcoord[face.vertex[e]]);
				if (IsNAN(uv.x) || IsNAN(uv.y))
				{
					++NaNCount;
					uv = Vector2f::zero;
				}

				importMesh.uvs[0].push_back(uv);
			}
		}
	}

	if (NaNCount > 0)
		warnings.push_back(Format("Mesh '%s' has %d (out of %d) invalid UVs (NaNs). They will be assigned value (0,0)", importMesh.name.c_str(), NaNCount, importMesh.uvs[0].size()));
}

Mesh3DSImporter::Mesh3DSImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	// TODO : implement import of tangents and normals for 3ds?
	m_MeshSettings.normalImportMode = kTangentSpaceOptionsCalculate;
}

Mesh3DSImporter::~Mesh3DSImporter ()
{}

void Mesh3DSImporter::Import3DSNode (primport::Import3DS& file, const primport::Node3DS& node, ImportNode* parent, ImportScene& scene)
{
	ImportNode* importNode = NULL;
	if (parent)
	{
		parent->children.push_back (ImportNode ());
		importNode = &parent->children.back ();
	}
	else
	{
		scene.nodes.push_back (ImportNode ());
		importNode = &scene.nodes.back ();
	}

	importNode->name = string (node.instancename);
	// In 3DS max target nodes have .target attached in the name
	if (node.type == Node3DS::TARGETCAMERA || node.type == Node3DS::TARGETLIGHT)
		importNode->name += ".target";
	
	// Import position, scale, rotation
	if (node.postrack.GetSize () != 0)
		importNode->position = Vector3f (&node.postrack[0].pos.x);
	if (node.rottrack.GetSize () != 0)
		importNode->rotation = Quaternionf (node.rottrack[0].rot.x, node.rottrack[0].rot.y, node.rottrack[0].rot.z, node.rottrack[0].rot.w);
	if (node.scaletrack.GetSize () != 0)
		importNode->scale = Vector3f (&node.scaletrack[0].scale.x);

	int meshIndex = file.FindMeshIndex (node);
	if (meshIndex != -1)
	{	
		TriMesh3DS& mesh = file.trimeshes[meshIndex];
/*		Matrix4x4f referenceMatrix;
		referenceMatrix.SetIdentity ();
		QuaternionToMatrix (rotation, &referenceMatrix);
		referenceMatrix.Get (0, 3) = position.x;
		referenceMatrix.Get (1, 3) = position.y;
		referenceMatrix.Get (2, 3) = position.z;
		referenceMatrix.Invert ();
		PrintRemapping (matrix3ds, referenceMatrix.GetPtr ());
*/		
		Matrix4x4f matrix;
		memcpy (&matrix, &mesh.xform, sizeof (Matrix4x4f));
		float* matrix3ds = (float*)&mesh.xform;
		
		const float negative[] = { 1.000000, 1.000000, -1.000000, -1.000000, 1.000000, 1.000000, -1.000000, -1.000000, -1.000000, -1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000 };
		const int remap[] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 7, 11, 15 };
		for (int i=0;i<16;i++)
			matrix.GetPtr ()[remap[i]] = negative[i] * matrix3ds[i];
		
		Vector3f pivotPoint (&node.pivot.x);
		matrix.Translate (pivotPoint);
		matrix.Invert_Full();
		
		importNode->meshTransform = matrix;
		importNode->meshIndex = meshIndex;
	}
	
	importNode->children.reserve (node.children.GetSize ());
	for (int i=0;i<node.children.GetSize ();i++)
		Import3DSNode (file, *node.children[i], importNode, scene);
}


void Mesh3DSImporter::Import3DSMeshAsNode (primport::Import3DS& file, int meshIndex, ImportNode* parent, ImportScene& scene)
{
	const primport::TriMesh3DS& mesh = file.trimeshes[meshIndex];
	ImportNode* importNode = NULL;
	if (parent)
	{
		parent->children.push_back (ImportNode ());
		importNode = &parent->children.back ();
	}
	else
	{
		scene.nodes.push_back (ImportNode ());
		importNode = &scene.nodes.back ();
	}

	importNode->name = string (mesh.name);

/*	Matrix4x4f referenceMatrix;
	referenceMatrix.SetIdentity ();
	QuaternionToMatrix (rotation, &referenceMatrix);
	referenceMatrix.Get (0, 3) = position.x;
	referenceMatrix.Get (1, 3) = position.y;
	referenceMatrix.Get (2, 3) = position.z;
	referenceMatrix.Invert ();
	PrintRemapping (matrix3ds, referenceMatrix.GetPtr ());
*/		
	Matrix4x4f matrix;
	memcpy (&matrix, &mesh.xform, sizeof (Matrix4x4f));
	float* matrix3ds = (float*)&mesh.xform;
	
	const float negative[] = { 1.000000, 1.000000, -1.000000, -1.000000, 1.000000, 1.000000, -1.000000, -1.000000, -1.000000, -1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000 };
	const int remap[] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 7, 11, 15 };
	for (int i=0;i<16;i++)
		matrix.GetPtr ()[remap[i]] = negative[i] * matrix3ds[i];
	
	matrix.Invert_Full();
	
	importNode->meshTransform = matrix;
	importNode->meshIndex = meshIndex;
}


bool Mesh3DSImporter::DoMeshImport (ImportScene& scene)
{
	// Read file into prophecy stream
	int fileLength = GetFileLength (GetAssetPathName ());
	if (fileLength == -1)
	{
		LogImportError ("3DS file can't be opened!");
		return false;
	}
	prcore::MemoryStream stream (fileLength);
	if (!ReadFromFile (GetAssetPathName (), stream.GetData (), 0, fileLength))
	{
		LogImportError ("3DS file can't be read!");
		return false;
	}

	try
	{
		// Import the file
		primport::Import3DS import (stream);
		for (int i = 0, size = import.warnings.GetSize(); i < size; ++i)
			LogImportWarning(static_cast<char*>(import.warnings[i]));
		
		std::vector<std::string> warnings;

		// Import Materials
		scene.materials.reserve (import.materials.GetSize ());
		for (int i=0;i<import.materials.GetSize ();i++)
			scene.materials.push_back (Import3DSMaterial (import.materials[i], warnings));
		
		// Import Meshes
		scene.meshes.reserve (import.trimeshes.GetSize ());
		for (int i=0;i<import.trimeshes.GetSize ();i++)
		{
			scene.meshes.push_back (ImportMesh ());
			Import3DSMesh (import.trimeshes[i], scene.meshes.back (), warnings);
		}

		for (int i = 0, size = warnings.size(); i < size; ++i)
			LogImportWarning(warnings[i]);
		
		if (import.rootnodes.GetSize ())
		{
			// Import nodes
			scene.nodes.reserve (import.rootnodes.GetSize ());
			for (int i=0;i<import.rootnodes.GetSize ();i++)
				Import3DSNode (import, *import.rootnodes[i], NULL, scene);
		}
		else
		{
			scene.nodes.reserve (import.trimeshes.GetSize ());
			for (int i=0;i<import.trimeshes.GetSize ();i++)
				Import3DSMeshAsNode (import, i, NULL, scene);
		}
	}
	catch (...)
	{
		string error = Format ("Error while parsing the 3DS file %s!\nThe 3DS file contains invalid data. Check if it was exported correctly.", GetAssetPathName ().c_str ());
		LogImportError (error);
		return false;
	}
	return true;
}