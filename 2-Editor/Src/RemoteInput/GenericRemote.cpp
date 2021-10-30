/*
 * To be used with the Unity Project C#-based remote at
 *
 * https://rhodecode.unity3d.com/users/remote
 *
 */

#include "UnityPrefix.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Input/SimulateInputEvents.h"
#include "Runtime/Network/SocketStreams.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/RemoteInput/GenericRemote.h"
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#include "Editor/Src/Application.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if UNITY_WIN
#define ADB_PATH1 "\\platform-tools\\adb.exe"
#define ADB_PATH2 "\\tools\\adb.exe"
#else
#define ADB_PATH1 "/platform-tools/adb"
#define ADB_PATH2 "/tools/adb"
#endif

#include <errno.h>
#include <new>

double GetTimeSinceStartup();

enum
{
	kMaxTouchCount = 32
};

enum TouchPhase
{
	kTouchBegan = 0,
	kTouchMoved = 1,
	kTouchStationary = 2,
	kTouchEnded = 3,
	kTouchCanceled = 4
};

namespace 
{

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
}

struct RemoteMessage
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
		ScreenOrientation = 8,
		GyroInput = 9,
		GyroSettings = 10,
		DeviceOrientation = 11,
		MaximumMessageID
	};
};

static const int MessageSizes[] = {
    0,  // Initialize
    28, // TouchInput (float32, float32, int64, int32, int32, int32)
    16, // AccelerometerInput (float32, float32, float32, int32)
    8,  // TrackBallInput (float32, float32)
    0,  // Shutdown
    0,  // Image
    8,  // Options (int32, int32)
    12, // Key (int32, int32, int32)
	0,  // Orientation
	64, // GyroInput (4 * vec3 + vec4)
	8,	// GyroSettings (int32, float32)
	4,	// DeviceOrientation (int32)
};


enum
{
	DATA_SIZE = 1024 * 128,
	REMOTE_PORT = 7201
};
#define PORT_ARGUMENT "tcp:7201"

static unsigned char sDummyImageData[3] = { 0 };

// TODO: move outside of the class?
class GenericRemote
{
private:

public:
	inline GenericRemote() :
	m_SocketHandle(-1),
	m_SocketStream(0),
	m_SocketOpFailed(false),
	m_AlreadyPlaying(false),
	m_Connected(false),
	m_AndroidSDKPathChecked(false),
	m_MissingAndroidSDKReported(false),
	m_Parser(m_Data),
	m_ScreenWidth(0),
	m_ScreenHeight(0),
	m_HalfWidth(80),
	m_HalfHeight(60),
	m_ImageAltered(false),
	m_LastImageTime(0.0),
	m_ImageData(sDummyImageData),
	m_DeviceOrientation(0),
	m_GyroAvailable(false),
	m_GyroEnabled(false),
	m_RotationRate(0.0f, 0.0f, 0.0f),
	m_RotationRateUnbiased(0.0f, 0.0f, 0.0f),
	m_Gravity(0.0f, 0.0f, 0.0f),
	m_UserAcceleration(0.0f, 0.0f, 0.0f),
	m_Attitude(0.0f, 0.0f, 0.0f, 1.0f),
	m_GyroUpdateInterval(0.f)
	{
		m_LastImageTime = GetTimeSinceStartup();

		InitData();
	}

	~GenericRemote();

	void Update();
	void UpdateScreenShot(const Rectf &rect);
	void SetConnected(bool value);
	inline bool IsConnected() const { return m_Connected && m_SocketStream && m_SocketStream->IsConnected(); }

	int GetReportedWidth() const { return m_ScreenWidth; }
	int GetReportedHeight() const { return m_ScreenHeight; }

	size_t GetTouchCount();
	size_t GetActiveTouchCount();

#if ENABLE_NEW_EVENT_SYSTEM
	Touch* GetTouch(unsigned index);
#else
	bool GetTouch (unsigned index, Touch& touch);
#endif
	int m_DeviceOrientation; // Unknown, Portrait/UpsideDown, LandscapeLeft/Right, FaceUp/Down

