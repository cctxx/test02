#ifndef MONOEXPORTUTILITY_H
#define MONOEXPORTUTILITY_H

#include "Runtime/Scripting/ScriptingUtility.h"

#if ENABLE_MONO
#include "MonoManager.h"

#if UNITY_EDITOR
ScriptingObjectPtr ClassIDToScriptingTypeObjectIncludingBasicTypes (int classID);
#endif

#endif
#endif
