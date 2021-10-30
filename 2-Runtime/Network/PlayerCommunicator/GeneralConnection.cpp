#include "UnityPrefix.h"

#if ENABLE_PLAYERCONNECTION

#include "GeneralConnection.h"
#include "GeneralConnectionInternals.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Network/NetworkUtility.h"
#include "Runtime/Profiler/TimeHelper.h"

bool GeneralConnection::ms_RunningUnitTests = false;
int GeneralConnection::ms_Version = 0x00100100;

GeneralConnection::GeneralConnection ()
:	m_LogEnabled(true)
{
	char ips[10][16];
	int count = GetIPs(ips);
	if (count != 0)
		m_LocalIP = ips[0];
	else
		m_LocalIP = "0.0.0.0";

#if UNITY_BB10
	for(int x = 0; x < sizeof(ips); x++)
	{
		if(std::string(ips[x]).find("169.254") == 0)
		{
			m_LocalIP = ips[x];
			break;
		}
	}
#endif

	m_LocalGuid = Rand((int)GetProfileTime(START_TIME)).Get();
	// We reserve ANY_PLAYERCONNECTION guid for special use
	if (m_LocalGuid == ANY_PLAYERCONNECTION) m_LocalGuid = ANY_PLAYERCONNECTION ^ 1;
}

UInt32 GeneralConnection::GetLocalGuid() const
{
	return m_LocalGuid;
}

void GeneralConnection::WaitForFinish()
{
	int msgWait = 60;

	while (HasBytesToSend())
	{
		if (msgWait++ == 60)
		{
			printf_console("Waiting for finish\n");
			msgWait = 0;
		}

		Poll();
		Thread::Sleep(0.05);
	}
}

bool GeneralConnection::HasBytesToSend() const
{
	ConnectionMap::const_iterator it = m_Connections.begin();
	for ( ; it != m_Connections.end(); ++it)
		if (it->second->HasBytesToSend())
			return true;

	return false;
}

GeneralConnection::~GeneralConnection()
{
	DisconnectAll();
}


void GeneralConnection::DisconnectAll()
{
	ConnectionMap::iterator it = m_Connections.begin();
	std::vector< int > disconnectGuids;
	for( ; it != m_Connections.end(); ++it)
		disconnectGuids.push_back(it->first);

	for (int i = 0; i < disconnectGuids.size(); i++)
		Disconnect(disconnectGuids[i]);

	Assert(m_Connections.empty());
}


void GeneralConnection::Initialize ()
{
	NetworkInitialize();
}

void GeneralConnection::Cleanup ()
{
	NetworkCleanup();
}


GeneralConnection::Connection* GeneralConnection::GetConnection(UInt32 guid)
{
	ConnectionMap::iterator it = m_Connections.find(guid);
	if (it == m_Connections.end())
		return NULL;
	return it->second;
}

void GeneralConnection::RegisterConnection(UInt32 guid, TSocketHandle socketHandle)
{
	if (GetConnection(guid))
		Disconnect(guid);

	LOG_PLAYER_CONNECTION(Format("PlayerConnection registered: %d", guid));
	m_Connections[guid] = new Connection(socketHandle);
	for(int i = 0; i < m_ConnectionHandlers.size(); i++)
		(m_ConnectionHandlers[i])(guid);
}

void GeneralConnection::Disconnect(UInt32 guid)
{
	ConnectionMap::iterator it = m_Connections.find(guid);
	if (it == m_Connections.end())
		return;

	LOG_PLAYER_CONNECTION(Format("PlayerConnection disconnecting: %d", guid));
	for (int i = 0; i < m_DisconnectionHandlers.size(); i++)
		(m_DisconnectionHandlers[i])(guid);

#if ENABLE_PLAYER_CONNECTION_DEBUG_LOG
	Connection& connection = *it->second;
	LOG_PLAYER_CONNECTION(Format("GeneralConnection::Connection send buffer mem usage %d\n", connection.GetSendBufferSize()));
	LOG_PLAYER_CONNECTION(Format("GeneralConnection::Connection recv buffer mem usage %d\n", connection.GetRecvBufferSize()));
#endif

	delete it->second;

	m_Connections.erase(it);
}


