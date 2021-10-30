#pragma once

#if (ENABLE_2D_PHYSICS || DOXYGEN) && ENABLE_SPRITECOLLIDER

#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Physics2D/PolygonColliderBase2D.h"

class Sprite;


// --------------------------------------------------------------------------


class SpriteCollider2D : public PolygonColliderBase2D
{
public:	
	REGISTER_DERIVED_CLASS (SpriteCollider2D, PolygonColliderBase2D)
	DECLARE_OBJECT_SERIALIZE (SpriteCollider2D)
	
	SpriteCollider2D (MemLabelId label, ObjectCreationMode mode);
	static void InitializeClass();

	virtual const Polygon2D& GetPoly() const;

	virtual void Reset ();
	virtual void SmartReset ();

	PPtr<Sprite> GetSprite() const { return m_Sprite; }
	void SetSprite(PPtr<Sprite> sprite);

private:
	PPtr<Sprite> m_Sprite;
};

#endif
