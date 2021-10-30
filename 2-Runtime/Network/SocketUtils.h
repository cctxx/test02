#ifndef SOCKETUTILS_H
#define SOCKETUTILS_H

#include "Sockets.h"
#if !UNITY_WINRT
inline void SetupAddress(unsigned long addr, unsigned short port, sockaddr_in* sock_addr)
{
	memset(sock_addr, 0, sizeof(sockaddr_in));
	sock_addr->sin_family = AF_INET;
	sock_addr->sin_addr.s_addr = addr;
	sock_addr->sin_port = port;
}
#endif
enum
{
#if UNITY_WINRT
	kPlatformConnectWouldBlock	= E_PENDING,
	kPlatformStreamWouldBlock	= E_PENDING,
	kPlatformAcceptWouldBlock	= E_PENDING,
#elif USE_WINSOCK_APIS
	kPlatformConnectWouldBlock	= WSAEWOULDBLOCK,
	kPlatformStreamWouldBlock	= WSAEWOULDBLOCK,
	kPlatformAcceptWouldBlock	= WSAEWOULDBLOCK
#elif UNITY_PS3
	kPlatformConnectWouldBlock	= SYS_NET_EINPROGRESS,
	kPlatformStreamWouldBlock	= SYS_NET_EAGAIN,
	kPlatformAcceptWouldBlock	= SYS_NET_EWOULDBLOCK
#else
	kPlatformConnectWouldBlock	= EINPROGRESS,
	kPlatformStreamWouldBlock	= EAGAIN,
	kPlatformAcceptWouldBlock	= EWOULDBLOCK
#endif
};

#if UNITY_FLASH
#include "Runtime/Scripting/ScriptingUtility.h"

extern "C" int flash_errno();
extern "C" int flash_set_errno(int error);
extern "C" int flash_socket(int domain, int type, int protocol);
extern "C" int flash_connect(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);
extern "C" int flash_close(int sockfd);
extern "C" ssize_t flash_recvfrom (int sockfd, void* buffer, size_t length, int flags, struct sockaddr* address, socklen_t* address_len);
extern "C" ssize_t flash_sendto(int, const void *, size_t, int, const struct sockaddr*, socklen_t);
extern "C" int flash_setsockopt(int sockfd, int level, int option_name, const void* option_value, socklen_t option_len);

#define setsockopt flash_setsockopt
#define recvfrom flash_recvfrom
#define sendto flash_sendto
#define socket flash_socket
#define connect flash_connect
#define close flash_close
#endif


#endif
