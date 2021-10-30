#include "UnityPrefix.h"
#include "NetworkManager.h"

#if !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_EDITOR
// TODO: what's a sensible default for pepper? web player?
NetworkReachability GetInternetReachability ()
{
	return ReachableViaLocalAreaNetwork;
}
#endif

#if ENABLE_NETWORK
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Profiler/Profiler.h"

#include "PackMonoRPC.h"
#include "MasterServerInterface.h"
#include "External/RakNet/builds/include/RakNetworkFactory.h"
#include "External/RakNet/builds/include/RakPeerInterface.h"
#include "External/RakNet/builds/include/RakNetStatistics.h"
#include "External/RakNet/builds/include/RakSleep.h"
#include "External/RakNet/builds/include/SocketLayer.h"
#include "Runtime/Input/InputManager.h"
#include "BitStreamPacker.h"
#include "Runtime/GameCode/CloneObject.h"
#include "Runtime/Utilities/Utility.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#endif

#define PACKET_LOGGER 0
#if PACKET_LOGGER
#include "External/RakNet/builds/include/PacketLogger.h"
PacketLogger messageHandler;
PacketLogger messageHandler2;
#endif

namespace {
	const int kFacilitatorPort = 50005;
	const int kConnectionTesterPort = 10737;
	const int kProxyPort = 10746;
}

/* 
 * TODO: When players can take ownership of another players instantiated object then
 * the Destory and RemoveRPC functions will no longer work. They remove 
 * based on player ID/prefix
 *
 * TODO: When sending timestamp from client to other clients the timestamp should be the timestamp from the client it was originally sent from not from the server.
 * when it is forwarding the RPC.
 *
 * TODO: optimize sending of RPCs to only include timestamp if the receiver uses it via NetworkMessageInfo
 */


using namespace std;

NetworkManager::NetworkManager(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Peer = RakNetworkFactory::GetRakPeerInterface();
	m_DebugLevel = kImportantErrors;
	m_PeerType = kDisconnected;
	m_Sendrate = 15.0F;
	m_MessageQueueRunning = true;
	
	m_Peer->RegisterAsRemoteProcedureCall("__RPCNetworkInstantiate", RPCNetworkInstantiate);
	m_Peer->RegisterAsRemoteProcedureCall("__RPCReceiveViewIDBatch", RPCReceiveViewIDBatch);
	m_Peer->RegisterAsRemoteProcedureCall("__RPCRequestViewIDBatch", RPCRequestViewIDBatch);
	m_Peer->RegisterAsRemoteProcedureCall("__RPCNetworkDestroy", RPCNetworkDestroy);

	m_Peer->SetOccasionalPing(true);	
	m_DoNAT = false;
	m_MinimumAllocatableViewIDs = kMinimumViewIDs;
	Disconnect(0);
	m_ConnTester = NULL;
	
	m_ServerAddress = UNASSIGNED_SYSTEM_ADDRESS;
	m_ServerPassword = "";
	m_FacilitatorID.binaryAddress = 0;
	m_FacilitatorID.port = kFacilitatorPort;
	m_ConnTesterAddress.binaryAddress = 0;
	m_ConnTesterAddress.port = kConnectionTesterPort;
	m_ConnStatus = kConnTestUndetermined;
	m_MaxConnections = 0;
	m_ProxyAddress.binaryAddress = 0;
	m_ProxyAddress.port = kProxyPort;
	m_UseProxy = false;
	m_ProxyPassword = "";
#if PACKET_LOGGER
	m_Peer->AttachPlugin(&messageHandler);
	messageHandler.LogHeader();
#endif
}

NetworkManager::~NetworkManager()
{
	RakNetworkFactory::DestroyRakPeerInterface(m_Peer);

	while (!m_PingQueue.empty())
	{
		m_PingQueue.back()->Release();
		m_PingQueue.pop();
	}
}

void NetworkManager::AddNetworkView (ListNode<NetworkView>& s)
{
	m_Sources.push_back(s);
}

void NetworkManager::AddAllNetworkView (ListNode<NetworkView>& s)
{
	m_AllSources.push_back(s);
}

void NetworkManager::AddNonSyncNetworkView (ListNode<NetworkView>& s)
{
	m_NonSyncSources.push_back(s);
}

void NetworkManager::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	#if UNITY_EDITOR
	AssertIf (!m_AssetToPrefab.empty());
	#endif

	m_PrefabToAsset.clear();
	for (AssetToPrefab::iterator i=m_AssetToPrefab.begin();i != m_AssetToPrefab.end();i++)
	{
		m_PrefabToAsset.insert(make_pair (i->second, i->first));	
	}
}

void NetworkManager::SetAssetToPrefab (const std::map<UnityGUID, PPtr<GameObject> >& mapping)
{
	m_AssetToPrefab = mapping;
	m_PrefabToAsset.clear();
	for (AssetToPrefab::iterator i=m_AssetToPrefab.begin();i != m_AssetToPrefab.end();i++)
		m_PrefabToAsset.insert(make_pair (i->second, i->first));	
} 

void NetworkManager::NetworkOnApplicationQuit()
{
// Don't tear down network if Application.CancelQuit has been called from user scripting (only for standalones)
#if !WEBPLUG && !UNITY_EDITOR
	if (GetInputManager().ShouldQuit()) {
#endif
		// Disconnect all connected peers, no wait interval, ordering channel 0
		Disconnect(100);

		m_MinimumAllocatableViewIDs = kMinimumViewIDs;
		SetUseNat(false);
		m_FacilitatorID.binaryAddress = 0;
		m_FacilitatorID.port = kFacilitatorPort;
		m_ConnTesterAddress.binaryAddress = 0;
		m_ConnTesterAddress.port = kConnectionTesterPort;
		delete m_ConnTester;
		m_ConnTester = NULL;
		m_ConnStatus = kConnTestUndetermined;
		m_MaxConnections = 0;
		m_PingThread.WaitForExit();
		m_ProxyAddress.binaryAddress = 0;
		m_ProxyAddress.port = kProxyPort;
		m_UseProxy = false;
		m_ProxyPassword = "";
#if !WEBPLUG && !UNITY_EDITOR
	}
#endif
}

void NetworkManager::InitializeSecurity()
{
	if (m_Peer->IsActive())
	{
		ErrorString("You may not be connected when initializing security layer.");
		return;
	}
	
	m_Peer->InitializeSecurity(0,0,0,0);
}

int NetworkManager::InitializeServer(int connections, int listenPort, bool useNat)
{
	Disconnect(kDefaultTimeout, false);
	
	m_MaxConnections = connections;
	SetUseNat(useNat);
	
	SocketDescriptor sd(listenPort,0);
	// Add two extra connections for the master server and facilitator
	if (!m_Peer->Startup(connections+2, 1, &sd, 1))
	{
		ErrorString("Failed to initialize network interface. Is the listen port already in use?");
		return kFailedToCreatedSocketOrThread;
	}
	// Hide the extra connections again.
	m_Peer->SetMaximumIncomingConnections(connections);

	m_PlayerID = 0;
	m_HighestPlayerID = 0;
	m_SendingEnabled = 0xFFFFFFFF;
	m_ReceivedInitialState = true;
	
	m_NetworkViewIDAllocator.Clear(kDefaultViewIDBatchSize, m_MinimumAllocatableViewIDs, m_PlayerID, kUndefindedPlayerIndex);
	m_NetworkViewIDAllocator.FeedAvailableBatchOnServer(m_NetworkViewIDAllocator.AllocateBatch(m_PlayerID));
	
	AssertIf(!m_Players.empty());
	
	m_PeerType = kServer;
	NetworkInfo(NULL, "Running as server. Player ID is 0.");
	
	// When running as server there is no need to go through the facilitator connection routine again. You just need to stay connected.
	if (m_DoNAT && !m_Peer->IsConnected(m_FacilitatorID))
	{
		GetFacilitatorAddress(true);
		
		// NOTE: At the moment the server peer interface maintains a constant connection to the master server for keeping the NAT
		// port open. This isn't neccessary after clients have connected as they will maintain the port. However, if all clients disconnect for
		// a while, the port will close sooner or later. Then the master server needs to have another port for the next client. Another
		// reason for keeping the connection persistant is that when a new client connects and needs NAT punch through the server must already
		// have a connection open or else the connection fails. Keeping it persistant is the most easy solution.
		if (!m_Peer->Connect(m_FacilitatorID.ToString(false),m_FacilitatorID.port,0,0))
		{
			ErrorString("Failed to connect to NAT facilitator\n");
		}
	}
	
	// TODO: Here we assume a connection to the proxy server cannot already be established. Could this happen?
	if (m_UseProxy)
	{
		ResolveProxyAddress();
		if (!m_Peer->Connect(m_ProxyAddress.ToString(false), m_ProxyAddress.port, m_ProxyPassword.c_str(), m_ProxyPassword.size(),0))
		{
			ErrorString(Format("Failed to connect to proxy server at %s.", m_ProxyAddress.ToString()));
		}
	}
	else
	{
		SendToAllNetworkViews(kServerInitialized, m_PlayerID);
	}
	
	return 0;
}

// Check if the IP addresses supplied are connectable and connect to the first successful IP.
int NetworkManager::Connect(std::vector<string> IPs, int remotePort, int listenPort, const std::string& password)
{
	if (IPs.size() == 1)
	{
		Connect(IPs[0].c_str(), remotePort, listenPort, password);
		return 0;
	}

	if (IPs.size() == 0) 
	{
		// Must prevent this or raknet will crash when an empty address is used
		ErrorString("Empty host IP list given in Connect\n");
		return kEmptyConnectTarget;
	}
	
	if (!password.empty()) m_ServerPassword = password;
	
	for (int i = 0; i < IPs.size(); i++)
	{
		if (IPs[i] != "")
		{
			if (!m_Peer->IsActive())
			{
				SocketDescriptor sd(listenPort,0);
				if (!m_Peer->Startup(2, 1, &sd, 1))
				{
					ErrorString("Failed to initialize network connection before connecting.");
					return kFailedToCreatedSocketOrThread;
				}
			}
			// Do request a reply if the system is not accepting connections, then can show the error message
			m_Peer->Ping(IPs[i].c_str(), remotePort, false);
			m_ConnectingAfterPing = true;
			m_PingConnectTimestamp = time(0);
		}
	}
	return 0;
}

int NetworkManager::Connect(std::string IP, int remotePort, const std::string& password)
{
	m_ReceivedInitialState = true;
	SetUseNat(false);

	// Connect to proxy which will from then on relay all server communication.
	// A proxy behind NAT is not supported, the UseNAT property is ignored here but
	// it applies to the game server the proxy server will connect to
	if (m_UseProxy)
	{
		ResolveProxyAddress();
		if (!m_Peer->Connect(m_ProxyAddress.ToString(false), m_ProxyAddress.port, m_ProxyPassword.c_str(), m_ProxyPassword.size(),0))
		{
			ErrorString(Format("Failed to connect to proxy server at %s\n", m_ProxyAddress.ToString()));
			return kFailedToCreatedSocketOrThread;
		}
		else
		{
			NetworkInfo(NULL, "Sent connect request to proxy at %s\n", m_ProxyAddress.ToString());
		}
		// Memorize server address as we will need it later when the proxy connection is established
		m_ServerAddress.SetBinaryAddress(IP.c_str());
		m_ServerAddress.port = (unsigned)remotePort;
		if (!password.empty())
			m_ServerPassword = password;
	}
	else
	{
		if (!m_Peer->Connect(IP.c_str(), remotePort, password.c_str(), password.size()))
		{
			ErrorString("Failed to connect. This is caused by an incorrect parameters, internal error or too many existing connections.");
			return kIncorrectParameters;
		}
	}
	AssertIf(!m_Players.empty());
	
	NetworkInfo(NULL, "Running as client. No player ID set yet.");
	
	return 0;
}

void NetworkManager::ResolveProxyAddress()
{
	ResolveAddress(m_ProxyAddress, "proxy.unity3d.com", "proxybeta.unity3d.com", 
		"Cannot resolve proxy address, make sure you are connected to the internet before connecting to a server.");
}

