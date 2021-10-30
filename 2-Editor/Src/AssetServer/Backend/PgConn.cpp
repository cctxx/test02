#include "UnityPrefix.h"

#include "PgConn.h"
#if UNITY_OSX
#include <pthread.h>
#endif

#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/File.h"

#ifndef USE_SELECT
#define USE_SELECT 0
#endif

#if UNITY_WIN
#	include <Winsock2.h>
#else
	#if USE_SELECT
	#	include <sys/select.h>
	#else
	#	define POLL_NO_WARN
	#	include <poll.h>
	#endif
	#include <netinet/in.h>
#endif

#include <time.h>

// How many pages to fetch at a time when doing bulk downloads (each page is between 0 and 2K)
static const int kLOBulkFetchSize = 256;
// size of data buffer when importing and exporting individual large objects (The libpq default of 8K is too small)
static const int kLOBufferSize = 65536;

static const int kActivityTimeout = 30000;
static const int kActivityTick = 100;
static const int kCopybufferSize = 4096;
using namespace std;

void AppendEscaped(string* target, const string& src);
void AppendEscaped(string* target, const string& src) {
	int from = 0;
	int to = 0;
	
	while (from < src.size()) {
		to = src.find_first_of("\"\\", from);
		if (to == string::npos) {
			target->append(src, from, src.size() - from);
			break;
		}
		else {
			target->append(src, from, to - from);
			target->append("\\");
			target->append(src, to, 1);
		}
		from = to + 1;
	}
}

string* illegal_characters;

void AppendBinaryEscaped(string* target, const string& src);
void AppendBinaryEscaped(string* target, const string& src) {
	if (! illegal_characters) {
		illegal_characters = new string;
		illegal_characters->push_back('\0');
		illegal_characters->push_back('\\');
		illegal_characters->push_back('"');
		illegal_characters->push_back('\'');
		for (int c = 1; c < 32; c++)
			illegal_characters->push_back((unsigned char) c);
		for (int c = 127; c <= 255; c++)
			illegal_characters->push_back((unsigned char) c);
	}

	int from = 0;
	int to = 0;
	
	while (from < src.size()) {
		to = src.find_first_of(*illegal_characters, from);
		if (to == string::npos) {
			target->append(src, from, src.size() - from);
			break;
		}
		else {
			target->append(src, from, to - from);

			char buf[4];
			buf[3] = '\0';
			sprintf(buf, "%03o", (unsigned int)(unsigned char)src[to]);
				
			target->append("\\\\");
			target->append(buf, 3);
	
		}
		from = to + 1;
	}
}

const string& PgArray::AsString() {
	m_Representation = "[" + IntToString(m_Values.size()) + "]={\"";

	list<string>::iterator i = m_Values.begin();
	if (i != m_Values.end()) {
		AppendEscaped(&m_Representation, *i);
		for (i++; i != m_Values.end(); i++) {
			m_Representation.append("\",\"");
			AppendEscaped(&m_Representation, *i);
		}
	}
	
	m_Representation.append("\"}");
	return m_Representation;
}

const string& PgBinaryArray::AsString() {
	m_Representation = "[" + IntToString(m_Values.size()) + "]={\"";

	list<string>::iterator i = m_Values.begin();
	AppendBinaryEscaped(&m_Representation, *i);
	for (i++; i != m_Values.end(); i++) {
		m_Representation.append("\",\"");
		AppendBinaryEscaped(&m_Representation, *i);
	}
	m_Representation.append("\"}");
	return m_Representation;
}

void PgArrayBase::Add(int value) {
	char buf[255];
	sprintf (buf, "%li", value);
	Add(string(buf));
}


void PgParams::Add(const string& value) {
	m_Values.push_back(value); 
	m_Binary.push_back(0);
}
void PgParams::Add(int value) {
	char buf[255];
	sprintf (buf, "%li", value);
	Add(string(buf));
}
void PgParams::AddBinary(const string& value) {
	m_Values.push_back(value); 
	m_Binary.push_back(1);
}
void PgParams::Add(PgArrayBase& ary) {
	m_Values.push_back(ary.AsString()); 
	m_Binary.push_back(0);
}
void PgParams::Append(const string& value) {
	m_Values.back().append(value);
}
void PgParams::Clear() {
	m_Values.clear(); 
	m_Binary.clear(); 
	m_ValueLengths.clear();
	m_ValuePtrs.clear();
}

