#include "UnityPrefix.h"
#include "ASBackend.h"

#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Configuration/UnityConfigureRevision.h"
#include "Runtime/Input/TimeManager.h"
#include "Editor/Src/AssetServer/ASConfiguration.h"
#include "Editor/Src/AssetServer/ASController.h"

#if ENABLE_ASSET_SERVER
#include "External/postgresql/include/libpq-fe.h"
#include "External/postgresql/include/libpq/libpq-fs.h"
#endif

//#include <unistd.h>
#include <set>
#include <string>
#include <vector>

#define MaintError(x) { Controller::Get().SetError(x); const string& tmp=x; DebugStringToFile (tmp, 0, __FILE__, __LINE__, kLog | kAssetImportError, 0, m_LogID );}
#define MaintWarning(x) { const string& tmp=x; DebugStringToFile (tmp, 0, __FILE__, __LINE__, kLog | kAssetImportWarning, 0, m_LogID );}

#define RETURN_ERROR( error ) { MaintError(CleanErrorString(error)); return false; }
#define RETURN_OK return true
#define RETURN_FAIL return false
#define RETURN_ERROR_CLOSE_PBAR( error ) { if (showProgress) {ClearProgressbar();} RETURN_ERROR( error ); }
#define RETURN_ERROR_INT( error ) { MaintError(CleanErrorString(error)); return -1; }


using namespace std;
using namespace AssetServer;

static const string kEmpty;

static string CleanErrorString(const string& error) {
	if( error.find("rights to check into current variant") != string::npos || error.find("person") != string::npos && error.find("nonexistant") != string::npos) 
		return "You don't have commit permissions for this project";
	int pos = error.find("CONTEXT:");
	if (pos != string::npos) {
		int end = error.rfind("\n",pos);
		string niceError = error.substr(0,end);
		printf_console("%s\n",error.substr(pos).c_str());
		return niceError;
	}
	return error;
}

static string TempFilename(const string& tempDir, const UnityGUID& guid, int version, const string& stream) {
	#if UNITY_OSX
	if ("assetRsrc" == stream) 
		return AppendPathName(tempDir , string("asset_server.") + GUIDToString(guid) + "v" + IntToString(version) + ".asset/..namedfork/rsrc");
	#endif
	return AppendPathName(tempDir , string("asset_server.") + GUIDToString(guid) + "v" + IntToString(version) + '.' + stream);
}

bool Backend::GetServerSchemaVersion(PgConn& conn, bool showOldSchemaWarning)
{
	const float clientSchemaVersion = 2.0f;

	list<string> argTypes;

	m_SchemaVersion=1.0f;

	// Get schema version
	if(! conn.Prepare("maint_get_schema_version", argTypes , 
		"select schema_version() as version"))
		return false;

	PgParams params;

	if ( conn.SendPrepared("maint_get_schema_version", params) ) {

		PgResult result;
		if ( FancyGetPgResult(conn, &result )) {
			PgColumn versionColumn = result.GetColumn("version");
			m_SchemaVersion=SimpleStringToFloat( versionColumn.GetString(0).c_str() );
		}
	}

	if (showOldSchemaWarning && clientSchemaVersion > m_SchemaVersion)
	{
		MaintWarning(Format("Asset Server client is newer than server. Please upgrade to take advantage of improvements. Supported server version is: %.1f", clientSchemaVersion));
	}

	return true;
}

/**
 * Attempt to connect to server and timeout if nothing happens for too long.
 */
