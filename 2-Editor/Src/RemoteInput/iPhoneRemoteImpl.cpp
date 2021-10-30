#include "UnityPrefix.h"
#include <iostream>
#include <errno.h>
#include "Runtime/Graphics/Image.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Input/LocationService.h"
#include "Runtime/Input/OnScreenKeyboard.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Network/NetworkManager.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Input/SimulateInputEvents.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Export/JPEGMemsrc.h"
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "External/ProphecySDK/src/extlib/jpglib/jpeglib.h"
#include "External/ProphecySDK/src/extlib/jpglib/jmemdst.h"

#if (UNITY_EDITOR && UNITY_OSX)

#include "Tools/iPhone/UnityRemote/MessageSocket/iPhoneRemoteVersion.h"
#include "Tools/iPhone/UnityRemote/Classes/iPhoneRemoteMessage.h"
#include "Editor/Src/RemoteInput/iPhoneInputPackets.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#else

#include "Runtime/Input/GetInput.h"

#endif

#define kUnityRemotePort 2574

enum
{
    kReceiveBufferSize = 0xffff
};


double GetTimeSinceStartup ();

enum UITouchPhase {
    UITouchPhaseBegan,
    UITouchPhaseMoved,
    UITouchPhaseStationary,
    UITouchPhaseEnded,
    UITouchPhaseCancelled
};

namespace iphone
{
    enum iPhoneGeneration
    {
        kiPhoneGenerationUnknown = 0
    };

	size_t GetActiveTouchCount();
    ScreenOrientation GetScreenOrientation();
    void SetScreenOrientation(ScreenOrientation flag);
    bool IsIdleTimerEnabled();
    void SetIdleTimerEnabled(bool flag);
    iPhoneGeneration GetDeviceGeneration();
    void PlayMovie(std::string const& path, ColorRGBA32 const& backgroundColor,
        UInt32 controlMode, UInt32 scalingMode, bool pathIsUrl);
}

int CompressImageBlock (unsigned char* outptr, size_t outBuferSize,
                        size_t w, size_t h, int compression,
                        unsigned char *inptr)
{
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // Size of jpeg after compression
    int numBytes = 0;

    jpeg_memory_dest(&cinfo, (JOCTET *)outptr, outBuferSize, &numBytes);

    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, compression, FALSE);
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];

    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer[0] =
            const_cast<unsigned char*>(&inptr[cinfo.next_scanline * 3 * w]);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

	mem_dest_ptr dest = (mem_dest_ptr)cinfo.dest;

	if (dest->errcount != 0)
		return -1;

    jpeg_destroy_compress(&cinfo);

    return numBytes;
}


struct OutgoingImage
{
    enum
    {
        Components = 3
    };

    unsigned char data[512 * 512 * 3];
    int srcWidth;
    int srcHeight;

    void Prepare(int outWidth, int outHeight, Image const& image);

    inline OutgoingImage() :
    srcWidth(0),
    srcHeight(0)
    {}
};


void OutgoingImage::Prepare(int outWidth, int outHeight, Image const& image)
{
    int imageSrcWidth = image.GetWidth();
    int imageSrcHeight = image.GetHeight();

    srcWidth = imageSrcWidth;
    srcHeight = imageSrcHeight;

    if (srcWidth > srcHeight)
    {
        std::swap(imageSrcWidth, imageSrcHeight);
    }

    float dx = (float)imageSrcWidth / (float)outWidth;
    float dy = (float)imageSrcHeight / (float)outHeight;

    for (int y = 0; y < outHeight; ++y)
    {
        for (int x = 0; x < outWidth; ++x)
        {
            unsigned int dstIndex = y * outWidth + x;

            unsigned int srcX = x * dx;
            unsigned int srcY = y * dy;

            if (srcWidth > srcHeight)
            {
                std::swap(srcX, srcY);
            }
            else
            {
                dstIndex = (outHeight - y - 1) * outWidth + x;
            }

            dstIndex *= 3;

            data[dstIndex] = *(image.GetRowPtr(srcY) + srcX * 4 + 2);
            data[dstIndex + 1] = *(image.GetRowPtr(srcY) + srcX * 4 + 1);
            data[dstIndex + 2] = *(image.GetRowPtr(srcY) + srcX * 4 + 0);
        }
    }

    srcWidth = outWidth;
    srcHeight = outHeight;
}

