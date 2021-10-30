#pragma once

#include "Editor/Src/SceneInspector.h"

class VCSceneInspector : public ISceneInspector
{
public:
	virtual bool HasObjectChangedCallback () { return true; }
	virtual void ObjectHasChanged (PPtr<Object> object);
};
