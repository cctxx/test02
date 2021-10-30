#include "UnityPrefix.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Input/SimulateInputEvents.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/RemoteInput/AndroidRemote.h"
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#include "Editor/Src/Application.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define PORT_ARGUMENT "tcp:7101"
#if UNITY_WIN
#define ADB_PATH1 "\\platform-tools\\adb.exe"
#define ADB_PATH2 "\\tools\\adb.exe"
#else
#define ADB_PATH1 "/platform-tools/adb"
#define ADB_PATH2 "/tools/adb"
#endif

#if UNITY_WIN
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#pragma warning (disable:4996)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <errno.h>
#include <new>

double GetTimeSinceStartup();

enum
{
	kEmptyTouchId = ~0UL,
	kMaxTouchCount = 32
};

#if !ENABLE_NEW_EVENT_SYSTEM
enum TouchPhase
{
	kTouchBegan = 0,
	kTouchMoved = 1,
	kTouchStationary = 2,
	kTouchEnded = 3,
	kTouchCanceled = 4
};
#endif

static float GetReportedHeight();

static inline bool IsBegin(size_t phase)
{
#if ENABLE_NEW_EVENT_SYSTEM
	return phase == Touch::kTouchBegan;
#else
	return kTouchBegan;
#endif
}

static inline bool IsTransitional(size_t phase)
{
#if ENABLE_NEW_EVENT_SYSTEM
	return phase == Touch::kTouchMoved || phase == Touch::kTouchStationary;
#else
	return phase == kTouchMoved || phase == kTouchStationary;
#endif
}

static inline bool IsEnd(size_t phase)
{
#if ENABLE_NEW_EVENT_SYSTEM
	return phase == Touch::kTouchEnded || phase == Touch::kTouchCanceled;
#else
	return phase == kTouchEnded || phase == kTouchCanceled;
#endif
}


class MessageFormatter
{
private:
    unsigned char *b;
    unsigned char *pos;
    unsigned char *limit;

public:
    inline MessageFormatter(unsigned char *t, int lim) :
        b(t),
        pos(t),
        limit(t + lim)
    {}

public:
    inline void WriteInt64(UInt64 value)
    {
        UInt64 val = ((value >> 56) |
                      ((value << 40) & 0x00ff000000000000ULL) |
                      ((value << 24) & 0x0000ff0000000000ULL) |
                      ((value <<  8) & 0x000000ff00000000ULL) |
                      ((value >>  8) & 0x00000000ff000000ULL) |
                      ((value >> 24) & 0x0000000000ff0000ULL) |
                      ((value >> 40) & 0x000000000000ff00ULL) |
                      (value << 56));
        *(reinterpret_cast<UInt64 *>(pos)) = val;
        pos += 8;
    }

    inline void WriteInt32(int value)
    {
        int val = (((value >> 24) & 0x000000ff) |
                   ((value >>  8) & 0x0000ff00) |
                   ((value <<  8) & 0x00ff0000) |
                   ((value << 24) & 0xff000000));
        *(reinterpret_cast<int *>(pos)) = val;
        pos += 4;
    }

    inline void WriteFloat32(float value)
    {
        int v = *(reinterpret_cast<int *>(&value));

        int val = (((v >> 24) & 0x000000ff) |
                   ((v >>  8) & 0x0000ff00) |
                   ((v <<  8) & 0x00ff0000) |
                   ((v << 24) & 0xff000000));
        *(reinterpret_cast<int *>(pos)) = val;
        pos += 4;
    }

    inline unsigned char *GetEnd()
    {
        return pos;
    }

    inline int GetLength()
    {
        return pos - b;
    }

    inline void WriteInt32AtIndex(int index, int value)
    {
        int val = (((value >> 24) & 0x000000ff) |
                   ((value >>  8) & 0x0000ff00) |
                   ((value <<  8) & 0x00ff0000) |
                   ((value << 24) & 0xff000000));
        *(reinterpret_cast<int *>(b + index)) = val;
    }

    inline void Advance(int amount)
    {
        pos += amount;
    }
};


class MessageParser
{
private:
    unsigned char *b;
    unsigned char *pos;
    unsigned char *limit;

public:
    inline MessageParser(unsigned char *t) :
        b(t),
        pos(t),
        limit(t)
    {}

public:
    inline UInt64 ReadInt64()
    {
        UInt64 value = *(reinterpret_cast<UInt64 *>(pos));
        pos += 8;
        return  ((value >> 56) |
                ((value << 40) & 0x00ff000000000000ULL) |
                ((value << 24) & 0x0000ff0000000000ULL) |
                ((value <<  8) & 0x000000ff00000000ULL) |
                ((value >>  8) & 0x00000000ff000000ULL) |
                ((value >> 24) & 0x0000000000ff0000ULL) |
                ((value >> 40) & 0x000000000000ff00ULL) |
                 (value << 56));
    }

    inline int ReadInt32()
    {
        int value = *(reinterpret_cast<int *>(pos));
        pos += 4;
        return (((value >> 24) & 0x000000ff) |
                ((value >>  8) & 0x0000ff00) |
                ((value <<  8) & 0x00ff0000) |
                ((value << 24) & 0xff000000));
    }

