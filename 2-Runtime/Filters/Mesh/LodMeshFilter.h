#ifndef LODMESHFILTER_H
#define LODMESHFILTER_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Modules/ExportModules.h"

class Mesh;

class EXPORT_COREMODULE MeshFilter : public Unity::Component
{
public:
	REGISTER_DERIVED_CLASS (MeshFilter, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (MeshFilter)

	MeshFilter (MemLabelId label, ObjectCreationMode mode);
	
	void SetSharedMesh (PPtr<Mesh> mesh);
	PPtr<Mesh> GetSharedMesh ();

	Mesh* GetInstantiatedMesh ();
	void SetInstantiatedMesh (Mesh* mesh);
	
	static void InitializeClass ();
	static void CleanupClass () {}

	void OnDidAddMesh ();
	
protected:
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);	


private:	
	void AssignMeshToRenderer ();
	
	PPtr<Mesh>				m_Mesh;
};

#endif
