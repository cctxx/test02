#include "UnityPrefix.h"
#include "NetworkUtility.h"

#if UNITY_OSX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>
#include "External/RakNet/builds/include/GetTime.h"
#elif UNITY_WIN
#include "External/RakNet/builds/include/GetTime.h"
#include <winsock2.h>
#if UNITY_WINRT
#include "PlatformDependent/MetroPlayer/MetroUtils.h"
#endif
#if !UNITY_WP8
#include <iphlpapi.h>
#include <windns.h>
#endif
#elif UNITY_WII || UNITY_PEPPER
#pragma message("Dummy network")
#elif UNITY_IPHONE || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#elif UNITY_PS3
#include <netex/libnetctl.h>
typedef unsigned long   u_long;
#elif UNITY_XENON
#include <Xtl.h>
#elif UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#include "PlatformDependent/AndroidPlayer/AndroidSystemInfo.h"
#include "Runtime/Threads/Thread.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include <errno.h>
#include <android/log.h>
#if DEBUGMODE
	#define TRACE_PING(...)	__android_log_print(ANDROID_LOG_VERBOSE, "PING", __VA_ARGS__)
#else
	#define TRACE_PING(...)
#endif
#elif UNITY_FLASH
#elif ENABLE_NETWORK
#error "Unsupported platform"
#endif


#if ENABLE_NETWORK
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "NetworkEnums.h"
#include "External/RakNet/builds/include/GetTime.h"
#include "External/RakNet/builds/include/SocketLayer.h"
#include "Configuration/UnityConfigureVersion.h"
#endif


void NetworkInitialize ()
{
#if USE_WINSOCK_APIS
	WSADATA WsaData;
	WSAStartup( MAKEWORD(2,2), &WsaData );
#endif
}

void NetworkCleanup ()
{
#if USE_WINSOCK_APIS
	WSACleanup();
#endif
}


std::string GetLocalIP() 
{
	std::string returnValue = "0.0.0.0";
	
	#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
	
	struct in_addr		remote;
	struct sockaddr_in  raddress; 
	struct sockaddr     *raddr = (struct sockaddr *)&raddress; 

	struct sockaddr_in  laddress; 
	struct sockaddr     *laddr = (struct sockaddr *)&laddress; 
	
	int sock;
	int err;
	
	std::string result;
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); 
	if ( sock < 1 ) {
		perror("GetLocalIP: Error setting socket");
		return returnValue; 
	}
	
	// www.unity3d.com hardcoded
	inet_aton("83.221.146.11", &remote);
	
	raddress.sin_port = htons(80);
	raddress.sin_family = AF_INET; 
	raddress.sin_addr = remote; 
	
	err = connect(sock, raddr, sizeof(raddress )); 
	if ( err < 0 ) { 
		perror("GetLocalIP: Error during connect");
		char availableIPs[10][16];
		int ipCount = GetIPs(availableIPs);
		close(sock);
		if (ipCount > 0) return availableIPs[0];		
		return returnValue; 
	} 
	
	socklen_t len = sizeof(laddress);
	err = getsockname(sock, laddr, &len );
	if ( err < 0 ) { 
		perror("GetLocalIP: Error using getsockname");
		char availableIPs[10][16];
		int ipCount = GetIPs(availableIPs);
		close(sock);
		if (ipCount > 0) return availableIPs[0];		
		return returnValue; 
	}
	
	close(sock); 
	
	returnValue = std::string(inet_ntoa(laddress.sin_addr)); 
	
	#elif UNITY_WIN

	#if !UNITY_METRO
	
	WSADATA WSAData;
	SOCKET s;

	// Initialize winsock
	if( ::WSAStartup(MAKEWORD(2, 2), &WSAData) != 0 ) {
		printf_console( "GetLocalIP: Failed to initialize winsock\n" );
		return returnValue; 
	}

	s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); 
	if ( s == INVALID_SOCKET ) {
		printf_console("GetLocalIP: Error setting socket, %d", ::WSAGetLastError());
		return returnValue; 
	}

	struct sockaddr_in	raddress; 
	struct sockaddr *raddr = (struct sockaddr *)&raddress; 

	struct sockaddr_in laddress; 
	struct sockaddr * laddr = (struct sockaddr *)&laddress; 

	int     err; 
		
	// www.unity3d.com hardcoded
	char target[16];
	strcpy(target, "83.221.146.11");

	raddress.sin_family = AF_INET;
	raddress.sin_port = htons(80);
	raddress.sin_addr.s_addr = inet_addr(target);

	err = connect(s, raddr, sizeof(raddress )); 
	if ( err != 0 ) { 
		printf_console("GetLocalIP: Error during connect, %d ", ::WSAGetLastError());
		return returnValue; 
	} 
		
	int len = sizeof(laddress);
	err = getsockname(s, laddr, &len );
	if ( err == SOCKET_ERROR ) { 
		printf_console("GetLocalIP: Error using getsockname, %d ", ::WSAGetLastError());
		return returnValue; 
	}
		
	closesocket(s);
		
	::WSACleanup();

	returnValue = std::string(inet_ntoa(laddress.sin_addr)); 

	#else

	#pragma message("todo: implement")	// ?!-

	#endif
	
	#elif UNITY_WII || UNITY_PS3 || UNITY_PEPPER || UNITY_FLASH

	returnValue = "127.0.0.1";

	#elif UNITY_XENON

	XNADDR xboxAddr;
	DWORD ret = XNetGetTitleXnAddr(&xboxAddr);
	if ((ret != XNET_GET_XNADDR_PENDING) &&		// IP address can not be determined yet
		(ret & XNET_GET_XNADDR_NONE) == 0)		// IP address is not available 
	{
		char buffer[64];
		if (XNetInAddrToString(xboxAddr.ina, buffer, sizeof(buffer)) == 0)
		{
			returnValue = buffer;
		}
	}

	#elif ENABLE_NETWORK
	#error "Unsupported platform"
	#endif
	
	return returnValue;
}