bool Backend::FancyConnect(PgConn& conn) {
	
	const int connectionTimeout = 15;
	if (! conn.DoConnect(m_ConnectionString, connectionTimeout))
		return false;

	if(! conn.Prepare("maint"))
		return false;
	
	list<string> argTypes;

	if (!GetServerSchemaVersion(conn, false))
		return false;
	
	if( m_SchemaVersion < 2.0f ) {
		argTypes.push_back("int[]");
		if(! conn.Prepare("maint_bulk_update_get_streams", argTypes , 
						  "select assetversion,tag,lobj from stream, assetcontents where stream = lobj and assetversion = ANY($1)"))
			return false;
		
		argTypes.clear();
		
		argTypes.push_back("int[]");
		argTypes.push_back("text");
		if(! conn.Prepare("maint_upload_prep_configuration", argTypes , 
				"delete from configuration__matview where for_variant=(select serial from variant where name=$2) and serial = ANY($1)")) {
			return false;
		}

		argTypes.clear();
	}
	
	argTypes.push_back("text");
	argTypes.push_back("int");
	if(! conn.Prepare("maint_get_changesets", argTypes , 
		"select vc.changeset, cs.description as log, (select username from get_person_record(cs.creator)) as owner, extract(epoch from commit_time)::int as date, "
		"		a.serial, guid2hex(a.guid) as guid, av.name, guid2hex(get_asset_guid_safe(av.parent)) as parent, av.assettype, av.digest, av.serial as oldversion "
		"	from variant v, variantinheritance vi, variantcontents vc, changeset cs, changesetcontents cc, assetversion av, asset a "
		"	where v.name = $1 "
		"		and vi.child = v.serial "
		"		and vc.variant = vi.parent "
		"		and cs.serial=vc.changeset "
		"		and cs.serial=cc.changeset "
		"		and cc.assetversion=av.serial "
		"		and av.asset=a.serial "
		"		and vc.changeset > $2"
		"	order by vc.changeset "
	)) {
		return false;
	}

	argTypes.clear();
	argTypes.push_back("text");
	argTypes.push_back("int");
	if(! conn.Prepare("maint_verify_changesets", argTypes , 
		"select count(*)::int as versioncount, count( distinct a.guid)::int as guidcount, "
		"	dateformat_nice(min(commit_time)) as first_commit, dateformat_nice(max(commit_time)) as last_commit "
		"	from variant v, variantinheritance vi, variantcontents vc, changeset cs, changesetcontents cc, "
		"	assetversion av, asset a "
		"	where v.name = $1"
		"		and vi.child = v.serial "
		"		and vc.variant = vi.parent "
		"		and cs.serial=vc.changeset "
		"		and cs.serial=cc.changeset "
		"		and cc.assetversion=av.serial "
		"		and av.asset=a.serial "
		"		and vc.changeset <= $2"
	)) {
		return false;
	}

	
	argTypes.clear();
	
	argTypes.push_back("int");
	argTypes.push_back("character(32)");
	if(! conn.Prepare("maint_get_assetversion_from_changeset_and_guid", argTypes , 
			"select assetversion.serial as assetversion from assetversion, asset where created_in = $1 and asset.serial = assetversion.asset and guid2hex(guid)=$2")) {
		return false;
	}
	argTypes.clear();
		return true;
}

/**
 * Await server result, while trying to update progress bar and timeout if nothing happens for too long.
 */
bool Backend::FancyGetPgResult(PgConn& conn, PgResult* result) {
	// Remember when this started (so we can time out)
	float startTime = GetTimeSinceStartup();

	// While the response hasn't been received
	while (! conn.IsReady()) {

		conn.WaitForRead(30);

		if (! conn.ConnectionOK()) 
		{
			MaintError("Lost connection from maint server");
			return false;
		}
		
		if (GetTimeSinceStartup() - startTime > m_MaxTimeout * 60 ) {
			MaintError("Timed out awaiting result from Asset Server server");
			return false;
		}

	}
	
	*result = conn.GetResult();
	
	conn.ClearQueue();
	return  result->Ok();
}

/**
 * Download an assetversion from the server
 */
bool Backend::DownloadAssets(vector<DownloadItem>& items, const string& tempDir, ProgressCallback* progress)
{
	// Initialize connection
	if (! FancyConnect(m_Conn))
		RETURN_ERROR( m_Conn.ErrorStr() );

	if (! m_Conn.BeginSerializableTransaction())
		RETURN_ERROR( "Failed to start transaction with server: " + m_Conn.ErrorStr() );

	if( m_SchemaVersion > 1.9f ) 
	{
		if (! DownloadAssetsIMPLv20(items, tempDir, progress) )
			RETURN_FAIL;
	}
	else
	{
		if (! DownloadAssetsIMPLv10(items, tempDir, progress) )
			RETURN_FAIL;
	}

	if(! m_Conn.LOExport( m_Downloads, progress ) ) 
		RETURN_ERROR("Failed downloading assets from server: " + m_Conn.ErrorStr());
	
	if (! m_Conn.Rollback())
		RETURN_ERROR( "Failed to send rollback command to server: " + m_Conn.ErrorStr() );

	RETURN_OK;
}
		
