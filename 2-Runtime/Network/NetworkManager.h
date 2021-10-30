#ifndef UNITY_NETWORK_MANAGER_H_
#define UNITY_NETWORK_MANAGER_H_

#include "Configuration/UnityConfigure.h"

enum NetworkReachability
{
	NotReachable = 0,
	ReachableViaCarrierDataNetwork,
	ReachableViaLocalAreaNetwork,
};

NetworkReachability GetInternetReachability ();

#if ENABLE_NETWORK

#include "Runtime/BaseClasses/GameManager.h"
#include <set>
#include <list>
#include <queue>
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/GUID.h"

#include "NetworkEnums.h"
#include "NetworkView.h"
#include "Runtime/Graphics/Transform.h"
#include "External/RakNet/builds/include/GetTime.h"
#include "External/RakNet/builds/include/MessageIdentifiers.h"
#include "External/RakNet/builds/include/BitStream.h"
#include "External/RakNet/builds/include/RakNetTypes.h"
#include "External/RakNet/builds/include/NatPunchthroughClient.h"
#include "External/RakNet/builds/include/NatTypeDetectionClient.h"
#include "NetworkViewIDAllocator.h"
#include <time.h>
#include "Runtime/Threads/Thread.h"
#include "NetworkUtility.h"


//class RakPeer;

class ConnectionTester
{
	public:
	ConnectionTester(SystemAddress& testerAddress);
	~ConnectionTester();
	void SetAddress(SystemAddress address);
	int Update();
	int RunTest(bool forceNATType);
	void ReportTestSucceeded();
		
	private:
	void StartTest();
	void FinishTest(int status = kConnTestError);
	int m_ConnStatus;
	int m_TestRunning; 
	RakPeerInterface* m_Peer;
	NatPunchthroughClient m_NATPunchthrough;
	RakNet::NatTypeDetectionClient *m_NatTypeDetection;
	SystemAddress m_ConnTesterAddress;
	time_t m_Timestamp;
};



class NetworkManager : public GlobalGameManager
{
public:
	typedef std::list<RPCMsg> RPCBuffer;
	typedef std::vector<PlayerTable> PlayerAddresses;
	typedef std::map<UnityGUID, PPtr<GameObject> > AssetToPrefab;
	typedef std::map<PPtr<GameObject>, UnityGUID > PrefabToAsset;
	typedef std::queue<Ping*> PingQueue;

