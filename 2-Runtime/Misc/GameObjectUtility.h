#ifndef GAMEOBJECTUTILITY_H
#define GAMEOBJECTUTILITY_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Mono/MonoScript.h"

struct ICallString;

class MonoBehaviour;
class Camera;

/// Adds a component by classID or className to the game object.
/// This method does several checks and returns null if any of them fail.
/// - Class has to be derived from Component
/// - Class has to be not already added to the game object or be allowed to be added multiple times (ComponentRequirement.cpp)
/// On failure this method returns NULL and if error != null an error string.
/// This method automatically orders filters by their sort priority.
/// * Default properties are only setup in edit mode
Unity::Component* AddComponentInternal (GameObject& go, int classID, MonoScriptPtr script, std::set<ScriptingClassPtr> &processed, std::string* error = NULL);
Unity::Component* AddComponent (GameObject& go, int classID, MonoScriptPtr script, std::string* error = NULL);
Unity::Component* AddComponent (GameObject& go, const char* className, std::string* error = NULL);
Unity::Component* AddComponentUnchecked (GameObject& go, int classID, MonoScriptPtr script, std::string* error);

/// Creates a game object with name. Add's a null terminated list of components by className.
/// Errors when a component can't be added!
Unity::GameObject& CreateGameObject (const std::string& name, const char* componentName, ...);
Unity::GameObject& CreateGameObjectWithVAList (const std::string& name, const char* componentName, va_list componentList);
Unity::GameObject& CreateGameObjectWithHideFlags (const string& name, bool isActive, int flags, const char* componentName, ...);

/// Adds a null terminated list of components by className.
/// Errors when a component can't be added!
void AddComponents (Unity::GameObject& go, const char* componentName, ...);

/// Checks if a component can be removed from its game object.
/// Does error checking so that 
/// we don't remove a component which is required by another component!
bool CanRemoveComponent(Unity::Component& component, std::string* error);
/// See whether a component can be replaced by another
/// Note: currently it only specifically handles requirements from RequireComponent attributes
bool CanReplaceComponent(Unity::Component& component, int replacementClassID, std::string* error);

bool CanAddComponent (Unity::GameObject& go, int classID);

EXPORT_COREMODULE void DestroyObjectHighLevel (Object* object, bool forceDestroy = false);
void DestroyTransformComponentAndChildHierarchy (Transform& transform);

/// On return all GameObject's with the specified tag are added to the array.
/// Only active game objects are returned
void FindGameObjectsWithTag (UInt32 tag, std::vector<Unity::GameObject*>& gos);
Camera* FindMainCamera ();

/// Returns the first game object with the specified tag found!
/// Only active game objects are returned
Unity::GameObject* FindGameObjectWithTag (UInt32 tag);

///
Unity::Component* FindAncestorComponentExactTypeImpl (Unity::GameObject& gameObject, int classId);
Unity::Component* FindAncestorComponentImpl (Unity::GameObject& gameObject, int classId);

template<class T>
T* FindAncestorComponent (Unity::GameObject& gameObject)
{
	if (T::IsSealedClass())
		return static_cast<T*> (FindAncestorComponentExactTypeImpl (gameObject, T::GetClassIDStatic()));
	else
		return static_cast<T*> (FindAncestorComponentImpl (gameObject, T::GetClassIDStatic()));
}

/// Sends the message to all active game objects
void SendMessageToEveryone(MessageIdentifier message, MessageData msgData);
bool UnloadGameObjectHierarchy (Unity::GameObject& go);
void UnloadGameObjectAndComponents (Unity::GameObject& go);

EXPORT_COREMODULE void SmartResetObject (Object& com);

Unity::Component* GetComponentWithScript (Unity::GameObject& go, int classID, MonoScriptPtr script);

std::string UnityObjectToString (Object *object);

/// Returns all components in this and any child game objects.
/// If includeInactive, in active components will be returned, otherwise only active components will be returned.
void GetComponentsInChildren (const GameObject& gameObject, bool includeInactive, int classID, dynamic_array<Unity::Component*>& outComponents);


typedef void AddComponentCallbackFunction (Unity::Component& com);
void RegisterAddComponentCallback (AddComponentCallbackFunction* callback);

int ExtractTagThrowing (ICallString& name);

#endif
