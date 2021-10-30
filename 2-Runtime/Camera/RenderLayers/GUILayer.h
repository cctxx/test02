#ifndef GUILAYER_H
#define GUILAYER_H

#include "GUIElement.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Utilities/delayed_set.h"

/// A GUI Layer is attached to the camera.
/// It tracks all enabled GUIElements (eg. Text, GUITexture) and renders them
class GUILayer : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS (GUILayer, Behaviour)
	
	GUILayer (MemLabelId label, ObjectCreationMode mode);
		
	void RenderGUILayer();
	GUIElement* HitTest (const Vector2f& screenPosition);

	static void InitializeClass ();
	static void CleanupClass ();

	// Behaviour
	virtual void AddToManager() { };
	virtual void RemoveFromManager() { };
	
private:	
	typedef delayed_set <PPtr<GUIElement> > GUIElements;
	static GUIElements* ms_GUIElements;
	friend class GUIElement;
};

#endif