int GetIPs( char ips[10][16] )
{
	memset( ips, 0, 10*16 );
	int ipnumber = 0;

#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
	
	struct ifaddrs *myaddrs, *ifa;
	struct sockaddr_in *addr;
	int status;
	status = getifaddrs(&myaddrs);
	if (status != 0)
	{
		perror("Error getting interface addresses");
	}
	
	for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL) continue;
		// Discard inactive interfaces
		if ((ifa->ifa_flags & IFF_UP) == 0) continue;

		// Get IPV4 address
		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			addr = (struct sockaddr_in *)(ifa->ifa_addr);
			if (inet_ntop(ifa->ifa_addr->sa_family, (void *)&(addr->sin_addr), ips[ipnumber], sizeof(ips[ipnumber])) == NULL)
			{
				printf_console("%s: inet_ntop failed!\n", ifa->ifa_name);
			}
			else
			{
				if (strcmp(ips[ipnumber], "127.0.0.1")==0) 
					continue;
				else
				{
					ipnumber++;
					if (ipnumber == 10) break;
				}
			}
		}
	}

	freeifaddrs(myaddrs);

#elif UNITY_WIN

	#if !UNITY_WINRT
	
	PMIB_IPADDRTABLE ipAddrTable;
	DWORD tableSize = 0;
	IN_ADDR in_addr;
	
	ipAddrTable = (MIB_IPADDRTABLE*)UNITY_MALLOC(kMemNetwork, sizeof(MIB_IPADDRTABLE));

	if (ipAddrTable)
	{
		// If sizeof(MIB_IPADDRTABLE) is not enough, allocate appropriate size
		if (GetIpAddrTable(ipAddrTable, &tableSize, 0) == ERROR_INSUFFICIENT_BUFFER)
		{
			UNITY_FREE(kMemNetwork, ipAddrTable);
			ipAddrTable = (MIB_IPADDRTABLE*)UNITY_MALLOC(kMemNetwork, tableSize);
		}
	}
	if (ipAddrTable == NULL)
	{	
		printf_console("Memory allocation failed for GetIpAddrTable\n");
		return 0;
	}
	else
	{
		// Get actual data
		DWORD status = 0;
		if ((status = GetIpAddrTable(ipAddrTable, &tableSize, 0)) != NO_ERROR)
		{ 
			printf_console("GetIpAddrTable failed with error %d\n", status);
			LPVOID messageBuffer;
			if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
				NULL, status, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&messageBuffer, 0, NULL))
			{
				printf_console("Error: %s", messageBuffer);
				LocalFree(messageBuffer);
				UNITY_FREE(kMemNetwork, ipAddrTable);
				ipAddrTable = NULL;
			}
			return 0;
		}
	}

	for (int i = 0; i < (int)ipAddrTable->dwNumEntries; i++)
	{
		in_addr.S_un.S_addr = (u_long)ipAddrTable->table[i].dwAddr;
		strcpy( ips[ipnumber], inet_ntoa(in_addr) );
		if (strcmp(ips[ipnumber], "127.0.0.1") == 0) 
			continue;
		else
		{
			++ipnumber;
			if (ipnumber == 10) break;
		}
	}

	if (ipAddrTable)
	{
		UNITY_FREE(kMemNetwork, ipAddrTable);
		ipAddrTable = NULL;
	}

	#else
	using namespace Windows::Networking::Connectivity;

	auto hostnames = NetworkInformation::GetHostNames();
	for (int i = hostnames->Size-1; i >= 0; --i)
	{
		auto ipInfo = hostnames->GetAt(i)->IPInformation;

		if (ipInfo == nullptr)
			continue;

		auto name = hostnames->GetAt(i)->CanonicalName;

		// IPv4 only - TODO IPv6
		if (name->Length() > 16)
			continue;

		strcpy( ips[ipnumber], ConvertStringToUtf8(name).c_str() );
		++ipnumber;
		if (ipnumber == 10) break;
	}
	if (ipnumber < 10)
	{
		strcpy( ips[ipnumber], "127.0.0.1" );
		++ipnumber;
	}
	#endif