// NOTE: If this client is supposed to accept connections then SetMaximumIncomingConnections
//       must be set.
int NetworkManager::Connect(std::string IP, int remotePort, int listenPort, const std::string& password)
{
	Disconnect(kDefaultTimeout);

	// Connect returns immediately on a successful connection ATTEMPT. IsConnected() tells you if the connection actually succeded.
	// The network message ID_UNABLE_TO_CONNECT_TO_REMOTE_HOST if the remote hosts responds (closed port etc.)
	// NOTE: At the moment client connections are limited to only two connections. In p2p mode each peer will need a seperate connection
	SocketDescriptor sd(listenPort,0);
	if (!m_Peer->Startup(2, 1, &sd, 1))
	{
		ErrorString("Failed to initialize network connection before connecting.");
		return kFailedToCreatedSocketOrThread;
	}
	
	return Connect(IP, remotePort, password);
}

int NetworkManager::Connect(RakNetGUID serverGUID, int listenPort, const std::string& password)
{
	SetUseNat(true);
	Disconnect(kDefaultTimeout);
	
	// Connect returns immediately on a successful connection ATTEMPT. IsConnected() tells you if the connection actually succeded.
	// The network message ID_UNABLE_TO_CONNECT_TO_REMOTE_HOST if the remote hosts responds (closed port etc.)
	// NOTE: At the moment client connections are limited to only two connections. In p2p mode each peer will need a seperate connection
	SocketDescriptor sd(listenPort,0);
	if (!m_Peer->Startup(2, 1, &sd, 1))
	{
		ErrorString("Failed to initialize network connection before connecting.");
		return kFailedToCreatedSocketOrThread;
	}
	
	ResolveFacilitatorAddress();
	
	if (!m_Peer->Connect(m_FacilitatorID.ToString(false), m_FacilitatorID.port,0,0))
	{
		ErrorString(Format("Failed to connect to NAT facilitator at %s\n", m_FacilitatorID.ToString()));
		return kFailedToCreatedSocketOrThread;
	}
	else
	{
		NetworkInfo(NULL, "Sent connect request to facilitator at %s\n", m_FacilitatorID.ToString());
	}
	
	if (!password.empty()) m_ServerPassword = password;
	
	m_ServerGUID = serverGUID;
	
	return 0;
}

// The CloseConnection function allows you to disconnect a specific playerID
void NetworkManager::Disconnect(int timeout, bool resetParams)
{
	if (GetMasterServerInterfacePtr()) 
	{
		GetMasterServerInterface().UnregisterHost();
		GetMasterServerInterface().Disconnect();
	}
	m_Peer->Shutdown(timeout);

	if (IsServer () || IsClient())
		SendToAllNetworkViews(kDisconnectedFromServer, ID_DISCONNECTION_NOTIFICATION);
	
	if (resetParams)
	{
		m_Peer->DisableSecurity();
		SetIncomingPassword("");
	}
	
	m_Players.clear();
	m_PeerType = kDisconnected;
	m_MessageQueueRunning = true;
	m_SendingEnabled = 0;
	m_LevelPrefix = 0;
	
	for (list<RPCMsg>::iterator irpc = m_RPCBuffer.begin(); irpc != m_RPCBuffer.end(); ++irpc ) 
	{
		RPCMsg& rpc = *irpc;
		delete rpc.stream;
	}
	m_RPCBuffer.clear();

	m_ServerAddress = UNASSIGNED_SYSTEM_ADDRESS;
	m_ServerPassword = "";
	m_HighestPlayerID = 0;
	m_PlayerID = -1;
	m_LastSendTime = -1.0F;
	m_ConnectingAfterPing = false;
	m_UsedInitIndices.clear();
	
	for( NetworkViewIterator iview = m_AllSources.begin(); iview != m_AllSources.end(); ++iview ) {
		NetworkView& view = **iview;
		view.ClearInitStateAndOwner();
	}
}

void NetworkManager::CloseConnection(int target, bool sendDisconnect)
{
	SystemAddress address = GetSystemAddressFromIndex(target);
	if (address != UNASSIGNED_SYSTEM_ADDRESS)
		m_Peer->CloseConnection(address, sendDisconnect, kInternalChannel);
	else
	{
		ErrorString("Couldn't close connection because the player is not connected.");
	}
}

PROFILER_INFORMATION(gNetworkUpdateProfile, "Network.Update", kProfilerNetwork)

void NetworkManager::NetworkUpdate()
{
	PROFILER_AUTO(gNetworkUpdateProfile, NULL)
	
	m_Packet = NULL;
	if (m_MessageQueueRunning)
		m_Packet = m_Peer->ReceiveIgnoreRPC();	// Receive whole message from peer
	
	if (m_ConnectingAfterPing && time(0) - m_PingConnectTimestamp > 5)
	{
		m_ConnectingAfterPing = false;
		NetworkError (NULL, "Unable to connect internally to NAT target(s), no response.");
		SendToAllNetworkViews(kConnectionAttemptFailed, kInternalDirectConnectFailed);
	}

	// Destroy connection tester after the timeout (test should be over) or if an error occured
	if (m_ConnTester)
	{
		m_ConnStatus = m_ConnTester->Update();
		// ATM -2 = timeout || error
		/*if (m_ConnStatus == -2)
		{
			delete m_ConnTester;
			m_ConnTester = NULL;
		}*/
	}
	
	// Check if there are any pending pings
	if (!m_PingQueue.empty())
	{
		if (!m_PingThread.IsRunning())
		{
			m_PingThread.Run(&PingImpl, (void*)m_PingQueue.front(), DEFAULT_UNITY_THREAD_STACK_SIZE, 2);
			m_PingQueue.pop();
		}
	}

	while (m_Packet)
	{
		unsigned char packetIdentifier = m_Packet->data[0];
		if (packetIdentifier == ID_TIMESTAMP && m_Packet->length > sizeof(unsigned char) + sizeof (RakNetTime))
		{
			packetIdentifier = m_Packet->data[sizeof(unsigned char) + sizeof(RakNetTime)];
		}
		
		bool fromMasterServer = m_Packet->systemAddress == GetMasterServerInterface().GetMasterServerID();
		if (fromMasterServer)
			GetMasterServerInterface().ProcessPacket(m_Packet);
		else
			ProcessPacket(packetIdentifier);
		
		m_Peer->DeallocatePacket(m_Packet);		// Ditch message
		
		if (m_MessageQueueRunning)
			m_Packet = m_Peer->ReceiveIgnoreRPC();			// Check if there are other messages queued	
		else
			m_Packet = NULL;
	}
	m_Packet = NULL;
	
	if (!m_Peer->IsActive())
		return;
	
	float realtime = GetTimeManager().GetRealtime();	
	if (realtime > m_LastSendTime + 1.0F / m_Sendrate && GetConnectionCount() >= 1)
	{
		// Send updates
		for (NetworkViewIterator i = m_Sources.begin(); i != m_Sources.end(); i++) {
			NetworkView *view = i->GetData();
			
			// Are we allowed to send?
			if (m_SendingEnabled & (1 << view->GetGroup()))
			{
				// If running as server and one or more client is connected
				if ( IsServer() )
				{
					view->SendToAllButOwner();
				}
				// If running as client and the views owner player address matches this one then send to server
				else if (IsClient() && m_Peer->GetInternalID() == view->GetOwnerAddress())
				{
					view->Send(m_ServerAddress, false);
				}
			}
		}
		
		m_LastSendTime = realtime;
	}
	
	RakSleep(0);
}