const vector<const char*>& PgParams::Values () {
	m_ValuePtrs.clear();
	m_ValuePtrs.reserve(m_Values.size());
	for (list<string>::iterator i = m_Values.begin(); i != m_Values.end(); i++)
		m_ValuePtrs.push_back((*i).c_str());
	return m_ValuePtrs;
}
const vector<int>& PgParams::Lengths () {
	m_ValueLengths.clear();
	m_ValueLengths.reserve(m_Values.size());
	list<string>::iterator valiter = m_Values.begin();	
	for (int i = 0; i != m_Values.size(); i++) {
		if (m_Binary[i] == 1)
			m_ValueLengths.push_back((*valiter).length());
		else
			m_ValueLengths.push_back(0);
		valiter++;
	}
	return m_ValueLengths;
}
const vector<int>& PgParams::Binary () {
	return m_Binary;
}


/*bool PgConn::StartConnect(const string& params) {
	m_Conn = PQconnectStart(params.c_str());

	if (! m_Conn)
		return Error("Failed to create server connection object");
	if (PQstatus(m_Conn) == CONNECTION_BAD)
		return Error("Got error while trying to contact server");
	
	m_ConnectPollStatus = PGRES_POLLING_WRITING;
	return Success();
}

bool PgConn::ConnectSucceeded() {
	if (m_ConnectPollStatus = PQconnectPoll(m_Conn) == PGRES_POLLING_OK)
		return Success();
	else
		return false;
}

*/

struct CachedConn {
	PGconn* conn;
	set<string>* prepared;
	map<string,string>* preparedStatements;
	string* errorstring;
	bool free;
	int lastUsed;

	CachedConn(PGconn* c, set<string>* p, map<string,string>* p2, string* e, bool f) : conn(c), prepared(p), preparedStatements(p2), errorstring(e), free(f), lastUsed(0) {}
};
typedef list<CachedConn> CachedConnList;

static map<string, CachedConnList> s_Cache;

const int kCachedConnetionStaleTimeout=300; // Don't try to reuse a connection if it has been unused for more than 5 minutes

PgConn::~PgConn() 
{
	#if ENABLE_ASSET_SERVER
	if (m_Conn)
	{
		int now=time(NULL);
		for (map<string, list<CachedConn> >::iterator i = s_Cache.begin(); i != s_Cache.end(); i++) {
			list<CachedConn>::iterator current = i->second.end();
			list<CachedConn>::iterator j = i->second.begin();
			while( j != i->second.end() ) {
				current = j++; // Store current iterator and fetch next one already in case we need to delete the current one

				if (current->conn == m_Conn) {
					current->free = true;
					current->lastUsed=now;
				}
				else if (current->free && now - current->lastUsed > kCachedConnetionStaleTimeout) {
					PQfinish(current->conn);
					delete current->prepared;
					delete current->preparedStatements;
					delete current->errorstring;
					i->second.erase(current);
				}
			}
		}
	}
	#endif // #if ENABLE_ASSET_SERVER
}

void PgConn::FlushFreeConnections()
{
	#if ENABLE_ASSET_SERVER
	for (map<string, list<CachedConn> >::iterator i = s_Cache.begin(); i != s_Cache.end(); i++) {
		list<CachedConn>::iterator current = i->second.end();
		list<CachedConn>::iterator j = i->second.begin();
		while( j != i->second.end() ) {
			current = j++; // Store current iterator and fetch next one already in case we need to delete the current one
			
			if ( current->free ) {
				PQfinish(current->conn);
				delete current->prepared;
				delete current->preparedStatements;
				delete current->errorstring;
				i->second.erase(current);
			}
		}
	}
	#endif // ENABLE_ASSET_SERVER
}