#if (UNITY_EDITOR && UNITY_OSX)

class IphoneRemote
{
public:
    IphoneRemote();
    virtual ~IphoneRemote();
public:
    void Init(unsigned int *port);
    void Process();
    Touch const& GetTouch(int index);
	void SetConnected(bool connected);
private:
    IphoneRemote(const IphoneRemote &other);
    IphoneRemote& operator= (const IphoneRemote &other);
    void ConsumeOldPacket();
    void SendImageBlock(int y, int m_BlockHeight, int index, int compression);
    void SendMessage(RemoteMessage *data, int length);
private:
    int m_ListenPort;
    int m_ListenFd;
    int m_ClientFd;
    struct sockaddr_in sin;
    struct sockaddr_in m_ClientAddr;
    socklen_t m_ClientAddrLength;
	unsigned long m_LastClientSAddr;
    unsigned char m_ReceiveBuffer[kReceiveBufferSize];

    void ProcessMessage(RemoteMessage *message);
    void SendImage();
	void SendExitMessage();

public:
    Image m_CapturedImage;
    bool m_ScreenshotModified;

    // Points to data field of the last received RemoteMessage
    iphone::InputPacket m_InputPacket;

    // Time when the last ping was received. If the ping is not received for
    // more than 3 seconds, remote is considered disconnected.
    // Time is since startup.
    double m_LastPingTime;

    // Time since startup when the last image was sent.
    // Images are sent 10 times/second.
    double m_LastImageSentTime;

    bool m_RemoteConnected;

    // Remote screen dimensions. This is not necessary strictly the size of
    // the screen of remote device, but rather buffer size preferred by that
    // device.
    // These values are updated with each Ping message received.
    int m_RemoteScreenWidth;
    int m_RemoteScreenHeight;

    bool m_SendImages;
	bool m_MessagePrinted;

    int m_BlockWidth;
    int m_BlockHeight;

    OutgoingImage m_OutgoingImage;

    int m_LastSentMessageID;
    int m_LastReceivedMessageID;

    unsigned char m_OutgoingBuffer[kReceiveBufferSize];
};


IphoneRemote::IphoneRemote() :
    m_ListenPort(-1),
    m_ListenFd(-1),
    m_ClientFd(-1),
    m_ScreenshotModified(false),
    m_LastPingTime(0.0),
    m_RemoteConnected(false),
    m_LastImageSentTime(0.0),
    m_RemoteScreenWidth(0),
    m_RemoteScreenHeight(0),
    m_SendImages(true),
	m_MessagePrinted(false),
    m_BlockWidth(0),
    m_BlockHeight(0),
    m_ClientAddrLength(sizeof(struct sockaddr_in)),
    m_LastClientSAddr(0),
    m_LastSentMessageID(1),
    m_LastReceivedMessageID(0)
{
    memset(&sin, 0, sizeof(sin));
    memset(m_ReceiveBuffer, 0, kReceiveBufferSize);
    memset(m_OutgoingBuffer, 0, kReceiveBufferSize);
}


IphoneRemote::~IphoneRemote()
{
	SendExitMessage();

	close(m_ListenFd);
	close(m_ClientFd);
}


void IphoneRemote::SetConnected(bool connected)
{
	if (!connected)
	{
		m_InputPacket.accelerator = Vector3f(0.0, 0.0, 0.0);
		m_InputPacket.touchCount = 0;
		m_InputPacket.touchPointCount = 0;
		m_LastClientSAddr = 0;
	}

	m_RemoteConnected = connected;
}