bool Backend::DownloadAssetsIMPLv20(vector<DownloadItem>& items, const string& tempDir, ProgressCallback* progress)
{
	PgArray guids;
	PgArray changesets;
	
	for( vector<DownloadItem>::iterator i = items.begin(); i != items.end(); i++) {
		guids.Add(GUIDToString(i->guid));
		changesets.Add(i->changeset);
	}

	PgParams params;
	params.Add(guids);
	params.Add(changesets);

	if (! m_Conn.SendPrepared("maint_update_get_streams_by_guids_changesets", params))
		RETURN_ERROR("Failed to download assets from server: "+ m_Conn.ErrorStr());

	PgResult result;
	if (! FancyGetPgResult(m_Conn, &result ))
		RETURN_ERROR( result.ErrorStr() );

	PgColumn indexCol = result.GetColumn("index");
	PgColumn streamCol = result.GetColumn("tag");
	PgColumn lobjCol = result.GetColumn("lobj");
	
	m_Downloads.clear();
	for (int i = 0; i < result.Rows(); i++) {
		int itemNo = indexCol.GetInt(i);
		string stream = streamCol.GetString(i);
		int lobj = lobjCol.GetInt(i);
		
		string tempFilename = TempFilename(tempDir, items[itemNo].guid, items[itemNo].changeset, stream);
		
		m_Downloads[lobj]=tempFilename;
		items[itemNo].streams[stream]=tempFilename;
	}	


	// check that we get at least a kAssetStream and a kBinMetaStream or kTxtMeta from the server for each GUID
	for( vector<DownloadItem>::iterator i = items.begin(); i != items.end(); i++) {
		if ( i->streams.empty() )
			RETURN_ERROR(Format("Internal error. Could not locate download streams for asset guid %s in changeset %d", GUIDToString(i->guid).c_str(), i->changeset));
	}

	RETURN_OK;
}

// Obsolete version used to support asser server versions prior to Asset Server 2.0
// Slow due to the need to look up asset version from guid and changeset for each asset, which is done with a separate query per asset
bool Backend::DownloadAssetsIMPLv10(vector<DownloadItem>& items, const string& tempDir, ProgressCallback* progress)
{
	PgArray assetversions;
	map<int,DownloadItem* > itemsByAssetVersion;
	
	size_t totalItems = items.size();
	size_t itemsCounter = 0;
	for( vector<DownloadItem>::iterator i = items.begin(); i != items.end(); i++) {
		int assetversion = GetAssetVersion(m_Conn, i->guid, i->changeset);
		if(assetversion < 0 ) 
			RETURN_FAIL;
		itemsByAssetVersion[assetversion] = &(*i); // We update the streams member variable after the download
		assetversions.Add(assetversion);

		++itemsCounter;
		if( progress && (itemsCounter & 15) == 0 ) {
			progress->UpdateProgress( 0, 0, Format("Getting versions (%i/%i)", (int)itemsCounter, (int)totalItems) );
		}
	}

	PgParams params;
	params.Add(assetversions);

	if (! m_Conn.SendPrepared("maint_bulk_update_get_streams", params))
		RETURN_ERROR("Failed to download assets from server: "+ m_Conn.ErrorStr());

	PgResult result;
	if (! FancyGetPgResult(m_Conn, &result ))
		RETURN_ERROR( result.ErrorStr() );

	PgColumn assetVersionCol = result.GetColumn("assetversion");
	PgColumn streamCol = result.GetColumn("tag");
	PgColumn lobjCol = result.GetColumn("lobj");
	
	m_Downloads.clear();
	for (int i = 0; i < result.Rows(); i++) {
		int assetversion = assetVersionCol.GetInt(i);
		string stream = streamCol.GetString(i);
		int lobj = lobjCol.GetInt(i);
		
		string tempFilename = TempFilename(tempDir, itemsByAssetVersion[assetversion]->guid, assetversion, stream);
		
		m_Downloads[lobj]=tempFilename;
		itemsByAssetVersion[assetversion]->streams[stream]=tempFilename;
	}	

	RETURN_OK;
}