    inline float ReadFloat32()
    {
        int value = *(reinterpret_cast<int *>(pos));
        pos += 4;
        int swapped =
                (((value >> 24) & 0x000000ff) |
                 ((value >>  8) & 0x0000ff00) |
                 ((value <<  8) & 0x00ff0000) |
                 ((value << 24) & 0xff000000));
        return *(reinterpret_cast<float *>(&swapped));
    }

    inline unsigned char ReadByte()
    {
        unsigned char value = *pos++;
        return value;
    }

    inline void SetLimit(int lim)
    {
        limit = b + lim;
    }

    inline int GetBytesRemaining()
    {
        return limit - pos;
    }

    inline void ResetPos()
    {
        pos = b;
    }
};

class MessageFormatterLE
{
private:
    unsigned char *b;
    unsigned char *pos;
    unsigned char *limit;

public:
    inline MessageFormatterLE(unsigned char *t, int lim) :
        b(t),
        pos(t),
        limit(t + lim)
    {}

public:
    inline void WriteInt64(UInt64 value)
    {
        *(reinterpret_cast<UInt64 *>(pos)) = value;
        pos += 8;
    }

    inline void WriteInt32(int value)
    {
        *(reinterpret_cast<int *>(pos)) = value;
        pos += 4;
    }

    inline void WriteFloat32(float value)
    {
        *(reinterpret_cast<float *>(pos)) = value;
        pos += 4;
    }

    inline unsigned char *GetEnd()
    {
        return pos;
    }

    inline int GetLength()
    {
        return pos - b;
    }

    inline void WriteInt32AtIndex(int index, int value)
    {
        *(reinterpret_cast<int *>(b + index)) = value;
    }

    inline void Advance(int amount)
    {
        pos += amount;
    }
};

class MessageParserLE
{
private:
    unsigned char *b;
    unsigned char *pos;
    unsigned char *limit;

public:
    inline MessageParserLE(unsigned char *t) :
	b(t),
	pos(t),
	limit(t)
    {}

public:
    inline UInt64 ReadInt64()
    {
        UInt64 value = *(reinterpret_cast<UInt64 *>(pos));
        pos += 8;
        return  value;
    }

    inline int ReadInt32()
    {
        int value = *(reinterpret_cast<int *>(pos));
        pos += 4;
        return value;
    }

    inline float ReadFloat32()
    {
        float value = *(reinterpret_cast<float *>(pos));
        pos += 4;
        return value;
    }

    inline unsigned char ReadByte()
    {
        unsigned char value = *pos++;
        return value;
    }

    inline void SetLimit(int lim)
    {
        limit = b + lim;
    }

    inline int GetBytesRemaining()
    {
        return limit - pos;
    }

    inline void ResetPos()
    {
        pos = b;
    }
};


class TouchImpl : public Touch
{
public:

	TouchImpl()
	{
		Clear();
	}

	long long timestamp;
	size_t frameToReport;
	size_t frameBegan;
	UInt32 endPhaseInQueue;

	void SetDeltaTime(long long newTimestamp)
	{
		if (timestamp == 0)
			return;

		deltaTime = (newTimestamp - timestamp) / 1000.0f;
	}

	void SetDeltaPos(Vector2f const& newPos)
	{
		if (CompareApproximately(pos, Vector2f::zero))
			return;

		deltaPos = newPos - pos;
	}

	bool IsMultitap(long long newTimestamp, Vector2f const& newPos)
	{
		static const int multitapTimeout = 150;
		static const float multitapRadiusSqr = 350.0f;

		return newTimestamp - timestamp < multitapTimeout
			&& SqrMagnitude (pos - newPos) < multitapRadiusSqr;
	}

	void SetTapCount(long long newTimestamp, Vector2f const &newPos)
	{
		if (IsMultitap(newTimestamp, newPos))
			++tapCount;
		else
			tapCount = 1;
	}

	// Clear stuff, but preserve tapCount-related things, which might be needed
	// in case another tap lands really soon
	void Expire()
	{
		id = kEmptyTouchId;
		phase = kTouchCanceled;
		endPhaseInQueue = 0;
		deltaPos = Vector2f(0.0f, 0.0f);
		deltaTime = 0.0f;
		frameToReport = 0;
		frameBegan = 0;
	}

	void Clear()
	{
		Expire();
		pos = Vector2f(0.0f, 0.0f);
		tapCount = 0;
		timestamp = 0;
	}

	bool IsOld(size_t frame) const
	{
		return frameToReport < frame;
	}

	bool IsEmpty () const
	{
		return id == kEmptyTouchId;
	}

	bool IsFinished () const
	{
		return IsEmpty () || IsEnd (phase);
	}

	bool WillBeFinishedNextFrame() const
	{
		return !IsEmpty() && IsEnd(endPhaseInQueue);
	}

	bool IsNow(size_t frame) const
	{
		return frameToReport == frame;
	}
};


static TouchImpl gAllTouches[kMaxTouchCount * 2];
static TouchImpl *gTouchBuffer = gAllTouches;
static TouchImpl *gVirtualTouches = gAllTouches + kMaxTouchCount;
static size_t gFrameCount = 0;