void NetworkManager::ProcessPacket(unsigned char packetIdentifier)
{
	switch (packetIdentifier)
	{
		case ID_CONNECTION_REQUEST_ACCEPTED:
		{
			bool fromFacilitator = m_Packet->systemAddress == m_FacilitatorID;
			bool fromProxyServer = m_Packet->systemAddress == m_ProxyAddress;
			
			// A client doing NAT punch through to game server with help from facilitator
			if (m_DoNAT && fromFacilitator && !IsServer())
			{
				NetworkInfo(NULL, "Connected to facilitator at %s\n",m_Packet->systemAddress.ToString());
				char tmp[32];
				strcpy(tmp, m_FacilitatorID.ToString());
				NetworkInfo(NULL, "Doing NAT punch through to %s using %s\n", m_ServerGUID.ToString(), tmp);
				m_NatPunchthrough.OpenNAT(m_ServerGUID, m_FacilitatorID);
			}
			else if (fromFacilitator && IsServer())
			{
				NetworkInfo(NULL, "Connected to facilitator at %s\n",m_Packet->systemAddress.ToString());
			}
			else if (fromProxyServer && IsServer())
			{
				if (!m_UseProxy)
				{
					ErrorString("Connected to proxy server but proxy support not enabled.");
					return;
				}
				m_BitStream.Reset();
				m_BitStream.Write((unsigned char) (ID_PROXY_SERVER_INIT));
				m_BitStream.Write(1);		// Proxy protocol version
				if (!m_Peer->Send(&m_BitStream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false))
				{
					ErrorString("Failed to send server init to proxy server.");
				}
				else
				{
					NetworkInfo(NULL, "Sending init req. to proxy server at %s", m_ProxyAddress.ToString());
				}
			}
			// Client connected to server (proxy or game)
			else
			{
				SystemAddress target =  m_Packet->systemAddress;
				m_BitStream.Reset();
				if (m_UseProxy)
				{
					m_BitStream.Write((unsigned char) (ID_PROXY_INIT_MESSAGE));
					m_BitStream.Write(1);		// Proxy protocol version
					m_BitStream.Write(m_ServerAddress);
					if (!m_ServerPassword.empty())
					{
						m_BitStream.Write1();
						m_BitStream.Write(m_ServerPassword.size());
						m_BitStream.Write(m_ServerPassword.c_str(), m_ServerPassword.size());
					}
					else
					{
						m_BitStream.Write0();
					}
					m_BitStream.Write(m_DoNAT);
					m_BitStream.Write(1);		// Network protocol version
					target = m_ProxyAddress;
					NetworkInfo(NULL, "Sending init req. to %s, relayed through proxy", m_ServerAddress.ToString());
				}
				else
				{
					// Specifically request init from server
					m_BitStream.Write((unsigned char) (ID_REQUEST_CLIENT_INIT));
					m_BitStream.Write((int)1);									// Network protocol version
					m_ServerAddress = m_Packet->systemAddress;	// Record server address
				}
				if (!m_Peer->Send(&m_BitStream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, target, false))
					ErrorString("Failed to send client init to server.");
				NetworkInfo(NULL, "Connected to %s\n",m_Packet->systemAddress.ToString());
			}
			break;
		}
		case ID_NO_FREE_INCOMING_CONNECTIONS:
			ErrorString("The system we attempted to connect to is not accepting new connections.");
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_NO_FREE_INCOMING_CONNECTIONS);
			break;
		case ID_RSA_PUBLIC_KEY_MISMATCH:
			ErrorString("We preset an RSA public key which does not match what the system we connected to is using.");
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_RSA_PUBLIC_KEY_MISMATCH);
			break;
		case ID_INVALID_PASSWORD:
		{
			if (m_Packet->systemAddress == m_ProxyAddress)
			{
				ErrorString("The proxy server is using a password and has refused our connection because we did not set it correctly.");
			}
			else
			{
				ErrorString("The remote system is using a password and has refused our connection because we did not set the correct password.");
			}
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_INVALID_PASSWORD);
			break;
		}
		case ID_CONNECTION_ATTEMPT_FAILED:
		{
			ErrorString(Format("The connection request to %s failed. Are you sure the server can be connected to?", m_Packet->systemAddress.ToString()));
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_CONNECTION_ATTEMPT_FAILED);
			break;
		}
		case ID_ALREADY_CONNECTED:
		{
			NetworkError(NULL, "Failed to connect to %s because this system is already connected. This can occur when the network connection is disconnected too quickly for the remote system to receive the disconnect notification, for example when using Network.Disconnect(0).", m_Packet->systemAddress.ToString());
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_ALREADY_CONNECTED);
			break;
		}
		case ID_CONNECTION_BANNED:
		{
			ErrorString("We are banned from the system we attempted to connect to.");
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_CONNECTION_BANNED);
			break;
		}
		case ID_NEW_INCOMING_CONNECTION:
		{
			// Happens during public IP firewall test
			if (m_Packet->systemAddress == m_ConnTesterAddress)
			{
				if (m_ConnTester)
				{
					m_ConnStatus = kPublicIPIsConnectable;
					m_ConnTester->ReportTestSucceeded();
				}
				return;
			}
			
			if (!IsServer())
			{
				m_Peer->CloseConnection(m_Packet->systemAddress, true);
				NetworkError(NULL, "New incoming connection but not a server.");
				return;
			}
			
			NetworkInfo(NULL, "New connection established (%s)", m_Packet->systemAddress.ToString());
			break;
		}
		case ID_REQUEST_CLIENT_INIT:
		{
			MsgNewConnection();
			break;
		}
		// TODO: Should we differentiate between these? One is expected, the other is not (important for integration tests)
		case ID_DISCONNECTION_NOTIFICATION:
		case ID_CONNECTION_LOST:
		{
			// When master server ID has been changed we need to ignore the disconnect from the old master server
			if (m_Packet->systemAddress == m_OldMasterServerID && IsServer())
			{
				// Do nothing
			}
			// Same with the old facilitator
			else if (m_Packet->systemAddress == m_OldFacilitatorID && IsServer())
			{
				// Do nothing
			}
			else if (m_Packet->systemAddress == m_ConnTesterAddress)
			{
				// Do nothing
			}
			// If connection was lost with the NAT facilitator we should reconnect as it must be persistent for now.
			// If it is not persistent then the connections must be set up every time a client wants to connect (and at the moment there
			// is no way of knowing when that is).
			else if (m_Packet->systemAddress == m_FacilitatorID && IsServer())
			{
				// No need to resolve again as the above condition will never happen if the address in unresolved
				if (!m_Peer->Connect(m_FacilitatorID.ToString(false),m_FacilitatorID.port,0,0))
				{
					ErrorString(Format("Failed to reconnect to NAT facilitator at %s\n", m_FacilitatorID.ToString()));
					return;
				}
				else
				{
					LogString("Connection lost to NAT facilitator, reconnecting.\n");
				}
			}
			else if (m_Packet->systemAddress == m_ProxyAddress)
			{
				NetworkLog(NULL, "Lost connection to proxy server at %s", m_Packet->systemAddress.ToString());
				// If we are a client then either the proxy server connection was lost or the game server connection
				if (IsClient())
				{
					//SendToAllNetworkViews(kConnectionAttemptFailed, ID_CONNECTION_ATTEMPT_FAILED);
					SendToAllNetworkViews(kDisconnectedFromServer, m_Packet->data[0]);
				}
			}
			else if (IsServer())
			{
				NetworkInfo(NULL, "A client has disconnected from this server. Player ID %d, IP %s", GetIndexFromSystemAddress(m_Packet->systemAddress), m_Packet->systemAddress.ToString());
				MsgClientDidDisconnect();
			}
			else
			{
				LogString("The server has disconnected from the client. Most likely the server has shut down.");
				ClientConnectionDisconnected(m_Packet->data[0]);
			}
			break;
		}
		case ID_STATE_UPDATE:
		case ID_STATE_INITIAL:
			// Normal state update, reset bitstream accordingly
			m_BitStream.Reset();
			m_BitStream.Write((char*)m_Packet->data, m_Packet->length);
			MsgStateUpdate(m_Packet->systemAddress);
			break;
		case ID_CLIENT_INIT:
			MsgClientInit();
			break;
		case ID_MODIFIED_PACKET:
			ErrorString("A packet has been tampered with in transit.");
			break;
		case ID_REMOVE_RPCS:
			MsgRemoveRPCs();
			break;
		// TODO: Implement other error codes: ID_NAT_TARGET_UNRESPONSIVE ID_NAT_ALREADY_IN_PROGRESS
		case ID_NAT_PUNCHTHROUGH_SUCCEEDED:
		{
			unsigned char weAreTheSender = m_Packet->data[1];
			if (weAreTheSender)
			{
				NetworkInfo(NULL, "Successfully performed NAT punchthrough to %s\n",m_Packet->systemAddress.ToString());
				m_ServerAddress = m_Packet->systemAddress;
				// TODO: Maybe we should be going through Connect just so we always go through that function.
				if (!m_Peer->Connect(m_ServerAddress.ToString(true), m_ServerAddress.port, m_ServerPassword.c_str(), m_ServerPassword.size()))
				{
					ErrorString("Failed to connect. This is caused by an incorrect parameters, internal error or too many existing connections.");
				}
			}
			else
			{
				NetworkInfo(NULL, "Successfully accepted NAT punchthrough connection from %s\n",m_Packet->systemAddress.ToString());
			}
			break;
		}
		case ID_NAT_TARGET_NOT_CONNECTED:
		{
			RakNetGUID remoteGuid;
			SystemAddress systemAddress;
			RakNet::BitStream b(m_Packet->data, m_Packet->length, false);
			b.IgnoreBits(8); // Ignore the ID_...
			b.Read(remoteGuid);
			
			// TODO: Lookup IP/port of the guid we were trying to connect to
			
			// SystemAddress.ToString() cannot be used twice in the same line because it uses a static char array
			ErrorString(Format("NAT target %s not connected to NAT facilitator %s\n", remoteGuid.ToString(), m_Packet->systemAddress.ToString()));
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_NAT_TARGET_NOT_CONNECTED);
			break;
		}
		case ID_NAT_CONNECTION_TO_TARGET_LOST:
		{
			RakNetGUID remoteGuid;
			SystemAddress systemAddress;
			RakNet::BitStream b(m_Packet->data, m_Packet->length, false);
			b.IgnoreBits(8); // Ignore the ID_...
			b.Read(remoteGuid);
			
			// TODO: Lookup IP/port of the guid we were trying to connect to
			
			ErrorString(Format("Connection to target %s lost\n", systemAddress.ToString()));
			SendToAllNetworkViews(kConnectionAttemptFailed, ID_NAT_CONNECTION_TO_TARGET_LOST);
			break;
		}
		case ID_NAT_PUNCHTHROUGH_FAILED:
		{
			bool isConnecting = false;
			RakNet::BitStream b(m_Packet->data, m_Packet->length, false);
			b.IgnoreBits(8); // Ignore the ID_...
			b.Read(isConnecting);
			if (isConnecting)
			{
				ErrorString(Format("Initiating NAT punchthrough attempt to target %s failed\n", m_Packet->guid.ToString()));
				SendToAllNetworkViews(kConnectionAttemptFailed, ID_NAT_PUNCHTHROUGH_FAILED);
			}
			else
			{
				ErrorString(Format("Receiving NAT punchthrough attempt from target %s failed\n", m_Packet->guid.ToString()));
				// No need to report the failed connection attempt on the receiver
				//SendToAllNetworkViews(kConnectionAttemptFailed, ID_NAT_PUNCHTHROUGH_FAILED);
			}
			break;
		}
		case ID_PONG:
		{
			LogString(Format("Received pong from %s", m_Packet->systemAddress.ToString()));
			if (m_ConnectingAfterPing)
			{
				m_ConnectingAfterPing = false;
				Connect(m_Packet->systemAddress.ToString(false), m_Packet->systemAddress.port, m_ServerPassword);
			}
			break;
		}
		case ID_PING:
			// Do nothing, this should be handled internally by RakNet (actually shouldn't be here)
			break;
		case ID_RPC:
		{
			char *message = NULL;
			message = m_Peer->HandleRPCPacket( ( char* ) m_Packet->data, m_Packet->length, m_Packet->systemAddress );
			if (message != NULL)
			{
				NetworkError(NULL, "%s", message);
			}
			break;
		}
		case ID_PROXY_SERVER_INIT:
		{
			int proxyVer;
			unsigned short relayPort;
			RakNet::BitStream b(m_Packet->data, m_Packet->length, false);
			b.IgnoreBits(8);
			b.Read(proxyVer);	// Proxy protocol version
			b.Read(relayPort);
			NetworkLog(NULL, "Proxy version %d", proxyVer);
			
			if (relayPort > 0) 
			{
				m_RelayPort = relayPort;
				NetworkLog(NULL, "Successfully initialized relay service from proxy server. Using port %u.", relayPort);
				SendToAllNetworkViews(kServerInitialized, -2);
			}
			else if (relayPort == 0)
			{
				NetworkLog(NULL, "Failed to initialize relay service from proxy server. No available ports reported.");
				SendToAllNetworkViews(kServerInitialized, -1);
			}
			else
			{
				NetworkLog(NULL, "Failed to initialize relay service from proxy server. Unkown error.");
				SendToAllNetworkViews(kServerInitialized, -1);
			}
			break;
		}
		case ID_PROXY_MESSAGE:
		{
			SystemAddress senderAddress;
			unsigned char messageID;
			int proxyVer = 0;
			m_BitStream.Reset();
			m_BitStream.Write((char*)m_Packet->data, m_Packet->length);
			m_BitStream.IgnoreBits(8);
			
			m_BitStream.Read(senderAddress);
			int proxiedMessageID = sizeof(MessageID) + SystemAddress::size();
			messageID = m_Packet->data[proxiedMessageID];
			
			// Record the proxy system address for future reference
			// TODO: With this method, only one proxy is allowed. Enforce it? Proxy address could be added to player structure.
			m_ProxyAddress = m_Packet->systemAddress;
			
			// If its a time stamp then we need to dig deeper and find out what it really is.
			// If there is a timestamp embedded, the real ID is at:
			if (messageID == ID_TIMESTAMP)
			{
				int timestampedProxiedMessageID = proxiedMessageID + sizeof(MessageID) + sizeof(RakNetTime);
				messageID = m_Packet->data[timestampedProxiedMessageID];
			}
			
			switch (messageID)
			{
				case ID_DISCONNECTION_NOTIFICATION:
				{
					int playerID = GetIndexFromSystemAddress(senderAddress);
					NetworkInfo(NULL, "A proxied client has disconnected from this server. Player ID %d, IP %s", playerID, senderAddress.ToString());
					MsgClientDidDisconnect(senderAddress);
					break;
				}
					break;
				case ID_REQUEST_CLIENT_INIT:
					m_BitStream.IgnoreBits(8);
					m_BitStream.Read(proxyVer);	// Proxy protocol version
					NetworkLog(NULL, "Proxy version %d", proxyVer);
					MsgNewConnection(senderAddress);
					break;
					// State update from client
				case ID_STATE_INITIAL:
				case ID_STATE_UPDATE:
					NetworkLog(NULL, "State update from client %s, through proxy", senderAddress.ToString());
					MsgStateUpdate(senderAddress);
					break;
				case ID_RPC:
				{
					char *message = NULL;
					// When passing packet data, skip over proxy data (ID+sender address)
					message = m_Peer->HandleRPCPacket( ( char* ) m_Packet->data+7, m_Packet->length-7, senderAddress );
					if (message != NULL)
						NetworkError(NULL, "Error receiving proxy RPC: %s", message);
				}
					break;
				default:
					NetworkError(NULL, "Unhandled relayed message %d from %s", messageID, m_Packet->systemAddress.ToString());
					break;
			}
			break;
		}
		default:
			NetworkError(NULL, "Unhandled message %d from %s", (int)m_Packet->data[0], m_Packet->systemAddress.ToString());
			break;
	}
}

string NetworkManager::GetStats(int i)
{
	char buf[8000];
	string output;
	if (IsServer())
	{
		RakNetStatistics* stats = m_Peer->GetStatistics(m_Players[i].playerAddress);
		if (stats)
		{
			StatisticsToString(stats, buf, 1);
			output += Format("Player %d\n", i);
			output += buf;
		}
	}
	else if (IsClient())
	{
		RakNetStatistics* serverStats = m_Peer->GetStatistics(m_ServerAddress);
		if (serverStats)
		{
			StatisticsToString(serverStats, buf, 1);
			output += buf;
		}
	}
	return output;	
}

void NetworkManager::ClientConnectionDisconnected(int msgType)
{
	SendToAllNetworkViews(kDisconnectedFromServer, msgType);
	m_PeerType = kDisconnected;
	m_Players.clear();
}