Backend::~Backend()
{
	// This is basically to close file open by PgConn (if any). If it remains open next AS operation will fail.
	m_Conn.Cancel();

	if (!m_LeaveTempFiles)
	{
		for (map<UInt32,string>::const_iterator i = m_Downloads.begin(); i != m_Downloads.end(); i++)
		{
			DeleteFile(i->second);
		}
	}
}

bool Backend::UploadAsset(PgConn& conn, const UploadItem& item, map<string, UInt32>* streamOids) {

	for( map<string,string>::const_iterator i=item.newStreams.begin(); i != item.newStreams.end(); i++) {
		(*streamOids)[i->first] = conn.LOImport(i->second);
		if ((*streamOids)[i->first] == 0)
			RETURN_ERROR("Failure while writing " + i->first + " stream for "+item.path+" to server: " + conn.ErrorStr());
	}
 	RETURN_OK;

}

string Backend::StartAndBindChangeset(PgConn& conn, const string& changesetDescription, int* changeset, bool send_client_version) {
	PgParams params;
	params.Add(changesetDescription);
	params.Add(m_Variant);
	
	if(send_client_version)
		params.Add("Unity/" UNITY_VERSION_FULL);
	
	// Create changeset on server
	if (! conn.SendPrepared(send_client_version?"maint_commit_start_changeset2":"maint_commit_start_changeset", params)) 
		return "Failed to send maint_commit_start_changeset request to server (" + conn.ErrorStr() + ")";
	

	PgResult result;
	if (! FancyGetPgResult(conn, &result)) 
		return result.ErrorStr();
	if (! result.HasOneValue())
		return "Server returned incorrect response";

	*changeset = result.GetOneInt();

	return "";
}

// Deletes assetversions that are going to be overwritten by a new version in configuration__matview in order to avoid unique constraint errors
// when  renaming assets to paths that existed in the previous version on the server.
bool Backend::PrepConfigurationForUpload(PgConn& conn, const set<int>& assetversions) {
	
	if(assetversions.empty() || m_SchemaVersion > 1.9)
		RETURN_OK;
		
	PgArray versionsToDelete;
	for(set<int>::const_iterator i = assetversions.begin(); i != assetversions.end(); i++) 
		versionsToDelete.Add(*i);

	PgParams params;
	params.Add(versionsToDelete);
	params.Add(m_Variant);
	
	// Create dirversion on server
	if (! conn.SendPrepared("maint_upload_prep_configuration", params))
		RETURN_ERROR("Failed to send maint_commit_prep_matview request to server (" + conn.ErrorStr() + ")");

	PgResult result;
	if (! FancyGetPgResult(conn, &result))
		RETURN_ERROR( result.ErrorStr() );

	RETURN_OK;
}
 

bool Backend::FreezeChangeset(PgConn& conn, int changeset, const vector<UploadItem>& items) 
{
	PgParams params;
	params.Add(changeset);
	
	// Create dirversion on server
	conn.ClearQueue();
	if (! conn.SendPrepared("maint_commit_freeze_changeset", params))
		RETURN_ERROR( "Could not upload assets. Failed to send maint_commit_freeze_changeset request to server (" + conn.ErrorStr() + ")" );

	PgResult result;
	if (! FancyGetPgResult(conn, &result)) 
	{
		string error = result.ErrorStr() ;
		if( error.find( "unique_names_in_dirs" ) != string::npos ) 
		{
			RETURN_ERROR ( "Could not upload assets. These assets are on the server with the same path name, but different GUID:" + Configuration::Get().GetDuplicateNames(items));
		}
		else 
		{
			RETURN_ERROR( Format("Could not upload assets. Got an SQL error while uploading assets: %s ",  error.c_str() ) );
		}
	}

	RETURN_OK;
}