void IphoneRemote::Init(unsigned int *port)
{
    int flags = 0;

    m_ListenFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (m_ListenFd == -1)
    {
        printf_console("Cannot create server socket\n");
        return;
    }

    m_ClientFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (m_ClientFd == -1)
    {
        printf_console("Cannot create client socket\n");
        close(m_ClientFd);
        return;
    }

    if ((flags = fcntl(m_ListenFd, F_GETFL, 0)) == -1)
    {
        flags = 0;
    }

    fcntl(m_ListenFd, F_SETFL, flags | O_NONBLOCK);

    if ((flags = fcntl(m_ClientFd, F_GETFL, 0)) == -1)
    {
        flags = 0;
    }

    fcntl(m_ClientFd, F_SETFL, flags | O_NONBLOCK);



    sin.sin_family = AF_INET;
    sin.sin_port = htons(kUnityRemotePort);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(m_ListenFd, (const sockaddr *)&sin, sizeof(sin)) == -1)
    {
        printf_console("Cannot bind to port %i\n", kUnityRemotePort);
    }
    else
    {
        m_ListenPort = kUnityRemotePort;
        *port = kUnityRemotePort;
    }
}


void IphoneRemote::SendExitMessage()
{
	RemoteMessage msg(RemoteMessage::Exit, 0, sizeof(RemoteMessage));
	SendMessage(&msg, sizeof(RemoteMessage));
}


void IphoneRemote::ProcessMessage(RemoteMessage *message)
{
    switch (message->type)
    {
        case RemoteMessage::Exit:
            SetConnected(false);
            break;

            // Unused in editor side
        case RemoteMessage::ImageBlock:
        case RemoteMessage::InvalidLicense:
            break;

        case RemoteMessage::Input:
            read(((unsigned char *)message) + sizeof(RemoteMessage),
                 m_InputPacket);
            break;

        case RemoteMessage::Ping:
            {
                PingMessage *pm = (PingMessage *)message;

                if (pm->m_Version != IPHONE_REMOTE_VERSION)
                {
                    RemoteMessage rm(RemoteMessage::InvalidVersion, 0,
                            sizeof(RemoteMessage));
                    SendMessage(&rm, sizeof(RemoteMessage));
                    break;
                }

                if (pm->width == 320)
                {
                    if (pm->height != 448)
                    {
                        break;
                    }
                }
                else if (pm->width == 192)
                {
                    if (pm->height != 384 && pm->height != 320 &&
                        pm->height != 256)
                    {
                        break;
                    }
                }
                else if (pm->width == 160)
                {
                    if (pm->height != 256 && pm->height != 240)
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }

                if (m_RemoteConnected == false && m_MessagePrinted == false)
                {
                    // Ping message is received for the first time:
                    // client connected
                    LogString(Format("Remote connected (version %i.%i)",
                        IPHONE_REMOTE_VERSION_MAJOR,
                        IPHONE_REMOTE_VERSION_MINOR));
					m_MessagePrinted = true;
                }

                m_SendImages = pm->m_SendImages;

                SetConnected(true);
                //m_LastPingTime = GetTimeSinceStartup();
                m_RemoteScreenWidth = pm->width;
                m_RemoteScreenHeight = pm->height;
                m_BlockWidth = pm->m_BlockWidth;
                m_BlockHeight = pm->m_BlockHeight;
            }

            break;

        default:
            break;
    }

	m_LastPingTime = GetTimeSinceStartup();
}


