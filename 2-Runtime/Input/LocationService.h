#ifndef UNITY_LOCATION_SERVICE_H_
#define UNITY_LOCATION_SERVICE_H_

#include "Runtime/Math/Vector3.h"

struct LocationInfo
{
	double timestamp;
	float latitude;
	float longitude;
	float altitude;
	float horizontalAccuracy;
	float verticalAccuracy;
};

struct HeadingInfo
{
	float magneticHeading;
	float trueHeading;
	Vector3f raw;
	double timestamp;
};

enum LocationServiceStatus
{
	kLocationServiceStopped,
	kLocationServiceInitializing,
	kLocationServiceRunning,
	kLocationServiceFailed
};

class LocationService
{
public:
	static void SetDesiredAccuracy (float val);
	static float GetDesiredAccuracy ();
	static void SetDistanceFilter (float val);
	static float GetDistanceFilter ();
	static bool IsServiceEnabledByUser ();
	static void StartUpdatingLocation ();
	static void StopUpdatingLocation ();
	static void SetHeadingUpdatesEnabled (bool enabled);
	static bool IsHeadingUpdatesEnabled();
	static LocationServiceStatus GetLocationStatus ();
	static LocationServiceStatus GetHeadingStatus ();
	static LocationInfo GetLastLocation ();
	static const HeadingInfo &GetLastHeading ();
	static bool IsHeadingAvailable ();
};

#endif    // #ifndef UNITY_LOCATION_SERVICE_H_