bool Backend::MakeAsset(PgConn& conn, const UnityGUID& guid) {
	PgParams params;
	params.Add(GUIDToString(guid));
	
	// Create asset on server
	if (! conn.SendPrepared("maint_commit_make_asset", params))
		RETURN_ERROR( "Failed to send maint_commit_make_asset request to server (" + conn.ErrorStr() + ")" );
	PgResult result;
	if (! FancyGetPgResult(conn, &result))
		RETURN_ERROR( result.ErrorStr() );
	if (! result.HasOneValue())
		RETURN_ERROR( "Server returned incorrect response" );

	RETURN_OK;		
}

bool Backend::AddAssetVersion(PgConn& conn, const UploadItem& item, int changeset, int derivesFrom, map<UnityGUID, int>* updatedVersions) {

	string assetPath = item.path;
	bool isFileAsset = item.type != kTDir;

	// Prepare parameters
	PgParams params;
	params.Add(GUIDToString(item.guid));
	params.Add(GUIDToString(item.parent));
	params.Add(item.name);
	params.Add(changeset);
	params.Add(m_Variant);


	PgArray streamsAry;
	PgArray oidsAry;
	
	bool reuse=false;
	
	// Upload file streams
	if(isFileAsset) {
		map<string, UInt32> streamOids;
		if(! item.reuseStreams ) {
			if ( item.newStreams.empty() )
				RETURN_ERROR("Internal consistency error. File asset must contain streams or reuse previous version");
			if(! UploadAsset(conn, item, &streamOids) ) // upload files and create new file streams
				RETURN_FAIL;
		}
		else { // reuse latest version
			reuse = true;
		}

		params.Add("text"); //\TODO: nuance this later

		MdFour digest = item.digest;
		params.AddBinary(string(digest, sizeof(digest)));

		if(! reuse) {
			for (map<string, UInt32>::iterator i = streamOids.begin(); i != streamOids.end(); i++) {
				streamsAry.Add(i->first);
				oidsAry.Add(IntToString(i->second));
			}
			params.Add(streamsAry);
			params.Add(oidsAry);
		}
	}
	
	if(reuse) {
		params.Add(derivesFrom);
	}
	else {
		PgArray derivesFromAry;
		derivesFromAry.Add(IntToString(derivesFrom));
		params.Add(derivesFromAry);
	}
	
	// Create fileversion on server
	if (! conn.SendPrepared(isFileAsset?(reuse?"maint_commit_add_fileversion_reuse":"maint_commit_add_fileversion"):"maint_commit_add_dirversion", params))
		RETURN_ERROR( Format("Could not upload asset %s. Internal error. Failed to send %s request to server (%s)",
			assetPath.c_str(), (isFileAsset?"maint_commit_add_fileversion":"maint_commit_add_dirversion"), conn.ErrorStr().c_str()));

	PgResult result;
	if (! FancyGetPgResult(conn, &result)) {
		string error = result.ErrorStr() ;
		if( error.find( "unique_names_in_dirs" ) != string::npos ) {
			RETURN_ERROR ( Format("Could not upload %s. There is another asset on the server with the same name.", assetPath.c_str()));
		}
		else if( error.find( "parent is not already" ) != string::npos ) {
			RETURN_ERROR ( Format("Could not upload %s. The parent folder does not exist on the server.", assetPath.c_str()));
		}
		else if ( error.find( "ERROR:" ) == 0 ){
			error.erase(0,7);
			RETURN_ERROR( Format("Could not upload %s. %s",  assetPath.c_str(), error.c_str() ) );
		}
		else {
			RETURN_ERROR( Format("Got an SQL error while uploading %s. (%s)",  assetPath.c_str(), error.c_str() ) );
		}
	}
	if (! result.HasOneValue())
		RETURN_ERROR(  Format("Could not upload asset %s. Internal error. Server returned incorrect response", assetPath.c_str()) );

	(*updatedVersions)[item.guid] = result.GetOneInt();

	RETURN_OK;
}