bool PgConn::DoConnect(const string& params, int timeout)
{
	#if ENABLE_ASSET_SERVER

	if ( timeout >= 0 )
		m_ConnectionString = "connect_timeout=" + IntToString(timeout) + " " + params;
	else
		m_ConnectionString = params;

	// \TODO Free already referenced m_Conn instead of just refusing to do anything
	if (m_Conn || m_Prepared || m_ErrorString || m_PreparedStatements) {
		if (! m_ErrorString)
			m_ErrorString = new string;
		return Error("Attempt at connecting already connected PgConn object.");
	}

	// Look up a cached connection which isn't in use
	map<string, CachedConnList>::iterator found = s_Cache.find(m_ConnectionString);
	if (found != s_Cache.end())
		for (CachedConnList::iterator i = found->second.begin(); i != found->second.end(); i++)
			if (i->free) {
			
				if(time(NULL) - i->lastUsed > kCachedConnetionStaleTimeout) 
					continue;
				
				m_Conn = i->conn;
				m_Prepared = i->prepared;
				m_PreparedStatements = i->preparedStatements;
				m_ErrorString = i->errorstring;
				

				// Is it stale? Try to freshen it
				if ((PQtransactionStatus(m_Conn) != PQTRANS_IDLE) ? ! Rollback() : false) {
					// ... then re-connect
					PQreset(m_Conn);
					m_Prepared->clear();
					m_PreparedStatements->clear();
					if (PQstatus(m_Conn) != CONNECTION_OK) {
						i->lastUsed=0; // force removal
						printf_console("Could not reuse stale db connection. (%s)\n",PQerrorMessage(m_Conn));
						continue;
					}
				}
				// Even though transaction status says OK, we have to execute a statement to be sure:
				
				PGresult* result = PQexec(m_Conn, "select 'Ping from PgConn'");
				ExecStatusType status = PGRES_FATAL_ERROR;
				if ( result != NULL ) {
					status = PQresultStatus(result);
					PQclear(result);		
				}
				if( PGRES_TUPLES_OK != status ) {
					i->lastUsed=0; // force removal
					printf_console("Could not reuse stale db connection. (%s)\n",PQerrorMessage(m_Conn));
					continue;
				}				
				// Ok now, mark as non-free and return
				i->free = false;
				return Success();
			}
	// ... or make a fresh one if none found
	m_Conn = PQconnectdb(m_ConnectionString.c_str());

	// ... and check that that succeeds
	if (m_Conn && PQstatus(m_Conn) == CONNECTION_OK) {
		m_ErrorString = new string;
		m_Prepared = new set<string>;
		m_PreparedStatements = new map<string,string>;

		s_Cache[m_ConnectionString].push_back(CachedConn(m_Conn, m_Prepared, m_PreparedStatements, m_ErrorString, false));

		return Success();
	}
	else {
		if (! m_ErrorString)
			m_ErrorString = new string;
		
		Error("Failed to connect to asset server: " + CleanErrorStr());
		
		if (m_Conn) {
			PQfinish(m_Conn);
			m_Conn = NULL;
		}
		return false;
	}
	#endif // #if ENABLE_ASSET_SERVER

	return false;
}


int PgConn::SocketFD()
{
	#if ENABLE_ASSET_SERVER
	return PQsocket(m_Conn); 
	#else
	return 0;
	#endif
}


// simple wrapper for select/poll used by WaitForRead and WaitForWrite
bool PgConn::PollSocket(bool write, int ms, int* elapsed) {
#if UNITY_WIN || USE_SELECT
	// On WIN32 we need to use select instead of poll
	struct timeval timeout;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET( SocketFD(), &fds );

	timeout.tv_sec=0;
	timeout.tv_usec=ms*1000;
	int ret = select(SocketFD()+1, write?NULL:&fds, write?&fds:NULL, NULL, &timeout);
#else
	pollfd fds[1];
	fds[0].fd = SocketFD();
	fds[0].events = write?POLLOUT:POLLIN;

	int ret = poll(fds, 1, ms);
#endif
#if UNITY_OSX
	pthread_testcancel(); // This is needed because poll and select  are not cancellation points on OS X (contrary to what posix says)
#endif
			
	if (ret > 0) {
		return true;
	}
	else {
		if (elapsed)
		*elapsed += ms;
		return false;
	}
}

bool PgConn::ConnectionOK()
{
	#if ENABLE_ASSET_SERVER
	if (PQstatus(m_Conn) == CONNECTION_BAD)
		return Error();
	else
		return true;
	#else
	return Error();
	#endif
}


bool PgConn::ClearQueue()
{
	#if ENABLE_ASSET_SERVER
	PQflush( m_Conn );
	PGresult* result = PQgetResult(m_Conn);
	if (result)
		PQclear(result);
	#endif
	return true; // Do any error checking here?
}

