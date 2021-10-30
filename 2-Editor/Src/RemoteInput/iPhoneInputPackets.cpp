#include "Editor/Src/RemoteInput/iPhoneInputPackets.h"
#include <memory>

namespace iphone {
	
	namespace {
		template <typename T>
		size_t read(UInt8 const*& buffer, T& t) {
			memcpy(&t, reinterpret_cast<T const*>(buffer), sizeof(T));
			buffer += sizeof(T);
			return sizeof(T);
		}
		template <typename T>
		size_t write(UInt8*& buffer, T const& t) {
			memcpy(buffer, &t, sizeof(T));
			buffer += sizeof(T);
			return sizeof(T);
		}	
	}
	
	namespace packet
	{
		enum { Version = 0x05 };
		
		struct Accelerometer
		{
			float x, y, z;
		};
		
		struct Touch
		{
			UInt32 id;
			float posX, posY;
			float deltaPosX, deltaPosY;
			float deltaTime;
			UInt32 tapCount;
			UInt32 phase;
		};
		
		struct TouchPoint
		{
			UInt32 id;
			float posX, posY;
			UInt32 tapCount;
		};
		
		struct Header
		{
			Accelerometer accelerometer;
			UInt32 touchCount;
			UInt16 orientation;
		};
	}
	
	namespace {
		// Touch
		packet::Touch& marshall(packet::Touch& d, Touch const& s)
		{
			d.id = s.id;
			d.posX = s.pos.x; d.posY = s.pos.y;
			d.deltaPosX = s.deltaPos.x; d.deltaPosY = s.deltaPos.y;
			d.deltaTime = s.deltaTime;
			d.tapCount = s.tapCount;
			d.phase = s.phase;
			return d;
		}
		void unmarshall(Touch& d, packet::Touch const& s)
		{
			d.id = s.id;
			d.pos.x = s.posX; d.pos.y = s.posY;
			d.deltaPos.x = s.deltaPosX; d.deltaPos.y = s.deltaPosY;
			d.deltaTime = s.deltaTime;
			d.tapCount = s.tapCount;
			d.phase = s.phase;
		}
		
		// Accelerometer
		packet::Accelerometer& marshall(packet::Accelerometer& d, Vector3f const& s)
		{
			d.x = s.x; d.y = s.y; d.z = s.z;
			return d;
		}
		void unmarshall(Vector3f& d, packet::Accelerometer const& s)
		{
			d.x = s.x; d.y = s.y; d.z = s.z;
		}
		
		// Header
		packet::Header& marshall(packet::Header& d, InputPacket const& s)
		{
			marshall(d.accelerometer, s.accelerator);
			d.touchCount = s.touchCount;
			d.orientation = s.orientation;
			return d;
		}
		void unmarshall(InputPacket& d, packet::Header const& s)
		{
			unmarshall(d.accelerator, s.accelerometer);
			d.touchCount = s.touchCount;
			d.orientation = s.orientation;
		}
	}
	
	bool checkVersion(void const* buffer, UInt32 version)
	{
		UInt32 srcVersion = *reinterpret_cast<UInt32 const*> (buffer);
		Assert(srcVersion == version);
		return (srcVersion == version);
	}
	
	size_t read(void const* buffer, InputPacket& in)
	{
		if (!checkVersion(buffer, packet::Version))
			return 0;
		
		size_t bytesRead = 0;
		UInt8 const* data = reinterpret_cast<UInt8 const*> (buffer);
		data += sizeof(UInt32); // skip version data
		
		packet::Header header;
		bytesRead += read(data, header);
		unmarshall(in, header);
		
		Assert(in.touchCount <= InputPacket::MaxTouchCount);
		Assert(in.touchPointCount <= InputPacket::MaxTouchPointCount);
		memset(in.touches, 0, sizeof(Touch) * InputPacket::MaxTouchCount);
		
		for (size_t q = 0; q < in.touchCount; ++q)
		{
			packet::Touch tmp;
			bytesRead += read(data, tmp);		
			if (q >= InputPacket::MaxTouchCount) continue;
			
			unmarshall(in.touches[q], tmp);
		}
		return bytesRead;
	}
	
	size_t write(void* buffer, InputPacket const& out)
	{
		UInt8* data = reinterpret_cast<UInt8*> (buffer);
		
		size_t a = 0;
		a += write(data, (int)packet::Version);
		
		packet::Header tmpHeader;
		a += write(data, marshall(tmpHeader, out));
		
		packet::Touch tmpTouch;
		for (size_t q = 0; q < out.touchCount; ++q)
			a += write(data, marshall(tmpTouch, out.touches[q]));
		
		return a;
	}
	
	size_t read(void const* buffer, ImagePacket& in)
	{
		UInt8 const* data = reinterpret_cast<UInt8 const*> (buffer);
		return read(data, in);
	}
	
	size_t write(void* buffer, ImagePacket const& out)
	{
		UInt8* data = reinterpret_cast<UInt8*> (buffer);
		return write(data, out);
	}
	
}
