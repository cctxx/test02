#include "UnityPrefix.h"
#include "Tags.h"
#include "ManagerContext.h"
#include "GameManager.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/GUID.h"
#include "External/MurmurHash/MurmurHash2.h"
#include <vector>

static const char* kDefaultSortingLayerName = "Default";

// -------------------------------------------------------------------


static Object* GetTagManagerPtr ();


typedef std::pair<const std::string, UInt32> StringUInt32Pair;
typedef std::map<std::string, UInt32, std::less<std::string>, STL_ALLOCATOR(kMemPermanent, StringUInt32Pair) > StringToUnsigned;


static StringToUnsigned* gStringToTag;
static UnsignedToString* gTagToString;
static int* gTagManagerContainer;

static StringToUnsigned* gStringToMask;
static std::string gMaskToString[32];
static std::string gEmpty;

namespace LayerTagManager
{
	void StaticInitialize()
	{
		gTagManagerContainer = UNITY_NEW_AS_ROOT(int, kMemResource, "LayerTagManager", "");
		SET_ALLOC_OWNER(gTagManagerContainer);
		gStringToTag = UNITY_NEW(StringToUnsigned,kMemResource);
		gTagToString = UNITY_NEW(UnsignedToString,kMemResource);
		gStringToMask = UNITY_NEW(StringToUnsigned,kMemResource);
	}
	void StaticDestroy()
	{
		UNITY_DELETE(gStringToTag, kMemResource);
		UNITY_DELETE(gTagToString, kMemResource);
		UNITY_DELETE(gStringToMask, kMemResource);
		for(int i = 0; i < 32; i++)
			gMaskToString[i] = std::string();
		gEmpty = std::string();
		UNITY_DELETE(gTagManagerContainer, kMemResource);
	}
}

static RegisterRuntimeInitializeAndCleanup s_LayerTagManagerCallbacks(LayerTagManager::StaticInitialize, LayerTagManager::StaticDestroy);


static void RegisterTag (UInt32 tag, const std::string& name);


void RegisterTag (UInt32 tag, const std::string& name)
{
	SET_ALLOC_OWNER(gTagManagerContainer);
	if (!gStringToTag->insert (make_pair (name, tag)).second && !name.empty ())
		LogStringObject ("Default GameObject Tag: " + name + " already registered", GetTagManagerPtr ());

	if (!gTagToString->insert (make_pair (tag, name)).second)
		LogStringObject ("Default GameObject Tag for name: " + name + " already registered", GetTagManagerPtr ());
}

// In the editor we might add / remove tags from the gStringToTag array.
// And we might do this from the loading thread and main thread at the same time.

// In the player this map is completely fixed, thus it even if two threads access the data at the same time, the data always stays constant
#if UNITY_EDITOR

Mutex gTagToStringMutex;

UInt32 StringToTagAddIfUnavailable (const std::string& tag)
{
	Mutex::AutoLock lock(gTagToStringMutex);
	StringToUnsigned::iterator i = gStringToTag->find (tag);
	if (i == gStringToTag->end ())
	{
		SET_ALLOC_OWNER(gTagManagerContainer);
		int nextTagID = last_iterator(*gTagToString)->first + 1;
		gTagToString->insert(make_pair(nextTagID, tag));
		gStringToTag->insert(make_pair(tag, nextTagID));
		return nextTagID;
	}
	else
		return i->second;
}

#endif


UInt32 StringToTag (const std::string& tag)
{
	#if UNITY_EDITOR
	Mutex::AutoLock lock(gTagToStringMutex);
	#endif
	
	StringToUnsigned::iterator i = gStringToTag->find (tag);
	if (i == gStringToTag->end ())
		return -1;
	else
		return i->second;
}

const std::string& TagToString (UInt32 tag)
{
	#if UNITY_EDITOR
	Mutex::AutoLock lock(gTagToStringMutex);
	#endif
	UnsignedToString::iterator i = gTagToString->find (tag);
	if (i == gTagToString->end ())
	{
		static std::string empty;
		return empty;
	}
	else
		return i->second;
}

UInt32 StringToLayerMask (const std::string& tag)
{
	StringToUnsigned::iterator i = gStringToMask->find (tag);
	if (i == gStringToMask->end ())
		return 0;
	else
		return 1 << i->second;
}

UInt32 StringToLayer (const std::string& tag)
{
	StringToUnsigned::iterator i = gStringToMask->find (tag);
	if (i == gStringToMask->end ())
		return -1;
	else
		return i->second;
}

const std::string& LayerToString (UInt32 layer)
{
	if (layer >= 32)
	{
		ErrorString("Layer index out of bounds");
		return gEmpty;
	}
	return gMaskToString[layer];
}