void IphoneRemote::Process()
{
    ssize_t size = 0;

    double currentTime = GetTimeSinceStartup();

    if ((currentTime - m_LastPingTime) > 2.0)
    {
        SetConnected(false);
    }

    ConsumeOldPacket();

    for (int n = 0; n < 32; n++)
    {
	struct sockaddr_in sain;
	socklen_t addrlen = sizeof(struct sockaddr_in);

        if ((size = recvfrom(m_ListenFd, m_ReceiveBuffer, kReceiveBufferSize,
            0, (struct sockaddr *)&sain, &addrlen)) != -1)
        {
	    if (m_LastClientSAddr == 0)
	    {
		m_LastClientSAddr = sain.sin_addr.s_addr;
	    }
	    else
	    {
		if (sain.sin_addr.s_addr != m_LastClientSAddr)
		{
		    // If we have received packet from one address while we still
		    // have alive connection to another, discard this datagram
		    continue;
		}
	    }

	    memcpy(&m_ClientAddr, &sain, addrlen);
	    m_ClientAddrLength = addrlen;

            RemoteMessage *message = (RemoteMessage *)m_ReceiveBuffer;

            if (size >= sizeof(RemoteMessage) && size == message->length)
            {
                if (m_LastReceivedMessageID != 0 &&
                    message->id <= m_LastReceivedMessageID)
                {
                    printf_console("Out-of-sequence datagram dropped\n");
                    continue;
                }

                ProcessMessage(message);
                continue;
            }
        }

        break;
    }

    // If remote is connected, try to send image if its modified

    if (m_RemoteConnected && m_ScreenshotModified &&
        m_RemoteScreenWidth != 0 &&
        m_RemoteScreenHeight != 0 &&
        m_SendImages)
    {
        // Send image 10 times/sec

        if ((currentTime - m_LastImageSentTime) >= (1.0f / 15.0f) &&
            m_CapturedImage.GetImageData() != 0)
        {
            m_ScreenshotModified = false;
            m_LastImageSentTime = currentTime;

            m_OutgoingImage.Prepare(m_RemoteScreenWidth,
                m_RemoteScreenHeight, m_CapturedImage);
            SendImage();
        }
    }
}

#if ENABLE_NEW_EVENT_SYSTEM
Touch* IphoneRemote::GetTouch (unsigned int index)
#else
Touch const& IphoneRemote::GetTouch(int index)
#endif
{
    Assert(index < iphone::InputPacket::MaxTouchCount);
    Assert(index < m_InputPacket.touchCount);

    size_t count = 0;
    size_t nonConsumedIndex = 0;

    for (; nonConsumedIndex < m_InputPacket.touchCount; ++nonConsumedIndex)
    {
        if (m_InputPacket.touches[nonConsumedIndex].id == ~0UL)
            continue;

        if (count == index)
            break;

        ++count;
    }

    Assert(index <= nonConsumedIndex);

#if ENABLE_NEW_EVENT_SYSTEM
    return &m_InputPacket.touches[nonConsumedIndex];
#else
	return m_InputPacket.touches[nonConsumedIndex];
#endif
}


void IphoneRemote::ConsumeOldPacket()
{
    for (size_t i = 0; i < m_InputPacket.touchCount; ++i)
    {
        if (m_InputPacket.touches[i].id == ~0UL)
            continue;

        Touch& touch = m_InputPacket.touches[i];

        if (touch.phase == UITouchPhaseEnded ||
            touch.phase == UITouchPhaseCancelled)
            touch.id = ~0UL;

        touch.phase = UITouchPhaseStationary;
        touch.deltaPos = Vector2f(0.0f, 0.0f);
    }
}


void IphoneRemote::SendImage()
{
    if (m_RemoteScreenWidth == 0)
    {
        return;
    }

    int maximumBlockHeight = m_BlockHeight;
    int index = 0;

    for (int y = 0; y < m_OutgoingImage.srcHeight; )
    {
        int bh = maximumBlockHeight;

        if ((m_OutgoingImage.srcHeight - y) < maximumBlockHeight)
        {
            bh = m_OutgoingImage.srcHeight - y;
        }

        SendImageBlock(y, bh, index, 50);

        y += bh;
        index++;
    }
}


// Sends image block starting at scanline set by y and of height set by
// m_BlockHeight.
void IphoneRemote::SendImageBlock(int y, int bh, int index, int compression)
{
    ImageMessage *msg = (ImageMessage *)m_OutgoingBuffer;

    msg->width = m_RemoteScreenWidth;
    msg->height = bh;
    msg->compression = compression;
    msg->frameId = 0;
    msg->frameY = index;
    msg->type = RemoteMessage::ImageBlock;

    unsigned char *p = ((unsigned char *)msg) + sizeof(ImageMessage);

    int compressedSize =
    CompressImageBlock(p, sizeof(m_OutgoingBuffer) - sizeof(ImageMessage),
                      m_OutgoingImage.srcWidth, bh, compression,
                      &m_OutgoingImage.data[m_OutgoingImage.srcWidth * 3 * y]);

	if (compressedSize < 0)
	{
		printf_console ("Error compressing image block, dropping...");
		return;
	}

    msg->compressedSize = compressedSize;
    msg->length = compressedSize + sizeof(ImageMessage);

    if (m_ClientAddrLength != 0)
    {
        SendMessage(msg, compressedSize + sizeof(ImageMessage));
    }
}


