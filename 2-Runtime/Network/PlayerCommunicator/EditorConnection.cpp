#include "UnityPrefix.h"

#if UNITY_EDITOR

#include "EditorConnection.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Misc/SystemInfo.h"

#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/CacheWrap.h"

#include "Runtime/Network/NetworkUtility.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Threads/Thread.h"

#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Network/PlayerCommunicator/GeneralConnectionInternals.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"

static const char* kLastConnectedPlayer = "LastConnectedPlayer";

EditorConnection* EditorConnection::ms_Instance = NULL;


EditorConnection::EditorConnection(unsigned short multicastPort)
: GeneralConnection()
{
	ABSOLUTE_TIME_INIT(m_LastCleanup);
	if (!m_MulticastSocket.Initialize(PLAYER_MULTICAST_GROUP, multicastPort))
		ErrorStringMsg("Failed to setup multicast socket for player connection.");
	if (!m_MulticastSocket.Join())
		ErrorStringMsg("Unable to join player connection multicast group.");
}

EditorConnection& EditorConnection::Get()
{
	return *ms_Instance;
}

void EditorConnection::Initialize ()
{
	GeneralConnection::Initialize();
	Assert(ms_Instance == NULL);
	ms_Instance = new EditorConnection();
}
void EditorConnection::Cleanup ()
{
	Assert(ms_Instance != NULL);
	delete ms_Instance;
	ms_Instance = NULL;
	GeneralConnection::Cleanup();
}

int EditorConnection::ConnectPlayerDirectIP(const std::string& IP)
{
	Connection* cnx = GetConnection(PLAYER_DIRECT_IP_CONNECT_GUID);
	if (cnx && cnx->IsValid())
		return PLAYER_DIRECT_IP_CONNECT_GUID;

	int socketHandle = -1;
	int timeout = 1;
	// Poll at 1 ms, 10 ms, and 100 ms before giving up
	while (socketHandle == -1)
 	{
		// A forced repaint is required, since this method is a blocking call
		LogStringMsg("Attempting to connect to player ip: %s with a %d ms timeout", IP.c_str(), timeout);
		ForceRepaintOnAllViews();

		for(int i = 0; i <= PLAYER_PORT_MASK; i++)
		{
			socketHandle = ::Socket::Connect(IP.c_str(), PLAYER_LISTEN_PORT + i, timeout, true, false);
			if(socketHandle != -1)
				break;
		}
		if (socketHandle != -1)
 			break;
		
		timeout *= 10;
		if (timeout > 100)
			break;
	}
	if(socketHandle == -1)
	{
		ErrorStringMsg("Failed to connect to player ip: %s", IP.c_str());
		return -1;
	}

	RegisterConnection(PLAYER_DIRECT_IP_CONNECT_GUID, socketHandle);
	if (!ms_RunningUnitTests)
		EditorPrefs::SetString(kLastConnectedPlayer, "");

	return PLAYER_DIRECT_IP_CONNECT_GUID;
}

int EditorConnection::ConnectPlayer (UInt32 guid)
{
	AvailablePlayerMap::iterator found = m_AvailablePlayers.find(guid);
	if (found == m_AvailablePlayers.end())
		return -1;

	MulticastInfo& multicastInfo = found->second.m_MulticastInfo;
	if(!multicastInfo.IsValid())
		return -1;

	Assert(multicastInfo.GetGuid() == guid);
	Connection* cnx = GetConnection(guid);
	if (cnx && cnx->IsValid())
		return guid;

	int socketHandle;
	if (SOCK_ERROR(socketHandle = ::Socket::Connect(multicastInfo.GetIP().c_str(), multicastInfo.GetPort(), 500 /*timeout*/)))
	{
		ErrorStringMsg("Failed to connect to player ip: %s, port: %d", multicastInfo.GetIP().c_str(), multicastInfo.GetPort());
		return 0;
	}

	RegisterConnection(guid, socketHandle);
	if (!ms_RunningUnitTests)
		EditorPrefs::SetString(kLastConnectedPlayer, multicastInfo.GetIdentifier());
	
	return guid;
}