TouchImpl* FindByPointerId(size_t pointerId)
{
	// check if we have touch in the array already
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (gTouchBuffer[i].id == pointerId)
		{
			return &(gTouchBuffer[i]);
		}
	}

	return NULL;
}


TouchImpl* AllocateNew(TouchImpl *touches)
{
	// find empty slot for touch
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& t = touches[i];

		if (t.id != kEmptyTouchId)
			continue;

		t.id = i;
		return &t;
	}

	Assert (!"Out of free touches!");
	return NULL;
}

#if ENABLE_NEW_EVENT_SYSTEM
void DispatchTouchEvent(size_t pointerId, Vector2f pos, Touch::TouchPhase newPhase,
                         long long timestamp, size_t currFrame)
#else
void DispatchTouchEvent(size_t pointerId, Vector2f pos, TouchPhase newPhase,
                         long long timestamp, size_t currFrame)
#endif
{
	TouchImpl *touch = FindByPointerId(pointerId);
	bool newEvent = false;

	if (NULL == touch)
	{
		newEvent = true;
		touch = AllocateNew(gTouchBuffer);

		if (NULL == touch)
			return;
	}

	if (IsBegin (newPhase) || newEvent)
	{
		if (!newEvent && IsBegin (newPhase))
		{
			TouchImpl *virtualTouch = AllocateNew(gVirtualTouches);

			if (NULL != virtualTouch)
			{
				// Back up the ID and add max touch count to make sure the ID
				// never clashes with the regular finger ID
				size_t id = virtualTouch->id + kMaxTouchCount;

				*virtualTouch = *touch;
				virtualTouch->id = id;

				// TODO: scan the array of virtual touches and decide the tap
				// count based on coords and timestamps. Without that, tapping
				// in opposite corners of the screen would still increment tap
				// count as if tapping with one finger.
				virtualTouch->tapCount = id - kMaxTouchCount + 2;
#if ENABLE_NEW_EVENT_SYSTEM
				virtualTouch->endPhaseInQueue = Touch::kTouchEnded;
#else
				virtualTouch->endPhaseInQueue = kTouchEnded;
#endif
				virtualTouch->frameToReport = currFrame;
			}
		}
		else
		{
			touch->SetTapCount(timestamp, pos);
		}

		touch->deltaPos = Vector2f(0, 0);
		touch->deltaTime = 0.0f;
		touch->phase = newPhase;
		touch->endPhaseInQueue = 0;
		touch->frameBegan = currFrame;
		touch->pos = pos;
		touch->timestamp = timestamp;
		touch->frameToReport = currFrame;
		return;
	}
	else if (IsEnd(newPhase))
	{
		// if touch began this frame, we will delay end phase for one frame
		if (touch->frameBegan == currFrame)
			touch->endPhaseInQueue = newPhase;
		else
			touch->phase = newPhase;
	}
#if ENABLE_NEW_EVENT_SYSTEM
	else if (newPhase == Touch::kTouchMoved && touch->phase == Touch::kTouchStationary)
#else
	else if (newPhase == kTouchMoved && touch->phase == kTouchStationary)
#endif
	{
		// old event is STATIONARY, the new one is MOVE. Android does not
		// report STATIONARY Events, so if MOVE's deltaPos is not big enough,
		// let's keep it STATIONARY. Promote to MOVE otherwise.
		static const float deltaPosTolerance = 0.5f;

		if (Magnitude(touch->pos - pos) >= deltaPosTolerance)
			touch->phase = newPhase;
	}

	touch->SetDeltaPos (pos);
	touch->pos = pos;
	touch->SetDeltaTime (timestamp);
	touch->timestamp = timestamp;
	touch->frameToReport = currFrame;

	// sanity checks
	//touch->deltaTime = std::max (touch->deltaTime, 0.0f);
	//touch->timestamp = std::max (touch->timestamp, 0.0f);
}

void AddTouchEvent(int pointerId, float x, float y, int newPhase,
                    long long timestamp)
{
	Vector2f pos = Vector2f(x, GetReportedHeight() - y);
#if ENABLE_NEW_EVENT_SYSTEM
	DispatchTouchEvent(pointerId, pos, static_cast<Touch::TouchPhase>(newPhase),
	                    timestamp, gFrameCount);
#else
	DispatchTouchEvent(pointerId, pos, static_cast<TouchPhase>(newPhase),
	                    timestamp, gFrameCount);
#endif
}

void FreeExpiredTouches(size_t eventFrame, TouchImpl *touches)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& touch = touches[i];

		if (touch.IsOld(eventFrame) && touch.IsFinished())
		{
			if (touch.endPhaseInQueue != 0)
				printf_console ("OOPS! cleaning touch.endPhaseInQueue != 0\n");

			touch.Expire();
		}
	}
}


void UpdateTapTouches (size_t eventFrame, TouchImpl *touches)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& touch = touches[i];

		if (touch.IsOld(eventFrame) && !touch.IsFinished ()
		    && touch.WillBeFinishedNextFrame ())
		{
			touch.phase = touch.endPhaseInQueue;
			touch.endPhaseInQueue = 0;
			touch.deltaPos = Vector2f(0, 0);
			touch.frameToReport = eventFrame;
		}
	}
}