void NetworkManager::MsgNewConnection(SystemAddress clientAddress)
{
	NetworkPlayer playerID = ++m_HighestPlayerID;
	int networkProtocolVer = 0;
	
	m_BitStream.Read(networkProtocolVer);
	
	NetworkLog(NULL, "Network protocol version %d connected", networkProtocolVer);

	m_BitStream.Reset();
	
	// Record the address of this new player
	PlayerTable player;
	player.playerIndex = playerID;
	player.initIndex = GetValidInitIndex();
	player.mayReceiveGroups = 0xFFFFFFFF;
	player.isDisconnected = false;
	player.maySendGroups = 0xFFFFFFFF;
	player.guid = m_Packet->guid.ToString();
	if (clientAddress != UNASSIGNED_SYSTEM_ADDRESS)
	{
		player.playerAddress = clientAddress;
		player.relayed = true;
		NetworkInfo(NULL, "Registering new proxied client %s", clientAddress.ToString());
		m_BitStream.Write((unsigned char)ID_PROXY_SERVER_MESSAGE);
		m_BitStream.Write(clientAddress);
	}
	else
	{
		player.playerAddress = m_Packet->systemAddress;
		player.relayed = false;
	}
	m_Players.push_back(player);

	m_BitStream.Write((unsigned char) (ID_CLIENT_INIT));
	m_BitStream.Write(1);			// Server version
	m_BitStream.Write(m_PlayerID);
	m_BitStream.Write(playerID);
	
	// Send Network allocator batch data
	UInt32 batchSize = m_NetworkViewIDAllocator.GetBatchSize();
	UInt32 nbBatches = ((m_MinimumAllocatableViewIDs - 1) / batchSize) + 1;
	m_BitStream.Write(batchSize);
	m_BitStream.Write(nbBatches);
	for (int i=0;i<nbBatches;i++)
	{
		UInt32 batch = m_NetworkViewIDAllocator.AllocateBatch(playerID);
		m_BitStream.Write(batch);
	}
	NetworkLog(NULL, "Allocated %d batches of size %d for player %d", nbBatches, batchSize, playerID);
	
	// Send init data only to client which connected
	if (!m_Peer->Send(&m_BitStream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_Packet->systemAddress, false))
	{
		ErrorString("Failed to send initialization message to new client");
	}
	else
	{
		NetworkInfo(NULL, "Sent initalization to player %d", playerID);
	}
	
	// Send buffered RPCs
	SendRPCBuffer(player);
	
	SendToAllNetworkViews(kPlayerConnected, playerID);
}

void NetworkManager::MsgClientInit()
{
	int serverVersion = 0;
	NetworkPlayer serverId = 0;

	// Extract player ID
	m_BitStream.Reset();
	m_BitStream.Write((char*)m_Packet->data, m_Packet->length);
	m_BitStream.IgnoreBits(8);
	m_BitStream.Read(serverVersion);
	m_BitStream.Read(serverId);
	m_BitStream.Read(m_PlayerID);

	// Setup network view id allocator
	// Feed initial available batches
	UInt32 batchSize = 0;
	m_BitStream.Read(batchSize);
	UInt32 batchCount = 0;
	m_BitStream.Read(batchCount);
	m_NetworkViewIDAllocator.Clear(batchSize, m_MinimumAllocatableViewIDs, serverId, m_PlayerID);
	for (int i=0;i<batchCount;i++)
	{
		UInt32 batch = 0;
		m_BitStream.Read(batch);
		m_NetworkViewIDAllocator.FeedAvailableBatchOnClient(batch);
	}
	
	// Register the server as a player in the player table. Note that the server address is already in m_ServerAddress
	PlayerTable player;
	player.playerIndex = serverId;
	player.initIndex = 0;
	player.playerAddress = m_Packet->systemAddress;		// This is the proxy address when proxy is in use
	player.mayReceiveGroups = 0xFFFFFFFF;
	player.maySendGroups = 0xFFFFFFFF;
	player.isDisconnected = false;
	player.relayed = false;
	m_Players.push_back(player);
	
	m_PeerType = kClient;	
	m_SendingEnabled = 0xFFFFFFFF;
	
	// Set the proxy address as the server address if appropriate or network loop will break
	if (m_UseProxy)
		m_ServerAddress = m_ProxyAddress;
		
	SendToAllNetworkViews(kConnectedToServer);
	
	NetworkInfo(NULL,"Set player ID to %d\n", m_PlayerID);
}

void NetworkManager::RPCReceiveViewIDBatch (RPCParameters *rpcParameters)
{
	NetworkManager& nm = GetNetworkManager();
	RakNet::BitStream bitStream;
	bitStream.Write((char*)rpcParameters->input, BITS_TO_BYTES(rpcParameters->numberOfBitsOfData));
	UInt32 batchIndex;
	if (bitStream.Read(batchIndex) && rpcParameters->sender == nm.m_ServerAddress)
	{
		nm.m_NetworkViewIDAllocator.FeedAvailableBatchOnClient(batchIndex);
		nm.m_NetworkViewIDAllocator.AddRequestedBatches(-1);
	}
	else
	{
		NetworkError (NULL, "Failed receiving RPC batch index");
	}
}

void NetworkManager::RPCRequestViewIDBatch (RPCParameters *rpcParameters)
{
	NetworkManager& nm = GetNetworkManager();
	NetworkPlayer player = nm.GetIndexFromSystemAddress(rpcParameters->sender);
	if (player != kUndefindedPlayerIndex)
	{
		UInt32 batchIndex = nm.m_NetworkViewIDAllocator.AllocateBatch(player);

		RakNet::BitStream bitStream;
		bitStream.Write(batchIndex);

		if (!nm.m_Peer->RPC("__RPCReceiveViewIDBatch", &bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, rpcParameters->sender, false, NULL, UNASSIGNED_NETWORK_ID, 0))
			NetworkError(NULL, "Failed to send RPC batch to client!");
		else
			NetworkLog(NULL, "Sent batch %d of size %d to %d\n", batchIndex, nm.m_NetworkViewIDAllocator.GetBatchSize(), player);
	}
	else
	{
		NetworkError(NULL, "Failed to send RPC batch to because he is not in the player list!");
	}
}

void NetworkManager::SendRPCBuffer (PlayerTable &player)
{
	RakNetTime time = GetTimestamp();
	
	/// Send the whole RPC buffer to the player
	list<RPCMsg>::iterator i;
	for( i = m_RPCBuffer.begin(); i != m_RPCBuffer.end(); i++ ) 
	{
		RPCMsg& rpc = *i;
		// Send RPC call just to client which requested RPC flush

		if (player.relayed)
		{
			if (!m_Peer->RPC((char*)rpc.name.c_str(), rpc.stream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false, &time, UNASSIGNED_NETWORK_ID, 0, (unsigned char) ID_PROXY_SERVER_MESSAGE,  player.playerAddress))
				ErrorString("Couldn't send buffered RPCs to proxied client");
		}
		else
		{
			if (!m_Peer->RPC((char*)rpc.name.c_str(), rpc.stream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, player.playerAddress, false, &time, UNASSIGNED_NETWORK_ID, 0))
				ErrorString("Couldn't send buffered RPCs to client");
		}
			
		/* ***** needs to be updated to match new rpc signature

		// DEBUG, print out info on buffered rpc call, erase eventually
		if (m_DebugLevel >= kInformational)
		{
			int parentID, newID, mode;
			i->stream->ResetReadPointer();
			i->stream->Read(parentID);
			i->stream->Read(mode);
			i->stream->Read(newID);
			LogString(Format("Sending buffered RPC \"%s\" to new client parent ID %d, inst ID %d, mode %d\n", i->name.c_str(), parentID, newID, mode));
		}
		*/
	}
}

bool NetworkManager::MayReceiveFromPlayer( SystemAddress adress, int group )
{
	PlayerTable* player = GetPlayerEntry( adress );
	if( player )
	{
		return player->mayReceiveGroups & (1 << group);
	}
	else
	{
		//ErrorString( "May not receive channel from player which does not exist" );
		///@TODO: ERROR ON THIS AND PREVENT SENDING!!!
		
		return false;
	}
}

bool NetworkManager::MaySendToPlayer( SystemAddress address, int group )
{
	PlayerTable* player = GetPlayerEntry( address );
	if( player )
	{
		return player->maySendGroups & (1 << group);
	}
	else
	{
		NetworkInfo(NULL,"NetworkPlayer instance not found for address %s, probably not connected", address.ToString());
		return false;
	}
}


void NetworkManager::SetReceivingGroupEnabled (int playerIndex, int group, bool enabled)
{
	PlayerTable* player = GetPlayerEntry(playerIndex);
	if (player)
	{
		if (enabled)
			player->mayReceiveGroups  |= 1 << group;
		else
			player->mayReceiveGroups &= ~(1 << group);
	}
	else
	{
		ErrorString("SetReceivingEnabled failed because the player is not connected.");
	}
}

void NetworkManager::SetSendingGroupEnabled (int group, bool enabled)
{
	if (enabled)
		m_SendingEnabled  |= 1 << group;
	else
		m_SendingEnabled  &= ~(1 << group);
}

void NetworkManager::SetSendingGroupEnabled (int playerIndex, int group, bool enabled)
{
	PlayerTable* player = GetPlayerEntry(playerIndex);
	if (player)
	{
		if (enabled) {
			NetworkInfo(NULL, "Enabling sending group %d for player %d", group, playerIndex);
			player->maySendGroups  |= 1 << group;
		}
		else {
			NetworkInfo(NULL, "Disabling sending group %d for player %d", group, playerIndex);
			player->maySendGroups &= ~(1 << group);
		}
	}
	else
	{
		ErrorString("SetSendingEnabled failed because the player is not connected.");
	}
}

void NetworkManager::MsgStateUpdate(SystemAddress senderAddress)
{
	// TODO: Maybe do some integrity checking and such to make sure
	// there are no inconsistencies like two objects having the same
	// network ID or one ID not being found on the receiver, etc.
	
	//m_BitStream.Reset();
	//m_BitStream.Write((char*)m_Packet->data, m_Packet->length);
	// IMPORTANT: The bitstream must have the read pointer at the correct position

	UInt8 msgType;
	m_BitStream.Read(msgType);

	NetworkMessageInfo info;
	info.timestamp = -1.0F;
	if (msgType == ID_TIMESTAMP)
	{
		RakNetTime timeStamp = 0;
		if (m_BitStream.Read(timeStamp))
			info.timestamp = TimestampToSeconds(timeStamp);
		m_BitStream.Read(msgType);
	}
	
	NetworkViewID viewID;
	viewID.Read(m_BitStream);

	info.viewID = viewID;
	info.sender = GetIndexFromSystemAddress(senderAddress);
	AssertIf(info.sender == -1);

	NetworkView* view = ViewIDToNetworkView(viewID);
	if (view != NULL)
	{
		if (MayReceiveFromPlayer (senderAddress, view->GetGroup()))
		{
			SystemAddress owner = view->GetOwnerAddress();
			// Check incoming packets if the sender is allowed to send them.
			if (m_PeerType == kClient)
			{
				if (owner.binaryAddress != 0)
				{
					NetworkError (NULL, "State update for an object this players owns has been received. Packet was ignored.");
					return;
				}
				
				// If the client has useProxy enabled then all state updates should come from the proxy.
				if (m_UseProxy)
				{
					if (m_ProxyAddress != m_Packet->systemAddress)
					{
						NetworkError (NULL, "State update was received from someone else than the server. Packet was ignored. Sender was %s", m_Packet->systemAddress.ToString());
						return;
					}
				}
				else if (m_Packet->systemAddress != m_ServerAddress)
				{
					NetworkError (NULL, "State update was received from someone else than the server. Packet was ignored. Sender was %s", m_Packet->systemAddress.ToString());
					return;
				}
			}
			// This check is not valid with proxy servers in existance
			// And for server
			// We could lookup the system address in the player table list and check is it is supposed to be relayed
			// and that the sender address is the proxy server
			/*else
			{
				if ( owner != m_Packet->systemAddress )
				{
					NetworkError (NULL, "State update for an object received that is not owned by the sender. Packet was ignored. Sender was %s", m_Packet->systemAddress.ToString());
					return;
				}
			}*/
									
			view->Unpack(m_BitStream, info, msgType);
		}
		else
		{
			NetworkInfo(view, "Received state update for view '%s' and ignored it because the channel %d is disabled.\n", viewID.ToString().c_str(), view->GetGroup());
		}
	}
	else
	{
		NetworkWarning(NULL, "Received state update for view id' %s' but the NetworkView doesn't exist", viewID.ToString().c_str());
	}
}

void NetworkManager::DestroyPlayerObjects(NetworkPlayer playerID)
{
	if (IsClient() && playerID != GetPlayerID())
	{
		NetworkError(NULL, "A client can only destroy his own player objects, %d is a remote player", playerID);
		return;
	}

	NetworkInfo(NULL, "Destroying objects belonging to player %d",playerID); 

	bool erased = false;
	
	for (int s=0;s<2;s++)
	{
		NetworkViewList& list = s == 0 ? m_Sources : m_NonSyncSources;
		
		SafeIterator<NetworkViewList> i (list);
		while (i.Next())
		{
			NetworkView& view = *i->GetData();
			NetworkViewID viewID = NetworkViewToViewID(&view);
			if (WasViewIdAllocatedByPlayer(viewID, playerID))
			{
				DestroyDelayed(viewID);
				erased = true;
			}
		}
	}
	
	if (!erased)
	{
		LogString(Format("No objects for the given player ID were deleted %d", (int)playerID));
	}
}

