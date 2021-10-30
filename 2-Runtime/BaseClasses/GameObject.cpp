#include "UnityPrefix.h"
#include "GameObject.h"
#include "CleanupManager.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Tags.h"
#include "MessageHandler.h"
#include "Runtime/Misc/ReproductionLog.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Misc/ComponentRequirement.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Containers/ConstantStringSerialization.h"
#include "Runtime/Profiler/Profiler.h"
#if UNITY_EDITOR
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Src/Utility/StaticEditorFlags.h"
#endif
#if UNITY_WII
#include <rvlaux/clib.h>
#endif

using namespace std;

namespace Unity
{

PROFILER_INFORMATION (gActivateGameObjectProfiler, "GameObject.Activate", kProfilerScripts)
PROFILER_INFORMATION (gDeactivateGameObjectProfiler, "GameObject.Deactivate", kProfilerScripts)
	
Unity::GameObject::DestroyGOCallbackFunction* Unity::GameObject::s_GameObjectDestroyedCallback = NULL;
Unity::GameObject::SetGONameFunction* Unity::GameObject::s_SetGONameCallback = NULL;
MessageForwarders* Unity::GameObject::s_RegisteredMessageForwarders = NULL;
MessageHandler* Unity::GameObject::s_MessageHandler = NULL;

GameObject::GameObject (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode),
//	m_Component (GameObject::Container::allocator_type (*baseAllocator)),
	m_ActiveGONode (this)
{
	m_SupportedMessages = 0;
	m_IsDestroying = false;
	m_IsActivating = false;
	m_Tag = 0;
	m_IsActive = false;
	m_IsActiveCached = -1;

	#if UNITY_EDITOR
	m_IsOldVersion = false;
	m_StaticEditorFlags = 0;
	m_IsMarkedVisible = kSelfVisible;
	#endif
}

void GameObject::Reset ()
{
	Super::Reset ();
	m_Layer = kDefaultLayer;
	m_Tag = 0;
	#if UNITY_EDITOR
	m_StaticEditorFlags = 0;
	m_TagString = TagToString (m_Tag);
	m_NavMeshLayer = 0;
	#endif
}

GameObject::~GameObject ()
{
	Assert(!m_ActiveGONode.IsInList());
}
	
void GameObject::WillDestroyGameObject ()
{
	Assert(!m_IsDestroying);
	m_IsDestroying = true;
		
	// Find a component with the requested ID
	Container::const_iterator i;
	Container::const_iterator end = m_Component.end ();
	for (i=m_Component.begin ();i != end; ++i)
	{
		Component& com = *i->second;
		com.WillDestroyComponent();
	}
}	

void GameObject::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	#if SUPPORT_LOG_ORDER_TRACE
	if (IsActive() && RunningReproduction())
	{
		if (SUPPORT_LOG_ORDER_TRACE == 2)
		{	
			LogString(Format("AwakeFromLoad %s (%s) [%d]", GetName(), GetClassName().c_str(), GetInstanceID()));
		}
		else
		{
			LogString(Format("AwakeFromLoad %s (%s)", GetName(), GetClassName().c_str()));
		}
	}
	#endif
	
	Super::AwakeFromLoad (awakeMode);
	SetSupportedMessagesDirty ();
	UpdateActiveGONode ();

	if (s_SetGONameCallback)
		s_SetGONameCallback(this);
	
	#if UNITY_EDITOR
	// When we are modifying the game object active state from the inspector
	// We need to Activate / Deactivate the relevant components
	// This never happens in the player.
	if (awakeMode == kDefaultAwakeFromLoad)
		ActivateAwakeRecursively();
	#endif
	
}

int GameObject::CountDerivedComponents (int compareClassID)const
{
	int count = 0;
	Container::const_iterator i;
	for (i=m_Component.begin ();i != m_Component.end (); ++i)
		count += Object::IsDerivedFromClassID (i->first, compareClassID);
	return count;
}


Component* GameObject::FindConflictingComponentPtr (int classID) const
{
	const vector_set<int>& conflicts = FindConflictingComponents(classID);
	if (conflicts.empty())
		return NULL;

	for (Container::const_iterator i = m_Component.begin(); i != m_Component.end(); ++i)
	{
		for (vector_set<int>::const_iterator c = conflicts.begin(); c != conflicts.end(); ++c)
		{
			if (Object::IsDerivedFromClassID(i->first, *c))
				return i->second;
		}
	}

	return NULL;
}