#elif UNITY_WII || UNITY_PEPPER || UNITY_FLASH

#elif UNITY_PS3
	cellNetCtlInit();
	CellNetCtlInfo info;
	cellNetCtlGetInfo(CELL_NET_CTL_INFO_IP_ADDRESS, &info);
	ipnumber = 1;
	strcpy(&ips[0][0], info.ip_address);


#elif UNITY_XENON

	std::string localIp = GetLocalIP();
	if (localIp.length() > 0 && localIp.length() < 16)
	{
		ipnumber = 1;
		strcpy(&ips[0][0], localIp.c_str());
	}

#elif UNITY_ANDROID
	struct ifconf ifc;
	struct ifreq ifreqs[8];
	memset(&ifc, 0, sizeof(ifc));
	ifc.ifc_buf = (char*) (ifreqs);
	ifc.ifc_len = sizeof(ifreqs);
	
	struct ifreq* IFR;
	
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		printf_console("android.permission.INTERNET not available?");
		return 0;
	}
	
	if ((ioctl(sock, SIOCGIFCONF, (char*) &ifc )) < 0 )
		ifc.ifc_len = 0;
	
	char* ifrp;
	struct ifreq* ifr, ifr2;
	IFR = ifc.ifc_req;
	for (ifrp = ifc.ifc_buf;
		 (ifrp - ifc.ifc_buf) < ifc.ifc_len;
		 ifrp += sizeof(ifr->ifr_name) + sizeof(struct sockaddr))		
	{
		ifr = (struct ifreq*)ifrp;
		
		// Get network interface flags
		ifr2 = *ifr;
		if (ioctl(sock, SIOCGIFFLAGS, &ifr2) < 0)
			continue;
		
		// Discard inactive interfaces
		if ((ifr2.ifr_flags & IFF_UP) == 0)
			continue;

		// Skip the loopback/localhost interface
		if ((ifr2.ifr_flags & IFF_LOOPBACK))
			continue;
				
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

		strcpy(&ips[ipnumber++][0], inet_ntoa(((struct sockaddr_in*)(&ifr->ifr_addr))->sin_addr));
		if (ipnumber == 10) break;
	}
	close(sock);

