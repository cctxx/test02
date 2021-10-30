#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <vector>
#include "EditorExtension.h"
#include "Runtime/Utilities/LogAssert.h"
#include "MessageIdentifier.h"
#include "MessageHandler.h"
#include "BitField.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Containers/ConstantString.h"

#if UNITY_EDITOR
#include "Editor/Src/Utility/StaticEditorFlags.h"
#endif

class GameManager;
class MessageHandler;
class Texture2D;
class AwakeFromLoadQueue;
typedef UNITY_SET(kMemTempAlloc, Object*) TempSelectionSet;

namespace Unity
{

/// A GameObject is basically a bag of GOComponents.
/// GOComponents are added and removed at runtime.
/// This allows GameObject to be composed of an arbitrary amount of GOComponents.

/// You can query for any GOCOmponents using
/// QueryComponent and GetComponent
/// This will Ask every of the component in the GO if it is derived from the wanted class
/// eg. from inside a Component you can query for a Transform:
/// Transform* t = QueryComponent (Transform);
/// If there is no Transform inside the GameObject, NULL is returned.
/// The difference between QueryComponent and GetComponent is that
/// QueryComponent returns a ptr, GetComponent returns a reference
/// but asserts if no matching component could be found
/// Also you are not allowed to Query for Component classes
/// that are in the GameObject more than once.

/// Querys for a component class, aksing every component if it is derived from wanted class
#define QueryComponent(x)			GetGameObject ().QueryComponentT<x> (ClassID (x))
/// Same as above only that it returns a reference and asserts if no component derived from x can be found
#define GetComponent(x)		GetGameObject ().GetComponentT<x> (ClassID (x))

/// Also GameObjects support messaging.
/// MessageIdentifier kTransformChanged ("TransformChanged");
/// MessageIdentifier kTestMessage ("Test", ClassID (float));
/// In order to receive a message
/// Register the notification inside the InitializeClass of the class
/// Renderer::InitializeClass ()
/// {
/// 	REGISTER_NOTIFICATION_VOID (Renderer, kTransformChanged, TransformChanged);
/// 	REGISTER_NOTIFICATION (Renderer, kTestMessage, TestMessage, float);
/// }
/// bool Renderer::TransformChanged () { ... }
/// bool Renderer::TestMessage (float f) { ... }

/// In order to send a message use:
/// SendMessage (kTransformChanged);
/// SendMessage (kTestMessage, 0.1f, ClassID (float));

class Component;
class GameObject;


enum DeactivateOperation
{
	kNormalDeactivate = 0,
	// Deactivate was called
	kDeprecatedDeactivateToggleForLevelLoad = 1,

	// Deactivate was called because the component will be destroyed
	kWillDestroySingleComponentDeactivate = 2,
	// Deactivate was called because the entire game object will be destroyed
	kWillDestroyGameObjectDeactivate = 3
};

class EXPORT_COREMODULE GameObject : public EditorExtension
{
	public:

	typedef std::pair<SInt32, ImmediatePtr<Component> > ComponentPair;
	typedef UNITY_VECTOR(kMemBaseObject, ComponentPair)	Container;

	GameObject (MemLabelId label, ObjectCreationMode mode);
	// ~GameObject (); declared-by-macro

	REGISTER_DERIVED_CLASS (GameObject, EditorExtension)
	DECLARE_OBJECT_SERIALIZE (GameObject)

	/// An GameObject can either be active or inactive (Template GameObjects are always inactive)
	/// If an GameObject is active/inactive all its components have the same state as well.
	/// (Components that are not added to a gameobject are always inactive)
	/// Querying and messaging still works for inactive GameObjects and Components
	void Activate ();

	/// Deactiates the game object and thus all it's components.
	void Deactivate (DeactivateOperation operation = kNormalDeactivate);

	bool IsActiveIgnoreImplicitPrefab ();
	bool IsActive () const;
	bool IsSelfActive () const { return m_IsActive; }
	void SetSelfActive (bool state);

