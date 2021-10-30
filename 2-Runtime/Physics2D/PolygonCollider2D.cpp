#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS

#include "Runtime/Physics2D/PolygonCollider2D.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#if ENABLE_SPRITES
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#endif

IMPLEMENT_CLASS (PolygonCollider2D)
IMPLEMENT_OBJECT_SERIALIZE (PolygonCollider2D)


// --------------------------------------------------------------------------


PolygonCollider2D::PolygonCollider2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}


PolygonCollider2D::~PolygonCollider2D ()
{
}


template<class TransferFunction>
void PolygonCollider2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER (m_Poly);
}

void PolygonCollider2D::Reset ()
{
	Super::Reset ();

	// Create pentagon shape
	CreateNgon (5, Vector2f(1, 1), Vector2f(0,0), m_Poly);
}


void PolygonCollider2D::SmartReset ()
{
	float radius;
	Vector2f offset;

	GameObject* go = GetGameObjectPtr();
#if ENABLE_SPRITES
	if (go)
	{
		SpriteRenderer* sr = go->QueryComponentT<SpriteRenderer>(ClassID(SpriteRenderer));
		if (sr)
		{
			Sprite* sprite = sr->GetSprite();
			if (sprite)
			{
				m_Poly.GenerateFrom(sprite, Vector2f(0, 0), 0.25f, 200, true);
				if (m_Poly.GetPathCount() > 0) // We might fail if all pixels are under the threshold. No workaround in 4.3.
					return;
			}
		}
	}
#endif

	// Resolve what size collider we should have from object bounds
	AABB aabb;
	if (go && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f dist = aabb.GetExtent ();
		radius = std::max(dist.x, dist.y);
		if (radius <= 0.0f)
			radius = 1.0f;
		offset.x = aabb.GetCenter().x;
		offset.y = aabb.GetCenter().y;
	}
	else
	{
		radius = 1.0f;
		offset = Vector2f::zero;
	}

	// Create pentagon shape
	CreateNgon (5, Vector2f(radius, radius), offset, m_Poly);
}


void PolygonCollider2D::RefreshPoly()
{
	Create();
	SetDirty();
}


void PolygonCollider2D::CreatePrimitive (int sides, Vector2f scale, Vector2f offset)
{
	Assert (sides > 2);
	Assert (scale.x > 0.0f);
	Assert (scale.y > 0.0f);

	CreateNgon (sides, scale, offset, m_Poly);

	// Create polygon shape.
	Create();
	SetDirty();
}


void PolygonCollider2D::CreateNgon (const int sides, const Vector2f scale, const Vector2f offset, Polygon2D& polygon2D)
{
	Polygon2D::TPath path;

	// Generate regular n-sided polygon.
	const float tau = kPI * 2.0f;
	const float chordAngle = tau / (float)sides;
	float angle = 0.0f;
	for (int chord = 0; chord < sides; ++chord, angle += chordAngle)
	{
		path.push_back (Vector2f(offset.x + scale.x * Sin(angle), offset.y + scale.y * Cos(angle)));
	}

	// Set polygon path.
	polygon2D.SetPathCount (1);
	polygon2D.SetPath (0, path);
}

#endif // #if ENABLE_2D_PHYSICS