/**
 * Quick way to peek if there are updates on server
 */

int Backend::GetLatestServerChangeset ()
{
	static bool checkedForOldSchema = false;

	PgConn conn;
	PgParams params;
	const int connectionTimeout = 15;

	if (!conn.DoConnect(m_ConnectionString, connectionTimeout))
		RETURN_ERROR_INT(conn.ErrorStr());

	if (!checkedForOldSchema)
	{
		GetServerSchemaVersion(conn, true);
		checkedForOldSchema = true;
	}

	if(!conn.Prepare("maint_get_latest_changeset", list<string>(), 
		"select max(changeset) from variantcontents, variantinheritance where variant = parent and child =  (select serial from variant where name = '" + m_Variant + "')"))
		RETURN_ERROR_INT(conn.ErrorStr());

	if (!conn.SendPrepared("maint_get_latest_changeset", params))
		RETURN_ERROR("Failed to get latest changeset from server: " + conn.ErrorStr());

	PgResult result;
	if (! FancyGetPgResult(conn, &result)) 
		RETURN_ERROR(result.ErrorStr());
	if (! result.HasOneValue())
		RETURN_ERROR("Server returned incorrect response");

	return result.GetOneInt();
}

bool Backend::UpdateConfiguration( Configuration& conf, bool showProgress )
{
	if (showProgress)
	{
		DisplayProgressbar("Updating history", "Connecting to server to download project history...", 0);
	}

	// Initialize connection
	PgConn conn;
	if (! FancyConnect(conn))
		RETURN_ERROR_CLOSE_PBAR( conn.ErrorStr());
	

	{
		if (showProgress)
		{
			DisplayProgressbar("Updating history", "Checking for changes on server...", 0);
		}

		Configuration::Signature sig = conf.GetSignature();
		
		PgResult result;
		// Params: Variant, last changeset
		PgParams params;
		params.Add(m_Variant);
		params.Add(sig.lastChangeset);

		if (! conn.SendPrepared("maint_verify_changesets", params))
				RETURN_ERROR_CLOSE_PBAR( "Failed to send command to server (" + conn.ErrorStr() + ")");
		if (! FancyGetPgResult(conn, &result))
			RETURN_ERROR_CLOSE_PBAR( "Unexpected error from server: " + result.ErrorStr());
		if( ! result.Rows() == 1 )
			RETURN_ERROR_CLOSE_PBAR("Unexpected number of results from query");
		PgColumn assetVersionCountCol = result.GetColumn("versioncount");
		PgColumn guidCountCol = result.GetColumn("guidcount");
		int assetVersionCount = assetVersionCountCol.GetInt(0);
		int guidCount = guidCountCol.GetInt(0);
		// If the signature doesn't match, refetch all changesets from server
		if( assetVersionCount != sig.assetVersionCount || guidCount != sig.guidCount ) {
			conf.Clear();
			params.Clear();
			params.Add(m_Variant);
			params.Add(0); 
		}
		if (! conn.SendPrepared("maint_get_changesets", params))
				RETURN_ERROR_CLOSE_PBAR( "Failed to send command to server (" + conn.ErrorStr() + ")");
		
		AssetServer::Changeset changeset;
		vector<AssetServer::Item> assets;
		changeset.changeset = -1;
		
		if (! FancyGetPgResult(conn, &result))
			RETURN_ERROR_CLOSE_PBAR( "Unexpected error from server: " + result.ErrorStr());
		
		PgColumn changesetCol = result.GetColumn("changeset");
		PgColumn logCol = result.GetColumn("log");
		PgColumn ownerCol = result.GetColumn("owner");
		PgColumn dateCol = result.GetColumn("date");
		PgColumn guidCol = result.GetColumn("guid");
		PgColumn nameCol = result.GetColumn("name");
		PgColumn parentCol = result.GetColumn("parent");
		PgColumn digestCol = result.GetColumn("digest");
		PgColumn typeCol = result.GetColumn("assettype");
		PgColumn oldVersionCol = result.GetColumn("oldversion");
	
		int rows = result.Rows();
		for (int i = 0; i < rows; i++) {
			if ( showProgress && i % 500 == 0 )
			{
				if (DisplayProgressbar("Updating history", "Downloading and caching history...", (float)i / rows, true) == kPBSWantsToCancel)
				{
					conf.Clear();
					RETURN_ERROR_CLOSE_PBAR("Action canceled");
				}
			}

			MdFour digest(digestCol.GetString(i).c_str());
			AssetServer::Item item(changesetCol.GetInt(i), StringToGUID(guidCol.GetString(i)), nameCol.GetString(i), StringToGUID(parentCol.GetString(i)), 
				(AssetServer::ItemType)typeCol.GetInt(i), digest, AssetServer::kOrServer, oldVersionCol.GetInt(i) );
			if(item.changeset != changeset.changeset) {
				if( changeset.changeset >= 0 ) 
					conf.Add(changeset, assets);
				changeset.changeset=item.changeset;
				changeset.message = logCol.GetString(i);
				changeset.owner = ownerCol.GetString(i);
				changeset.date = dateCol.GetInt(i);
				assets.clear();
			}
			assets.push_back(item);
		}
		if( changeset.changeset >= 0 ) 
				conf.Add(changeset, assets);
	}

	if (showProgress)
	{
		ClearProgressbar();
	}

	RETURN_OK;
}