const std::string& LayerMaskToString (UInt32 layerMask)
{
	Assert (IsPowerOfTwo (layerMask));
	if (layerMask == 0)
		return gEmpty;
	int layer = AnyBitFromMask (layerMask);
	return gMaskToString[layer];
}

void RegisterLayer (UInt32 tag, const std::string& name)
{
	SET_ALLOC_OWNER(gTagManagerContainer);
	if (!gStringToMask->insert (make_pair (name, tag)).second && !name.empty ())
		LogStringObject ("Default GameObject BitMask: " + name + " already registered", GetTagManagerPtr ());

	if (gMaskToString[tag].empty ())
		gMaskToString[tag] = name;
	else
		LogStringObject ("Default GameObject BitMask for name: " + name + " already registered", GetTagManagerPtr ());
}

UnsignedToString GetTags ()
{
	// Cull out all empty string tags. (The user is allowed to add those but we dont want them to show up!)
	UnsignedToString tags;
	for (UnsignedToString::iterator i=gTagToString->begin ();i != gTagToString->end ();i++)
	{
		if (!i->second.empty ())
			tags.insert (make_pair (i->first, i->second));
	}
	return tags;
}


// -------------------------------------------------------------------

struct SortingLayerEntry
{
	DECLARE_SERIALIZE_NO_PPTR (SortingLayerEntry)

	SortingLayerEntry() : userID(1), uniqueID(1), locked(false) { }
	UnityStr name;
	UInt32 userID;
	UInt32 uniqueID;
	bool locked;
};

template<class TransferFunc>
void SortingLayerEntry::Transfer (TransferFunc& transfer)
{
	TRANSFER (name);
	TRANSFER (userID);
	TRANSFER (uniqueID);
	TRANSFER_EDITOR_ONLY (locked);
	transfer.Align();
}

class TagManager : public GlobalGameManager
{
public:
	DECLARE_OBJECT_SERIALIZE (TagManager)
	REGISTER_DERIVED_CLASS (TagManager, GlobalGameManager)
	
	TagManager (MemLabelId label, ObjectCreationMode mode) : Super(label, mode), m_DefaultLayerIndex(0) {}
	
	bool ShouldIgnoreInGarbageDependencyTracking () { return true; }
	
	virtual void Update () {  }
	// virtual ~TagManager () { } declared-by-macro

	void AddDefaultLayerIfNeeded();
	void FindDefaultLayerIndex();

	std::vector<SortingLayerEntry> m_SortingLayers;
	int m_DefaultLayerIndex;
};

TagManager::~TagManager ()
{
}

void TagManager::FindDefaultLayerIndex()
{
	m_DefaultLayerIndex = 0;
	for (size_t i = 0, n = m_SortingLayers.size(); i != n; ++i)
	{
		if (m_SortingLayers[i].userID == 0)
		{
			m_DefaultLayerIndex = i;
			break;
		}
	}
}

void TagManager::AddDefaultLayerIfNeeded()
{
	// do we have a default layer?
	for (size_t i = 0, n = m_SortingLayers.size(); i != n; ++i)
	{
		if (m_SortingLayers[i].userID == 0)
			return;
	}

	// no default layer, add one in front
	SortingLayerEntry layer;
	layer.name = kDefaultSortingLayerName;
	layer.uniqueID = 0;
	layer.userID = 0;
	m_SortingLayers.insert(m_SortingLayers.begin(), layer);
	m_DefaultLayerIndex = 0;
}



void RegisterDefaultTagsAndLayerMasks ()
{
	SET_ALLOC_OWNER(gTagManagerContainer);
	gStringToTag->clear (); gTagToString->clear ();
	gStringToMask->clear ();
	for (int i=0;i<32;i++)
		gMaskToString[i].clear ();
	if (GetTagManagerPtr())
	{
		TagManager& tags = (TagManager&)GetTagManager();
		tags.m_SortingLayers.clear();
		// add "Default" sorting layer
		tags.m_SortingLayers.push_back(SortingLayerEntry());
		SortingLayerEntry& layer = tags.m_SortingLayers[0];
		layer.name = kDefaultSortingLayerName;
		layer.userID = 0;
		layer.uniqueID = 0;
		tags.m_DefaultLayerIndex = 0;
	}

	RegisterTag (kUntagged, "Untagged");
	RegisterTag (kRespawnTag, "Respawn");
	RegisterTag (kFinishTag, "Finish");
	RegisterTag (kEditorOnlyTag, "EditorOnly");
	RegisterTag (kMainCameraTag, "MainCamera");
	RegisterTag (kGameControllerTag, "GameController");
	RegisterTag (kPlayerTag, "Player");

	RegisterLayer (kDefaultLayer, "Default");
	RegisterLayer (kNoFXLayer, "TransparentFX");
	RegisterLayer (kIgnoreRaycastLayer, "Ignore Raycast");
	RegisterLayer (kWaterLayer, "Water");
}

