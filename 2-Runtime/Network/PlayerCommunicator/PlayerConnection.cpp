#include "UnityPrefix.h"

#if ENABLE_PLAYERCONNECTION

#include "PlayerConnection.h"
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
#include "Runtime/Network/SocketConsts.h"

#if UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#include "PlatformDependent/AndroidPlayer/DVMCalls.h"
#endif

#define ALL_INTERFACES_IP "0.0.0.0"

const char* kPlayerConnectionConfigFile = "PlayerConnectionConfigFile";

PlayerConnection* PlayerConnection::ms_Instance = NULL;

PlayerConnection::PlayerConnection(const std::string& dataPath, unsigned short multicastPort, bool enableDebugging)
: GeneralConnection()
, m_WaitingForPlayerConnectionBeforeStartingPlayback(false)
#if ENABLE_LISTEN_SOCKET
	, m_ListenSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
#endif
#if UNITY_ANDROID
, m_UnixSocket(AF_LOCAL, SOCK_STREAM, 0)
#endif
{
	ABSOLUTE_TIME_INIT(m_LastMulticast);

	m_IsPlayerConnectionEnabled = false;

	bool hasConfigFile = ReadConfigFile(dataPath);
	if (!hasConfigFile)
		m_AllowDebugging = (enableDebugging? 1: 0);

	if (!PLATFORM_SUPPORTS_PLAYERCONNECTION_LISTENING && !hasConfigFile)
	{
		printf_console("PlayerConnection disabled  - listening mode not supported\n");
		return;
	}

	m_IsPlayerConnectionEnabled = true;

	if (m_InitiateMode == kPlayerConnectionInitiateByConnecting)
	{
		for (int i = 0; i < m_NumIPs; ++i)
		{
			m_ConnectToIP = m_ConnectToIPList[i];
#if UNITY_FLASH
			Ext_GetSocketPolicyFile(m_ConnectToIP.c_str());
#endif
			printf_console("Connecting directly to [%s]...\n", m_ConnectToIP.c_str());
			// Try to connect to next IP
			Poll();
			if (IsConnected())
			{
				break;
			}
		}

		if (!IsConnected())
		{
			ErrorString("Connecting to host failed, aborting playback");
#if UNITY_XENON
			XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
#elif !UNITY_FLASH // Cant handle exit
			exit(1);
#endif
		}

		return;
	}

	// so we are in listening mode.

	CreateListenSocket ();
#if UNITY_ANDROID
	CreateUnixSocket();
#endif
	m_HostName = GetHostName ();
	std::replace (m_HostName.begin (), m_HostName.end (), ' ', '_');
	m_WhoAmI = ConstructWhoamiString ();
	InitializeMulticastAddress (multicastPort);

	if (m_WaitingForPlayerConnectionBeforeStartingPlayback)
	{
		ABSOLUTE_TIME startTime = START_TIME;
		printf_console("Waiting for connection from host on [%s:%i]...\n", m_LocalIP.c_str(), (int)m_ListenPort);

		// Try to connect for some time
		while((GetProfileTime(ELAPSED_TIME(startTime)) < kPlayerConnectionInitialWaitTimeout) && (!IsConnected()))
		{
			Poll();
			Thread::Sleep(0.05);
		}
	}

	if (!IsConnected() && m_WaitingForPlayerConnectionBeforeStartingPlayback)
		printf_console("Timed out. Continuing without host connection.\n");
}

std::string PlayerConnection::ConstructWhoamiString ()
{
	std::string runtimeAndHostName = Format ("%s(%s)",
	                                         systeminfo::GetRuntimePlatformString ().c_str (),
	                                         m_HostName.c_str ());
	UInt32 flags = (ImmediateConnect () ? kRequestImmediateConnect : 0);
	flags |= (ENABLE_PROFILER ? kSupportsProfile : 0);
	std::string whoAmI = Format (SERVER_IDENTIFICATION_FORMAT,
	                             m_LocalIP.c_str (), (UInt32)m_ListenPort,
	                             flags, m_LocalGuid,
	                             m_EditorGuid, ms_Version,
	                             runtimeAndHostName.c_str (),
	                             m_AllowDebugging);
	return whoAmI;
}