UInt32 EditorConnection::AddPlayer(std::string hostName, std::string localIP, unsigned short port, UInt32 guid, int flags)
{
	std::string buffer = Format(SERVER_IDENTIFICATION_FORMAT, localIP.c_str(), port, flags, guid, -1, ms_Version, hostName.c_str(), 0 );
	MulticastInfo multicastInfo;
	if(multicastInfo.Parse(buffer.c_str()))
	{
		AvailablePlayerMap::iterator found = m_AvailablePlayers.find(multicastInfo.GetGuid());
		if(found != m_AvailablePlayers.end())
		{
			// entry already in the list - renew the timestamp
			found->second.m_LastPing = START_TIME;
		}
		else
		{
			m_AvailablePlayers.insert (std::make_pair(multicastInfo.GetGuid(), AvailablePlayer(START_TIME, multicastInfo))).first;
		}
		return multicastInfo.GetGuid();
	}
	return 0;
}

void EditorConnection::RemovePlayer(UInt32 guid)
{
	Disconnect(guid);
	AvailablePlayerMap::iterator found = m_AvailablePlayers.find(guid);
	if(found != m_AvailablePlayers.end())
		m_AvailablePlayers.erase(found);
}

void EditorConnection::EnablePlayer(UInt32 guid, bool enable)
{
	AvailablePlayerMap::iterator player = m_AvailablePlayers.find(guid);
	if (player != m_AvailablePlayers.end())
		player->second.m_Enabled = enable;
}

void 
EditorConnection::ResetLastPlayer()
{
	EditorPrefs::SetString(kLastConnectedPlayer, "");
}

void EditorConnection::GetAvailablePlayers( std::vector<UInt32>& values )
{
	AvailablePlayerMap::iterator it = m_AvailablePlayers.begin();
	for( ; it != m_AvailablePlayers.end(); ++it){
		if (it->second.m_Enabled && it->second.m_MulticastInfo.GetIP() == m_LocalIP)
			values.push_back(it->second.m_MulticastInfo.GetGuid());
	}
	it = m_AvailablePlayers.begin();
	for( ; it != m_AvailablePlayers.end(); ++it){
		if (it->second.m_Enabled && it->second.m_MulticastInfo.GetIP() != m_LocalIP)
			values.push_back(it->second.m_MulticastInfo.GetGuid());
	}
}

void EditorConnection::GetAvailablePlayers(RuntimePlatform platform, vector<UInt32>& values)
{
	string const id = systeminfo::GetRuntimePlatformString(platform);

	for (AvailablePlayerMap::const_iterator it = m_AvailablePlayers.begin() ; it != m_AvailablePlayers.end(); ++it)
	{
		AvailablePlayer const& player = it->second;
		if (player.m_Enabled && BeginsWith(player.m_MulticastInfo.GetIdentifier(), id) && (player.m_MulticastInfo.GetIP() == m_LocalIP))
			values.push_back(player.m_MulticastInfo.GetGuid());
	}

	for (AvailablePlayerMap::const_iterator it = m_AvailablePlayers.begin() ; it != m_AvailablePlayers.end(); ++it)
	{
		AvailablePlayer const& player = it->second;
		if (player.m_Enabled && BeginsWith(player.m_MulticastInfo.GetIdentifier(), id) && (player.m_MulticastInfo.GetIP() != m_LocalIP))
			values.push_back(player.m_MulticastInfo.GetGuid());
	}
}

std::string EditorConnection::GetConnectionIdentifier( UInt32 guid )
{
	AvailablePlayerMap::iterator it = m_AvailablePlayers.find(guid);
	if (it != m_AvailablePlayers.end())
		return it->second.m_MulticastInfo.GetIdentifier();
	return "None";	
}

bool EditorConnection::IsIdentifierConnectable( UInt32 guid )
{
	AvailablePlayerMap::iterator it = m_AvailablePlayers.find(guid);
	if (it != m_AvailablePlayers.end())
		return it->second.m_MulticastInfo.IsValid();
	return false;	
}