void UpdateStationaryTouches(size_t eventFrame)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& touch = gTouchBuffer[i];

		if (touch.IsOld (eventFrame) && !touch.IsFinished ())
		{
#if ENABLE_NEW_EVENT_SYSTEM
			touch.phase = Touch::kTouchStationary;
#else
			touch.phase = kTouchStationary;
#endif
			touch.deltaPos = Vector2f(0, 0);
			touch.frameToReport = eventFrame;
		}
	}
}


void NextTouchFrame()
{
	++gFrameCount;
	FreeExpiredTouches(gFrameCount, gTouchBuffer);
	FreeExpiredTouches(gFrameCount, gVirtualTouches);

	UpdateTapTouches(gFrameCount, gTouchBuffer);
	UpdateTapTouches(gFrameCount, gVirtualTouches);
	UpdateStationaryTouches(gFrameCount);

	SimulateMouseInput();
}


void InitTouches()
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		gTouchBuffer[i].Clear();
		gVirtualTouches[i].Clear();
	}

	gFrameCount = 1;
}
/*
void PackTouchIds ()
{
	// TODO
}
*/

struct AndroidMessage
{
    enum Type
    {
        Initialize = 0,
        TouchInput = 1,
        AccelerometerInput = 2,
        TrackBallInput = 3,
        Shutdown = 4,
        Image = 5,
        Options = 6,
        Key = 7,
        MaximumMessageID = 8
    };
};

static const int MessageSizes[] = {
    0,  // Initialize
    24, // TouchInput (float32, float32, int64, int32, int32)
    16, // AccelerometerInput (float32, float32, float32, int32)
    8,  // TrackBallInput (float32, float32)
    0,  // Shutdown
    0,  // Image
    8,  // Options (int32, int32)
    12, // Key (int32, int32, int32)
};


enum
{
    DATA_SIZE = 1024 * 128,
	REMOTE_PORT = 7101
//	REMOTE_PORT = 7201
};


static unsigned char sDummyImageData[3] = { 0 };

// TODO: move outside of the class?
class AndroidRemote
{
private:
	bool m_SocketOpFailed;
	bool m_AlreadyPlaying;
    bool m_Connected;
    bool m_SDKPathChecked;
	bool m_MissingSDKReported;
    MessageParser m_Parser;
//    MessageParserLE m_Parser;
    int m_Sock;
    int m_Dimension1;
    int m_Dimension2;
    int m_HalfWidth;
    int m_HalfHeight;
    bool m_ImageAltered;
    int m_LastAccelerationTimestamp;
    double m_LastImageTime;
	unsigned char *m_ImageData;
    Image m_EditorImage;
    Acceleration m_LastAcceleration;
    unsigned char m_Data[DATA_SIZE];
    struct sockaddr_in m_SAddr;

public:
    inline AndroidRemote() :
		m_SocketOpFailed(false),
		m_AlreadyPlaying(false),
        m_Connected(false),
        m_SDKPathChecked(false),
		m_MissingSDKReported(false),
        m_Parser(m_Data),
        m_Sock(-1),
        m_Dimension1(0),
        m_Dimension2(0),
        m_HalfWidth(80),
        m_HalfHeight(60),
        m_ImageAltered(false),
        m_LastAccelerationTimestamp(0),
        m_LastImageTime(0.0),
		m_ImageData(sDummyImageData)
    {
        m_LastImageTime = GetTimeSinceStartup();

        InitData();
    }

	int GetReportedWidth() const { return m_Dimension1; }
	int GetReportedHeight() const { return m_Dimension2; }

public:

    ~AndroidRemote();

private:

    void InitData();
    void InputProcess();
    bool InitSocket();
    void DestroySocket();
    bool SendBytes(int len);
    void SendImageToClient();
    void PrepareImage();
    void HandleTouchMessage();
    void HandleAccelerometerMessage();
    void HandleTrackBallMessage();
    void HandleOptionsMessage();
    void HandleKeyMessage();
    void CheckSDKPath();
    void UpdateAccelerometer(float x, float y, float z, int timestamp);

public:

    void UpdateScreenShot(const Rectf &rect);
    Vector3f GetAcceleration();
    void SetConnected(bool value);

public:

    inline bool IsConnected() const
    {
        return m_Connected && m_Sock > 0;
    }

    inline Acceleration GetAccelerationData() const
    {
        return m_LastAcceleration;
    }

	inline bool IsMissingSDKReported() const
	{
		return m_MissingSDKReported;
	}

	inline void SetMissingSDKReported(bool value)
	{
		m_MissingSDKReported = value;
	}

public:

    void Update();
};


static AndroidRemote sAndroidRemote;


void AndroidRemote::InitData()
{
    memset(static_cast<void *>(&m_SAddr),
           0, sizeof(struct sockaddr_in));

    memset(static_cast<void *>(m_Data),
           0, DATA_SIZE);

	InitTouches();
}


AndroidRemote::~AndroidRemote()
{
    DestroySocket();
}