void PlayerConnection::InitializeMulticastAddress (UInt16 multicastPort)
{
	Assert (m_InitiateMode == kPlayerConnectionInitiateByListening);

#if !UNITY_FLASH
	// We use broadcast in case of XBOX360 or AdHoc connection
	// For AdHoc connections we need to specify Auto IP broadcast address instead of 255.255.255.255
	if (UNITY_XENON || m_LocalIP.find("169.254") == 0)
	{
		const char* broadcastAddress = (UNITY_XENON || UNITY_BB10) ? "255.255.255.255" : "169.254.255.255";
		if (!m_MulticastSocket.Initialize(broadcastAddress, multicastPort))
			ErrorString("Unable to setup multicast socket for player connection.");
		if (!m_MulticastSocket.SetBroadcast(true))
			ErrorString("Unable to set broadcast mode for player connection socket.");
		printf_console("Broadcasting \"%s\" to [%s:%i]...\n", m_WhoAmI.c_str(), broadcastAddress, (int)multicastPort);
	}
	// For all other cases we use multicast address
	else
	{
		if (!m_MulticastSocket.Initialize(PLAYER_MULTICAST_GROUP, multicastPort))
			ErrorString("Unable to setup multicast socket for player connection.");
		printf_console("Multi-casting \"%s\" to [%s:%i]...\n", m_WhoAmI.c_str(), PLAYER_MULTICAST_GROUP, (int)multicastPort);
		#if UNITY_EDITOR
			m_MulticastSocket.SetTTL(ms_RunningUnitTests ? 0 : 31);
		#else
			m_MulticastSocket.SetTTL(31);
		#endif
		m_MulticastSocket.SetLoop(true);
	}


#endif
}


void PlayerConnection::CreateListenSocket ()
{
	Assert (m_InitiateMode == kPlayerConnectionInitiateByListening);

	// create a random listen port (will be send out with the multicast ping)
	Rand r (GetProfileTime(START_TIME));
	m_ListenPort = PLAYER_LISTEN_PORT + (r.Get() & PLAYER_PORT_MASK);

#if ENABLE_LISTEN_SOCKET
	InitializeListenSocket(m_ListenSocket, ALL_INTERFACES_IP, m_ListenPort);
#endif
}

#if ENABLE_LISTEN_SOCKET
void PlayerConnection::InitializeListenSocket(ServerSocket& socket, const std::string& localIP, int listenPort)
{
	printf_console("PlayerConnection initialized network socket : %s %i\n", localIP.c_str(), listenPort);
	socket.StartListening(localIP.c_str(), listenPort, false);
}
#endif

#if UNITY_ANDROID
void PlayerConnection::CreateUnixSocket ()
{
	Assert (m_InitiateMode == kPlayerConnectionInitiateByListening);
	InitializeUnixSocket (m_UnixSocket, Format("Unity-%s", DVM::GetPackageName()));
}
#endif

#if UNITY_ANDROID
void PlayerConnection::InitializeUnixSocket (ServerSocket& socket, const std::string& name)
{
	printf_console("PlayerConnection initialized unix socket : %s\n", name.c_str());
	size_t len = name.length();

	struct sockaddr_un address;
	Assert (len < sizeof(address.sun_path));
	memset(&address, 0, sizeof(sockaddr_un));
	memcpy(address.sun_path + 1, name.data(), len);
	address.sun_path[0] = 0;
	address.sun_family = AF_LOCAL;

	socklen_t address_len = offsetof(struct sockaddr_un, sun_path) + len + 1;
	socket.StartListening((const sockaddr *) &address, address_len, false);
}
#endif

