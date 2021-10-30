#include "UnityPrefix.h"

#if ENABLE_SOCKETS //|| UNITY_WINRT

#include "SocketStreams.h"
#include "ServerSocket.h"
#include "Runtime/Profiler/TimeHelper.h"

static const ProfileTimeFormat kTimeMillisecond = 1000000ULL;

// ---------------------------------------------------------------
// SocketStream
// ---------------------------------------------------------------
SocketStream::SocketStream(TSocketHandle socketHandle, bool block)
: Socket(socketHandle)
, m_IsConnected(true)
, m_IsBlocking(block)
{
	if (!SetBlocking(block))
	{
		ErrorStringMsg("Unable to set blocking mode for socket stream, shutting down socket!");
		Shutdown(); // Shutdown if unable to switch mode
	}
}

int SocketStream::Send(const void* data, UInt32 data_len)
{
	if (data_len == 0)
		return data_len;

	int result = Socket::Send(data, data_len);
	if (result < 0 && !Socket::WouldBlockError())
		OnSocketError();

	return result;
}

int SocketStream::Recv(void* data, UInt32 data_len)
{
	if (data_len == 0)
		return data_len;

	int result = Socket::Recv(data, data_len);
	if (result == 0 || (result < 0 && !Socket::WouldBlockError()))
		OnSocketError();

	return result;
}

bool SocketStream::SendAll(const void* data, UInt32 data_len)
{
	while (data_len > 0)
	{
		int nBytes = Send(data, data_len);
		if (nBytes <= 0 && WouldBlockError())
		{
			if(!Poll())
				return false;
			continue;
		}
		if (nBytes < 0)
			return false;

		data_len -= nBytes;
		data = (char*)data + nBytes;
	}
	return true;
}

bool SocketStream::RecvAll(void* data, UInt32 data_len)
{
	while (data_len > 0)
	{
		int nBytes = Recv(data, data_len);
		if (nBytes < 0 && WouldBlockError())
		{
			if(!Poll())
				return false;
			continue;
		}
		if (nBytes <= 0)
			return false;

		data_len -= nBytes;
		data = (char*)data + nBytes;
	}
	return true;
}

bool SocketStream::Shutdown()
{
	if (!m_IsConnected)
		return true;

#if USE_WINSOCK_APIS
	if (CheckError(shutdown(m_SocketHandle, SD_BOTH), "failed to shutdown stream", WSAENOTCONN))
#elif UNITY_WINRT
	if (CheckError(false, "failed to shutdown stream", ENOTCONN))
#else
	if (CheckError(shutdown(m_SocketHandle, SHUT_RDWR), "failed to shutdown stream", ENOTCONN))
#endif
	{
		m_IsConnected = false; // always tag as disconnected to avoid loops
		return false;
	}
	m_IsConnected = false;
	return true;
}

void SocketStream::OnSocketError()
{
	Shutdown();
}

bool SocketStream::CanSendNonblocking( UInt32 data_len )
{
	return true;
}


// ---------------------------------------------------------------
// BufferedSocketStream
// ---------------------------------------------------------------
BufferedSocketStream::BufferedSocketStream(TSocketHandle socketHandle, UInt32 sendbufferMaxSize, UInt32 recvbufferMaxSize)
#if UNITY_FLASH
 // This is like the worst hack ever, flash can't read data so it never blocks on recv
: SocketStream(socketHandle, true)
#else
: SocketStream(socketHandle, false)
#endif
, m_IsArtificiallyConnected(false)
, m_Sendbuffer(kMemNetwork, sendbufferMaxSize)
, m_Recvbuffer(kMemNetwork, recvbufferMaxSize)
{}

BufferedSocketStream::BufferedSocketStream(TSocketHandle socketHandle, UInt32 sendbufferMaxSize, UInt32 recvbufferMaxSize, bool block)
: SocketStream(socketHandle, block)
, m_IsArtificiallyConnected(false)
, m_Sendbuffer(kMemNetwork, sendbufferMaxSize)
, m_Recvbuffer(kMemNetwork, recvbufferMaxSize)
{}