void AndroidRemote::SetConnected(bool value)
{
	if (m_Connected != value)
	{
		m_Connected = value;

		if (!value)
			m_SDKPathChecked = false;

		InitTouches();
	}
}


bool AndroidRemote::InitSocket()
{
	if (m_SocketOpFailed)
	{
		return false;
	}

    if (m_Sock > 0)
    {
        // Socket is already created and connection is either established or in-progress.
		// Check if it's connected and if connected, return true and set appropriate flag.

#if UNITY_OSX || UNITY_LINUX
		fd_set writefds;
		struct timeval tv;

		tv.tv_sec = 0;
		tv.tv_usec = 10;

		FD_ZERO(&writefds);
		FD_SET(m_Sock, &writefds);

		select(m_Sock + 1, NULL, &writefds, NULL, &tv);

		if (FD_ISSET(m_Sock, &writefds))
		{
			int optval = 0;
			socklen_t optlen = sizeof(optval);

			if (getsockopt(m_Sock, SOL_SOCKET, SO_ERROR, &optval, &optlen) > -1)
			{
				if (optval == 0)
				{
					SetConnected(true);
					return true;
				}
/*				else if (optval == ECONNREFUSED)
				{
					close(m_Sock);
					m_Sock = 0;
					return false;
				}*/
			}
		}

		m_SocketOpFailed = true;
		return false;
#else
		fd_set writefds;
		fd_set exceptfds;
		struct timeval tv;

		tv.tv_sec = 0;
		tv.tv_usec = 10;

		FD_ZERO(&writefds);
		FD_SET(m_Sock, &writefds);
		FD_ZERO(&exceptfds);
		FD_SET(m_Sock, &exceptfds);

		int result = select(m_Sock + 1, NULL, &writefds, &exceptfds, &tv);

		if (result == SOCKET_ERROR)
		{
			m_SocketOpFailed = true;
			return false;
		}

		if (FD_ISSET(m_Sock, &writefds))
		{
			SetConnected(true);
			return true;
		}

		if (FD_ISSET(m_Sock, &exceptfds))
		{
			m_SocketOpFailed = true;
			return false;
		}
#endif

        return false;
    }

    if ((m_Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        return false;
    }

	// TODO: refactor these ifdefs...
#if UNITY_OSX || UNITY_LINUX
	int flags = fcntl(m_Sock, F_GETFL);

	if (fcntl(m_Sock, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		close(m_Sock);
		m_Sock = -1;
		return false;
	}
#else
	u_long blockingMode = 1;

	int result = ioctlsocket(m_Sock, FIONBIO, &blockingMode);

	if (result != NO_ERROR)
	{
		closesocket(m_Sock);
		m_Sock = -1;
		return false;
	}
#endif

    m_SAddr.sin_family = AF_INET;
    m_SAddr.sin_port = htons(REMOTE_PORT);
    m_SAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(m_Sock, reinterpret_cast<struct sockaddr *>(&m_SAddr),
                sizeof(struct sockaddr_in)))
    {
#if UNITY_OSX || UNITY_LINUX

		if (errno != EINPROGRESS)
		{
			close(m_Sock);
		}
		else
		{
			// Do not return true yet
			return false;
		}
#else
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			closesocket(m_Sock);
		}
		else
		{
			// Do not return true yet
			return false;
		}
#endif
		m_SocketOpFailed = true;
		m_Sock = -1;
		return false;
    }

	// connect() was successful immediately
	SetConnected(true);

    return true;
}


void AndroidRemote::DestroySocket()
{
	if (m_Sock != -1)
	{
#if UNITY_OSX || UNITY_LINUX
		close(m_Sock);
#else
		closesocket(m_Sock);
#endif
		m_Sock = -1;
	}

    SetConnected(false);
}


bool AndroidRemote::SendBytes(int len)
{
    unsigned char *data = reinterpret_cast<unsigned char *>(m_Data);
    int bytesAlreadySent = 0;

    for (;;)
    {
        int bytesSent = send(m_Sock, reinterpret_cast<const char *>(data + bytesAlreadySent),
            len - bytesAlreadySent, 0);

        if (bytesSent <= 0)
        {
			m_SocketOpFailed = true;
            SetConnected(false);
            return false;
        }

        bytesAlreadySent += bytesSent;

        if (bytesAlreadySent == len)
        {
            break;
        }
    }

    return true;
}


void AndroidRemote::SendImageToClient()
{
    PrepareImage();

    MessageFormatter mf(m_Data, DATA_SIZE);
//    MessageFormatterLE mf(m_Data, DATA_SIZE);

    mf.WriteInt32(AndroidMessage::Image);
    mf.WriteInt32(m_HalfWidth);
    mf.WriteInt32(m_HalfHeight);

	// +4 to reserve one more int for compressed size:
	unsigned char *dataPtr = mf.GetEnd () + 4;
	static const int quality = 80;
	int compressedSize = CompressImageBlock (dataPtr, DATA_SIZE - 16,
	                                         m_HalfWidth, m_HalfHeight,
	                                         quality, m_ImageData);

	if (compressedSize < 0)
	{
		printf_console ("Error compressing frame, dropping...");
		return;
	}

	mf.WriteInt32 (compressedSize);
    mf.Advance(compressedSize);

    SendBytes(mf.GetLength());
}