	/// Set the GameObject Layer.
	/// This is used for collisions and messaging
	void SetLayer (int layerIndex);
	int GetLayer () const  	 { return m_Layer; }
	UInt32 GetLayerMask () const { return 1 << m_Layer; }

	/// Set the Tag of the gameobject
	UInt32 GetTag () const { return m_Tag; }
	void SetTag (UInt32 tag);

	// Adds a new Component to the GameObject.
	// Using the PersistentObject interface so that Components,
	// which are not loaded at the moment can be added.
	// Use GameObjectUtility instead, you must invoke specific callbacks etc.
	void AddComponentInternal (Component* component);

	// Removes a Component from the GameObject.
	void RemoveComponentAtIndex (int index);

	int GetComponentCount () const						{ return m_Component.size (); }
	Component& GetComponentAtIndex (int i) const;
	Component* GetComponentPtrAtIndex (int i) const;
	bool GetComponentAtIndexIsLoaded (int i) const;
	int GetComponentClassIDAtIndex (int i) const;
	int CountDerivedComponents (int compareClassID)const;
	
	/// Checks if GameObject has any components conflicting with the specified classID.
	bool HasConflictingComponents (int classID) const { return FindConflictingComponentPtr (classID) != NULL; }

	/// Find the first conflicting component classID for the specified classID.
	Component* FindConflictingComponentPtr (int classID) const;

	/// Swap two components in the vector.
	void SwapComponents (int index1, int index2);

	/// Get the index of a component.
	int GetComponentIndex (Component *component);

	/// Send a message identified by messageName to all components if they can handle it
	void SendMessageAny (const MessageIdentifier& messageID, MessageData& messageData);

	/// Send a message identified by messageName to all components if they can handle it
	template<class T>
	void SendMessage (const MessageIdentifier& messageID, T messageData, int classId);

	/// Will this message be handled by any component in the gameobject?
	bool WillHandleMessage (const MessageIdentifier& messageID);

	// Use the QueryComponent macro
	// Gives back a component by its classID.
	// If the GameObject doesnt have such a Component, NULL is returned.
	template<class T>
	T* QueryComponentT (int inClassID) const;

	// Use the GetComponent macro
	// Gives back a component by its classID.
	// If the GameObject doesnt have such a Component, the function will assert
	template<class T>
	T& GetComponentT (int inClassID) const;

	// Use the GetComponent macro
	// Gives back a component by its classID.
	// If the GameObject doesnt have such a Component, the function will assert
	template<class T>
	T& GetComponentExactTypeT (int inClassID) const;

	const GameObject& GetGameObject ()const  { return *this; }
	GameObject& GetGameObject ()             { return *this; }

	virtual char const* GetName () const { return m_Name.c_str(); }
	virtual void SetName (char const* name);

	// Deprecated
	void SetActiveRecursivelyDeprecated (bool state);
	bool GetIsStaticDeprecated ();
	void SetIsStaticDeprecated (bool s);
	
	#if UNITY_EDITOR
	bool AreStaticEditorFlagsSet (StaticEditorFlags flags) const;
	StaticEditorFlags GetStaticEditorFlags () const;
	void SetStaticEditorFlags (StaticEditorFlags flags);
	#endif

	//@TODO: When we rewrite static batching in C++ fix this up
	bool IsStaticBatchable () const;

	// Callback functions
	typedef void DestroyGOCallbackFunction (GameObject* go);
	static void RegisterDestroyedCallback (DestroyGOCallbackFunction* callback);
	static void InvokeDestroyedCallback (GameObject* go);

	typedef void SetGONameFunction (GameObject* go);
	static void RegisterSetGONameCallback (SetGONameFunction* callback);