bool BufferedSocketStream::FillRecvbuffer()
{
	UInt32 recvBufferFree = m_Recvbuffer.GetFreeSize();
	if (!recvBufferFree)
	{
		if (!SocketStream::IsBlocking())
			return false;
		m_Recvbuffer.BlockUntilFree();
	}

	void* recvBuffer = m_Recvbuffer.WritePtr(&recvBufferFree);
	int nRecvBytes = SocketStream::Recv(recvBuffer, recvBufferFree);
	if (nRecvBytes <= 0)
		return false;

	m_Recvbuffer.WritePtrUpdate(recvBuffer, nRecvBytes);
	return true;
}

bool BufferedSocketStream::FlushSendbuffer()
{
	UInt32 sendBufferAvail = m_Sendbuffer.GetAvailableSize();
	if (!sendBufferAvail)
	{
		if (!SocketStream::IsBlocking())
			return false;
		m_Sendbuffer.BlockUntilAvailable();
	}

	const void* sendBuffer = m_Sendbuffer.ReadPtr(&sendBufferAvail);
	int nSentBytes = SocketStream::Send(sendBuffer, sendBufferAvail);
	if (nSentBytes < 0)
		return false;

	m_Sendbuffer.ReadPtrUpdate(sendBuffer, nSentBytes);
	return true;
}

bool BufferedSocketStream::Poll(UInt64 timeoutMS)
{
	if (!m_IsConnected)
		return false;

	Mutex::AutoLock lock(m_PollMutex);

	ABSOLUTE_TIME start = START_TIME;

	bool notBlocked = true;
	while (notBlocked && GetProfileTime(ELAPSED_TIME(start)) < timeoutMS * kTimeMillisecond)
	{
		notBlocked = FlushSendbuffer();
		notBlocked = FillRecvbuffer() || notBlocked;
		notBlocked = m_IsConnected && notBlocked;
	}

	return m_IsConnected;
}

int BufferedSocketStream::Send(const void* data, UInt32 data_len)
{
	if (!m_IsConnected)
		return -1;

	void* sendBuffer = m_Sendbuffer.WritePtr(&data_len);
	memcpy(sendBuffer, data, data_len);
	m_Sendbuffer.WritePtrUpdate(sendBuffer, data_len);

	return data_len;
}

int BufferedSocketStream::Recv(void* data, UInt32 data_len)
{
	if (!m_IsConnected && !m_IsArtificiallyConnected)
		return 0;

	const void* recvBuffer = m_Recvbuffer.ReadPtr(&data_len);
	memcpy(data, recvBuffer, data_len);
	m_Recvbuffer.ReadPtrUpdate(recvBuffer, data_len);

	if (data_len == 0)
	{
		if (m_IsArtificiallyConnected)
			Shutdown();
		else
			return -1;
	}

	return data_len;
}

void BufferedSocketStream::OnSocketError()
{
	m_IsArtificiallyConnected = 0 < m_Recvbuffer.GetAvailableSize();
	SocketStream::Shutdown();
}

bool BufferedSocketStream::Shutdown()
{
	bool result = SocketStream::Shutdown();
	m_IsArtificiallyConnected = false;
	m_Sendbuffer.ReleaseBlockedThreads();
	m_Recvbuffer.ReleaseBlockedThreads();
	return result;
}

bool BufferedSocketStream::CanSendNonblocking( UInt32 data_len )
{
	UInt32 sendBufferFree = m_Sendbuffer.GetFreeSize();
	return sendBufferFree >= data_len;
}

// ---------------------------------------------------------------
// ThreadedSocketStream
// ---------------------------------------------------------------
#if SUPPORT_THREADS
ThreadedSocketStream::ThreadedSocketStream(TSocketHandle socketHandle, UInt32 sendbufferSize, UInt32 recvbufferSize)
: BufferedSocketStream(socketHandle, sendbufferSize, recvbufferSize, false)
{
	m_Reader.SetName("UnitySocketReader");
	m_Writer.SetName("UnitySocketWriter");
	m_Reader.Run(ReaderLoop, this);
	m_Writer.Run(WriterLoop, this);
}

ThreadedSocketStream::~ThreadedSocketStream()
{
	Shutdown();
	m_Reader.WaitForExit();
	m_Writer.WaitForExit();
}

void* ThreadedSocketStream::ReaderLoop(void* _arg)
{
	ThreadedSocketStream* _this = reinterpret_cast<ThreadedSocketStream*>(_arg);


	while(_this->m_IsConnected)
	{
		if(_this->WaitForAvailableRecvData(10))
			_this->FillRecvbuffer();
	}
	return NULL;
}