template<class TransferFunction>
void TagManager::Transfer (TransferFunction& transfer)
{
	std::vector<UnityStr> tags;
	
	// Build tags array
	if (transfer.IsWriting ())
	{
		UnsignedToString::iterator begin = gTagToString->lower_bound (kFirstUserTag);
		UnsignedToString::iterator end = gTagToString->upper_bound (kLastUserTag);
		for (UnsignedToString::iterator i=begin;i != end;i++)
			tags.push_back (i->second);
		if (tags.empty () || !tags.back ().empty ())
			tags.push_back ("");
	}
	else if (transfer.IsReading ())
	{
		RegisterDefaultTagsAndLayerMasks ();
	}
	
	TRANSFER_SIMPLE (tags);
	
	// Register tags we've read (if there actually was tag data in the stream).
	if (transfer.DidReadLastProperty ())
	{
		for (int i=0;i<tags.size ();i++)
			RegisterTag (kFirstUserTag + i, tags[i]);
	}
	
	// Build bitnames array
	UnityStr bitnames[32];
	for (int i=0;i<32;i++)
	{
		char name[64];
		bool editable = i >= kUserLayer;
		if (editable)
			sprintf (name, "User Layer %d", i);
		else
			sprintf (name, "Builtin Layer %d", i);

		bitnames[i] = LayerToString (i);
		transfer.Transfer (bitnames[i], name, editable ? kNoTransferFlags : kNotEditableMask);
	
		if (transfer.DidReadLastProperty ())
		{
			if (i >= kUserLayer)
				RegisterLayer (i, bitnames[i]);
		}
	}

	// Sorting layers
	TRANSFER (m_SortingLayers);
	if (!transfer.IsWriting () && transfer.IsReading())
	{
		AddDefaultLayerIfNeeded();
		FindDefaultLayerIndex();
	}
}

IMPLEMENT_CLASS (TagManager)
IMPLEMENT_OBJECT_SERIALIZE (TagManager)

Object& GetTagManager ()
{
	return GetManagerFromContext (ManagerContext::kTagManager);
}

static Object* GetTagManagerPtr ()
{
	return GetManagerPtrFromContext (ManagerContext::kTagManager);
}



// -------------------------------------------------------------------



UnityStr GetSortingLayerName(int index)
{
	TagManager& tags = (TagManager&)GetTagManager();
	if (index < 0 || index >= tags.m_SortingLayers.size())
		return UnityStr();
	return tags.m_SortingLayers[index].name;
}

UnityStr GetSortingLayerNameFromValue(int layerValue)
{
	TagManager& tags = (TagManager&)GetTagManager();
	int index = tags.m_DefaultLayerIndex + layerValue;
	if (index < 0 || index >= tags.m_SortingLayers.size())
		return UnityStr();
	return tags.m_SortingLayers[index].name;
}


int GetSortingLayerUserID(int index)
{
	TagManager& tags = (TagManager&)GetTagManager();
	if (index < 0 || index >= tags.m_SortingLayers.size())
		return 0;
	return tags.m_SortingLayers[index].userID;
}

int GetSortingLayerUniqueIDFromValue(int layerValue)
{
	TagManager& tags = (TagManager&)GetTagManager();
	int index = tags.m_DefaultLayerIndex + layerValue;
	if (index < 0 || index >= tags.m_SortingLayers.size())
		return 0;
	return tags.m_SortingLayers[index].uniqueID;
}


int GetSortingLayerUserIDFromValue(int layerValue)
{
	TagManager& tags = (TagManager&)GetTagManager();
	int index = tags.m_DefaultLayerIndex + layerValue;
	if (index < 0 || index >= tags.m_SortingLayers.size())
		return 0;
	return tags.m_SortingLayers[index].userID;
}

int GetSortingLayerIndexFromValue(int layerValue)
{
	TagManager& tags = (TagManager&)GetTagManager();
	int index = tags.m_DefaultLayerIndex + layerValue;
	if (index < 0 || index >= tags.m_SortingLayers.size())
		index = 0;
	return index;
}



int GetSortingLayerUniqueID(int index)
{
	TagManager& tags = (TagManager&)GetTagManager();
	Assert (index >= 0 && index < tags.m_SortingLayers.size());
	return tags.m_SortingLayers[index].uniqueID;
}

