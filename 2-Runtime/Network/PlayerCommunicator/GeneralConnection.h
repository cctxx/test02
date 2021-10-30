#pragma once

#if ENABLE_PLAYERCONNECTION
#define ENABLE_MULTICAST (!UNITY_FLASH)

#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Runtime/Network/MulticastSocket.h"
#include "Runtime/Network/ServerSocket.h"
#include "Runtime/Network/SocketStreams.h"


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


// used ports:
//   MulticastPort : 54998
//   ListenPorts : 55000 - 55511
//   Multicast(unittests) : 55512 - 56023

#define ENABLE_PLAYER_CONNECTION_DEBUG_LOG 0

#if ENABLE_PLAYER_CONNECTION_DEBUG_LOG
#define LOG_PLAYER_CONNECTION(str) { \
	bool oldlog = IsLogEnabled(); \
	SetLogEnabled(false); \
	printf_console ("[%04x] %s\n", Thread::GetCurrentThreadID(), (str).c_str()); \
	SetLogEnabled(oldlog); \
}
#else
#define LOG_PLAYER_CONNECTION(str)
#endif

#if SUPPORT_THREADS
#define LOG_PLAYER_CONNECTION_CRITICAL(str) printf_console ("[%04x] %s\n", Thread::GetCurrentThreadID(), (str).c_str())
#else
#define LOG_PLAYER_CONNECTION_CRITICAL(str) printf_console ("[] %s\n", (str).c_str())
#endif

#define PLAYER_MULTICAST_GROUP "225.0.0.222"
#define PLAYER_MULTICAST_PORT 54997

#define PLAYER_DIRECTCONNECT_PORT 54999
#define PLAYER_DIRECTCONNECT_GUID 1337

#define PLAYER_DIRECT_IP_CONNECT_GUID 0xFEED

#define PLAYER_LISTEN_PORT 55000
#define PLAYER_UNITTEST_MULTICAST_PORT 55512
#define PLAYER_PORT_MASK 511
#define PLAYER_MESSAGE_MAGIC_NUMBER 0x4E8F

#define PLAYER_CONNECTION_CONFIG_DATA_FORMAT_LISTEN "listen %u %d %d %d"
#define PLAYER_CONNECTION_CONFIG_DATA_FORMAT_LISTEN_PRINT "listen <guid> <debugging> <waitonstartup> <startprofiler>"
#define PLAYER_CONNECTION_CONFIG_DATA_FORMAT_CONNECT_LIST "connect %s %s %s %s %s %s %s %s %s %s"
#define PLAYER_CONNECTION_CONFIG_DATA_FORMAT_CONNECT "connect %s"
#define PLAYER_CONNECTION_CONFIG_DATA_FORMAT_CONNECT_PRINT "connect <ip>"

#define SERVER_IDENTIFICATION_FORMAT "[IP] %s [Port] %lu [Flags] %lu [Guid] %lu [EditorId] %lu [Version] %d [Id] %s [Debug] %d"


#define ANY_PLAYERCONNECTION 0

bool SocketCallSucceeded(int res);

class GeneralConnection
{
public:
	enum MulticastFlags
	{
		kRequestImmediateConnect = 1<<0,
		kSupportsProfile = 1<<1,
		kCustomMessage = 1<<2
	};

	enum { kMulticastBufferSize = 256 };
	enum { kDataBufferMaxSize =  8*1024*1024 };
	enum { kDataBufferFlushThreshold =  7*1024*1024 };

#if ENABLE_MULTICAST
	// Parsed struct from a buffer that contains all the information sent by the player to the editor
	// Used for showing a list of all available profilers and performing autoconnect on the profiler
	struct MulticastInfo
	{
		MulticastInfo();

		bool Parse (const char* buffer, void* in = NULL);

		bool IsValid () const { return m_Valid; }
		bool IsLocalhost() const { return m_IsLocalhost; }

		UInt32 GetGuid () const { return m_Guid; }
		UInt32 GetEditorGuid () const { return m_EditorGuid; }

		UInt32 GetPort () const { return m_Port; }
		std::string const& GetIP () const { return m_Ip; }

		bool ImmediateConnect() const { return (m_Flags & kRequestImmediateConnect) != 0; }
		bool HasCustomMessage() const { return (m_Flags & kCustomMessage) != 0; }
		bool HasProfiler() const { return (m_Flags & kSupportsProfile) != 0; }

		std::string const& GetIdentifier() const { return m_Identifier; }
	private:

		void Clear ();
		void SetLocalhostFlag();

		std::string m_Buffer;
		std::string m_Ip;
		UInt32 m_Port;
		UInt32 m_Flags;
		UInt32 m_Guid;
		UInt32 m_EditorGuid;
		std::string m_Identifier;
		bool m_Valid;
		bool m_IsLocalhost;
	};
#endif