bool PgConn::BeginTransaction()
{
	#if ENABLE_ASSET_SERVER
	if (! PQsendQuery(m_Conn, ("begin transaction")))
		return Error();
	PQflush( m_Conn );
	PgResult result = GetResult(kActivityTimeout);
	if (result.Status() != PGRES_COMMAND_OK)
		return Error(result);

	ClearQueue();
	#endif
	return Success();
}

bool PgConn::BeginSerializableTransaction()
{
	#if ENABLE_ASSET_SERVER
	if (! PQsendQuery(m_Conn, ("begin transaction; set transaction isolation level serializable")))
		return Error();
	PQflush( m_Conn );
	PgResult result1 = GetResult(kActivityTimeout);
	if (result1.Status() != PGRES_COMMAND_OK)
		return Error(result1);

	PgResult result2 = GetResult(kActivityTimeout);
	if (result2.Status() != PGRES_COMMAND_OK)
		return Error(result2);


	ClearQueue();
	#endif
	return Success();
}

bool PgConn::Commit()
{
	#if ENABLE_ASSET_SERVER
	if (! PQsendQuery(m_Conn, ("commit")))
		return Error();
	PQflush( m_Conn );
	PgResult result = GetResult(kActivityTimeout);
	if (result.Status() != PGRES_COMMAND_OK)
		return Error(result);
	
	ClearQueue();
	#endif
	return Success();
}

bool PgConn::Rollback()
{
	#if ENABLE_ASSET_SERVER
	if (! PQsendQuery(m_Conn, ("rollback")))
		return Error();
	PQflush( m_Conn );
	PgResult result = GetResult(kActivityTimeout);
	if (result.Status() != PGRES_COMMAND_OK)
		return Error(result);
	
	ClearQueue();
	#endif
	return Success();
}

bool PgConn::Prepare(const string& name)
{
	#if ENABLE_ASSET_SERVER
	// Already prepared?
	set<string>::iterator found = m_Prepared->find(name);
	if (found != m_Prepared->end())
		// Yes, just skip doing it again
		return Success();
	else {
		// Not prepared already, so prepare now

		if (! PQsendQuery(m_Conn, ("select prepare_" + name + "_queries()").c_str()))
			return Error();
		PQflush( m_Conn );
		PgResult result = GetResult(kActivityTimeout);
		if (result.Status() != PGRES_TUPLES_OK)
			return Error(result);

		// And remember that we did so
		m_Prepared->insert(name);
		
		ClearQueue();
		return Success();
	}
	#else
	return Success();
	#endif
}

bool PgConn::Prepare(const string& name, const list<string>& types, const string& sql)
{
	#if ENABLE_ASSET_SERVER
	// Already prepared?
	map<string,string>::iterator found = m_PreparedStatements->find(name);
	if (found != m_PreparedStatements->end()) {
		// Yes
		
		// Different from old prepared statement?
		if (found->second != sql)
			// Yes: error
			return Error("Can not prepare a new query with the same name as a previously prepared query");
		else
			// No: fine just skip preparing it again
			return Success();
	}
	else {
		// Not prepared already, so prepare now
		(*m_PreparedStatements)[name] = sql;		

		string prepareQuery = "prepare ";
		prepareQuery.append(name);

		if (! types.empty()) {
			prepareQuery.append(" (");
			list<string>::const_iterator i = types.begin();
			prepareQuery.append(*i);
			for (i++; i != types.end(); i++) {
				prepareQuery.append(",");
				prepareQuery.append(*i);
			}
			prepareQuery.append(")");
		}
		prepareQuery.append(" as ");
		prepareQuery.append(sql);
		
		if (! PQsendQuery(m_Conn, prepareQuery.c_str()))
			return Error();
		PQflush( m_Conn );
		PgResult result = GetResult(kActivityTimeout);
		
		if (result.Status() != PGRES_COMMAND_OK)
			return Error(result);		

		ClearQueue();
		return Success();
	}
	#else
	return Success();
	#endif
}