	/// Registers an message callback. Used by the REGISTER_NOTIFICATION macros
	typedef void (*MessagePtr)(void* receiver, int messageIndex, MessageData& data);
	typedef bool (*CanHandleMessagePtr)(void* receiver, int messageIndex, MessageData& data);
	static void RegisterMessageHandler (int classID, const MessageIdentifier& messageIdentifier, MessagePtr functor, int typeId);
	static void RegisterAllMessagesHandler (int classID, MessagePtr message, CanHandleMessagePtr canHandleNotification);

	virtual void Reset ();

	static void InitializeClass ();
	static void CleanupClass ();

	// Initializes the message system
	static void InitializeMessageHandlers ();
	static void InitializeMessageIdentifiers ();
	// Returns the message handler
	static class MessageHandler& GetMessageHandler ();

	// Internally used during object destruction to prevent double deletion etc.
	bool IsDestroying () const { return m_IsDestroying; }
	bool IsActivating () const { return m_IsActivating; }

	void WillDestroyGameObject ();


	inline UInt32 GetSupportedMessages ();
	void SetSupportedMessagesDirty ();

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	virtual void SetHideFlags (int flags);

	virtual void CheckConsistency ();


	static void AddComponentInternal (GameObject& gameObject, Component& clone);
	static void RemoveComponentFromGameObjectInternal (Component& clone);

	#if UNITY_EDITOR
	enum
	{
		kNotVisible = 0,
		kSelfVisible = 1,
		kVisibleAsChild = 2,
	};
	int IsMarkedVisible () const { return m_IsMarkedVisible; }
	void SetMarkedVisible (int marked) { m_IsMarkedVisible = marked; }
	void SetIcon (PPtr<Texture2D> icon);

	// Get the custom icon for this gameobject. Does not scan components for their icons.
	PPtr<Texture2D> GetIcon () const;

	UInt32 GetNavMeshLayer () const { return m_NavMeshLayer; }
	void SetNavMeshLayer (UInt32 layer) { m_NavMeshLayer = layer; SetDirty(); }
	#endif

	// Internal functions that you should never call unless you really understand all side effects.
	void SetActiveBitInternal (bool value) { m_IsActive = value; }

	void SetComponentAtIndexInternal (PPtr<Component> component, int index);

	void UpdateActiveGONode();

	void TransformParentHasChanged ();

	Container& GetComponentContainerInternal () { return m_Component; }

	void ActivateAwakeRecursively (DeactivateOperation deactivateOperation = kNormalDeactivate);
	void ActivateAwakeRecursivelyInternal (DeactivateOperation deactivateOperation, AwakeFromLoadQueue &queue);

	Component* QueryComponentImplementation (int classID) const;
	Component* QueryComponentExactTypeImplementation (int classID) const;

private:
	void GetSupportedMessagesRecalculate ();

	void MarkActiveRecursively (bool state);

	template <class TransferFunction>
	void TransferComponents(TransferFunction& transfer);

	Container	m_Component;

	UInt32			m_Layer;
	UInt16			m_Tag;
	bool			m_IsActive;
	mutable SInt8	m_IsActiveCached;
	UInt8			m_IsDestroying; //// OPTIMIZE THIS INTO A COMMON BITMASK!
	UInt8			m_IsActivating;

	UInt32			m_SupportedMessages;

	ConstantString  m_Name;

	#if UNITY_EDITOR
	UInt32          m_StaticEditorFlags;
	UnityStr        m_TagString;
	int             m_IsMarkedVisible;
	PPtr<Texture2D> m_Icon;
	UInt32			m_NavMeshLayer;
	bool            m_IsOldVersion;
	#endif

	ListNode<GameObject> m_ActiveGONode;

	static DestroyGOCallbackFunction* s_GameObjectDestroyedCallback;
	static SetGONameFunction*         s_SetGONameCallback;
	static MessageForwarders*         s_RegisteredMessageForwarders;
	static MessageHandler*            s_MessageHandler;

	friend class Component;
};

class EXPORT_COREMODULE Component : public EditorExtension
{
	private:

