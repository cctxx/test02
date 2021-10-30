#ifndef SERVERSOCKET_H
#define SERVERSOCKET_H

#if ENABLE_SOCKETS
#include "Sockets.h"
#include "SocketConsts.h"

class ServerSocket : protected Socket
{
public:
	ServerSocket(int domain = AF_INET, int type = SOCK_STREAM, int protocol = IPPROTO_TCP);

	bool StartListening(unsigned short port, bool block);
	bool StartListening(const char* ip, unsigned short port, bool block);
#if !UNITY_WINRT
	bool StartListening(const sockaddr* addr, socklen_t addr_len, bool block);
#endif

	int GetPort();
	bool IsListening() const { return m_IsListening; }

	TSocketHandle Accept();
#if !UNITY_WINRT
	TSocketHandle Accept(sockaddr* addr, socklen_t* addr_len);
#endif
private:
	bool m_IsListening;
};

#endif // ENABLE_SOCKETS
#endif // SERVERSOCKET_H
