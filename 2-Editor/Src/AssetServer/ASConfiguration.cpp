#include "UnityPrefix.h"
#include "ASConfiguration.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "ASController.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "External/sqlite/SQLite.h"
#include "Runtime/Utilities/LogAssert.h"
#include <ctime>
#include "Editor/Src/AssetServer/Backend/PgConn.h"
#include "ASCache.h"
#include "ASMonoUtility.h"

using namespace AssetServer;

// Increment this when changing the database schema
const int kSchemaVersion=5;

const string kDatabasePath = "Library/AssetVersioning.db";
static const char* kSchema[] = {
			"create table if not exists assetversion ( "
			"	changeset	integer,"
			"	guid	blob,"
			"	name	text collate nocase,"
			"	parent	blob,"
			"	assettype integer,"
			"	digest	blob,  "
			"	oldversion	integer,  "
			"	parentfolderid integer, "
			"primary key (changeset, guid) );",

			"create table if not exists parentfolders ("
			"	id			integer primary key,"
			"	folder		text"
			");",

			"insert or ignore into parentfolders (folder) values ('') ", // folder #1 - root path

			"create table if not exists changeset ( "
			"	id			integer primary key,"
			"	message		text, "
			"	owner		text, "
			"	date_time	integer"
			");",
			
			"create table if not exists configuration ( "
			"	guid		blob primary key, "
			"	changeset	integer "
			");",

			"create table if not exists row_counts ( "
			"	id		integer primary key, "
			"	rows	integer "
			");",
			
			"insert or ignore into row_counts (id,rows) values (0,0) ",
			"insert or ignore into row_counts (id,rows) values (1,0) ",
			"insert or ignore into row_counts (id,rows) values (2,0) ",
			
			"create trigger if not exists assetversion_insert_tr "
			"	after insert on assetversion "
			"	for each row begin "
			"		update row_counts set rows = rows+1 where id=0; "
			"		update configuration set changeset=new.changeset where guid=new.guid; "
			"		insert or ignore into configuration(guid, changeset) values( new.guid, new.changeset ) ;"
			"	end; ",
			
			"create trigger if not exists changeset_insert_tr "
			"	after insert on changeset "
			"	for each row begin "
			"		update row_counts set rows = rows+1 where id=1; "
			"	end; ",

			"create trigger if not exists configuration_insert_tr "
			"	after insert on configuration "
			"	for each row begin "
			"		update row_counts set rows = rows+1 where id=2; "
			"	end; ",

			"create trigger if not exists configuration_delete_tr "
			"	after delete on configuration "
			"	for each row begin "
			"		update row_counts set rows = rows-1 where id=2; "
			"	end; ",

			"create index if not exists assetversion_guid_cs__ix on assetversion ( guid, changeset ) ;",
			"create index if not exists assetversion_name_parent_cs__ix on assetversion ( name, parent, changeset ) ;",
			"create unique index if not exists assetversion_folder_cs__ix on parentfolders ( folder ) ;",
			
			NULL
};

#define FatalIfSQLError(st) if(! st.ResultOK() ) FatalErrorStringDontReport(st.GetErrorString())


static SQLite* GetDatabase() {

	SQLite* result = NULL;
	if (GetOrCreateSQLiteDatabase( &result, kDatabasePath, kSchema, kSchemaVersion) )
	{
		AssetServerCache::Get().InvalidateCachedItems();
	}

	return result;
}