void GeneralConnection::SendMessage(UInt32 guid, MessageID id, const void* data, UInt32 size)
{
	NetworkMessage message;
	message.InitializeMessage();
	message.SetID(id);
	message.SetDataSize(size);

	bool oldlog = IsLogEnabled();
	SetLogEnabled(false);

	if (guid == ANY_PLAYERCONNECTION)
	{
		LOG_PLAYER_CONNECTION(Format("PlayerConnection send message to ALL, id '%d', size '%d'", id, size));
		ConnectionMap::iterator it = m_Connections.begin();
		for ( ; it != m_Connections.end(); ++it)
			it->second->SendMessage(message, data);
	}
	else
	{
		LOG_PLAYER_CONNECTION(Format("PlayerConnection send message to '%d', id '%d', size '%d'", guid, id, size));
		ConnectionMap::iterator it = m_Connections.find(guid);
		if (it != m_Connections.end())
			it->second->SendMessage(message, data);
	}
	SetLogEnabled(oldlog);
}

void GeneralConnection::Poll()
{
	ABSOLUTE_TIME start = START_TIME;
	ConnectionMap::iterator it = m_Connections.begin();
	UNITY_TEMP_VECTOR(int) disconnectGuids;
	for ( ; it != m_Connections.end(); ++it)
	{
		Connection& connection = *(it->second);
		connection.Poll();
		const void*		messageDataPtr;
		NetworkMessage	messageHeader;
		while ( (GetProfileTime(ELAPSED_TIME(start)) < 20*kTimeMillisecond) && ((messageDataPtr = connection.ReceiveMessage(&messageHeader)) != NULL) )
		{
			LOG_PLAYER_CONNECTION(Format("PlayerConnection recv message id '%d', size '%d'", messageHeader.GetID(), messageHeader.GetDataSize()));
			std::map< MessageID, MessageHandlerFunc >::iterator handler = m_HandlerMap.find(messageHeader.GetID());
			if (handler != m_HandlerMap.end())
				(handler->second)(messageDataPtr, messageHeader.GetDataSize(), it->first);
			connection.ReleaseReceivedMessage();
		}
		if (!connection.IsValid())
			disconnectGuids.push_back(it->first);
	}

	for (int i = 0; i < disconnectGuids.size(); i++)
		Disconnect(disconnectGuids[i]);
}

// Handlers
void GeneralConnection::RegisterMessageHandler( MessageID messageID, MessageHandlerFunc func )
{
	if(m_HandlerMap.find(messageID) != m_HandlerMap.end())
		ErrorString("MessageHandler already registered");
	m_HandlerMap[messageID] = func;
}

void GeneralConnection::RegisterConnectionHandler( ConnectionHandlerFunc func )
{
	for (int i = 0; i < m_ConnectionHandlers.size(); i++)
		Assert(m_ConnectionHandlers[i] != func);

	m_ConnectionHandlers.push_back(func);

	// call the connect handler on already connected sockets
	ConnectionMap::iterator it = m_Connections.begin();
	for( ; it != m_Connections.end(); ++it)
		(func)(it->first);
}

void GeneralConnection::RegisterDisconnectionHandler( ConnectionHandlerFunc func )
{
	for (int i = 0; i < m_DisconnectionHandlers.size(); i++)
		Assert(m_DisconnectionHandlers[i] != func);

	m_DisconnectionHandlers.push_back(func);
}

void GeneralConnection::UnregisterMessageHandler( MessageID messageID, MessageHandlerFunc func )
{
	std::map< MessageID, MessageHandlerFunc >::iterator found = m_HandlerMap.find(messageID);
	if(found == m_HandlerMap.end())
		ErrorString("MessageHandler not registered");
	Assert(found->second == func);
	m_HandlerMap.erase(found);
}

void GeneralConnection::UnregisterConnectionHandler( ConnectionHandlerFunc func )
{
	std::vector< ConnectionHandlerFunc >::iterator handlerIt = m_ConnectionHandlers.begin();
	for ( ; handlerIt != m_ConnectionHandlers.end(); ++handlerIt)
	{
		if (*handlerIt == func)
		{
			m_ConnectionHandlers.erase(handlerIt);
			return;
		}
	}
}

void GeneralConnection::UnregisterDisconnectionHandler( ConnectionHandlerFunc func )
{
	// call the disconnect handler on already connected sockets
	ConnectionMap::iterator it = m_Connections.begin();
	for( ; it != m_Connections.end(); ++it)
		(func)(it->first);

	std::vector< ConnectionHandlerFunc >::iterator handlerIt = m_DisconnectionHandlers.begin();
	for ( ; handlerIt != m_DisconnectionHandlers.end(); ++handlerIt)
	{
		if (*handlerIt == func)
		{
			m_DisconnectionHandlers.erase(handlerIt);
			return;
		}
	}
}

// ------------------------------------------------------------------------------
// Connection
// ------------------------------------------------------------------------------
GeneralConnection::Connection::Connection(TSocketHandle socketHandle)
: m_SocketStream(socketHandle, kDataBufferMaxSize, kDataBufferMaxSize)
, m_PendingMessageData(NULL)
, m_HasPendingMessage(false)
{
}