	Acceleration m_Acceleration;

	bool m_GyroAvailable;
	bool m_GyroEnabled;
	Vector3f m_RotationRate;
	Vector3f m_RotationRateUnbiased;
	Vector3f m_Gravity;
	Vector3f m_UserAcceleration;
	Quaternionf m_Attitude;
	float m_GyroUpdateInterval;

private:

	void InitData();

	void ResetTouches();
	void InputProcess();

	bool InitSocket();
	void DestroySocket();

	bool SendBytes(int len);

	void SendImageToClient();
	void PrepareImage();

	void SendScreenOrientation();
	void SendGyroSettings();

	void HandleOrientationMessage();
	void HandleTouchMessage();
	void HandleAccelerometerMessage();
	void HandleTrackBallMessage();
	void HandleOptionsMessage();
	void HandleKeyMessage();
	void HandleGyroSettings();
	void HandleGyroMessage();

	void CheckAndroidSDKPath();
	void PrintAndroidSDKWarning();


	TSocketHandle m_SocketHandle;
	SocketStream* m_SocketStream;
	bool m_SocketOpFailed;
	bool m_AlreadyPlaying;
	bool m_Connected;
	bool m_AndroidSDKPathChecked;
	bool m_MissingAndroidSDKReported;

	MessageParser m_Parser;

	int m_ScreenWidth;
	int m_ScreenHeight;
	int m_HalfWidth;
	int m_HalfHeight;
	bool m_ImageAltered;

	double m_LastImageTime;
	unsigned char *m_ImageData;
	Image m_EditorImage;

	Touch m_Touches[kMaxTouchCount];

	unsigned char m_Data[DATA_SIZE];
};


static GenericRemote sGenericRemote;


void GenericRemote::InitData()
{
	memset(static_cast<void *>(m_Data),
		0, DATA_SIZE);

	ResetTouches();
}

void GenericRemote::ResetTouches()
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		m_Touches[i].id = -1;
	}
}


GenericRemote::~GenericRemote()
{
	DestroySocket();
}


void GenericRemote::SetConnected(bool value)
{	
	if (m_Connected != value)
	{
		m_Connected = value;

		if (!value)
		{
			m_AndroidSDKPathChecked = false;
			m_ScreenWidth = m_ScreenHeight = -1;
		}

		ResetTouches();
	}
}


bool GenericRemote::InitSocket()
{
	if (m_SocketOpFailed)
		return false;

	if (m_SocketHandle < 0)
		m_SocketHandle = Socket::Connect("127.0.0.1", REMOTE_PORT, -1);

	if (m_SocketHandle > 0)
	{
		if (Socket::PollAsyncConnection(m_SocketHandle) == 0)
			return true;

		if (!Socket::WouldBlockError())
		{
			m_SocketOpFailed = true;
			DestroySocket();
		}
	}
	return false;
}

void GenericRemote::DestroySocket()
{
	if (m_SocketHandle > 0)
		Socket::Close(m_SocketHandle);
	m_SocketHandle = -1;
	delete m_SocketStream;
	m_SocketStream = 0;
	SetConnected(false);
}


bool GenericRemote::SendBytes(int len)
{
	if (!m_SocketStream)
		return false;

	if (!m_SocketStream->SendAll(m_Data, len))
	{
		m_SocketOpFailed = true;
		SetConnected(false);
		return false;
	}

	return true;
}

