#ifndef MULTICASTSOCKET_H
#define MULTICASTSOCKET_H

#if ENABLE_SOCKETS
#include "Sockets.h"

#if UNITY_WINRT
namespace UnityPlayer
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MulticastSocketContext sealed
	{
	public:
		MulticastSocketContext( Windows::Networking::HostName^ host, Platform::String^ port );

		void OnSocketMessageReceived( Windows::Networking::Sockets::DatagramSocket^ dagSocket, Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs^ args);
		Windows::Networking::Sockets::DatagramSocket^ GetSocket() { return dagSocket; }
		void Bind();
		void Send(const Platform::Array<byte>^ dataToSend);
		void SetLoop(bool loop);

	private:
		Windows::Networking::Sockets::DatagramSocket^ dagSocket;
		Windows::Networking::HostName^ hostName;
		Platform::String^ portNumber;
		bool isLoopingBroadcast;
	};
}
#endif // UNITY_WINRT

class MulticastSocket : protected Socket
{
public:
	MulticastSocket();

	bool Initialize(const char* group, unsigned short port, bool block = false);
	bool Join();
	bool Disband();
	bool SetBroadcast(bool broadcast);
	bool SetTTL(unsigned char ttl);
	bool SetLoop(bool loop);
	int Send(const void* data, size_t data_len);
	int Recv(void* data, size_t data_len, RecvUserData* userData = NULL);

private:
#if UNITY_EDITOR
	bool SetOptionForAllAddresses(int option, const char* msg = NULL);
#endif
	bool m_Bound;
	struct sockaddr_in m_MulticastAddress;
#if UNITY_WINRT
	UnityPlayer::MulticastSocketContext^ m_context;
	bool m_initalised;
#endif
};


#endif
#endif