void AndroidRemote::PrepareImage()
{
    if (m_EditorImage.GetImageData() != 0 && m_HalfWidth > 0 &&
        m_HalfHeight > 0)
    {
        unsigned char *dst = m_ImageData;
        int src_w = m_EditorImage.GetWidth();
        int src_h = m_EditorImage.GetHeight();

		if (src_w >= src_h)
		{
			int x_ratio = (int)((src_w << 16) / m_HalfWidth);
			int y_ratio = (int)((src_h << 16) / m_HalfHeight);

			for (int i = 0; i < m_HalfHeight; i++)
			{
				int y2 = (((m_HalfHeight - i) * y_ratio) >> 16);

				if (y2 >= m_EditorImage.GetHeight())
					y2 = m_EditorImage.GetHeight() - 1;

				unsigned char *src_row = m_EditorImage.GetRowPtr(y2);

				for (int j = 0; j < m_HalfWidth; j++)
				{
					int x2 = ((j * x_ratio) >> 16);

					if (x2 >= m_EditorImage.GetWidth())
						x2 = m_EditorImage.GetWidth() - 1;

					unsigned char *src_pixel = &src_row[x2 * 4];

					*dst++ = src_pixel[2];
					*dst++ = src_pixel[1];
					*dst++ = src_pixel[0];
				}
			}
		}
		else
		{
			int x_ratio = (int)((src_h << 16) / m_HalfWidth);
			int y_ratio = (int)((src_w << 16) / m_HalfHeight);

			for (int i = 0; i < m_HalfHeight; i++)
			{
				for (int j = 0; j < m_HalfWidth; j++)
				{
					int y2 = (((m_HalfWidth - j) * x_ratio) >> 16);

					if (y2 >= m_EditorImage.GetHeight())
						y2 = m_EditorImage.GetHeight() - 1;

					unsigned char *src_row = m_EditorImage.GetRowPtr(y2);

					int x2 = (((m_HalfHeight - i) * y_ratio) >> 16);

					if (x2 >= m_EditorImage.GetWidth())
						x2 = m_EditorImage.GetWidth() - 1;

					unsigned char *src_pixel = &src_row[x2 * 4];

					*dst++ = src_pixel[2];
					*dst++ = src_pixel[1];
					*dst++ = src_pixel[0];
				}
            }
		}
    }
}


void AndroidRemote::InputProcess()
{
	NextTouchFrame();
}


void AndroidRemote::HandleTouchMessage()
{
    float x = m_Parser.ReadFloat32();
    float y = m_Parser.ReadFloat32();
    long long timestamp = m_Parser.ReadInt64();
    int pointer = m_Parser.ReadInt32();
    int action = m_Parser.ReadInt32();

    AddTouchEvent(pointer, x, y, action, timestamp);
}


void AndroidRemote::HandleAccelerometerMessage()
{
    float x = m_Parser.ReadFloat32();
    float y = m_Parser.ReadFloat32();
    float z = m_Parser.ReadFloat32();
    int timestamp = m_Parser.ReadInt32();

    UpdateAccelerometer(x, y, z, timestamp);
}


void AndroidRemote::HandleTrackBallMessage()
{
    /* float x = */ m_Parser.ReadFloat32();
    /* float y = */ m_Parser.ReadFloat32();
}


void AndroidRemote::HandleOptionsMessage()
{
    int dim1 = m_Parser.ReadInt32();
    int dim2 = m_Parser.ReadInt32();

    int hdim1 = dim1 / 2;
    int hdim2 = dim2 / 2;

    if (hdim1 >= 1 && hdim2 >= 1)
    {
        m_Dimension1 = dim1;
        m_Dimension2 = dim2;
        m_HalfWidth = hdim1;
        m_HalfHeight = hdim2;

		if (m_ImageData != sDummyImageData)
		{
			delete m_ImageData;
		}

		m_ImageData = new unsigned char[m_HalfWidth * m_HalfHeight * 3];
    }
}


void AndroidRemote::HandleKeyMessage()
{
    int state = m_Parser.ReadInt32();
    int key = m_Parser.ReadInt32();
    int unicode = m_Parser.ReadInt32();

    InputManager& input = GetInputManager();
    input.SetKeyState(key, state);

    if (state)
    {
        if (key == SDLK_BACKSPACE || key == SDLK_ESCAPE)
        {
            input.GetInputString() += key;
        }
        else
        {
            input.GetInputString() += unicode;
        }
    }
}

static void PrintAndroidSDKWarning()
{
	if (!sAndroidRemote.IsMissingSDKReported() && IsHumanControllingUs())
	{
		BuildTargetPlatform platformGroup =
			GetEditorUserBuildSettings().GetActiveBuildTarget();

		if (platformGroup == kBuild_Android)
		{
			WarningString("Set-up Android SDK path to make Android remote work");
			sAndroidRemote.SetMissingSDKReported(true);
		}
	}
}

