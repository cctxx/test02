#ifndef UNITY_NETWORK_STUBS_H_
#define UNITY_NETWORK_STUBS_H_

#if !ENABLE_NETWORK
typedef int RPCParameters;

struct SystemAddress
{
	const char *ToString(bool writePort=true) const { return "dummy-system-address"; }
	void SetBinaryAddress(const char *str) {}

	unsigned int binaryAddress;
	unsigned short port;
};

//typedef int NetworkPlayer;
typedef int AssetToPrefab;
typedef int NatPunchthrough;

typedef unsigned int RakNetTime;
typedef long long RakNetTimeNS;

class RakPeerInterface;
//class NetworkMessageInfo;

//typedef int PlayerTable;
//typedef int HostData;

/*class Ping
{
	int m_Time;
	bool m_IsDone;
	std::string m_IP;
//	Mutex m_Mutex;
	
	public:
	int GetTime() { return m_Time; }
	void SetTime(int value) { m_Time = value; }
	int GetIsDone() { return m_IsDone; }
	void SetIsDone(bool value) { m_IsDone=value; }
	std::string GetIP() { return m_IP; }
	void SetIP(std::string value) { m_IP = value; }
};*/

class BitstreamPacker
{
public:
	template<class A>
	void Serialize( A const& a ) {}

	template<class A, class B>
	void Serialize( A const& a, B const& b ) {}

	bool IsReading() const { return false; }
	bool IsWriting() const { return false; }
};

namespace RakNet {

class BitStream;

}

typedef int PacketPriority;
/*{
	SYSTEM_PRIORITY,   /// \internal Used by RakNet to send above-high priority messages.
	HIGH_PRIORITY,   /// High priority messages are send before medium priority messages.
	MEDIUM_PRIORITY,   /// Medium priority messages are send before low priority messages.
	LOW_PRIORITY,   /// Low priority messages are only sent when no other messages are waiting.
	NUMBER_OF_PRIORITIES
};*/


#endif


#endif // UNITY_NETWORK_STUBS_H_