bool PgConn::SendPrepared(const string& name, const vector<const char*>& values, const vector<int>& lengths, const vector<int>& isBinary)
{
	#if ENABLE_ASSET_SERVER
	if (values.size() != lengths.size() || values.size() != isBinary.size())
		return Error("PgConn::SendPrepared: All parameter arrays must have same size");
	

//	for (int i = 0; i != values.size(); i++)
//		printf_console ("Query parameter %d, length %d, binary %d: '%s'\n", i, lengths[i], (int)isBinary[i], values[i]);

	if (! PQsendQueryPrepared(m_Conn, name.c_str(), values.size(), &values.front(), &lengths.front(), &isBinary.front(), 1 /* want binary result */))
		return Error();
	PQflush( m_Conn );
	#endif // ENABLE_ASSET_SERVER
	return Success();
}


bool PgConn::SendPrepared(const string& name, const vector<const char*>& values) {
	vector<int> lengths(values.size(), -1);
	vector<int> isBinary(values.size(), 0);
	return SendPrepared(name, values, lengths, isBinary);
}

bool PgConn::SendPrepared(const string& name, PgParams& params) {
	return SendPrepared(name, params.Values(), params.Lengths(), params.Binary());
}


bool PgConn::SendPrepared(const string& name) {
	static const vector<const char*> vchar;
	static const vector<int> vint;
	return SendPrepared(name, vchar, vint, vint);
}

bool PgConn::IsReady()
{
	#if ENABLE_ASSET_SERVER
	if (! PQconsumeInput(m_Conn))
		return Error();
	return PQisBusy(m_Conn) == 0;
	#else
	return Error();
	#endif
}

PgResult PgConn::GetResult()
{
	#if ENABLE_ASSET_SERVER
	PGresult* result;
	if (! PQisBusy(m_Conn)) {
		result = PQgetResult(m_Conn);
		if (result)
			return PgResult(this, result);
	}
	else {
		// Usage error, should not be called unless IsReady()
		Error("PgConn::GetResult(): Result fetched before it had been read from server!");
	}
	#endif // ENABLE_ASSET_SERVER
	return PgResult(this);
}


PgResult PgConn::GetResult(int timeout)
{
	#if ENABLE_ASSET_SERVER
	// While the response hasn't been received
	int elapsed = 0;
	while (! IsReady()) {
		elapsed = 0;
		while (! WaitForRead(kActivityTick, &elapsed) ) {

			if (elapsed > timeout) {
				Error("Timed out awaiting result from server");
				return PgResult(this);
			}
		}
	}
	
	return PgResult(this, PQgetResult(m_Conn));
	#else
	return PgResult(this);
	#endif
}


//bool PgConn::LOExport(UInt32 oid, const string& filename) {
//	// Must be performed inside transaction, so fail otherwise
//	if (PQtransactionStatus(m_Conn) == PQTRANS_IDLE)
//		return Error("PgConn::LOExport must be called insided transaction block.");
//	
//	File fh;
//	if( ! fh.Open(filename, File::kWritePermission) ) // fh.Open with kWritePermission will automatically truncate the file.
//		return Error("Could not open file for writing");
//		
//	int			nbytes;
//	char		buf[kLOBufferSize];
//	int			lobj;
//
//	/*
//	 * open the large object.
//	 */
//	lobj = lo_open(m_Conn, oid, INV_READ);
//	if (lobj == -1)
//	{
//		/* we assume lo_open() already set a suitable error message */
//		fh.Close();
//		return Error();
//	}
//
//	/*
//	 * read in from the large object and write to the file
//	 */
//	while ((nbytes = lo_read(m_Conn, lobj, buf, kLOBufferSize)) > 0)
//	{
//		if(! fh.Write((void*) buf, nbytes) ) {
//			(void) lo_close(m_Conn, lobj);
//			fh.Close();
//			return Error("Could not write data to file");
//		}
//	}
//
//	/*
//	 * If lo_read() failed, we are now in an aborted transaction so there's no
//	 * need for lo_close(); furthermore, if we tried it we'd overwrite the
//	 * useful error result with a useless one. So skip lo_close() if we got a
//	 * failure result.
//	 */
//	if (nbytes < 0 ||
//		lo_close(m_Conn, lobj) != 0)
//	{
//		/* assume lo_read() or lo_close() left a suitable error message */
//		fh.Close();
//		return Error();
//	}
//
//	
//	fh.Close();
//	return Success();
//
//	
//	
///*	
//	//Note: The above is a modified version of the actual implementation of lo_export, except it uses a much larger buffer, which speeds
//	//      up downloads of large files over slow links.
//	if (lo_export(m_Conn, oid, filename.c_str()) == 1)
//		return Success();
//	else
//		return Error();
//*/
//}


