
#include "NetworkManager.h"

#if ENABLE_NETWORK
#include "MasterServerInterface.h"
#include "External/RakNet/builds/include/RakNetworkFactory.h"
#include "External/RakNet/builds/include/RakPeerInterface.h"
#include "External/RakNet/builds/include/RakNetStatistics.h"
#include "External/RakNet/builds/include/RakSleep.h"
#include "External/RakNet/builds/include/SocketLayer.h"
#endif
#include "Runtime/GameCode/CloneObject.h"
#include "BitStreamPacker.h"
#include "Runtime/Utilities/Utility.h"


NetworkManager::NetworkManager(BaseAllocator* baseAllocator, ObjectCreationMode mode)
:	Super(baseAllocator, mode) { }

NetworkManager::~NetworkManager() { }

void NetworkManager::AddNetworkView (ListNode_& s) { }

void NetworkManager::AddAllNetworkView (ListNode_& s) { }

void NetworkManager::AddNonSyncNetworkView (ListNode_& s) { }

void NetworkManager::AwakeFromLoad (AwakeFromLoadMode awakeMode) { }

void NetworkManager::SetAssetToPrefab (const std::map<UnityGUID, PPtr<GameObject> >& mapping) {	}

void NetworkManager::NetworkOnApplicationQuit() { }

void NetworkManager::InitializeSecurity() { }

int NetworkManager::InitializeServer(int connections, int listenPort, bool useNat)
{
	return 0;
}

int NetworkManager::Connect(std::vector<string> IPs, int remotePort, int listenPort, const std::string& password)
{
	return 0;
}

int NetworkManager::Connect(std::string IP, int remotePort, const std::string& password)
{
	return 0;
}

int NetworkManager::Connect(std::string IP, int remotePort, int listenPort, const std::string& password)
{
	return 0;
}

void NetworkManager::Disconnect(int timeout, bool resetParams) { }

void NetworkManager::CloseConnection(int target, bool sendDisconnect) { }

void NetworkManager::NetworkUpdate() { }

string NetworkManager::GetStats(int i)
{
	return string();	
}

void NetworkManager::ClientConnectionDisconnected(int msgType) { }


void NetworkManager::MsgNewConnection(SystemAddress clientAddress) { }

void NetworkManager::MsgClientInit() { }

void NetworkManager::RPCReceiveViewIDBatch (RPCParameters *rpcParameters) { }

void NetworkManager::RPCRequestViewIDBatch (RPCParameters *rpcParameters) { }

void NetworkManager::SendRPCBuffer (PlayerTable &player) { }

bool NetworkManager::MayReceiveFromPlayer( SystemAddress adress, int group )
{
	return false;
}

bool NetworkManager::MaySendToPlayer( SystemAddress address, int group )
{
	return false;
}


void NetworkManager::SetReceivingGroupEnabled (int playerIndex, int group, bool enabled) { }

void NetworkManager::SetSendingGroupEnabled (int group, bool enabled) { }

void NetworkManager::SetSendingGroupEnabled (int playerIndex, int group, bool enabled) { }

void NetworkManager::MsgStateUpdate(SystemAddress senderAddress) { }

void NetworkManager::DestroyPlayerObjects(NetworkPlayer playerID) { }

void NetworkManager::DestroyDelayed(NetworkViewID viewID) { }

void NetworkManager::RPCNetworkDestroy(RPCParameters *rpcParameters) { }

void NetworkManager::SetMessageQueueRunning(bool run) { }

void NetworkManager::RegisterRPC(const char* reg, void ( *functionPointer ) ( RPCParameters *rpcParms )) { }

void NetworkManager::PerformRPC(const std::string &function, int mode, RakNet::BitStream& parameters, NetworkViewID viewID, UInt32 group) { }

void NetworkManager::BroadcastRPC(const char* name, const RakNet::BitStream *parameters, PacketPriority priority, SystemAddress target, RakNetTime *includedTimestamp, UInt32 group ) { }

