#include "UnityPrefix.h"
#include "MasterServerInterface.h"

#if ENABLE_NETWORK
#include "External/RakNet/builds/include/TableSerializer.h"
#include "External/RakNet/builds/include/BitStream.h"
#include "External/RakNet/builds/include/StringCompressor.h"
#include "External/RakNet/builds/include/DS_Table.h"
#include "External/RakNet/builds/include/RakNetworkFactory.h"
#include "External/RakNet/builds/include/SocketLayer.h"
#include "Configuration/UnityConfigureVersion.h"
#include "NetworkManager.h"
#include <time.h>
#include "NetworkUtility.h"

// Future todos
// TODO: ATM there is a 200 ms delay on disconnects. It "could" be possible that more than 200 ms pass before a network action is completed
//       maybe this should be done only after we make sure the operation is done.
// NOTE: Row IDs are sent to clients when they do not have a row ID (first reg) or when the master server has restarted (and forgotten everything).
//       The problem is that updates from the client are sent with the old row ID which is not found during lookup and a new one sent to the client
//       The client never receives this new row ID because the connection is always closed after sending updates. The simples solution is to
//       keep client connections persistent for clients running hosts. Clients which just query for the host list (game clients) disconnect immediately.

namespace {
	const int kMasterServerPort = 23466;
	const time_t kMaxUpdateInterval = 2;
}

MasterServerInterface::MasterServerInterface(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Peer = RakNetworkFactory::GetRakPeerInterface();
	m_GameType = "";
	m_HostName = "";
	m_PendingRegister = false;
	m_PendingHostUpdate = false;
	m_PendingQuery = false;
	m_RowID = -1;
	m_LastHostUpdateTime = 0;
	m_Registered = false;
	m_MasterServerID.binaryAddress = 0;
	m_MasterServerID.port = kMasterServerPort;
	m_Version[0]=2; m_Version[1]=0; m_Version[2]=0;
	m_UpdateRate = 10;
	m_IsDedicatedServer = false;
	time(&m_ShutdownTimer);

	m_HostDatabaseClient = new LightweightDatabaseClient;
	
	// Create the cell update that will be reused for storing last update
	strcpy(m_LastUpdate[0].columnName,"NAT");
	m_LastUpdate[0].columnType = DataStructures::Table::NUMERIC;
	m_LastUpdate[0].cellValue.Set(0);
	strcpy(m_LastUpdate[1].columnName,"Game name");
	m_LastUpdate[1].columnType = DataStructures::Table::STRING;
	m_LastUpdate[1].cellValue.Set(0);
	strcpy(m_LastUpdate[2].columnName,"Connected players");
	m_LastUpdate[2].columnType = DataStructures::Table::NUMERIC;
	m_LastUpdate[2].cellValue.Set(0);
	strcpy(m_LastUpdate[3].columnName,"Player limit");
	m_LastUpdate[3].columnType = DataStructures::Table::NUMERIC;
	m_LastUpdate[3].cellValue.Set(0);
	strcpy(m_LastUpdate[4].columnName,"Password protected");
	m_LastUpdate[4].columnType = DataStructures::Table::NUMERIC;
	m_LastUpdate[4].cellValue.Set(0);
	strcpy(m_LastUpdate[5].columnName,"IP address");
	m_LastUpdate[5].columnType = DataStructures::Table::BINARY;
	m_LastUpdate[5].cellValue.Set(NULL, 0);
	strcpy(m_LastUpdate[6].columnName,"Port");
	m_LastUpdate[6].columnType = DataStructures::Table::NUMERIC;
	m_LastUpdate[6].cellValue.Set(0);
	strcpy(m_LastUpdate[7].columnName,"Comment");
	m_LastUpdate[7].columnType = DataStructures::Table::STRING;
	m_LastUpdate[7].cellValue.Set(0);
}

MasterServerInterface::~MasterServerInterface()
{
	delete m_HostDatabaseClient;
	m_HostDatabaseClient = NULL;
	RakNetworkFactory::DestroyRakPeerInterface(m_Peer);
	m_Peer = NULL;
}