void NetworkManager::DestroyDelayed(NetworkViewID viewID)
{
	if (m_DebugLevel >= kInformational) LogString(Format("Destroying object with view ID '%s'", viewID.ToString().c_str())); 

	// Destroy object locally
	NetworkView* view = ViewIDToNetworkView(viewID);
	if (view)
	{
		Scripting::DestroyObjectFromScripting(&view->GetGameObject(), -1.0F);
	}
	else
	{
		ErrorString("Couldn't destroy object because the associate network view was not found");
		return;
	}
	
	// Destroy remotely
	m_BitStream.Reset();
	viewID.Write(m_BitStream);
	
	if (IsClient())
	{
		// NOTE: It is assumed that the server is the first entry in the player table
		PerformRPCSpecificTarget("__RPCNetworkDestroy", GetPlayerEntry(0), m_BitStream, view->GetGroup());
	}
	else
	{
		BroadcastRPC ("__RPCNetworkDestroy", &m_BitStream, HIGH_PRIORITY, UNASSIGNED_SYSTEM_ADDRESS, NULL, view->GetGroup());
	}
}

void NetworkManager::RPCNetworkDestroy(RPCParameters *rpcParameters)
{
	NetworkManager& nm = GetNetworkManager();
	RakNet::BitStream stream (rpcParameters->input, BITS_TO_BYTES(rpcParameters->numberOfBitsOfData), false);

	NetworkViewID viewID;
	viewID.Read(stream);
	
	NetworkLog(NULL,"Network destroying view ID '%s'", viewID.ToString().c_str());
	
	NetworkView *view = nm.ViewIDToNetworkView(viewID);
	int group = 0;
	if (view)
	{
		group = view->GetGroup();
		Scripting::DestroyObjectFromScripting(&view->GetGameObject(), -1.0F);
	}
	else
		NetworkError (NULL, "Couldn't perform remote Network.Destroy because the network view '%s' could not be located.", viewID.ToString().c_str());
	
	stream.ResetReadPointer();
	
	// If running as server relay the message to all except owner
	if (nm.IsServer())
	{
		nm.BroadcastRPC ("__RPCNetworkDestroy", &stream, HIGH_PRIORITY, rpcParameters->sender, NULL, group);
	}
}

void NetworkManager::SetMessageQueueRunning(bool run)
{
	m_MessageQueueRunning = run;
}

void NetworkManager::RegisterRPC(const char* reg, void ( *functionPointer ) ( RPCParameters *rpcParms ))
{
	m_Peer->RegisterAsRemoteProcedureCall(const_cast<char*> (reg), functionPointer);
}

void NetworkManager::PerformRPC(const std::string &function, int mode, RakNet::BitStream& parameters, NetworkViewID viewID, UInt32 group)
{
	char* name = const_cast<char*>(function.c_str());
	RakNetTime time = GetTimestamp();

	// Also check for send permission on client, he can block the server if he wants to
	if (m_PeerType == kClient)
	{
		if (m_UseProxy && MaySendToPlayer(m_ServerAddress, group))
		{
			NetworkLog(NULL, "Performing proxied RPC '%s' to server %s", function.c_str(), m_ServerAddress.ToString());
			// Send to proxy, the server address is actually not used, the proxy knows what server the client is using
			if(!m_Peer->RPC(name, &parameters, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false, &time, UNASSIGNED_NETWORK_ID,0, (unsigned char) ID_PROXY_CLIENT_MESSAGE, m_ServerAddress))
			{
				NetworkError(NULL, "Couldn't send proxied RPC function '%s' to proxy server\n", name);
			}
		}
		// Send only to registered server address with no ordering, no timestamp and no reply
		else if(!m_Peer->RPC(name, &parameters, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ServerAddress, false, &time, UNASSIGNED_NETWORK_ID,0))
		{
			NetworkError(NULL, "Couldn't send RPC function '%s' to server\n", name);
		}
	}
	else if (m_PeerType == kServer)
	{
		BroadcastRPC(name, &parameters, HIGH_PRIORITY, UNASSIGNED_SYSTEM_ADDRESS, &time, group);
		
		NetworkInfo(NULL, "Sent RPC call '%s' to all connected clients\n", name);
		
		if (mode & kBufferRPCMask)
		{
			AssertIf(GetTargetMode(mode) != kOthers && GetTargetMode(mode) != kAll);
			//printf_console("DEBUG: Couldn't send RPC call: %s. Buffering call.\n", name);
			AddRPC(function, GetPlayerID(), viewID, group, parameters);
		}
	}
}

void NetworkManager::BroadcastRPC(const char* name, const RakNet::BitStream *parameters, PacketPriority priority, SystemAddress target, RakNetTime *includedTimestamp, UInt32 group )
{
	for (int i=0;i<m_Players.size();i++)
	{
		SystemAddress current = m_Players[i].playerAddress;
		if (current != target && MaySendToPlayer(current, group) && !m_Players[i].isDisconnected)
		{
			if (m_Players[i].relayed)
			{
				if (!m_Peer->RPC ((const char*)name, (const RakNet::BitStream*)parameters, priority, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false, includedTimestamp, UNASSIGNED_NETWORK_ID, NULL, (unsigned char) ID_PROXY_SERVER_MESSAGE,  m_Players[i].playerAddress))
				{
					NetworkError(NULL, "Couldn't send RPC function '%s' through proxy\n", name);
				}
			}
			else
			{
				if (!m_Peer->RPC ((const char*)name, (const RakNet::BitStream*)parameters, priority, RELIABLE_ORDERED, kDefaultChannel, current, false, includedTimestamp, UNASSIGNED_NETWORK_ID, NULL))
				{
					NetworkError(NULL, "Couldn't send RPC function '%s'\n", name);
				}
			}
		}
	}
}

void NetworkManager::PerformRPCSpecificTarget(char const* function, PlayerTable *player, RakNet::BitStream& parameters, UInt32 group)
{
	RakNetTime timestamp = GetTimestamp();
	
	if (MaySendToPlayer(player->playerAddress, group))
	{
		if (IsClient() && m_UseProxy)
		{
			NetworkLog(NULL, "Client sending specific target RPC '%s' to %s", function, player->playerAddress.ToString());
			if (!m_Peer->RPC(function, &parameters, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false, &timestamp, UNASSIGNED_NETWORK_ID, 0, (unsigned char) ID_PROXY_CLIENT_MESSAGE, player->playerAddress))
			{
				NetworkError(NULL, "Couldn't send RPC function '%s'\n", function);
			}
		}
		else if (IsServer() && player->relayed)
		{
			NetworkLog(NULL, "Server sending specific target RPC '%s' to %s", function, player->playerAddress.ToString());
			if (!m_Peer->RPC(function, &parameters, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false, &timestamp, UNASSIGNED_NETWORK_ID, 0, (unsigned char) ID_PROXY_SERVER_MESSAGE, player->playerAddress))
			{
				NetworkError(NULL, "Couldn't send RPC function '%s'\n", function);
			}
		}
		else
		{
			// Send only to registered server address with no ordering, no timestamp and no reply
			if (!m_Peer->RPC(function, &parameters, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, player->playerAddress, false, &timestamp, UNASSIGNED_NETWORK_ID,0))
			{
				NetworkError(NULL, "Couldn't send RPC function '%s'\n", function);
			}
		}
	}
}

void NetworkManager::PeformRPCRelayAll(char *name, int mode, NetworkViewID viewID, UInt32 group, RakNetTime timestamp, SystemAddress sender, RakNet::BitStream &stream)
{
	NetworkLog(NULL, "Relay RPC - name: %s - mode %d - sender %s", name, GetTargetMode(mode), sender.ToString());
	// Relay the RPC call to all other clients!
	if ( IsServer() && (GetTargetMode(mode) == kOthers || GetTargetMode(mode) == kAll))
	{
		BroadcastRPC(name, &stream, HIGH_PRIORITY, sender, &timestamp, group);
	}

	// Buffer the message if appropriate
	if (GetNetworkManager().IsServer() && (mode & kBufferRPCMask) != 0 )
	{
		AssertIf(GetTargetMode(mode) != kOthers && GetTargetMode(mode) != kAll);
		NetworkPlayer senderIndex = GetNetworkManager().GetIndexFromSystemAddress(sender);
		AddRPC(name, senderIndex, viewID, group, stream);
	}
}

void NetworkManager::PerformRPCRelaySpecific(char *name, RakNet::BitStream *stream, NetworkPlayer player)
{
	//SystemAddress address = GetSystemAddressFromIndex (player);
	PlayerTable *playerEntry = GetPlayerEntry (player);
	
	if (playerEntry == NULL)
	{
		NetworkError(NULL, "Couldn't relay RPC call '%s' because the player %d is not connected", name, player);
		return;
	}
	
	RakNetTime time = GetTimestamp();
	if (playerEntry->relayed)
	{
		NetworkLog(NULL, "Server sending proxied relay specific target RPC '%s' to %s", name, playerEntry->playerAddress.ToString());
		if (!m_Peer->RPC(name, stream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ProxyAddress, false, &time, UNASSIGNED_NETWORK_ID, 0, (unsigned char)ID_PROXY_SERVER_MESSAGE, playerEntry->playerAddress))
		{
			NetworkError(NULL, "Couldn't relay RPC call '%s'", name);
		}
	}
	else
	{
		if (!m_Peer->RPC(name, stream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, playerEntry->playerAddress, false, &time, UNASSIGNED_NETWORK_ID,0))
		{
			NetworkError(NULL, "Couldn't relay RPC call '%s'", name);
		}
	}
}


void NetworkManager::AddRPC(const std::string& name, NetworkPlayer sender, NetworkViewID viewID, UInt32 group, RakNet::BitStream& stream)
{
	RPCMsg msg;
	msg.name = name;
	msg.viewID = viewID;
	msg.sender = sender;
	msg.group = group;
	msg.stream = NULL;
	m_RPCBuffer.push_back(msg);
	m_RPCBuffer.back().stream = new RakNet::BitStream(stream.GetData(), stream.GetNumberOfBytesUsed(), true);
	NetworkInfo (NULL, "Added RPC '%s' to buffer.", name.c_str());
}

void NetworkManager::MsgRemoveRPCs()
{
	NetworkViewID viewID;
	int systemAddress;
	UInt32 groupMask; 
	m_BitStream.Reset();
	m_BitStream.Write((char*)m_Packet->data, m_Packet->length);
	m_BitStream.IgnoreBits(8);
	m_BitStream.Read(systemAddress);
	viewID.Read(m_BitStream);
	m_BitStream.Read(groupMask);
	RemoveRPCs(systemAddress, viewID, groupMask);
}

void NetworkManager::RemoveRPCs(NetworkPlayer playerIndex, NetworkViewID viewID, UInt32 groupMask)
{
	if (m_PeerType == kClient)
	{
		m_BitStream.Reset();
		m_BitStream.Write((unsigned char)ID_REMOVE_RPCS);
		m_BitStream.Write(playerIndex);
		viewID.Write(m_BitStream);
		m_BitStream.Write(groupMask);
		if (!m_Peer->Send(&m_BitStream, HIGH_PRIORITY, RELIABLE_ORDERED,  kDefaultChannel, m_ServerAddress, false))
		{
			NetworkError(NULL, "Failed to send remove RPCs command to network");
		}
		else
		{
			// @todo: more details
			NetworkInfo (NULL, "Sent remove RPCs player command to server");
		}
	}
	else
	{
		int erased = 0;
		RPCBuffer::iterator next = m_RPCBuffer.begin();
		for (RPCBuffer::iterator i=next;i != m_RPCBuffer.end();i=next)
		{
			RPCMsg& rpc = *i;
			next++;
			
			if (((1 << rpc.group) & groupMask) == 0)
				continue;
			if (rpc.viewID != viewID && viewID != NetworkViewID::GetUnassignedViewID())
				continue;
			if (rpc.sender != playerIndex && playerIndex != kUndefindedPlayerIndex)
				continue;
	
			NetworkInfo(NULL, "RPC %s with %s, player ID %d and group %d, removed from RPC buffer.", rpc.name.c_str(), rpc.viewID.ToString().c_str(), rpc.sender, rpc.group);
			delete rpc.stream;
			m_RPCBuffer.erase(i);
			erased++;
		}
		
		NetworkInfo (NULL, "%d RPC function were removed with RemoveRPC", erased);
	}
}

