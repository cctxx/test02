#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK

#include "Runtime/GameCode/Behaviour.h"
#include "Configuration/UnityConfigure.h"
#include <deque>
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "NetworkViewID.h"
#include "NetworkEnums.h"
#include "External/RakNet/builds/include/BitStream.h"



struct MonoArray;
enum {
	kNoStateSynch = 0,
	kReliableDeltaCompressed = 1,
	kUnreliableBruteForce = 2
};

class NetworkView : public Behaviour
{
public:
	typedef std::set<Component*> Components;		// Declare the NetworkView container
	typedef std::vector<UInt8> PackState;
	
	REGISTER_DERIVED_CLASS   (NetworkView, Behaviour)
	DECLARE_OBJECT_SERIALIZE (NetworkView)

	static void InitializeClass ();
	static void CleanupClass ();

	NetworkView (MemLabelId label, ObjectCreationMode mode);
	// virtual ~NetworkView (); declared-by-macro

	void Update();
	void AwakeFromLoad (AwakeFromLoadMode mode);
	void Reset ();
	
	bool Pack(RakNet::BitStream &stream, PackState* writeStatePtr, UInt8* readData, int &readSize, int msgID);
	void SendWithInitialState(std::vector<SystemAddress> &initAddresses, std::vector<SystemAddress> &normalAddresses, unsigned char msgID);
	void Send(SystemAddress systemAddress, bool broadcast);
	void SendToAllButOwner();

	NetworkViewID GetViewID();

	// Assign the given view ID to this view. Add the player address information
	// to the view as appropriate (depening on owner/peer type)
	void SetViewID(NetworkViewID viewID);

	// Call the given user scripted RPC function. The mode defines if it should be 
	// buffered on the server and how it should be relayed. The array stores all the user
	// defined variables.
	void RPCCall (const std::string& function, int mode, MonoArray* args);

	// Call the given user scripted RPC function. The mode defines if it should be 
	// buffered on the server and how it should be relayed. The array stores all the user
	// defined variables.
	void RPCCallSpecificTarget (const std::string &function, NetworkPlayer target, MonoArray* args);

	SystemAddress GetOwnerAddress();
	
	unsigned GetGroup() { return m_Group; }
	void SetGroup (unsigned group);
	
	void SetStateReliability(int reliability);
	int GetStateReliability();

	Unity::Component* GetObserved ();
	void SetObserved (Unity::Component* component);

	void Unpack (RakNet::BitStream& stream, NetworkMessageInfo& msgData, int msgType);
	bool Pack (RakNet::BitStream& stream);
	
	int GetStateSynchronization () { return m_StateSynchronization; }
	void SetStateSynchronization (int sync);

	void SetInitState(int index, bool isSent);
	void GrowInitState();
	bool GetInitStateStatus(int index);
	void ClearInitStateAndOwner();
	
	bool SetPlayerScope(NetworkPlayer playerIndex, bool relevancy);
	void SetScope(unsigned int initIndex, bool relevancy);
	bool CheckScope(int initIndex);

private:
	void SetupSceneViewID ();
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	
	NetworkViewID	m_ViewID;
	PPtr<Unity::Component> m_Observed;
	SystemAddress	   m_OwnerAddress;	// The address of the player which owns this object. Used by server when relaying to other players.
	int				   m_Group;
	int                m_StateSynchronization;	///< enum { Off = 0, Reliable Delta Compressed = 1, Unreliable = 2 }

	// Pack state for delta compression
	// We need seperate pack / unpack states because the server can receive & send at the same time.
	PackState          m_LastPackState;
	PackState          m_LastUnpackState;
	dynamic_bitset     m_InitState;
	dynamic_bitset     m_Scope;

	ListNode<NetworkView> m_Node;
	ListNode<NetworkView> m_AllNode;
	bool              m_HasReceivedInitialState;
};

#endif