bool EditorConnection::IsIdentifierOnLocalhost( UInt32 guid )
{
	AvailablePlayerMap::iterator it = m_AvailablePlayers.find(guid);
	if (it != m_AvailablePlayers.end())
		return it->second.m_MulticastInfo.IsLocalhost();
	return false;
}

GeneralConnection::MulticastInfo EditorConnection::PollWithCustomMessage()
{
	MulticastInfo multicastInfo;

	sockaddr_in srcaddr;
	socklen_t srcaddr_len = sizeof(srcaddr);
	char buffer[kMulticastBufferSize];
	RecvUserData recvData;
	recvData.srcAddr = (struct sockaddr*)&srcaddr;
	recvData.srcLen = srcaddr_len;
	int size = m_MulticastSocket.Recv(buffer, kMulticastBufferSize, &recvData);
	if (!SOCK_ERROR(size))
	{
		// Ensure that buffer is null terminated string
		buffer[size] = 0;
		if(multicastInfo.Parse(buffer, &srcaddr))
		{
			AvailablePlayerMap::iterator found = m_AvailablePlayers.find(multicastInfo.GetGuid());
			if(found != m_AvailablePlayers.end())
			{
				// entry already in the list - renew the timestamp
				found->second.m_LastPing = START_TIME;
			}
			else
			{
				// remove old player if new one has connected
				for (AvailablePlayerMap::const_iterator it = m_AvailablePlayers.begin(); it != m_AvailablePlayers.end(); ++it)
					if (!it->second.m_Enabled && !StrCmp(it->second.m_MulticastInfo.GetIdentifier(), multicastInfo.GetIdentifier()))
					{
						RemovePlayer(it->first);
						break;
					}

				m_AvailablePlayers.insert (std::make_pair(multicastInfo.GetGuid(), AvailablePlayer(START_TIME, multicastInfo)));
				if (multicastInfo.IsValid() && multicastInfo.ImmediateConnect())
				{
					bool atemptAutoConnect;
					if (ms_RunningUnitTests)
					{
						atemptAutoConnect = false;
						if (!IsConnected())
						{
							const char* connectingIp = multicastInfo.GetIP().c_str();

							const int maxNumberOfIps = 10;
							char localIps[maxNumberOfIps][16];
							GetIPs(localIps);
							for (int i = 0; i < maxNumberOfIps; ++i)
							{
								if (strcmp(localIps[i], connectingIp) == 0)
								{
									atemptAutoConnect = true;
									break;
								}
							}
						}
					}
					else
					{
						string lastPlayerId = EditorPrefs::GetString(kLastConnectedPlayer);
						
						// No prior player are known and new player is built by us and requires autoconnection
						if (lastPlayerId.empty() && multicastInfo.GetEditorGuid() == GetLocalGuid())
							atemptAutoConnect = true;
						else
							atemptAutoConnect = (lastPlayerId == multicastInfo.GetIdentifier());
					}

					if (atemptAutoConnect)
						ConnectPlayer(multicastInfo.GetGuid());
				}
			}
		}
		else
		{
			// can not connect to this player (version mismatch)

		}
	}	

	// clean up the list every 2 seconds ( players not pinging in 10 seconds are removed from the map )
	if(GetProfileTime(ELAPSED_TIME(m_LastCleanup)) > 2*kTimeSecond)
	{
		m_LastCleanup = START_TIME;
		AvailablePlayerMap::iterator it = m_AvailablePlayers.begin();
		while (it != m_AvailablePlayers.end())
		{
			UInt32 guid = it->first;
			AvailablePlayer& player = it->second;
			if (player.m_MulticastInfo.GetGuid() == PLAYER_DIRECTCONNECT_GUID)
			{
				++it;
				continue;
			}
			if (!GetConnection(guid) && (GetProfileTime(ELAPSED_TIME(player.m_LastPing)) > 10*kTimeSecond))
			{
				AvailablePlayerMap::iterator erase = it++;
				m_AvailablePlayers.erase(erase);
			}
			else
				++it;
		}
	}

	GeneralConnection::Poll();
	
	return multicastInfo;
}

#endif // ENABLE_PLAYERCONNECTION