void MasterServerInterface::NetworkOnApplicationQuit()
{
	m_Peer->Shutdown(100);
	m_HostList.clear();
	// Reset to default values
	m_MasterServerID.binaryAddress = 0;
	m_MasterServerID.port = kMasterServerPort;	
	m_GameType = "";
	m_HostName = "";
	m_HostComment = "";
	m_PendingRegister = false;
	m_PendingHostUpdate = false;
	m_PendingQuery = false;
	m_RowID = -1;
	m_Registered = false;
	m_UpdateRate = 10;
	m_IsDedicatedServer = false;
}

// Resolve the master server address if it is invalid
void MasterServerInterface::ResolveMasterServerAddress()
{
	ResolveAddress(m_MasterServerID, "masterserver.unity3d.com", "masterserverbeta.unity3d.com",
		"Cannot resolve master server address, you must be connected to the internet before using it or set the address to something accessible to you.");
}

void MasterServerInterface::ClientConnect()
{
	ResolveMasterServerAddress();

	SocketDescriptor sd(0,0);
	if (!m_Peer->Startup(1, 30, &sd, 1))
	{
		SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, kFailedToCreatedSocketOrThread);
	}
	m_Peer->AttachPlugin(&m_DatabaseClient);
	if (!m_Peer->Connect(m_MasterServerID.ToString(false), m_MasterServerID.port, 0, 0))
	{
		if (m_Peer->GetMaximumNumberOfPeers() >= m_Peer->NumberOfConnections())
		{
			ErrorString("Internal error while connecting to master server. Too many connected peers.");
		}
		else
		{
			ErrorString("Internal error while attempting to connect to master server.");
			SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, kIncorrectParameters);
		}
	}
}

bool MasterServerInterface::CheckServerConnection()
{
	ResolveMasterServerAddress();
	
	if (!GetNetworkManager().GetPeer()->IsConnected(m_MasterServerID))
	{
		ServerConnect();
		return false;
	}
	if (!GetNetworkManager().GetPeer()->IsActive())
	{
		ServerConnect();
		return false;
	}
	return true;
}

void MasterServerInterface::ServerConnect()
{
	if (!GetNetworkManager().GetPeer()->Connect(m_MasterServerID.ToString(false), m_MasterServerID.port, 0, 0))
	{
		ErrorString("Internal error while attempting to connect to master server\n");
		SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, kInternalDirectConnectFailed);
	}
	NetworkInfo(NULL, "Attempting to connect to master server at %s:%d", m_MasterServerID.ToString(false), m_MasterServerID.port);
	m_PendingRegister = true;
}

