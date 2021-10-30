#include "UnityPrefix.h"
#include "NetworkView.h"

#if ENABLE_NETWORK
#include "NetworkManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "PackStateSpecialized.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Utilities/Utility.h"
#include "PackMonoRPC.h"
#include "BitStreamPacker.h"
#include "External/RakNet/builds/include/RakPeerInterface.h"

NetworkView::NetworkView (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_Node (this)
,	m_AllNode(this)
{
	m_OwnerAddress.binaryAddress = 0;
	m_StateSynchronization = kReliableDeltaCompressed;
	m_Group = 0;
	m_InitState.clear();
	m_Scope.clear();
	m_HasReceivedInitialState = false;
}

void NetworkView::Update()
{
}

// This static RPC function executes the user scripted function by taking name and input from rpcParameters.
// Name contains the function name called and input contains the parameters for the function (in a BitStream).
static void NetworkViewRPCCallScript (RPCParameters *rpcParameters);
static void NetworkViewRPCCallScript (RPCParameters *rpcParameters)
{
	NetworkManager& nm = GetNetworkManager ();
	NetworkViewID viewID;
	UInt8 mode = 0;
	RakNet::BitStream input (rpcParameters->input, BITS_TO_BYTES(rpcParameters->numberOfBitsOfData), false);

	viewID.Read(input);
	input.ReadBits(&mode, kRPCModeNbBits);

	NetworkLog(NULL, "Received RPC '%s'- mode %d - sender %s", rpcParameters->functionName, GetTargetMode(mode), rpcParameters->sender.ToString());
	// Specific target handling.
	// We might have to reroute the RPC call to another player.
	if (GetTargetMode(mode) == kSpecificTarget)
	{
		bool relaySpecificTarget = false;
		input.Read(relaySpecificTarget);

		// We have to reroute the RPC call to another player
		if (relaySpecificTarget)
		{
			NetworkPlayer relayTarget;
			input.Read(relayTarget);

			NetworkLog(NULL, "Relay RPC to specifc target - player ID %s", relayTarget);

			RakNet::BitStream rerouted;
			rerouted.Write(viewID);
			rerouted.WriteBits(&mode, kRPCModeNbBits);
			rerouted.Write0();

			int unreadBits = input.GetNumberOfUnreadBits();
			UInt8* data;
			ALLOC_TEMP(data, UInt8, BITS_TO_BYTES (unreadBits));
			input.ReadBits(data, unreadBits, false);
			rerouted.WriteBits(data, unreadBits, false);

			nm.PerformRPCRelaySpecific(rpcParameters->functionName, &rerouted, relayTarget);
			return;
		}
	}

	// Get the view and observer which should receive the rpc call
	NetworkView* view = nm.ViewIDToNetworkView(viewID);
	int group = 0;
	if (view != NULL)
	{
		group  = view->GetGroup();
	}
	else
	{
		NetworkWarning(NULL, "Could't invoke RPC function '%s' because the networkView '%s' doesn't exist", rpcParameters->functionName, viewID.ToString().c_str());
		return;
	}

	// Unpack and invoke rpc call
	if (nm.MayReceiveFromPlayer(rpcParameters->sender, group))
	{
		nm.PeformRPCRelayAll(rpcParameters->functionName, mode, viewID, group, rpcParameters->remoteTimestamp, rpcParameters->sender, input);
		UnpackAndInvokeRPCMethod (view->GetGameObject(), rpcParameters->functionName, input, rpcParameters->sender, view->GetViewID(), rpcParameters->remoteTimestamp, view);
	}
	else
	{
		NetworkInfo (NULL, "RPC %s is ignored since the group of the network view is disabled.");
		return;
	}
}

void NetworkView::RPCCall (const std::string &function, int inMode, MonoArray* args)
{
	NetworkManager& nm = GetNetworkManager();
	if (!nm.IsConnected())
	{
		NetworkError(NULL, "Can't send RPC function since no connection was started.");
		return;
	}

	if (!nm.MaySend(m_Group))
	{
		NetworkInfo (NULL, "RPC %s is ignored since the group of its network view is disabled.");
		return;
	}

	UInt8 mode = inMode;

	RakNet::BitStream parameters;
	m_ViewID.Write(parameters);
	parameters.WriteBits(&mode, kRPCModeNbBits);

	int parameterOffset = parameters.GetWriteOffset();

	if (PackRPCParameters (GetGameObject(), function.c_str(), parameters, args, this))
	{
		// Call ourselves immediately
		if (GetTargetMode(mode) == kAll)
		{
			parameters.SetReadOffset(parameterOffset);
			UnpackAndInvokeRPCMethod (GetGameObject(), function.c_str(), parameters, nm.GetPlayerAddress(), GetViewID(), nm.GetTimestamp(), this);
		}

		// Send the RPC function via network
		nm.PerformRPC(function, mode, parameters, m_ViewID, m_Group);
	}
	else
	{
		//NetworkError(NULL, "Could't relay remote function message to Unity object");
	}
}

void NetworkView::RPCCallSpecificTarget (const std::string &function, NetworkPlayer target, MonoArray* args)
{
	NetworkManager& nm = GetNetworkManager();
	if (!nm.IsConnected())
	{
		NetworkError(NULL, "Can't send RPC function since no connection was started.");
		return;
	}

	if (!nm.MaySend(m_Group))
	{
		NetworkInfo (NULL, "RPC %s is ignored since the group of its network view is disabled.");
		return;
	}

	UInt8 mode = kSpecificTarget;

	RakNet::BitStream parameters;
	m_ViewID.Write(parameters);
	parameters.WriteBits(&mode, kRPCModeNbBits);

	PlayerTable *targetPlayer = GetNetworkManager().GetPlayerEntry(target);;
	if (targetPlayer != NULL)
	{
		parameters.Write0();
	}
	else if (GetNetworkManager().IsClient())
	{
		parameters.Write1();
		parameters.Write(target);
		targetPlayer = GetNetworkManager().GetPlayerEntry(0);
	}
	else
	{
		NetworkError(this, "Can't send RPC function because the target is not connected to the server.");
		return;
	}

	if (PackRPCParameters (GetGameObject(), function.c_str(), parameters, args, this))
	{
		// Send the rpc function via network
		nm.PerformRPCSpecificTarget(const_cast<char*> (function.c_str()), targetPlayer, parameters, m_Group);
	}
	else
	{
		//NetworkError(NULL, "Could't relay remote function message to Unity object");
	}
}


// Add this network view to the network manager so it will be synchronized over the network.
void NetworkView::AddToManager ()
{
	if (m_StateSynchronization != kNoStateSynch)
		GetNetworkManager ().AddNetworkView (m_Node);
	else
		GetNetworkManager ().AddNonSyncNetworkView (m_Node);
}

void NetworkView::RemoveFromManager ()
{
	m_Node.RemoveFromList();
}

void NetworkView::SetupSceneViewID ()
{
	// When in edit mode ensure that all objects get a view id.
	// In play mode we maintain view id's
	if (!IsWorldPlaying())
	{
		// No view id assigned, allocate new one
		if (m_ViewID == NetworkViewID())
		{
			if (GetNetworkManager().GetDebugLevel() >= kInformational) LogString("Allocating scene view ID to new object");
			m_ViewID = GetNetworkManager ().AllocateSceneViewID();
		}
		else
		{
			m_ViewID = GetNetworkManager ().ValidateSceneViewID(this, m_ViewID);
		}
	}
}

void NetworkView::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);

	GetNetworkManager().AddAllNetworkView(m_AllNode);

	if (IsActive())
		SetupSceneViewID ();

	// When loading a new scene replace level prefix of all objects that are being loaded!
	if (mode & kDidLoadFromDisk)
	{
		if (m_ViewID.IsSceneID())
			m_ViewID.ReplaceLevelPrefix(GetNetworkManager().GetLevelPrefix());
	}

	if (IsPrefabParent())
		m_ViewID.SetAllocatedID(0);
}