#elif ENABLE_NETWORK
#error "Unsupported platform"
#endif

	return ipnumber;
}

std::string GetHostName () 
{
#if UNITY_OSX || (UNITY_WIN && !UNITY_WINRT) || UNITY_IPHONE
	// Get local host name
	char szHostName[128] = "";

	if( ::gethostname(szHostName, sizeof(szHostName)) )
		printf_console( "Failed to get host name - returning IP\n" );
	else
		return std::string(szHostName);
#elif UNITY_WINRT
	using namespace Windows::Networking::Connectivity;

	auto hostnames = NetworkInformation::GetHostNames();
	if (hostnames->Size > 0)
	{
		auto hostname = hostnames->GetAt(0)->DisplayName;
		return ConvertStringToUtf8(hostname);
	}
#elif UNITY_XENON && !MASTER_BUILD

	// Only available in development builds
	char hostName[256] = "";
	DWORD length = sizeof(hostName);
	if (XBDM_NOERR == DmGetXboxName(hostName, &length))
		return std::string(hostName);
	else
		printf_console( "Failed to get host name - returning IP\n" );
#elif UNITY_ANDROID
	return std::string(android::systeminfo::HardwareModel()) + ":" + GetLocalIP();
#endif

	return GetLocalIP();
}




#if ENABLE_SOCKETS && !UNITY_WINRT

std::string InAddrToIP(sockaddr_in* in)
{
	#if UNITY_XENON
		char ip[256] = "";
		XNetInAddrToString(in->sin_addr, ip, sizeof(ip));
		return std::string(ip);
	#else
		return std::string(inet_ntoa(in->sin_addr));
	#endif
}

#endif

#if ENABLE_NETWORK

bool CompareToPrivateRange(u_long host_number)
{
	u_long network_number = htonl(host_number);
	
	if ( (!(network_number > 167772160u && network_number < 184549375u) &&
		  !(network_number > 2851995648u && network_number < 2852061183u) &&
		  !(network_number > 2886729728u && network_number < 2887778303u) &&
		  !(network_number > 3232235520u && network_number < 3232301055u) ) &&
		(network_number != 2130706433) )
		return true;
	return false;
}

bool CheckForPublicAddress()
{
	bool isPublic = false;

	#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_ANDROID || UNITY_WIN || UNITY_BB10 || UNITY_TIZEN
	
	char availableIPs[10][16];
	int ipCount = GetIPs(availableIPs);
	for (int i=0; i<ipCount; ++i)
	{
	#if UNITY_WIN
		unsigned int address;
	#else
		in_addr_t address;
	#endif
		address = inet_addr(availableIPs[i]);
		isPublic = CompareToPrivateRange((u_long)address);
	}

	#elif UNITY_WII || UNITY_PEPPER || UNITY_PS3 || UNITY_XENON

	#else
	#error "Unsupported platform"
	#endif

	return isPublic;
}

#if UNITY_WIN
char* DNSQueryRecursive(const char* domainName)
{
	IN_ADDR ipaddr;
	char* ipAddress;
	PDNS_RECORD pDnsRecord = NULL;

	DNS_STATUS status = DnsQuery(domainName, DNS_TYPE_A, DNS_QUERY_STANDARD, NULL, &pDnsRecord, NULL);
	
	if (status)
		printf_console("DNSLookup: Error looking up %s (%d)\n", domainName, status);
	// If it's a CNAME then the A record is bogus, so lookup again
	else if (pDnsRecord->wType == DNS_TYPE_CNAME)	
	{ 
		char* aliasName;
		size_t size = strlen(pDnsRecord->Data.CNAME.pNameHost);
		ALLOC_TEMP(aliasName, char, size+1);
		strncpy(aliasName, pDnsRecord->Data.CNAME.pNameHost, size+1);
		DnsRecordListFree(pDnsRecord, DnsFreeRecordListDeep);
		return DNSQueryRecursive(aliasName);
	}
	else
	{
		ipaddr.S_un.S_addr = (pDnsRecord->Data.A.IpAddress);
		ipAddress = inet_ntoa(ipaddr);
		DnsRecordListFree(pDnsRecord, DnsFreeRecordListDeep);
		return ipAddress;
	}

	return NULL;
}
#endif


