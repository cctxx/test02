#include "UnityPrefix.h"
#include "LodMeshFilter.h"
#include "LodMesh.h"
#include "MeshRenderer.h"
#include "Runtime/Filters/Particles/MeshParticleEmitter.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

MeshFilter::MeshFilter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Mesh = NULL;
}

MeshFilter::~MeshFilter ()
{
}

void MeshFilter::OnDidAddMesh ()
{
	AssignMeshToRenderer ();
}

void MeshFilter::AssignMeshToRenderer ()
{
	if (GetGameObjectPtr())
	{
		MeshRenderer* renderer = QueryComponent(MeshRenderer);
		if (renderer && renderer->GetSharedMesh() != m_Mesh)
			renderer->SetSharedMesh(m_Mesh);	

		MeshParticleEmitter* emitter = QueryComponent(MeshParticleEmitter);
		if (emitter && emitter->GetMesh() != m_Mesh)
			emitter->SetMesh(m_Mesh);	
	}
}

void MeshFilter::SetSharedMesh (PPtr<Mesh> mesh)
{
	m_Mesh = mesh;

	MeshRenderer* renderer = QueryComponent(MeshRenderer);
	if (renderer)
		renderer->SetSharedMesh(m_Mesh);	

	MeshParticleEmitter* emitter = QueryComponent(MeshParticleEmitter);
	if (emitter)
		emitter->SetMesh(m_Mesh);	

	SetDirty ();
}

PPtr<Mesh> MeshFilter::GetSharedMesh ()
{
	return m_Mesh;
}

Mesh* MeshFilter::GetInstantiatedMesh ()
{
	Mesh* instantiated = &Mesh::GetInstantiatedMesh (m_Mesh, *this);
	if (PPtr<Mesh> (instantiated) != m_Mesh)
	{
		SetSharedMesh(instantiated);
	}

	return instantiated;
}

void MeshFilter::SetInstantiatedMesh (Mesh* mesh)
{
	SetSharedMesh(mesh);
}

IMPLEMENT_CLASS_HAS_INIT (MeshFilter)
IMPLEMENT_OBJECT_SERIALIZE (MeshFilter)

template<class TransferFunction> inline
void MeshFilter::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_Mesh, "m_Mesh", kSimpleEditorMask);
}

void MeshFilter::InitializeClass ()
{
	RegisterAllowNameConversion(GetClassStringStatic(), "m_LodMesh", "m_Mesh");
	RegisterAllowTypeNameConversion ("PPtr<LodMesh>", "PPtr<Mesh>");
	
	REGISTER_MESSAGE_VOID(MeshFilter, kDidAddComponent, OnDidAddMesh);
}

void MeshFilter::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	AssignMeshToRenderer ();
}
