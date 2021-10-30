#ifndef _MANAGERCONTEXT_LOADING_H_
#define _MANAGERCONTEXT_LOADING_H_

#include "Runtime/Utilities/dynamic_array.h"

typedef dynamic_array<int> InstanceIDArray;
class AwakeFromLoadQueue;

void CollectLevelGameManagers (InstanceIDArray& outputObjects);
void DestroyLevelManagers ();
void RemoveDuplicateGameManagers ();
std::string PlayerLoadSettingsAndInput(const std::string& dataFile);
std::string PlayerLoadGlobalManagers (const char* dataFile);
std::string ResetManagerContextFromLoaded ();
void LoadManagers (AwakeFromLoadQueue& awakeFromLoadQueue);

#endif