#ifndef ASBACKEND_H
#define ASBACKEND_H

#include "Runtime/Utilities/GUID.h"
#include "PgConn.h"
#include "PgResult.h"

#include "Editor/Src/AssetServer/ASController.h"

#include <string>
#include <vector>


namespace AssetServer {

// The backend class handles the connection with the asset server.
// It knows nothing about how to resolve conflicts or how to to add the downloaded assets to the project.
// Responsibilities:
//
//	1) Syncronize local configuration with configuration on server. (SA: AssetServer::Configuration)
//	2) Commit new changesets to server, uploading file contents from file paths given to it.
//	3) Dowload assets to temp files given a changeset number and asset guid (bulk or individual assets)
//
class Backend {

public:
	
	Backend(string connectionString, int timeout, int logID, bool leaveTempFiles = false, string variant="work") 
		: m_ConnectionString(connectionString)
		, m_MaxTimeout(timeout)
		, m_LogID(logID)
		, m_Variant(variant)
		, m_LeaveTempFiles(leaveTempFiles)		
	{
	}
	~Backend();
	
	bool UpdateConfiguration(Configuration& configuration, bool showProgress = true);
	bool CommitChangeset(const vector<UploadItem>& items, const string& message, int* outChangeset=NULL, ProgressCallback* progress=NULL );
	bool DownloadAssets(vector<DownloadItem>& items, const string& tempDir, ProgressCallback* progress=NULL);
	int GetLatestServerChangeset ();
private:
	

	string m_ConnectionString;
	string m_Variant;
	float m_SchemaVersion;
	int m_LogID;
	int m_MaxTimeout;
	bool m_LeaveTempFiles;

	bool FancyConnect(PgConn& conn);
	bool FancyGetPgResult(PgConn& conn, PgResult* result);
	bool UploadAsset(PgConn& conn, const UploadItem& item, map<string, UInt32>* streamOids);
	string StartAndBindChangeset(PgConn& conn, const string& changesetDescription, int* changeset, bool send_client_version=true);
	bool PrepConfigurationForUpload(PgConn& conn, const set<int>& assetversions);
	bool FreezeChangeset(PgConn& conn, int changeset, const vector<UploadItem>& items);
	bool MakeAsset(PgConn& conn, const UnityGUID& guid);
	bool AddAssetVersion(PgConn& conn, const UploadItem& item, int changeset, int derivesFrom, map<UnityGUID, int>* updatedVersions);
	int GetAssetVersion(PgConn& conn, const UnityGUID& guid, int changeset);

	bool DownloadAssetsIMPLv10(vector<DownloadItem>& items, const string& tempDir, ProgressCallback* progress=NULL);
	bool DownloadAssetsIMPLv20(vector<DownloadItem>& items, const string& tempDir, ProgressCallback* progress=NULL);

	bool GetServerSchemaVersion(PgConn& conn, bool showOldSchemaWarning);

	// save these for proper cleanup in case of canceling download
	PgConn m_Conn;
	map<UInt32,string> m_Downloads; 
};

}
#endif
