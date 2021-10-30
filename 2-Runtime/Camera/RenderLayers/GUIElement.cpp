#include "UnityPrefix.h"
#include "GUIElement.h"
#include "GUILayer.h"
#include "Runtime/Math/Vector2.h"

GUIElement::GUIElement (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

GUIElement::~GUIElement ()
{
}

void GUIElement::AddToManager ()
{
	GUILayer::ms_GUIElements->add_delayed (this);
}

void GUIElement::RemoveFromManager ()
{
	GUILayer::ms_GUIElements->remove_delayed (this);
}

bool GUIElement::HitTest (const Vector2f& screenSpacePosition, const Rectf& cameraRect)
{
	Rectf rect = GetScreenRect (cameraRect);
	return rect.Contains (screenSpacePosition.x, screenSpacePosition.y);
}

IMPLEMENT_CLASS (GUIElement)