void GameObject::SetName (char const* name)
{
	m_Name.assign(name, GetMemoryLabel());
	if (s_SetGONameCallback)
		s_SetGONameCallback(this);
	SetDirty ();
}

void GameObject::UpdateActiveGONode()
{
	m_ActiveGONode.RemoveFromList();
	if (IsActive())
	{
		if (m_Tag != 0)
			GetGameObjectManager().m_TaggedNodes.push_back(m_ActiveGONode);
		else
			GetGameObjectManager().m_ActiveNodes.push_back(m_ActiveGONode);
	}
}

void GameObject::MarkActiveRecursively (bool state)
{
	Transform &transform = GetComponent (Transform);
	for (Transform::iterator i=transform.begin ();i != transform.end ();i++)
		(*i)->GetGameObject().MarkActiveRecursively (state);

	m_IsActive = state;
	SetDirty();
}

void GameObject::ActivateAwakeRecursivelyInternal (DeactivateOperation deactivateOperation, AwakeFromLoadQueue &queue)
{
	if (m_IsActivating)
	{
		ErrorStringObject("GameObject is already being activated or deactivated.", this);
		return;
	}
	bool state;
	bool changed;
	m_IsActivating = true;
	if (m_IsActiveCached != -1)
	{
		bool oldState = m_IsActiveCached;
		m_IsActiveCached = -1;
		state = IsActive();
		changed = oldState != state;
	}
	else
	{
		state = IsActive();
		changed = true;
	}
	
	Transform *transform = QueryComponent (Transform);
	if (transform)
	{
		// use a loop by index rather than a iterator, as the children can adjust
		// the child list during the Awake call, and invalidate the iterator
		for (int i = 0; i < transform->GetChildrenCount(); i++)
			transform->GetChild(i).GetGameObject().ActivateAwakeRecursivelyInternal (deactivateOperation, queue);
	}
	
	if (changed)
	{
		for (int i=0;i<m_Component.size ();i++)
		{
			Component& component = *m_Component[i].second;
			if (state)
			{
				AssertIf (&*component.m_GameObject != this);
				component.SetGameObjectInternal (this);
				if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
					queue.Add(*m_Component[i].second);
				else
					component.AwakeFromLoad (kActivateAwakeFromLoad);
			}
			else
				component.Deactivate (deactivateOperation);
		}
		
		if (state)
			UpdateActiveGONode ();
		else
			m_ActiveGONode.RemoveFromList();
	}
	m_IsActivating = false;
}

void GameObject::ActivateAwakeRecursively (DeactivateOperation deactivateOperation)
{
	AwakeFromLoadQueue queue (kMemTempAlloc);
	ActivateAwakeRecursivelyInternal (deactivateOperation, queue);
	queue.AwakeFromLoad (kActivateAwakeFromLoad);
}

void GameObject::SetActiveRecursivelyDeprecated (bool state)
{
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion3_5_a1))
	{
		if (IsPrefabParent ())
		{
			ErrorString(Format("Prefab GameObject's can not be made active! (%s)", GetName()));
			return;
		}

		// First Mark all objects as active
		MarkActiveRecursively (state);
		
		// Then awake them.
		ActivateAwakeRecursively ();
	}
	else 
	{
		// Old versions used to mark active and awake each object after another.
		// That would cause problems with colliders being created twice, once without the active 
		// parent rigibdodies, and once with, thus causing the unnecessary creation and destruction
		// of static colliders (slow). So we fixed it (see above), but we keep the old behaviour for
		// legacy content.
		Transform &transform = GetComponent (Transform);
		for (Transform::iterator i=transform.begin ();i != transform.end ();i++)
			(*i)->GetGameObject().SetActiveRecursivelyDeprecated (state);
			
		if (state)
			Activate();
		else
			Deactivate();			
	}
}

void GameObject::AddComponentInternal (Component* com)
{
	AssertIf (com == NULL);
	{
		m_Component.push_back (std::make_pair (com->GetClassID (), ImmediatePtr<Component> (com)));
	}
	// Make sure it isn't already added to another GO
	Assert ( com->m_GameObject.GetInstanceID() == 0 || com->GetGameObjectPtr() == this );

	com->SetHideFlags(GetHideFlags());
	com->m_GameObject = this;

	if (IsActive ())
		com->AwakeFromLoad (kActivateAwakeFromLoad);
	else
		com->AwakeFromLoad (kDefaultAwakeFromLoad);
	
	com->SetDirty ();
	SetDirty ();

	SendMessage(kDidAddComponent, com, ClassID (Component));
	
	SetSupportedMessagesDirty ();
}

