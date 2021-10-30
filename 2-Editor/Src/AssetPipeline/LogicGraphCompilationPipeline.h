#if UNITY_LOGIC_GRAPH
#pragma once

#include "Runtime/Scripting/ScriptingTypes.h"

class MonoScript;
class MonoBehaviour;

MonoClass* CompileAndLoadLogicGraph (MonoScript& script, bool logStickyErrors);
void ClearLogicGraphCompilationDirectory ();

void CompileAllGraphsInTheScene(bool logStickyErrors);
void LogGraphCompilationError(MonoBehaviour &graph, std::string error);
bool HasGraphCompilationErrors();
void ClearGraphCompilationErrors();
void InitializeMonoBehaviourWithSerializationDummy(int monoBehaviourID, MonoType* type, ScriptingObject* instance);
#endif