void MasterServerInterface::ProcessPacket(Packet *packet)
{
	switch(packet->data[0])
	{
		// Disconnect and connection lost only occurs for clients requesting host info, not servers registering their info
		case ID_DISCONNECTION_NOTIFICATION:
		{
			NetworkInfo(NULL, "Disconnected from master server");
			SendToAllNetworkViews(kDisconnectedFromMasterServer, ID_DISCONNECTION_NOTIFICATION);
			m_PendingQuery = false;
			break;
		}
		case ID_CONNECTION_LOST:
		{
			// If connection was lost with master server we should re-register the host. If running as client do nothing.
			if (GetNetworkManager().IsServer())
			{
				NetworkInfo(NULL, "Lost connection to master server, reconnecting and resending host info");
				ResetHostState();
				SendHostUpdate();
			}
			else
			{
				ErrorString("Connection with master server lost");
				SendToAllNetworkViews(kDisconnectedFromMasterServer, ID_CONNECTION_LOST);
				m_PendingQuery = false;
			}
			break;
		}
		case ID_CONNECTION_BANNED:
		{
			ErrorString("Temporarily banned from the master server");
			SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, ID_CONNECTION_BANNED);
			break;
		}
		case ID_CONNECTION_REQUEST_ACCEPTED:
		{
			NetworkInfo(NULL, "Connected to master server at %s", packet->systemAddress.ToString());
			if (m_PendingRegister)
			{
				m_PendingRegister = false;
				RegisterHost(m_GameType, m_HostName, m_HostComment);
			}
			if (m_PendingQuery)
			{
				m_PendingQuery = false;
				QueryHostList(m_GameType);
			}
			if (m_PendingHostUpdate)
			{
				m_PendingHostUpdate = false;
				SendHostUpdate();
			}
			break;
		}
		case ID_ALREADY_CONNECTED:
		{
			NetworkError(NULL, "Already connected to the master server, the server probably hasn't cleaned up because of an abrupt disconnection.");
			SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, ID_ALREADY_CONNECTED);
			m_PendingQuery = false;
			break;
		}
		case ID_CONNECTION_ATTEMPT_FAILED:
		{
			ErrorString(Format("Failed to connect to master server at %s", packet->systemAddress.ToString()));
			SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, ID_CONNECTION_ATTEMPT_FAILED);
			ResetHostState();
			break;
		}
		// TODO: atm the server does not send this during client lookups, but maybe it should (as all tables are dynamically created this will never be returned to hosts)
		case ID_DATABASE_UNKNOWN_TABLE:
		{
			ErrorString("Unkown game type");
			// This uses NetworkConnectionError enums, and this status message doesn't belong in there 
			//SendToAllNetworkViews(kMasterServerConnectionAttemptFailed, kUnkownGameType);
			break;
		}
		// This should never occur as we don't use db passwords directly
		case ID_DATABASE_INCORRECT_PASSWORD:
		{
			ErrorString("Incorrect master server password");
			break;
		}
		case ID_DATABASE_QUERY_REPLY:
		{
			NetworkInfo(NULL, "Incoming host list query response from master server.");
			
			DataStructures::Table table;
			if (TableSerializer::DeserializeTable(packet->data+sizeof(MessageID), packet->length-sizeof(MessageID), &table))
			{
				m_HostList.clear();
				DataStructures::Page<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> *cur = table.GetListHead();
				while (cur)
				{
					for (int i=0; i < (unsigned)cur->size; i++)
					{
						// NOTE: The first three cell values are the systemId (binary), last ping response time and next ping 
						// send time(numerics)
						DataStructures::List<DataStructures::Table::Cell*> cells = cur->data[i]->cells;
						HostData data;
						if (cells[5]->c != NULL && 
							(int)cells[9]->i != 0 && 
							cells[9]->c != NULL && 
							(int)cells[10]->i != 0 &&
							cells[12]->c != NULL)
						{
							data.useNat			   = (int)cells[4]->i;
							data.gameType		   = m_GameType;
							data.gameName		   = cells[5]->c;
							data.connectedPlayers  = (int)cells[6]->i;
							data.playerLimit	   = (int)cells[7]->i;
							data.passwordProtected = (int)cells[8]->i;
							data.guid = cells[12]->c;

							int ipCount = int(cells[9]->i / 16);
							// If the size of the data is not a factor of 16 then something has gone wrong (IP addresses have size 16)
							if (((int)cells[9]->i) % 16 == 0)
							{
								for (int ip=0; ip < ipCount; ip++)
								{
									const char* ipData = cells[9]->c + ip * 16;
									if( ipData[0] == 0 ) break;
									data.IP.push_back( ipData );
								}														
							}
							else
							{
								ErrorString(Format("Malformed data inside IP information packet. Size was %f", cells[8]->i));
							}
							data.port			   = int(cells[10]->i);
							if (cells[11]->c != NULL)
								data.comment	   = cells[11]->c;
							else
								data.comment       = "";
							m_HostList.push_back(data);
						}
						else
						{
							ErrorString("Received malformed data in the host list from the master server\n");
						}
					}
					cur=cur->next;
				}
			} 
			else
			{
				m_HostList.clear();
			}
			SendToAllNetworkViews(kMasterServerEvent, kHostListReceived);
			break;
		}
		case ID_DATABASE_ROWID:
		{
			unsigned int rowID;
			RakNet::BitStream stream;
			stream.Write((char*)packet->data, packet->length);
			stream.IgnoreBits(8);
			stream.Read(rowID);

			NetworkInfo(NULL, "Received identifier %u from master server", rowID);
			SendToAllNetworkViews(kMasterServerEvent, kRegistrationSucceeded);
			m_RowID = rowID;
			
			break;
		}
		case ID_MASTERSERVER_REDIRECT:
		{
			SystemAddress newMaster, newFacilitator;
			RakNet::BitStream b(packet->data, packet->length, false);
			b.IgnoreBits(8);
			b.Read(newMaster);
			b.Read(newFacilitator);
			
			GetNetworkManager().SwapFacilitatorID(newFacilitator);
			SystemAddress oldMasterServer = m_MasterServerID;
			GetNetworkManager().SetOldMasterServerAddress(oldMasterServer);
			m_MasterServerID = newMaster;
			ResetHostState();
			
			if (GetNetworkManager().IsServer())
			{
				GetNetworkManager().GetPeer()->CloseConnection(oldMasterServer, true);
				NetworkInfo(NULL, "Redirecting master server host updates to %s", newMaster.ToString());
				NetworkInfo(NULL, "Changing facilitator location to %s", newFacilitator.ToString());
				SendHostUpdate();
			}
			else
			{
				NetworkInfo(NULL, "Redirecting master server host list queries to %s", newMaster.ToString());
				NetworkInfo(NULL, "Changing facilitator location to %s", newFacilitator.ToString());
				Disconnect();
				QueryHostList();
			}
			break;
		}
		case ID_MASTERSERVER_MSG:
		{
			int msgLength;
			RakNet::BitStream b(packet->data, packet->length, false);
			b.IgnoreBits(8);
			b.Read(msgLength);
			if (msgLength > 0)
			{
				char* msg = new char[msgLength];
				b.Read(msg, msgLength);
				LogString(Format("Message from master server: %s", msg));
				delete[] msg;
			}
			break;
		}
		default:
		{
			NetworkError(NULL, "Unknown message from master server (%s) %d", packet->systemAddress.ToString(), packet->data[0]);
			break;
		}
	}
}

