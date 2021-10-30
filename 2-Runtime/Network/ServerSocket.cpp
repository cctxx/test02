#include "UnityPrefix.h"

#if ENABLE_SOCKETS
#include "ServerSocket.h"
#include "SocketUtils.h"

ServerSocket::ServerSocket(int domain, int type, int protocol)
: Socket(domain, type, protocol)
, m_IsListening(false)
{
	SetReuseAddress(true);
}

#if !UNITY_WINRT

// Metro does not support BSD sockets, so these methods are reimplemented
// in Metro platform specific codebase

bool ServerSocket::StartListening(unsigned short port, bool block)
{
	struct sockaddr_in addr;
	SetupAddress(htonl(INADDR_ANY), htons(port), &addr);
	return StartListening((const sockaddr*) &addr, sizeof(addr), block);
}

bool ServerSocket::StartListening(const char* ip, unsigned short port, bool block)
{
	struct sockaddr_in addr;
	SetupAddress(inet_addr(ip), htons(port), &addr);
	return StartListening((const sockaddr*) &addr, sizeof(addr), block);
}

bool ServerSocket::StartListening(const sockaddr* addr, socklen_t addr_len, bool block)
{
	if (!m_IsListening)
	{
		if (!SetBlocking(block))
			return false;

		if (CheckError(bind(m_SocketHandle, addr, addr_len), "bind failed"))
			return false;

		if (CheckError(listen(m_SocketHandle, 5), "listen failed"))
			return false;

		m_IsListening = true;
		return true;
	}
	ErrorStringMsg("already listening");
	return false;
}

int ServerSocket::GetPort()
{
	sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	if (CheckError(getsockname(m_SocketHandle, (struct sockaddr *) &addr, &addr_len)))
		return -1;
	return ntohs(addr.sin_port);
}

TSocketHandle ServerSocket::Accept()
{
	return Accept(NULL, NULL);
}

TSocketHandle ServerSocket::Accept(sockaddr* addr, socklen_t* addr_len)
{
	int socketHandle = accept(m_SocketHandle, addr, addr_len);
	if (CheckError(socketHandle, "accept failed", kPlatformAcceptWouldBlock))
		return socketHandle; // Shutdown?
	return socketHandle;
}


#undef Error
#undef SocketError

// ---------------------------------------------------------------------------
#if ENABLE_UNIT_TESTS && !UNITY_XENON

#include "External/UnitTest++/src/UnitTest++.h"
#include "NetworkUtility.h"
SUITE (ServerSocketTests)
{
	struct SocketFixture
	{
		SocketFixture()
		{
			NetworkInitialize();
		};

		~SocketFixture()
		{
			NetworkCleanup();
		}
	};

	TEST_FIXTURE(SocketFixture, ServerSocket_Connect)
	{
		int socketHandle, port;

		ServerSocket socket;
		CHECK((socket.StartListening("127.0.0.1", 0, false)) == true);
		CHECK((port = socket.GetPort()) > 0);
		CHECK((socketHandle = Socket::Connect("127.0.0.1", port)) >= 0);

		Socket::Close(socketHandle);
		CHECK(socket.IsListening());
	}
}

#endif //ENABLE_UNIT_TESTS

#endif // UNITY_WINRT

#endif // ENABLE_SOCKETS
