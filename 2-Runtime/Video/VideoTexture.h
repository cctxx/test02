#ifndef LIVE_VIDEO_TEXTURE
#define LIVE_VIDEO_TEXTURE

#if ENABLE_WEBCAM

#include "BaseVideoTexture.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Scripting/ScriptingUtility.h"

#if UNITY_WINRT
#include <windows.foundation.h>
#endif

struct PlatformDependentWebCamTextureData;

enum WebCamFlags
{
	kWebCamFrontFacing = 1,
};

struct MonoWebCamDevice
{
	ScriptingStringPtr name;
	int flags;

	bool operator== (std::string const &other) const
	{
		std::string cppStr = scripting_cpp_string_for (name);
		return cppStr == other;
	}
};

typedef UNITY_VECTOR(kMemWebCam, MonoWebCamDevice) MonoWebCamDevices;
typedef MonoWebCamDevices::iterator MonoWebCamDevicesIter;

class WebCamTexture: public BaseVideoTexture
{
public:
	REGISTER_DERIVED_CLASS (WebCamTexture, Texture)

	WebCamTexture (MemLabelId label, ObjectCreationMode mode = kCreateObjectDefault)
	:	BaseVideoTexture(label, mode)
	{
		m_RequestedFPS = 0.0f;
		m_RequestedWidth = 0;
		m_RequestedHeight = 0;
		m_IsCreated = false;
		m_VT = NULL;

#if UNITY_WINRT
		RunExactlyOnce();
#endif
	}

	static void InitializeClass ();
	static void CleanupClass ();

	virtual void Play();
	virtual void Pause();
	virtual void Stop ();
	virtual void Update ();

#if UNITY_WP8
	virtual void Suspend();
	virtual void Resume();
#endif

	void SetRequestedWidth (int width) { m_RequestedWidth = width; SetDirty(); }
	int GetRequestedWidth () const { return m_RequestedWidth; }

	void SetRequestedHeight (int width) { m_RequestedHeight = width; SetDirty(); }
	int GetRequestedHeight () const { return m_RequestedHeight; }

	void SetRequestedFPS (float fps) { m_RequestedFPS = fps; SetDirty(); }
	float GetRequestedFPS () const { return m_RequestedFPS; }

	void SetDevice (const std::string &name) { m_DeviceName = name; SetDirty(); }
	std::string GetDevice() const;

	static void GetDeviceNames (MonoWebCamDevices &devices, bool forceUpdate);

	#if ENABLE_PROFILER || UNITY_EDITOR
	virtual int GetStorageMemorySize() const { return 0; }
	#endif

	ColorRGBAf GetPixel (int x, int y) const;
	bool GetPixels (int x, int y, int width, int height, ColorRGBAf* data) const;
	bool GetPixels (int dstFormat, void *dstData, size_t dstSize) const;

#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BLACKBERRY || UNITY_TIZEN
    virtual int GetVideoRotationAngle() const;
#endif
#if UNITY_IPHONE || UNITY_BLACKBERRY
	virtual bool IsVideoVerticallyMirrored() const;
#endif

private:
#if UNITY_WINRT
	// C-tor helper. It is here to ensure that some invariants are satisfied from the moment
	// the object is created, so we don't have to check and recheck the state all the time
	void RunExactlyOnce();
#endif

	void Create();
	void Cleanup ();

public:
	static void EnsureUniqueName (MonoWebCamDevice &device,
	                              const MonoWebCamDevices &devs);
private:
	int m_RequestedWidth;
	int m_RequestedHeight;
	float m_RequestedFPS;
	std::string m_DeviceName;
	bool m_IsCreated;

	static void InitDeviceList();
	int GetDeviceIdFromDeviceList(const std::string& name) const;