void GenericRemote::SendScreenOrientation()
{
	MessageFormatter mf(m_Data, DATA_SIZE);

	mf.WriteInt32(RemoteMessage::ScreenOrientation);
	mf.WriteInt32(PlayerSettingsToScreenOrientation(GetPlayerSettings().GetDefaultScreenOrientation()));
	mf.WriteInt32(GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kPortrait)));
	mf.WriteInt32(GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kPortraitUpsideDown)));
	mf.WriteInt32(GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kLandscapeRight)));
	mf.WriteInt32(GetPlayerSettings().GetAutoRotationAllowed(ScreenOrientationToPlayerSettings(kLandscapeLeft)));

	SendBytes(mf.GetLength());
}

void GenericRemote::SendGyroSettings()
{
	if (!m_GyroAvailable)
		return;
	MessageFormatter mf(m_Data, DATA_SIZE);

	mf.WriteInt32(RemoteMessage::GyroSettings);
	mf.WriteInt32(m_GyroEnabled);
	mf.WriteInt32(m_GyroUpdateInterval);

	SendBytes(mf.GetLength());
}

void GenericRemote::SendImageToClient()
{
	PrepareImage();

	MessageFormatter mf(m_Data, DATA_SIZE);

	mf.WriteInt32(RemoteMessage::Image);
	mf.WriteInt32(m_HalfWidth);
	mf.WriteInt32(m_HalfHeight);

	// +4 to reserve one more int for compressed size:
	unsigned char *dataPtr = mf.GetEnd () + 4;
	static int quality = 80;
	int compressedSize = CompressImageBlock (dataPtr, DATA_SIZE - 16,
		m_HalfWidth, m_HalfHeight,
		quality, m_ImageData);

	if (compressedSize < 0)
	{
		printf_console ("Error compressing frame, dropping... Q = %i\n", quality);
		quality = 10;
		return;
	}

	// Try to keep a datarate cap at 11Mbps = 1375KBps ~= 45KB / frame (30fps)
	// Use a setpoint at 90% of maximum datarate
	// Recalculate quality every 5 frames
	const int maxFrameSize = 11 /*Mbps*/ * 1000 * 1000 /*M*/ / 8 /*bits/byte*/ / 30 /*fps*/;
	const int spFrameSize = maxFrameSize * 90 / 100;
	static int accuSize = 0;
	accuSize += compressedSize;
	static int frameCnt = 0;
	if (++frameCnt == 5)
	{
		int avgSize = accuSize / frameCnt;
		float compRatio = (float) avgSize / (float)spFrameSize;
		float oldQuality = (float)quality / 100.f;
		float newQuality = oldQuality / compRatio;
		float midQuality = oldQuality + (newQuality - oldQuality) * (newQuality < oldQuality ? 0.4f : 0.2f);
	//	printf_console("avgSize = %i; Q = %i | compRatio = %1.3f | new Q = %2.0f\n", avgSize, quality, compRatio, midQuality * 100);
		if (midQuality > 0.9f)
			midQuality = 0.9f;
		if (midQuality < 0.f)
			midQuality = 0.f;
		quality = (int)(midQuality * 100.f);
		accuSize = 0;
		frameCnt = 0;
	}

	mf.WriteInt32 (compressedSize);
	mf.Advance(compressedSize);

	SendBytes(mf.GetLength());
}


void GenericRemote::PrepareImage()
{
	if (m_ImageData == sDummyImageData)
		return;

	if (m_EditorImage.GetImageData() == 0 || m_HalfWidth <= 0 || m_HalfHeight <= 0)
		return;

	unsigned char *dst = m_ImageData;
	int src_w = m_EditorImage.GetWidth();
	int src_h = m_EditorImage.GetHeight();

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


void GenericRemote::InputProcess()
{
	SimulateMouseInput();

	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (m_Touches[i].id == i &&
			(m_Touches[i].phase == kTouchEnded ||
				m_Touches[i].phase == kTouchCanceled) )
			m_Touches[i].id = -1;
	}
}

void GenericRemote::HandleOrientationMessage()
{
	m_DeviceOrientation = m_Parser.ReadInt32();
}