void NetworkView::SetObserved (Unity::Component* component)
{
	m_Observed = component;
	SetDirty();
}

Unity::Component* NetworkView::GetObserved ()
{
	return m_Observed;
}

void NetworkView::Unpack (RakNet::BitStream& bitStream, NetworkMessageInfo& info, int msgType)
{
	// Initial can always be received, but make sure that the last unpack state is clear
	if (msgType == ID_STATE_INITIAL)
	{
		m_HasReceivedInitialState = true;
		m_LastUnpackState.clear();
	}
	// Updates can only be received after some initial state was already sent! Except when delta compression is not being used.
	else
	{
		if (!m_HasReceivedInitialState && m_StateSynchronization == kReliableDeltaCompressed)
		{
			NetworkError(NULL, "Received state update for view ID %s but no initial state has ever been sent. Ignoring message.\n", m_ViewID.ToString().c_str());
			return;
		}
	}

	// Calculate the last state to perform delta compression against!
	PackState  writeState;
	PackState*  writeStatePtr = NULL;
	UInt8* readData = NULL;
	int readSize = 0;
	if (m_StateSynchronization == kReliableDeltaCompressed)
	{
		writeState.resize(m_LastUnpackState.size());
		writeStatePtr = &writeState;
		readData = &m_LastUnpackState[0];
		readSize = m_LastUnpackState.size();
	}

	// Pack state
	BitstreamPacker packer (bitStream, writeStatePtr, readData, readSize, true);

	Unity::Component* observed = GetObserved ();
	Rigidbody* body = dynamic_pptr_cast<Rigidbody*> (observed);
	Transform* transform = dynamic_pptr_cast<Transform*> (observed);
	Animation* animation = dynamic_pptr_cast<Animation*> (observed);
	MonoBehaviour* mono = dynamic_pptr_cast<MonoBehaviour*> (observed);

	if (body)
		SerializeRigidbody(*body, packer);
	else if (transform)
		UnpackTransform(*transform, packer);
	else if (animation)
		SerializeAnimation(*animation, packer);
	else if (mono)
		SerializeMono(*mono, packer, info);
	else if (observed)
	{
		ErrorStringObject ("Network View synchronization error. Received packet but the observed class is not supported as a synchronization type", this);
	}
	else
	{
		LogStringObject("Receiving state for an object whose network view exists but the observed object no longer exists", this);
	}

	NetworkLog(NULL, "Received state update for view ID %s\n", m_ViewID.ToString().c_str());

	m_LastUnpackState.swap(writeState);
}

