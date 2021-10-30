#include "SerializedObjectCache.h"

static int s_CachedInstanceID = 0;
static SerializedObject *s_CachedSerializedObject = NULL;

void SerializedObjectCache::SaveToCache (SerializedObject* obj, int instanceID)
{
	if (s_CachedSerializedObject != NULL)
		delete s_CachedSerializedObject;
	
	s_CachedSerializedObject = obj;
	s_CachedInstanceID = instanceID;
}

SerializedObject* SerializedObjectCache::LoadFromCache (int instanceID)
{
	SerializedObject *result = NULL;
	if (instanceID == s_CachedInstanceID)
		result = s_CachedSerializedObject;
	else if (s_CachedSerializedObject != NULL)
		delete s_CachedSerializedObject;
	s_CachedSerializedObject = NULL;
	s_CachedInstanceID = 0;
	return result;
}