void NetworkManager::PerformRPCSpecificTarget(const char* function, PlayerTable *player, RakNet::BitStream& parameters, UInt32 group) { }

void NetworkManager::PeformRPCRelayAll(char *name, int mode, NetworkViewID viewID, UInt32 group, RakNetTime timestamp, SystemAddress sender, RakNet::BitStream &stream) { }

void NetworkManager::PerformRPCRelaySpecific(char *name, RakNet::BitStream *stream, NetworkPlayer player) { }


void NetworkManager::AddRPC(const std::string& name, NetworkPlayer sender, NetworkViewID viewID, UInt32 group, RakNet::BitStream& stream) { }

void NetworkManager::MsgRemoveRPCs() { }

void NetworkManager::RemoveRPCs(NetworkPlayer playerIndex, NetworkViewID viewID, UInt32 groupMask) { }

bool NetworkManager::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}

#pragma mark -

NetworkView* NetworkManager::ViewIDToNetworkView(const NetworkViewID& ID)
{
	return NULL;
}


NetworkViewID NetworkManager::NetworkViewToViewID(NetworkView* view)
{
    NetworkViewID dummy;
	return dummy;
}

int NetworkManager::GetValidInitIndex()
{
	return 0;
}

NetworkViewID NetworkManager::AllocateViewID()
{
	NetworkViewID dummy;
	return dummy;
}

NetworkViewID NetworkManager::AllocateSceneViewID()
{
	NetworkViewID dummy;
	return dummy;
}

NetworkViewID NetworkManager::ValidateSceneViewID(NetworkView* validateView, NetworkViewID viewID)
{
	NetworkViewID dummy;
	return dummy;
}

bool NetworkManager::WasViewIdAllocatedByPlayer (NetworkViewID viewID, NetworkPlayer playerID)
{
	return false;
}

bool NetworkManager::WasViewIdAllocatedByMe(NetworkViewID viewID)
{
	return false;
}

NetworkPlayer NetworkManager::GetNetworkViewIDOwner(NetworkViewID viewID)
{
	NetworkPlayer dummy;
    return dummy;
}

int NetworkManager::GetPlayerID()
{
	return 0;
}

int NetworkManager::GetPeerType()
{
	return 0;
}

int NetworkManager::GetDebugLevel()
{
	return 0;
}

SystemAddress NetworkManager::GetPlayerAddress()
{
	return UNASSIGNED_SYSTEM_ADDRESS;
}

bool NetworkManager::IsClient()
{
	return false;
}

bool NetworkManager::IsServer()
{
	return false;
}

void NetworkManager::SetSimulation (NetworkSimulation simulation) { }

void NetworkManager::MsgClientDidDisconnect() { }

void NetworkManager::MsgClientDidDisconnect(SystemAddress clientAddress) { }

void NetworkManager::SetIncomingPassword (const std::string& incomingPassword) { }

std::string NetworkManager::GetIncomingPassword ()
{
	return string();
}

int NetworkManager::GetMaxConnections()
{
	return 0;
}

int NetworkManager::GetConnectionCount()
{
	return 0;
}

PlayerTable* NetworkManager::GetPlayerEntry(SystemAddress playerAddress)
{
	return NULL;
}

PlayerTable* NetworkManager::GetPlayerEntry(NetworkPlayer index)
{
	return NULL;
}

SystemAddress NetworkManager::GetSystemAddressFromIndex(NetworkPlayer playerIndex)
{
	return UNASSIGNED_SYSTEM_ADDRESS;
}

int NetworkManager::GetIndexFromSystemAddress(SystemAddress playerAddress)
{
	return -1;
}

std::vector<PlayerTable> NetworkManager::GetPlayerAddresses()
{
	return std::vector<PlayerTable>();
}


bool NetworkManager::IsPasswordProtected()
{
	return false;
}

