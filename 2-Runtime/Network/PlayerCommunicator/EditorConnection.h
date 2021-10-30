#pragma once

#if UNITY_EDITOR

#include "GeneralConnection.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Serialize/SwapEndianBytes.h"


#if UNITY_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Threads/Mutex.h"


class EditorConnection : public GeneralConnection 
{
public:
	EditorConnection (unsigned short multicastPort = PLAYER_MULTICAST_PORT);

	static void Initialize ();
	static void Cleanup ();

	void ResetLastPlayer();
	
	void GetAvailablePlayers( std::vector<UInt32>& values );
	void GetAvailablePlayers( RuntimePlatform platform, std::vector<UInt32>& values );
	std::string GetConnectionIdentifier( UInt32 guid );
	bool IsIdentifierConnectable( UInt32 guid );
	bool IsIdentifierOnLocalhost( UInt32 guid );

	int ConnectPlayer (UInt32 guid);
	int ConnectPlayerDirectIP(const std::string& IP);

	UInt32 AddPlayer(std::string hostName, std::string localIP, unsigned short port, UInt32 guid, int flags);
	void RemovePlayer(UInt32 guid);
	void EnablePlayer(UInt32 guid, bool enable);

	// Singleton accessor for editorconnection
	static EditorConnection& Get ();
	static EditorConnection* ms_Instance;

	MulticastInfo PollWithCustomMessage ();

private:
	virtual bool IsServer () { return false; }

	struct AvailablePlayer
	{
		AvailablePlayer(ABSOLUTE_TIME time, const MulticastInfo& info)
		: m_LastPing (time)
		, m_MulticastInfo(info)
		, m_Enabled(true)
		{}

		ABSOLUTE_TIME m_LastPing;
		MulticastInfo m_MulticastInfo;
		bool m_Enabled;
	};

private:
	typedef std::map< UInt32, AvailablePlayer > AvailablePlayerMap;
	AvailablePlayerMap m_AvailablePlayers;

	ABSOLUTE_TIME m_LastCleanup;
};

#endif