void GenericRemote::HandleTouchMessage()
{
	Vector2f pos;
	pos.x = m_Parser.ReadFloat32();
	pos.y = m_Parser.ReadFloat32();
   /* long long timestamp = */m_Parser.ReadInt64();
	int pointer = m_Parser.ReadInt32();
	int action = m_Parser.ReadInt32();
	int tapCount = m_Parser.ReadInt32();

	if (pointer < 0 || pointer >= kMaxTouchCount)
		return;
	if (action != kTouchBegan)
		m_Touches[pointer].deltaPos = pos - m_Touches[pointer].pos;
	m_Touches[pointer].pos = pos;
	m_Touches[pointer].deltaTime = 1.f / 30.f;
	m_Touches[pointer].tapCount = tapCount;
	m_Touches[pointer].phase = action;
	m_Touches[pointer].id = pointer;
}


void GenericRemote::HandleAccelerometerMessage()
{
	float x = m_Parser.ReadFloat32();
	float y = m_Parser.ReadFloat32();
	float z = m_Parser.ReadFloat32();
	float dt = m_Parser.ReadFloat32();

	m_Acceleration.acc.x = x;
	m_Acceleration.acc.y = y;
	m_Acceleration.acc.z = z;
	m_Acceleration.deltaTime = dt;
}


void GenericRemote::HandleTrackBallMessage()
{
    /* float x = */ m_Parser.ReadFloat32();
    /* float y = */ m_Parser.ReadFloat32();
}


void GenericRemote::HandleOptionsMessage()
{
	int dim1 = m_Parser.ReadInt32();
	int dim2 = m_Parser.ReadInt32();

	const float maxPixels = 640 * 480;	// limit the resolution to VGA
	const float screenPixels = dim1 * dim2;
	const int divider = (int)(ceil(sqrt(screenPixels / maxPixels)));

	int hdim1 = dim1 / divider;
	int hdim2 = dim2 / divider;

	if (hdim1 >= 1 && hdim2 >= 1)
	{
		m_ScreenWidth = dim1;
		m_ScreenHeight = dim2;
		m_HalfWidth = hdim1;
		m_HalfHeight = hdim2;

		if (m_ImageData != sDummyImageData)
		{
			delete m_ImageData;
		}

		m_ImageData = new unsigned char[m_HalfWidth * m_HalfHeight * 3];
	}
}


void GenericRemote::HandleKeyMessage()
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

void GenericRemote::HandleGyroSettings()
{
	// the fact that we get this packet is indicative of gyro being supported
	m_GyroAvailable = true;
	// we need to separate read/write here - for now we ignore what the remote says
	/*m_GyroEnabled = */(m_Parser.ReadInt32() != 0);
    /*m_GyroUpdateInterval =*/ m_Parser.ReadFloat32();
}

void GenericRemote::HandleGyroMessage()
{
	m_RotationRate.x = m_Parser.ReadFloat32();
	m_RotationRate.y = m_Parser.ReadFloat32();
	m_RotationRate.z = m_Parser.ReadFloat32();
	m_RotationRateUnbiased.x = m_Parser.ReadFloat32();
	m_RotationRateUnbiased.y = m_Parser.ReadFloat32();
	m_RotationRateUnbiased.z = m_Parser.ReadFloat32();
	m_Gravity.x = m_Parser.ReadFloat32();
	m_Gravity.y = m_Parser.ReadFloat32();
	m_Gravity.z = m_Parser.ReadFloat32();
	m_UserAcceleration.x = m_Parser.ReadFloat32();
	m_UserAcceleration.y = m_Parser.ReadFloat32();
	m_UserAcceleration.z = m_Parser.ReadFloat32();
	m_Attitude.x = m_Parser.ReadFloat32();
	m_Attitude.y = m_Parser.ReadFloat32();
	m_Attitude.z = m_Parser.ReadFloat32();
	m_Attitude.w = m_Parser.ReadFloat32();
}