bool PlayerConnection::ReadConfigFile (const std::string& dataPath)
{
	m_InitiateMode = kPlayerConnectionInitiateByListening;
	m_EditorGuid = -1;
	m_AllowDebugging = 0;
	m_EnableProfiler = 0;
	m_WaitingForPlayerConnectionBeforeStartingPlayback = 0;
	int tmpWaiting = 0;

	std::string configFile = AppendPathName(dataPath, kPlayerConnectionConfigFile);
	if (!IsFileCreated(configFile))
		return false;

	InputString confData;
	ReadStringFromFile(&confData, configFile);
	char tmp[100];

	if (sscanf(confData.c_str(), PLAYER_CONNECTION_CONFIG_DATA_FORMAT_LISTEN, (unsigned*)&m_EditorGuid, &m_AllowDebugging, &tmpWaiting, &m_EnableProfiler) == 4)
	{
		m_WaitingForPlayerConnectionBeforeStartingPlayback = tmpWaiting;
		m_InitiateMode = kPlayerConnectionInitiateByListening;
		return true;
	}

	m_NumIPs = sscanf(confData.c_str(), PLAYER_CONNECTION_CONFIG_DATA_FORMAT_CONNECT_LIST, m_ConnectToIPList[0], m_ConnectToIPList[1], m_ConnectToIPList[2],
		 			m_ConnectToIPList[3], m_ConnectToIPList[5],  m_ConnectToIPList[6],  m_ConnectToIPList[7],  m_ConnectToIPList[8],  m_ConnectToIPList[9]);

	if (m_NumIPs > 0)
	{
		m_InitiateMode = kPlayerConnectionInitiateByConnecting;
		return true;
	}

	ErrorString(Format("PlayerConnection config should be in the format: \"%s\" or \"%s\"", PLAYER_CONNECTION_CONFIG_DATA_FORMAT_LISTEN_PRINT, PLAYER_CONNECTION_CONFIG_DATA_FORMAT_CONNECT_PRINT));
	return false;
}

PlayerConnection& PlayerConnection::Get()
{
	return *ms_Instance;
}

void PlayerConnection::Initialize (const std::string& dataPath, bool enableDebugging)
{
	if (ms_Instance == NULL)
	{
		SET_ALLOC_OWNER(NULL);
		printf_console("PlayerConnection initialized from %s (debug = %i)\n", dataPath.c_str(), enableDebugging);
		GeneralConnection::Initialize();
		ms_Instance = new PlayerConnection(dataPath, PLAYER_MULTICAST_PORT, enableDebugging);
	}
	else
	{
		if (ms_Instance->m_IsPlayerConnectionEnabled)
		{
			switch (ms_Instance->m_InitiateMode)
			{
			case kPlayerConnectionInitiateByListening:
				printf_console("PlayerConnection already initialized - listening to [%s:%i]\n", ms_Instance->m_LocalIP.c_str(), (int)ms_Instance->m_ListenPort);
				break;
			case kPlayerConnectionInitiateByConnecting:
				printf_console("PlayerConnection already initialized - connecting to [%s:%i]\n", ms_Instance->m_ConnectToIP.c_str(), PLAYER_DIRECTCONNECT_PORT);
				break;
			default:
				printf_console("PlayerConnection already initialized - unknown mode\n");
				break;
			}
		}
		else
		{
			printf_console("PlayerConnection already initialized, but disabled\n");
		}
	}
}

void PlayerConnection::Cleanup ()
{
	Assert(ms_Instance != NULL);
	delete ms_Instance;
	ms_Instance = NULL;
	GeneralConnection::Cleanup();
}

