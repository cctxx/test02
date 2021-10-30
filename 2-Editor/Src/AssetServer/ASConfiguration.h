#ifndef __AS_CONFIGURATION_H
#define __AS_CONFIGURATION_H

// Handling of changesets and project configuration.
// Used to generate project status in reference to a project on server.

#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "External/sqlite/SQLite.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace AssetServer {

struct ChangesetDummy;
struct UploadItem;

enum Status { 
	kCalculating = -1,							// pseudostatus: currently unknown
	kClientOnly = 0,							// exists only on client [c]
	kServerOnly = 1,							// exists only on server [s]
	kUnchanged = 2,								// no new versions, same data on client and server [=]
	kConflict = 3,								// client changed its data, and server has a new version (conflict case!) [~]
	kSame = 4,									// new version on server, but client has the same data as it [&]
	kNewVersionAvailable = 5,					// client didn't change its data, and server has a new verison [<]
	kNewLocalVersion = 6,						// client changed its data, the server didn't [>]
	kRestoredFromTrash = 7,						// pseudostatus: set when an asset has been restored from the trash. Will behave as kNewLocalVersion,
												// but is shown in the server view as if it was kClientOnly
	kIgnored = 8,								// set on assets that have been excluded from versioning
	kBadState = 9								// pseudostatus: connection error (new version of server protocol perhaps?)
};

	
// Returned by Item::GetChangeFlags()
// Note that the values are ORed together as there might have been more than one change.
enum ChangeFlags {
	kCFNone = 0,
	kCFModified = 1,
	kCFRenamed = 2,
	kCFMoved = 4,
	kCFDeleted = 8,
	kCFUndeleted = 16,
	kCFCreated = 32
};

enum HierarchyStatus {
	kChangeNone = 0,
	kChangeClient,
	kChangeServer,
	kChangeBoth,
	kChangeConflict
};


// Where does the Item origin from? Server or client
enum Origin {
	kOrUnknown	= 0,
	kOrServer	= 1,
	kOrClient	= 2
};

// Type of asset. Dir or file. The  constants match values used on the server
enum ItemType {
	kTUnknown	= 0,	// Only used by the empty/invalid Item
	kTDir		= 7000,
	kTText		= 7001,
	kTBinary	= 7002	// Currently not used
};

// Item stores information on an asset affected by the changeset
class Item {
  public:
	int changeset;		// the internal asset version of the item
	UnityGUID guid;		// the guid of the asset
	UnityStr name;		// the name of the asset
	UnityGUID parent;	// parent directory
	ItemType type;		// the asset type. Used to ignore the digest on directory assets
	MdFour digest;		// content digest
	Origin origin;		// does this item represent a server or a client item -- not used when comparing items
	int oldVersion;		// used for backwards compatibility with projects checked out with Unity 2.0.x
	int parentFolderID;	// used for parent folder caching
	ChangeFlags changeFlags; // used only for change flag caching (set manually)
	
	// Construct an item from the parts
	Item(int iChangeset, const UnityGUID& iGuid, const string& iName, const UnityGUID& iParent, const ItemType iType, const MdFour& iDigest, Origin iOrigin, int iOldVersion=-1, int iParentFolderID=-1)
		:	changeset(iChangeset)
		,	guid(iGuid)
		,	name(iName)
		,	parent(iParent)
		,	type(iType)
		,	digest(iType == kTDir?MdFour():iDigest)
		,	origin(iOrigin)
		,	oldVersion(iOldVersion)
		,	parentFolderID(iParentFolderID)
	{}
	
	// Copy constructor
	Item(const Item& i)
		:	changeset(i.changeset)
		,	guid(i.guid)
		,	name(i.name)
		,	parent(i.parent)
		,	type(i.type)
		,	digest(i.digest)
		,	origin(i.origin)
		,	oldVersion(i.oldVersion)
		,	parentFolderID(i.parentFolderID)
		,	changeFlags(i.changeFlags)
	{}

	// Default constructor. Creates the empty Item, returned by search functions when no assets are found.
	Item()
		:	changeset(-1)
		,	guid()
		,	name()
		,	parent()
		,	type(kTUnknown)
		,	digest()
		,	origin(kOrUnknown)
		,	oldVersion(-1)
		,	parentFolderID(-1)
		,	changeFlags(kCFNone)
	{}

	~Item() {}
	
	// Compares the data in the current item with the one in the work area and returns the difference
	Status GetParentStatus();
	Status GetNameStatus();
	Status GetDigestStatus();
	Status GetStatus();  // Returns the combination of the above 3 statuses
	
	ChangeFlags GetChangeFlags(); // Use this for a summary of changes on an Item in a changeset

	bool IsMoved();
	bool IsDeleted() const;

	bool operator==(const Item &other) const {
		return changeset == other.changeset && SameContent(other);
	}
	
	bool SameContent(const Item &other) const {
		return guid == other.guid && name == other.name && parent == other.parent && 
		type == other.type && ( type <= kTDir || digest == other.digest );
	}

	
	bool operator!=(const Item &other) const {
		return !(*this == other);
	}
	
	// The less than operator is required by set and map when using Items as keys
	bool operator < (const Item &other) const { 
		return changeset < other.changeset || 
			(changeset == other.changeset && guid < other.guid || 
				( guid == other.guid && name < other.name || 
					( name == other.name && parent < other.parent || 
						parent == other.parent && type < other.type || 
							( type == other.type && type != kTDir && digest < other.digest ) )));
	}

	operator bool () const {
		return guid != UnityGUID() ;
	}

	DECLARE_SERIALIZE(Item);
};

// A changeset is a list of modified assets plus metadata
struct Changeset {
	int	changeset;			// The changeset id on server or -1 for a changeset that has not yet been committed
	string message;			// The commit message
	int date;				// The timestamp of the commit.
	string owner;			// The user who performed the commit.