bool NetworkView::Pack(RakNet::BitStream &stream, PackState* writeStatePtr, UInt8* readData, int &readSize, int msgID)
{
	// Pack the state with the appropriate specialized packer
	Unity::Component* observed = GetObserved ();
	Rigidbody* body = dynamic_pptr_cast<Rigidbody*> (observed);
	Transform* transform = dynamic_pptr_cast<Transform*> (observed);
	Animation* animation = dynamic_pptr_cast<Animation*> (observed);
	MonoBehaviour* mono = dynamic_pptr_cast<MonoBehaviour*> (observed);

	bool doSend = false;
	stream.Reset();

	if (GetNetworkManager().GetUseProxy() && GetNetworkManager().IsClient())
	{
		stream.Write((unsigned char) ID_PROXY_CLIENT_MESSAGE);
	}
	// For now always include timestamp
	bool useTimeStamp = true;
	RakNetTime timeStamp = GetNetworkManager().GetTimestamp();
	if (useTimeStamp)
	{
		stream.Write((unsigned char)ID_TIMESTAMP);
		stream.Write(timeStamp);
	}

	// Msg type
	stream.Write((unsigned char)msgID);
	// View ID
	m_ViewID.Write(stream);

	// Pack data
	BitstreamPacker packer (stream, writeStatePtr, readData, readSize, false);
	NetworkMessageInfo info;
	info.timestamp = -1.0;
	info.sender = -1;
	info.viewID = GetViewID();

	if (body)
		doSend |= SerializeRigidbody(*body, packer);
	else if (transform)
		doSend |= PackTransform(*transform, packer);
	else if (animation)
		doSend |= SerializeAnimation(*animation, packer);
	else if (mono)
		doSend |= SerializeMono(*mono, packer, info);
	else if (observed)
	{
		NetworkError (this, "Network View synchronization of  %s is not supported. Pack the state manually from a script.", this);
		return false;
	}

	return doSend;
}