// Hmm maybe this ought to go into a utility module somewhere instead of here....
static const char* kWeekDay[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* kMonth[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

#if UNITY_WIN
static struct tm *localtime_r(time_t *clock, struct tm *result)
{
  struct tm *p = localtime(clock); // Win32 uses thread-local storage, so using localtime should be threadsafe

  if (p)
    *(result) = *p;

  return p;
}
#endif

// Replacement for dateformat_nice
static string GetDateStringImpl(time_t then) {
	time_t now;
	time(&now);
	
	struct tm now_tm;
	localtime_r(&now, &now_tm); //convert time_t to a structure
	struct tm then_tm;
	localtime_r(&then, &then_tm); //convert time_t to a structure

	if ( then_tm.tm_year == now_tm.tm_year ) {
		if( then_tm.tm_yday == now_tm.tm_yday )
			return Format("Today %02d:%02d", then_tm.tm_hour, then_tm.tm_min);
		if( then_tm.tm_yday+1 == now_tm.tm_yday )
			return Format("Yesterday %02d:%02d", then_tm.tm_hour, then_tm.tm_min);
		// Truncate now to midnight
		now -= (now_tm.tm_hour * 3600 + now_tm.tm_min * 60 + now_tm.tm_sec);
		if( now - then < 6 * 24 * 3600 ) // within a week ago from today
			return Format("%s %02d:%02d", kWeekDay[then_tm.tm_wday], then_tm.tm_hour, then_tm.tm_min);
		
		return Format("%s %02d %02d:%02d", kMonth[then_tm.tm_mon], then_tm.tm_mday, then_tm.tm_hour, then_tm.tm_min);
	}
	
	
	return Format("%s %02d %04d", kMonth[then_tm.tm_mon], then_tm.tm_mday, then_tm.tm_year+1900);
}


string Changeset::GetDateString() const { return GetDateStringImpl(date); }
string DeletedAsset::GetDateString() const { return GetDateStringImpl(date); }

bool Changeset::GetItems(vector<Item>* result) const { return Configuration::Get().GetServerItems(changeset, result); }

// Instead of complex ifs or cases, simply map through a matrix when merging two statuses together. 
// Note: the table is only valid for values in the range [kClientOnly; kIgnored] which is the range returned by StatusImpl:
static Status kStatusMerge[9][9] = {
	// kClientOnly,		kServerOnly,	kUnchanged,			kConflict,		kSame,					kNewVersionAvailable,	kNewLocalVersion,	kRestoredFromTrash,	kIgnored
	{ kClientOnly,		kClientOnly,	kClientOnly,			kClientOnly,	kClientOnly,			kClientOnly,			kClientOnly,		kClientOnly,			kIgnored				}, // kClientOnly,		
	{ kClientOnly,		kServerOnly,	kServerOnly,			kServerOnly,	kServerOnly,			kServerOnly,			kServerOnly,		kServerOnly,			kServerOnly				}, // kServerOnly,	
	{ kClientOnly,		kServerOnly,	kUnchanged,				kConflict,		kSame,					kNewVersionAvailable,	kNewLocalVersion,	kRestoredFromTrash,		kIgnored				}, // kUnchanged,
	{ kClientOnly,		kServerOnly,	kConflict,				kConflict,		kConflict,				kConflict,				kConflict,			kConflict,				kNewVersionAvailable	}, // kConflict,
	{ kClientOnly,		kServerOnly,	kSame,					kConflict,		kSame,					kNewVersionAvailable,	kNewLocalVersion,	kRestoredFromTrash,		kIgnored				}, // kSame,
	{ kClientOnly,		kServerOnly,	kNewVersionAvailable,	kConflict,		kNewVersionAvailable,	kNewVersionAvailable,	kConflict,			kConflict,				kNewVersionAvailable	}, // kNewVersionAvailable,
	{ kClientOnly,		kServerOnly,	kNewLocalVersion,		kConflict,		kNewLocalVersion,		kConflict,				kNewLocalVersion,	kRestoredFromTrash,		kIgnored				}, // kNewLocalVersion
	{ kClientOnly,		kServerOnly,	kRestoredFromTrash,		kConflict,		kRestoredFromTrash,		kConflict,				kRestoredFromTrash,	kRestoredFromTrash,		kIgnored				}, // kRestoredFromTrash
	{ kIgnored,			kServerOnly,	kIgnored,				kNewVersionAvailable,	kIgnored,		kNewVersionAvailable,	kIgnored,			kIgnored,				kIgnored				}  // kIgnored	
	
};

bool AssetServer::ShouldIgnoreAsset( const Item& item )
{
	return ShouldIgnoreAsset(item.guid);
}

bool AssetServer::ShouldIgnoreAsset( const UnityGUID& guid )
{
	if ( guid == UnityGUID(0,0,5,0) ) // Ignore obsolete BuildPlayer.prefs asset
		return true;
	
	// this will ignore all assets that have the word "##ignore" in the path name
	string path = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
	if(path.find("##ignore") != string::npos) 
		return true;
	
	return false;
}


// The status functions perform similar logic on different variables. 
// Using templates to reduce copy and paste. Templates are cool :-P

template<class T> inline const T& ExtractField(const Item& i);
template<> inline const UnityStr& ExtractField(const Item& i) { return i.name ;}
template<> inline const UnityGUID& ExtractField(const Item& i) { return i.parent ;}
template<> inline const MdFour& ExtractField(const Item& i) { return i.digest ;}
template<> inline const Item& ExtractField(const Item& i) { return i ;}


template<class T> inline bool SameContent(const T& a, const T& b) { return a == b; }
template<> inline bool SameContent(const Item& a, const Item& b) { 
	return a.SameContent(b); }


template<class T> inline bool ShouldSkipPartByItemType(ItemType t) { return false ; }
template<> inline bool ShouldSkipPartByItemType<MdFour>(ItemType t) { return t==kTDir ; }

// when the ancestor parent guid is the trash guid, kNewLocalVersion turns into kRestoredFromTrash
template<class T> inline Status NewLocalOrRestoredFromTrash(const T& ancestor) { return kNewLocalVersion ; }
template<> inline Status NewLocalOrRestoredFromTrash(const UnityGUID& ancestor) { 
	if ( ancestor == kTrashGUID ) return kRestoredFromTrash ; 
	return kNewLocalVersion ;
}

template<class T> inline Status StatusIMPL( T& server,  T& working,  T& ancestor) { 
	bool server_changed = (server != ancestor); 
	bool client_changed = (working != ancestor); 
	if ( server_changed && client_changed ) { 
		if ( SameContent(server, working) ) 
			return kSame; 
		else 
			return kConflict; 
	} 
	else if ( client_changed ) 
		return NewLocalOrRestoredFromTrash(ancestor); 
	else if ( server_changed ) 
		return kNewVersionAvailable; 

	return kUnchanged; 
}

template<> inline Status StatusIMPL(Item& server, Item& working, Item& ancestor) {
	Status parent_status	= StatusIMPL(server.parent, working.parent, ancestor.parent);
	Status name_status		= StatusIMPL(server.name, working.name, ancestor.name);
	
	Status merged_status = kStatusMerge[name_status][parent_status];
	// Ignore digest when comparing directories
	if(server.type != kTDir && working.type != kTDir) {
		Status digest_status	= StatusIMPL(server.digest, working.digest, ancestor.digest);
		merged_status = kStatusMerge[digest_status][merged_status];
	}
	
	// Update metadata if returning kUnmodified and originalVersion is not up to date
	if(merged_status == kUnchanged){
		AssetServerCache::CachedAssetMetaData* meta = AssetServerCache::Get().FindCachedMetaData(working.guid);
		if( meta && meta->originalChangeset != server.changeset ) {
			meta->originalChangeset = server.changeset;
			meta->originalName = server.name;
			meta->originalParent = server.parent;
			meta->originalDigest = server.digest;
			AssetServerCache::Get().SetDirty();
			working.changeset = server.changeset;
		}
	}
		
	return merged_status;
}


template<class T> inline Status StatusIMPL (Item& my) { 
	if( my.origin == kOrUnknown ) return kBadState; 
	Status basicStatus = ShouldIgnoreAsset(my)?kIgnored:kUnchanged;

	if( my.origin == kOrServer ) { 
		if (my.IsDeleted())
		{
			if (my.type != kTDir)
			{
				if (Configuration::Get().HasDeletionConflict(my.guid))
					return kStatusMerge[basicStatus][kConflict]; // deleted asset was modified on the server. Don't allow committing.
			}

			return kStatusMerge[basicStatus][kServerOnly]; 
		}

		Item working = Configuration::Get().GetWorkingItem(my.guid); 
		if ( working == Item() )
		{
			return kStatusMerge[basicStatus][my.parent == kTrashGUID? kUnchanged : kServerOnly]; 
		} 
		if( ShouldSkipPartByItemType<T>(my.type) ) return basicStatus;
		if( working.changeset == my.changeset ) 
			return kStatusMerge[basicStatus][StatusIMPL(ExtractField<T>(my), ExtractField<T>(working), ExtractField<T>(my) )]; 
		Item ancestor = Configuration::Get().GetAncestor(working, false);
		return kStatusMerge[basicStatus][StatusIMPL(ExtractField<T>(my), ExtractField<T>(working), ExtractField<T>(ancestor) )]; 
	} 
	else { 
		Item server =  Configuration::Get().GetServerItem(my.guid); 
		if ( server == Item() ) 
			return kStatusMerge[basicStatus][kClientOnly]; 
		if( ShouldSkipPartByItemType<T>(my.type) ) return basicStatus;
		if( server.changeset == my.changeset ) 
			return kStatusMerge[basicStatus][StatusIMPL(ExtractField<T>(server), ExtractField<T>(my), ExtractField<T>(server) )];
		Item ancestor = Configuration::Get().GetAncestor(my, false); 
		return kStatusMerge[basicStatus][StatusIMPL(ExtractField<T>(server), ExtractField<T>(my), ExtractField<T>(ancestor) )];
	} 
}

Status Item::GetParentStatus () {
	return StatusIMPL<UnityGUID>(*this);
}
Status Item::GetNameStatus () {
	return StatusIMPL<UnityStr>(*this);
}
Status Item::GetDigestStatus () {
	return StatusIMPL<MdFour>(*this);
}

Status Item::GetStatus () {
	return StatusIMPL<Item>(*this);
}

inline static bool StatusIndicatesMoved(const Status& s) {
	return s == kConflict || s == kNewVersionAvailable || s == kNewLocalVersion || s == kRestoredFromTrash;
}

bool Item::IsMoved () {
	return !AssetServerCache::Get().IsItemDeleted(guid) && (StatusIndicatesMoved(GetParentStatus()) || StatusIndicatesMoved(GetNameStatus()));
}

bool Item::IsDeleted() const
{
	return AssetServerCache::Get().IsItemDeleted(guid);
}

ChangeFlags Item::GetChangeFlags() {
	Item ancestor = Configuration::Get().GetAncestor(*this, true);
	
	if(!ancestor)
		return kCFCreated;
	
	int result=kCFNone;
	
	if(ancestor.name != name)
		result |= kCFRenamed;
	
	if(ancestor.digest != digest)
		result |= kCFModified;
	
	if(ancestor.parent != parent)
	{
		result |= kCFMoved;

		if(parent == kTrashGUID)
			result |= kCFDeleted;
		else 
		if (ancestor.parent == kTrashGUID)
			result |= kCFUndeleted;
	}else
	{
		if(parent == kTrashGUID)
		{
			result |= kCFDeleted;
		}
		else
		{
			Item serverItem = Configuration::Get().GetServerItem(guid);

			if (serverItem.parent == kTrashGUID)
				result |= kCFUndeleted;
		}
	}

	return (ChangeFlags)result;
}

Configuration::Configuration() : m_StickyChangeset(-1), lastWorkingItemEnabled(0) {
	m_DB=GetDatabase();
	m_CachedPaths.push_back("");
}

Configuration::~Configuration() {
	for(map<string,SQLite::StatementHandle*>::iterator i=m_CachedStatementHandles.begin(); i != m_CachedStatementHandles.end() ; i++) 
		delete i->second;
	delete m_DB;

}

Configuration& Configuration::Get() {
	static Configuration* s_Configuration= new Configuration();
	return *s_Configuration;
}

int Configuration::GetStickyChangeset() {

	return m_StickyChangeset;
}

bool Configuration::SetStickyChangeset(int changeset) {
	m_StickyChangeset=changeset;
	return true;
}

Item Configuration::GetServerItem(const UnityGUID& guid, int changeset) {
	if( changeset >=0 )
	{ 
		SQLite::StatementHandle st = GetCachedStatementHandle("select changeset, name, parent, assettype, digest, oldversion, parentfolderid from assetversion a where  a.changeset<=?1 and guid=?2 "
		 " order by changeset desc limit 1");
		FatalIfSQLError(st);

		st.Bind(1,(SInt32)changeset);
		st.Bind(2,guid);
		if ( st.ExecuteStep() ) {
			return Item(st.GetIntColumn(0), guid, st.GetTextColumn(1), st.GetGUIDColumn(2), (ItemType) st.GetIntColumn(3), st.GetMdFourColumn(4), kOrServer, st.GetIntColumn(5), st.GetIntColumn(6));
		}
		FatalIfSQLError(st);
	}
	else
	{ 
		SQLite::StatementHandle st = GetCachedStatementHandle(string("select changeset,name, parent, assettype, digest, oldversion, parentfolderid from assetversion a where guid=?1  ") +
		(m_StickyChangeset >= 0?" and  a.changeset <= ?2  ":"") +
		" order by changeset desc limit 1");
		FatalIfSQLError(st);

		st.Bind(1,guid);
		if (m_StickyChangeset >= 0)
			st.Bind(2,(SInt32) m_StickyChangeset);
		if ( st.ExecuteStep() ) {
			return Item(st.GetIntColumn(0), guid, st.GetTextColumn(1), st.GetGUIDColumn(2), (ItemType) st.GetIntColumn(3), st.GetMdFourColumn(4), kOrServer, st.GetIntColumn(5), st.GetIntColumn(6));
		}
		FatalIfSQLError(st);
		
		
	}
	if (m_StickyChangeset >= 0 ) {
		// This is to make sure asssets are removed when updating to an earlier version:
		SQLite::StatementHandle st = GetCachedStatementHandle("select changeset,name, parent, assettype, digest, oldversion, parentfolderid from assetversion a where ( ?1 < 0 or a.changeset<=?1) and guid=?2 order by changeset desc  limit 1");
		FatalIfSQLError(st);

		st.Bind(1,(SInt32)changeset);
		st.Bind(2,guid);
		if ( st.ExecuteStep() && st.GetGUIDColumn(2) != UnityGUID() ) {
			return Item(1, guid, st.GetTextColumn(1), kTrashGUID, (ItemType) st.GetIntColumn(3), st.GetMdFourColumn(4), kOrServer, st.GetIntColumn(5), st.GetIntColumn(6));
		}
		FatalIfSQLError(st);
	
	}
	return Item();
}

bool Configuration::GetServerItems(int changeset, vector<Item>* result) {
	if(result == NULL)
		return false;
	result->clear();
		
	SQLite::StatementHandle st = GetCachedStatementHandle("select changeset, guid, name, parent, assettype, digest, oldversion, parentfolderid from assetversion where changeset=?1");
	FatalIfSQLError(st);
	

	st.Bind(1,(SInt32)changeset);
	while ( st.ExecuteStep() ) {
		result->push_back( Item(st.GetIntColumn(0),  st.GetGUIDColumn(1), st.GetTextColumn(2), st.GetGUIDColumn(3), (ItemType) st.GetIntColumn(4), st.GetMdFourColumn(5), kOrServer, st.GetIntColumn(6), st.GetIntColumn(7)) );
	}
	FatalIfSQLError(st);
	return result->size() > 0;
}

Item Configuration::GetAncestor(Item& child, bool dontTouchMetadata)
{
	if (child.origin == kOrClient ) {
		AssetServerCache::CachedAssetMetaData* meta = AssetServerCache::Get().FindCachedMetaData(child.guid);

		if( meta ) {
			if ( meta->originalName != "" && ( meta->originalParent != UnityGUID() || child.guid == kTrashGUID || child.guid == UnityGUID(0,0,1,0) ) ) {
				Item result=GetServerItem( child.guid, child.changeset );
				if( result.changeset != child.changeset ||  meta->originalName != result.name || meta->originalParent != result.parent  || 
					( child.type != kTDir && meta->originalDigest != result.digest) ) {
					
					// Try to find best match in asset history when metadata don't match the server version
					// (This usually means that the user switched from one server project to another.)
					SQLite::StatementHandle st = GetCachedStatementHandle( 
						string("select changeset, "
						"	case name when ?2 then 1 else 0 end "
						"	+ case parent when ?3 then 1 else 0 end  ") +
						( child.type == kTDir ? "" : 
							"	+ case digest when ?4 then 1 else 0 end " ) + "  as matchlevel  "
						"	from assetversion o "
						"		where o.guid=?1 "
						"	order by matchlevel desc, changeset asc "
						"	limit 1"
					);
					FatalIfSQLError(st);
					
					st.Bind(1,child.guid);
					st.Bind(2,meta->originalName);
					st.Bind(3,meta->originalParent);
					if(child.type != kTDir)
						st.Bind(4,meta->originalDigest);
					
					if ( st.ExecuteStep() ) {
						result.changeset = st.GetIntColumn(0);
					}
					
					if (!dontTouchMetadata)
					{
						AssetServerCache::CachedAssetMetaData *meta = AssetServerCache::Get().FindCachedMetaData(child.guid);
						child.changeset=result.changeset;
						meta->originalChangeset=result.changeset;
						AssetServerCache::Get().SetDirty();
					}
					
					result.name=meta->originalName;
					result.parent=meta->originalParent;
					result.digest=meta->originalDigest;
				}

				return result;
			}
			// Handling of newly created projects: Project settings are always marked as non-conflicting when they exist on the server, but the local copy is not marked as being versioned.
			else if( child.changeset == 0 && GetGUIDPersistentManager().IsConstantGUID(child.guid) )
			{
				Item result = GetServerItem( child.guid, -1 );
				if ( result ) {
					meta->originalName = GetLastPathNameComponent(meta->pathName);
					meta->originalDigest = AssetServerCache::Get().FindCachedDigest(child.guid);
					meta->originalChangeset = child.changeset;
					result.changeset = child.changeset;
					result.name = meta->originalName;
					result.parent = UnityGUID();
					result.digest = meta->originalDigest;
					
					AssetServerCache::Get().SetDirty();
				}
				return result;

			}
			// Backwards compatibility when upgrading pre 3.5 projects
			else {
				Item result = GetServerItem( child.guid, child.changeset );
				if ( result ) {
					child.changeset=result.changeset;
					meta->originalChangeset=result.changeset;
					meta->originalName=result.name;
					meta->originalParent=result.parent;
					meta->originalDigest=result.digest;
					AssetServerCache::Get().SetDirty();
				}
				return result;
			}
		}
	}
	else { 
		SQLite::StatementHandle st = GetCachedStatementHandle("select changeset,name, parent, assettype, digest from assetversion where  changeset<?1 and guid=?2 order by changeset desc limit 1");
		FatalIfSQLError(st);

		st.Bind(1,(SInt32)child.changeset);
		st.Bind(2,child.guid);
		if ( st.ExecuteStep() ) {
			return Item(st.GetIntColumn(0), child.guid, st.GetTextColumn(1), st.GetGUIDColumn(2), (ItemType) st.GetIntColumn(3), st.GetMdFourColumn(4), kOrServer);
		}
		FatalIfSQLError(st);
		
	}
	return Item();
}

void Configuration::EnableWorkingItemCache() {
	lastWorkingItemEnabled++;
}

void Configuration::DisableWorkingItemCache() {
	lastWorkingItemEnabled--;
	
	if(lastWorkingItemEnabled <=0 ) {
		lastWorkingItemEnabled = 0;
		lastWorkingItem = Item();
	}
	
}


Item Configuration::GetWorkingItem(const UnityGUID& guid) 
{
	if ( guid != kTrashGUID ) \
	{
		if( lastWorkingItem.guid == guid) return lastWorkingItem;
		
		//printf_console("Configuration::GetWorkingItem: %s\n", GUIDToString(guid).c_str());

		AssetDatabase& db = AssetDatabase::Get();
		const Asset* asset = db.AssetPtrFromGUID(guid); 

		if( asset == NULL ) 
		{
			//return Item();
			// is it a "deleted" item?
			AssetServerCache::DeletedItem di;
			if (!AssetServerCache::Get().GetDeletedItem(guid, di))
				return Item();

			Item newItem;

			newItem.changeset = di.changeset;
			newItem.guid = di.guid;
			newItem.name = GetLastPathNameComponent(di.fullPath);
			newItem.parent = kTrashGUID;
			newItem.digest = di.digest;
			newItem.type = (ItemType)di.type;
			newItem.origin = kOrServer;

			return newItem;
		}

		int originalChangeset;
		string pathName;
		MdFour digest = AssetServerCache::Get().FindCachedDigest(guid);

		if (!AssetServerCache::Get().GetCachesInitialized())
		{
			const AssetMetaData* meta = FindAssetMetaData(guid); 
			if( meta == NULL ) return Item();
			originalChangeset = meta->originalChangeset;
			pathName = meta->pathName;
		}
		else
		{
			const AssetServerCache::CachedAssetMetaData* meta = AssetServerCache::Get().FindCachedMetaData(guid); 
			if( meta == NULL ) return Item();
			originalChangeset = meta->originalChangeset;
			pathName = meta->pathName;
		}

		ItemType assettype = IsDirectoryCreated (pathName) ? kTDir : kTText;

		Item result = Item (originalChangeset, guid, GetLastPathNameComponent(GetActualPathSlow(pathName)), 
			Controller::Get().IsMarkedForRemoval(guid)?kTrashGUID:asset->parent, assettype, digest, kOrClient);
		if( lastWorkingItemEnabled > 0 ) lastWorkingItem = result;
		return result;
	}
	else 
	{ 
		// We pretend that the Trash on the client is always up to date and just return the latest server version with origin changed to kOrClient
		Item tmp = GetServerItem(guid,-1);
		tmp.origin=kOrClient;
		return tmp;
	}
}

Item Configuration::GetWorkingItemAtLocation(const UnityGUID& parent, const string& name) {
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	
	string parentPath = pm.AssetPathNameFromGUID (parent);
	if ( parentPath != string() ) {
		return GetWorkingItemAtLocation( AppendPathName( parentPath, name) );
	}
	return Item();
}

Item Configuration::GetWorkingItemAtLocation(const string& assetPath) {
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	
	UnityGUID guid;
	if ( pm.PathNameToGUID(assetPath, &guid)  ) {
		return GetWorkingItem(guid);
	}
	return Item();
}

UnityGUID Configuration::GetPathNameConflict(const UnityGUID& guid) {
	Item working = GetWorkingItem(guid);
	if ( working ) {
		
		SQLite::StatementHandle st = GetCachedStatementHandle(string("select guid from assetversion a where name=?1 and parent=?2 and guid <>?3 ") +
		(m_StickyChangeset >= 0?" and a.changeset <= ?4 ":"") +
		" order by changeset desc limit 1");
		FatalIfSQLError(st);
		
		st.Bind(1,working.name);
		st.Bind(2,working.parent);
		st.Bind(3,guid);
		if (m_StickyChangeset >= 0)
			st.Bind(4,(SInt32)m_StickyChangeset);
			
		if ( st.ExecuteStep() ) {
			UnityGUID tmp =  st.GetGUIDColumn(0);
			Item server = GetServerItem(tmp);
			
			if ( server.name == working.name && server.parent == working.parent ) {
				Status cstatus = server.GetStatus() ;
				Status wstatus = working.GetStatus() ;
				if ( (cstatus == kServerOnly || cstatus == kConflict || cstatus == kNewVersionAvailable) &&
					  (wstatus == kNewLocalVersion || wstatus == kConflict || wstatus == kClientOnly) )
					return tmp;
			}
		}
		FatalIfSQLError(st);
	}
	else {
		Item server = GetServerItem(guid);
		if ( server ) {
			working = GetWorkingItemAtLocation(server.parent, server.name);
			if(working && working.guid != guid) {
				Status cstatus = server.GetStatus() ;
				Status wstatus = working.GetStatus() ;
				if ( (cstatus == kServerOnly || cstatus == kConflict || cstatus == kNewVersionAvailable) &&
						(wstatus == kNewLocalVersion || wstatus == kConflict || wstatus == kClientOnly) )
					return working.guid;
			}
		}
	}
	return UnityGUID();
}

bool Configuration::HasDeletionConflict(const UnityGUID& guid)
{
	Item server = Configuration::Get().GetServerItem(guid); 
	AssetServerCache::DeletedItem item;
	AssetServerCache::Get().GetDeletedItem(guid, item);

	if ((server.type == kTDir) || (item.type == kTDir))
		return false;

	return server.changeset != item.changeset;
}

// Clearing reinitializes the entire database. Simplifies a lot of stuff.
void Configuration::Clear() {
	for(map<string,SQLite::StatementHandle*>::iterator i=m_CachedStatementHandles.begin(); i != m_CachedStatementHandles.end() ; i++) 
		delete i->second;
	m_CachedStatementHandles.clear();

	delete m_DB;
	
	DeleteFileOrDirectory(kDatabasePath);
	m_DB=GetDatabase();

	m_CachedPaths.clear();
	m_CachedPaths.push_back("");

	AssetServerCache::Get().InvalidateCachedItems();
	AssetServerCache::Get().ClearDeletedItems();
}

Configuration::Signature Configuration::GetSignature() {

	Configuration::Signature result;
	result.lastChangeset=result.assetVersionCount=result.guidCount=result.changesetCount=0;
	
	SQLite::StatementHandle asset_st = GetCachedStatementHandle("select id, rows from row_counts");
	FatalIfSQLError(asset_st);
	while(asset_st.ExecuteStep()) {
		int value=asset_st.GetIntColumn(1);
		switch(asset_st.GetIntColumn(0)) {
			case 0:
				result.assetVersionCount=value;
				break;
			case 1:
				result.changesetCount=value;
				break;
			case 2:
				result.guidCount=value;
				break;
			default:
				break;
		}
	}
	FatalIfSQLError(asset_st);

	SQLite::StatementHandle changeset_st = GetCachedStatementHandle("select max(id) from changeset");
	FatalIfSQLError(changeset_st);
	if(changeset_st.ExecuteStep()) {
		result.lastChangeset=changeset_st.GetIntColumn(0);
	}
	FatalIfSQLError(changeset_st);
	return result;
}

bool Configuration::GetChanges( vector<Item> * items, bool (*ProgressCallback) (string text, float progress) ) 
{
	if( items == NULL ) return false;
	items->clear();
	
	int counter = 0;

	Thread::Sleep(0.1f);
	if (!ProgressCallback("Getting all assets in project", 0))
		return false;

	// Find guids of all current client assets
	AssetDatabase& adb = AssetDatabase::Get();
	vector_set<UnityGUID> clientGuids;
	adb.GetAllAssets(clientGuids);

	// Find guids of all current server assets
	{
		SQLite::StatementHandle st = GetCachedStatementHandle( 
			string("select o.changeset, o.guid, name, parent, assettype, digest   "
			"	from assetversion o, ") + 
			(m_StickyChangeset >= 0?
				"		(select max(changeset) changeset, guid from assetversion a " 
				"			where  a.changeset <= ?1  "
				"		group by guid) c "
			:
				"		configuration c ") +
			"	where o.changeset = c.changeset "
			"		and o.guid = c.guid"
		);
		FatalIfSQLError(st);
		if(m_StickyChangeset >= 0)
			st.Bind(1,(SInt32)m_StickyChangeset);

		while(st.ExecuteStep()) 
		{
			Item tmp = Item(st.GetIntColumn(0), st.GetGUIDColumn(1), st.GetTextColumn(2), st.GetGUIDColumn(3), (ItemType) st.GetIntColumn(4), st.GetMdFourColumn(5), kOrServer);
			if (ShouldIgnoreAsset(tmp.guid))
				continue;
			
			clientGuids.erase(tmp.guid); // Mark the guid as seen on server
			switch( tmp.GetStatus() ) {
			 case kUnchanged : 
			 case kIgnored : 
				break;
			 case kNewLocalVersion :
			 case kRestoredFromTrash :
				items->push_back(GetWorkingItem(tmp.guid));
				break;
			 default :
				items->push_back(tmp);
				break;
			}

			counter++;
			if (counter % 500 == 0)
			{
				if (!ProgressCallback("Getting server assets.", 0))
					return false;
			}
		}
		FatalIfSQLError(st);
	}
	// Find guids of all assets that were added after the fixed asset version
	if(m_StickyChangeset >= 0)
	{
		SQLite::StatementHandle st = GetCachedStatementHandle(
			"select distinct guid from assetversion o "
			"	where o.changeset > ?1 "
			"		and parent != '00000000000000000000000000000000' "
			"		and not exists (select 1 from assetversion i where i.changeset <= ?1 and i.guid = o.guid  ) "
		);
		FatalIfSQLError(st);
		st.Bind(1,(SInt32)m_StickyChangeset);

		while(st.ExecuteStep()) 
		{
			UnityGUID guid = st.GetGUIDColumn(0);
			clientGuids.erase(guid); // Mark the guid as seen on server
			Item tmp = GetServerItem(guid);
			if(tmp) {
				Status status = tmp.GetStatus() ;
				switch( status ) {
				 case kUnchanged : 
				 case kIgnored : 
					break;
				 case kNewLocalVersion :
				 case kRestoredFromTrash :
					AssertString("Impossible case?");
					break;
				 default :
					items->push_back(tmp);
					break;
				}
			}

			counter++;
			if (counter % 500 == 0)
			{
				if (!ProgressCallback("Getting server assets.", 0.5f))
					return false;
			}
		}
		FatalIfSQLError(st);
	}
	
	// Add client-only items
	for (vector_set<UnityGUID>::iterator i= clientGuids.begin(); i != clientGuids.end(); i++) 
	{
		if(ShouldIgnoreAsset(*i) )
			continue;
		
		Item item = GetWorkingItem(*i);
		items->push_back(item);

		counter++;
		if (counter % 50 == 0)
		{
			Thread::Sleep(0.1f);
			if (!ProgressCallback("Getting client-only assets.", 1))
				return false;
		}
	}

	return true;
}

bool Configuration::GetChanges( vector<Item> * items , const set<UnityGUID>& base ) {
	if( items == NULL ) return false;
	items->clear();

	// Find guids of all current client assets in the selection
	AssetDatabase& adb = AssetDatabase::Get();
	set<UnityGUID> clientGuids;

	for( set<UnityGUID>::const_iterator i = base.begin(); i != base.end() ; i++ ) {
		if (adb.IsAssetAvailable (*i))
			adb.CollectAllChildren (*i, &clientGuids);
	}
	clientGuids.insert(base.begin(), base.end());
	
	set<Item> allItems;
	map<UnityGUID,Item> result;

	// Find guids of all current server assets
	{
		SQLite::StatementHandle st = GetCachedStatementHandle( string(
			"select o.changeset, o.guid, name, parent, assettype, digest   "
			"	from assetversion o, ")+
			(m_StickyChangeset >= 0?
				"	(select max(changeset) changeset, guid from assetversion a "
				"		where  a.changeset <= ?1  "
				"		group by guid) c "
			:
				"	configuration c") +
			"	where o.changeset = c.changeset "
			"		and o.guid = c.guid"
		);
		FatalIfSQLError(st);
		if(m_StickyChangeset >= 0)
			st.Bind(1,(SInt32)m_StickyChangeset);

		while(st.ExecuteStep()) {
			allItems.insert(Item(st.GetIntColumn(0), st.GetGUIDColumn(1), st.GetTextColumn(2), st.GetGUIDColumn(3), (ItemType) st.GetIntColumn(4), st.GetMdFourColumn(5), kOrServer));
		}
		FatalIfSQLError(st);
	}
	// Find guids of all assets that were added after the fixed asset version
	if(m_StickyChangeset >= 0)
	{
		SQLite::StatementHandle st = GetCachedStatementHandle(
			"select distinct guid from assetversion o "
			"	where o.changeset > ?1 "
			"		and parent != '00000000000000000000000000000000' "
			"		and not exists (select 1 from assetversion i where i.changeset <= ?1 and i.guid = o.guid ) "
		);
		FatalIfSQLError(st);
		st.Bind(1,(SInt32)m_StickyChangeset);

		while(st.ExecuteStep()) {
			UnityGUID guid = st.GetGUIDColumn(0);
			Item tmp = GetServerItem(guid);
			allItems.insert(tmp);
		}
		FatalIfSQLError(st);
	}
	
	// Add items in the hierarchy to the results
	int oldCount=-1;
	while(oldCount != result.size() ) { // we're done when we stop adding to the result list.
		oldCount = result.size();
		set<Item>::iterator next;
		for(set<Item>::iterator i=allItems.begin(); i != allItems.end(); i=next) {
			next = i; next++;
			if ( clientGuids.count(i->guid) > 0 || clientGuids.count(i->parent) > 0 || result.count(i->parent) > 0 ) {
				clientGuids.erase(i->guid);
				result[i->guid]=*i;
				allItems.erase(i);
			}
		}
	}
	
	// Populate actual result list
	for (map<UnityGUID,Item>::iterator i= result.begin(); i != result.end(); i++) {
		switch( i->second.GetStatus() ) {
		 case kUnchanged : 
		 case kIgnored : 
			break;
		 case kNewLocalVersion :
		 case kRestoredFromTrash :
			items->push_back(GetWorkingItem(i->second.guid));
			break;
		 default :
			items->push_back(i->second);
			break;
		}
	}
	
	// Add client-only items
	for (set<UnityGUID>::iterator i= clientGuids.begin(); i != clientGuids.end(); i++) {
		if( ShouldIgnoreAsset(*i) ) 
			continue;
			
		Item item = GetWorkingItem(*i);
		items->push_back(item);
	}
	
	return true;
}

// Returns history affecting a set of assets. (ie. it filters out all changesets that have not modified the given set of guids)
// Passing an empty set of guids is the same as calling GetHistory( vector<Changeset>* history );
bool Configuration::GetHistory(const set<UnityGUID>& guids, vector<Changeset>* history) {
	if(guids.empty())
		return GetHistory(history);
		
	if( history == NULL ) return false;
	history->clear();

	{
		BeginExclusive();
		SQLite::StatementHandle st1 = GetCachedStatementHandle(
			"create temporary table if not exists tmp_get_history_guids ( guid blob primary key ); "
			"delete from tmp_get_history_guids;"
		);
		FatalIfSQLError(st1);
		st1.ExecuteStep();
		FatalIfSQLError(st1);
		st1.Reset();
		
		
		SQLite::StatementHandle st2 = GetCachedStatementHandle(
			"insert or ignore into tmp_get_history_guids(guid) values(?);"
		);
		FatalIfSQLError(st2);
		for(set<UnityGUID>::const_iterator i= guids.begin(); i!=guids.end(); i++) {
			st2.Bind(1, *i);
			st2.ExecuteStep();
			FatalIfSQLError(st2);
			st2.Reset();
		}

		Commit();
	}

	SQLite::StatementHandle st(m_DB,  
		"select distinct id, message, owner, date_time " 
			"	from changeset c, "
			"		 assetversion a, "
			"		 tmp_get_history_guids t "
			"	where a.changeset=c.id " 
			"		and a.guid = t.guid " 
			"	order by id desc"
	);
	FatalIfSQLError(st);

	while(st.ExecuteStep()) {
		Changeset changeset;
		changeset.changeset = st.GetIntColumn(0);
		changeset.message = st.GetTextColumn(1);
		changeset.owner = st.GetTextColumn(2);
		changeset.date = st.GetIntColumn(3);
		history->push_back(changeset);
	}
	FatalIfSQLError(st);
	
	SQLite::StatementHandle st1 = GetCachedStatementHandle(
		"delete from tmp_get_history_guids;"
	);
	FatalIfSQLError(st1);
	st1.ExecuteStep();
	FatalIfSQLError(st1);

	return true;
}

// Returns the entire history of the project
bool Configuration::GetHistory(vector<Changeset>* history) {
	if( history == NULL ) return false;
	history->clear();

		
	SQLite::StatementHandle st(m_DB,  
		"select id, message, owner, date_time " 
			"	from changeset c "
			"	order by id desc"
	);
	FatalIfSQLError(st);

	while(st.ExecuteStep()) {
		Changeset changeset;
		changeset.changeset = st.GetIntColumn(0);
		changeset.message = st.GetTextColumn(1);
		changeset.owner = st.GetTextColumn(2);
		changeset.date = st.GetIntColumn(3);
		history->push_back(changeset);
	}
	FatalIfSQLError(st);

	return true;
}


bool Configuration::ListDeletedAssets(vector<DeletedAsset>* assetList) {
	if( assetList == NULL ) return false;
	assetList->clear();
	AssetDatabase& db = AssetDatabase::Get();

	SQLite::StatementHandle st = GetCachedStatementHandle( 
		"select * from (select a.changeset, a.guid, o.parent, o.name, cs.date_time, o.assettype, a.parentfolderid "
		"	from assetversion a,  "
		"		assetversion o, "
		"		configuration c, "
		"		changeset cs "
		"	where a.parent = ?1 "
		"		and o.guid = a.guid "
		"		and c.guid = a.guid and  c.changeset = a.changeset  " // make sure only to select current versions of assets
		"		and o.changeset in ( select max(changeset) from assetversion i where i.guid = a.guid and i.parent <> ?1  ) " // last version not in trash
		"		and cs.id=a.changeset "
		"union select b.changeset, b.guid, b.parent, b.name, cs2.date_time, b.assettype, b.parentfolderid " // Also include assets that were uploaded directly to the trash
		"	from assetversion b, "
		"		configuration k, "
		"		changeset cs2 "
		"	where b.parent = ?1 "
		"		and k.changeset = b.changeset and k.guid = b.guid  " // current version
		"		and not exists ( select 1 from assetversion l where l.guid = b.guid and l.parent <> ?1  ) " // ... that has never existed outside of the trash
		"		and cs2.id = b.changeset  ) order by 1 desc, 4"
	);
	FatalIfSQLError(st);

	st.Bind(1, kTrashGUID);

	
	FatalIfSQLError(st);
	while(st.ExecuteStep()) {
		DeletedAsset entry;
		entry.changeset = st.GetIntColumn(0);
		entry.guid = st.GetGUIDColumn(1);

		if( db.IsAssetAvailable(entry.guid) ) continue; // Ignore assets that exist locally (either not updated yet or restored from trash)

		entry.parent = st.GetGUIDColumn(2);
		entry.name = st.GetTextColumn(3);
		entry.date = st.GetIntColumn(4);
		entry.type = (ItemType)st.GetIntColumn(5);
		entry.parentFolderID = st.GetIntColumn(6);

		// Fix the file name of assets that were uploaded directly to the Trash bin.
		int delIndex = entry.name.rfind(" (DEL_");
		if(entry.parent == kTrashGUID && delIndex != string::npos  &&
			entry.name[entry.name.size()-1] == ')' && 
			entry.name.find_first_not_of("0123456789abcdef",delIndex+6) == entry.name.size()-1 
			) {
			entry.name.erase(delIndex);
			entry.parent = UnityGUID (0,0,1,0);
		}
		assetList->push_back(entry);
	}
	FatalIfSQLError(st);

	return true;
}

void Configuration::GetAllServerChangesets(map<UnityGUID, int> &changesets)
{
	// find changesets for all non-deleted server assets
	SQLite::StatementHandle st(m_DB, "SELECT c.guid, c.changeset FROM configuration AS c, assetversion AS v "
		"WHERE v.changeset = c.changeset AND v.parent != ?1 AND c.guid = v.guid" );

	st.Bind(1,kTrashGUID);

	FatalIfSQLError(st);

	while(st.ExecuteStep())
	{
		changesets[st.GetGUIDColumn(0)] = st.GetIntColumn(1);
	}
	FatalIfSQLError(st);
}

string Configuration::GetDuplicateNames(const vector<UploadItem>& items)
{
	string conflictingPaths;

	// find names and parents for all server assets
	SQLite::StatementHandle st(m_DB, "SELECT c.guid, pf.folder, v.name FROM configuration AS c, assetversion AS v, parentfolders AS pf "
		"WHERE v.changeset = c.changeset AND c.guid = v.guid AND pf.id = v.parentfolderid" );

	FatalIfSQLError(st);

	while(st.ExecuteStep())
	{
		UnityGUID guid = st.GetGUIDColumn(0);
		string folder = st.GetTextColumn(1);
		string name = st.GetTextColumn(2);

		for (vector<UploadItem>::const_iterator i = items.begin(); i != items.end(); i++)
		{
			if (guid != i->guid && ToLower(i->name) == ToLower(name) && ToLower(i->path) == ToLower(folder + name))
			{
				conflictingPaths += "\n" + i->path;
			}
		}
	}
	FatalIfSQLError(st);

	return conflictingPaths;
}

void Configuration::GetAllLocalChangesets(map<UnityGUID, int> &changesets)
{
	AssetServerCache::Get().GetWorkingItemChangesets(changesets);

	// add locally deleted items
	vector<AssetServerCache::DeletedItem> delItems;
	AssetServerCache::Get().GetDeletedItems(delItems);

	for (vector<AssetServerCache::DeletedItem>::const_iterator i = delItems.begin(); i != delItems.end(); i++)
	{
		changesets[i->guid] = i->changeset;
	}
}

// build the changeset structure
void Configuration::BuildChangesetsFromGUIDs(const vector<UnityGUID> &changes, const vector<int> &workingChangesets, UNITY_MAP(kMemTempAlloc, int, ChangesetDummy) &availableUpdates)
{
	// retrieve items for changesets
	vector<int>::const_iterator ic = workingChangesets.begin();

	for (vector<UnityGUID>::const_iterator i = changes.begin(); i != changes.end(); i++, ic++)
	{
		SQLite::StatementHandle st = GetCachedStatementHandle("SELECT changeset, guid, name, parent, assettype, digest, oldversion, parentfolderid FROM assetversion AS v WHERE changeset >= ?1 AND guid = ?2");
		FatalIfSQLError(st);

		st.Bind(1,(SInt32)(*ic) + 1);
		st.Bind(2,*i);

		while ( st.ExecuteStep() )
		{
			int changeset = st.GetIntColumn(0);
			Item item = Item(changeset, st.GetGUIDColumn(1), st.GetTextColumn(2), st.GetGUIDColumn(3), (ItemType) st.GetIntColumn(4), st.GetMdFourColumn(5), kOrServer, st.GetIntColumn(6), st.GetIntColumn(7));

			//TODO: optimize this to not iterate map every time?
			ChangesetDummy *cd = &availableUpdates[changeset];
			cd->items.push_back(item);
		}
		FatalIfSQLError(st);
	}

	// retrieve changeset information
	for (UNITY_MAP(kMemTempAlloc, int, ChangesetDummy)::iterator i = availableUpdates.begin(); i != availableUpdates.end(); i++)
	{
		SQLite::StatementHandle st(m_DB, "SELECT id, message, owner, date_time FROM changeset c WHERE c.id = ?1 LIMIT 1");
		FatalIfSQLError(st);

		st.Bind(1,(SInt32)i->first);

		if (!st.ExecuteStep())
			FatalIfSQLError(st);

		Changeset changeset;
		changeset.changeset = st.GetIntColumn(0);
		changeset.message = st.GetTextColumn(1);
		changeset.owner = st.GetTextColumn(2);
		changeset.date = st.GetIntColumn(3);

		i->second.srcChangeset = changeset;
	}
}

// figure out all new changes
void Configuration::GetUpdatesForMono(int startingFromChangeset, UNITY_MAP(kMemTempAlloc, int, ChangesetDummy) &availableUpdates)
{
	map<UnityGUID, int> serverChangesets;
	map<UnityGUID, int> localChangesets;

	GetAllLocalChangesets(localChangesets);
	GetAllServerChangesets(serverChangesets);

	vector<UnityGUID> changes;
	vector<int> workingChangesets;

	// look for differences between server and local assets
	for (map<UnityGUID, int>::const_iterator i = serverChangesets.begin(); i != serverChangesets.end(); i++)
	{
		map<UnityGUID, int>::iterator found = localChangesets.find(i->first);
		if ((found == localChangesets.end() || found->second != i->second) && i->first != kTrashGUID)
		{
			changes.push_back(i->first);

			if (found == localChangesets.end())
				workingChangesets.push_back(-1); // we don't have this server item locally
			else
				workingChangesets.push_back(found->second);
		}

		if (found != localChangesets.end())
		{
			localChangesets.erase(found);
		}
	}

	// what's left in localChangesets are new local items or items that are deleted on server but still exist locally
	for (map<UnityGUID, int>::const_iterator i = localChangesets.begin(); i != localChangesets.end(); i++)
	{
		if (i->second != -1 && !AssetServerCache::Get().IsItemDeleted(i->first))
		{
			changes.push_back(i->first);
			workingChangesets.push_back(i->second);
		}
	}
	
	BuildChangesetsFromGUIDs(changes, workingChangesets, availableUpdates);
}

bool Configuration::GetOtherAssetNamesInDirectory( const UnityGUID& asset, const UnityGUID& folder, set<string>* names) {
	if( names == NULL ) return false;
	names->clear();

	SQLite::StatementHandle st = GetCachedStatementHandle( 
		"select name from assetversion a, configuration c where a.guid <>?1 and a.parent = ?2 and a.guid = c.guid and  a.changeset = c.changeset " 
	);
	FatalIfSQLError(st);

	st.Bind(1, asset);
	st.Bind(2, folder);
	FatalIfSQLError(st);

	while(st.ExecuteStep()) 
		names->insert(ToLower(st.GetTextColumn(0)));

	FatalIfSQLError(st);

	return true;
}

string Configuration::RestoreDeletedAssetPathRestoreParent(Item &i)
{
	static string libraryFolder = "ProjectSettings/";

	if (GetGUIDPersistentManager().IsConstantGUID(i.parent))
	{
		return Controller::Get().GetAssetPathName(i.parent) + "/";
	}
	else
	{
		Item actualItem = GetServerItem(i.parent);

		if (actualItem != Item())
		{
			if (actualItem.parentFolderID > 0)
			{
				return CachedPathFromID(actualItem.parentFolderID) + NicifyAssetName(actualItem.name) + "/";
			}
			else
			{
				string restoredPath = RestoreDeletedAssetPath(actualItem);
				return restoredPath.empty() ? restoredPath : restoredPath + NicifyAssetName(actualItem.name) + "/";
			}
		}
		else
		{
			// ProjectSettings folder does not have a GUID
			string name = Controller::Get().GetAssetPathName(i.guid);
			if (name.substr (0, libraryFolder.size ()) == libraryFolder)
			{
				return "../" + libraryFolder;
			}

			return ""; // item was in root folder, or we just don't have info about it in db... for some reason
		}
	}
}

string Configuration::RestoreDeletedAssetPath (Item &i)
{
	if (GetGUIDPersistentManager().IsConstantGUID(i.parent))
	{
		return Controller::Get().GetAssetPathName(i.parent) + "/";
	}
	else
	{
		if (i.parent == kTrashGUID) // asset "parent" field does not contain info about parent folder, need to traverse database
		{
			// look at previous changeset
			Item actualItem = GetAncestor(i, true);

			if (actualItem != Item())
				return RestoreDeletedAssetPathRestoreParent(actualItem);
			else 
				return "";
		}
		else if (i.guid == kTrashGUID)
		{
			return "";
		}

		return RestoreDeletedAssetPathRestoreParent(i);
	}
}

int Configuration::GetParentFolderID(const Item &i)
{
	Item nonConstItem = i; // FIXME:
	string folder = RestoreDeletedAssetPath(nonConstItem);
	int id;

	SQLite::StatementHandle st(m_DB, "select id from parentfolders where folder = ?1" ); // FIXME: getcachedstatement gets me to sqlite errors
	FatalIfSQLError(st);

	st.Bind(1, folder);
	FatalIfSQLError(st);

	if (st.ExecuteStep())
	{
		// folder already existed
		id = st.GetIntColumn(0);
	}
	else
	{
		// is a new folder
		SQLite::StatementHandle sti(m_DB, "insert into parentfolders (folder) values(?)" );
		FatalIfSQLError(sti);

		sti.Bind(1, folder);
		FatalIfSQLError(sti);

		sti.ExecuteStep();
		FatalIfSQLError(sti);

		id = m_DB->GetLastRowID();

		m_CachedPaths.push_back(folder);
	}

	return id;
}

void Configuration::GetCachedPaths()
{
	static string assetsFolder = "Assets";
	static std::string empty;

	m_CachedPaths.clear();

	SQLite::StatementHandle st(m_DB, "select folder from parentfolders" );
	FatalIfSQLError(st);

	while ( st.ExecuteStep() ) 
	{
		string path = st.GetTextColumn(0);

		if (path.substr (0, assetsFolder.size()) == assetsFolder)
		{
			if (path.size() != assetsFolder.size())
				path.erase(0, assetsFolder.size() + 1);
			else
				path = empty;
		}

		m_CachedPaths.push_back(path);
	}

	FatalIfSQLError(st);
}

const string& Configuration::CachedPathFromID(int pathID)
{
	if (m_CachedPaths.size() <= 1) // if AS is setup, this will always have at least Assets and Trash, if it's not we won't get this far
		GetCachedPaths();

	if( pathID < 1 || m_CachedPaths.size() < pathID ) {
		static std::string empty;
		return empty;
	} else {
		return m_CachedPaths[pathID - 1];
	}
}

void Configuration::SetupCachedParents(int changeset, const vector<Item>& assets, bool files)
{
	for (vector<Item>::const_iterator i = assets.begin(); i != assets.end() ; i++)
	{
		if ((i->type == kTDir) && !files || (i->type != kTDir) && files)
		{
			SQLite::StatementHandle update_st = GetCachedStatementHandle("update assetversion set parentfolderid=?1 where changeset=?2 and guid=?3");
			FatalIfSQLError(update_st);

			update_st.Bind(1,(SInt32)GetParentFolderID(*i));
			update_st.Bind(2, (SInt32)changeset);
			update_st.Bind(3, i->guid);
			update_st.ExecuteStep();
			FatalIfSQLError(update_st);
		}
	}
}

void Configuration::Add(const Changeset& changeset, const vector<Item>& assets) {

	BeginExclusive();
	{
		SQLite::StatementHandle items_st = GetCachedStatementHandle( 
			"insert into assetversion(changeset, guid, name, parent, assettype, digest, oldversion) values(?,?,?,?,?,?,?) ");
		FatalIfSQLError(items_st);
		items_st.Bind(1,(SInt32)changeset.changeset);
		for(vector<Item>::const_iterator i= assets.begin(); i != assets.end() ; i++) {
			items_st.Bind(2, i->guid);
			items_st.Bind(3, i->name);
			items_st.Bind(4, i->parent);
			items_st.Bind(5, (SInt32)i->type);
			items_st.Bind(6, i->digest);
			items_st.Bind(7, (SInt32)i->oldVersion);
			items_st.ExecuteStep();
			FatalIfSQLError(items_st);
			items_st.Reset();
		}
		SQLite::StatementHandle changeset_st = GetCachedStatementHandle("insert into changeset(id, message, date_time, owner) values(?, ?, ?, ?) ");
		FatalIfSQLError(changeset_st);
		changeset_st.Bind(1,(SInt32)changeset.changeset);
		changeset_st.Bind(2,changeset.message);
		changeset_st.Bind(3, (SInt32)changeset.date);
		changeset_st.Bind(4,changeset.owner);
		changeset_st.ExecuteStep();
		FatalIfSQLError(changeset_st);

		// set parent folder IDs for items
		// can't do this when inserting items, because their parents might be in the same changeset, and they don't come sorted.

		// first setup parents for folders, because there generally are many more files then folders, 
		// and looking for file parents will be hitting on those folders. That means way less round trips to the database
		SetupCachedParents(changeset.changeset, assets, false);
		SetupCachedParents(changeset.changeset, assets, true);
	}

	Commit();
}

SQLite::StatementHandle Configuration::GetCachedStatementHandle(const string& querystring) {
	map<string,SQLite::StatementHandle*>::iterator found = m_CachedStatementHandles.find(querystring);
	if ( found != m_CachedStatementHandles.end() )
		return *(found->second);
	
	
	return *(m_CachedStatementHandles[querystring]=new SQLite::StatementHandle(m_DB, querystring));
}

void Configuration::BeginExclusive() {
	SQLite::StatementHandle st = GetCachedStatementHandle("begin exclusive");
	st.ExecuteStep();
	st.Reset();
	FatalIfSQLError(st);
}
void Configuration::Rollback() {
	SQLite::StatementHandle st = GetCachedStatementHandle("rollback");
	st.ExecuteStep();
	st.Reset();
	FatalIfSQLError(st);
}
void Configuration::Commit() {
	SQLite::StatementHandle st = GetCachedStatementHandle("commit");
	st.ExecuteStep();
	st.Reset();
	FatalIfSQLError(st);
}

void* Configuration::GetUserConnectionForDatabase(const string& server, const string& user, const string& password, const string& port, const string& databaseName, bool reportErrors)
{
	#if ENABLE_ASSET_SERVER

	PGconn* connection= NULL;

	string connectionString = "host='" + server + "' user='" + user + "' password='" + password + 
		"' dbname='" + databaseName + "' port='" + port + "' sslmode=disable";

	Controller::Get().ClearError ();
	connection = PQconnectdb(connectionString.c_str());

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(connection) == CONNECTION_OK)
		return connection;

	if (reportErrors)
	{
		const char* error = PQerrorMessage(connection);

		if(strstr(error, "FATAL: ") == error)
			error += 8;

		string err = error;
		Controller::Get().SetConnectionError ("Connection failed\n" + err);
	}

	PQfinish(connection);

	#endif // ENABLE_ASSET_SERVER

	return NULL; 
}

string Configuration::GetDatabaseName(const string& server, const string& user, const string& password, const string& port, const string& projectName)
{
	#if ENABLE_ASSET_SERVER

	// Try to connect to postgres database on the server and get the database name from the project name
	PGconn	   *conn;
	PGresult   *res;
	ExecStatusType status;
	string databaseName;

	Controller::Get().ClearError ();
	conn = (PGconn*)GetUserConnectionForDatabase(server, user, password, port, "postgres", true);

	if (conn)
	{
		const char* values[1];
		values[0] = projectName.c_str();

		res = PQexecParams(conn,  "select db_name($1)", 1, NULL, values, NULL, NULL, 0);
		status  = PQresultStatus(res);

		if ( status <= PGRES_TUPLES_OK )
			databaseName = PQgetvalue(res, 0, 0);

		PQclear(res);
		PQfinish(conn);
	}
	else 
		return string();

	// now reconnect to the acutal database to verify it exists
	conn = (PGconn*)GetUserConnectionForDatabase(server, user, password, port, databaseName, true);

	if	(!conn) 
	{
		// Try to fix up some common error messages to look nicer
		string error = Controller::Get().GetErrorString();
		printf_console("%s", error.c_str());
		if( error.find( "database \""+databaseName+"\" does not exist") != string::npos )
			Controller::Get().SetConnectionError ("Could not locate project " + projectName + " on database server.");
		else if ( error.find( "not have CONNECT privilege" ) != string::npos )
			Controller::Get().SetConnectionError ("User " + user + " is not allowed to access " + projectName);
		return string();
	}

	PQfinish(conn);

	return databaseName;

	#else
	return string();
	#endif // ENABLE_ASSET_SERVER
}

std::string AssetServer::NicifyAssetName(const std::string& name)
{
	// removes " (guid)" or " (DEL_guid)" from end of the name
	size_t pos = name.rfind(" (");
	if( pos == string::npos )
		return name;

	if( name[name.size()-1] != ')' )
		return name;

	size_t guidIndex = pos + 2;
	if( name.find( "DEL_", guidIndex ) == guidIndex )
		guidIndex += 4;
	if( name.find_first_not_of("0123456789abcdef", guidIndex) != name.size()-1 )
		return name;

	std::string res = name;
	res.erase( pos );
	return res;
}