// A bulk version of the above. Should speed up downloads of small files as it queries directly from the pg_largeobject table
bool PgConn::LOExport(map<UInt32,string>& oid2file, AssetServer::ProgressCallback* progress) 
{
	#if ENABLE_ASSET_SERVER

	PgArray loids;
	
	// Increase timeout when performing huge downloads
	int timeout = kActivityTimeout+(kActivityTimeout * oid2file.size()) / 100;
	
	// Truncate existing files and generate parameter list
	for( map<UInt32,string>::const_iterator i = oid2file.begin(); i != oid2file.end(); i++ ) {
		loids.Add(i->first);
		if( ! m_LastOpenFile.Open(i->second, File::kWritePermission) ) // opening with kWritePermission will automatically truncate the file
			return Error("Could not truncate file before writing");
		m_LastOpenFile.Close();
	}
	
	SInt64 estBytes = 0;
	SInt64 totalBytes = 0;
	//int timeStart = time(NULL);

	if ( progress ) 
		progress->UpdateProgress(0, 0, "Initiating download...");

	// Get total size of download. (Run in a second database connection so we can start the download straight away.)
	PgConn conn2;
	if (! conn2.DoConnect(m_ConnectionString) )
		estBytes = -1;
		
	{
	
		if ( m_LOPageSize < 0 ) // We only fetch the page size the first time around, as is is bound to stay the same for a database
		{
			
			if(! PQsendQueryParams( m_Conn,
								   "select setting::int/4 as sz  from pg_settings where name='block_size'", 0,NULL,NULL,NULL,NULL,1 /* want binary result */) ) 
				return Error();
			
			PQflush( m_Conn );
			PgResult result = GetResult(timeout);
			if ( !result.Ok() )
				return Error(result);
			
			PgColumn szCol = result.GetColumn(0);
			m_LOPageSize=szCol.GetInt(0);
			ClearQueue();
		}
		
		PgParams params;
		params.Add(loids);
		
		if(estBytes == 0 &&  PQsendQueryParams(conn2.m_Conn, 
				"select count(*) :: int as count from pg_largeobject  where loid = ANY($1::oid[]) ",
				params.GetValueCount(),
				(const Oid*) NULL,
				&params.Values()[0],
				&params.Lengths()[0],
				&params.Binary()[0],
				1 /* want binary result */))	
		{
			PQflush(conn2.m_Conn);
		}
		else
		{
			conn2.Error();
			printf_console("Could not calculate download size: %s\n", conn2.ErrorStr().c_str());
			estBytes = -1;
		}
			
		
		// Use a cursor in order not to fill up the entire memory with multiple file contents
		
		if(! PQsendQueryParams( m_Conn,
				"declare _lo_export_bulk_cursor no scroll cursor for  "
				"select loid, pageno, data  "
				"from pg_largeobject "
				"where loid = ANY($1::oid[]) ",
				params.GetValueCount(),
				(const Oid*) NULL,
				&params.Values()[0], 
				&params.Lengths()[0],
				&params.Binary()[0],
				1 /* want binary result */))
			return Error();
		PQflush( m_Conn );	
		PgResult result2 = GetResult(timeout);
		if ( !result2.Ok() )
			return Error(result2);
	}

	string fetchQuery =  Format("fetch forward %d from _lo_export_bulk_cursor", kLOBulkFetchSize);
	
	UInt32 currentLoid=0;
	for(;;) {
		ClearQueue();
		if(! PQsendQueryParams(m_Conn,fetchQuery.c_str(),0,NULL,NULL,NULL,NULL,1 /* want binary result */) ) 
			return Error();
		PQflush( m_Conn );	
		
		if ( estBytes == 0 && conn2.IsReady() ) {
			PgResult result = conn2.GetResult();
			if ( !result.Ok() ) {
				estBytes = -1;
				conn2.Error(result);
				printf_console("Could not calculate download size: %s\n", conn2.ErrorStr().c_str());
			}
			else {
				PgColumn countCol = result.GetColumn(0);
				estBytes = countCol.GetInt(0)*m_LOPageSize - (oid2file.size() * m_LOPageSize / 2);
				//printf_console("Block count: %d, Page size %d, No files: %d, Estimate: %d\n",countCol.GetInt(0),m_LOPageSize,oid2file.size(),estBytes);
				if (estBytes == 0 ) estBytes = -1;
			}
			conn2.ClearQueue();
		}
		
		PgResult result = GetResult(timeout);
		if ( !result.Ok() )
			return Error(result);
		
		// The last fetch will return zero rows, indicating that there are no more results
		if (result.Rows() == 0 )
			break;

		PgColumn loidCol = result.GetColumn(0);
		PgColumn pageCol = result.GetColumn(1);
		PgColumn dataCol = result.GetColumn(2);
		
		if (progress && progress->ShouldAbort())
		{
			if(currentLoid != 0) 
				m_LastOpenFile.Close();
			ClearQueue();
			return Error("Aborted by user");
		}

		// Loop through the results and write them to the files
		for (int i = 0; i < result.Rows(); i++) {
			UInt32 loid=loidCol.GetInt(i);
			int page=pageCol.GetInt(i);
			int offset=page*m_LOPageSize;
			//printf_console("LOID: %d -> %s, offset: %d\n", loid, oid2file[loid].c_str(), offset);

			if(currentLoid != loid) { // Switch to the correct file
				if(currentLoid != 0) 
					m_LastOpenFile.Close();
				if( !m_LastOpenFile.Open(oid2file[loid], File::kReadWritePermission) )
					return Error("Could not open file for writing");
				currentLoid=loid;
			}
			
			if(! dataCol.WriteString(i, m_LastOpenFile, offset) )
				return Error("Could not write data received to file");
			totalBytes += dataCol.GetLength(i);
		}
		//int elapsed = time(NULL) - timeStart;
		//printf_console("Block: %02.0f%% %.2f/%.2f KBytes, elapsed %02d:%02d, %.2f Kb/s\n", (float)totalBytes / (float)estBytes * 100,  (float)totalBytes / 1024.0, (float)estBytes / 1024.0, elapsed / 60, elapsed % 60, (float)totalBytes / (float)elapsed / 1024.0);
		if(NULL != progress ) 
			progress->UpdateProgress(totalBytes, estBytes, oid2file[currentLoid]);
	}
	
	
	ClearQueue();
	if(currentLoid != 0) 
		m_LastOpenFile.Close();
	if(NULL != progress ) 
		progress->UpdateProgress(totalBytes, totalBytes, oid2file[currentLoid]);
	return Success();

	#else
	return Error();
	#endif // ENABLE_ASSET_SERVER
}

