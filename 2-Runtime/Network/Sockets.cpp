#include "UnityPrefix.h"

#if ENABLE_SOCKETS
#include "Runtime/Network/Sockets.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "PlatformDependent/CommonWebPlugin/Verification.h"

#if UNITY_WIN
#include "PlatformDependent/Win/GetFormattedErrorString.h"
#endif

#define SUPPORT_NON_BLOCKING !UNITY_FLASH

#include "SocketUtils.h"

#define SocketError(msg, identifier) { DebugStringToFile(Format("Socket: %s, error: %s(%d)", msg, GetSocketErrorMsg(Socket::GetError()).c_str(), Socket::GetError()), 0, __FILE_STRIPPED__, __LINE__, kError, 0, identifier); }

#if !UNITY_WINRT

int Socket::PollAsyncConnection(int socketHandle, time_t timeoutMS)
{
#if SUPPORT_NON_BLOCKING
	// Set timeout on socket connection
	struct timeval tv;
	tv.tv_sec = timeoutMS / 1000;
	tv.tv_usec = (timeoutMS - tv.tv_sec * 1000) * 1000;

	if (timeoutMS == 0)
		tv.tv_usec = 10;

	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(socketHandle, &fdset);

	fd_set exceptfds;
	FD_ZERO(&exceptfds);
	FD_SET(socketHandle, &exceptfds);

#if UNITY_PS3
	int selectedDescriptors = socketselect(socketHandle + 1, NULL, &fdset, &exceptfds, &tv);
#else
	int selectedDescriptors = select(socketHandle + 1, NULL, &fdset, &exceptfds, &tv);
#endif
	// we need one selected descriptor to be able to determine socket state
	if (selectedDescriptors == 1)
	{
	#if USE_WINSOCK_APIS
		if (FD_ISSET(socketHandle, &fdset) && !FD_ISSET(socketHandle, &exceptfds))
		{
			Socket::SetError(0);
			return 0;
		}
		Socket::SetError(WSAECONNREFUSED);
	#else
		int so_error = 0;
		socklen_t len = sizeof (so_error);
		if (getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, &so_error, &len) > -1)
			if (Socket::SetError(so_error) == 0)
				return 0;
	#endif
	}
	// we don't have any writeable or errornous descriptors yet
	else if (selectedDescriptors == 0)
	{
		Socket::SetError(kPlatformConnectWouldBlock);
	}

	return -1;
#endif // SUPPORT_NON_BLOCKING
	return 0;
}

// ---------------------------------------------------------------
// Socket::Statics
// ---------------------------------------------------------------
int Socket::Close(int socketHandle)
{
#if USE_WINSOCK_APIS
	return closesocket(socketHandle);
#elif UNITY_PS3
	return socketclose(socketHandle);
#else
	return close(socketHandle);
#endif
}

int Socket::Connect(const char* ip, unsigned short port, time_t timeoutMS, bool polling, bool logConnectError)
{
	struct sockaddr_in addr;
	SetupAddress(inet_addr(ip), htons(port), &addr);
	return Connect((const sockaddr*) &addr, sizeof(addr), timeoutMS, polling, logConnectError);
}

UInt32 ComputeIdentifier(const sockaddr* addr)
{
	if (addr == NULL)
		return 0;

	UInt32 identifier = CRCBegin();
	identifier = CRCFeed( identifier, (const UInt8*)&( ((const sockaddr_in*)addr)->sin_addr.s_addr ), sizeof(unsigned long) );
	identifier = CRCFeed( identifier, (const UInt8*)&( ((const sockaddr_in*)addr)->sin_port ), sizeof(unsigned short) );
	identifier = CRCDone(identifier);

	return identifier;
}

int Socket::Connect(const sockaddr* addr, socklen_t addr_len, time_t timeoutMS, bool polling, bool logConnectError)
{
	int socketHandle;
	int identifier = (int)ComputeIdentifier(addr);
	CheckError(socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), polling ? NULL : "failed to create socket", 0, identifier);
	int so_error = 0;