GeneralConnection::Connection::~Connection()
{
	if (m_PendingMessageData)
		ReleaseReceivedMessage();
}

const void* GeneralConnection::Connection::ReceiveMessage(NetworkMessage* message)
{
	AssertBreak(!m_PendingMessageData);
	Mutex::AutoLock lock (m_RecvMutex);

	ExtendedGrowingRingbuffer& recvBuffer = m_SocketStream.RecvBuffer();
	UInt32 bytesAvailable = recvBuffer.GetAvailableSize();
	if (!m_HasPendingMessage && bytesAvailable >= sizeof(NetworkMessage))
	{
		m_SocketStream.RecvAll(&m_PendingMessage, sizeof(NetworkMessage));
		m_HasPendingMessage = true;
		bytesAvailable = recvBuffer.GetAvailableSize();
		Assert(recvBuffer.GetSize() >= m_PendingMessage.GetDataSize() && "Buffer is not large enough for the message.");

	}

	if (m_HasPendingMessage)
	{
		UInt32 messageSize = m_PendingMessage.GetDataSize();
		if (m_HasPendingMessage && bytesAvailable >= messageSize)
		{
			m_HasPendingMessage = false;
			UInt32 linearData = messageSize;
			m_PendingMessageData = (void*) recvBuffer.ReadPtr(&linearData);
			m_LocallyAllocatedMessageData = linearData < messageSize;
			if (m_LocallyAllocatedMessageData)
			{
				m_PendingMessageData = UNITY_MALLOC(kMemNetwork, messageSize);
				m_SocketStream.RecvAll(m_PendingMessageData, messageSize);
			}
			memcpy(message, &m_PendingMessage, sizeof(NetworkMessage));
			return m_PendingMessageData;
		}
	}
	return NULL;
}

void GeneralConnection::Connection::ReleaseReceivedMessage()
{
	AssertBreak(m_PendingMessageData);
	Mutex::AutoLock lock (m_RecvMutex);

	if (m_LocallyAllocatedMessageData)
		UNITY_FREE(kMemNetwork, m_PendingMessageData);
	else
		m_SocketStream.RecvBuffer().ReadPtrUpdate(m_PendingMessageData, m_PendingMessage.GetDataSize());
	m_PendingMessageData = NULL;
}

void GeneralConnection::Connection::SendMessage(NetworkMessage& message, const void* data)
{
	Mutex::AutoLock lock (m_SendMutex);
	if(message.AllowSkip() && !m_SocketStream.CanSendNonblocking(sizeof(NetworkMessage) + message.GetDataSize()))
	{
		WarningString("Skipping profile frame. Reciever can not keep up with the amount of data sent");
		return;
	}
	m_SocketStream.SendAll(&message, sizeof(NetworkMessage));
	m_SocketStream.SendAll(data, message.GetDataSize());
}

// ------------------------------------------------------------------------------
// NetworkMessage
// ------------------------------------------------------------------------------
void GeneralConnection::NetworkMessage::InitializeMessage()
{
	UInt16 magicNumber = PLAYER_MESSAGE_MAGIC_NUMBER;
	SwapEndianBytesLittleToNative(magicNumber);
	m_MagicNumber = magicNumber;
}

bool GeneralConnection::NetworkMessage::CheckMessageValidity()
{
	UInt16 magicNumber = m_MagicNumber;
	SwapEndianBytesLittleToNative(magicNumber);
	return magicNumber == PLAYER_MESSAGE_MAGIC_NUMBER;
}

void GeneralConnection::NetworkMessage::SetID( GeneralConnection::MessageID messageID )
{
	UInt16 id = messageID;
	SwapEndianBytesLittleToNative(id);
	m_ID = id;
}

GeneralConnection::MessageID GeneralConnection::NetworkMessage::GetID()
{
	UInt16 id = m_ID;
	SwapEndianBytesLittleToNative(id);
	return (MessageID) id;
}

void GeneralConnection::NetworkMessage::SetDataSize( UInt32 size )
{
	SwapEndianBytesLittleToNative(size);
	m_Size = size;
}

UInt32 GeneralConnection::NetworkMessage::GetDataSize()
{
	UInt32 size = m_Size;
	SwapEndianBytesLittleToNative(size);
	return size;
}

// ------------------------------------------------------------------------------
// MulticastInfo
// ------------------------------------------------------------------------------
#if ENABLE_MULTICAST
GeneralConnection::MulticastInfo::MulticastInfo()
{
	Clear();
}