UnityStr GetSortingLayerNameFromUniqueID(int id)
{
	if (id == 0)
		return UnityStr(kDefaultSortingLayerName);

	TagManager& tags = (TagManager&)GetTagManager();
	for (size_t i = 0; i < tags.m_SortingLayers.size(); ++i)
		if (tags.m_SortingLayers[i].uniqueID == id)
			return tags.m_SortingLayers[i].name;

	return "<unknown layer>";
}

int GetSortingLayerValueFromUniqueID(int id)
{
	if (id == 0)
		return 0;

	TagManager& tags = (TagManager&)GetTagManager();
	for (size_t i = 0; i < tags.m_SortingLayers.size(); ++i)
		if (tags.m_SortingLayers[i].uniqueID == id)
			return i - tags.m_DefaultLayerIndex;

	return 0; // unknown layer: treat as if no layer is assigned
}

int GetSortingLayerValueFromUserID(int id)
{
	if (id == 0)
		return 0;

	TagManager& tags = (TagManager&)GetTagManager();
	for (size_t i = 0; i < tags.m_SortingLayers.size(); ++i)
		if (tags.m_SortingLayers[i].userID == id)
			return i - tags.m_DefaultLayerIndex;

	return 0; // unknown layer: treat as if no layer is assigned
}

int GetSortingLayerValueFromName(const UnityStr& name)
{
	TagManager& tags = (TagManager&)GetTagManager();
	if (name.empty())
		return 0;

	for (size_t i = 0; i < tags.m_SortingLayers.size(); ++i)
		if (tags.m_SortingLayers[i].name == name)
			return i - tags.m_DefaultLayerIndex;

	return 0; // unknown layer: treat as default layer
}


#if UNITY_EDITOR
void AddSortingLayer()
{
	TagManager& tags = (TagManager&)GetTagManager();
	SortingLayerEntry s;

	// internal ID should be quite unique; generate a GUID and hash to and integer
	UnityGUID guid;
	guid.Init();
	s.uniqueID = MurmurHash2A(&guid.data, sizeof(guid.data), 0x8f37154b);
	s.uniqueID |= 1; // make sure it's never zero

	// user-visible ID: smallest unused one
	int id = 1;
	while (true)
	{
		bool gotIt = false;
		for (size_t i = 0; i < tags.m_SortingLayers.size(); ++i)
		{
			if (tags.m_SortingLayers[i].userID == id)
			{
				gotIt = true;
				break;
			}
		}
		if (!gotIt)
			break;
		++id;
	}

	s.name = Format("New Layer %d", id);
	s.userID = id;
	s.locked = false;

	tags.m_SortingLayers.push_back (s);
	tags.SetDirty();
}


void UpdateSortingLayersOrder()
{
	TagManager& tags = (TagManager&)GetTagManager();
	tags.FindDefaultLayerIndex();

	vector<SInt32> objs;
	Object::FindAllDerivedObjects (ClassID (Renderer), &objs);
	for (size_t i = 0, n = objs.size(); i != n; ++i)
	{
		Renderer* r = PPtr<Renderer> (objs[i]);
		r->SetupSortingOverride();
	}
}


void SetSortingLayerName(int index, const std::string& name)
{
	TagManager& tags = (TagManager&)GetTagManager();
	Assert (index >= 0 && index < tags.m_SortingLayers.size());
	tags.m_SortingLayers[index].name = name;
	tags.SetDirty();
}

void SwapSortingLayers(int idx1, int idx2)
{
	TagManager& tags = (TagManager&)GetTagManager();
	Assert (idx1 >= 0 && idx1 < tags.m_SortingLayers.size());
	Assert (idx2 >= 0 && idx2 < tags.m_SortingLayers.size());
	std::swap(tags.m_SortingLayers[idx1], tags.m_SortingLayers[idx2]);
	UpdateSortingLayersOrder();
	tags.SetDirty();
}


void SetSortingLayerLocked(int index, bool locked)
{
	TagManager& tags = (TagManager&)GetTagManager();
	Assert (index >= 0 && index < tags.m_SortingLayers.size());
	tags.m_SortingLayers[index].locked = locked;
	tags.SetDirty();
}

bool GetSortingLayerLocked(int index)
{
	TagManager& tags = (TagManager&)GetTagManager();
	if (index < 0 || index >= tags.m_SortingLayers.size())
		return false;
	return tags.m_SortingLayers[index].locked;
}

bool IsSortingLayerDefault(int index)
{
	TagManager& tags = (TagManager&)GetTagManager();
	return index == tags.m_DefaultLayerIndex;
}


int g_LockedPickingLayers = 0;

#endif // #if UNITY_EDITOR


int GetSortingLayerCount()
{
	TagManager& tags = (TagManager&)GetTagManager();
	return tags.m_SortingLayers.size();
}