NetworkView* NetworkManager::ViewIDToNetworkView(const NetworkViewID& ID)
{
	NetworkView* view;
	NetworkViewIterator i;
	for (i = m_Sources.begin(); i != m_Sources.end(); i++) {
		view = i->GetData();
		if (view->GetViewID() == ID) {
			return view;
		}
	}

	for (i = m_NonSyncSources.begin(); i != m_NonSyncSources.end(); i++) {
		view = i->GetData();
		if (view->GetViewID() == ID) {
			return view;
		}
	}
	
	ErrorString(Format("View ID %s not found during lookup. Strange behaviour may occur", ID.ToString().c_str()));
	return NULL;
}


NetworkViewID NetworkManager::NetworkViewToViewID(NetworkView* view)
{
	if (view)
		return view->GetViewID();
	else
		return NetworkViewID();
}

int NetworkManager::GetValidInitIndex()
{
	int available = 0;
	while (available < m_UsedInitIndices.size() && m_UsedInitIndices[available])
		available++;
	
	if (available == m_UsedInitIndices.size())
	{
		m_UsedInitIndices.push_back(true);
		// Must set the scope or else a scope check later on will fail (the scope does not exist up until now).
		for (NetworkViewIterator i = m_AllSources.begin(); i != m_AllSources.end(); i++) {
			NetworkView* view = i->GetData();
			view->SetScope(available, true);		// By default all views are in scope
		}
		return m_UsedInitIndices.size() - 1;
	}
	else
	{
		m_UsedInitIndices[available] = true;

		// Reset the initial state variable for all network views
		NetworkView* view;
		NetworkViewIterator i;
		for (i = m_AllSources.begin(); i != m_AllSources.end(); i++) {
			view = i->GetData();
			view->SetInitState(available, false);
			view->SetScope(available, true);		// By default all views are in scope
		}

		return available;
	}
}

NetworkViewID NetworkManager::AllocateViewID()
{
	// Make sure we always have enough NetworkViewId's
	int requested = m_NetworkViewIDAllocator.ShouldRequestMoreBatches();
	if (requested != 0)
	{
		if (IsServer())
		{
			for (int i=0;i<requested;i++)
			{
				UInt32 batch = m_NetworkViewIDAllocator.AllocateBatch(m_PlayerID);
				m_NetworkViewIDAllocator.FeedAvailableBatchOnServer(batch);
			}
		}
		else if (IsClient())
		{
			m_NetworkViewIDAllocator.AddRequestedBatches(requested);
			for (int i=0;i<requested;i++)
			{
				RakNet::BitStream bitStream;
				if (!m_Peer->RPC("__RPCRequestViewIDBatch", &bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, kDefaultChannel, m_ServerAddress, false, NULL, UNASSIGNED_NETWORK_ID,0))
					ErrorString("Failed to request view id batch");
			}
		}
	}

	NetworkViewID viewID = m_NetworkViewIDAllocator.AllocateViewID();

	if (viewID == NetworkViewID())
		ErrorString("Failed to allocate view id because no NetworkView's were available to allocate from. You should increase the minimum client NetworkViewID count.");
	NetworkInfo(NULL, "Allocating view ID %s.\n", viewID.ToString().c_str());

	return viewID;
}

NetworkViewID NetworkManager::AllocateSceneViewID()
{
	UInt32 highestViewID = 0;
		for (NetworkViewIterator i=m_AllSources.begin();i != m_AllSources.end();i++)
		{
			NetworkView *view = i->GetData();
			if (view->GetViewID().IsSceneID())
				highestViewID = max(view->GetViewID().GetIndex(), highestViewID);
		}

	highestViewID++;

	NetworkViewID viewID;
	viewID.SetSceneID(highestViewID);
	
	return viewID;
}

NetworkViewID NetworkManager::ValidateSceneViewID(NetworkView* validateView, NetworkViewID viewID)
{
	// Find conflicting view ids
	bool isValidViewID = viewID.IsSceneID() && viewID.GetIndex() != 0;

	for (int s=0;s<2;s++)
	{
		NetworkViewList& list = s == 0 ? m_Sources : m_NonSyncSources;
		for (NetworkViewIterator i=list.begin();i != list.end();i++)
		{
			NetworkView *view = i->GetData();
			if (validateView != view)
			{
				if (viewID == view->GetViewID())
					isValidViewID = false;
			}
		}
	}
		
	if (!isValidViewID) {
		LogString(Format("Fixing invalid scene view ID %s", viewID.ToString().c_str()));
		viewID = AllocateSceneViewID();
	}
	
	return viewID;
}

bool NetworkManager::WasViewIdAllocatedByPlayer (NetworkViewID viewID, NetworkPlayer playerID)
{
	//AssertIf (!IsServer());
	return m_NetworkViewIDAllocator.FindOwner(viewID) == playerID;
}

bool NetworkManager::WasViewIdAllocatedByMe(NetworkViewID viewID)
{
	return m_NetworkViewIDAllocator.FindOwner(viewID) == GetPlayerID ();
}

NetworkPlayer NetworkManager::GetNetworkViewIDOwner(NetworkViewID viewID)
{
	return m_NetworkViewIDAllocator.FindOwner(viewID);
}

int NetworkManager::GetPlayerID()
{
	return m_PlayerID;
}

int NetworkManager::GetPeerType()
{
	return m_PeerType;
}

int NetworkManager::GetDebugLevel()
{
	return m_DebugLevel;
}

void NetworkManager::SetDebugLevel(int value)
{
	m_DebugLevel = value;
}

SystemAddress NetworkManager::GetPlayerAddress()
{
	return m_Peer->GetInternalID();
}

bool NetworkManager::IsClient()
{
	return m_PeerType == kClient;
}

bool NetworkManager::IsServer()
{
	return m_PeerType == kServer;
}

void NetworkManager::SetSimulation (NetworkSimulation simulation)
{
	switch (simulation)
	{
		case kBroadband:
			m_Peer->ApplyNetworkSimulator(1000000, 20, 0);
			break;
		case kDSL:
			m_Peer->ApplyNetworkSimulator(700000, 40, 0);
			break;
		case kISDN:
			m_Peer->ApplyNetworkSimulator(128000, 60, 0);
			break;
		case kDialUp:
			m_Peer->ApplyNetworkSimulator(56000, 150, 100);
			break;
		default:
			m_Peer->ApplyNetworkSimulator(0, 0, 0);
			break;
	}
}

void NetworkManager::MsgClientDidDisconnect()
{
	MsgClientDidDisconnect(m_Packet->systemAddress);
}

void NetworkManager::MsgClientDidDisconnect(SystemAddress clientAddress)
{
	int playerID = GetIndexFromSystemAddress(clientAddress);
	
	if (playerID == -1)
	{
		ErrorString("A client which was not in the connected player list disconnected. ???");
		return;
	}
	
	PlayerTable* player = GetPlayerEntry(playerID);
	player->isDisconnected = true;
	
	SendToAllNetworkViews(kPlayerDisconnected, playerID);

	for (PlayerAddresses::iterator i = m_Players.begin(); i != m_Players.end(); i++)
	{
		PlayerTable& pl = *i;
		if (clientAddress == pl.playerAddress)
		{
			if (pl.initIndex < m_UsedInitIndices.size())
				m_UsedInitIndices[pl.initIndex] = false;
			m_Players.erase(i);
			break;
		}
	}
}

void NetworkManager::SetIncomingPassword (const std::string& incomingPassword)
{
	const char* pass = NULL;
	if (!incomingPassword.empty())
		pass = incomingPassword.c_str();
	
	m_Peer->SetIncomingPassword(pass, incomingPassword.size());
}

std::string NetworkManager::GetIncomingPassword ()
{
	string password;
	int size = 0;
	m_Peer->GetIncomingPassword(NULL, &size);
	password.resize(size);
	m_Peer->GetIncomingPassword(const_cast<char*>(password.data()), &size);
	return password;
}

void NetworkManager::SetMaxConnections(int connections)
{
	if (connections == -1)
	{
		int currentConnectionCount = m_Players.size();
		m_Peer->SetMaximumIncomingConnections(currentConnectionCount);
		m_MaxConnections = currentConnectionCount;
	}
	else
	{
		m_Peer->SetMaximumIncomingConnections(connections);
		m_MaxConnections = connections;
	}
}

int NetworkManager::GetMaxConnections()
{
	return m_MaxConnections;	
	//return m_Peer->GetMaximumIncomingConnections();
}

int NetworkManager::GetConnectionCount()
{
	return m_Players.size();
}

PlayerTable* NetworkManager::GetPlayerEntry(SystemAddress playerAddress)
{
	for (PlayerAddresses::iterator i = m_Players.begin(); i != m_Players.end(); i++)
	{
		PlayerTable& player = *i;
		if (playerAddress == player.playerAddress)
		{
			return &player;
		}
	}
	return NULL;
}

PlayerTable* NetworkManager::GetPlayerEntry(NetworkPlayer index)
{
	for (PlayerAddresses::iterator i = m_Players.begin(); i != m_Players.end(); i++)
	{
		PlayerTable& player = *i;
		if (index == player.playerIndex)
		{
			return &player;
		}
	}
	return NULL;
}

SystemAddress NetworkManager::GetSystemAddressFromIndex(NetworkPlayer playerIndex)
{
	for (PlayerAddresses::iterator i = m_Players.begin(); i != m_Players.end(); i++)
	{
		PlayerTable& player = *i;
		if (playerIndex == player.playerIndex)
		{
			return player.playerAddress;
		}
	}
	return UNASSIGNED_SYSTEM_ADDRESS;
}

int NetworkManager::GetIndexFromSystemAddress(SystemAddress playerAddress)
{
	for (PlayerAddresses::iterator i = m_Players.begin(); i != m_Players.end(); i++)
	{
		PlayerTable& player = *i;
		if (playerAddress == player.playerAddress)
		{
			return player.playerIndex;
		}
	}
	return -1;
}

std::vector<PlayerTable> NetworkManager::GetPlayerAddresses()
{
	return m_Players;
}


bool NetworkManager::IsPasswordProtected()
{
	int size = 0;
	m_Peer->GetIncomingPassword(NULL, &size);
	if (size == 0)
		return false;
	else
		return true;
}

// NOTE: This returns only the internal address, in the case of a NATed address we do not know which port will open on the outside
// and thus cannot know our external playerID
std::string NetworkManager::GetIPAddress()
{
	if (m_Peer->IsActive())
		return m_Peer->GetInternalID().ToString(false);
	else
		return string();
}

std::string NetworkManager::GetExternalIP()
{
	return std::string(m_Peer->GetExternalID(UNASSIGNED_SYSTEM_ADDRESS).ToString(false));
}

int NetworkManager::GetExternalPort()
{
	return m_Peer->GetExternalID(UNASSIGNED_SYSTEM_ADDRESS).port;
}

std::string NetworkManager::GetIPAddress(int player)
{
	if (player == -2 && IsServer() && m_UseProxy)
	{
		return m_ProxyAddress.ToString(false);
	}
	// NOTE: It will not work running this on clients as the client only has the server index number, no other numbers
	SystemAddress address = GetSystemAddressFromIndex(player);
	if (address != UNASSIGNED_SYSTEM_ADDRESS)
		return address.ToString(false);
	else
		return string ();
}

int NetworkManager::GetPort()
{
	if (!m_Peer->IsActive())
		return 0;
	else
		return m_Peer->GetInternalID().port;
}

int NetworkManager::GetPort(int player)
{
	if (player == -2 && IsServer() && m_UseProxy)
	{
		return m_RelayPort;
	}
	SystemAddress address = GetSystemAddressFromIndex(player);
	if (address != UNASSIGNED_SYSTEM_ADDRESS)
		return address.port;
	else
		return 0;
}

std::string NetworkManager::GetGUID()
{
	return m_Peer->GetGuidFromSystemAddress(UNASSIGNED_SYSTEM_ADDRESS).ToString();
}

std::string NetworkManager::GetGUID(int player)
{
	if (player == -2 && IsServer() && m_UseProxy)
		return m_Peer->GetGuidFromSystemAddress(m_ProxyAddress).ToString();
	PlayerTable *playerTable = GetPlayerEntry(player);
	if (playerTable)
		return playerTable->guid;
	else
		return "0";
}