void AndroidRemote::CheckSDKPath()
{
    if (!m_SDKPathChecked)
    {
		m_SDKPathChecked = true;

        std::string sdkPath = EditorPrefs::GetString("AndroidSdkRoot");

        if (!sdkPath.empty())
        {
            std::string adbPath = sdkPath + ADB_PATH1;

            int s = GetFileLength(adbPath);

            if (s <= 0)
            {
				adbPath = sdkPath + ADB_PATH2;

				s = GetFileLength(adbPath);

				if (s <= 0)
				{
					PrintAndroidSDKWarning();
					return;
				}
            }

			// Do NOT try to read command output here. If adb is not running yet, at least
			// on Windows it will spawn itself again, inheriting stdout handle. This newly spawned
			// adb will never quit (same as running "adb start-server"), and we'll keep on trying
			// to read it's stdout forever, case 389707
			LaunchTask(adbPath, NULL, "forward", PORT_ARGUMENT,
				PORT_ARGUMENT, NULL);
        }
        else
        {
			PrintAndroidSDKWarning();
        }
    }
}


void AndroidRemote::UpdateScreenShot(const Rectf &rect)
{
	if (IsConnected())
	{
		m_EditorImage.ReformatImage(rect.Width(), rect.Height(), kTexFormatRGBA32);
		GetGfxDevice().CaptureScreenshot(rect.x, rect.y, rect.Width(),
										 rect.Height(), m_EditorImage.GetImageData());
		m_ImageAltered = true;
	}
}


void AndroidRemote::UpdateAccelerometer(float x, float y, float z, int timestamp)
{
    float dt = 0.0f;

	if (m_LastAccelerationTimestamp > 0)
    {
		dt = (timestamp - m_LastAccelerationTimestamp) / 1000.0f;
    }

	m_LastAcceleration.acc.x = x;
	m_LastAcceleration.acc.y = y;
	m_LastAcceleration.acc.z = z;
	m_LastAcceleration.deltaTime = dt;

	m_LastAccelerationTimestamp = timestamp;
}


Vector3f AndroidRemote::GetAcceleration()
{
    Vector3f vec = m_LastAcceleration.acc;

    if (GetScreenManager().GetWidth() > GetScreenManager().GetHeight())
    {
        std::swap(vec.x, vec.y);
        vec.x *= -1.0f;
    }

    return vec;
}

// recv() can legally return -1, but then it must also let us know there's no
// more data to receive:
bool IsEmptyTube (int recvCode)
{
	if (recvCode == -1)
	{
#if UNITY_OSX || UNITY_LINUX
		if (errno == EAGAIN)
#else
		int wsaError = WSAGetLastError ();
		if (wsaError == WSAEWOULDBLOCK || wsaError == WSAENOTCONN /*connection still in progress*/)
#endif
			return true;
	}

	return false;
}

void AndroidRemote::Update()
{
    if (!IsWorldPlaying())
    {
		m_AlreadyPlaying = false;
		m_SocketOpFailed = false;
        DestroySocket();
        return;
    }

	if (!m_AlreadyPlaying)
	{
		// Prevent LaunchTask from being called when initializing playmode. This fires a repaint
		// event on OSX that causes a crash because our GUIViews have not finalized yet. Fix for case 403635.
		if (GetApplication().IsInitializingPlayModeLayout ())
			return;

		m_AlreadyPlaying = true;

		CheckSDKPath();
	}

	if (!IsConnected())
	{
		if (!InitSocket())
		{
			return;
		}
	}

    InputProcess();

    int br = recv(m_Sock, reinterpret_cast<char *>(m_Data), DATA_SIZE, 0);

	if (IsEmptyTube (br))
		return;

    if (br <= 0)
    {
		m_SocketOpFailed = true;
        SetConnected(false);
        return;
    }
    else
    {
		if (!IsConnected())
		{
			SetConnected(true);
		}
    }

    m_Parser.SetLimit(br);

    for (;;)
    {
        int type = m_Parser.ReadInt32();

        if (type >= AndroidMessage::MaximumMessageID || type < 0)
        {
            break;
        }

        int messageSize = MessageSizes[type];
        int bytesRemaining = m_Parser.GetBytesRemaining();

        if (messageSize <= bytesRemaining)
        {
            switch (type)
            {
                case AndroidMessage::TouchInput:
                    HandleTouchMessage();
                    break;
                case AndroidMessage::AccelerometerInput:
                    HandleAccelerometerMessage();
                    break;
                case AndroidMessage::TrackBallInput:
                    HandleTrackBallMessage();
                    break;
                case AndroidMessage::Options:
                    HandleOptionsMessage();
                    break;
                case AndroidMessage::Key:
                    HandleKeyMessage();
                    break;
                default:
                    break;
            }

            bytesRemaining = m_Parser.GetBytesRemaining();

            if (bytesRemaining == 0)
            {
                break;
            }
        }
        else
        {
            printf_console("Error in reading: only partial message is read\n");
            break;
        }
    }

    m_Parser.ResetPos();

    if (m_ImageAltered == true)
    {
        double t = GetTimeSinceStartup();

        if ((t - m_LastImageTime) >= (1.0 / 30.0))
        {
            SendImageToClient();
            m_ImageAltered = false;
            m_LastImageTime = GetTimeSinceStartup();
        }
    }
}