// Uses lo_creat, lo_open and lo_write to import the file, as lo_import uses a buffer size that's too small for high latency connections.
// Note: bulk import like the one implemented above for export is not possible, as the users do not have permission to insert data into pg_largeobject
// This means that uploading multiple small files to the server will always be slower than downloading them again. This should not be a big issue,
// as large uploads like that only happen when populating the database initially. -- large downloads happen every time someone checks out a new work-area,
// which is also more likely to be over a slow connection.
UInt32 PgConn::LOImport(const string& filename) 
{
	#if ENABLE_ASSET_SERVER

	int			nbytes,
				tmp;
	char		buf[kLOBufferSize];
	Oid			lobjOid;
	int			lobj;

	/*
	 * open the file to be read in
	 */
	if(! m_LastOpenFile.Open(filename, File::kReadPermission) ) 
	{							/* error */
		Error(Format("could not open file \"%s\" for reading", filename.c_str()));
		return InvalidOid;
	}

	/*
	 * create an inversion object
	 */
	lobjOid = lo_creat(m_Conn, INV_READ | INV_WRITE);
	if (lobjOid == InvalidOid)
	{
		/* we assume lo_creat() already set a suitable error message */
		m_LastOpenFile.Close();
		Error();
		return InvalidOid;
	}

	lobj = lo_open(m_Conn, lobjOid, INV_WRITE);
	if (lobj == -1)
	{
		/* we assume lo_open() already set a suitable error message */
		m_LastOpenFile.Close();
		Error();
		return InvalidOid;
	}

	/*
	 * read in from the file and write to the large object
	 */
	while ((nbytes = m_LastOpenFile.Read((void*)buf, kLOBufferSize)) > 0)
	{
		tmp = lo_write(m_Conn, lobj, buf, nbytes);
		if (tmp != nbytes)
		{
			/*
			 * If lo_write() failed, we are now in an aborted transaction so
			 * there's no need for lo_close(); furthermore, if we tried it
			 * we'd overwrite the useful error result with a useless one. So
			 * just nail the doors shut and get out of town.
			 */
			m_LastOpenFile.Close();
			Error();
			return InvalidOid;
		}
	}

	if (nbytes < 0)
	{
		Error(Format("could not read from file \"%s\"", filename.c_str()));
		lobjOid = InvalidOid;
	}

	m_LastOpenFile.Close();
	
	if (lo_close(m_Conn, lobj) != 0)
	{
		/* we assume lo_close() already set a suitable error message */
		return InvalidOid;
	}

	return lobjOid;

	#else
	return 0;
	#endif // ENABLE_ASSET_SERVER
}


