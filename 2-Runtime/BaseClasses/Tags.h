#pragma once

#include <map>
#include <string>
#include "Runtime/Misc/Allocator.h"

class Object;

enum BitMasks
{
	// Can't modify these without breaking backwards compatibility!
	kDefaultLayer = 0,
	kNoFXLayer = 1,
	kIgnoreRaycastLayer = 2,
	kIgnoreCollisionLayer = 3,
	kWaterLayer = 4,
	kNumLayers = 32,
	
	kDefaultLayerMask = 1 << kDefaultLayer,
	kNoFXLayerMask = 1 << kNoFXLayer,
	kIgnoreRaycastMask = 1 << kIgnoreRaycastLayer,
	kIgnoreCollisionMask = 1 << kIgnoreCollisionLayer,
	kPreUnity2UnusedLayer = 1 << 5,
	
	kUserLayer = 8,
};

enum Tags
{
	kUntagged = 0,
	kRespawnTag = 1,
	kFinishTag = 2,
	kEditorOnlyTag = 3,
	kMainCameraTag = 5,
	kPlayerTag = 6,
	kGameControllerTag = 7,
	kFirstUserTag = 20000,
	kLastUserTag = 30000,
	kUndefinedTag = -1
};

// converts tag to string
UInt32 StringToTag (const std::string& tag); 
UInt32 StringToTagAddIfUnavailable (const std::string& name);
const std::string& TagToString (UInt32 tag);

// Converts between layer [0..31] and string
UInt32 StringToLayerMask (const std::string& layerName); 
const std::string& LayerToString (UInt32 layer);
UInt32 StringToLayer (const std::string& layer);
// Converts a layer mask (1 << [0..31]) to a string
const std::string& LayerMaskToString (UInt32 mask);

void RegisterLayer (UInt32 layer, const std::string& name);

typedef std::pair<const UInt32, std::string> UInt32StringPair;
typedef std::map<UInt32, std::string, std::less<UInt32>, STL_ALLOCATOR(kMemPermanent, UInt32StringPair) > UnsignedToString;
UnsignedToString GetTags ();

void RegisterDefaultTagsAndLayerMasks ();

Object& GetTagManager ();


// -------------------------------------------------------------------
// Global sorting layers:
//
// Defined globally, and can be reordered in the inspector. The drawing order is as shown in the inspector.
// Internally each global sorting layer has "unique ID" (GUID hashed into an int), and in-editor Renderers that want to
// use them refer to the layer by this ID.
//
// @TODO:
// * Do we need this "user friendly ID"?

int GetSortingLayerCount();
UnityStr GetSortingLayerName(int index);
UnityStr GetSortingLayerNameFromUniqueID(int id);
int GetSortingLayerUniqueID(int index);
int GetSortingLayerUserID(int index);

UnityStr GetSortingLayerNameFromValue(int layerValue);
int GetSortingLayerUserIDFromValue(int layerValue);
int GetSortingLayerUniqueIDFromValue(int layerValue);
int GetSortingLayerIndexFromValue(int layerValue);

// these return final sorting layer values
// (i.e. zero is always "default" - the returned value can be negative or positive)
int GetSortingLayerValueFromUniqueID(int id);
int GetSortingLayerValueFromUserID(int id);
int GetSortingLayerValueFromName(const UnityStr& name);


#if UNITY_EDITOR
void SetSortingLayerName(int index, const std::string& name);
void AddSortingLayer();
void UpdateSortingLayersOrder();
void SetSortingLayerLocked(int index, bool locked);
bool GetSortingLayerLocked(int index);
bool IsSortingLayerDefault(int index);
void SwapSortingLayers(int idx1, int idx2);

extern int g_LockedPickingLayers;

#endif // #if UNITY_EDITOR
