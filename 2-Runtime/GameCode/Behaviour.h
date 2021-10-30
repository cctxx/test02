#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Modules/ExportModules.h"

class EXPORT_COREMODULE Behaviour : public Unity::Component
{
  public: 
	
	REGISTER_DERIVED_ABSTRACT_CLASS (Behaviour, Component)
	DECLARE_OBJECT_SERIALIZE (Behaviour)	
	Behaviour (MemLabelId label, ObjectCreationMode mode) : Super(label, mode) { m_Enabled = true; m_IsAdded = false; }

	void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	virtual void Update () {}
	virtual void FixedUpdate () {}
	virtual void LateUpdate () {}

	void Deactivate (DeactivateOperation operation);
	/// Enable or disable updates of this behaviour
	virtual void SetEnabled (bool enab);
	bool GetEnabled () const { return m_Enabled; }
	
	bool IsAddedToManager () const { return m_IsAdded; }
	
	#if UNITY_EDITOR
	void SetEnabledNoDirty (bool enab);
	virtual bool ShouldDisplayEnabled () { return true; }
	#endif
	
	
	static void InitializeClass ();
	static void CleanupClass ();
	
//	protected:
	

	/// Override this to add the behaviour not to BehaviourManager but some other Manager
	/// You should NOT call Super.
	/// This is called when the behaviour has become enabled and its game object is disabled
	/// You can rely on that AddToManager is only called once and will always be balanced out by RemoveFromManager before it is destroyed.
	virtual void AddToManager () = 0;
	virtual void RemoveFromManager () = 0;

	
  private:
	void UpdateEnabledState (bool active);

	///@todo DO THIS PROPERLY. MORE SPACE EFFICIENT
	UInt8 m_Enabled;
	UInt8 m_IsAdded;
};

typedef ListNode<Behaviour> BehaviourListNode;

class EXPORT_COREMODULE BaseBehaviourManager
{
	public:
	virtual ~BaseBehaviourManager ();
	
	virtual void Update() = 0;
	
	void AddBehaviour (BehaviourListNode& node, int queue);
	void RemoveBehaviour (BehaviourListNode& node);
	
	protected:

	template<typename T> void CommonUpdate ();
	void IntegrateLists();

	typedef List<BehaviourListNode> BehaviourList;

	// Need to use map instead of vector_map here, because it can change during iteration
	// (Behaviours added in update calls).
	typedef std::map<int, std::pair<BehaviourList*,BehaviourList*> > Lists;
	Lists m_Lists;
};

EXPORT_COREMODULE BaseBehaviourManager& GetBehaviourManager ();
EXPORT_COREMODULE BaseBehaviourManager& GetFixedBehaviourManager ();
EXPORT_COREMODULE BaseBehaviourManager& GetLateBehaviourManager ();
EXPORT_COREMODULE BaseBehaviourManager& GetUpdateManager ();

#endif
