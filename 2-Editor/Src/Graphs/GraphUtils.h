#pragma once

#include <string>
#include <vector>

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/BaseClasses/GameObject.h"

std::string GenerateGraphName();
MonoBehaviour* GetEditorGraphData(int graphBehaviourComponentsInstanceID);
GameObject *GetGraphComponentsGameObject(int graphBehaviourComponentsInstanceID);
std::vector<int> AllGraphsOnGameObject(const GameObject* go);
std::vector<int> AllGraphsInScene();
bool IsNode(const Object* object);
bool IsGraph(const Object *object);

extern const char *kGraphsEditorBaseNamespace;
extern const char* kLogicGraphEditorNamespace;
extern const char* kAnimationBlendTreeNamespace;
extern const char* kAnimationStateMachineNamespace;