Component* GameObject::QueryComponentExactTypeImplementation (int classID) const
{
	// Find a component with the requested ID
	Container::const_iterator i;
	Container::const_iterator end = m_Component.end ();
	for (i=m_Component.begin ();i != end; ++i)
	{
		if (i->first == classID)
			return i->second;
	}
	
	return NULL;
}
	
	
Component* GameObject::QueryComponentImplementation (int classID) const
{
	// Find a component with the requested ID
	Container::const_iterator i;
	Container::const_iterator end = m_Component.end ();
	for (i=m_Component.begin ();i != end; ++i)
	{
		if (Object::IsDerivedFromClassID (i->first, classID))
			return i->second;
	}
	
	return NULL;
}

void GameObject::RemoveComponentAtIndex (int index)
{
	Container::iterator i = m_Component.begin () + index;

	Component* com = i->second;
	AssertIf (com == NULL);

	m_Component.erase (i);
	com->m_GameObject = NULL;

	com->SetDirty ();
	SetDirty ();
	SetSupportedMessagesDirty ();
}

void GameObject::SetComponentAtIndexInternal (PPtr<Component> component, int index)
{
	m_Component[index].first = component->GetClassID();
	m_Component[index].second.SetInstanceID(component.GetInstanceID());
}


void GameObject::SetSupportedMessagesDirty ()
{
	Assert(!IsDestroying());
	
	int oldSupportedMessage = m_SupportedMessages;
	m_SupportedMessages = 0;
	if (IsDestroying ())
		return;
	
	GetSupportedMessagesRecalculate ();
	if (oldSupportedMessage != m_SupportedMessages)
	{
		for (Container::iterator i=m_Component.begin ();i != m_Component.end (); ++i)
			if (i->second)
				i->second->SupportedMessagesDidChange (m_SupportedMessages);
	}
}

void GameObject::GetSupportedMessagesRecalculate ()
{
	Assert(!IsDestroying());
	
	m_SupportedMessages = 0;
	for (Container::iterator i=m_Component.begin ();i != m_Component.end (); ++i)
		if (i->second)
			m_SupportedMessages |= i->second->CalculateSupportedMessages ();
}

Component* GameObject::GetComponentPtrAtIndex (int i)const
{
	return m_Component[i].second;
}
	
int GameObject::GetComponentIndex (Component *component)
{
	Assert(!IsDestroying());
	
	for (int i = 0; i < GetComponentCount (); i++) 
	{
		if (&GetComponentAtIndex (i) == component)
			return i;
	}

	return -1;
}


void GameObject::SwapComponents (int index1, int index2)
{
	AssertIf (index1 > m_Component.size() || index1 < 0);
	AssertIf (index2 > m_Component.size() || index2 < 0);

	ComponentPair tmp = m_Component[index1];
	m_Component[index1] = m_Component[index2];
	m_Component[index2] = tmp;
	
	Component* comp1 = m_Component[index1].second;
	Component* comp2 = m_Component[index2].second;
	if (comp1 && comp1->IsDerivedFrom(ClassID(Behaviour)))
	{
		Behaviour* beh = static_cast<Behaviour*>(comp1);
		if (beh->GetEnabled())
		{
			beh->SetEnabled (false);
			beh->SetEnabled (true);
		}
	}
	if (comp2 && comp2->IsDerivedFrom(ClassID(Behaviour)))
	{
		Behaviour* beh = static_cast<Behaviour*>(comp2);
		if (beh->GetEnabled())
		{
			beh->SetEnabled (false);
			beh->SetEnabled (true);
		}
	}
	SetDirty();
}

bool GameObject::IsActive () const
{
	if (m_IsActiveCached != -1)
		return m_IsActiveCached;
	
	// For pre 4.0 content activate state is the same as m_IsActive.
	if (!IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		m_IsActiveCached = m_IsActive;
	else 
	{
		// Calculate active state based on the hierarchy
		m_IsActiveCached = m_IsActive && !(IsPersistent() || IsPrefabParent());
		Transform *trs = QueryComponent (Transform);
		if (trs)
		{
			Transform *parent = GetComponent (Transform).GetParent();
			if (parent)
				m_IsActiveCached = m_IsActiveCached && parent->GetGameObject().IsActive();
		}
	}
	
	return m_IsActiveCached;
}
	
bool GameObject::IsActiveIgnoreImplicitPrefab ()
{
	// This function does not make sense to be called for old content.
	Assert (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1));
	
	Transform *trs = QueryComponent (Transform);
	if (trs)
	{
		Transform *parent = GetComponent (Transform).GetParent();
		if (parent)
			return m_IsActive && parent->GetGameObject().IsActiveIgnoreImplicitPrefab();
	}
	
	return m_IsActive;
}

