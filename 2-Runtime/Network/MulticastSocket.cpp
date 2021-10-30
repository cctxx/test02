#include "UnityPrefix.h"

#if ENABLE_SOCKETS
#include "MulticastSocket.h"
#include "SocketUtils.h"
#if UNITY_EDITOR && UNITY_OSX
#include <ifaddrs.h>
#include <net/if.h>
#endif

MulticastSocket::MulticastSocket()
: Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
, m_Bound(false)
{
	SetReuseAddress(true);
#if UNITY_WINRT
	m_initalised = false;
#endif
}

#if !UNITY_WINRT

bool MulticastSocket::Initialize(const char* group, unsigned short port, bool block)
{
	if (!SetBlocking(block))
		return false;

	SetupAddress(inet_addr(group), htons(port), &m_MulticastAddress);
	return true;
}

bool MulticastSocket::Join()
{
	if (!m_Bound)
	{
		struct sockaddr_in listen_addr;
		SetupAddress(htonl(INADDR_ANY), m_MulticastAddress.sin_port, &listen_addr);
		if (CheckError(bind(m_SocketHandle, (struct sockaddr*) &listen_addr, sizeof(sockaddr_in)), "bind failed"))
			return false;
		m_Bound = true;
	}

#if UNITY_EDITOR
	// Join the multicast group on all valid network addresses, so that a lower value routing metric adapter doesn't
	// override a higher value routing metric adapter from processing the message (e.g. Ethernet and Wifi); This is
	// primarily needed to fix the Profiler being able to auto-discover available players

	// NOTE: Simply because we are able to join a multicast group doesn't mean that the port isn't blocked by a firewall.
	// A common issue among Windows machines is when a network is marked as "Public" and when Unity first ran the user
	// disallowed traffic on Public networks to come through the firewall.
	return SetOptionForAllAddresses(IP_ADD_MEMBERSHIP, "unable to join multicast group");
#else
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = m_MulticastAddress.sin_addr.s_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	return !CheckError(SetSocketOption(IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)), "unable to join multicast group");
#endif
}

bool MulticastSocket::Disband()
{
#if UNITY_EDITOR
	return SetOptionForAllAddresses(IP_DROP_MEMBERSHIP, "unable to disband multicast group");
#else
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = m_MulticastAddress.sin_addr.s_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	return !CheckError(SetSocketOption(IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)), "unable to disband multicast group");
#endif
}

#if UNITY_EDITOR
bool MulticastSocket::SetOptionForAllAddresses(int option, const char* msg)
{
	bool error = false;
	// Windows doesn't have the getifaddrs call and OSX doesn't return all addresses with an empty
	// string passed to getaddrinfo, so it is necessary to have two approaches.
#if UNITY_WIN
#define IFADDR_T			ADDRINFOA
#define PIFADDR_T			PADDRINFOA
#define GET_PSOCKADDR(i)	i->ai_addr
#define GET_NEXT_IFADDR(i)	i->ai_next
#define FREE_IFADDRS(i)		freeaddrinfo(i);

	// Set the filter to return network addresses that can handle multicast (udp)
	IFADDR_T hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	PIFADDR_T ifaddr = NULL;
	// Empty string is sent to return all addresses	registered to the local computer
	getaddrinfo("", NULL, &hints, &ifaddr);
#else
#define IFADDR_T			ifaddrs
#define PIFADDR_T			ifaddrs*
#define GET_PSOCKADDR(i)	i->ifa_addr
#define GET_NEXT_IFADDR(i)	i->ifa_next
#define FREE_IFADDRS(i)		freeifaddrs(i);

	PIFADDR_T ifaddr = NULL;
	if (getifaddrs(&ifaddr) != -1)
#endif
	{
		PIFADDR_T ifa = ifaddr;
		while (ifa)
		{
#if !UNITY_WIN
			// We only care about IPv4 interfaces that support multicast
			if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && (ifa->ifa_flags & IFF_MULTICAST) != 0)
#endif
			{
				sockaddr_in* addr = (sockaddr_in*)GET_PSOCKADDR(ifa);
				//char* address = inet_ntoa(addr->sin_addr);

				struct ip_mreq mreq;
				mreq.imr_multiaddr= m_MulticastAddress.sin_addr;
				mreq.imr_interface = addr->sin_addr;
			
				error |= CheckError(SetSocketOption(IPPROTO_IP, option, &mreq, sizeof(mreq)), msg);
			}

			ifa = GET_NEXT_IFADDR(ifa);
		}
	}
	FREE_IFADDRS(ifaddr);
	
#undef IFADDR_T
#undef PIFADDR_T
#undef GET_PSOCKADDR
#undef GET_NEXT_IFADDR
#undef FREE_IFADDRS

	return !error;
}
#endif

bool MulticastSocket::SetTTL(unsigned char ttl)
{
#if UNITY_XENON
	return false;
#else
	return !CheckError(SetSocketOption(IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)), "failed to set TTL");
#endif
}

bool MulticastSocket::SetLoop(bool loop)
{
#if UNITY_XENON
	return false;
#else
	return !CheckError(SetSocketOption(IPPROTO_IP, IP_MULTICAST_LOOP, loop), "failed to set loop mode");
#endif
}

bool MulticastSocket::SetBroadcast(bool broadcast)
{
	return !CheckError(SetSocketOption(SOL_SOCKET, SO_BROADCAST, broadcast), "failed to set broadcast mode");
}

int MulticastSocket::Send(const void* data, size_t data_len)
{
	SendUserData userData;
	userData.dstAddr = (sockaddr*)&m_MulticastAddress;
	userData.dstLen = sizeof(m_MulticastAddress);
	return Socket::Send(data, data_len, &userData);
}

int MulticastSocket::Recv(void* data, size_t data_len, RecvUserData* userData)
{
	int result = Socket::Recv(data, data_len, userData);
	return result;
}

#undef Error
#undef SocketError

// ---------------------------------------------------------------------------
#if ENABLE_UNIT_TESTS && !UNITY_XENON

#include "External/UnitTest++/src/UnitTest++.h"
#include "NetworkUtility.h"
SUITE (MulticastSocketTests)
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
#if !UNITY_XENON
	TEST_FIXTURE(SocketFixture, Multicast)
	{
		char actual[10], expected[] = "foobar";

		MulticastSocket sender;
		CHECK(sender.Initialize("225.0.0.224", 54996));
		CHECK(sender.SetTTL(0));
		CHECK(sender.SetLoop(true));

		MulticastSocket receiver;
		CHECK(receiver.Initialize("225.0.0.224", 54996, true));
		CHECK(receiver.Join());

		CHECK_EQUAL(sizeof(expected), sender.Send(expected, sizeof(expected)));
		CHECK_EQUAL(sizeof(expected), receiver.Recv(actual, sizeof(actual)));
		CHECK_EQUAL(expected, actual);
		CHECK(receiver.Disband());
	}
#endif
}

#endif //ENABLE_UNIT_TESTS

#endif // !UNITY_WINRT

#endif // ENABLE_SOCKETS
