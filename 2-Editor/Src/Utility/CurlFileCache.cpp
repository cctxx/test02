/*
 *  CurlFileCache.cpp
 *
 */

#include "CurlFileCache.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "External/sqlite/SQLite.h"
#include <curl/curl.h>
#include <time.h>
#include <vector>

static SQLite* GetDatabase() ;
const string kDatabaseName = "CurlRequestCache.db";
const int kSchemaVersion=10;

const SInt32 kSizeHighWater=100 * 1024 * 1024;
const SInt32 kSizeLowWater=80 * 1024 * 1024;
const SInt32 kMaxItemSize=1024 * 1024;

static const char* kSchema[] = {
			"PRAGMA foreign_keys = ON;",
			"create table if not exists curl_cache_response ( "
			"	id integer primary key autoincrement not null, "
			"	url text unique not null, "
			"	content_size integer not null default(0), "
			"	time_stamp not null default current_timestamp, "
			"	expires not null default current_timestamp "
			");",
			"create table if not exists curl_cache_response_data ( "
			"	response_id integer primary key not null, "
			"	response_data blob not null, "
			"	foreign key(response_id) references curl_cache_response(id) on delete cascade"
			");",
			"create table if not exists curl_cache_response_header ( "
			"	id integer primary key autoincrement unique,"
			"	response_id integer not null,"
			"	header_value text not null, "
			"	foreign key(response_id) references curl_cache_response(id) on delete cascade"
			");",
		    "CREATE INDEX IF NOT EXISTS curl_cache_response_header_reponse_id_idx ON curl_cache_response_header(response_id);",
		    "CREATE INDEX IF NOT EXISTS curl_cache_response_data_reponse_id_idx ON curl_cache_response_data(response_id);",
			NULL
};

CurlFileCache *CurlFileCache::s_SingletonInstance = NULL;

CurlFileCache::CurlFileCache() {
	m_Db=GetDatabase();
}

CurlFileCache& CurlFileCache::Instance()
{
	if ( s_SingletonInstance == NULL ) 
	{
		s_SingletonInstance = new CurlFileCache();
	}
	return *s_SingletonInstance;
}

static bool HeaderMatch(const string& header, const string& name, string& out_value)
{
	int header_size = header.size(); 
	int value_start = name.size();
	
	if (header_size <= value_start)
		return false;
		
	if ( StrNICmp(header.c_str(), name.c_str(), value_start) == 0 )
	{
		if (header[value_start] != ':' )
			return false;
		value_start++;
		
		while (value_start < header_size && header[value_start] == ' ')
			value_start++;
		
		out_value = header.substr(value_start);
		return ! out_value.empty();
	}
	return false;
} 

CurlCacheResults::CurlCacheResults(const string& url)
	: m_Url(url)
	, m_ResponseID(-1)
	, m_TimeStamp(-1)
	, m_MaxAge(-1)
	, m_ValidationHeaders()
	, m_ResponseHeaders()
	, m_AllowCache(true)
{
}

int CurlCacheResults::GetAge()
{
	return time(NULL)-m_TimeStamp;
}

bool CurlCacheResults::Fetch()
{
	return CurlFileCache::Instance().Fetch(this);
}

void CurlCacheResults::Store(const std::string& response)
{
	if (m_AllowCache && response.size() <= kMaxItemSize)
		CurlFileCache::Instance().Store(this, response);
	else 
		Remove();
}

void CurlCacheResults::Touch()
{
	if (IsCached())
		CurlFileCache::Instance().Touch(this);
}

void CurlCacheResults::Remove()
{
	if (IsCached())
		CurlFileCache::Instance().Remove(this);
}


string CurlCacheResults::GetResponseData()
{
	if (IsCached())
		return CurlFileCache::Instance().GetResponseData(this);
	else
		return string();
}

void CurlCacheResults::ClearResponseHeaders()
{
	m_ResponseHeaders.clear();
}