char* DNSLookup(const char* domainName)
{
    char* ipAddress;
	
#if UNITY_WIN
	ipAddress = DNSQueryRecursive(domainName);
	// Fall back to gethostbyname()
	if (!ipAddress)
#endif
		ipAddress = const_cast<char*>(SocketLayer::Instance()->DomainNameToIP(domainName));

	return ipAddress;
}

void ResolveAddress(SystemAddress& address, const char* domainName, const char* betaDomainName, const char* errorMessage)
{
	if (address.binaryAddress == 0)
	{
		char* resolvedAddress;
		if (UNITY_IS_BETA)
			resolvedAddress = DNSLookup(betaDomainName);
		else
			resolvedAddress = DNSLookup(domainName);
		if (resolvedAddress)
			address.SetBinaryAddress(resolvedAddress);
		else
			ErrorString(errorMessage);
	}
}

unsigned short makeChecksum(unsigned short *buffer, int size)
{
	unsigned long cksum=0;
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(unsigned short);
	}
	if (size)
	{
		cksum += *(unsigned char*)buffer;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >>16);
	return (unsigned short)(~cksum);
}


#if UNITY_WIN
/*
// ICMP structures for Windows
typedef struct {
	unsigned char Ttl;                         // Time To Live
	unsigned char Tos;                         // Type Of Service
	unsigned char Flags;                       // IP header flags
	unsigned char OptionsSize;                 // Size in bytes of options data
	unsigned char *OptionsData;                // Pointer to options data
} IP_OPTION_INFORMATION, * PIP_OPTION_INFORMATION;

typedef struct {
	DWORD Address;                             // Replying address
	unsigned long  Status;                     // Reply status
	unsigned long  RoundTripTime;              // RTT in milliseconds
	unsigned short DataSize;                   // Echo data size
	unsigned short Reserved;                   // Reserved for system use
	void *Data;                                // Pointer to the echo data
	IP_OPTION_INFORMATION Options;             // Reply options
} ICMP_ECHO_REPLY, * PICMP_ECHO_REPLY;
*/

// Wrapper for the ICMP library instance handle to make sure it unloads on quit.
class PingLib
{
public:
	PingLib() : hIcmp(NULL) {}

	~PingLib()
	{
		if(hIcmp)
			FreeLibrary(hIcmp);
	}

	HINSTANCE getInstance() {
		if (hIcmp == NULL)
		{
			hIcmp = LoadLibrary("icmp.dll");
			if (hIcmp == 0) 
				printf_console("Unable to locate icmp.dll");
		}
		return hIcmp;
	}
private:
	HINSTANCE hIcmp;
};

PingLib pingLib;

#endif


