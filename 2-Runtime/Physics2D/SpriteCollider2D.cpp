#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS && ENABLE_SPRITECOLLIDER

#include "Runtime/Physics2D/SpriteCollider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"

#include "External/Box2D/Box2D/Box2D.h"
#include "External/libtess2/libtess2/tesselator.h"

PROFILER_INFORMATION(gPhysics2DProfileSpriteColliderCreate, "Physics2D.SpriteColliderCreate", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileSpriteColliderDecomposition, "Physics2D.SpriteColliderDecomposition", kProfilerPhysics)

IMPLEMENT_CLASS_INIT_ONLY (SpriteCollider2D)
IMPLEMENT_OBJECT_SERIALIZE (SpriteCollider2D)


// --------------------------------------------------------------------------


static Polygon2D gEmptyPolygon2D;
void SpriteCollider2D::InitializeClass()
{
	gEmptyPolygon2D.Clear();
}


SpriteCollider2D::SpriteCollider2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}


SpriteCollider2D::~SpriteCollider2D ()
{
}


template<class TransferFunction>
void SpriteCollider2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER (m_Sprite);
}


void SpriteCollider2D::Reset ()
{
	Super::Reset ();

	m_Sprite = NULL;
}


void SpriteCollider2D::SmartReset ()
{
	Super::SmartReset ();
#if UNITY_EDITOR
	GameObject* go = GetGameObjectPtr();
	if (go)
	{
		SpriteRenderer* sr = go->QueryComponent(SpriteRenderer);
		if (sr)
			m_Sprite = sr->GetSprite();
	}
#endif
}

void SpriteCollider2D::SetSprite(PPtr<Sprite> sprite)
{
	if (m_Sprite != sprite)
	{
		m_Sprite = sprite;
	
		Create();
		SetDirty();
	}
}

const Polygon2D& SpriteCollider2D::GetPoly() const
{
	return m_Sprite.IsNull() ? gEmptyPolygon2D : m_Sprite->GetPoly();
}

#endif // #if ENABLE_2D_PHYSICS