	enum MessageID{
		// messageID 1 - 31 reserved for (future) internal use
		kProfileDataMessage             = 32,
		kProfileStartupInformation      = 33,

		kObjectMemoryProfileSnapshot    = 40, //request the memory profile
		kObjectMemoryProfileDataMessage = 41, //send the object memory profile

		kLogMessage                     = 100,
		kCleanLogMessage                = 101,

		kFileTransferMessage            = 200,
		kFileReadyMessage               = 201,
		kCaptureHeaphshotMessage        = 202,

		kPingAliveMessage               = 300,
		kApplicationQuitMessage         = 301,
		kLastMessageID,
	};

	struct NetworkMessage
	{
		void InitializeMessage();
		bool CheckMessageValidity();
		void SetID (MessageID id);
		MessageID GetID ();
		void SetDataSize (UInt32 size);
		UInt32 GetDataSize ();

		bool AllowSkip(){return m_ID == kProfileDataMessage;}

	private:
		UInt16 m_MagicNumber; // in little endian
		UInt16 m_ID; // in little endian
		UInt32 m_Size; // in little endian
	};

	struct Connection
	{
	public:
		Connection(TSocketHandle socketHandle);
		~Connection();

		bool IsValid() const { return m_SocketStream.IsConnected(); };
		bool Poll() { return m_SocketStream.Poll(5); }
		bool HasBytesToSend() const { return m_SocketStream.SendBuffer().GetAvailableSize() > 0; }
		const void* ReceiveMessage(NetworkMessage* message);
		void ReleaseReceivedMessage();
		void SendMessage(NetworkMessage& message, const void* data);

		UInt32 GetSendBufferSize() const { return m_SocketStream.SendBuffer().GetAllocatedSize(); }
		UInt32 GetRecvBufferSize() const { return m_SocketStream.RecvBuffer().GetAllocatedSize(); }

	private:
		Mutex					m_SendMutex;
		Mutex					m_RecvMutex;
		NetworkMessage			m_PendingMessage;
		void*					m_PendingMessageData;
		bool					m_LocallyAllocatedMessageData;
		bool					m_HasPendingMessage;
#if UNITY_FLASH
		BufferedSocketStream	m_SocketStream;
#elif UNITY_WINRT
		BufferedSocketStream m_SocketStream;
#else
		ThreadedSocketStream	m_SocketStream;
#endif
	};

	virtual ~GeneralConnection ();

	bool IsConnected () { return !m_Connections.empty(); }
	void SetLogEnabled (bool v) { m_LogEnabled = v; }
	bool IsLogEnabled () const { return m_LogEnabled; }
	void DisconnectAll ();

	void SendMessage (UInt32 guid, MessageID id, const void* data, UInt32 size);

	typedef void (*MessageHandlerFunc) (const void* data, UInt32 size, UInt32 guid);
	void RegisterMessageHandler (MessageID messageID, MessageHandlerFunc func);
	void UnregisterMessageHandler (MessageID messageID, MessageHandlerFunc func);

	typedef void (*ConnectionHandlerFunc) (UInt32 guid);
	void RegisterDisconnectionHandler( ConnectionHandlerFunc func );
	void RegisterConnectionHandler( ConnectionHandlerFunc func );
	void UnregisterDisconnectionHandler( ConnectionHandlerFunc func );
	void UnregisterConnectionHandler( ConnectionHandlerFunc func );

	virtual void WaitForFinish();

	UInt32 GetLocalGuid() const;

	static void RunningUnitTest() { ms_RunningUnitTests = true;}

protected:
	GeneralConnection ();

	bool HasBytesToSend() const;

	static void Initialize ();
	static void Cleanup ();

	void RegisterConnection (UInt32 guid, TSocketHandle socketHandle);
	void Disconnect (UInt32 guid);

	Connection* GetConnection(UInt32 guid);

	virtual bool IsServer () = 0;

	void Poll ();

protected:
	std::string m_LocalIP;

#if ENABLE_MULTICAST
	MulticastSocket m_MulticastSocket;
#endif

	typedef std::map< int, Connection* > ConnectionMap;
	ConnectionMap m_Connections;

	std::map< MessageID, MessageHandlerFunc > m_HandlerMap;
	std::vector< ConnectionHandlerFunc > m_ConnectionHandlers;
	std::vector< ConnectionHandlerFunc > m_DisconnectionHandlers;

	UInt32 m_LocalGuid;

	bool m_LogEnabled;

	static bool ms_RunningUnitTests;
	static int ms_Version;
};

#endif // ENABLE_PLAYERCONNECTION
