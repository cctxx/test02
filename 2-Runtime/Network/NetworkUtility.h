#pragma once

#include "Runtime/Threads/Mutex.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Network/Sockets.h"

#if ENABLE_NETWORK
#include "External/RakNet/builds/include/RakNetTypes.h"
#endif

void NetworkInitialize();
void NetworkCleanup();

std::string GetLocalIP();
int GetIPs(char ips[10][16]);
std::string GetHostName();

#if ENABLE_SOCKETS && !UNITY_WINRT
std::string InAddrToIP(sockaddr_in* in);
#endif

#if ENABLE_NETWORK

class MessageIdentifier;

bool CheckForPublicAddress();

char* DNSLookup(const char* domainName);
void ResolveAddress(SystemAddress& address, const char* domainName, const char* betaDomainName, const char* errorMessage);

unsigned short makeChecksum(unsigned short *buffer, int size);

void* PingImpl(void* data);

void SendToAllNetworkViews (const MessageIdentifier& msg, int inData);
void SendToAllNetworkViews (const MessageIdentifier& msg);

// The structure of the ICMP header. Mainly used to reference locations inside the ping packet.
struct icmpheader
{
	unsigned char type;
	unsigned char code;
	unsigned short checksum;
	unsigned short identifier;
	unsigned short seq_num;
};


class Ping
{
	int         m_Time;
	bool        m_IsDone;
	std::string m_IP;
	int         m_Refcount;
	Mutex       m_Mutex;
	
	public:
	
	Ping (const std::string& ip);	
	
	int GetTime();
	void SetTime(int value);

	int GetIsDone();
	void SetIsDone(bool value);
	
	std::string GetIP();
	
	void Retain ();
	void Release ();
	
};

#endif