std::string NetworkManager::GetIPAddress()
{
	return string();
}

std::string NetworkManager::GetExternalIP()
{
	return std::string();
}

int NetworkManager::GetExternalPort()
{
	return 0;
}

std::string NetworkManager::GetIPAddress(int player)
{
	return string ();
}

int NetworkManager::GetPort()
{
	return 0;
}

int NetworkManager::GetPort(int player)
{
	return 0;
}

void NetworkManager::GetConnections(int* connection) { }

bool NetworkManager::GetUseNat()
{
	return false;
}

void NetworkManager::SetUseNat(bool enabled) {	 }




template<class TransferFunc>
void NetworkManager::Transfer (TransferFunc& transfer) { }

int NetworkManager::GetLastPing (NetworkPlayer player)
{
	return 0;
}

int NetworkManager::GetAveragePing (NetworkPlayer player)
{
	return 0;
}

Object* NetworkManager::Instantiate (Object& prefab, Vector3f pos, Quaternionf rot, UInt32 group)
{
    return NULL;
}


Object* NetworkManager::NetworkInstantiateImpl (RakNet::BitStream& bitstream, SystemAddress sender, RakNetTime time)
{
    return NULL;
}

void NetworkManager::RPCNetworkInstantiate (RPCParameters* rpcParameters) { }

void NetworkManager::SetLevelPrefix(int levelPrefix) { }

IMPLEMENT_CLASS_HAS_INIT (NetworkManager)
IMPLEMENT_OBJECT_SERIALIZE (NetworkManager)
GET_MANAGER (NetworkManager)
GET_MANAGER_PTR (NetworkManager)

void NetworkManager::InitializeClass () { }

void NetworkManager::CleanupClass () { }

bool NetworkManager::MaySend(  int group )
{
	return false;
}

RakNetTime NetworkManager::GetTimestamp()
{
	return 0;
}

double NetworkManager::GetTime()
{
	return 0;
}

RakPeerInterface* NetworkManager::GetPeer()
{
	return 0;
}

void NetworkManager::SwapFacilitatorID(SystemAddress newAddress) { }

void NetworkManager::SetOldMasterServerAddress(SystemAddress address) { }

void NetworkManager::SetConnTesterAddress(SystemAddress address) { }

int NetworkManager::TestConnection(bool forceNATType, bool forceTest)
{
    return -1;
}

void NetworkManager::SetMaxConnections(int connections) { }

void NetworkManager::PingWrapper(Ping *time) { }


static SystemAddress dummy_system_address;
SystemAddress& NetworkManager::GetFacilitatorAddress(bool resolve) 
{
	return dummy_system_address;
}

ConnectionTester::ConnectionTester(SystemAddress address) { }

ConnectionTester::~ConnectionTester() { }

void ConnectionTester::SetAddress(SystemAddress address) { }

int ConnectionTester::Update()
{
    return 0;
}

void ConnectionTester::ReportTestSucceeded() { }

int ConnectionTester::RunTest(bool forceNATType)
{
    return 0;
}


// NetworkView

NetworkView::NetworkView (BaseAllocator* baseAllocator, ObjectCreationMode mode)
:	Super(baseAllocator, mode)
,	m_Node (this)
,	m_AllNode(this) { }

NetworkView::~NetworkView () { }

void NetworkView::Update() { }

void NetworkView::RPCCall (const std::string &function, int inMode, MonoArray* args) { }

void NetworkView::RPCCallSpecificTarget (const std::string &function, NetworkPlayer target, MonoArray* args) { }

void NetworkView::AddToManager () { }

void NetworkView::RemoveFromManager () { }

void NetworkView::SetupSceneViewID () { }

void NetworkView::AwakeFromLoad (AwakeFromLoadMode mode) { }

void NetworkView::SetObserved (Unity::Component* component) { }

Unity::Component* NetworkView::GetObserved ()
{
    return NULL;
}

