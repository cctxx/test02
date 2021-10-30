#ifndef SAVEANDLOADHELPER_H
#define SAVEANDLOADHELPER_H

#include <map>
#include <string>
#include <set>
#include <vector>
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Misc/GarbageCollectSharedAssets.h"

class AssetBundle;
class AwakeFromLoadQueue;

typedef dynamic_array<int> InstanceIDArray;

void DestroyWorld (bool destroySceneAssets);

bool InitializeEngineNoGraphics ();
bool InitializeEngineGraphics (bool batch = false);
void CleanupEngine ();
void CleanupAllObjects (bool reloadable);

void CreateWorldEditor ();
void CleanupAfterLoad ();

/// Loads a new world from a pathname, playmode or editmode
/// Returns true if any datatemplates were merged while loading.
bool LoadSceneEditor (const std::string& pathName, std::map<LocalIdentifierInFileType, SInt32>* hintFileIDToHeapID, int mode);

void PostLoadLevelAdditive (const std::string& pathName, AwakeFromLoadQueue& awakeQueue);
void PostEditorLoadLevelAdditive (const std::string& pathName, AwakeFromLoadQueue& awakeQueue);
void CompletePreloadManagerLoadLevel (const std::string& path, AwakeFromLoadQueue& awakeQueue);
void CompletePreloadMainData (AwakeFromLoadQueue& awakeQueue);
void CompletePreloadManagerLoadLevelEditor (const std::string& path, AwakeFromLoadQueue& awakeQueue, int preloadOperationMode);

void LoadLevelAdditiveEditor (const std::string& level);

void CollectSceneGameObjects (InstanceIDArray& instanceIDs);

void CheckAllGOConsistency ();

void PostprocessScene ();

void UnloadAssetBundle (AssetBundle& file, bool unloadAllLoadedObjects);

void VerifyNothingIsPersistentInLoadedScene (const std::string& pathName);

/// Make there are any game objects and/or level managers still present, print an error message.
///
/// @param includeAllEditorExtensions If true, not only check for GameObjects but also for
///		any object derived from EditorExtension.
void ValidateNoSceneObjectsAreLoaded (bool includeAllEditorExtensions = false);

extern const char* kMainData;


#endif