void GeneralConnection::MulticastInfo::Clear()
{
	m_Valid = false;
	m_Flags = 0;
	m_Port = 0;
	m_Guid = 0;
	m_EditorGuid = 0;
}
bool GeneralConnection::MulticastInfo::Parse( const char* buffer, void* in)
{
	char ip[kMulticastBufferSize];
	char id[kMulticastBufferSize];
	int version;
	int debug;
	if(sscanf(buffer, SERVER_IDENTIFICATION_FORMAT, ip, &m_Port, &m_Flags, &m_Guid, &m_EditorGuid, &version, id, &debug) != 8)
	{
		Clear();
		m_Valid = false;
		m_IsLocalhost = false;
		return false;
		//ErrorString(Format("MulticastInfo should be in this format: '%s' but got: '%s'", SERVER_IDENTIFICATION_FORMAT, buffer));
	}
#if !UNITY_WINRT
	m_Ip = in ? InAddrToIP((sockaddr_in*)in) : std::string(ip);
#endif

	m_Identifier = std::string(id);
	m_Valid = (version == ms_Version);
	SetLocalhostFlag();
	return true;
}

void GeneralConnection::MulticastInfo::SetLocalhostFlag()
{
	std::string localIP;
	char ips[10][16];
	int count = GetIPs(ips);
	if (count != 0)
		localIP = ips[0];
	else
		localIP = "0.0.0.0";
	m_IsLocalhost = (m_Ip.compare("0.0.0.0") == 0) || (m_Ip.compare("127.0.0.1") == 0) || (m_Ip.compare(localIP) == 0);
}

#endif //ENABLE_MULTICAST


/// These tests are unstable and run for too long?
// ---------------------------------------------------------------------------
#if ENABLE_UNIT_TESTS && 0

#include "External/UnitTest++/src/UnitTest++.h"
#include "EditorConnection.h"
#include "PlayerConnection.h"