	REGISTER_DERIVED_CLASS   (NetworkManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (NetworkManager)

	static void InitializeClass ();
	static void CleanupClass ();

	NetworkManager (MemLabelId label, ObjectCreationMode mode);
	// virtual ~NetworkManager (); declared-by-macro

	/// Virtual API exposed in GameManager to avoid direct depdendencies to the network layer
	virtual void NetworkUpdate ();	
	virtual void NetworkOnApplicationQuit();

	void AddNetworkView (ListNode<NetworkView>& s);
	void AddNonSyncNetworkView (ListNode<NetworkView>& s);
	void AddAllNetworkView (ListNode<NetworkView>& s);
	bool ShouldIgnoreInGarbageDependencyTracking ();
	
	// Destroy locally and broadcast destroy message to peers (or just server)
	void DestroyDelayed(NetworkViewID viewID);
	
	// Destory all objects which belong the the given player locally and remotely and
	// possibly remove all RPC calls currently in server RPC buffer.
	void DestroyPlayerObjects(NetworkPlayer playerID);

	void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	int InitializeServer(int connections, int listenPort, bool useNat);
	void InitializeSecurity();
	
	int Connect(std::string IP, int remotePort, int listenPort, const std::string& password);
	int Connect(std::string IP, int remotePort, const std::string& password);
	int Connect(std::vector<string> IPs, int remotePort, int listenPort, const std::string& password);
	int Connect(RakNetGUID serverGUID, int listenPort, const std::string& password);
	void Disconnect(int timeout, bool resetParams = true);
	void CloseConnection(int target, bool sendDisconnect);

	NetworkView* ViewIDToNetworkView (const NetworkViewID& ID);
	NetworkViewID NetworkViewToViewID(NetworkView* view);

	void LoadLevel(const std::string& level);

	void SetIncomingPassword (const std::string& incomingPassword);
	std::string GetIncomingPassword ();
	
	void RegisterRPC(const char* reg, void ( *functionPointer ) ( RPCParameters *rpcParms ));
	void BroadcastRPC(const char* name, const RakNet::BitStream *parameters, PacketPriority priority, SystemAddress target, RakNetTime *includedTimestamp, UInt32 group);
	
	// Send an RPC call to every connected peer except the owner of the call (systemAddress)
	void PerformRPC(const std::string &function, int mode, RakNet::BitStream& parameters, NetworkViewID viewID, UInt32 group);
	void PerformRPCSpecificTarget(char const* function, PlayerTable *player, RakNet::BitStream& parameters, UInt32 group);

	// Perform RPC to every connected client except systemAddress
	void PeformRPCRelayAll(char *name, int mode, NetworkViewID viewID, UInt32 group, RakNetTime timestamp, SystemAddress sender, RakNet::BitStream &stream);
	void PerformRPCRelaySpecific(char *name, RakNet::BitStream *stream, NetworkPlayer player);
	void AddRPC(const std::string& name, NetworkPlayer sender, NetworkViewID viewID, UInt32 group, RakNet::BitStream& stream);

	// This function must give out unique ID numbers per game. Getting a view ID automatically registers it.
	// The editor must always "run" as a server so he will get the correct prefix (no prefix),
	// then after play has been pressed the proper type is selected (server or client).
	// m_playerID, which is used as a prefix on clients, will be updated to a proper value by the server after the connect.
	NetworkViewID AllocateViewID();
	// This allocate routine is for use in the editor, here we don't want any prefix and the
	// number is decremented with each new ID (opposed to incrementing it like AllocateViewID does).
	NetworkViewID AllocateSceneViewID();
	NetworkViewID ValidateSceneViewID(NetworkView* validateView, NetworkViewID viewID);
	
	
	void SetLevelPrefix(int levelPrefix);
	int GetLevelPrefix() { return m_LevelPrefix; }
		
	int GetPlayerID();

	int GetPeerType();
	int GetDebugLevel();
	void SetDebugLevel(int value);

	// Get this players address
	SystemAddress GetPlayerAddress();

	// Lookup a player ID number in the player address table
	/// UNASSIGNED_SYSTEM_ADDRESS if can't be found
	SystemAddress GetSystemAddressFromIndex(NetworkPlayer playerIndex);

	/// the player index for the adress.
	// -1 if can't be found. Server is always 0.
	int GetIndexFromSystemAddress(SystemAddress playerAddress);

	PlayerTable* GetPlayerEntry(SystemAddress playerAddress);
	PlayerTable* GetPlayerEntry(NetworkPlayer playerAddress);
	
	// Get all the registered player addresses
	std::vector<PlayerTable> GetPlayerAddresses();
	
	// For user scripting
	bool IsClient();
	bool IsServer();
	
	void SetReceivingGroupEnabled (int player, int group, bool enabled);
	void SetSendingGroupEnabled (int group, bool enabled);
	void SetSendingGroupEnabled (int playerIndex, int group, bool enabled);

	bool MayReceiveFromPlayer( SystemAddress player, int group );
	bool MaySendToPlayer( SystemAddress adress, int group );
	bool MaySend(  int group );

	void SetMessageQueueRunning(bool run);
	bool GetMessageQueueRunning() { return m_MessageQueueRunning; }
	
	bool WasViewIdAllocatedByMe(NetworkViewID viewID);
	bool WasViewIdAllocatedByPlayer (NetworkViewID viewID, NetworkPlayer playerID);
	NetworkPlayer GetNetworkViewIDOwner(NetworkViewID viewID);

	void SetSimulation (NetworkSimulation simulation);
	string GetStats(int player);

	void SetSendRate (float rate) { m_Sendrate = rate; }
	float GetSendRate () { return m_Sendrate; }

//	void LoadLevel(const std::string& levelName);
//	void LoadLevel(int level);
	
	int GetMaxConnections();
	int GetConnectionCount();
	void GetConnections(int* connection);
	
	bool IsPasswordProtected();
	
	std::string GetIPAddress();
	std::string GetIPAddress(int player);
	SystemAddress GetServerAddress() { return m_ServerAddress; }
	std::string GetExternalIP();
	int GetExternalPort();

	int GetPort();
	int GetPort(int player);
	
	std::string GetGUID();
	std::string GetGUID(int player);
	
//	void SetChannelEnabled (UInt32 channel, bool enabled);
//	bool GetChannelEnabled (UInt32 channel) { return m_EnabledChannels & (1 << channel); }
	
	// If viewId is 0 all view ids will be removed
	// If playerIndex is -1 all players will be removed
	void RemoveRPCs(NetworkPlayer playerIndex, NetworkViewID viewID, UInt32 channelMask);
	
	bool IsConnected() { return m_PeerType != kDisconnected; };

	double GetTime();
	RakNetTime GetTimestamp();

	bool GetUseNat();
	void SetUseNat(bool enabled);

	int GetLastPing (NetworkPlayer player);
	int GetAveragePing (NetworkPlayer player);
	
	Object* Instantiate (Object& asset, Vector3f pos, Quaternionf rot, UInt32 group);
	Object* NetworkInstantiateImpl (RakNet::BitStream& bitstream, SystemAddress sender, RakNetTime time);
	static void RPCNetworkInstantiate (RPCParameters* params);
	
	void SetAssetToPrefab (const AssetToPrefab& mapping);
	
	RakPeerInterface* GetPeer();
	
	void ResolveFacilitatorAddress();
	SystemAddress& GetFacilitatorAddress(bool resolve = true);
	void SwapFacilitatorID(SystemAddress newAddress);
	
	int TestConnection(bool forceNATType, bool forceTest = false);
	SystemAddress GetConnTesterAddress() { return m_ConnTesterAddress; }
	void SetConnTesterAddress(SystemAddress address);
	
	void SetOldMasterServerAddress(SystemAddress address);
	
	void SetMinimumAllocatableViewIDs (int v) { m_MinimumAllocatableViewIDs = v; m_NetworkViewIDAllocator.SetMinAvailableViewIDs(v); } 
	int GetMinimumAllocatableViewIDs () { return m_MinimumAllocatableViewIDs; } 

	void SetMaxConnections(int connections);
	
	void PingWrapper(Ping *time);

	void SetProxyIP(string address) { m_ProxyAddress.SetBinaryAddress(address.c_str()); }
	string GetProxyIP() { return string(m_ProxyAddress.ToString(false)); }
	void SetProxyPort(int port) { m_ProxyAddress.port = port; }
	int GetProxyPort() { return m_ProxyAddress.port; }
	SystemAddress GetProxyAddress() { return m_ProxyAddress; }
	void SetUseProxy(bool value) { m_UseProxy = value; }
	bool GetUseProxy() { return m_UseProxy; }
	string GetProxyPassword() { return m_ProxyPassword; }
	void SetProxyPassword(string password) { m_ProxyPassword = password; }

	int GetInitIndexSize() { return m_UsedInitIndices.size(); }
	
	private:

	void ProcessPacket(unsigned char packetIdentifier);
	void ResolveProxyAddress();

//	void DumpNewConnectionInitialData( SystemAddress player, int channel );
	void SendRPCBuffer (PlayerTable &player);
	static void RPCReceiveViewIDBatch (RPCParameters *rpcParameters);
	static void RPCRequestViewIDBatch (RPCParameters *rpcParameters);
	static void RPCNetworkDestroy(RPCParameters *rpcParms);

	// Successfully connected to server. Request rpc buffer contents.
	void MsgConnected();
	// New client connected to server. Send initialization message back to him (contains player ID number)
	void MsgNewConnection(SystemAddress sender = UNASSIGNED_SYSTEM_ADDRESS);
	void MsgStateUpdate(SystemAddress senderAddress);
	void MsgClientInit();
	void MsgRemoveRPCs();
	void MsgRemovePlayerRPCs();
	void MsgClientDidDisconnect();
	void MsgClientDidDisconnect(SystemAddress clientAddress);
	
	int GetValidInitIndex();
	void ClientConnectionDisconnected(int msgType);

private:

	typedef List< ListNode<NetworkView> > NetworkViewList;
	typedef NetworkViewList::iterator NetworkViewIterator;

	bool              m_MessageQueueRunning;
	float             m_Sendrate;
	float             m_LastSendTime;
//	float             m_TimeoutTime;
	int				  m_PeerType;		///< enum { Server = 0, Client = 1, Server-Client = 2}
	int				  m_PlayerID;		// Player ID number
	int				  m_HighestPlayerID;// Available player ID numbers (server) This is used only on the server
	int               m_MinimumAllocatableViewIDs;
	RakPeerInterface  *m_Peer;
	Packet            *m_Packet;
	int				  m_LevelPrefix;
	RakNet::BitStream m_BitStream;
	SystemAddress	  m_ServerAddress;	// The system address of the server
	std::string		  m_ServerPassword;
	RakNetGUID		  m_ServerGUID;
	RPCBuffer         m_RPCBuffer;
	NetworkViewList   m_Sources;		// The set of object views which are to be synchronized
	NetworkViewList   m_NonSyncSources;	// Network views which do not sync with other network objects
	NetworkViewList   m_AllSources;
	
	PlayerAddresses	  m_Players;
	int				  m_DebugLevel;		///< enum { Off = 0, Informational = 1, Full = 2}
	NetworkViewIDAllocator m_NetworkViewIDAllocator;
	UInt32            m_SendingEnabled;
	UInt32            m_ReceivedInitialState;
	bool			  m_DoNAT;
	NatPunchthroughClient   m_NatPunchthrough;
	SystemAddress	  m_FacilitatorID;
	bool			  m_ConnectingAfterPing;
	time_t			  m_PingConnectTimestamp;
	SystemAddress	  m_OldMasterServerID;
	SystemAddress	  m_OldFacilitatorID;
	dynamic_bitset	  m_UsedInitIndices;
	ConnectionTester *m_ConnTester;
	int				  m_ConnStatus;
	SystemAddress	  m_ConnTesterAddress;
	int				  m_MaxConnections;
	Thread			  m_PingThread;
	PingQueue		  m_PingQueue;
	SystemAddress	  m_ProxyAddress;
	bool			  m_UseProxy;
	string			  m_ProxyPassword;
	unsigned short	  m_RelayPort;
	
	AssetToPrefab m_AssetToPrefab;
	PrefabToAsset m_PrefabToAsset;
};

void NetworkError (Object* obj, const char* format, ...);
void NetworkWarning (Object* obj, const char* format, ...);
void NetworkInfo (Object* obj, const char* format, ...);
void NetworkLog (Object* obj, const char* format, ...);

NetworkManager& GetNetworkManager ();
NetworkManager* GetNetworkManagerPtr ();

#endif
#endif	//UNITY_NETWORK_MANAGER_H_
