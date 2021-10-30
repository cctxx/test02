#ifndef RUNTIME_NETWORK_SOCKETS_H
#define RUNTIME_NETWORK_SOCKETS_H

#include "SocketConsts.h"

#if ENABLE_SOCKETS

#include "Runtime/Utilities/NonCopyable.h"

//
// Socket implementations, all sockets are by default non-blocking.
// Metro: the implementation of this is located in PlatformDependent\MetroPlayer\MetroSocket.cpp
//
class Socket : public NonCopyable
{
#if UNITY_WINRT
#	define UNITY_SOCKET_STATIC_ERROR_STATE
#else
#	define UNITY_SOCKET_STATIC_ERROR_STATE static
#endif // UNITY_WINRT

public:
	static TSocketHandle Connect(const char* ip, unsigned short port, time_t timeoutMS = 4000, bool polling = false, bool logConnectError = true);
#if !UNITY_WINRT
	static int Connect(const sockaddr* addr, socklen_t addr_len, time_t timeoutMS = 4000, bool polling = false, bool logConnectError = true);
#endif
	static int Close(TSocketHandle socketHandle);

	static int PollAsyncConnection(TSocketHandle socketHandle, time_t timeoutMS = 0);
	UNITY_SOCKET_STATIC_ERROR_STATE bool CheckError(int result, const char* msg = NULL, int valid_state = 0, int identifier = 0);
	UNITY_SOCKET_STATIC_ERROR_STATE bool WouldBlockError();

	bool WaitForAvailableSendBuffer(time_t timeoutMS);
	bool WaitForAvailableRecvData(time_t timeoutMS);

protected:
	Socket(TSocketHandle socketHandle);
	Socket(int domain, int type, int protocol);
	virtual ~Socket();

	int SetSocketOption(int level, int option, void* value, size_t value_len);
	int SetSocketOption(int level, int option, bool value);

	bool SetReuseAddress(bool reuse);

	int Send(const void* data, size_t data_len, SendUserData* userData = NULL);
	int Recv(void* data, size_t data_len, RecvUserData* userData = NULL);
	
	bool SetIgnoreSIGPIPE(bool ignore);
	bool SetBlocking(bool block);

protected:
	TSocketHandle m_SocketHandle;
	int m_SendRecvFlags;
	volatile int m_SocketError;

private:
	static bool SetBlocking(TSocketHandle socketHandle, bool block);

	UNITY_SOCKET_STATIC_ERROR_STATE int GetError();
	UNITY_SOCKET_STATIC_ERROR_STATE int SetError(int error);

#undef UNITY_SOCKET_STATIC_ERROR_STATE
};

 
#if UNITY_WINRT
namespace UnityPlayer
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class StreamListenerContext sealed
	{
	public:
		StreamListenerContext( Windows::Networking::HostName^ hostname, Platform::String^ serviceName );
		void OnConnection( Windows::Networking::Sockets::StreamSocketListener^ streamSocket, Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^ args);
		Windows::Networking::Sockets::StreamSocketListener^ GetStreamSocket() { return m_listener; }
		Windows::Networking::Sockets::StreamSocket^ GetConnectionSocket() { return m_connectionSocket; }
		void Bind();

	private:
		Windows::Networking::Sockets::StreamSocketListener^ m_listener;
		Windows::Networking::Sockets::StreamSocket^ m_connectionSocket;
		Windows::Networking::HostName^ m_hostname;
		Platform::String^ m_port;
	};
}
#endif // UNITY_WINRT


#endif // ENABLE_SOCKETS

#endif // RUNTIME_NETWORK_SOCKETS_H