void IphoneRemote::SendMessage(RemoteMessage *data, int length)
{
    data->id = m_LastSentMessageID++;

    ssize_t bytesSent = sendto(m_ClientFd, (const void *)data, length, 0,
        (struct sockaddr *)&m_ClientAddr, sizeof(struct sockaddr_in));

    if (bytesSent == -1)
    {
        printf("Error: %s\n", strerror(errno));
    }
}

#else
	// Dummy IphoneRemote for Windows Editor (will host Android Remote eventually...)

	namespace iphone {
		struct InputPacket
		{
			enum { MaxTouchCount = 16, MaxTouchPointCount = 16 };

			InputPacket() : accelerator(0,0,0), touchCount(0), touchPointCount(0) {}

			Vector3f accelerator;
			UInt32 touchCount;
			UInt32 touchPointCount:16;
			UInt32 orientation:16;

			Touch touches[MaxTouchCount];
		};
	} // namespace iphone

	class IphoneRemote
	{
	public:
		IphoneRemote()
		: m_RemoteConnected(false)
		, m_RemoteScreenWidth(0)
		, m_RemoteScreenHeight(0) {}
		virtual ~IphoneRemote() {}
	public:
		void Init(unsigned int */*port*/) {}
		void Process() {}
#if ENABLE_NEW_EVENT_SYSTEM
		Touch* GetTouch(unsigned int /*index*/){ return &m_DummyTouch; }
#else
		Touch const& GetTouch(int /*index*/){ return m_DummyTouch; }
#endif
		void SetConnected(bool connected) {}
	private:
		IphoneRemote(const IphoneRemote &other);
		IphoneRemote& operator= (const IphoneRemote &other);
		Touch m_DummyTouch;

	public:
		Image m_CapturedImage;
		bool m_ScreenshotModified;

		// Points to data field of the last received RemoteMessage
		iphone::InputPacket m_InputPacket;

		bool m_RemoteConnected;
		int m_RemoteScreenWidth;
		int m_RemoteScreenHeight;
	};

	Vector3f iPhoneGetAcceleration();

#endif


static IphoneRemote *gRemoteInstance = 0;


bool iPhoneRemoteInputInit(unsigned* port)
{
    gRemoteInstance = new IphoneRemote;
    gRemoteInstance->Init(port);
    return true;
}

void iPhoneRemoteUpdate()
{
    if (gRemoteInstance != 0)
    {
        if (gRemoteInstance->m_RemoteConnected)
            SimulateMouseInput();
        gRemoteInstance->Process();
    }
}

Image& iPhoneGetRemoteScreenShotBuffer()
{
    return gRemoteInstance->m_CapturedImage;
}

void iPhoneDidModifyScreenshotBuffer(bool value)
{
    gRemoteInstance->m_ScreenshotModified = value;
}

void iPhoneRemoteInputShutdown()
{
    delete gRemoteInstance;
    gRemoteInstance = 0;
}

size_t iPhoneGetTouchCount()
{
    if (gRemoteInstance != 0)
    {
        size_t count = 0;

        for (int i = 0; i < gRemoteInstance->m_InputPacket.touchCount; ++i)
        {
            if (gRemoteInstance->m_InputPacket.touches[i].id != ~0UL)
            {
                ++count;
            }
        }

        return count;
    }

    return 0;
}