inline bool MayReceiveGroup (PlayerTable& table, int group)
{
	return (table.mayReceiveGroups & (1<<group)) != 0;
}

// When broadcast is enabled, every connected peer gets the message except the one given in the system address
// TODO: Optimize code for initial/update proxied clients, they could go into seperate list(s) and get different streams (no memcpy)
//       Right now real stream is memcpd'd into a stream with proxy header for each client which is proxied
void NetworkView::Send (SystemAddress systemAddress, bool broadcast)
{
	// Calculate the last state to perform delta compression against!
	PackState  writeState;
	PackState*  writeStatePtr = NULL;
	UInt8* readData = NULL;
	int readSize = 0;

	typedef std::vector<PlayerTable> Addresses;
	Addresses initialStateAddresses;
	Addresses updateStateAddresses;

	// If doing broadcast, check if there is any intial state which needs to be sent
	std::vector<PlayerTable> players = GetNetworkManager().GetPlayerAddresses();
	updateStateAddresses.reserve(players.size());

	// Find the players which need update or initial state
	for (int i = 0; i != players.size(); i++)
	{
		// Calculate if we should include address based:
		// * broadcast flag and ignore address
		// * single cast must match address
		bool include =  broadcast && systemAddress != players[i].playerAddress;
		include     |= !broadcast && systemAddress == players[i].playerAddress;
		bool maySendToPlayer = GetNetworkManager().MaySendToPlayer(players[i].playerAddress, m_Group);

		if (((include && maySendToPlayer) || GetNetworkManager().IsClient()) && CheckScope(players[i].initIndex) )
		{
			bool hasInitialState = GetInitStateStatus(players[i].initIndex);
			if (!hasInitialState)
			{
				initialStateAddresses.push_back(players[i]);
				SetInitState(players[i].initIndex, true);
			}
			else
			{
				updateStateAddresses.push_back(players[i]);
			}
		}
	}

	RakPeerInterface* peer = GetNetworkManager().GetPeer();

	// Send update state data
	if (!updateStateAddresses.empty())
	{
		if (m_StateSynchronization == kReliableDeltaCompressed)
		{
			writeState.clear();
			writeState.reserve(m_LastPackState.size());
			writeStatePtr = &writeState;
			readData = &m_LastPackState[0];
			readSize = m_LastPackState.size();
		}
		else
		{
			writeStatePtr = NULL;
			readData = NULL;
			readSize = 0;
		}

		RakNet::BitStream stream;
		RakNet::BitStream relayStream;
		bool doSend = Pack(stream, writeStatePtr, readData, readSize, ID_STATE_UPDATE);
		if (doSend)
		{
			PacketReliability reliability;
			if (m_StateSynchronization == kReliableDeltaCompressed)
				reliability = RELIABLE_ORDERED;
			else
				reliability = UNRELIABLE_SEQUENCED;

			for (int i=0;i<updateStateAddresses.size();i++)
			{
				if (updateStateAddresses[i].relayed == true)
				{
					relayStream.Reset();
					relayStream.Write((MessageID) ID_PROXY_SERVER_MESSAGE);
					relayStream.Write(updateStateAddresses[i].playerAddress);
					relayStream.Write((char*)stream.GetData(), stream.GetNumberOfBytesUsed());
					if (!peer->Send (&relayStream, (PacketPriority)HIGH_PRIORITY, reliability, kDefaultChannel, GetNetworkManager().GetProxyAddress(), false))
						NetworkError (this, "Failed to send relayed state update");

					NetworkLog(this, "Sending state update relay message through proxy, destination is %s", updateStateAddresses[i].playerAddress.ToString());
				}
				else
				{
					if (!peer->Send (&stream, (PacketPriority)HIGH_PRIORITY, reliability, kDefaultChannel, updateStateAddresses[i].playerAddress, false))
						NetworkError (this, "Failed to send state update");
				}
			}

			NetworkLog(this, "Sending generic state update, broadcast %s, view ID '%s'\n", (broadcast)?"on":"off", m_ViewID.ToString().c_str());

			m_LastPackState.swap(writeState);
		}
	}

	// Send initial state data
	if (!initialStateAddresses.empty())
	{
		if (m_StateSynchronization == kReliableDeltaCompressed)
		{
			writeState.clear();
			writeState.reserve(m_LastPackState.size());
			writeStatePtr = &writeState;
			readData = NULL;
			readSize = 0;
		}
		else
		{
			writeStatePtr = NULL;
			readData = NULL;
			readSize = 0;
		}

		RakNet::BitStream stream;
		RakNet::BitStream relayStream;

		Pack(stream, writeStatePtr, readData, readSize, ID_STATE_INITIAL);

		for (int i=0;i<initialStateAddresses.size();i++)
		{
			if (initialStateAddresses[i].relayed == true)
			{
				relayStream.Reset();
				relayStream.Write((MessageID) ID_PROXY_SERVER_MESSAGE);
				relayStream.Write(initialStateAddresses[i].playerAddress);
				relayStream.Write((char*)stream.GetData(), stream.GetNumberOfBytesUsed());
				if (!peer->Send (&relayStream, (PacketPriority)HIGH_PRIORITY, (PacketReliability)RELIABLE_ORDERED, kDefaultChannel, GetNetworkManager().GetProxyAddress(), false))
					NetworkError (this, "Failed to send relayed initial update");

				NetworkLog(NULL, "Sending initial state relay message through proxy, destination is %s", initialStateAddresses[i].playerAddress.ToString());
			}
			else
			{
				if (!peer->Send (&stream, (PacketPriority)HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, initialStateAddresses[i].playerAddress, false))
					NetworkError (this, "Failed to send initial update");
			}
		}

		NetworkLog(this, "Sending generic initial state update, broadcast %s, view ID '%s'\n", (broadcast)?"on":"off", m_ViewID.ToString().c_str());

		m_LastPackState.swap(writeState);
	}

}