void* PingImpl(void* data)
{
	Ping& time = *(Ping*)data;
	
	time.SetTime(-1);
	time.SetIsDone(false);
	
	#if UNITY_WIN

	// Get an instance of the libary, it is only loaded once.
	HINSTANCE hIcmp = pingLib.getInstance();

    typedef HANDLE (WINAPI* pfnHV)(VOID);
    typedef BOOL (WINAPI* pfnBH)(HANDLE);
    typedef DWORD (WINAPI* pfnDHDPWPipPDD)(HANDLE, DWORD, LPVOID, WORD, PIP_OPTION_INFORMATION, LPVOID, DWORD, DWORD);
    pfnHV pIcmpCreateFile;
    pfnBH pIcmpCloseHandle;
    pfnDHDPWPipPDD pIcmpSendEcho;
    pIcmpCreateFile = (pfnHV)GetProcAddress(hIcmp, "IcmpCreateFile");
    pIcmpCloseHandle = (pfnBH)GetProcAddress(hIcmp, "IcmpCloseHandle");
    pIcmpSendEcho = (pfnDHDPWPipPDD)GetProcAddress(hIcmp, "IcmpSendEcho");
    if ((pIcmpCreateFile == 0) || (pIcmpCloseHandle == 0) || (pIcmpSendEcho == 0))
	{
        printf_console("Failed to get proc addr info for ICMP functions.");
		time.Release();
        return NULL;
    }

	unsigned int targetAddress = inet_addr(time.GetIP().c_str());

	HANDLE h = pIcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) {
        printf_console("Ping: Error creating icmp handle\n");
		time.Release();
        return NULL;
    }

	char pingBody[56];
	memset(pingBody, 'X', 56);

	LPVOID replyBuffer = malloc(sizeof(ICMP_ECHO_REPLY) + sizeof(pingBody));
	if (!replyBuffer) {
		printf_console("Ping: Error allocating reply buffer\n");
		pIcmpCloseHandle(h);
		time.Release();
        return NULL;
	}

	unsigned int retval = pIcmpSendEcho(h, targetAddress, &pingBody, sizeof(pingBody), 0, replyBuffer, sizeof(ICMP_ECHO_REPLY) + sizeof(pingBody), 1000);
	if (retval == 0) 
	{
		printf_console("Ping: Error performing ICMP transmission. Possibly because of a timeout\n");
		pIcmpCloseHandle(h);
		free(replyBuffer);
		time.Release();
        return NULL;
	}

	PICMP_ECHO_REPLY rep = (PICMP_ECHO_REPLY)replyBuffer;

	time.SetIsDone(true);
	time.SetTime((int)rep->RoundTripTime);

	pIcmpCloseHandle(h);
	free(replyBuffer);
	
	#elif UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
	
	int err = 0;
	char packet[64];
	int psize = 64;
	unsigned short checksum;
	struct sockaddr_in  address_in; 
	struct sockaddr     *address = (struct sockaddr *)&address_in;
	
	if (time.GetIP().empty())
	{
		ErrorString("No IP present in PingTime structure.");
		time.Release();
        return NULL;
	}

	address_in.sin_family = AF_INET;
	address_in.sin_addr.s_addr = inet_addr(time.GetIP().c_str());
	
	// Use a ICMP datagram type IPv4 socket
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);

	if (s < 0)
	{
		perror ("Ping: Error creating socket");
		time.Release();
        return NULL;
	}
	
	// Set timeout for recv
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	err = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	if (err < 0)
	{
		perror("Ping: Error setting socket options");
		time.Release();
        return NULL;
	}
	
	err = connect(s, address, sizeof(address_in)); 
	if (err < 0)
	{
		perror("Ping: Error during connect");
		close(s);
		time.Release();
        return NULL;
	}

	// Prepare ping packet
	memset(packet, 0, psize-1);
	memset(packet+8, 'X', psize-9);	
	((struct icmpheader*)packet)->type = 0x8;
	checksum = makeChecksum( (unsigned short*)&packet, psize);	
	((struct icmpheader*)packet)->checksum = checksum;
	
	/*printf_console("Checksum is: %x\n", checksum);
	printf_console("Packet contents: \n");
	for (int i=0; i<psize-1; i++)
		printf_console("%x", packet[i]);
	printf_console("\n");*/
	
	// NOTE: This is capable of doing multiple pings in a row with updated sequence numbers, but at the moment only a single ping is ever performed
	RakNetTime currTime;
	int count = 1;
	int pingTime = -1;
	for (int i=0; i < count; i++)
	{
		currTime = RakNet::GetTime();
		
		// Send ping
		err = send(s, (packet), psize, 0);
		if ( err != psize )
		{
			perror("Ping: Error sending ICMP packet");
			close(s);
			time.Release();
			return NULL;
		}
				
		// Receive reponse
		char buffer[64];
		err = recv(s, buffer, 64, 0);
		if ( err < 0 )
		{
			// This is a timeout
			perror("Ping: Error receiving ICMP packet response. Possibly a timeout.");
			//lets just return -1 to keep it consistent with windows version
		} 
		else if ( err == 0)
		{
			printf_console("Ping: Nothing to receive");
			close(s);
			time.Release();
			return NULL;
		}
		else
		{
			pingTime = (int)(RakNet::GetTime() - currTime);
			//printf_console("Ping: Ping nr. %d - Packet size is %d - Ping time %d ms\n", i, psize, pingTime);
		}
		
		// Update ping packet
		((struct icmpheader*)packet)->seq_num = htons(i+1);
		((struct icmpheader*)packet)->checksum = 0;
		checksum = makeChecksum( (unsigned short*)&packet, psize);
		((struct icmpheader*)packet)->checksum = checksum;
	}
	
	// Stop receiving and transmitting
	err = close(s);
	if (err < 0)
		perror ("Ping: Error closing socket");
		
	time.SetTime(pingTime);
	time.SetIsDone(true);
	
	#elif UNITY_WII || UNITY_PEPPER || UNITY_PS3 || UNITY_XENON

	data = 0;

	#elif UNITY_ANDROID
	// ICMP sockets are not available without root. The only protocols supported for applications are TCP and UDP.
	// Because /system/bin/ping is a setuid program, we can use that to send ICMP packets.
	// (using "/system/bin/ping -qnc 1 <addr>")

	char ip[256];
	strncpy(ip, time.GetIP().c_str(), sizeof(ip)); ip[sizeof(ip)-1] = 0;
	time.SetTime(-1);		// in case of error, return -1 to keep it consistent with windows version

	// create stdout pipe from child process
	int	rwpipes[2];
	if( pipe(rwpipes) )
	{
		ErrorStringMsg("Error creating pipe! (%s, %i)", strerror(errno), errno);
		time.SetIsDone(true);
		return NULL;
	}
	if( fcntl(rwpipes[0], F_SETFL, O_NONBLOCK) != 0 ||
		fcntl(rwpipes[1], F_SETFL, O_NONBLOCK) != 0 )
	{
		ErrorStringMsg("Unable to set non-blocking pipe! (%s, %i)", strerror(errno), errno);
		time.SetIsDone(true);
		return NULL;
	}

	TRACE_PING("forking process @ %s:%i", __FUNCTION__, __LINE__);
	// fork process
	pid_t pid;
	if( (pid=fork()) == -1)
	{
		ErrorStringMsg("Error forking process! (%s, %i)", strerror(errno), errno);
		time.SetIsDone(true);
		return NULL;
	}

	TRACE_PING("process forked @ %s:%i", __FUNCTION__, __LINE__);

	if (!pid)		// <child> process fork
	{
		TRACE_PING("secondary process @ %s:%i", __FUNCTION__, __LINE__);

		TRACE_PING("dup'ing pipes @ %s:%i", __FUNCTION__, __LINE__);
		dup2(rwpipes[1],1);					// replace stdout with write pipe ..
		dup2(rwpipes[1],2);					// .. and stderr too ..

		TRACE_PING("closing pipes @ %s:%i", __FUNCTION__, __LINE__);
		close(rwpipes[0]);						// .. close read pipe ..

		TRACE_PING("exec'ing @ %s:%i", __FUNCTION__, __LINE__);
		const char* ping = "/system/bin/ping";
		if(execl(ping, ping, "-qnc", "1", ip, NULL) == -1)
		{
			printf("Error spawning child process '%s'! (%s, %i)", ping, strerror(errno), errno);
			exit(1);
		}

		TRACE_PING("error? @ %s:%i", __FUNCTION__, __LINE__);
	}

	// <parent> process fork ...
	TRACE_PING("parent fork @ %s:%i", __FUNCTION__, __LINE__);

	// close write pipe
	close(rwpipes[1]);

	std::string output;
	char buffer[256];

	TRACE_PING("starting looping @ %s:%i", __FUNCTION__, __LINE__);

	// loop until child process has died...
	const double sleep_value = 0.5; // seconds
	const double max_wait = 5.0;	// seconds
	int num_loops = (int)ceil(max_wait/sleep_value);
	int ret_value;
	while(waitpid(pid, &ret_value, WNOHANG) == 0)
	{
		TRACE_PING("reading @ %s:%i", __FUNCTION__, __LINE__);
		int num_read = read(rwpipes[0], buffer, sizeof(buffer));
		if (num_read > 0)
			output.append(buffer, num_read);

		TRACE_PING("sleeping # %i @ %s:%i", num_loops, __FUNCTION__, __LINE__);
		Thread::Sleep(sleep_value);
		if (num_loops-- < 0)
			break;
	}

	// .. read any leftovers in the pipe ..
	int num_read = read(rwpipes[0], buffer, sizeof(buffer));
	if (num_read > 0)
		output.append(buffer, num_read);

	// ... and close read pipe
	close(rwpipes[0]);

	if (num_loops < 0)
	{
		TRACE_PING("pid killed @ %s:%i", __FUNCTION__, __LINE__);
		kill(pid, SIGKILL);
		ret_value = num_loops;
	}

	if (ret_value != 0)
	{
		ErrorString(output.c_str());
		time.SetIsDone(true);
		return NULL;
	}

	float ping_rtt;
	size_t rtt_pos = output.rfind(" = ");
	if (rtt_pos == std::string::npos || sscanf(&output[rtt_pos], "%*s%f", &ping_rtt) != 1)
	{
		ErrorStringMsg("Error parsing ping output!\n%s", output.c_str());
		time.SetIsDone(true);
		return NULL;
	}

	TRACE_PING("ping done %i @ %s:%i", (int)ping_rtt, __FUNCTION__, __LINE__);

	time.SetTime((int)ping_rtt);
	time.SetIsDone(true);

	#else
	#error "Unsupported platform"
	#endif
		
	time.Release();
	return NULL;
}

