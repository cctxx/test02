#ifndef DEFORMABLEMESH_H
#define DEFORMABLEMESH_H

#include "Configuration/UnityConfigure.h"
#if ENABLE_CLOTH || DOXYGEN

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Utilities/dynamic_array.h"

class Mesh;
class ClothRenderer;
class NxMeshData;
class NxScene;

class NxClothMesh;
class NxCloth;
class NxClothDesc;

namespace Unity {

class Cloth : public Behaviour
{
public:	
	REGISTER_DERIVED_ABSTRACT_CLASS (Cloth, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Cloth)

	Cloth (MemLabelId label, ObjectCreationMode mode);
	
	bool GetSuspended() { return m_SuspendCount != 0; }
	void SetSuspended(bool suspended);
	virtual void SetEnabled (bool enab);
	virtual void Reset ();
	
	
	virtual void FixedUpdate ();
	virtual void Deactivate (DeactivateOperation operation);
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void ProcessMeshForRenderer ();

	static void InitializeClass();
	static void CleanupClass ();

#if UNITY_EDITOR
	void TransformChanged( int changeMask );
	void BecameVisible();
	void BecameInvisible();
#endif
	
	float GetBendingStiffness () const { return m_BendingStiffness; }
	void SetBendingStiffness (float value);

	float GetStretchingStiffness () const { return m_StretchingStiffness; }
	void SetStretchingStiffness (float value);

	float GetDamping () const { return m_Damping; }
	void SetDamping (float value);

	bool GetUseGravity () const { return m_UseGravity; }
	void SetUseGravity (bool value);

	Vector3f GetExternalAcceleration () const { return m_ExternalAcceleration; }
	void SetExternalAcceleration (const Vector3f &value);

	Vector3f GetRandomAcceleration () const { return m_RandomAcceleration; }
	void SetRandomAcceleration (const Vector3f &value);

	bool GetSelfCollision () { return m_SelfCollision; }
	void SetSelfCollision (bool value);

	float GetThickness () const { return m_Thickness; }
	void SetThickness (float value);

	dynamic_array<Vector3f> &GetVertices () { return m_Vertices; }
	dynamic_array<Vector3f> &GetNormals () { return m_Normals; }
protected:
	
	virtual void PauseSimulation ();
	virtual void ResumeSimulation ();

	virtual void Create () = 0;
	virtual void Cleanup ();
	bool SetupMeshData (bool transformToWorldSpace, bool externalVertexTranlation, int tearMemoryFactor);
	void SetupMeshBuffers (NxMeshData &meshData);

	void SetupClothDesc (NxClothDesc &clothDesc, bool tearable);
	NxClothMesh *CookClothMesh (bool tearable);
	
	// configuration
	PPtr<Mesh> m_Mesh;	
	float m_BendingStiffness; ///<Bending stiffness. 0 = disabled. range { 0, 1 }
	float m_StretchingStiffness; ///<Stretching stiffness. range { 0.001, 1 }
	float m_Damping; ///<Motion damping coefficient. 0 = disabled. range { 0, 1 }
	float m_Thickness;	///<Thickness of the Cloth surface. range { 0.001, 10000 }
	bool m_UseGravity; ///<Should gravitational acceleration be applied to the cloth?
	bool m_SelfCollision; ///<Can the cloth collide with itself?
	Vector3f m_ExternalAcceleration; ///<External acceleration applied to the cloth.
	Vector3f m_RandomAcceleration; ///<Random acceleration applied to the cloth.


	// state
	NxCloth* m_Cloth;

	NxScene* m_ClothScene;

	int m_SuspendCount;

	bool m_IsSuspended;
	bool m_NeedToWakeUp; 

	UInt32 m_NumVertices;
	UInt32 m_NumVerticesFromPhysX;	
	UInt32 m_NumVerticesForRendering;
	UInt32 m_NumIndices;
	UInt32 m_NumIndicesFromPhysX;
	UInt32 m_NumIndicesForRendering;
	dynamic_array<Vector3f> m_Vertices;
	dynamic_array<Vector3f> m_TranslatedVertices;
	dynamic_array<Vector3f> *m_VerticesForRendering;
	dynamic_array<Vector3f> m_Normals;
	dynamic_array<Vector3f> m_TranslatedNormals;
	dynamic_array<Vector3f> *m_NormalsForRendering;
	dynamic_array<UInt16> m_Indices;
	dynamic_array<UInt16> m_TranslatedIndices;
	dynamic_array<UInt16> *m_IndicesForRendering;
	dynamic_array<Vector4f> m_Tangents;
	dynamic_array<Vector2f> m_UVs;
	dynamic_array<Vector2f> m_UV1s;
	dynamic_array<ColorRGBA32> m_Colors;
	UInt32 m_NumParentIndices;
	dynamic_array<UInt16> m_ParentIndices;
	dynamic_array<UInt16> m_VertexTranslationTable;

	BehaviourListNode m_FixedUpdateNode;
		
	friend class ::ClothRenderer;
};
}

#endif // ENABLE_CLOTH || DOXYGEN
#endif // DEFORMABLEMESH_H