void* ThreadedSocketStream::WriterLoop(void* _arg)
{
	ThreadedSocketStream* _this = reinterpret_cast<ThreadedSocketStream*>(_arg);
	while(_this->m_IsConnected)
	{
		_this->SendBuffer().BlockUntilAvailable();
		if(_this->WaitForAvailableSendBuffer(10))
			_this->FlushSendbuffer();
	}
	return NULL;
}
#endif // SUPPORT_THREADS

// ---------------------------------------------------------------------------
#if ENABLE_UNIT_TESTS && !UNITY_XENON

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Network/NetworkUtility.h"
SUITE (SocketStreamTests)
{
	struct SocketStreamFixture
	{
		SocketStreamFixture()
		{
			NetworkInitialize();
			m_Socket = new ServerSocket();
			CHECK((m_Socket->StartListening("127.0.0.1", 0, true)) == true);
			CHECK((m_Port = m_Socket->GetPort()) > 0);
		};

		~SocketStreamFixture()
		{
			delete m_Socket;
			NetworkCleanup();
		}

		int Accept()
		{
			return m_Socket->Accept();
		};

		int Connect()
		{
			return Socket::Connect("127.0.0.1", m_Port);
		};

		int				m_Port;
		ServerSocket*	m_Socket;
	};

	void TestNonBlockingSendAndRecv(SocketStream& server, SocketStream& client)
	{
		char buffer[4096];
		int nBytesToSend = sizeof(buffer);
		int nBytesToRecv = sizeof(buffer);
		while (nBytesToRecv)
		{
			int nBytesSent = client.Send(buffer, nBytesToSend);
			if (nBytesSent > 0)
				nBytesToSend -= nBytesSent;
			AssertBreak(nBytesSent >= 0 || client.WouldBlockError());

			int nBytesRecv = server.Recv(buffer, nBytesToRecv);
			if (nBytesRecv > 0)
				nBytesToRecv -= nBytesRecv;
			AssertBreak(nBytesRecv > 0 || (nBytesRecv < 0 && server.WouldBlockError()));
		}
		CHECK_EQUAL(nBytesToSend, nBytesToRecv);
	}

	TEST_FIXTURE(SocketStreamFixture, SocketStreamNB_SendRecv)
	{
		SocketStream client(Connect(), false);
		SocketStream server(Accept(), false);
		TestNonBlockingSendAndRecv(server, client);
	}

#if SUPPORT_THREADS
	static void* PollBufferedStream(void* arg)
	{
		BufferedSocketStream* stream = reinterpret_cast<BufferedSocketStream*>(arg);
		while (stream->Poll(1000)) { /* spin */ }
		return NULL;
	}

	TEST_FIXTURE(SocketStreamFixture, BufferedSocketStreamNB_SendRecvNonBlocking)
	{
		BufferedSocketStream client(Connect(), 1024, 1024);
		BufferedSocketStream server(Accept(), 1024, 1024);

		Thread clientPoller, serverPoller;
		clientPoller.Run(PollBufferedStream, &client);
		serverPoller.Run(PollBufferedStream, &server);

		TestNonBlockingSendAndRecv(server, client);

		char buffer[4096];
		CHECK(client.SendAll(buffer, sizeof(buffer)));
		CHECK(server.RecvAll(buffer, sizeof(buffer)));

		server.Shutdown();
		client.Shutdown();
		serverPoller.WaitForExit();
		clientPoller.WaitForExit();
	}

	TEST_FIXTURE(SocketStreamFixture, ThreadedSocketStreamNB_SendRecvNonBlocking)
	{
		ThreadedSocketStream client(Connect(), 1024, 1024);
		ThreadedSocketStream server(Accept(), 1024, 1024);
		TestNonBlockingSendAndRecv(server, client);

		char buffer[4096];
		CHECK(client.SendAll(buffer, sizeof(buffer)));
		CHECK(server.RecvAll(buffer, sizeof(buffer)));

		server.Shutdown();
		client.Shutdown();
	}
#endif // SUPPORT_THREADS
}

#endif //ENABLE_UNIT_TESTS

#endif // ENABLE_SOCKETS