#if ENABLE_NEW_EVENT_SYSTEM
Touch* iPhoneGetTouch (unsigned int index)
{
	if (gRemoteInstance != 0)
	{
		if (index >= iPhoneGetTouchCount())
		{
			return NULL;
		}

		Touch* touch = gRemoteInstance->GetTouch(index);

		if (touch != NULL)
		{
			touch->pos.x /= 320.0f;
			touch->pos.y /= 480.0f;
			touch->deltaPos.x /= 320.0f;
			touch->deltaPos.y /= 480.0f;

			if (GetScreenManager().GetWidth() > GetScreenManager().GetHeight())
			{
				std::swap(touch->pos.x, touch->pos.y);
				touch->pos.x = (1.0f - touch->pos.x);

				std::swap(touch->deltaPos.x, touch->deltaPos.y);
				touch->deltaPos.x *= -1.0f;
			}

			touch->pos.x *= GetScreenManager().GetWidth();
			touch->pos.y *= GetScreenManager().GetHeight();
			touch->deltaPos.x *= GetScreenManager().GetWidth();
			touch->deltaPos.y *= GetScreenManager().GetHeight();
		}
        return touch;
    }
    return NULL;
}
#else
bool iPhoneGetTouch(unsigned index, Touch& touch)
{
	if (gRemoteInstance != 0)
	{
		if (index >= iPhoneGetTouchCount())
		{
			return false;
		}

		touch = gRemoteInstance->GetTouch(index);

		touch.pos.x /= 320.0f;
		touch.pos.y /= 480.0f;
		touch.deltaPos.x /= 320.0f;
		touch.deltaPos.y /= 480.0f;

		if (GetScreenManager().GetWidth() > GetScreenManager().GetHeight())
		{
			std::swap(touch.pos.x, touch.pos.y);
			touch.pos.x = (1.0f - touch.pos.x);

			std::swap(touch.deltaPos.x, touch.deltaPos.y);
			touch.deltaPos.x *= -1.0f;
		}

		touch.pos.x *= GetScreenManager().GetWidth();
		touch.pos.y *= GetScreenManager().GetHeight();
		touch.deltaPos.x *= GetScreenManager().GetWidth();
		touch.deltaPos.y *= GetScreenManager().GetHeight();
		return true;
	}

	return false;
}
#endif

size_t iPhoneGetActiveTouchCount()
{
    size_t count = 0;
    size_t tc = iPhoneGetTouchCount();

    for (int i = 0; i < tc; ++i)
    {
        if (gRemoteInstance->m_InputPacket.touches[i].id != ~0UL &&
            gRemoteInstance->m_InputPacket.touches[i].phase != UITouchPhaseEnded &&
            gRemoteInstance->m_InputPacket.touches[i].phase != UITouchPhaseCancelled)
        {
            ++count;
        }
    }

    return count;
}

size_t iPhoneGetAccelerationCount()
{ return 1; }

Vector3f iPhoneGetAcceleration()
{
    if (gRemoteInstance)
    {
		Vector3f acceleration = gRemoteInstance->m_InputPacket.accelerator;
		if (GetScreenManager().GetWidth() > GetScreenManager().GetHeight())
        {
            std::swap(acceleration.x, acceleration.y);
			acceleration.x *= -1;
        }
		
        return acceleration;
    }
    else
    {
        return Vector3f(0, 0, 0);
    }
}

void iPhoneGetAcceleration(size_t index, Acceleration& acceleration)
{
    acceleration.acc = iPhoneGetAcceleration();
    acceleration.deltaTime = 0.0f;//gImpl->m_m_InputPacketDeltaTime;
}

