#pragma once

#if ENABLE_PLAYERCONNECTION
#define ENABLE_LISTEN_SOCKET (!UNITY_FLASH)

#include "GeneralConnection.h"
#include "Runtime/Serialize/SwapEndianBytes.h"


#if UNITY_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#elif UNITY_XENON
#include <xtl.h>
#else 
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Threads/Mutex.h"
#if UNITY_ANDROID
#include <sys/un.h>
#endif

// flags are bits denoting capabilities of the broadcaster
enum PlayerConnectionInitiateMode
	{
	kPlayerConnectionInitiateByListening,
	kPlayerConnectionInitiateByConnecting
	};

class PlayerConnection : public GeneralConnection 
{
public:
	PlayerConnection (const std::string& dataPath = "", unsigned short multicastPort = PLAYER_MULTICAST_PORT, bool enableDebugging=false);

	static void Initialize (const std::string& dataPath, bool enableDebugging=false);
	static void Cleanup ();

	// Singleton accessor for playerconnection
	static PlayerConnection& Get ();
	static PlayerConnection* ms_Instance;

	void Poll ();
	inline bool AllowDebugging () { return (0 != m_AllowDebugging); }
	bool ShouldEnableProfiler() { return m_EnableProfiler != 0; }

	// ugly hack to fix gfx tests
	inline bool IsTestrigMode() { return (m_InitiateMode == kPlayerConnectionInitiateByConnecting); }
	inline bool HasBytesToSend() { return GeneralConnection::HasBytesToSend(); }

private:
	virtual bool IsServer() { return true; }
	bool ReadConfigFile (const std::string& dataPath);
	void CreateListenSocket ();
	void CreateUnixSocket();
	void InitializeMulticastAddress (UInt16 multicastPort);
	void PollListenMode ();
	void PollConnectMode ();
	void CreateAndReportConnection(TSocketHandle socketHandle);
	std::string ConstructWhoamiString ();

	bool ImmediateConnect () const
	{
		return ms_RunningUnitTests
		       || m_WaitingForPlayerConnectionBeforeStartingPlayback;
	}

#if ENABLE_LISTEN_SOCKET
	static void InitializeListenSocket(ServerSocket& socket, const std::string& localIP, int listenPort);
#endif
#if UNITY_ANDROID
	static void InitializeUnixSocket(ServerSocket& socket, const std::string& socketname);
#endif


private:
	bool    m_IsPlayerConnectionEnabled;
	PlayerConnectionInitiateMode		m_InitiateMode;
	bool	m_WaitingForPlayerConnectionBeforeStartingPlayback;

	// player specific
	unsigned short m_ListenPort;
	std::string m_HostName;
	std::string m_WhoAmI;
#if ENABLE_LISTEN_SOCKET
	ServerSocket m_ListenSocket; // player only
#endif
#if UNITY_ANDROID
	ServerSocket m_UnixSocket; // local
#endif
	
	UInt32 m_EditorGuid;
	int m_AllowDebugging;
	int m_EnableProfiler;
	int m_NumIPs;
	std::string m_ConnectToIP;
	char m_ConnectToIPList[10][16];
	
	ABSOLUTE_TIME m_LastMulticast;
};

void InstallPlayerConnectionLogging (bool install);
void TransferFileOverPlayerConnection(const std::string& fname, void* body, unsigned int length, void* header = 0, unsigned int headerLength = 0);
void NotifyFileReadyOverPlayerConnection(const std::string& fname);

#endif // ENABLE_PLAYERCONNECTION