void NetworkView::Unpack (RakNet::BitStream& bitStream, NetworkMessageInfo& info, int msgType) { }

bool NetworkView::Pack(RakNet::BitStream &stream, PackState* writeStatePtr, UInt8* readData, int &readSize, int msgID)
{
    return false;
}

void NetworkView::Send (SystemAddress systemAddress, bool broadcast) { }

void NetworkView::SendToAllButOwner() { }

NetworkViewID NetworkView::GetViewID()
{
	NetworkViewID dummy;
    return dummy;
}

void NetworkView::SetViewID(NetworkViewID viewID) { }

void NetworkView::SetGroup(unsigned group) { }

void NetworkView::Reset () { }

void NetworkView::SetStateSynchronization (int sync) { }

void NetworkView::SetInitState(int index, bool isSent) { }

bool NetworkView::GetInitStateStatus(int index)
{
	return false;
}

void NetworkView::ClearInitStateAndOwner() { }

bool NetworkView::SetPlayerScope(NetworkPlayer playerIndex, bool relevancy)
{
	return false;
}

void NetworkView::SetScope(unsigned int initIndex, bool relevancy) { }

bool NetworkView::CheckScope(int initIndex)
{
	return false;
}

template<class TransferFunc>
void NetworkView::Transfer (TransferFunc& transfer) { }

SystemAddress NetworkView::GetOwnerAddress ()
{
	return UNASSIGNED_SYSTEM_ADDRESS;
}

void RegisterRPC (const char* name) { }

void NetworkView::InitializeClass () { }

void NetworkView::CleanupClass () { }


IMPLEMENT_CLASS_HAS_INIT (NetworkView)
IMPLEMENT_OBJECT_SERIALIZE (NetworkView)


// BitStreamPacker

void BitstreamPacker::ReadPackState (Quaternionf& t) { }

void BitstreamPacker::ReadPackState (Vector3f& t) { }

void BitstreamPacker::WritePackState (Vector3f& t) { }

void BitstreamPacker::WritePackState (Quaternionf& t) { }

void BitstreamPacker::WritePackState (string& t) { }

void BitstreamPacker::ReadPackState (string& t) { }

void BitstreamPacker::WritePackState (char* t, int& length) { }

void BitstreamPacker::ReadPackState (char*& t, int& length) { }

void BitstreamPacker::ReadPackState (NetworkViewID& t) { }

void BitstreamPacker::WritePackState (NetworkViewID& t) { }

#define READ_WRITE_PACKSTATE(TYPE) \
void BitstreamPacker::ReadPackState (TYPE& t) \
{ \
} \
void BitstreamPacker::WritePackState (TYPE t) \
{ \
}

READ_WRITE_PACKSTATE(UInt32)
READ_WRITE_PACKSTATE(float)
READ_WRITE_PACKSTATE(short)
READ_WRITE_PACKSTATE(UInt8)
READ_WRITE_PACKSTATE(bool)

void BitstreamPacker::Serialize (NetworkViewID& value) { }

void BitstreamPacker::Serialize (float& value, float maxDelta) { }

void BitstreamPacker::Serialize (bool& value) { }

#define SERIALIZE(TYPE) void BitstreamPacker::Serialize (TYPE& value) {\
}

SERIALIZE(UInt32)
SERIALIZE(short)
SERIALIZE(UInt8)


void BitstreamPacker::Serialize (Vector3f& value, float maxDelta) { }

void BitstreamPacker::Serialize (Quaternionf& value, float maxDelta) { }

void BitstreamPacker::Serialize (std::string& value) { }

void BitstreamPacker::Serialize (char* value, int& valueLength) { }

BitstreamPacker::BitstreamPacker (RakNet::BitStream& stream, std::vector<UInt8>* delta, UInt8* readData, int readSize, bool reading) { }

BitstreamPacker::BitstreamPacker (RakNet::BitStream& stream, bool reading) { }