#if SUPPORT_NON_BLOCKING
	if (!SetBlocking(socketHandle, false) && !polling)
		ErrorStringMsg("unable to set blocking mode");
	if (CheckError(connect(socketHandle, addr, addr_len), (logConnectError && !polling) ? "connect failed" : NULL, kPlatformConnectWouldBlock, identifier))
		so_error = -1;

	// Ensure that the connection is alive
	if (so_error == 0 && timeoutMS != -1)
	{
		if (CheckError(PollAsyncConnection(socketHandle, timeoutMS), (logConnectError && !polling) ? "connect failed" : NULL, 0, identifier))
			so_error = -1;
	}
#else
	if (CheckError(connect(socketHandle, addr, addr_len), (logConnectError && !polling) ? "connect failed" : NULL, 0, identifier))
		so_error = -1;
#endif
	// Handle connection error
	if (so_error != 0)
	{
		if(logConnectError && !polling)
			DebugStringToFile(Format("connect failed"), 0, __FILE_STRIPPED__, __LINE__, kError, 0, identifier);
		Close(socketHandle);
		return -1;
	}

	RemoveErrorWithIdentifierFromConsole(identifier);

	return socketHandle;
}

bool Socket::SetBlocking(int socketHandle, bool blocking)
{
	bool success = true;

#if !SUPPORT_NON_BLOCKING
	success = blocking;
#elif UNITY_PS3
	int nonBlockingValue = blocking ? 0 : 1;
	if (setsockopt(socketHandle, SOL_SOCKET, SO_NBIO, &nonBlockingValue, sizeof(nonBlockingValue)) != 0)
		success = false;
#elif USE_WINSOCK_APIS
	u_long nonBlocking = blocking ? 0 : 1;
	if (ioctlsocket(socketHandle, FIONBIO, &nonBlocking) != 0)
		success = false;
#else // unix
	if ((fcntl(socketHandle, F_SETFL, blocking ? 0 : O_NONBLOCK) == -1))
		success = false;
#endif
	return success;
}

// ---------------------------------------------------------------
// Base Socket
// ---------------------------------------------------------------
Socket::Socket(int domain, int type, int protocol)
: m_SocketError(0)
, m_SendRecvFlags(0)
{
	if (!CheckError(m_SocketHandle = socket(domain, type, protocol), "unable to create socket"))
		SetIgnoreSIGPIPE(true);
}

int Socket::SetSocketOption(int level, int option, void* value, size_t value_len)
{
	return setsockopt(m_SocketHandle, level, option, (char*) value, value_len);
}

int Socket::SetSocketOption(int level, int option, bool optionValue)
{
#if USE_WINSOCK_APIS
	BOOL val = optionValue ? TRUE : FALSE;
#else
	int val = !!optionValue;
#endif
	return SetSocketOption(level, option, &val, sizeof(val));
}

bool Socket::SetReuseAddress(bool reuse)
{
	if (CheckError(SetSocketOption(SOL_SOCKET, SO_REUSEADDR, reuse), "set reusable addr failed"))
		return false;
#ifdef SO_REUSEPORT
	if (CheckError(SetSocketOption(SOL_SOCKET, SO_REUSEPORT, reuse), "set reusable port failed"))
		return false;
#endif
	return true;
}

int Socket::Send(const void* data, size_t data_len, SendUserData* userData)
{
	int result;
	if (userData != NULL)
	{
		result = sendto(m_SocketHandle, (char*) data, data_len, userData->flags | m_SendRecvFlags, userData->dstAddr, userData->dstLen);
	}
	else
	{
		result = sendto(m_SocketHandle, (char*) data, data_len, m_SendRecvFlags, NULL, 0);
	}
	CheckError(result);
	return result;
}

int Socket::Recv(void* data, size_t data_len, RecvUserData* userData)
{
	int result;
	if (userData != NULL)
	{
		result = recvfrom(m_SocketHandle, (char*) data, data_len, userData->flags | m_SendRecvFlags, userData->srcAddr, &userData->srcLen);
	}
	else
	{
		result = recvfrom(m_SocketHandle, (char*) data, data_len, m_SendRecvFlags, NULL, NULL);
	}
	CheckError(result);
	return result;
}