void GenericRemote::PrintAndroidSDKWarning()
{
	if (!m_MissingAndroidSDKReported && IsHumanControllingUs())
	{
		BuildTargetPlatform platformGroup =
		GetEditorUserBuildSettings().GetActiveBuildTarget();

		if (platformGroup == kBuild_Android)
		{
			WarningString("Set-up Android SDK path to make Android remote work");
			m_MissingAndroidSDKReported = true;
		}
	}
}

void GenericRemote::CheckAndroidSDKPath()
{
	if (!m_AndroidSDKPathChecked)
	{
		m_AndroidSDKPathChecked = true;

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


void GenericRemote::UpdateScreenShot(const Rectf &rect)
{
	if (IsConnected())
	{
		m_EditorImage.ReformatImage(rect.Width(), rect.Height(), kTexFormatRGBA32);
		GetGfxDevice().CaptureScreenshot(rect.x, rect.y, rect.Width(),
			rect.Height(), m_EditorImage.GetImageData());
		m_ImageAltered = true;
	}
}

void GenericRemote::Update()
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

		CheckAndroidSDKPath();
	}

	if (!IsConnected())
	{
		if (!InitSocket())
		{
			return;
		}
		// Wrap socket in a SocketStream - ownership is passed.
		m_SocketStream = new SocketStream(m_SocketHandle, false);
		m_SocketHandle = -1;
		SetConnected(true);
	}

	InputProcess();

	int br = m_SocketStream->Recv(m_Data, DATA_SIZE);

	if (m_SocketStream->WouldBlockError())
		return;

	if (!m_SocketStream->IsConnected())
	{
		m_SocketOpFailed = true;
		DestroySocket();
		return;
	}
	else
	{
		if (!IsConnected())
		{
			SetConnected(true);
		}
		if (br <= 0)
			return;
	}

	m_Parser.SetLimit(br);

	for (;;)
	{
		int type = m_Parser.ReadInt32();

		if (type >=RemoteMessage::MaximumMessageID || type < 0)
		{
			break;
		}

		int messageSize = MessageSizes[type];
		int bytesRemaining = m_Parser.GetBytesRemaining();

		if (messageSize <= bytesRemaining)
		{
			switch (type)
			{
				case RemoteMessage::TouchInput:
					HandleTouchMessage();
					break;
				case RemoteMessage::AccelerometerInput:
					HandleAccelerometerMessage();
					break;
				case RemoteMessage::TrackBallInput:
					HandleTrackBallMessage();
					break;
				case RemoteMessage::Options:
					HandleOptionsMessage();
					break;
				case RemoteMessage::Key:
					HandleKeyMessage();
					break;
				case RemoteMessage::GyroSettings:
					HandleGyroSettings();
					break;
				case RemoteMessage::GyroInput:
					HandleGyroMessage();
					break;
				case RemoteMessage::DeviceOrientation:
					HandleOrientationMessage();
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

			SendScreenOrientation();
			SendGyroSettings();
		}
	}
}


// Input handling

static void MapTouchToEditorCoordinates(Touch &touch)
{
	float d1 = static_cast<float>(sGenericRemote.GetReportedWidth());
	float d2 = static_cast<float>(sGenericRemote.GetReportedHeight());

	float touchXMapped = touch.pos.x / d1;
	float touchYMapped = touch.pos.y / d2;
	float touchDeltaXMapped = touch.deltaPos.x / d1;
	float touchDeltaYMapped = touch.deltaPos.y / d2;

	int screenWidthInt = GetScreenManager().GetWidth();
	int screenHeightInt = GetScreenManager().GetHeight();
	float screenWidth = static_cast<float>(screenWidthInt);
	float screenHeight = static_cast<float>(screenHeightInt);

	touch.pos.x = touchXMapped * screenWidth;
	touch.pos.y = touchYMapped * screenHeight;
	touch.deltaPos.x = touchDeltaXMapped * screenWidth;
	touch.deltaPos.y = touchDeltaYMapped * screenHeight;
}

size_t GenericRemote::GetTouchCount()
{
	size_t count = 0;

	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (m_Touches[i].id != -1)
		{
			++count;
		}
	}

	return count;
}