void MasterServerInterface::NetworkUpdate()
{
	if (!m_Peer)
		return;
		
	// Send heartbeat if running as server, only send if running as server and we have already registered (m_HostName set)
	if (m_UpdateRate > 0 && m_Registered)
	{
		if ((time(0) - m_LastHostUpdateTime > m_UpdateRate) && m_HostName.size() > 1 && !m_PendingRegister)
		{
			SendHostUpdate();
		}
	}
	
	if (!m_Peer->IsActive())
		return;
		
	// If not registering or already registered, this is a client, then check for shutdown timeout
	if (!m_Registered && !m_PendingRegister && time(0) > m_ShutdownTimer + 20)
	{
		// Use a short delay, its not that bad if the disconnect notification never arrives at master server
		m_Peer->Shutdown(50, 0);
	}

	Packet *p;
	p=m_Peer->Receive();
	while (p)
	{
		ProcessPacket(p);
		m_Peer->DeallocatePacket(p);
		p=m_Peer->Receive();
	}
}

void MasterServerInterface::QueryHostList()
{
	QueryHostList(m_GameType);
}

void MasterServerInterface::QueryHostList(string gameType)
{
	time(&m_ShutdownTimer);

	// Wait for previous query to clear
	if (m_PendingQuery) return;

	if (gameType.empty())
	{
		ErrorString("Empty game type given in QueryHostList(), aborting query.");
		return;
	}
	m_GameType = gameType;
	
	ResolveMasterServerAddress();
	
	if (m_Peer == NULL)
	{
		ClientConnect();
		m_PendingQuery = true;
		return;
	}
	else if (!m_Peer->IsActive())
	{
		ClientConnect();
		m_PendingQuery = true;
		return;
	}
	else if (!m_Peer->IsConnected(m_MasterServerID))
	{
		ClientConnect();
		m_PendingQuery = true;
		return;
	}
	
	m_DatabaseClient.QueryTable(m_Version, gameType.c_str(), 0, 0, 0, 0, 0, 0, 0, m_MasterServerID, false);
//	LogString("Sent host query to master server");
	// Disconnect after the list has arrived
}

void MasterServerInterface::ClearHostList()
{
	m_HostList.clear();
}

bool MasterServerInterface::PopulateUpdate()
{
	return PopulateUpdate(m_HostName, m_HostComment);
}