void GameObject::Activate ()
{
	if (IsActive())
		return;
	
	PROFILER_AUTO(gActivateGameObjectProfiler, this);
	
	SetDirty ();

	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
	{
		m_IsActive = true;
		ActivateAwakeRecursively ();
		// After AwakeFromLoad, 'this' could have been destroyed (if user is Destroying in OnEnable or Awake)
		// So do not access it any further
	}
	else
	{
		if (IsPrefabParent ())
		{
			ErrorString(Format("Prefab GameObject's can not be made active! (%s)", GetName()));
			return;
		}
		
		if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_3_a1) && IsPersistent ())
		{
			ErrorString(Format("GameObjects stored in assets can not be made active! (%s)", GetName()));
			return;
		}

		m_IsActive = true;
		m_IsActiveCached = m_IsActive;
		for (int i=0;i<m_Component.size ();i++)
		{
			Component& component = *m_Component[i].second;
			AssertIf (&*component.m_GameObject != this);
			component.m_GameObject = this;
			component.AwakeFromLoad (kActivateAwakeFromLoad);
		}
		
		UpdateActiveGONode ();
	}

}

void GameObject::Deactivate (DeactivateOperation operation)
{
	PROFILER_AUTO(gDeactivateGameObjectProfiler, this)
	
	if (!IsActive())
	{
		if (m_IsActive)
		{
			m_IsActive = false;
			SetDirty ();
		}
		return;
	}
	
	m_IsActive = false;

	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		ActivateAwakeRecursively (operation);
	else
	{
		m_IsActiveCached = m_IsActive;
		for (int i=0;i<m_Component.size ();i++)
		{
			Component& com = *m_Component[i].second;
			com.Deactivate (operation);
		}
		
		m_ActiveGONode.RemoveFromList();		
	}
		
	SetDirty ();
}
	
void GameObject::SetSelfActive (bool state)
{
	if (state)
		Activate();
	else
		Deactivate(kNormalDeactivate);
}

void GameObject::AddComponentInternal (GameObject& gameObject, Component& clone)
{
	SET_ALLOC_OWNER(&gameObject);
	Assert(clone.m_GameObject == NULL);
	gameObject.m_Component.push_back(make_pair(clone.GetClassID(), &clone));
	clone.m_GameObject = &gameObject;
}

void GameObject::RemoveComponentFromGameObjectInternal (Component& clone)
{
	GameObject* go = clone.GetGameObjectPtr();
	if (go == NULL)
		return;
	
	int index = go->GetComponentIndex(&clone);
	if (index == -1)
		return;
	
	go->m_Component.erase(go->m_Component.begin() + index);
	clone.m_GameObject = NULL;
}
	