void PlayerConnection::PollListenMode()
{
	Assert (m_InitiateMode == kPlayerConnectionInitiateByListening);
#if ENABLE_LISTEN_SOCKET
	if(!m_IsPlayerConnectionEnabled )
		return;

	if (!IsConnected() || GetProfileTime(ELAPSED_TIME(m_LastMulticast)) > 1*kTimeSecond)
	{
		TSocketHandle socketHandle;
#if UNITY_WINRT
		if(m_ListenSocket.IsListening() && !SOCK_ERROR(socketHandle = m_ListenSocket.Accept()))
		{
			printf_console("PlayerConnection accepted from WinRT socket\n");
			CreateAndReportConnection(socketHandle);
		}
#endif
#if UNITY_ANDROID
		if (m_UnixSocket.IsListening() && !SOCK_ERROR(socketHandle = m_UnixSocket.Accept()))
		{
			printf_console("PlayerConnection accepted from unix socket\n");
			CreateAndReportConnection(socketHandle);
		}
#endif
#if !UNITY_WINRT
		// player looking for connections
		struct sockaddr_in remoteAddr;
		socklen_t remoteAddrLen = sizeof(remoteAddr);
		if(m_ListenSocket.IsListening() && !SOCK_ERROR(socketHandle = m_ListenSocket.Accept((sockaddr*)&remoteAddr, &remoteAddrLen)))
		{
			printf_console("PlayerConnection accepted from [%s]\n", InAddrToIP(&remoteAddr).c_str());
			CreateAndReportConnection(socketHandle);
		}
#endif

		// broadcast ip and port with 1 sec interval
		// 10ms interval if immediate connect is set
		UInt64 interval = 1*kTimeSecond;
		if (!IsConnected() && ImmediateConnect())
			interval = 10*kTimeMillisecond;

		if (GetProfileTime(ELAPSED_TIME(m_LastMulticast)) > interval)
		{
			m_LastMulticast = START_TIME;
			m_MulticastSocket.Send(m_WhoAmI.c_str (), m_WhoAmI.length () + 1);
		}
	}
#endif
}

void PlayerConnection::CreateAndReportConnection(TSocketHandle socketHandle)
{
	RegisterConnection(NextGUID(), socketHandle);
}

void PlayerConnection::PollConnectMode()
{
	Assert (m_InitiateMode == kPlayerConnectionInitiateByConnecting);

	if(!m_IsPlayerConnectionEnabled )
		return;

	if (IsConnected())
		return;

	int port = PLAYER_DIRECTCONNECT_PORT;
	TSocketHandle socketHandle;
	if (SOCK_ERROR(socketHandle = ::Socket::Connect(m_ConnectToIP.c_str(), port)))
	{
		ErrorStringMsg("Connect failed for direct socket. Ip=%s, port=%d", m_ConnectToIP.c_str(), port);
		return;
	}
	CreateAndReportConnection(socketHandle);
}


void PlayerConnection::Poll()
{
	GeneralConnection::Poll();

	switch(m_InitiateMode)
	{
		case kPlayerConnectionInitiateByListening:
		PollListenMode ();
			break;
		case kPlayerConnectionInitiateByConnecting:
		PollConnectMode ();
			break;
	}
}

static int custom_asprintf(char** buffer, const char* log, va_list alist)
{
	va_list list, sizelist;
	va_copy (list, alist);
	va_copy (sizelist, alist);
	int result = 0;

#if USE_WINSOCK_APIS || UNITY_WINRT
	int bufferSize = _vscprintf(log, list) + 1;
	*buffer = (char *)UNITY_MALLOC_ALIGNED(kMemUtility, bufferSize, 4);
	result = vsnprintf(*buffer, bufferSize, log, list);
#elif UNITY_PS3 || defined(__GNUC__)
    int bufferSize = vsnprintf(0, 0, log, sizelist) + 1;
	*buffer = (char *)UNITY_MALLOC_ALIGNED(kMemUtility, bufferSize, 4);;
	result = vsnprintf(*buffer, bufferSize, log, list);
#else
#error "Not implemented"
#endif

	va_end (sizelist);
	va_end (list);
	return result;
}