void NetworkManager::GetConnections(int* connection)
{
	for (int i=0;i<m_Players.size();i++)
	{
		PlayerTable& player = m_Players[i];
		connection[i] = player.playerIndex;
	}
}

bool NetworkManager::GetUseNat()
{
	return m_DoNAT;
}

void NetworkManager::SetUseNat(bool enabled)
{
	if (m_DoNAT != enabled)
	{
		m_DoNAT = enabled;
		if (m_DoNAT)
			m_Peer->AttachPlugin(&m_NatPunchthrough);
		else
			m_Peer->DetachPlugin(&m_NatPunchthrough);
	}
}

bool NetworkManager::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}



/*
float NetworkManager::GetTimeout ()
{
	return m_TimeoutTime;
}

void NetworkManager::SetTimeout (float timeout)
{
	m_TimeoutTime = timeout;
	SetTimeoutTime();
}
*/
int NetworkManager::GetLastPing (NetworkPlayer player)
{
	return m_Peer->GetLastPing(GetSystemAddressFromIndex(player));
}

int NetworkManager::GetAveragePing (NetworkPlayer player)
{
	return m_Peer->GetLastPing(GetSystemAddressFromIndex(player));
}

static void GetSetNetworkViewIDs (Transform& obj, NetworkViewID*& ids, SInt32& netViewCount, bool assign);
static void GetSetNetworkViewIDs (Transform& obj, NetworkViewID*& ids, SInt32& netViewCount, bool assign)
{
	GameObject& go = obj.GetGameObject();
	int count = go.GetComponentCount ();
	for (int i=0;i<count;i++)
	{
		NetworkView* view = dynamic_pptr_cast<NetworkView*> (&go.GetComponentAtIndex(i));
		if (view)
		{
			if (assign)
			{
				if (netViewCount <= 0)
				{
					netViewCount = -1;
					return;
				}
				
				view->SetViewID(ids[0]);
				ids++;
				netViewCount--;		
			}
			else
			{
				netViewCount++;
			}
		}
	}
	
	for (int i=0;i<obj.GetChildrenCount();i++)	
	{
		Transform& child = obj.GetChild(i);
		GetSetNetworkViewIDs(child, ids, netViewCount, assign);
	}
}

Object* NetworkManager::Instantiate (Object& prefab, Vector3f pos, Quaternionf rot, UInt32 group)
{
	if (!IsConnected())
	{
		NetworkError(NULL, "Failed Network.Instantiate because we are not connected.");
		return NULL;
	}

	// Grab the prefab game object
	GameObject* prefabGO = dynamic_pptr_cast<GameObject*> (&prefab);
	Unity::Component* prefabCom = dynamic_pptr_cast<Unity::Component*> (&prefab);
	if (prefabCom != NULL)
		prefabGO = prefabCom->GetGameObjectPtr();
	
	if (prefabGO == NULL)
	{
		NetworkError(NULL, "The prefab '%s' reference must be a component or game object.", prefab.GetName());
		return NULL;
	}
	
	// Get the UnityGUID of the prefab
	#if UNITY_EDITOR
	UnityGUID guid = ObjectToGUID(prefabGO);

	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset == NULL)
	{
		NetworkError(NULL, "The object %s must be a prefab in the project view.", prefab.GetName());
		return NULL;
	}

	Object* mainRep = asset->mainRepresentation.object;
	if (mainRep != prefabGO)
	{
		NetworkError(NULL, "The object is not the main asset. Only root objecs of a prefab may be used for Instantiating");
		return NULL;
	}
		
	#else
	PrefabToAsset::iterator found = m_PrefabToAsset.find(PPtr<GameObject> (prefabGO));
	if (found == m_PrefabToAsset.end())
	{
		NetworkError(NULL, "The object %s must be a prefab in the project view.", prefab.GetName());
		return NULL;
	}
	UnityGUID guid = found->second;
	#endif

	// Get the component index
	UInt8 componentIndex = 0xFF;
	if (prefabCom)
	{
		AssertIf(prefabCom->GetGameObject().GetComponentIndex(prefabCom) >= std::numeric_limits<UInt8>::max());
		componentIndex = prefabGO->GetComponentIndex(prefabCom);
	}
	
	// Allocate view ids for all network views and assign them
	SInt32 networkViewCount = 0;
	NetworkViewID* viewIDs = NULL;
	GetSetNetworkViewIDs (prefabGO->GetComponent(Transform), viewIDs, networkViewCount, false);
	ALLOC_TEMP(viewIDs, NetworkViewID, networkViewCount)
	
	for (int i=0;i<networkViewCount;i++)
		viewIDs[i] = AllocateViewID();
		
	/// Send message to everyone
	RakNet::BitStream stream;
	BitstreamPacker packer (stream, false);
	packer.Serialize(group);
	packer.Serialize(guid.data[0]);
	packer.Serialize(guid.data[1]);
	packer.Serialize(guid.data[2]);
	packer.Serialize(guid.data[3]);
	packer.Serialize(componentIndex);
	packer.Serialize(pos);
	packer.Serialize(rot);
	packer.Serialize(networkViewCount);
	for (int i=0;i<networkViewCount;i++)
		packer.Serialize(viewIDs[i]);
		
	/// Send/Buffer RPC on to other machines
	NetworkViewID sourceViewID = NetworkViewID();
	if (networkViewCount > 0)
		sourceViewID = viewIDs[0];
	PerformRPC("__RPCNetworkInstantiate", kAll | kBufferRPCMask, stream, sourceViewID, group);
	
	stream.ResetReadPointer();
	return NetworkInstantiateImpl(stream, GetPlayerAddress(), GetTimestamp());
}

static void RecursiveOnNetworkInstantiate (Transform& obj, RakNetTime time, SystemAddress sender)
{
	GameObject& go = obj.GetGameObject();
	int count = go.GetComponentCount ();
	RakNet::BitStream stream;
	for (int i=0;i<count;i++)
	{
		MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&go.GetComponentAtIndex(i));
		if (behaviour)
		{
			if (!behaviour->GetInstance())
			{
				NetworkError(&go, "Network instantiated object, %s, has a missing script component attached", go.GetName());
			}
			else
			{
				ScriptingMethodPtr method = behaviour->GetMethod(MonoScriptCache::kNetworkInstantiate);
				if (method)
				{
					int readoffset = stream.GetReadOffset();
					UnpackAndInvokeRPCMethod(*behaviour, method->monoMethod, stream, sender, NetworkViewID(), time, NULL);	
					stream.SetReadOffset(readoffset);
				}
			}
		}
	}
	
	for (int i=0;i<obj.GetChildrenCount();i++)	
	{
		Transform& child = obj.GetChild(i);
		RecursiveOnNetworkInstantiate(child, time, sender);
	}
}

PROFILER_INFORMATION(gInstantiateProfile, "Network.Instantiate", kProfilerNetwork)

Object* NetworkManager::NetworkInstantiateImpl (RakNet::BitStream& bitstream, SystemAddress sender, RakNetTime time)
{
	PROFILER_AUTO(gInstantiateProfile, NULL);
	
	SInt32 networkViewCount = 0;
	NetworkViewID* viewIDs = NULL;
	UnityGUID guid;
	UInt32 group;
	Vector3f pos;
	Quaternionf rot;
	UInt8 componentIndex;
	// Read Instantiate data	
	BitstreamPacker packer (bitstream, true);
	packer.Serialize(group);
	packer.Serialize(guid.data[0]);
	packer.Serialize(guid.data[1]);
	packer.Serialize(guid.data[2]);
	packer.Serialize(guid.data[3]);
	packer.Serialize(componentIndex);
	packer.Serialize(pos);
	packer.Serialize(rot);
	packer.Serialize(networkViewCount);
	ALLOC_TEMP(viewIDs, NetworkViewID, networkViewCount)
	for (int i=0;i<networkViewCount;i++)
		packer.Serialize(viewIDs[i]);

	/// Find the asset
	#if UNITY_EDITOR
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset == NULL)
	{
		NetworkError(NULL, "Network.Instantiate on the receiving client failed because the asset couldn't be found in the project");
		return NULL;
	}

	GameObject* prefabGO = dynamic_pptr_cast<GameObject*> (asset->mainRepresentation.object);		
	#else
	AssetToPrefab::iterator found = m_AssetToPrefab.find(guid);
	if (found == m_AssetToPrefab.end())
	{
		NetworkError(NULL, "Network.Instantiate on the receiving client failed because the asset couldn't be found in the project");
		return NULL;
	}
	GameObject* prefabGO = found->second;	
	#endif

	// Make sure we got the game object
	if (prefabGO == NULL)
	{
		NetworkError(NULL, "Network.Instantiate sent component but found asset is not a prefab.");
		return NULL;
	}
	
	// Get the right component	
	Object* prefab = prefabGO;
	if (componentIndex != 0xFF)
	{
		if (componentIndex >= prefabGO->GetComponentCount())
		{
			NetworkError(NULL, "Network.Instantiate component index is out of bounds.");
			return NULL;
		}

		prefab = &prefabGO->GetComponentAtIndex(componentIndex);
	}
	
	// Instantiate the object
	TempRemapTable ptrs;
	Object& clone = InstantiateObject(*prefab, pos, rot, ptrs);

	Transform* cloneTransform = NULL;
	GameObject* cloneGO = dynamic_pptr_cast<GameObject*> (&clone);
	Unity::Component* cloneComponent = dynamic_pptr_cast<Unity::Component*> (&clone);
	if (cloneGO)
		cloneTransform = cloneGO->QueryComponent(Transform);
	if (cloneComponent)
		cloneTransform = cloneComponent->QueryComponent(Transform);
	AssertIf(!cloneTransform);
	
	// Assign view ids for all network views
	GetSetNetworkViewIDs(*cloneTransform, viewIDs, networkViewCount, true);
	if (networkViewCount != -0)
	{
		NetworkError(NULL, "Network.Instantiate received non-matching number of view id's as contained in prefab");
	}
	
	AwakeAndActivateClonedObjects(ptrs);

	// Call network instantiate function
	RecursiveOnNetworkInstantiate(*cloneTransform, time, sender);
	
	return &clone;
}

void NetworkManager::RPCNetworkInstantiate (RPCParameters* rpcParameters)
{
	NetworkManager& nm = GetNetworkManager();
	RakNet::BitStream stream (rpcParameters->input, BITS_TO_BYTES(rpcParameters->numberOfBitsOfData), false);
	
	UInt32 group = 0;
	stream.Read(group);
	
	// Should we be receiving this at all?
	if (!nm.MayReceiveFromPlayer(rpcParameters->sender, group))
	{
		NetworkInfo (NULL, "Network.Instantiate was ignored since the group of the network view is disabled.");
		return;
	}

	stream.ResetReadPointer();
	nm.NetworkInstantiateImpl (stream, rpcParameters->sender, rpcParameters->remoteTimestamp);

	stream.ResetReadPointer();
	nm.PeformRPCRelayAll(rpcParameters->functionName, kAll | kBufferRPCMask, NetworkViewID(), group, rpcParameters->remoteTimestamp, rpcParameters->sender, stream);
}

void NetworkManager::SetLevelPrefix(int levelPrefix)
{
	m_LevelPrefix = levelPrefix;
}

IMPLEMENT_CLASS_HAS_INIT (NetworkManager)
IMPLEMENT_OBJECT_SERIALIZE (NetworkManager)
GET_MANAGER (NetworkManager)
GET_MANAGER_PTR (NetworkManager)

void NetworkManager::InitializeClass ()
{
	#if UNITY_EDITOR
	RegisterAllowNameConversion ("NetworkViewID", "m_SceneID", "m_ID");
	#endif
}

void NetworkManager::CleanupClass ()
{
}

bool NetworkManager::MaySend(  int group )
{
	return m_SendingEnabled & (1 << group);
}

// Shouldn't errors always be shown???
void NetworkError (Object* obj, const char* format, ...)
{
	if (GetNetworkManager().GetDebugLevel() >= kImportantErrors)
	{
		va_list va;
		va_start( va, format );
		ErrorStringObject(VFormat(format, va), obj);
	}
}


void NetworkWarning (Object* obj, const char* format, ...)
{
	if (GetNetworkManager().GetDebugLevel() >= kImportantErrors)
	{
		va_list va;
		va_start( va, format );
		ErrorStringObject(VFormat(format, va), obj);
	}
}

void NetworkInfo (Object* obj, const char* format, ...)
{
	if (GetNetworkManager().GetDebugLevel() >= kInformational)
	{
		va_list va;
		va_start( va, format );
		LogStringObject(VFormat(format, va), obj);
	}
}