	ImmediatePtr<GameObject>	m_GameObject;

	public:


	DECLARE_OBJECT_SERIALIZE (Component)
	REGISTER_DERIVED_CLASS (Component, EditorExtension)

	Component (MemLabelId label, ObjectCreationMode mode);
	// ~Component (); declared-by-macro

	// Returns a reference to the GameObject holding this component
	GameObject& GetGameObject ()					{ return *m_GameObject; }
	const GameObject& GetGameObject () const		{ return *m_GameObject; }
	GameObject* GetGameObjectPtr ()		{ return m_GameObject; }
	GameObject* GetGameObjectPtr () const	{ return m_GameObject; }

	/// Send a message identified by messageName to every components of the gameobject
	/// that can handle it
	void SendMessageAny (const MessageIdentifier& messageID, MessageData& messageData);

	template<class T>
	void SendMessage (const MessageIdentifier& messageID, T messageData, int classId);
	void SendMessage (const MessageIdentifier& messageID);

	/// Is this component active?
	/// A component is always inactive if its not attached to a gameobject
	/// A component is always inactive if its gameobject is inactive
	/// If its a datatemplate, the gameobject and its components are always set to be inactive
	bool IsActive () const;

	virtual char const* GetName () const;
	virtual void SetName (char const* name);

	virtual UInt32 CalculateSupportedMessages () { return 0; }
	virtual void SupportedMessagesDidChange (int /*newMask*/) { }

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	// Invoke any callbacks prior to component destruction.
	virtual void WillDestroyComponent () { }

	/// Deactivate will be called just before the Component is going to be removed from a GameObject
	/// It can still communicate with other components at this point.
	/// Deactivate will only be called when the component is remove from the GameObject,
	/// not if the object is persistet to disk and removed from memory
	/// Deactivate will only be called if the GameObject the Component is being removed from is active
	/// YOU CAN NOT RELY ON IsActive returning false inside Deactivate
	virtual void Deactivate (DeactivateOperation /*operation*/) { }

	virtual void CheckConsistency ();

	#if UNITY_EDITOR
	// Some components always go together, e.g. when removing one you have
	// to remove other. Example, ParticleSystem and ParticleSystemRenderer.
	// Override and return class ID of that "dependent" component.
	virtual int GetCoupledComponentClassID() const { return -1; }
	#endif

	public:

	int GetGameObjectInstanceID () const { return m_GameObject.GetInstanceID(); }

	/// SetGameObject is called whenever the GameObject of a component changes.
	void SetGameObjectInternal (const GameObject* go) { m_GameObject = go; }

	friend class GameObject;
};

typedef List< ListNode<GameObject> > GameObjectList;

// A fast lookup for all tagged and active game objects
class GameObjectManager
{
public:
	static void StaticInitialize();
	static void StaticDestroy();
	// Nodes that are tagged and active
	GameObjectList m_TaggedNodes;
	// Nodes that are just active
	// (If you want to get all active nodes you need to go through tagged and active nodes)
	GameObjectList m_ActiveNodes;