string PgConn::CleanErrorStr()
{
	#if ENABLE_ASSET_SERVER
	string error = PQerrorMessage(m_Conn); 

	if (error.find("FATAL: ") == 0)
		error.erase(0, 7);
	
	return error;

	#else
	return "Asset Server is disabled";
	#endif
}


/// Copy-in interface
bool PgConn::WriteCopyInData(const char* data, int length)
{
	#if ENABLE_ASSET_SERVER
	int elapsed = 0;
	while(elapsed < kActivityTimeout)
		switch (PQputCopyData(m_Conn, data, length)) {
			case 1: return Success();
			case -1: return Error();
			case 0: WaitForWrite(kActivityTick, &elapsed);
		}
	return Error("Timeout while submitting copy-in data to server");
	#else
	return Error();
	#endif
}

bool PgConn::WriteFieldHeader(int length) {
	int network = htonl(length);
	if (WriteCopyInData((char*)&network,  4))
		return Success();
	else
		return Error();
}

bool PgConn::BeginCopyIn(const string& table, int cols)
{
	#if ENABLE_ASSET_SERVER

	if (! PQsendQuery(m_Conn, ("copy " + table + " from stdin with binary").c_str()))
		return Error();
	PQflush( m_Conn );
	PgResult result = GetResult(kActivityTimeout);
	if (result.Status() != PGRES_COPY_IN)
		return false;
	
	m_CopyCols = (short)cols;
	if (WriteCopyInData("PGCOPY\n\377\r\n\0\0\0\0\0\0\0\0\0", 19))
		return Success();
	else
		return Error();

	#else
	return Error();
	#endif
}

bool PgConn::SendRow() {
	short network = htons(m_CopyCols);
	if (WriteCopyInData((char*)&network, sizeof(short)))
		return Success();
	else
		return Error();
}
bool PgConn::SendField(int value) {
	int network = htonl(value);
	if (WriteFieldHeader(sizeof(int)) && WriteCopyInData((char*)&network,  sizeof(int)))
		return Success();
	else
		return Error();
}

bool PgConn::SendField(const string& value) {
	if (WriteFieldHeader(value.length()) && WriteCopyInData(value.c_str(), value.length()))
		return Success();
	else
		return Error();
}
bool PgConn::SendNull() {
	if (WriteFieldHeader(-1))
		return Success();
	else
		return Error();
}

bool PgConn::EndCopyIn()
{
	#if ENABLE_ASSET_SERVER

	// Write footer
	short network = htons(0xffff);
	if (! WriteCopyInData((char*)&network,  sizeof(short)))
		return Error();
	
	// Finish copy-in
	int elapsed = 0;
	bool done = false;
	while(! done)
		switch (PQputCopyEnd(m_Conn, NULL)) {
			case 1: done = true; break;
			case -1: return Error();
			case 0: 
				if (elapsed > kActivityTimeout) 
					return Error("Timeout while submitting copy-in data to server");
				WaitForWrite(kActivityTick, &elapsed); 
		}

	// Check that everything went ok
	PgResult result = GetResult(kActivityTimeout);
	PgResult result2 = GetResult(kActivityTimeout);
	if (result.Status() == PGRES_COMMAND_OK)
		return Success();
	else
		return Error();

	#else
	return Error();
	#endif
}