bool Backend::CommitChangeset(const vector<UploadItem>& items, const string& changesetDescription, int* outChangeset, ProgressCallback* progress ) {
	if (items.size() == 0)
		RETURN_ERROR( "Empty selection" );
	
	// Initialize connection
	if (! FancyConnect(m_Conn))
		RETURN_ERROR( m_Conn.ErrorStr());
	
	if (! m_Conn.BeginSerializableTransaction())
		RETURN_ERROR( "Failed to start transaction with server: " + m_Conn.ErrorStr());

	// Create changeset to work in
	int changeset;
	string error;
	if( (error = StartAndBindChangeset(m_Conn, changesetDescription, &changeset) ) != "" ) {
		 if( error.find ("does not exist") == string::npos )
			RETURN_ERROR(error);
		 m_Conn.Rollback();
		 if (! m_Conn.BeginSerializableTransaction())
			RETURN_ERROR( "Failed to start transaction with server: " + m_Conn.ErrorStr());
		 if( (error = StartAndBindChangeset(m_Conn, changesetDescription, &changeset, false) ) != "" ) 
			RETURN_ERROR(error);

	}
	
	if(outChangeset != NULL)
		*outChangeset=changeset;

	map<UnityGUID, UploadItem> assetsToUpload;
	map<UnityGUID, int> derivesFrom;

	if( m_SchemaVersion > 1.9 )
	{		
		PgArray guids;
		PgArray predecessors;
		PgArray names;
		PgArray parents;
		for (vector<UploadItem>::const_iterator i = items.begin(); i != items.end(); i++) {
			guids.Add(GUIDToString(i->guid));
			predecessors.Add(i->predecessor);
			names.Add(i->name);
			parents.Add(GUIDToString(i->parent));
			assetsToUpload[i->guid] = *i;
		}

		PgParams params;
		params.Add(guids);
		params.Add(predecessors);
		params.Add(names);
		params.Add(parents);
		params.Add(m_Variant);
		if (! m_Conn.SendPrepared("maint_commit_get_derives_from_and_make_assets", params))
				RETURN_ERROR( "Failed to send command to server (" + m_Conn.ErrorStr() + ")");
		
		PgResult result;
		if (! FancyGetPgResult(m_Conn, &result)) {
			string error = result.ErrorStr() ;
			int pos;
			if( (pos = error.find( "unique_names_in_dirs(" )) != string::npos ) {
				UnityGUID guid = StringToGUID(error.substr(pos+21,32));
				AssertIf( assetsToUpload.find(guid) == assetsToUpload.end() );
				RETURN_ERROR ( Format("Could not upload %s. There is another asset on the server with the same path name. Either rename the local asset or update from server and resolve the confilct.", assetsToUpload[guid].path.c_str() ));
			}
			else if ( error.find( "ERROR:" ) == 0 )
			{
				error.erase(0,7);
				RETURN_ERROR( Format("Could not prepare upload. %s",  error.c_str() ) );
			}
			else
			{
				RETURN_ERROR( Format("Got an SQL error while preparing to upload (%s)",   error.c_str() ) );
			}
		}
		else
		{
			
			PgColumn guidCol = result.GetColumn("guid");
			PgColumn derivesFromCol = result.GetColumn("assetversion");
			
			for (int i = 0; i < result.Rows(); i++) 
			{
				UnityGUID guid = StringToGUID(guidCol.GetString(i));
				derivesFrom[guid]=derivesFromCol.GetInt(i);
			}	
			
			
		}
		
		
	}
	else 
	{
		set<int> assetversionsToBeReplaced;
		// Sort through assets and select resolution. Tell server about asset GUID if local only.
		for (vector<UploadItem>::const_iterator i = items.begin(); i != items.end(); i++) {
			// Resolve collected data and add on server
			int assetversion = GetAssetVersion(m_Conn, i->guid, i->predecessor);
			derivesFrom[i->guid]=assetversion;
			if( assetversion <= 0 && ! MakeAsset(m_Conn, i->guid)) {
				RETURN_FAIL;
			}
			else {						
				assetversionsToBeReplaced.insert(assetversion);
			}
		
			assetsToUpload[i->guid] = *i;

		}
	
		// Make sure we don't get conflicts when moving and renaming files to paths that existed as a different asset in the previous changeset
		if( ! PrepConfigurationForUpload(m_Conn, assetversionsToBeReplaced) )
			RETURN_FAIL;
	}

	// Resolve assets so parent directories are added before their children
	vector<UploadItem> sortedAssetsToUpload;
	while (! assetsToUpload.empty() ) {
		for ( map<UnityGUID,UploadItem>::iterator i = assetsToUpload.begin(); i != assetsToUpload.end(); i++ ) {
			if ( assetsToUpload.find(i->second.parent) == assetsToUpload.end() ) {
				sortedAssetsToUpload.push_back(i->second);
				assetsToUpload.erase(i);
				break;
			}
		}
		// TODO: add check that the size of assetsToUpload decreases on every iteration to verify against circular references
	}
	
	map<UnityGUID, int> updatedVersions;

	int c=0;
	for ( vector<UploadItem>::iterator i = sortedAssetsToUpload.begin(); i != sortedAssetsToUpload.end(); i++) {
		c++;
		if(progress)
		{
			progress->UpdateProgress(c, sortedAssetsToUpload.size(), i->name);
			if (progress->ShouldAbort())
			{
				m_Conn.Rollback();
				RETURN_ERROR("Aborted by user");
			}
		}
		
		if( ! AddAssetVersion(m_Conn, *i, changeset, derivesFrom[i->guid], &updatedVersions) )
			RETURN_FAIL;
	}
	
	{
		if(! FreezeChangeset(m_Conn, changeset, items))
		{
			RETURN_FAIL;
		}
	}
	
	
	if (! m_Conn.Commit())
		RETURN_ERROR( "Failed to store changes to server: " + m_Conn.ErrorStr() );
	RETURN_OK;
}

int Backend::GetAssetVersion(PgConn& conn, const UnityGUID& guid, int changeset) {
	int assetversion=-1; // unknown
	
	PgParams params;
	params.Add(changeset);
	params.Add(GUIDToString(guid));
	if (! conn.SendPrepared("maint_get_assetversion_from_changeset_and_guid", params)) {
		MaintError( "Failed to send maint_get_assetversion_from_changeset_and_guid request to server" );
		return -1;
	}
	
	PgResult result;
	if (! FancyGetPgResult(conn, &result))
		RETURN_ERROR( result.ErrorStr() );

	PgColumn assetversionCol = result.GetColumn("assetversion");

	if( result.Rows() > 0) {
		assetversion=assetversionCol.GetInt(0);
	}
	return assetversion;
	
}