	static GameObjectManager* s_Instance;
};
GameObjectManager& GetGameObjectManager();


template<class T> inline
T* GameObject::QueryComponentT (int compareClassID) const
{
	Component* com;
	if (T::IsSealedClass())
		com = QueryComponentExactTypeImplementation(compareClassID);
	else
		com = QueryComponentImplementation (compareClassID);
	DebugAssertIf (com != dynamic_pptr_cast<Component*> (com));
	return static_cast<T*> (com);
}

template<class T> inline
T& GameObject::GetComponentT (int compareClassID) const
{
	Component* com;
	if (T::IsSealedClass())
		com = QueryComponentExactTypeImplementation(compareClassID);
	else
		com = QueryComponentImplementation (compareClassID);
	DebugAssertIf (dynamic_pptr_cast<T*> (com) == NULL);
	return *static_cast<T*> (com);
}

inline Component& GameObject::GetComponentAtIndex (int i)const
{
	return *m_Component[i].second;
}

inline bool GameObject::GetComponentAtIndexIsLoaded (int i)const
{
	return m_Component[i].second.IsLoaded();
}

inline int GameObject::GetComponentClassIDAtIndex (int i) const
{
	return m_Component[i].first;
}

inline bool Component::IsActive () const
{
	GameObject* go = m_GameObject;
	return go != NULL && go->IsActive ();
}

#define IMPLEMENT_SENDMESSAGE_FOR_CLASS(ClassName)						\
	template<class T> inline											\
	void ClassName::SendMessage (const MessageIdentifier& messageID,	\
								 T messageData, int classId)			\
	{																	\
		MessageData data;												\
		data.SetData (messageData, classId);							\
		SendMessageAny (messageID, data);								\
	}

IMPLEMENT_SENDMESSAGE_FOR_CLASS (Component)
IMPLEMENT_SENDMESSAGE_FOR_CLASS (GameObject)

inline UInt32 GameObject::GetSupportedMessages ()
{
	return m_SupportedMessages;
}

inline void Component::SendMessage (const MessageIdentifier& messageID)
{
	MessageData data;
	SendMessageAny (messageID, data);
}

void SendMessageDirect (Object& target, const MessageIdentifier& messageIdentifier, MessageData& messageData);

// Compares the MessageData's type with the method signatures expected parameter type
bool EXPORT_COREMODULE CheckMessageDataType (int messageIdentifier, MessageData& data);

/// Creates a wrapper that calls a function with no parameter
#define REGISTER_MESSAGE_VOID(ClassType,NotificationName,Function) \
struct FunctorImpl_##ClassType##_##NotificationName { \
	static void Call (void* object, int, MessageData&) { \
		ClassType* castedObject = reinterpret_cast<ClassType*> (object); \
		castedObject->Function (); \
	} \
}; \
GameObject::RegisterMessageHandler (ClassID (ClassType), NotificationName, \
				FunctorImpl_##ClassType##_##NotificationName::Call, 0)

#if DEBUGMODE
#define CHECK_MSG_DATA_TYPE \
if (!CheckMessageDataType (messageID, messageData)) \
	DebugStringToFile ("Check message data", 0, __FILE__, 0, kAssert);
#else
#define CHECK_MSG_DATA_TYPE
#endif

/// Creates a wrapper thats sends the specified DataType to the member functions
#define REGISTER_MESSAGE(ClassType,NotificationName,Function,DataType) \
struct FunctorImpl_##ClassType##_##NotificationName { \
	static void Call (void* object, int messageID, MessageData& messageData) { \
		CHECK_MSG_DATA_TYPE \
		DataType data = messageData.GetData<DataType> (); \
		ClassType* castedObject = reinterpret_cast<ClassType*> (object); \
		castedObject->Function (data); \
	} \
}; \
GameObject::RegisterMessageHandler (ClassID (ClassType), \
									NotificationName, \
									FunctorImpl_##ClassType##_##NotificationName::Call, \
									ClassID (DataType))


/// Creates a wrapper thats sends the specified DataType to the member functions
#define REGISTER_MESSAGE_PTR(ClassType,NotificationName,Function,DataType) \
struct FunctorImpl_##ClassType##_##NotificationName { \
	static void Call (void* object, int messageID, MessageData& messageData) { \
		CHECK_MSG_DATA_TYPE \
		DataType* data = messageData.GetData<DataType*> (); \
		ClassType* castedObject = reinterpret_cast<ClassType*> (object); \
	castedObject->Function (data); \
	} \
}; \
GameObject::RegisterMessageHandler (ClassID (ClassType), \
									NotificationName, \
									FunctorImpl_##ClassType##_##NotificationName::Call, \
									ClassID (DataType))


}

using namespace Unity;

#endif