bool MasterServerInterface::PopulateUpdate(string gameName, string comment)
{
	// TODO: The function inside GetIPs uses char arrays which are pre-allocated. If it returns a full array then it is possible there
	// are more IP addresses, in which case it should get a larger char array to use.
	char ips[10][16];
	int size = GetIPs(ips)*16;
	if (size == 0)
		ErrorString("Could not retrieve internal IP address. Host registration failed.");
	
	bool changed = false;
	
	if (((int)m_LastUpdate[0].cellValue.i) != static_cast<int>(GetNetworkManager().GetUseNat()))
		changed = true;
	
	if (((int)m_LastUpdate[1].cellValue.i) != 0 && changed != true)
	{
		if (strcmp(m_LastUpdate[1].cellValue.c,gameName.c_str()) != 0)
		{
			changed = true;
			m_LastUpdate[1].cellValue.Clear();
			m_LastUpdate[1].cellValue.Set(const_cast<char*>(gameName.c_str()));
		}
	}
	else
	{
		changed = true;
	}
	
	//printf_console("connCount: Comparing %d and %d\n", intCell, GetNetworkManager().GetConnectionCount());
	if (((int)m_LastUpdate[2].cellValue.i) != (GetNetworkManager().GetConnectionCount() + static_cast<int>(!m_IsDedicatedServer)) && changed != true)
		changed = true;		
	//printf_console("maxCount: Comparing %d and %d\n", intCell, GetNetworkManager().GetMaxConnections());
	if (((int)m_LastUpdate[3].cellValue.i) != GetNetworkManager().GetMaxConnections() + static_cast<int>(!m_IsDedicatedServer) && changed != true)
		changed = true;
	//printf_console("password: Comparing %d and %d\n", intCell, GetNetworkManager().IsPasswordProtected());
	if (((int)m_LastUpdate[4].cellValue.i) != static_cast<int>(GetNetworkManager().IsPasswordProtected()) && changed != true)
		changed = true;
		
	if (((int)m_LastUpdate[5].cellValue.i) != 0 && changed != true)
	{
		/*printf_console("IPs: Comparing size %d and %d\n", intCell, size);
		for (int i=0; i < intCell; i++)
			printf_console("%x", tmpIPs[i]);
		printf_console(" ");
		for (int i=0; i < size; i++)
			printf_console("%x", static_cast<char*>(ips[0])[i]);
		printf_console("\n");*/
		if (m_LastUpdate[5].cellValue.i != size)
			changed = true;
		else if (memcmp(m_LastUpdate[5].cellValue.c, ips, size) != 0)
			changed = true;
	}
	else
	{
		changed = true;
	}
	
	//printf_console("port: Comparing %d and %d\n", intCell, GetNetworkManager().GetPort());
	if (((int)m_LastUpdate[6].cellValue.i) != GetNetworkManager().GetPort() && changed != true)
		changed = true;
	
	if (((int)m_LastUpdate[7].cellValue.i) != 0 && changed != true)
	{
		//printf_console("comment: Comparing %s and %s\n", tmpComment, comment.c_str());
		if (strcmp(m_LastUpdate[7].cellValue.c, comment.c_str()) != 0)
			changed = true;
	}
	else
	{
		changed = true;
	}
	
	if (changed)
	{
		for (int i = 0; i < CELL_COUNT; i++)
			m_LastUpdate[i].cellValue.Clear();
		m_LastUpdate[0].columnType=DataStructures::Table::NUMERIC;
		m_LastUpdate[0].cellValue.Set(GetNetworkManager().GetUseNat());
		m_LastUpdate[1].columnType=DataStructures::Table::STRING;
		m_LastUpdate[1].cellValue.Set(const_cast<char*>(gameName.c_str()));
		m_LastUpdate[2].columnType=DataStructures::Table::NUMERIC;
		m_LastUpdate[2].cellValue.Set(GetNetworkManager().GetConnectionCount() + static_cast<int>(!m_IsDedicatedServer));
		m_LastUpdate[3].columnType=DataStructures::Table::NUMERIC;
		m_LastUpdate[3].cellValue.Set(GetNetworkManager().GetMaxConnections() + static_cast<int>(!m_IsDedicatedServer));
		m_LastUpdate[4].columnType=DataStructures::Table::NUMERIC;
		m_LastUpdate[4].cellValue.Set(static_cast<int>(GetNetworkManager().IsPasswordProtected()));
		m_LastUpdate[5].columnType=DataStructures::Table::BINARY;
		m_LastUpdate[5].cellValue.Set((char*)ips, size);
		m_LastUpdate[6].columnType=DataStructures::Table::NUMERIC;
		m_LastUpdate[6].cellValue.Set(GetNetworkManager().GetPort());
		m_LastUpdate[7].columnType=DataStructures::Table::STRING;
		m_LastUpdate[7].cellValue.Set(const_cast<char*>(comment.c_str()));
	}
	
	return changed;
}