bool Socket::WaitForAvailableSendBuffer( time_t timeoutMS )
{
	// Set timeout on socket connection
	struct timeval tv;
	tv.tv_sec = timeoutMS / 1000;
	tv.tv_usec = (timeoutMS - tv.tv_sec * 1000) * 1000;

	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(m_SocketHandle, &fdset);

#if UNITY_PS3
	return socketselect(m_SocketHandle + 1, NULL, &fdset, NULL, &tv) == 1;
#else
	return select(m_SocketHandle + 1, NULL, &fdset, NULL, &tv) == 1;
#endif
}

bool Socket::WaitForAvailableRecvData( time_t timeoutMS )
{
	// Set timeout on socket connection
	struct timeval tv;
	tv.tv_sec = timeoutMS / 1000;
	tv.tv_usec = (timeoutMS - tv.tv_sec * 1000) * 1000;

	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(m_SocketHandle, &fdset);

#if UNITY_PS3
	return socketselect(m_SocketHandle + 1, &fdset, NULL, NULL, &tv) == 1;
#else
	return select(m_SocketHandle + 1, &fdset, NULL, NULL, &tv) == 1;
#endif
}

#endif // !UNITY_WINRT

std::string GetSocketErrorMsg(int error)
{
#if UNITY_WIN
	return WideToUtf8( GetHResultErrorMessage(error) );
#elif UNITY_FLASH || UNITY_XENON
	return "Unknown network error";
#else
	return strerror(error);
#endif
}

Socket::Socket(TSocketHandle socketHandle)
: m_SocketError(0)
, m_SendRecvFlags(0)
, m_SocketHandle(socketHandle)
{
#if UNITY_WINRT
	AssertBreak(nullptr != m_SocketHandle);
#else
	AssertBreak(m_SocketHandle >= 0);
#endif
	SetIgnoreSIGPIPE(true);
}

Socket::~Socket()
{
	Close(m_SocketHandle);
}

bool Socket::WouldBlockError()
{
	int error = Socket::GetError();
	return error == kPlatformConnectWouldBlock
		|| error == kPlatformStreamWouldBlock
		|| error == kPlatformAcceptWouldBlock;
}

bool Socket::SetIgnoreSIGPIPE(bool ignore)
{
#if USE_WINSOCK_APIS || UNITY_FLASH || UNITY_PS3 || UNITY_WINRT
	return true;
#elif UNITY_OSX || UNITY_IPHONE
	return !CheckError(SetSocketOption(SOL_SOCKET, SO_NOSIGPIPE, ignore), "failed to install NOSIGPIPE");
#else // linux
	m_SendRecvFlags = MSG_NOSIGNAL;
	return true;
#endif
}

bool Socket::SetBlocking(bool blocking)
{
	if (!SetBlocking(m_SocketHandle, blocking))
	{
		ErrorStringMsg("failed to set blocking mode");
		return false;
	}
	return true;
}

#if UNITY_WINRT
// Implemented in MetroSockets.cpp
int SocketWrapperErrorState(TSocketHandle socketHandle);
int SocketWrapperErrorState(TSocketHandle socketHandle, int error);
#endif // UNITY_WINRT

int Socket::GetError()
{
#if UNITY_WINRT
	return SocketWrapperErrorState(m_SocketHandle);
#elif USE_WINSOCK_APIS
	return WSAGetLastError();
#elif UNITY_FLASH
	return flash_errno();
#elif UNITY_PS3
	return sys_net_errno;
#else
	return errno;
#endif
}

int Socket::SetError(int error)
{
#if UNITY_WINRT
	SocketWrapperErrorState(m_SocketHandle, error);
#elif USE_WINSOCK_APIS
	WSASetLastError(error);
#elif UNITY_FLASH
	flash_set_errno(error);
#elif UNITY_PS3
	sys_net_errno = error;
#else
	errno = error;
#endif
	return error;
}

bool Socket::CheckError(int result, const char* msg, int validState, int identifier)
{
	if (result < 0)
	{
		int error = Socket::GetError();
		if (error != validState)
		{
			if (msg)
				SocketError(msg, identifier);
			return true;
		}
	}
	else
	{
		Socket::SetError(0);
	}
	return false;
}

#endif // ENABLE_SOCKETS