SUITE (GeneralConnectionTests)
{
	bool isClientConnected = false;
	UInt32 clientConnectionGuid = -1;
	void ClientConnectionHandler (UInt32 guid) { isClientConnected = true; clientConnectionGuid = guid; }
	void ClientDisconnectionHandler (UInt32 guid) { isClientConnected = false; clientConnectionGuid = -1;}

	bool isServerConnected = false;
	UInt32 serverConnectionGuid = -1;
	void ServerConnectionHandler (UInt32 guid) { isServerConnected = true; serverConnectionGuid = guid;}
	void ServerDisconnectionHandler (UInt32 guid) { isServerConnected = false; serverConnectionGuid = -1;}

	struct GeneralConnectionFixture
	{
		GeneralConnectionFixture ()
		{
			Initialize();
		}
		~GeneralConnectionFixture ()
		{
			Destroy();
		}

		void Initialize ()
		{
			GeneralConnection::RunningUnitTest();
			Rand r (GetProfileTime(START_TIME));
			int multicastport = PLAYER_UNITTEST_MULTICAST_PORT + (r.Get() & PLAYER_PORT_MASK);
			player = new PlayerConnection("", multicastport);
			editor = new EditorConnection(multicastport);
			editor->RegisterConnectionHandler( ClientConnectionHandler );
			player->RegisterConnectionHandler( ServerConnectionHandler );
			editor->RegisterDisconnectionHandler( ClientDisconnectionHandler );
			player->RegisterDisconnectionHandler( ServerDisconnectionHandler );
		}

		void Destroy ()
		{
			delete player;
			delete editor;
			player = NULL;
			editor = NULL;
		}

		void Connect ()
		{
			//record time
			ABSOLUTE_TIME start = START_TIME;
			while (GetProfileTime(ELAPSED_TIME(start)) < 1*kTimeSecond)
			{
				player->Poll();
				editor->PollWithCustomMessage();
				if (player->IsConnected() && editor->IsConnected()) break;
			}
		}

		void CheckConnectionState(bool state)
		{
			// is internal connection state
			CHECK_EQUAL (state, player->IsConnected() );
			CHECK_EQUAL (state, editor->IsConnected() );
			// is custom connect handler called
			CHECK_EQUAL (state, isClientConnected);
			CHECK_EQUAL (state, isServerConnected);
		}

		PlayerConnection* player;
		EditorConnection* editor;
	};

	std::string message;
	void HandleGeneralConnectionMessage (const void* data, UInt32 size, UInt32 guid)
	{
		message = std::string((char*)data);
	}

	int* largeMessage;
	void HandleLargeMessage(const void* data, UInt32 size, UInt32 guid)
	{
		largeMessage = new int[size/sizeof(int)];
		memcpy(largeMessage, data, size);
	}

	int messageCount = 0;
	void HandleManyMessages(const void* data, UInt32 size, UInt32 guid)
	{
		CHECK_EQUAL(((char*)data)[10],messageCount);
		messageCount++;
	}

	TEST_FIXTURE(GeneralConnectionFixture, CanDisconnectServer)
	{
		Connect();
		CheckConnectionState(true);

		player->DisconnectAll();

		ABSOLUTE_TIME start = START_TIME;
		while (GetProfileTime(ELAPSED_TIME(start)) < 1*kTimeSecond)
		{
			editor->PollWithCustomMessage();
			if (!editor->IsConnected()) break;
		}
		CheckConnectionState(false);
	}

	TEST_FIXTURE(GeneralConnectionFixture, CanConnect)
	{
		Connect();
		CheckConnectionState(true);
	}

	TEST_FIXTURE(GeneralConnectionFixture, CanDisconnectClient)
	{
		Connect();
		CheckConnectionState(true);

		editor->DisconnectAll();

		ABSOLUTE_TIME start = START_TIME;
		while (GetProfileTime(ELAPSED_TIME(start)) < 1*kTimeSecond)
		{
			player->Poll();
			if (!player->IsConnected()) break;
		}
		CheckConnectionState(false);
	}

	TEST_FIXTURE(GeneralConnectionFixture, CanSendMessage)
	{
		Connect();
		editor->RegisterMessageHandler(GeneralConnection::kLastMessageID,HandleGeneralConnectionMessage);
		player->RegisterMessageHandler(GeneralConnection::kLastMessageID,HandleGeneralConnectionMessage);

		player->SendMessage(serverConnectionGuid, GeneralConnection::kLastMessageID, "Hello World", 12);
		ABSOLUTE_TIME start = START_TIME;
		while (GetProfileTime(ELAPSED_TIME(start)) < 1*kTimeSecond)
		{
			player->Poll();
			editor->PollWithCustomMessage();
			if (!message.empty()) break;
		}
		CHECK_EQUAL (std::string("Hello World"), message);

		message = std::string("");
		editor->SendMessage(clientConnectionGuid, GeneralConnection::kLastMessageID, "Are you there", 14);
		start = START_TIME;
		while (GetProfileTime(ELAPSED_TIME(start)) < 1*kTimeSecond)
		{
			player->Poll();
			editor->PollWithCustomMessage();
			if (!message.empty()) break;
		}
		CHECK_EQUAL (std::string("Are you there"), message);

	}

	TEST_FIXTURE(GeneralConnectionFixture, SendLargeMessage)
	{
		Connect();
		editor->RegisterMessageHandler(GeneralConnection::kLastMessageID,HandleLargeMessage);

		int msgSize = 400*1024;
		int* buffer = new int[msgSize];
		for(int i = 0; i < msgSize; i++)
			buffer[i] = i;
		largeMessage = NULL;
		player->SendMessage(serverConnectionGuid, GeneralConnection::kLastMessageID, (char*)buffer, msgSize*sizeof(int));
		ABSOLUTE_TIME start = START_TIME;
		while (GetProfileTime(ELAPSED_TIME(start)) < 2*kTimeSecond)
		{
			player->Poll();
			editor->PollWithCustomMessage();
			if (largeMessage != NULL) break;
		}
		if (largeMessage != NULL)
		{
			CHECK_EQUAL (1, largeMessage[1]);
			CHECK_EQUAL (1024, largeMessage[1024]);
			CHECK_EQUAL (10000, largeMessage[10000]);
			CHECK_EQUAL (msgSize -1, largeMessage[msgSize-1]);
		}
		else
			CHECK(largeMessage != NULL);

		delete[] largeMessage; largeMessage = NULL;
	}

	TEST_FIXTURE(GeneralConnectionFixture, SendMultipleMessage)
	{
		Connect();
		editor->RegisterMessageHandler(GeneralConnection::kLastMessageID,HandleManyMessages);

		char message[20];

		for(int i = 0; i < 100; i++)
		{
			message[10] = (char)i;
			player->SendMessage(serverConnectionGuid, GeneralConnection::kLastMessageID, message, 20);
		}

		ABSOLUTE_TIME start = START_TIME;
		while (GetProfileTime(ELAPSED_TIME(start)) < 1*kTimeSecond)
		{
			player->Poll();
			editor->PollWithCustomMessage();
			if (messageCount == 100) break;
		}
		CHECK_EQUAL (100, messageCount);
	}
}

#endif

#endif // ENABLE_PLAYERCONNECTION
