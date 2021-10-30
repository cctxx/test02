#ifndef GUIELEMENT_H
#define GUIELEMENT_H

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Rect.h"
class Vector2f;


// Base class for all GUI elements.
// Registers itself with the GUILayer when enabled.
class GUIElement : public Behaviour
{
public:
	
	REGISTER_DERIVED_ABSTRACT_CLASS (GUIElement, Behaviour)
	
	GUIElement (MemLabelId label, ObjectCreationMode mode);
	
	virtual void RenderGUIElement (const Rectf& cameraRect) = 0;
	virtual Rectf GetScreenRect (const Rectf& cameraRect) = 0;
	
	bool HitTest (const Vector2f& screenSpaceCoordinates, const Rectf& cameraRect);
	
private:
		
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
};

#endif
