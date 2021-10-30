#ifndef SERIALIZEDOBJECTCACHE_H
#define SERIALIZEDOBJECTCACHE_H

#include "SerializedProperty.h"

class SerializedObjectCache 
{
public:
	static void SaveToCache (SerializedObject* obj, int instanceID);
	static SerializedObject *LoadFromCache (int instanceID);
};

#endif