void NetworkLog (Object* obj, const char* format, ...)
{
	if (GetNetworkManager().GetDebugLevel() >= kCompleteLog)
	{
		va_list va;
		va_start( va, format );
		LogStringObject(VFormat(format, va), obj);
	}
}

RakNetTime NetworkManager::GetTimestamp()
{
	return RakNet::GetTime();
}

double NetworkManager::GetTime()
{
	return TimestampToSeconds(RakNet::GetTime());
}

RakPeerInterface* NetworkManager::GetPeer()
{
	return m_Peer;
}

void NetworkManager::SwapFacilitatorID(SystemAddress newAddress)
{
	m_Peer->CloseConnection(m_FacilitatorID, true);
	m_OldFacilitatorID = m_FacilitatorID;
	m_FacilitatorID = newAddress;
	if (m_Peer->IsConnected(m_OldFacilitatorID))
	{
		if (!m_Peer->Connect(newAddress.ToString(false), newAddress.port, 0, 0))
			ErrorString("Internal problem connecting to new facilitator address");
	}
}

void NetworkManager::SetOldMasterServerAddress(SystemAddress address)
{
	m_OldMasterServerID = address;
}

void NetworkManager::SetConnTesterAddress(SystemAddress address)
{
	m_ConnTesterAddress = address;
	if (!m_ConnTester)
			m_ConnTester = new ConnectionTester(m_ConnTesterAddress);
	m_ConnTester->SetAddress(address);
	
	// Reset test
	m_ConnStatus = kConnTestUndetermined;
}

int NetworkManager::TestConnection(bool forceNATType, bool forceTest)
{
	// If the test is undetermined or if a test is forced -> it's OK to recreate tester and/or reenter test function
	// If there is a kConnTestError then the test must be forced to get a new result
	if ( m_ConnStatus == kConnTestUndetermined || forceTest)
	{
		if (!m_ConnTester)
			m_ConnTester = new ConnectionTester(m_ConnTesterAddress);
		m_ConnStatus = m_ConnTester->RunTest(forceNATType);
	}
	return m_ConnStatus;
}

void NetworkManager::PingWrapper(Ping *time)
{		
	time->Retain();
	
	// If the thread is not running then execute ping now, else put it in queue
	if (!m_PingThread.IsRunning())
		m_PingThread.Run(&PingImpl, (void*)time);
	else
		m_PingQueue.push(time);
}

void NetworkManager::ResolveFacilitatorAddress()
{
	ResolveAddress(m_FacilitatorID, "facilitator.unity3d.com", "facilitatorbeta.unity3d.com", 
		"Cannot resolve facilitator address, make sure you are connected to the internet before connecting to a server with NAT punchthrough enabled");
}

SystemAddress& NetworkManager::GetFacilitatorAddress(bool resolve) 
{
	if (resolve)
		ResolveFacilitatorAddress();

	return m_FacilitatorID;
}


/*	The connection tester.
	m_ConnStatus indicates the connection test results:
		-2 error
		-1 undetermined, test not completed
		 0 No NAT punchthrough capability
		 1 NAT puConnectionTesternchthrough test successful
		 2 Public IP test successful, listen port directly connectable
		 3 Public IP test unsuccessful, tester unable to connect to listen port (firewall blocking access)
		 4 Public IP test unsuccessful because no port is listening (server not initialized)
	m_TestRunning indicates type of test:
		 0 No test running
		 1 NAT test running
		 2 Public IP test running

	The only reason m_TestRunning needs to display seperate test types is because we need to be able to interpret what
	has happened when a successful connect has occured. I.e. did NAT punchthrough connect work or did we just connect to
	the tester server do start a public IP connectivity test.
*/
ConnectionTester::ConnectionTester(SystemAddress& address) 
{	
	ResolveAddress(address, "connectiontester.unity3d.com", "connectiontesterbeta.unity3d.com",
		"Cannot resolve connection tester address, you must be connected to the internet before performing this or set the address to something accessible to you.");

	m_ConnTesterAddress = address;
	m_Peer = RakNetworkFactory::GetRakPeerInterface();
	m_NatTypeDetection = new RakNet::NatTypeDetectionClient;
	m_Peer->AttachPlugin(m_NatTypeDetection);
	m_ConnStatus = kConnTestUndetermined;
	m_TestRunning = 0;
	#if PACKET_LOGGER
	m_Peer->AttachPlugin(&messageHandler2);
	messageHandler2.LogHeader();
	#endif
}
	
ConnectionTester::~ConnectionTester()
{
	m_Peer->DetachPlugin(m_NatTypeDetection);
	delete m_NatTypeDetection;
	m_NatTypeDetection = NULL;
	RakNetworkFactory::DestroyRakPeerInterface(m_Peer);
	m_Peer = NULL;
}

void ConnectionTester::SetAddress(SystemAddress address)
{
	m_ConnTesterAddress = address;
	FinishTest(kConnTestUndetermined);
}

// TODO: At the moment -2 is returned on all errors, -1 on undetermined and 0 or 1 on success or failure. Maybe there
// should be seperate error numbers for each kind of error and "less than -1" indicates and error
int ConnectionTester::Update()
{
	// Check for timeout, if it has expired some problem has occured
	if (m_TestRunning > 0 && time(0) - m_Timestamp > kConnTestTimeout)
	{
		LogString("Timeout during connection test");
		m_TestRunning = 0;
		return kConnTestError;
	}
	
	if (!m_Peer->IsActive())
		return m_ConnStatus;
		
	if (m_TestRunning > 0)
	{
		Packet* packet;
		packet = m_Peer->Receive();
		while (packet)
		{
			switch(packet->data[0])
			{
				case ID_CONNECTION_REQUEST_ACCEPTED:
					StartTest();
					break;
				case ID_NO_FREE_INCOMING_CONNECTIONS:
					NetworkError(NULL, "The connection tester is not accepting new connections, test finished.");
					FinishTest();
					break;
				case ID_CONNECTION_ATTEMPT_FAILED:
					NetworkInfo(NULL, "Failed to connect to connection tester at %s", packet->systemAddress.ToString());
					FinishTest();
					break;
				case ID_CONNECTION_BANNED:
					NetworkInfo(NULL, "The connection tester has banned this connection.");
					FinishTest();
					break;
				case ID_DISCONNECTION_NOTIFICATION:
					NetworkInfo(NULL, "Disconnected from connection tester.");
					FinishTest();
					break;
				case ID_CONNECTION_LOST:
					NetworkError(NULL, "Lost connection to connection tester.");
					FinishTest();
					break;
				case ID_NAT_TYPE_DETECTION_RESULT:
					{
						// TODO: We know certain types might not be able to connect to another one. How
						// should this be exposed... It's not as straight forward as saying NAT punchthrough
						// capable or not capable. Maybe expose Network.CanConnect(type1,type2).
						RakNet::NATTypeDetectionResult r = (RakNet::NATTypeDetectionResult) packet->data[1];
						// TODO: Now check if router is UPNP compatible
						switch (r)
						{
							case RakNet::NAT_TYPE_PORT_RESTRICTED: m_ConnStatus = kLimitedNATPunchthroughPortRestricted; break;
							case RakNet::NAT_TYPE_SYMMETRIC: m_ConnStatus = kLimitedNATPunchthroughSymmetric; break;
							case RakNet::NAT_TYPE_FULL_CONE: m_ConnStatus = kNATpunchthroughFullCone; break;
							case RakNet::NAT_TYPE_ADDRESS_RESTRICTED: m_ConnStatus = kNATpunchthroughAddressRestrictedCone; break;
							case RakNet::NAT_TYPE_NONE: m_ConnStatus = kPublicIPIsConnectable; break;
							default: 
								m_ConnStatus = kConnTestError; 
								NetworkError(NULL, "Connection Tester returned invalid NAT type.");
								break;
						}
						m_TestRunning = 0;
						m_Peer->Shutdown(kDefaultTimeout);
					}
					break;
				// Public IP test: tester is reporting that the couln't connect to the game server IP and port, test unsuccessful
				case 254:
					FinishTest(kPublicIPPortBlocked);
					break;
				case ID_ALREADY_CONNECTED:
					NetworkInfo(NULL, "Already connected to connection tester, attempting to trigger new test.");
					StartTest();
					break;
				default:
					NetworkInfo(NULL, "Received invalid message type %d from connection tester at %s", (int)packet->data[0], packet->systemAddress.ToString());
					break;
			}
			m_Peer->DeallocatePacket(packet);
			packet = m_Peer->Receive();
		}
	}
	return m_ConnStatus;
}

void ConnectionTester::StartTest()
{
	// NAT test: successfully connected with NAT punchthrough
	if (m_TestRunning == 1)
	{
		NetworkInfo(NULL, "Starting NAT connection test.");
		m_NatTypeDetection->DetectNATType(m_ConnTesterAddress);
	}
	// Public IP test: connected with tester so send him the connect-to-me command
	else
	{
		RakNet::BitStream bitStream;
		bitStream.Write((unsigned char)253);
		bitStream.Write(GetNetworkManager().GetPort());
		m_Peer->Send(&bitStream, HIGH_PRIORITY, RELIABLE, 0, m_ConnTesterAddress, false);
		NetworkInfo(NULL, "Connection Tester requesting test on external IP and port %d", GetNetworkManager().GetPort());
	}
}

void ConnectionTester::ReportTestSucceeded()
{
	FinishTest(kPublicIPIsConnectable);
}

void ConnectionTester::FinishTest(int status)
{
	m_ConnStatus = status;
	m_TestRunning = 0;
	m_Peer->Shutdown(kDefaultTimeout);
}
	
int ConnectionTester::RunTest(bool forceNATType)
{
	// If a problem occured the timeout must expire before anything is done. See below
	if ( m_ConnStatus > kConnTestError && !(m_ConnStatus == kConnTestUndetermined && m_TestRunning > 0) )
	{
		// If test has already been triggered, stop here
		if (m_TestRunning > 0)
			return kConnTestUndetermined;
		
		// First check if the peer interface has been initialized
		if (!m_Peer->IsActive())
		{
			SocketDescriptor sd(0,0);
			if (!m_Peer->Startup(2, 1, &sd, 1))
			{
				ErrorString("Failed to initialize network connection before NAT test.");
				return kConnTestError;
			}
		}
		
		// Set the timer, this ensures this instance will be destroyed after the timeout
		m_Timestamp = time(0);
		
		// There are two kinds of tests, one for public IP addresses and one for private addresses, either one is executed
		if (CheckForPublicAddress() && !forceNATType)
		{
			NetworkInfo(NULL, "Starting public address connection test.");
			if (GetNetworkManager().IsServer())
			{
				if (!m_Peer->Connect(m_ConnTesterAddress.ToString(false),m_ConnTesterAddress.port,0,0))
				{
					ErrorString("Failed to connect the connection tester.");
					return kConnTestError;
				}
				m_TestRunning = 2;
			}
			else
			{
				FinishTest(kPublicIPNoServerStarted);
				return m_ConnStatus;
			}
		}
		else
		{
			if (!m_Peer->IsConnected(m_ConnTesterAddress))
			{
				NetworkInfo(NULL, "Connecting to connection tester at %s", m_ConnTesterAddress.ToString());
				if (!m_Peer->Connect(m_ConnTesterAddress.ToString(false),m_ConnTesterAddress.port,0,0))
				{
					ErrorString("Failed to connect to connection tester during NAT test.");
					return kConnTestError;
				}
			}

			m_TestRunning = 1;
		}
		
		return m_ConnStatus = kConnTestUndetermined;
	}
	// If there is a problem (-2) and the timeout is expired you may try again (reset test and recall RunTest())
	else if (m_ConnStatus == kConnTestError && (time(0) - m_Timestamp > kConnTestTimeout))
	{
		m_Timestamp = time(0);
		m_ConnStatus = kConnTestUndetermined;
		return RunTest(forceNATType);
	}

	return m_ConnStatus;
}

template<class TransferFunc>
void NetworkManager::Transfer (TransferFunc& transfer) {

	AssertIf(transfer.GetFlags() & kPerformUnloadDependencyTracking);

	Super::Transfer (transfer);
	TRANSFER(m_DebugLevel);
	TRANSFER(m_Sendrate);
	transfer.Transfer(m_AssetToPrefab, "m_AssetToPrefab", kHideInEditorMask);	
}

#endif // ENABLE_NETWORK
