#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK
#include "Runtime/GameCode/Behaviour.h"
#include "NetworkEnums.h"
#include "External/RakNet/builds/include/RakPeerInterface.h"
#include "External/RakNet/builds/include/LightweightDatabaseClient.h"
#include "External/RakNet/builds/include/MessageIdentifiers.h"
#include "Runtime/BaseClasses/ManagerContext.h"

#include <vector>

const int CELL_COUNT=8;



class MasterServerInterface : public GlobalGameManager
{
public:

	REGISTER_DERIVED_CLASS   (MasterServerInterface, GlobalGameManager)
	typedef std::vector<HostData> HostList;

	MasterServerInterface(MemLabelId label, ObjectCreationMode mode);
	// ~MasterServerInterface(); declared-by-macro
	
	virtual void NetworkOnApplicationQuit();
	virtual void NetworkUpdate();
	
	void ClientConnect();
	void ServerConnect();
	bool CheckServerConnection();
	void QueryHostList();
	void QueryHostList(string gameType);
	void ClearHostList();
	void RegisterHost(string gameType, string gameName, string comment);
	void SendHostUpdate();
	void UnregisterHost();
	void Disconnect();
	HostList PollHostList();
	void ProcessPacket(Packet *packet);
	void ResetHostState();
	
	string GetIPAddress() { return string(m_MasterServerID.ToString(false)); }
	void SetIPAddress(std::string address) { m_MasterServerID.SetBinaryAddress(address.c_str()); }
	int GetPort() {	return m_MasterServerID.port; }
	void SetPort(int port) { m_MasterServerID.port = port; }
	SystemAddress& GetMasterServerID() { return m_MasterServerID; }
	
	void SetUpdateRate(int rate) { m_UpdateRate = rate; }
	int GetUpdateRate() { return m_UpdateRate; }
	
	bool PopulateUpdate();
	bool PopulateUpdate(string gameName, string comment);
	
	void SetDedicatedServer(bool value) { m_IsDedicatedServer = value; };
	bool GetDedicatedServer() { return m_IsDedicatedServer; };

private:
	void ResolveMasterServerAddress();
	
	RakPeerInterface *m_Peer;
	LightweightDatabaseClient m_DatabaseClient;
	LightweightDatabaseClient *m_HostDatabaseClient;
	bool			 m_PendingRegister;
	bool			 m_PendingQuery;
	bool			 m_PendingHostUpdate;
	string			 m_GameType;
	string			 m_HostName;
	string			 m_HostComment;
	HostList		 m_HostList;
	unsigned int	 m_RowID;
	bool			 m_Registered;
	time_t			 m_LastHostUpdateTime;
	SystemAddress    m_MasterServerID;
	char			 m_Version[3];
	int				 m_UpdateRate;
	DatabaseCellUpdate m_LastUpdate[CELL_COUNT];
	bool			 m_IsDedicatedServer;
	time_t			 m_ShutdownTimer;
};

MasterServerInterface* GetMasterServerInterfacePtr ();
MasterServerInterface& GetMasterServerInterface ();

#endif
