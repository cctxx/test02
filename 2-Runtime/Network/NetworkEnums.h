#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "External/RakNet/builds/include/BitStream.h"
#include "External/RakNet/builds/include/MessageIdentifiers.h"
#endif

#include "NetworkViewID.h"
#include "Runtime/Threads/Mutex.h"

enum { /*kMaxOrderChannels = 32, */kDefaultChannel = 0, kInternalChannel = 0, kMaxGroups = 32 };
enum { kUndefindedPlayerIndex = -1 };
// Default timeout when disconnecting
enum { kDefaultTimeout = 200 };

//typedef int NetworkViewID;
typedef SInt32 NetworkPlayer;

struct RPCMsg
{
	std::string name; // name of the function
	NetworkViewID    viewID; // view id
	int    sender; // player index of the sender
	int    group;
	RakNet::BitStream* stream; 
};

struct PlayerTable
{
	int playerIndex;
	// Index for checking what players have received initial state updates, must match the m_InitReceived flag in the network views.
	unsigned int initIndex;
	SystemAddress playerAddress;
	UInt32 mayReceiveGroups;
	UInt32 maySendGroups;
	bool isDisconnected;
	bool relayed;
	std::string guid;
};

struct NetworkMessageInfo
{
	double timestamp;
	int        sender;
	NetworkViewID viewID;
};

// Peer type
enum {
	kDisconnected = 0,
	kServer = 1,
	kClient = 2,
};

// RPC modes
enum {
/*
	self
	buffer
	server
	others
	immediate
*/
	/// The first 2 bits are used for the target
	kServerOnly = 0,
	kOthers = 1,
	kAll = 2,
	kSpecificTarget = 3,
	kTargetMask = 3,
	// The third bit is used for buffering or not
	kBufferRPCMask = 4,
	kRPCModeNbBits  = 3
};

inline UInt32 GetTargetMode (UInt32 mode)
{
	return mode & kTargetMask;
}

enum { kChannelCompressedBits = 5 }; //0-32

// Debug level
enum {
	kImportantErrors = 0,
	kInformational = 1,
	kCompleteLog = 2
};

enum {
	kPlayerIDBase = 10000000
};

enum NetworkSimulation {
	kNoSimulation = 0,
	kBroadband = 1,
	kDSL = 2,
	kISDN = 3,
	kDialUp = 4
};

enum {
	kAlreadyConnectedToOtherServer = -1,
	kFailedToCreatedSocketOrThread = -2,
	kIncorrectParameters = -3,
	kEmptyConnectTarget = -4,
	kInternalDirectConnectFailed = -5,
	kUnkownGameType = -6,
	kCannotConnectToGUIDWithoutNATEnabled = -7
};

// Connection Tester status enums
enum {
	kConnTestError = -2,
	/// Test result undetermined, still in progress.
	kConnTestUndetermined = -1,
	/// Private IP address detected which cannot do NAT punchthrough.
	kPrivateIPNoNATPunchthrough = 0,
	/// Private IP address detected which can do NAT punchthrough.
	kPrivateIPHasNATPunchThrough = 1,
	/// Public IP address detected and game listen port is accessible to the internet.
	kPublicIPIsConnectable = 2,
	/// Public IP address detected but the port it's not connectable from the internet.
	kPublicIPPortBlocked = 3,
	/// Public IP address detected but server is not initialized and no port is listening.
	kPublicIPNoServerStarted = 4,
	/// Port-restricted NAT type, can do NAT punchthrough to everyone except symmetric.
	kLimitedNATPunchthroughPortRestricted = 5,
	/// Symmetric NAT type, cannot do NAT punchthrough to other symmetric types nor port restricted type.
	kLimitedNATPunchthroughSymmetric = 6,
	/// Full cone type, NAT punchthrough fully supported.
	kNATpunchthroughFullCone = 7,
	/// Address-restricted cone type, NAT punchthrough fully supported.
	kNATpunchthroughAddressRestrictedCone = 8
};

enum {
	kRegistrationFailedGameName = 0,
	kRegistrationFailedGameType = 1,
	kRegistrationFailedNoServer = 2,
	kRegistrationSucceeded = 3,
	kHostListReceived = 4
};

enum {
	kConnTestTimeout = 60
};

// Network packet types
enum {
	ID_STATE_UPDATE

#if ENABLE_NETWORK
			= ID_USER_PACKET_ENUM // 127
#endif
			,

        ID_STATE_INITIAL,
        ID_CLIENT_INIT, 
        ID_REMOVE_RPCS,
		ID_REQUEST_CLIENT_INIT,
		ID_PROXY_INIT_MESSAGE,
		ID_PROXY_CLIENT_MESSAGE,
		ID_PROXY_SERVER_MESSAGE,
		ID_PROXY_MESSAGE,
		ID_PROXY_SERVER_INIT,
		// Master server specific network messages. This must be reflected in Tools/MasterServer/MasterServerMessages.h
		ID_DATABASE_ROWID = 200,
		ID_MASTERSERVER_REDIRECT,
		ID_MASTERSERVER_MSG
};

// NetworkViewIDAllocator enums
enum { kDefaultViewIDBatchSize = 50, kMinimumViewIDs = 100 };

inline double TimestampToSeconds (RakNetTime time)
{
	return (double)time / 1000.0;
}

// Host data used with the master server
struct HostData 
{	
	int    useNat;
	std::string gameType;
	std::string gameName;
	int    connectedPlayers;
	int    playerLimit;
	std::vector<std::string> IP;
	int    port;
	bool   passwordProtected;
	std::string comment;
	std::string guid;
};