void GameObject::CheckConsistency ()
{
	Super::CheckConsistency ();
	
	
	// Remove Components from m_Component if they vanished without deactivating.
	// (eg. class hierarchy changed and the class doesn't exist anymore when loading from disk)
	int i = 0;
	while (i < m_Component.size ())
	{
		// Use is object available instead of normal comparison so that we dont load the object
		// which might already trigger the component to query for other components.
		int CurComponentInstanceID = m_Component[i].second.GetInstanceID ();
		if (!IsObjectAvailable (CurComponentInstanceID))
		{
			ErrorStringObject (Format("Component %s could not be loaded when loading game object. Cleaning up!", ClassIDToString(m_Component[i].first).c_str()), this);
			m_Component.erase (m_Component.begin () + i);
		}
		else
			i++;
	}
	
	// Preload all the components!
	// This is necessary to avoid recursion and awake functions calling remove component,
	// When we are removing the wrong ones anyway.
	i = 0;
	while (i < m_Component.size ())
	{
		Component* com = m_Component[i].second;
		UNUSED(com);
		i++;
	}

	// Remove Components with wrong gameobject ptrs
	i = 0;
	while (i < m_Component.size ())
	{
		Component* com = m_Component[i].second;
		if (com && com->GetGameObjectPtr () == this)
		{
			i++;
			continue;
		}
		
		if (com)
		{
			if (com->GetGameObjectPtr () == NULL)
			{
				com->SetGameObjectInternal (this);
				ErrorStringObject ("Component (" + com->GetClassName () +  ") has a broken GameObject reference. Fixing!", this);
				continue;
			}
			else
			{
				ErrorStringObject ("Failed to load component (" + com->GetClassName () +  ")! Removing it!", this);
				com->SetHideFlags(kHideAndDontSave);
			}
		}
		else
		{
			ErrorStringObject ("Failed to load component (" + Object::ClassIDToString (m_Component[i].first) +  ")! Removing it!", this);
		}

		m_Component.erase (m_Component.begin () + i);
	}

	// make sure we always have at least one transform on a gameobject
	int transformCount = 0;
	for (int i = 0; i < m_Component.size (); ++i)
	{
		if(m_Component[i].first == ClassID (Transform))
			transformCount++;

		if (transformCount > 1)
		{
			// More than one transform on object.  If it's a scene object (transient),
			// remove the extraneous transform.  For prefabs (persistent), touching the
			// transform hierarchy will lead to all kinds of troubles so we just leave
			// it be.
			if (!IsPersistent ())
			{
				Transform* com = static_cast<Transform*> (&*m_Component[i].second);
				com->SetParent (NULL);

				RemoveComponentAtIndex (i);
				--i;

				GameObject* dummyObject = CreateObjectFromCode<GameObject> ();
				dummyObject->SetName ("!! ORPHAN TRANSFORM !!");
				dummyObject->AddComponentInternal (com);

				ErrorStringObject ("Object has multiple transform components. Created dummy GameObject and added transform to it!", this);
			}
			else
			{
				ErrorStringObject ("Object has multiple transform components!", this);
			}
		}
	}
	if(transformCount == 0)
	{
		ErrorStringObject (Format("Transform component could not be found on game object. Adding one!"), this);
		AddComponentUnchecked(*this,ClassID(Transform), NULL, NULL);
	}

#if UNITY_EDITOR
	if (m_IsOldVersion && m_IsActive && !IsPersistent() && !IsActiveIgnoreImplicitPrefab())
		WarningStringObject ("GameObject is active but a parent is inactive. Active state is now inherited. Change the parenting to get back the old behaviour!", this);
#endif

	SetSupportedMessagesDirty ();
}

void GameObject::SetLayer (int layer)
{
	if (layer >= 0 && layer < 32)
	{
		m_Layer = layer;	
		MessageData data;
		SendMessageAny (kLayerChanged, data);
		SetDirty (); 
	}
	else
		ErrorString ("A game object can only be in one layer. The layer needs to be in the range [0...31]");
}

void GameObject::SetTag (UInt32 tag)
{
	#if UNITY_EDITOR
	m_TagString = TagToString (tag);
	#endif
	
	m_Tag = tag; 
	UpdateActiveGONode();
	
	AssertIf (tag != -1 && tag != m_Tag);
	AssertIf (tag == -1 && m_Tag != 0xFFFF);
	MessageData data;
	SendMessageAny (kLayerChanged, data);
	SetDirty (); 
}

void GameObject::SetHideFlags (int flags)
{
	SetHideFlagsObjectOnly(flags);
	for (int i=0;i<m_Component.size ();i++)
	{
		Component& com = *m_Component[i].second;
		com.SetHideFlags(flags);
	}
}

template<class TransferFunction>
void GameObject::TransferComponents (TransferFunction& transfer)
{
	Container* components_to_serialize = &m_Component;

	// When cloning objects for prefabs and instantiate, we don't use serialization to duplicate the hierarchy,
	// we duplicate the hierarchy directly
	if (!SerializePrefabIgnoreProperties(transfer))
		return;

#if UNITY_EDITOR
	Container filtered_components;
	if (transfer.IsWritingGameReleaseData ())
	{
		components_to_serialize = &filtered_components;
		for (Container::iterator i = m_Component.begin(); i != m_Component.end(); i++)
		{
			if (IsClassSupportedOnBuildTarget(i->first, transfer.GetBuildingTarget().platform))
				filtered_components.push_back (*i);
		}
	}
#endif

	transfer.Transfer (*components_to_serialize, "m_Component", kHideInEditorMask | kStrongPPtrMask | kIgnoreWithInspectorUndoMask);
}

	
	
	
	
