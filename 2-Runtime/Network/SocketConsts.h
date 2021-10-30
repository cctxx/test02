#ifndef SOCKETCONSTS_H
#define SOCKETCONSTS_H

#if UNITY_WINRT

	#include <winsock2.h>

#if UNITY_METRO
	// WP8's winsock2.h is newer with full socket support.
	// TEMP use local defines for Metro, until Microsoft updates Metro API.
	#define AF_INET 2
	#define SOCK_STREAM 1
	#define IPPROTO_TCP 6
	#define IPPROTO_UDP 17
	#define SOCK_DGRAM 2
	#define SOL_SOCKET 0xffff
	#define SO_BROADCAST 0x0020
	#define IPPROTO_IP 0
	#define IP_MULTICAST_TTL 10
	typedef USHORT ADDRESS_FAMILY;
	typedef struct sockaddr {
		ADDRESS_FAMILY sa_family;
		CHAR sa_data[14];
	} SOCKADDR, *PSOCKADDR, FAR *LPSOCKADDR;
	typedef struct sockaddr_in {
		ADDRESS_FAMILY sin_family;
		USHORT sin_port;
		char sin_addr[32];
		CHAR sin_zero[8];
	} SOCKADDR_IN, *PSOCKADDR_IN;

#endif

	struct SocketWrapper;
	typedef SocketWrapper* TSocketHandle;
	typedef int socklen_t;
	
//	typedef void SendUserData;
//	typedef void RecvUserData;

#else

	#if UNITY_WIN
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#elif UNITY_XENON
	#include <xtl.h>
	typedef int socklen_t;
	#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/select.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <errno.h>
	#include <netdb.h>
	#endif
	#if UNITY_PS3
	#include <sys/time.h>
	#include <netex/net.h>
	#include <netex/errno.h>
	#endif


	typedef int TSocketHandle;
#endif

struct SendUserData
{
	int flags;
	struct sockaddr* dstAddr;
	socklen_t dstLen;
	SendUserData() : flags(0) {}
};
struct RecvUserData
{
	int flags;
	struct sockaddr* srcAddr;
	socklen_t srcLen;
	RecvUserData() : flags(0) {}
};

enum { kDefaultBufferSize = 16*1024 };
enum { kDefaultPollTime = 1 };

#define USE_WINSOCK_APIS ((UNITY_WIN && !UNITY_WINRT) || UNITY_XENON)

#endif