	PlatformDependentWebCamTextureData *m_VT;

protected:
#if UNITY_IPHONE
	virtual TextureFormat	GetBufferTextureFormat() const { return kTexFormatBGRA32; }
	virtual bool			CanSetReadable(bool readable) const;
#endif

#if UNITY_BLACKBERRY || UNITY_TIZEN || UNITY_WP8
	virtual TextureFormat	GetBufferTextureFormat() const { return kTexFormatBGRA32; }
#endif
};

inline ColorRGBAf WebCamTexture::GetPixel (int x, int y) const
{
	if (!m_IsCreated)
	{
		ErrorString ("Cannot get pixels when webcam is not running");
		return ColorRGBAf(0,0,0,0);
	}
	if (!IsReadable())
	{
		ErrorString ("Cannot get pixels when webcam is non-readable");
		return ColorRGBAf(0,0,0,0);
	}

	return GetImagePixel ((UInt8*)GetImageBuffer(), GetPaddedWidth(), GetPaddedHeight(), GetBufferTextureFormat(), static_cast<TextureWrapMode>(GetSettings().m_WrapMode), x, y);
}

inline bool WebCamTexture::GetPixels( int x, int y, int width, int height, ColorRGBAf* colors ) const
{
	if (width == 0 || height == 0)
		return true; // nothing to do

	if (!m_IsCreated)
	{
		ErrorString ("Cannot get pixels when webcam is not running");
		return false;
	}
	if (!IsReadable())
	{
		ErrorString ("Cannot get pixels when webcam is non-readable");
		return false;
	}

	return GetImagePixelBlock ((UInt8*)GetImageBuffer(), GetPaddedWidth(), GetPaddedHeight(), GetBufferTextureFormat(), x, y, width, height, colors);
}

inline bool WebCamTexture::GetPixels (int dstFormat, void *dstData, size_t dstSize) const
{
	size_t srcRowBytes = GetRowBytesFromWidthAndFormat(GetPaddedWidth(), GetBufferTextureFormat());
	size_t dstRowBytes = GetRowBytesFromWidthAndFormat(GetDataWidth(), dstFormat);
	if (dstSize < dstRowBytes * GetDataHeight())
	{
		ErrorString ("Buffer is too small to get image data");
		return false;
	}

	ImageReference src (GetDataWidth(), GetDataHeight(), srcRowBytes, GetBufferTextureFormat(), (UInt8*)GetImageBuffer());
	ImageReference dst (GetDataWidth(), GetDataHeight(), dstRowBytes, dstFormat, dstData);
	dst.BlitImage( src );
	return true;
}

inline int WebCamTexture::GetDeviceIdFromDeviceList(const std::string& name) const
{
	MonoWebCamDevices names;
	GetDeviceNames(names, false);
	if(!name.empty())
	{
		for(int i = 0 ; i < names.size() ; i++)
		{
			if(names[i] == name)
				return i;
		}
		ErrorString ("Cannot find webcam device "+name+".");
		return -1;
	}
	else
	{
		// Return camera 0 as default
		if(!names.empty())
			return 0;
		else
		{
			ErrorString ("No available webcams are found. Either there is no webcam connected, or they are all in use by other applications (like Skype).");
			return -1;
		}
	}
}

inline std::string WebCamTexture::GetDevice() const
{
	if(m_DeviceName.size() > 0)
	{
		return m_DeviceName;
	}
	else
	{
		MonoWebCamDevices names;
		GetDeviceNames(names, false);

		if(names.size() > 0)
			return scripting_cpp_string_for(names[0].name);
		else
			return "no camera available.";
	}
}

inline void WebCamTexture::EnsureUniqueName (MonoWebCamDevice &device,
                                             MonoWebCamDevices const &devs)
{
	int num = 0;
	std::string testname = scripting_cpp_string_for (device.name);

	while (true)
	{
		if (num > 0)
			testname += Format (" %d", num);

		if (std::find (devs.begin (), devs.end (), testname) == devs.end ())
		{
			device.name = scripting_string_new(testname.c_str ());
			break;
		}

		num++;
	}
}

#endif
#endif