void LogToPlayerConnectionMessage(LogType logType, PlayerConnection::MessageID msgId, const char* log, va_list alist)
{
	va_list list;
	va_copy (list, alist);

	PlayerConnection& pc = PlayerConnection::Get();
	if (pc.IsConnected() && pc.IsLogEnabled())
	{

		// don't try to recursively sent logs from inside player connection over player connection
		pc.SetLogEnabled (false);

		char* buffer = NULL;
		int len = custom_asprintf(&buffer, log, list);

		if (len >= 0 && buffer && buffer[0] != 0)
			PlayerConnection::Get().SendMessage(ANY_PLAYERCONNECTION, msgId, buffer, len);

		if (buffer)
			UNITY_FREE (kMemUtility, buffer);

		pc.SetLogEnabled (true);
	}
	va_end (list);
}

bool PlainLogToPlayerConnection (LogType logType, const char* log, va_list alist)
{
	va_list list;
	va_copy (list, alist);
	LogToPlayerConnectionMessage(logType, PlayerConnection::kLogMessage, log, list);
	va_end (list);
	return true;
}

bool CleanLogToPlayerConnection (LogType logType, const char* log, va_list alist)
{
	va_list list;
	va_copy (list, alist);
	LogToPlayerConnectionMessage(logType, PlayerConnection::kCleanLogMessage, log, list);
	va_end (list);
	return true;
}

void InstallPlayerConnectionLogging (bool install)
{
	if (install)
	{
		SetLogEntryHandler(&PlainLogToPlayerConnection);
		AddCleanLogEntryHandler(&CleanLogToPlayerConnection);
	}
	else
	{
		SetLogEntryHandler(NULL);
	}
}

void TransferFileOverPlayerConnection(const std::string& fname, void* body, unsigned int length, void* header, unsigned int headerLength)
{
#if !UNITY_EDITOR
	printf_console("about to send file over playerconnection %s  with length %d\n",fname.c_str(),length);

	dynamic_array<UInt8> buffer;

	MemoryCacheWriter memoryCache (buffer);
	CachedWriter writeCache;

	unsigned int fnameLength = fname.length();

	unsigned int fnameLengthLE = fnameLength;
	unsigned int lengthLE = length + headerLength;
	SwapEndianBytesNativeToLittle(fnameLengthLE);
	SwapEndianBytesNativeToLittle(lengthLE);

	writeCache.InitWrite (memoryCache);
	writeCache.Write(&fnameLengthLE, sizeof(fnameLengthLE));
	writeCache.Write((void*)fname.c_str(), fnameLength);
	writeCache.Write(&lengthLE, sizeof(lengthLE));
	if (headerLength > 0)
		writeCache.Write(header, headerLength);
	writeCache.Write(body, length);

	writeCache.CompleteWriting();

	PlayerConnection::Get().SendMessage(ANY_PLAYERCONNECTION, GeneralConnection::kFileTransferMessage, &buffer[0], buffer.size());

	// ugly hack to fix gfx tests
	PlayerConnection& playercnx = PlayerConnection::Get();
	while (playercnx.IsTestrigMode())
	{
		playercnx.Poll();
		if (!playercnx.HasBytesToSend())
			break;

		Thread::Sleep(0.005);
	}

#endif //!UNITY_EDITOR
}

void NotifyFileReadyOverPlayerConnection(const std::string& fname)
{
#if !UNITY_EDITOR
	dynamic_array<UInt8> buffer;

	MemoryCacheWriter memoryCache (buffer);
	CachedWriter writeCache;

	unsigned int fnameLength = fname.length();
	unsigned int fnameLengthLE = fnameLength;
	SwapEndianBytesNativeToLittle(fnameLengthLE);

	writeCache.InitWrite (memoryCache);
	writeCache.Write(&fnameLengthLE, sizeof(fnameLengthLE));
	writeCache.Write((void*)fname.c_str(), fnameLength);

	writeCache.CompleteWriting();

	PlayerConnection::Get().SendMessage(ANY_PLAYERCONNECTION, GeneralConnection::kFileReadyMessage, &buffer[0], buffer.size());
#endif //!UNITY_EDITOR
}


#endif // ENABLE_PLAYERCONNECTION