void MasterServerInterface::RegisterHost(string gameType, string gameName, string comment)
{
	// Wait until pending registrations have cleared or that a certain interval has passed (don't want to hammer the server)
	if (m_PendingRegister || m_LastHostUpdateTime > time(0) - kMaxUpdateInterval)
		return;
		
	if (gameType.empty())
	{
		ErrorString("Empty game type given during host registration, aborting");
		SendToAllNetworkViews(kMasterServerEvent, kRegistrationFailedGameName);
		return;
	}
	if (gameName.empty())
	{
		ErrorString("Empty game name given during host registration, aborting");
		SendToAllNetworkViews(kMasterServerEvent, kRegistrationFailedGameType);
		return;
	}
	if (GetNetworkManager().GetPort() == 0)
	{
		ErrorString("It's not possible to register a host until it is running.");
		SendToAllNetworkViews(kMasterServerEvent, kRegistrationFailedNoServer);
		return;
	}
	
	m_GameType = gameType;
	m_HostName = gameName;
	m_HostComment = comment;

	GetNetworkManager().GetPeer()->AttachPlugin(m_HostDatabaseClient);

	if (!CheckServerConnection())
		return;
	
	PopulateUpdate();
	
	m_LastHostUpdateTime = time(0);

	m_HostDatabaseClient->UpdateRow(m_Version, gameType.c_str(), 0, RUM_UPDATE_OR_ADD_ROW, false, 0, m_LastUpdate, CELL_COUNT, m_MasterServerID, false );
	NetworkLog(NULL, "Sent host registration to master server, registering a %sNAT assisted game as\n \"%s\", %d, %d, %s, \"%s\"", 
		(GetNetworkManager().GetUseNat()) ? "" : "non-", 
		gameName.c_str(),
		GetNetworkManager().GetConnectionCount() + static_cast<int>(!m_IsDedicatedServer),
		GetNetworkManager().GetMaxConnections() + static_cast<int>(!m_IsDedicatedServer),
		(GetNetworkManager().IsPasswordProtected()) ? "password protected" : "not password protected",
		comment.c_str());
	
	m_Registered = true;
}

// Uses the game server peer
void MasterServerInterface::SendHostUpdate()
{
	// Wait until pending host updates are finished
	if (m_PendingHostUpdate) 
	{
		NetworkInfo(NULL, "Still waiting for a master server reponse to another host update, ignoring this update.");
		return;
	}
	
	if (!CheckServerConnection())
		return;

	if (!PopulateUpdate())
		return;

	m_LastHostUpdateTime = time(0);
	
	if (m_RowID == -1)
	{
		m_HostDatabaseClient->UpdateRow(m_Version, m_GameType.c_str(), 0, RUM_UPDATE_OR_ADD_ROW, false, 0, m_LastUpdate, CELL_COUNT, m_MasterServerID, false );
		NetworkInfo(NULL, "Sent new host update to master server");
	}
	else
	{
		m_HostDatabaseClient->UpdateRow(m_Version, m_GameType.c_str(), 0, RUM_UPDATE_OR_ADD_ROW, true, m_RowID, m_LastUpdate, CELL_COUNT, m_MasterServerID, false );
		NetworkInfo(NULL, "Sent host update to master server with identifier %d", m_RowID);
	}
	
	m_Registered = true;
}

void MasterServerInterface::Disconnect()
{
	m_Peer->Shutdown(200);
	m_Peer->DetachPlugin(&m_DatabaseClient);
}

// Uses the game server peer
void MasterServerInterface::UnregisterHost()
{
	if (GetNetworkManagerPtr())
	{
		if (GetNetworkManager().GetPeer()->IsConnected(m_MasterServerID))
			m_HostDatabaseClient->RemoveRow(m_GameType.c_str(), 0, m_RowID, m_MasterServerID, false);
		// Always detach DB plugin when running as a server, nothing will happen if it's not attached
		if (GetNetworkManager().IsServer())
			GetNetworkManager().GetPeer()->DetachPlugin(m_HostDatabaseClient);
	}
	
	// Reset some local variables
	m_RowID = -1;
	m_GameType = "";
	m_HostName = "";
	m_HostComment = "";
	m_Registered = false;
}

std::vector<HostData> MasterServerInterface::PollHostList()
{	
	return m_HostList;
}

void MasterServerInterface::ResetHostState()
{
	m_PendingRegister = false;
	m_PendingHostUpdate = false;
	m_Registered = false;
}



IMPLEMENT_CLASS (MasterServerInterface)
GET_MANAGER (MasterServerInterface)
GET_MANAGER_PTR (MasterServerInterface)
#endif
