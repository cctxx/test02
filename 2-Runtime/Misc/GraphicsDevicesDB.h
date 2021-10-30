#pragma once

#define GRAPHICS_DEVICES_DB_AVAILABLE (UNITY_WIN || UNITY_OSX || UNITY_LINUX)

#if GRAPHICS_DEVICES_DB_AVAILABLE
int GetGraphicsPixelFillrate (int vendorID, int deviceID);
#else
inline int GetGraphicsPixelFillrate (int vendorID, int deviceID)
{
	#if (UNITY_XENON || UNITY_PS3)
		return 4000;
	#elif (UNITY_WII)
		return 972;
	#else
		return -1;
	#endif
}
#endif
