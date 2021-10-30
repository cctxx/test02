#ifndef RUNTIME_NETWORK_SOCKETSTREAMS_H
#define RUNTIME_NETWORK_SOCKETSTREAMS_H

#if ENABLE_SOCKETS //|| UNITY_WINRT

#include "Sockets.h"
#include "Runtime/Containers/ExtendedRingbuffer.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"

class SocketStream : public Socket
{
public:
	SocketStream(TSocketHandle socketHandle, bool block);
	virtual ~SocketStream() {};

	virtual int Send(const void* data, UInt32 data_len);
	virtual int Recv(void* data, UInt32 data_len);
	virtual bool IsBlocking() const { return m_IsBlocking; };
	virtual bool IsConnected() const { return m_IsConnected; };
	virtual bool WouldBlockError() { return Socket::WouldBlockError(); }
	virtual bool Shutdown();

	bool SendAll(const void* data, UInt32 data_len);
	bool RecvAll(void* data, UInt32 data_len);

	virtual bool CanSendNonblocking(UInt32 data_len);

protected:
	virtual	bool Poll(UInt64 /*timeoutMS*/ = kDefaultPollTime) { return false; }
	virtual void OnSocketError();

protected:
	bool			m_IsBlocking;
	volatile bool 	m_IsConnected;
};

class BufferedSocketStream : public SocketStream
{
public:
	BufferedSocketStream(TSocketHandle socketHandle, UInt32 sendbufferMaxSize = kDefaultBufferSize, UInt32 recvbufferMaxSize = kDefaultBufferSize);
	virtual ~BufferedSocketStream() {};

	virtual int Send(const void* data, UInt32 data_len);
	virtual int Recv(void* data, UInt32 data_len);
	virtual bool IsBlocking() const { return false; };
	virtual bool IsConnected() const { return (m_IsArtificiallyConnected && m_Recvbuffer.GetAvailableSize() != 0) || m_IsConnected; };
	virtual bool WouldBlockError() { return IsConnected(); }
	virtual bool Shutdown();

	virtual bool Poll(UInt64 timeoutMS = kDefaultPollTime);

	ExtendedGrowingRingbuffer& SendBuffer() { return m_Sendbuffer; };
	ExtendedGrowingRingbuffer& RecvBuffer() { return m_Recvbuffer; };

	const ExtendedGrowingRingbuffer& SendBuffer() const { return m_Sendbuffer; };
	const ExtendedGrowingRingbuffer& RecvBuffer() const { return m_Recvbuffer; };

	virtual bool CanSendNonblocking(UInt32 data_len);

protected:
	BufferedSocketStream(TSocketHandle socketHandle, UInt32 sendbufferMaxSize, UInt32 recvbufferMaxSize, bool block);

	void OnSocketError();
	bool FlushSendbuffer();
	bool FillRecvbuffer();

private:
	volatile bool				m_IsArtificiallyConnected;
	ExtendedGrowingRingbuffer	m_Sendbuffer;
	ExtendedGrowingRingbuffer	m_Recvbuffer;
	Mutex						m_PollMutex;
};

#if SUPPORT_THREADS
class ThreadedSocketStream : public BufferedSocketStream
{
public:
	ThreadedSocketStream(TSocketHandle socketHandle, UInt32 sendbufferMaxSize = kDefaultBufferSize, UInt32 recvbufferMaxSize = kDefaultBufferSize);
	virtual ~ThreadedSocketStream();

	virtual bool Poll(UInt64 /*timeoutMS*/ = kDefaultPollTime) { return IsConnected(); }

private:
	static void* WriterLoop(void* _arg);
	static void* ReaderLoop(void* _arg);

	Thread m_Reader;
	Thread m_Writer;
};
#endif // SUPPORT_THREADS

#endif // ENABLE_SOCKETS
#endif // RUNTIME_NETWORK_SOCKETSTREAMS_H
