#include "UnityPrefix.h"
#include "Behaviour.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Misc/ReproductionLog.h"
#include "Runtime/Threads/Thread.h"

Behaviour::~Behaviour ()
{}

void Behaviour::SetEnabled (bool enab)
{
	if ((bool)m_Enabled == enab)
		return;
	m_Enabled = enab;
	UpdateEnabledState (IsActive ());
	SetDirty ();
}

#if UNITY_EDITOR
void Behaviour::SetEnabledNoDirty (bool enab)
{
	if ((bool)m_Enabled == enab)
		return;
	m_Enabled = enab;
	UpdateEnabledState (IsActive ());
}
#endif

void Behaviour::UpdateEnabledState (bool active)
{
	bool shouldBeAdded = active && m_Enabled;
	if (shouldBeAdded == (bool)m_IsAdded)
		return;

	// Set IsAdded flag before adding/removing from manager. Otherwise if we get enabled update
	// from inside of AddToManager/RemoveFromManager, we'll early out in the check above because
	// flag is not set yet!
	if (shouldBeAdded)	
	{
		m_IsAdded = true;
		AddToManager ();
	}
	else
	{
		m_IsAdded = false;
		RemoveFromManager ();
	}
}

void Behaviour::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	UpdateEnabledState (IsActive ());
}

void Behaviour::Deactivate (DeactivateOperation operation)
{
	UpdateEnabledState (false);
	Super::Deactivate (operation);
}

IMPLEMENT_OBJECT_SERIALIZE (Behaviour)
template<class TransferFunc>
void Behaviour::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_Enabled, "m_Enabled", kHideInEditorMask | kEditorDisplaysCheckBoxMask);
	transfer.Align();
}


// BEHAVIOURMANAGER
// --------------------------------------------------------------------------


BaseBehaviourManager::~BaseBehaviourManager ()
{
	for (Lists::iterator i=m_Lists.begin();i != m_Lists.end();i++)
	{
		Lists::mapped_type& listPair = i->second;

		Assert(listPair.first == NULL || listPair.first->empty());
		delete listPair.first;

		Assert(listPair.second == NULL || listPair.second->empty());
		delete listPair.second;
	}
	m_Lists.clear();
}

void BaseBehaviourManager::AddBehaviour (BehaviourListNode& p, int queueIndex)
{
	ASSERT_RUNNING_ON_MAIN_THREAD

	Lists::mapped_type& listPair = m_Lists[queueIndex];
	if (listPair.first == NULL)
	{
		Assert(listPair.second == NULL);
		listPair.first = new BehaviourList();
		listPair.second = new BehaviourList();
	}

	listPair.second->push_back (p);
}

void BaseBehaviourManager::RemoveBehaviour (BehaviourListNode& p)
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	p.RemoveFromList();
}

void BaseBehaviourManager::IntegrateLists()
{
	for (Lists::iterator i=m_Lists.begin();i!=m_Lists.end();i++)
	{
		Lists::mapped_type& listPair = (*i).second;

		listPair.first->append(*listPair.second);
		Assert(listPair.second->empty());
	}
}

template<typename T>
void BaseBehaviourManager::CommonUpdate ()
{
	IntegrateLists();

	for (Lists::iterator i=m_Lists.begin();i!=m_Lists.end();i++)
	{
		Lists::mapped_type& listPair = (*i).second;

		SafeIterator<BehaviourList> iterator (*listPair.first);
		while (iterator.Next())
		{
			Behaviour& behaviour = **iterator;
			
			#if SUPPORT_LOG_ORDER_TRACE
			if (RunningReproduction())
			{
				if (SUPPORT_LOG_ORDER_TRACE == 2)
				{
					LogString(Format("UpdateBehaviour %s (%s) [%d]", behaviour.GetName(), behaviour.GetClassName().c_str(), behaviour.GetInstanceID()));
				}
				else
				{
					LogString(Format("UpdateBehaviour %s (%s)", behaviour.GetName(), behaviour.GetClassName().c_str()));
				}
			}			
			#endif
			
			Assert(behaviour.IsAddedToManager ());
			
			#if !UNITY_RELEASE
			PPtr<Behaviour> behaviourPPtr (&behaviour);
			#endif
			T::UpdateBehaviour(behaviour);
			
			// Behaviour might get destroyed in the mean time, so we have to check if the object still exists first
			#if !UNITY_RELEASE
			AssertIf (behaviourPPtr.IsValid() && (behaviour.GetEnabled () && behaviour.IsActive ()) != behaviour.IsAddedToManager ());
			#endif
		}
	}
}	

class BehaviourManager : public BaseBehaviourManager
{
  public:
		
	virtual void Update()
	{
		BaseBehaviourManager::CommonUpdate<BehaviourManager>();
	}

	static inline void UpdateBehaviour(Behaviour& beh)
	{
		beh.Update();
	}
};

class FixedBehaviourManager : public BaseBehaviourManager {
  public:


	virtual void Update()
	{
		BaseBehaviourManager::CommonUpdate<FixedBehaviourManager>();
	}

	static inline void UpdateBehaviour(Behaviour& beh)
	{
		beh.FixedUpdate();
	}
};

class LateBehaviourManager : public BaseBehaviourManager
{
  public:
	
	virtual void Update()
	{
		BaseBehaviourManager::CommonUpdate<LateBehaviourManager>();
	}

	static inline void UpdateBehaviour(Behaviour& beh)
	{
		beh.LateUpdate();
	}
};

class UpdateManager : public BaseBehaviourManager {
  public:

	
	virtual void Update()
	{
		BaseBehaviourManager::CommonUpdate<BehaviourManager>();
	}

	static inline void UpdateBehaviour(Behaviour& beh)
	{
		beh.Update();
	}
};



#define GET_BEHAVIOUR_MANAGER(x) \
	x* s_instance##x; \
	BaseBehaviourManager& Get##x () { return reinterpret_cast<BaseBehaviourManager&> (*s_instance##x); } \
	void CreateInstance##x() { s_instance##x = new x; } \
	void ReleaseInstance##x() { delete s_instance##x; }
 

GET_BEHAVIOUR_MANAGER(BehaviourManager)
GET_BEHAVIOUR_MANAGER(FixedBehaviourManager)
GET_BEHAVIOUR_MANAGER(LateBehaviourManager)
GET_BEHAVIOUR_MANAGER(UpdateManager)

void Behaviour::InitializeClass ()
{
	CreateInstanceBehaviourManager();
	CreateInstanceFixedBehaviourManager();
	CreateInstanceLateBehaviourManager();
	CreateInstanceUpdateManager();
}

void Behaviour::CleanupClass ()
{
	ReleaseInstanceBehaviourManager();
	ReleaseInstanceFixedBehaviourManager();
	ReleaseInstanceLateBehaviourManager();
	ReleaseInstanceUpdateManager();
	
}

IMPLEMENT_CLASS_HAS_INIT (Behaviour)
INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED(Behaviour)
