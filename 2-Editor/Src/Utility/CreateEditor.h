#pragma once

#include "InspectorMode.h"
#include "Runtime/BaseClasses/BaseObject.h"

class MonoBehaviour;
MonoBehaviour* CreateInspectorMonoBehaviour (std::vector <PPtr<Object> > &objs, ScriptingObjectPtr forcedClass, InspectorMode mode, bool isHidden);

void SetCustomEditorIsDirty (MonoBehaviour* inspector, bool dirty);
bool IsCustomEditorDirty (MonoBehaviour* inspector);