Ping::Ping (const std::string& ip)
{
	m_Time = -1;
	m_IsDone = false;
	m_IP = ip;
	m_Refcount = 1;
}

int Ping::GetTime() 
{
	Mutex::AutoLock lock(m_Mutex);
	return m_Time;
}

void Ping::SetTime(int value)
{
	Mutex::AutoLock lock(m_Mutex);
	m_Time = value; 
}

int Ping::GetIsDone() 
{
	Mutex::AutoLock lock(m_Mutex);
	return m_IsDone;
}

void Ping::SetIsDone(bool value)
{
	Mutex::AutoLock lock(m_Mutex);
	m_IsDone = value; 
}

std::string Ping::GetIP()
{
	Mutex::AutoLock lock(m_Mutex);
	return m_IP;
}

void Ping::Retain()
{
	Mutex::AutoLock lock(m_Mutex);
	m_Refcount++;
}

void Ping::Release()
{
	Mutex::AutoLock lock(m_Mutex);
	m_Refcount--;
	if (m_Refcount == 0)
		delete this;
}

void SendToAllNetworkViews (const MessageIdentifier& msg, int inData)
{
	MessageData data;
	data.SetData(inData, ClassID (int));
	SendMessageToEveryone(msg, data);
} 

void SendToAllNetworkViews (const MessageIdentifier& msg)
{
	MessageData data;
	SendMessageToEveryone(msg, data);
}

// These are machine specific + depend on network connection, so we can't run them by default
#if ENABLE_UNIT_TESTS && 0

#include "External/UnitTest++/src/UnitTest++.h"

TEST(ResolveAddressWorks)
{
	char* ip = DNSLookup("masterserver.unity3d.com");
	const char* expected = "67.225.180.24";
	bool isValid = strncmp(expected, ip, strlen(expected)) == 0;
	CHECK(isValid);
}

TEST(CollectAllIPsWorks)
{
	char ips[10][16];
	int count = GetIPs(ips);
	printf ("Got %d ips\n", count);
	for (int i=0; i<count; i++)
		printf("\t%s\n", ips[i]);
	CHECK(count == 3);
}

#endif // ENABLE_UNIT_TESTS

#endif // ENABLE_NETWORK