void BitstreamPacker::Init(RakNet::BitStream& stream, std::vector<UInt8>* delta, UInt8* readData, int readSize, bool reading) { }



// MasterServerInterface


MasterServerInterface::MasterServerInterface(BaseAllocator* baseAllocator, ObjectCreationMode mode)
:	Super(baseAllocator, mode) { }

MasterServerInterface::~MasterServerInterface() { }

void MasterServerInterface::NetworkOnApplicationQuit() { }

// Resolve the master server address if it is invalid
void ResolveMasterServerAddress(SystemAddress& address) { }

void MasterServerInterface::Connect() { }

void MasterServerInterface::ProcessPacket(Packet *packet) { }

void MasterServerInterface::NetworkUpdate() { }

void MasterServerInterface::QueryHostList() { }

void MasterServerInterface::QueryHostList(string gameType) { }

void MasterServerInterface::ClearHostList() { }

bool MasterServerInterface::PopulateUpdate()
{
	return false;
}

bool MasterServerInterface::PopulateUpdate(string gameName, string comment)
{
	return false;
}

void MasterServerInterface::RegisterHost(string gameType, string gameName, string comment) { }

// Uses the game server peer
void MasterServerInterface::SendHostUpdate() { }

void MasterServerInterface::Disconnect() { }

// Uses the game server peer
void MasterServerInterface::UnregisterHost() { }

std::vector<HostData> MasterServerInterface::PollHostList()
{	
	return std::vector<HostData>();
}

void MasterServerInterface::ResetHostState() { }

IMPLEMENT_CLASS (MasterServerInterface)
GET_MANAGER (MasterServerInterface)
GET_MANAGER_PTR (MasterServerInterface)



// Utilities

std::string GetLocalIP()
{
    std::string s = "0.0.0.0";
    return s;
}

bool CheckForPublicAddress()
{
    return false;
}

int Ping::GetTime() 
{
	return 0;
}

void Ping::SetTime(int value) { }

int Ping::GetIsDone() 
{
	return 0;
}

void Ping::SetIsDone(bool value) { }

std::string Ping::GetIP()
{
	return std::string();
}

void Ping::SetIP(std::string value) { }



// NetworkViewID


std::string NetworkViewID::ToString () const
{
	return std::string();
}

bool operator == (const NetworkViewID& lhs, const NetworkViewID& rhs)
{
	return false;
}


// NetworkViewIDAllocator


NetworkViewIDAllocator::NetworkViewIDAllocator() { }



// RakNet


using namespace RakNet;

BitStream::BitStream() { }

BitStream::~BitStream() { }

LightweightDatabaseClient::LightweightDatabaseClient() { }

LightweightDatabaseClient::~LightweightDatabaseClient() { }

using namespace DataStructures;

Table::Cell::Cell() { }

Table::Cell::~Cell() { }

NatPunchthroughClient::NatPunchthroughClient() { }

NatPunchthroughClient::~NatPunchthroughClient() { }

bool NatPunchthroughClient::OpenNAT(RakNetGUID destination, SystemAddress facilitator)
{
	return true;
}

void NatPunchthroughClient::Clear(void) { }

void NatPunchthroughClient::OnAttach(void) { }

void NatPunchthroughClient::Update(void) { }

PluginReceiveResult NatPunchthroughClient::OnReceive(Packet *packet)
{
    return RR_STOP_PROCESSING_AND_DEALLOCATE;
}

void NatPunchthroughClient::OnClosedConnection(SystemAddress systemAddress, RakNetGUID rakNetGUID, PI2_LostConnectionReason lostConnectionReason) { }

void NatPunchthroughClient::OnRakPeerShutdown(void) { }

PluginInterface2::PluginInterface2() { }

PluginInterface2::~PluginInterface2() { }

const char *SystemAddress::ToString(bool writePort) const
{
    return "";
}

void SystemAddress::SetBinaryAddress(const char *str) { }
