#ifndef MESH3DSIMPORTER_H
#define MESH3DSIMPORTER_H

#include "ModelImporter.h"

class Transform;
namespace Unity { class GameObject; class Material; }
class Mesh;
class Matrix4x4f;

namespace primport
{
	class Import3DS;
	struct Node3DS;
	struct TriMesh3DS;
	struct Material3DS;
}

class Mesh3DSImporter : public ModelImporter
{
	public:
	
	REGISTER_DERIVED_CLASS (Mesh3DSImporter, ModelImporter)
//	DECLARE_OBJECT_SERIALIZE (ModelImporter)
	
	Mesh3DSImporter (MemLabelId label, ObjectCreationMode mode);
	// ~Mesh3DSImporter (); declared-by-macro
	
	virtual bool DoMeshImport (ImportScene& scene);
	virtual bool IsTangentImportSupported() const { return false; }

	static void InitializeClass ();
	static void CleanupClass () {}
	
	private:
	
	void Import3DSNode (primport::Import3DS& file, const primport::Node3DS& node, ImportNode* parent, ImportScene& scene);
	void Import3DSMeshAsNode (primport::Import3DS& file, int meshIndex, ImportNode* parent, ImportScene& scene);
};

#endif