void CurlCacheResults::AddResponseHeader(const string& header, bool max_age_is_known)
{
	string value;
	if (HeaderMatch(header, "ETag", value))
	{
		m_ValidationHeaders.push_back("If-None-Match: " + value); 
	}
	else if (HeaderMatch(header, "Last-Modified", value) )
	{
		m_ValidationHeaders.push_back("If-Modified-Since: " + value);
	}
	// Only use Expires if CacheControl: max-age is not defined
	else if (!max_age_is_known && m_MaxAge == -1 && HeaderMatch(header, "Expires", value) )
	{
		m_MaxAge=curl_getdate(value.c_str(),NULL)-time(NULL);
	}
	else if (!max_age_is_known && HeaderMatch(header, "Cache-Control", value) )
	{
		size_t found = value.find("max-age=");
		if (found!=string::npos)
		{
			m_MaxAge=StringToInt(value.substr(found+8));
		}
		if ( value.find("no-cache") != string::npos || value.find("no-store") != string::npos )
		{
			m_AllowCache=false;
		}
	}
	else if (HeaderMatch(header, "Pragma", value) )
	{
		if (value == "no-cache") 
		{
			m_AllowCache=false;
		}
	}
	
	m_ResponseHeaders.push_back(header);
}

void CurlCacheResults::SetResponseHeaders(const std::vector<string>& headers)
{
	ClearResponseHeaders();
	for ( std::vector<string>::const_iterator i = headers.begin(); i != headers.end() ; i++ )
		AddResponseHeader(*i, false);
}

std::pair<std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator> CurlCacheResults::GetResponseHeaders()
{
	return make_pair(m_ResponseHeaders.begin(), m_ResponseHeaders.end());
}

std::pair<std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator> CurlCacheResults::GetValidationHeaders()
{
	return make_pair(m_ValidationHeaders.begin(), m_ValidationHeaders.end());
}


bool CurlFileCache::Fetch(CurlCacheResults* res)
{
	SQLite::StatementHandle data(m_Db, "select id, strftime('%s',datetime(time_stamp)) ts,  strftime('%s',datetime(expires))-strftime('%s',datetime(time_stamp)) max_age from curl_cache_response where url = ?1");
	SQLite::StatementHandle headers(m_Db, "select header_value from curl_cache_response_header where response_id = ?1 order by id asc");
	res->m_ResponseID = -1;
	
	data.Bind(1, res->m_Url);
	if (data.ExecuteStep() ) 
	{
		res->m_ResponseID = data.GetIntColumn(0);
		res->m_TimeStamp = data.GetIntColumn(1);
		res->m_MaxAge = data.GetIntColumn(2);
	}
	if(! data.ResultOK() )
		LogString(data.GetErrorString());

	if ( res->m_ResponseID == -1 )
		return false;
	
	res->ClearResponseHeaders();
	res->m_ValidationHeaders.clear();
	headers.Bind(1, res->m_ResponseID);
	while (headers.ExecuteStep())
	{
		string header = headers.GetTextColumn(0);
		res->AddResponseHeader(header, true);
	}
	
	return true;
}

string CurlFileCache::GetResponseData(CurlCacheResults* res)
{
	SQLite::StatementHandle data(m_Db, "select response_data from curl_cache_response_data where response_id = ?1");
	data.Bind(1, res->m_ResponseID);
	if(data.ExecuteStep()){
		return data.GetBlobColumn(0);
	}
	if(! data.ResultOK() )
		LogString(data.GetErrorString());
	return string();
}

