#ifndef UNITY_IPHONE_INPUT_PACKETS_
#define UNITY_IPHONE_INPUT_PACKETS_

#include "Runtime/Input/GetInput.h"

namespace iphone
{
	struct InputPacket
	{
		enum
        {
            MaxTouchCount = 16,
            MaxTouchPointCount = 16
        };
		
		InputPacket() :
        accelerator(0, 0, 0),
        touchCount(0),
        touchPointCount(0)
        {}

		Vector3f accelerator;
		uint touchCount;
		uint touchPointCount : 16;
		uint orientation : 16;
		
		Touch touches[MaxTouchCount];
	};
    
	struct ImagePacket
	{
		uint srcWidth;
        uint srcHeight;
		uint x, y;
		uint compressedSize;
	};
    
	size_t read(void const* buffer, InputPacket& in);
	size_t write(void* buffer, InputPacket const& out);
    
	size_t read(void const* buffer, ImagePacket& in);
	size_t write(void* buffer, ImagePacket const& out);
}

#endif
