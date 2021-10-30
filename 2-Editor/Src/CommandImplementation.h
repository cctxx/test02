#ifndef COMMANDIMPLEMENTATION_H
#define COMMANDIMPLEMENTATION_H

namespace Unity { class GameObject; class Component; }

class StateMachine;
class AnimatorController;
class State;


bool PasteGameObjectsFromPasteboard ();
bool CopyGameObjectsToPasteboard ();
bool DuplicateGameObjectsUsingPasteboard ();
void DeleteGameObjectSelection ();

bool CopyGraphAndNodesToPasteboard (int sourceGraphBehaviourInstanceID, const std::set<SInt32>& ptrs);
Object* InstantiateGraphAndNodesFromPasteboard (int destinationGraphBehaviourInstanceID);
Object* DuplicateGraphAndNodesUsingPasteboard (int graphBehaviourInstanceID, const std::set<SInt32>& ptrs);

void ClearComponentInPasteboard ();
bool HasComponentInPasteboard (int& outClassID);
bool HasMatchingComponentInPasteboard (Unity::Component* comp);
bool CopyComponentToPasteboard (Unity::Component* comp);
bool PasteComponentFromPasteboard (Unity::GameObject* go);
bool PasteComponentValuesFromPasteboard (Unity::Component* dest);

bool HasStateMachineDataInPasteboard ();

bool CopyStateToPasteboard(State* state, AnimatorController const * controller);
bool CopyStateMachineToPasteboard(StateMachine* stateMachine, AnimatorController const * controller);

bool PasteToStateMachineFromPasteboard(StateMachine* stateMachine, AnimatorController* animatorController);


#endif