// Gyroscope
Vector3f iPhoneGetGyroRotationRate(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
bool iPhoneIsGyroAvailable() { return false; }
Vector3f iPhoneGetGyroRotationRateUnbiased(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Vector3f iPhoneGetGravity(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Vector3f iPhoneGetUserAcceleration(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Quaternionf iPhoneGetAttitude(int idx) { return Quaternionf(0.0f, 0.0f, 0.0f, 1.0f); }
bool iPhoneIsGyroEnabled(int idx) { return false; }
void iPhoneSetGyroEnabled(int idx, bool enabled) {}
float iPhoneGetGyroUpdateInterval(int idx) { return 0.0f; }
void iPhoneSetGyroUpdateInterval(int idx, float interval) {}
int iPhoneGetGyro() { return 0; }

float iPhoneGetScreenWidth()
{
	return gRemoteInstance ? gRemoteInstance->m_RemoteScreenWidth : 0;
}
float iPhoneGetScreenHeight()
{
	return gRemoteInstance ? gRemoteInstance->m_RemoteScreenHeight : 0;
}

bool IsMultiTouchEnabled()
{ return true; }

void SetMultiTouchEnabled(bool flag)
{ }

void SetScreenOrientation(ScreenOrientation flag)
{ }

unsigned iPhoneGetOrientation()
{ return gRemoteInstance != NULL ? gRemoteInstance->m_InputPacket.orientation : 0; }

bool IsIdleTimerEnabled()
{ return false; }

void SetIdleTimerEnabled(bool flag)
{ }

iphone::iPhoneGeneration iphone::GetDeviceGeneration()
{ return iphone::kiPhoneGenerationUnknown; }

void PlayMovie (std::string const& path,
                ColorRGBA32 const& backgroundColor, UInt32 controlMode,
                UInt32 scalingMode, bool pathIsUrl)
{
    if (!GetBuildSettings().hasAdvancedVersion)
    {
        ScriptWarning("You are using basic license. You are not allowed to use iPhoneUtils.PlayMovie()\n", 0);
        return;
    }

    std::string msg = "iPhoneUtils.PlayMovie(" + path + ")";
    LogString(msg);
}

Rectf KeyboardOnScreen::GetRect()
{ return Rectf(0, 0, 0, 0); }

bool KeyboardOnScreen::IsVisible()
{ return false; }

KeyboardOnScreen::KeyboardOnScreen(std::string const& text,
                                   UInt32 keyboardType,
                                   bool autocorrection, bool multiline,
                                   bool secure, bool alert,
                                   std::string const& textPlaceholder_)
{
    textPlaceholder = text;
}

KeyboardOnScreen::~KeyboardOnScreen()
{ }

bool KeyboardOnScreen::isActive() const
{ return false; }

bool KeyboardOnScreen::isDone() const
{ return true; }

bool KeyboardOnScreen::wasCanceled() const
{ return false; }

std::string KeyboardOnScreen::getText() const
{ return textPlaceholder; }

void KeyboardOnScreen::setText(std::string text)
{ textPlaceholder = text; }

void KeyboardOnScreen::setActive(bool flag)
{ }

bool LocationService::IsServiceEnabledByUser()
{ return true; }

void LocationService::SetDesiredAccuracy(float val)
{ }

float LocationService::GetDesiredAccuracy()
{ return 0.0f; }

void LocationService::SetDistanceFilter(float val)
{ }

float LocationService::GetDistanceFilter()
{ return 0.0f; }

LocationInfo LocationService::GetLastLocation()
{
    return LocationInfo();
}

void LocationService::StartUpdatingLocation()
{ }

void LocationService::StopUpdatingLocation()
{ }

LocationServiceStatus LocationService::GetLocationStatus()
{ return kLocationServiceStopped; }

void LocationService::SetHeadingUpdatesEnabled (bool enabled)
{ }

bool LocationService::IsHeadingUpdatesEnabled()
{ return false; }

LocationServiceStatus LocationService::GetHeadingStatus ()
{ return  kLocationServiceStopped; }

const HeadingInfo &LocationService::GetLastHeading ()
{
	static HeadingInfo dummy = { 0, 0, Vector3f::zero, 0 };
	return dummy;
}

bool LocationService::IsHeadingAvailable ()
{ return false; }

void Vibrate ()
{ LogString("iPhoneUtils.Vibrate()"); }

bool IsApplicationGenuine ()
{ return true; }

bool IsApplicationGenuineAvailable ()
{ return true; }

void KeyboardOnScreen::Hide()
{ }

void KeyboardOnScreen::setInputHidden(bool flag)
{ }

bool KeyboardOnScreen::isInputHidden()
{ return true; }

NetworkReachability GetInternetReachability()
{ return ReachableViaLocalAreaNetwork; }

bool iPhoneHasRemoteConnected ()
{
    if (gRemoteInstance)
    {
        return gRemoteInstance->m_RemoteConnected;
    }
    else
    {
        return false;
    }
}

void iPhoneRemoteSetConnected(bool value)
{
    if (gRemoteInstance)
    {
		gRemoteInstance->SetConnected(value);
    }
}