template<class TransferFunction>
void GameObject::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (4);
	TransferComponents(transfer);

	TRANSFER (m_Layer);

	#if GAMERELEASE
	TransferConstantString(m_Name, "m_Name", kNoTransferFlags, GetMemoryLabel(), transfer);
	TRANSFER (m_Tag);
	transfer.Transfer (m_IsActive, "m_IsActive");
	#else
	
	#if UNITY_EDITOR
	if (transfer.IsVersionSmallerOrEqual (3))
		m_IsOldVersion = true;
	#endif
	
	if (transfer.IsOldVersion (3) || transfer.IsCurrentVersion ())
	{
		TransferConstantString(m_Name, "m_Name", kNoTransferFlags, GetMemoryLabel(), transfer);
		
		if (transfer.IsSerializingForGameRelease ())
		{
			TRANSFER (m_Tag);
			if (transfer.IsReading ())
				m_TagString = TagToString (m_Tag);

			transfer.Transfer (m_IsActive, "m_IsActive");
		}
		else
		{
			transfer.Transfer (m_TagString, "m_TagString");
			if (transfer.IsReading ())
				m_Tag = StringToTagAddIfUnavailable (m_TagString);

			transfer.Transfer (m_Icon, "m_Icon", kNoTransferFlags);
			transfer.Transfer (m_NavMeshLayer, "m_NavMeshLayer", kHideInEditorMask);

			transfer.Transfer (m_StaticEditorFlags, "m_StaticEditorFlags", kNoTransferFlags | kGenerateBitwiseDifferences);
			
			// Read deprecated static flag and set it up as m_StaticEditorFlags
			if (transfer.IsReadingBackwardsCompatible ())
			{
				bool isStatic = false;
				transfer.Transfer (isStatic, "m_IsStatic", kNoTransferFlags);
				if (isStatic)
					m_StaticEditorFlags = 0xFFFFFFFF;
			}
			transfer.Transfer (m_IsActive, "m_IsActive", kHideInEditorMask);
		}
	}
	else if (transfer.IsOldVersion (2))
	{
		TRANSFER (m_TagString);
		m_Tag = StringToTag (m_TagString);
		transfer.Transfer (m_IsActive, "m_IsActive");
	}
	else if (transfer.IsOldVersion (1))
	{
		TRANSFER (m_Tag);
		m_TagString = TagToString (m_Tag);
		transfer.Transfer (m_IsActive, "m_IsActive");
	}
	#endif

	// Make sure that old prefabs are always active.
	if (transfer.IsVersionSmallerOrEqual (3) && IsPersistent() && IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		m_IsActive = true;
}

bool GameObject::GetIsStaticDeprecated ()
{
#if UNITY_EDITOR
	return m_StaticEditorFlags != 0;
#else
	return false;
#endif
}

void GameObject::SetIsStaticDeprecated(bool s)
{
#if UNITY_EDITOR
	m_StaticEditorFlags = s ? 0xFFFFFFFF : 0;
	SetDirty();
#endif
}

bool GameObject::IsStaticBatchable () const
{
#if UNITY_EDITOR
	return AreStaticEditorFlagsSet (kBatchingStatic);
#else
	return false;
#endif
}

#if UNITY_EDITOR

bool GameObject::AreStaticEditorFlagsSet (StaticEditorFlags flags) const
{
	return (m_StaticEditorFlags & (UInt32)flags) != 0;
}

StaticEditorFlags GameObject::GetStaticEditorFlags () const
{
	return (StaticEditorFlags)m_StaticEditorFlags;
}

void GameObject::SetStaticEditorFlags (StaticEditorFlags flags)
{
	m_StaticEditorFlags = (UInt32)flags;
	SetDirty();
}

void GameObject::SetIcon (PPtr<Texture2D> icon) 
{
	if (m_Icon != icon)
	{
		m_Icon = icon;
		SetDirty ();
	}
}

PPtr<Texture2D> GameObject::GetIcon () const
{
	return m_Icon;
}
#endif
	
void GameObject::RegisterDestroyedCallback (DestroyGOCallbackFunction* callback)
{
	s_GameObjectDestroyedCallback = callback;
}

void GameObject::InvokeDestroyedCallback (GameObject* go)
{
	if (s_GameObjectDestroyedCallback)
		s_GameObjectDestroyedCallback (go);
}
	
void GameObject::RegisterSetGONameCallback (SetGONameFunction* callback)
{
	s_SetGONameCallback = callback;
}

static int GetHighestGOComponentClassID ()
{
	static int highestGOComponentClassID = 0;
	if (highestGOComponentClassID != 0)
		return highestGOComponentClassID;

	vector<SInt32> classes;
	Object::FindAllDerivedClasses (ClassID (Component), &classes, false);
	for (int i=0;i<classes.size ();i++)
		highestGOComponentClassID = max<int> (highestGOComponentClassID, classes[i]);
	
	return highestGOComponentClassID;
}