void AndroidRemoteUpdate()
{
    sAndroidRemote.Update();
}


void AndroidUpdateScreenShot(const Rectf &rect)
{
    sAndroidRemote.UpdateScreenShot(rect);
}


bool AndroidHasRemoteConnected()
{
    return sAndroidRemote.IsConnected();
}


void AndroidRemoteSetConnected(bool connected)
{
	sAndroidRemote.SetConnected(connected);
}


static float GetReportedHeight()
{
	return sAndroidRemote.GetReportedHeight();
}



// Input handling

static void MapTouchToEditorCoordinates(Touch &touch)
{
    float d1 = static_cast<float>(sAndroidRemote.GetReportedWidth());
    float d2 = static_cast<float>(sAndroidRemote.GetReportedHeight());

    float touchXMapped = touch.pos.x / d1;
    float touchYMapped = touch.pos.y / d2;
    float touchDeltaXMapped = touch.deltaPos.x / d1;
    float touchDeltaYMapped = touch.deltaPos.y / d2;

    int screenWidthInt = GetScreenManager().GetWidth();
    int screenHeightInt = GetScreenManager().GetHeight();
    float screenWidth = static_cast<float>(screenWidthInt);
    float screenHeight = static_cast<float>(screenHeightInt);

    if (screenWidthInt < screenHeightInt)
    {
        std::swap(touchXMapped, touchYMapped);
        touchYMapped = (1.0f - touchYMapped);

        std::swap(touchDeltaXMapped, touchDeltaYMapped);
        touchDeltaYMapped *= -1.0f;
    }

    touch.pos.x = touchXMapped * screenWidth;
    touch.pos.y = touchYMapped * screenHeight;
    touch.deltaPos.x = touchDeltaXMapped * screenWidth;
    touch.deltaPos.y = touchDeltaYMapped * screenHeight;
}


size_t AndroidGetTouchCount()
{
	size_t count = 0;

	// TODO: on first call to GetTouchCount() per frame, call PackTouchIds() to
	// compact virtual touch IDs to stand out less

	for (size_t i = 0; i < kMaxTouchCount * 2; ++i)
	{
		if (gAllTouches[i].IsNow(gFrameCount)
		    && !gAllTouches[i].IsEmpty())
		{
			++count;
		}
	}

	return count;
}

size_t AndroidGetActiveTouchCount()
{
	size_t count = 0;

	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (!gAllTouches[i].IsFinished())
		{
			++count;
		}
	}

	return count;
}

#if ENABLE_NEW_EVENT_SYSTEM
Touch* AndroidGetTouch(unsigned int index)
{
	for (size_t i = 0; i < kMaxTouchCount * 2; ++i)
	{
		if (gAllTouches[i].IsNow(gFrameCount)
			&& !gAllTouches[i].IsEmpty ()
			&& index-- == 0)
		{
			Touch* touch = &gAllTouches[i];
			MapTouchToEditorCoordinates(*touch);
			return touch;
		}
	}
	return NULL;
}
#else
bool AndroidGetTouch(unsigned index, Touch& touch)
{
	for (size_t i = 0; i < kMaxTouchCount * 2; ++i)
	{
		if (gAllTouches[i].IsNow(gFrameCount)
		    && !gAllTouches[i].IsEmpty ()
		    && index-- == 0)
		{
			touch = gAllTouches[i];
			MapTouchToEditorCoordinates(touch);
			return true;
		}
	}
	return false;
}
#endif

size_t AndroidGetAccelerationCount()
{
    return 1;
}


void AndroidGetAcceleration(size_t index, Acceleration& acceleration)
{
    if (index == 0)
    {
        acceleration = sAndroidRemote.GetAccelerationData();
    }
    else
    {
        acceleration.acc.x = 0.0f;
        acceleration.acc.y = 0.0f;
        acceleration.acc.z = 0.0f;
        acceleration.deltaTime = 0;
    }
}


unsigned AndroidGetOrientation()
{
    return 0;
}


Vector3f AndroidGetAcceleration()
{
    return sAndroidRemote.GetAcceleration();
}

// Gyroscope
Vector3f AndroidGetGyroRotationRate(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
bool AndroidIsGyroAvailable() { return false; }
Vector3f AndroidGetGyroRotationRateUnbiased(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Vector3f AndroidGetGravity(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Vector3f AndroidGetUserAcceleration(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Quaternionf AndroidGetAttitude(int idx) { return Quaternionf(0.0f, 0.0f, 0.0f, 1.0f); }
bool AndroidIsGyroEnabled(int idx) { return false; }
void AndroidSetGyroEnabled(int idx, bool enabled) {}
float AndroidGetGyroUpdateInterval(int idx) { return 0.0f; }
void AndroidSetGyroUpdateInterval(int idx, float interval) {}
int AndroidGetGyro() { return 0; }

float AndroidGetScreenWidth()
{
    return sAndroidRemote.GetReportedWidth();
}
float AndroidGetScreenHeight()
{
    return sAndroidRemote.GetReportedHeight();
}