void NetworkView::SendToAllButOwner()
{
	Send(m_OwnerAddress, true);
}

NetworkViewID NetworkView::GetViewID()
{
	return m_ViewID;
}

void NetworkView::SetViewID(NetworkViewID viewID)
{
	NetworkManager& nm = GetNetworkManager();
	NetworkLog(NULL, "Assigning a view ID: old view ID '%s', new view ID '%s'\n", m_ViewID.ToString().c_str(), viewID.ToString().c_str());

	// If this viewID does not belong to my pool of IDs (i.e. another player)
	if ( nm.WasViewIdAllocatedByMe(viewID) )
	{
		m_OwnerAddress = nm.GetPlayerAddress();
	}
	// Since this is our own view object, send our playerId address to server
	else
	{
		// If we are a server, look up from player address table
		if (nm.IsServer())
		{
			NetworkPlayer player = nm.GetNetworkViewIDOwner(viewID);
			m_OwnerAddress = nm.GetSystemAddressFromIndex(player);
		}
		// If we are a client, we default to server owning, since we don't know the players anyway.
		else
		{
			m_OwnerAddress.binaryAddress = 0;
		}
	}

	// Make sure all current players get added into the scope of this network view
	m_Scope.resize(nm.GetInitIndexSize(), true);

	m_ViewID = viewID;
}