void GameObject::RegisterMessageHandler (int classID, const MessageIdentifier& messageIdentifier,
										 MessagePtr message, int typeId)
{
	Assert(s_RegisteredMessageForwarders);
	s_RegisteredMessageForwarders->resize (max(classID, GetHighestGOComponentClassID ()) + 1);
	(*s_RegisteredMessageForwarders)[classID].RegisterMessageCallback (messageIdentifier.messageID, message, typeId);
}

void GameObject::RegisterAllMessagesHandler (int classID, MessagePtr message, CanHandleMessagePtr canHandleNotification)
{
	Assert(s_RegisteredMessageForwarders);
	s_RegisteredMessageForwarders->resize (max(classID, GetHighestGOComponentClassID ()) + 1);
	(*s_RegisteredMessageForwarders)[classID].RegisterAllMessagesCallback (message, canHandleNotification);
}

static void PropagateNotificationsToDerivedClasses (MessageForwarders& notifications)
{
	vector<SInt32> classes;
	Object::FindAllDerivedClasses (ClassID (Object), &classes, false);
	int highestClassID = 0;
	for (unsigned i=0;i<classes.size ();i++)
		highestClassID = max<int> (classes[i], highestClassID);

	notifications.resize (highestClassID + 1);
	
	for (int classID=0;classID<notifications.size ();classID++)
	{
		if (Object::ClassIDToRTTI (classID) == NULL)
			continue;
			
		int superClassID = Object::GetSuperClassID (classID);
		while (superClassID != ClassID (Object))
		{
			notifications[classID].AddBaseMessages (notifications[superClassID]);
			superClassID = Object::GetSuperClassID (superClassID);
		}
	}
}

void GameObject::InitializeMessageHandlers ()
{
	Assert(s_MessageHandler && s_RegisteredMessageForwarders);
	PropagateNotificationsToDerivedClasses (*s_RegisteredMessageForwarders);
	s_MessageHandler->Initialize (*s_RegisteredMessageForwarders);
	s_RegisteredMessageForwarders->clear ();
}

void GameObject::InitializeMessageIdentifiers ()
{
	Assert(s_MessageHandler == NULL);
	s_MessageHandler = UNITY_NEW(MessageHandler,kMemNewDelete);
	s_RegisteredMessageForwarders = UNITY_NEW(MessageForwarders,kMemNewDelete);
	GetMessageHandler ().InitializeMessageIdentifiers ();
}

void GameObject::InitializeClass ()
{
	GameObjectManager::StaticInitialize();
}

void GameObject::CleanupClass ()
{
	GameObjectManager::StaticDestroy();
	UNITY_DELETE(s_MessageHandler,kMemNewDelete);
	UNITY_DELETE(s_RegisteredMessageForwarders,kMemNewDelete);
}

bool CheckMessageDataType (int messageIdentifier, MessageData& data)
{
	return GameObject::GetMessageHandler ().MessageIDToParameter (messageIdentifier) == data.type;
}

void GameObject::SendMessageAny (const MessageIdentifier& messageIdentifier, MessageData& messageData)
{
	int messageID = messageIdentifier.messageID;
	AssertIf (messageIdentifier.messageID == -1);
	#if DEBUGMODE
	if (!CheckMessageDataType (messageID, messageData))
		AssertString ("The messageData sent has an incorrect type.");
	#endif

	for (int i=0;i<m_Component.size ();i++)
	{
		int classID = m_Component[i].first;
		if (s_MessageHandler->HasMessageCallback (classID, messageID))
		{
			Component& component = *m_Component[i].second;
			s_MessageHandler->HandleMessage (&component, classID, messageID, messageData);
		}
	}
}

bool GameObject::WillHandleMessage (const MessageIdentifier& messageIdentifier)
{
	int messageID = messageIdentifier.messageID;
	AssertIf (messageIdentifier.messageID == -1);

	for (Container::iterator i=m_Component.begin ();i != m_Component.end ();i++)
	{
		int classID = i->first;
		if (s_MessageHandler->HasMessageCallback (classID, messageID))
		{
			Component& component = *i->second;
			if (s_MessageHandler->WillHandleMessage (&component, classID, messageID))
				return true;
		}
	}
	return false;
}
 	
void GameObject::TransformParentHasChanged ()
{
	// Reactivate transform hieararchy, but only if it has been activated before,
	// otherwise we change activation order.
	if (m_IsActiveCached != -1)
		ActivateAwakeRecursively ();
}
	