	bool GetItems(vector<Item>* result) const;	// Returns the list of modified assets
	string GetDateString() const;
	
};

struct DeletedAsset {
	int changeset;
	UnityGUID guid;
	UnityGUID parent;
	string name;
	int parentFolderID;
	int date;
	ItemType type;

	DeletedAsset() : 
		changeset(-1), guid(UnityGUID()), parent(UnityGUID(0,1,0,0)), date(0), name("") {}
		
	bool operator < (const DeletedAsset &other) const {
		return changeset < other.changeset;
	}
	string ToString() {
		return Format("[%d] %d, %s GUID: %s parent: %s", date, changeset, name.c_str(), GUIDToString(guid).c_str(), GUIDToString(parent).c_str());
	}
	string GetDateString() const;
};


// A configuration is basically a list of changesets and represents the project status on the server.
// The configuration can be queried for differences between the configuration and the current working project.
// The configuration is stored as an SQLite database to ease implementation and avoid storing everything in memory.
class Configuration {
	public:
		~Configuration();
		
		// Returns the difference between the active configuration and the local work area
		bool GetChanges(vector<Item>* output, bool (*ProgressCallback) (string text, float progress));
		
		// Same as above but limited to assets in base or children thereof
		bool GetChanges( vector<Item> * output , const set<UnityGUID>& base);
		
		// Lists all asset names stored inside the directory apart from asset. Set asset to UnityGUID() to get all asset names in the directory
		bool GetOtherAssetNamesInDirectory( const UnityGUID& asset, const UnityGUID& folder, set<string>* names) ;
		
		// Clears the list of changesets stored. Used when reinitializing the configuration from the server.
		void Clear();
		
		// Adds a new changeset to the configuration
		void Add(const Changeset& changeset, const vector<Item>& assets);
		
		// Used by Maint client when updating the configuration
		struct Signature {
			int lastChangeset;
			int assetVersionCount;
			int guidCount;
			int changesetCount;
			
			bool operator==(const Signature &other) const {
				return lastChangeset == other.lastChangeset && assetVersionCount == other.assetVersionCount && 
					guidCount == other.guidCount && changesetCount == other.changesetCount;
			}
			bool operator!=(const Signature &other) const {
				return !(*this == other);
			}	
		};
		Signature GetSignature();
		
		static Configuration& Get ();
		
		Item GetServerItem(const UnityGUID& guid, int changeset=-1);
		bool GetServerItems(int changeset, vector<Item>* result) ;
		Item GetWorkingItem(const UnityGUID& guid);
		Item GetWorkingItemAtLocation(const UnityGUID& parent, const string& name);
		Item GetWorkingItemAtLocation(const string& assetPath);
		UnityGUID GetPathNameConflict(const UnityGUID& guid);
		bool HasDeletionConflict(const UnityGUID& guid);
		Item GetAncestor(Item& child, bool dontTouchMetadata);
		
		// History for entire project
		bool GetHistory(vector<Changeset>* history);
		// History for a given set of assets
		bool GetHistory(const set<UnityGUID>& guids, vector<Changeset>* history);

		// Returns the contents of the trash folder on the server
		bool ListDeletedAssets(vector<DeletedAsset>* assetList) ; 
		
		// Fix the project to a given changeset (similar to svn update -r VERSION)
		bool SetStickyChangeset(int changeset); // set to -1 to unfix
		int GetStickyChangeset(); // returns -1 if not fixed

		// Optimization as GetWorkingItem is often invoked with the same guid multiple times in a row
		void EnableWorkingItemCache();
		void DisableWorkingItemCache();

		// Asset server configuration
		string GetDatabaseName(const string& server, const string& user, const string& password, const string& port, const string& projectName);

		// Items have path index, by whitch actual index can be retrieved (works for deleted paths)
		void GetCachedPaths();
		const string& CachedPathFromID(int pathID);

		void GetUpdatesForMono(int startingFromChangeset, UNITY_MAP(kMemTempAlloc, int, ChangesetDummy) &availableUpdates);
		string GetDuplicateNames(const vector<UploadItem>& items);
	private:
		int m_StickyChangeset;
		// All constructors are private as one should use Configuration::Get() to get a reference to the global configuration object
		Configuration();
		Configuration(const Configuration& other);
		Configuration& operator = (const Configuration& other);
		SQLite* m_DB;
		int lastWorkingItemEnabled;
		Item lastWorkingItem;
		map<string,SQLite::StatementHandle*> m_CachedStatementHandles;

		vector<string> m_CachedPaths;

		void BeginExclusive();
		void Rollback();
		void Commit();

		SQLite::StatementHandle GetCachedStatementHandle(const string& querystring);
		int GetParentFolderID(const Item &i);
		string RestoreDeletedAssetPath (Item &i);
		string RestoreDeletedAssetPathRestoreParent(Item &i);
		void SetupCachedParents(int changeset, const vector<Item>& assets, bool files);

		// Asset server configuration
		void* GetUserConnectionForDatabase(const string& server, const string& user, const string& password, const string& port, const string& databaseName, bool reportErrors);

		void GetAllServerChangesets(map<UnityGUID, int> &changesets);
		void GetAllLocalChangesets(map<UnityGUID, int> &changesets);
		void BuildChangesetsFromGUIDs(const vector<UnityGUID> &changes, const vector<int> &workingChangesets, UNITY_MAP(kMemTempAlloc, int, ChangesetDummy) &availableUpdates);
};

bool ShouldIgnoreAsset( const Item& item );
bool ShouldIgnoreAsset( const UnityGUID& guid );

std::string NicifyAssetName(const std::string& name);

}

#endif