void NetworkView::SetGroup(unsigned group)
{
	if (group < kMaxGroups)
	{
		m_Group = group;
	}
	else
	{
		ErrorString("Groups must be between 0 and 31.");
	}
}

void NetworkView::Reset ()
{
	Super::Reset();

	if (!m_Observed && GetGameObjectPtr())
		m_Observed = QueryComponent(Transform);
}

void NetworkView::SetStateSynchronization (int sync)
{
	m_StateSynchronization = sync;
	SetDirty();
}

void NetworkView::SetInitState(int index, bool isSent)
{
	if  (index < m_InitState.size())
	{
		m_InitState[index] = isSent;
		NetworkInfo(NULL, "Initial state being sent to index %d", index);
	}
	else
	{
		if (isSent)
		{
			m_InitState.resize(index + 1, false);
			m_InitState[index] = isSent;
		}
	}
}

bool NetworkView::GetInitStateStatus(int index)
{
	if (index < m_InitState.size())
	{
		return m_InitState[index];
	}
	else
	{
		return false;
	}
}

void NetworkView::ClearInitStateAndOwner()
{
	m_InitState.clear();
	m_OwnerAddress.binaryAddress = 0;
}

// Get initIndex for player and set/unset according to bool
bool NetworkView::SetPlayerScope(NetworkPlayer playerIndex, bool relevancy)
{
	std::vector<PlayerTable> players = GetNetworkManager().GetPlayerAddresses();
	unsigned int initIndex = 0xFFFFFFFF;

	// Find the players which need update or initial state
	for (int i = 0; i != players.size(); i++)
	{
		if (playerIndex == players[i].playerIndex)
		{
			initIndex = players[i].initIndex;
			break;
		}
	}

	if (initIndex != 0xFFFFFFFF)
	{
		SetScope(initIndex, relevancy);
		return true;
	}
	else
	{
		NetworkError(NULL, "Player index %d not found when setting scope in network view %s", playerIndex, m_ViewID.ToString().c_str());
		return false;
	}
}

void NetworkView::SetScope(unsigned int initIndex, bool relevancy)
{
		if  (initIndex < m_Scope.size())
		{
			// Unset means this is in scope (so if relevancy==true => scope==0)
			m_Scope[initIndex] = relevancy;
			NetworkInfo(NULL, "Scope index %d is now %s scope for %s", initIndex, relevancy?"in":"out of", m_ViewID.ToString().c_str());
		}
		else
		{
			m_Scope.resize(initIndex + 1, false);
			m_Scope[initIndex] = relevancy;
			NetworkInfo(NULL, "New scope index %d is now %s scope for %s", initIndex, relevancy?"in":"out of", m_ViewID.ToString().c_str());
		}
}

// Check if given index is in this network views scope
bool NetworkView::CheckScope(int initIndex)
{
	if (initIndex < m_Scope.size())
	{
		return m_Scope[initIndex];
	}
	else
	{
		// Scope does not exist, create it with default value
		SetScope(initIndex, true);
		return true;
	}
}

SystemAddress NetworkView::GetOwnerAddress ()
{
	return m_OwnerAddress;
}

NetworkView::~NetworkView ()
{
}



template<class TransferFunc>
void NetworkView::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);

	TRANSFER(m_StateSynchronization);
	TRANSFER(m_Observed);
	transfer.Transfer(m_ViewID, "m_ViewID", kNotEditableMask);
}

void RegisterRPC (const char* name)
{
	GetNetworkManager().RegisterRPC(name, NetworkViewRPCCallScript);
}

void NetworkView::InitializeClass ()
{
	RegisterMonoRPC(RegisterRPC);
}

void NetworkView::CleanupClass ()
{
}


IMPLEMENT_CLASS_HAS_INIT (NetworkView)
IMPLEMENT_OBJECT_SERIALIZE (NetworkView)

#endif // ENABLE_NETWORK