void SendMessageDirect (Object& target, const MessageIdentifier& messageIdentifier, MessageData& messageData)
{
	int classID = target.GetClassID();
	if (GameObject::GetMessageHandler ().HasMessageCallback (classID, messageIdentifier.messageID))
	{
		GameObject::GetMessageHandler ().HandleMessage (&target, classID, messageIdentifier.messageID, messageData);
	}
}

MessageHandler& GameObject::GetMessageHandler ()
{
	Assert(s_MessageHandler);
	return *s_MessageHandler;
}


char const* Component::GetName () const
{
	if (m_GameObject)
		return m_GameObject->m_Name.c_str();
	else
		return GetClassName().c_str();
}

void Component::SetName (char const* name)
{
	if (m_GameObject)
		m_GameObject->SetName (name);
}

Component::Component (MemLabelId label, ObjectCreationMode mode) : Super(label, mode)
{
	m_GameObject = NULL;
}

Component::~Component ()
{
}

void Component::SendMessageAny (const MessageIdentifier& messageID, MessageData& messageData)
{
	GameObject* go = GetGameObjectPtr ();
	if (go)
		go->SendMessageAny (messageID, messageData);
}
	
void Component::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	#if SUPPORT_LOG_ORDER_TRACE
	if (IsActive() && RunningReproduction())
	{
		if (SUPPORT_LOG_ORDER_TRACE == 2)
		{
			LogString(Format("AwakeFromLoad %s (%s) [%d]", GetName(), GetClassName().c_str(), GetInstanceID()));
		}
		else
		{
			LogString(Format("AwakeFromLoad %s (%s)", GetName(), GetClassName().c_str()));
		}
	}
	#endif

	// Force load the game object. This is in order to prevent ImmediatePtrs not being dereferenced after loading.
	// Which can cause a crash in Resources.UnloadUnusedAssets()
	// Resources.Load used to store incorrect preload data which made this trigger.
	GameObject* dereferenceGameObject = m_GameObject;
	UNUSED(dereferenceGameObject);
}

template<class TransferFunction>
void Component::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	
	if (SerializePrefabIgnoreProperties(transfer))
		transfer.Transfer (m_GameObject, "m_GameObject", kHideInEditorMask | kStrongPPtrMask | kIgnoreWithInspectorUndoMask);
}

void Component::CheckConsistency ()
{
	Super::CheckConsistency ();
	GameObject* go = GetGameObjectPtr ();
	if (go)
	{
		for (int i = 0; i < go->GetComponentCount(); i++)
		{
			if (&go->GetComponentAtIndex(i) == this)
				return;
		}

		ErrorStringObject (Format("CheckConsistency: GameObject does not reference component %s. Fixing.", GetClassName().c_str()), go);
		go->AddComponentInternal (this);
	}

	// MonoBehaviours are allowed to exists without a game object
	if (IsDerivedFrom(ClassID(Behaviour)))
	{
		return;
	}

	#if UNITY_EDITOR
	if (m_GameObject == NULL)
	{
		GetCleanupManager ().MarkForDeletion (this, "GameObject pointer is invalid");
	}
	#endif
}

GameObjectManager* GameObjectManager::s_Instance = NULL;
void GameObjectManager::StaticInitialize()
{
	Assert(GameObjectManager::s_Instance == NULL);
	GameObjectManager::s_Instance = UNITY_NEW(GameObjectManager,kMemBaseObject);
}

void GameObjectManager::StaticDestroy()
{
	Assert(GameObjectManager::s_Instance);
	UNITY_DELETE(GameObjectManager::s_Instance,kMemBaseObject);
}

GameObjectManager& GetGameObjectManager()
{
	Assert(GameObjectManager::s_Instance);
	return *GameObjectManager::s_Instance;
}

IMPLEMENT_OBJECT_SERIALIZE (GameObject)
IMPLEMENT_OBJECT_SERIALIZE (Component)

IMPLEMENT_CLASS_HAS_INIT (GameObject)
IMPLEMENT_CLASS (Component)

INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED(GameObject)
INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED(Component)

}

// Hack to make register class work with name spaces. Optimally IMPLEMENT_CLASS / IMPLEMENT_OBJECT_SERIALIZE
// could be moved out of the namespace but that gives compile errors on gcc
void RegisterClass_Component () { Unity::RegisterClass_Component(); }
void RegisterClass_GameObject () { Unity::RegisterClass_GameObject(); }
