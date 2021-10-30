#pragma once

#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Network/SocketConsts.h"
#if UNITY_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#elif UNITY_XENON
#include <xtl.h>
typedef int socklen_t;
#else 
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#endif
#if UNITY_PS3
#include <sys/time.h>
#include <netex/net.h>
#include <netex/errno.h>
#define SOCK_ERROR(s) ((s) < 0)
#elif UNITY_WINRT
#define SOCK_ERROR(s) ((s) == nullptr)
#else
#define SOCK_ERROR(s) ((s) == -1)
#endif


static const ProfileTimeFormat kTimeMillisecond = 1000000ULL;
static const ProfileTimeFormat kTimeSecond = 1000000000ULL;
static const ProfileTimeFormat kPlayerConnectionInitialWaitTimeout = 5*kTimeSecond;

#if UNITY_FLASH
extern "C" void Ext_GetSocketPolicyFile(const char* ipString);
extern int g_AlchemySocketErrno;
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

static UInt32 NextGUID()
{
	static volatile int guid_counter = 0;
	return (UInt32) AtomicIncrement(&guid_counter);
}