void CurlFileCache::Store(CurlCacheResults* res, const std::string& response)
{
	SQLite::StatementHandle begin(m_Db, "begin transaction");
	SQLite::StatementHandle commit(m_Db, "commit transaction");
	SQLite::StatementHandle rollback(m_Db, "rollback transaction");
	SQLite::StatementHandle entry(m_Db,  
		res->m_ResponseID == -1 ? 
			"insert into curl_cache_response (url, expires, content_size, time_stamp) values (?1, (datetime(strftime('%s','now') + ?2, 'unixepoch')), ?3, datetime('now'))" :
			"update curl_cache_response set expires = (datetime(strftime('%s','now') + ?2, 'unixepoch')), content_size=?3, time_stamp = datetime('now') where id = ?4 and url =?1"
	);
	SQLite::StatementHandle data(m_Db, "replace into curl_cache_response_data(response_id, response_data) values(?1, ?2)");
	SQLite::StatementHandle delete_headers(m_Db, "delete from curl_cache_response_header where response_id = ?1");
	SQLite::StatementHandle insert_header(m_Db, "insert into curl_cache_response_header (response_id, header_value) values (?1, ?2)");

	begin.ExecuteStep();
	
	if(! begin.ResultOK() )
	{
		LogString(begin.GetErrorString());
		return;
	}

	entry.Bind(1, res->m_Url);
	entry.Bind(2, res->m_MaxAge);
	entry.Bind(3, (SInt32)response.size());
	if ( res->m_ResponseID != -1 )
		entry.Bind(4, res->m_ResponseID);
	entry.ExecuteStep();
	if(! entry.ResultOK() )
	{
		// The broken constraint condition is actually not an error because two consecutive requests 
		// for the same url have been performed. Just ignore.
		if ( entry.GetErrorCode() != SQLITE_CONSTRAINT )
			LogString(entry.GetErrorString());
		rollback.ExecuteStep();
		return;
	}
	
	if ( res->m_ResponseID == -1 )
	{
		res->m_ResponseID = m_Db->GetLastRowID();
	}
	else if ( m_Db->GetAffectedRowCount() < 1 )
	{
		rollback.ExecuteStep();
		return;
	}

	data.Bind(1,  res->m_ResponseID);
	data.BindBlob(2, response);
	data.ExecuteStep();
	if(! data.ResultOK() ) 
	{
		LogString(data.GetErrorString());
		rollback.ExecuteStep();
		return;
	}
	
	delete_headers.Bind(1,  res->m_ResponseID);
	delete_headers.ExecuteStep();
	if(! delete_headers.ResultOK() ) 
	{
		LogString(delete_headers.GetErrorString());
		return;
	}
	
	
	for ( std::vector<std::string>::iterator i = res->m_ResponseHeaders.begin(); i != res->m_ResponseHeaders.end() ; i++ )
	{
		insert_header.Bind(1, res->m_ResponseID);
		insert_header.Bind(2, *i);
		insert_header.ExecuteStep();
		if(! insert_header.ResultOK() ) 
		{
			LogString(insert_header.GetErrorString());
			rollback.ExecuteStep();
			return ;
		}
		insert_header.Reset();
		
	}
	
	commit.ExecuteStep();
}

void CurlFileCache::Touch(CurlCacheResults* res)
{
	SQLite::StatementHandle touch(m_Db, "update  curl_cache_response set time_stamp=datetime('now'), expires = (datetime(strftime('%s','now') + ?2, 'unixepoch')) where id=?1");

	touch.Bind(1, res->m_ResponseID);
	touch.Bind(2, res->m_MaxAge);
	touch.ExecuteStep();
	if(! touch.ResultOK() )
		LogString(touch.GetErrorString());
}

void CurlFileCache::Remove(CurlCacheResults* res)
{
	SQLite::StatementHandle rm(m_Db, "delete from curl_cache_response where id=?1");

	rm.Bind(1, res->m_ResponseID);
	rm.ExecuteStep();
	if(! rm.ResultOK() )
		LogString(rm.GetErrorString());
}

int CurlFileCache::GetSize()
{
	SQLite::StatementHandle data(m_Db, "select sum(content_size) from curl_cache_response");
	if(data.ExecuteStep()){
		data.GetIntColumn(0);
	}
	if(! data.ResultOK() )
		LogString(data.GetErrorString());
	return 0;
}

void CurlFileCache::Cleanup()
{
	if ( GetSize() > kSizeHighWater )
	{
		/*
			Following query finds the oldest expires that if we delete all time stamps older or equal to it,
			the size of the remaining records will be less than kSizeLowWater and then performs the deletion.
		*/
		SQLite::StatementHandle rm(m_Db, "delete from curl_cache_response where expires <=  "
			"(select a.expires from curl_cache_response a, curl_cache_response b "
			" where b.expires > a.expires group by a.id,a.expires having sum(b.content_size) < ?1 "
			" order by sum(b.content_size) desc, a.expires asc limit 1) ");
		rm.Bind(1, kSizeLowWater );
		rm.ExecuteStep();
		if(! rm.ResultOK() || m_Db->GetAffectedRowCount() == 0 ) {
			// In case the above query failed to remove any entries, we clear the cache completely
			Clear();
		}		
	}
}

void CurlFileCache::Clear()
{	
	SQLite::StatementHandle all(m_Db, "delete from curl_cache_response");
	all.ExecuteStep();
	if(! all.ResultOK() )
		LogString(all.GetErrorString());
}

static string GetDatabasePath() {
	string dir=GetUserAppCacheFolder();
	CreateDirectory( dir );
	return AppendPathName(dir,kDatabaseName);
}

static SQLite* GetDatabase() {
	SQLite* result = NULL;
	GetOrCreateSQLiteDatabase( &result, GetDatabasePath(), kSchema, kSchemaVersion);
	return result;
}

void CurlFileCacheCleanup() {
	if ( IsFileCreated(GetDatabasePath()) )
	{
		CurlFileCache::Instance().Cleanup();
	} 
}