#if ENABLE_NEW_EVENT_SYSTEM
Touch* GenericRemote::GetTouch (unsigned index)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (m_Touches[i].id != -1
			&& index-- == 0)
		{
			Touch* touch = &m_Touches[i];
			MapTouchToEditorCoordinates(*touch);
			return touch;
		}
	}
	return NULL;
}
#else
bool GenericRemote::GetTouch (unsigned index, Touch& touch)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (m_Touches[i].id != -1
			&& index-- == 0)
		{
			touch = m_Touches[i];
			MapTouchToEditorCoordinates(touch);
			return true;
		}
	}
	return false;
}
#endif

// C-Style interface ftw..

void RemoteUpdate()								{ sGenericRemote.Update(); }
void RemoteUpdateScreenShot(const Rectf &rect)	{ sGenericRemote.UpdateScreenShot(rect); }
bool RemoteIsConnected()						{ return sGenericRemote.IsConnected(); }
void RemoteSetConnected(bool connected)			{ sGenericRemote.SetConnected(connected); }

// Poor-man's manual vtable starts here..

// Device orientation (Input.deviceOrientation)
unsigned RemoteGetDeviceOrientation()			{ return sGenericRemote.m_DeviceOrientation; }

// Touches (Input.touch)
size_t RemoteGetTouchCount()						{ return sGenericRemote.GetTouchCount(); }
size_t RemoteGetActiveTouchCount()					{ return sGenericRemote.GetTouchCount(); }
#if ENABLE_NEW_EVENT_SYSTEM
Touch* RemoteGetTouch(unsigned index)				{ return sGenericRemote.GetTouch(index); }
#else
bool RemoteGetTouch(unsigned index, Touch& touch)	{ return sGenericRemote.GetTouch(index, touch); }
#endif

// Acceleration (Input.acceleration)
size_t RemoteGetAccelerationCount()				{ return 1; }
Vector3f RemoteGetAcceleration()				{ return sGenericRemote.m_Acceleration.acc; }
void RemoteGetAcceleration(size_t index, Acceleration& acceleration) { acceleration = sGenericRemote.m_Acceleration; }

// Gyroscope (Input.gyro)
bool RemoteIsGyroAvailable()						{ return sGenericRemote.m_GyroAvailable; }
bool RemoteIsGyroEnabled(int idx)					{ return sGenericRemote.m_GyroEnabled; }
void RemoteSetGyroEnabled(int idx, bool enabled)	{ sGenericRemote.m_GyroEnabled = enabled; }
int RemoteGetGyro()									{ return 0; }
Vector3f RemoteGetGyroRotationRate(int idx)			{ return sGenericRemote.m_RotationRate; }
Vector3f RemoteGetGyroRotationRateUnbiased(int idx)	{ return sGenericRemote.m_RotationRateUnbiased; }
Vector3f RemoteGetGravity(int idx)					{ return sGenericRemote.m_Gravity; }
Vector3f RemoteGetUserAcceleration(int idx)			{ return sGenericRemote.m_UserAcceleration; }
Quaternionf RemoteGetAttitude(int idx)				{ return sGenericRemote.m_Attitude; }
float RemoteGetGyroUpdateInterval(int idx)			{ return sGenericRemote.m_GyroUpdateInterval; }
void RemoteSetGyroUpdateInterval(int idx, float interval) { sGenericRemote.m_GyroUpdateInterval = interval; }

// Screen size (Screen.width/.height)
float RemoteGetScreenWidth()			{ return sGenericRemote.GetReportedWidth(); }
float RemoteGetScreenHeight()			{ return sGenericRemote.GetReportedHeight(); }
