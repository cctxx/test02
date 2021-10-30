#ifndef PGCONN_H
#define PGCONN_H


#include "PgResult.h"

#if ENABLE_ASSET_SERVER
#include "External/postgresql/include/libpq-fe.h"
#include "External/postgresql/include/libpq/libpq-fs.h"
#endif

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include "Editor/Src/AssetServer/ASProgress.h" 

using std::set;
using std::list;
using std::string;
using std::vector;
using std::map;

class PgArrayBase {
 public:
	void Add(const string& value) { m_Values.push_back(value); }
	void Add(int value);
	void Add(PgArrayBase& ary) { m_Values.push_back(ary.AsString()); }
 	void Append(const string& value) { m_Values.back().append(value); }
 	void Clear() { m_Values.clear(); m_Representation.clear(); }
	virtual const string& AsString() = 0;

 protected:
 	list<string> m_Values;
	string m_Representation;

};

class PgArray : public PgArrayBase {
 public:
 	virtual const string& AsString();
};
class PgBinaryArray : public PgArrayBase {
 public:
 	virtual const string& AsString();
};


class PgParams {
 public:
 	void Add(const string& value);
 	void Add(int value);
 	void Add(PgArrayBase& ary);
	void AddBinary(const string& value);
 	void Append(const string& value);
 	void Clear();
	const vector<const char*>& Values();
	const vector<int>& Lengths();
	const vector<int>& Binary();
	size_t GetValueCount() const { return m_Values.size(); }

 private:
 	list<string> m_Values;
	vector<const char*> m_ValuePtrs;
	vector<int> m_ValueLengths;
	vector<int> m_Binary;

};
 

class PgConn {
 public:
 	/// Create PgConn object (does not connect yet)
	PgConn() : m_Conn(NULL), m_Prepared(NULL), m_ErrorString(NULL), m_PreparedStatements(NULL), m_LOPageSize(-1) { }
	~PgConn();

	
/*	/// Start a connect-procedure
	bool StartConnect(const string& params);

	/// Attempt to reset connection, use ConnectSucceeded() to check if it actually worked 
	void StartReset();

	/// Use to check if a connection has been made (only when connecting both
	/// ConnectSucceeded() and ConnectionFailure() may be false)
	bool ConnectSucceeded();
	/// Use to check if connecting has failed or connection has been lost
*/

	bool WaitForRead(int ms, int* elapsed = NULL) { return PollSocket(false, ms, elapsed) ;}
	bool WaitForWrite(int ms, int* elapsed = NULL)  { return PollSocket(true, ms, elapsed) ;}

	bool DoConnect(const string& params, int timeout=-1);
	static void FlushFreeConnections();
	bool ConnectionOK();

	/// Return if there was an error with the last function (important when using
	/// IsReady() and GetResult() that don't have other ways to inform of errors)
	bool HasError() const { return ! m_ErrorString->empty(); }
	/// Return error of last function (	or empty string if none)
	const string& ErrorStr() const { return *m_ErrorString; }

	bool ClearQueue();

	bool BeginTransaction();
	bool BeginSerializableTransaction();
	bool Commit();
	bool Rollback();

	/// Copy-in interface
	bool BeginCopyIn(const string& table, int cols);
	bool SendRow();
	bool SendField(int value);
	bool SendField(const string& value);
	bool SendNull();
	bool EndCopyIn();


	/// Prepare server side functions (there must exist a stored procedure to do this on the server)
	bool Prepare(const string& name);

	/// Prepare custom SQL code (So that we don't need to update the schema for every minor tweak)
	bool Prepare(const string& name, const list<string>& types, const string& sql) ;

	/// Execute prepared statement (prepared with Prepare()), simplified version allowing no parameters
 	bool SendPrepared(const string& name);
	/// Execute prepared statement (prepared with Prepare()), simplified version with automatic parameter handling
 	bool SendPrepared(const string& name, PgParams& params);

	/// Call before doing GetResult() to avoid blocking
	bool IsReady();
	/// Get result set from one command
 	PgResult GetResult();
	/// Get result set from one command, with timeout (in millisecond)
	PgResult GetResult(int timeout);

	/// Attempt to cancel operation. Clean up open file for now. Connections are closed on destroying object.
 	bool Cancel()
	{
		m_LastOpenFile.Close();
		return true;
	}

	/// Large Object functions
	bool LOExport(UInt32 oid, const string& filename);
	bool LOExport(map<UInt32,string>& oid2file, AssetServer::ProgressCallback* callback = NULL);
	UInt32 LOImport(const string& filename);

 private:
	int SocketFD();
	bool PollSocket(bool write, int ms, int* elapsed=NULL);

	bool WriteCopyInData(const char* data, int length);
	bool WriteFieldHeader(int length);

	bool Success() { *m_ErrorString = ""; return true; }
	bool Error() {
		#if ENABLE_ASSET_SERVER
		string tmp = PQerrorMessage(m_Conn);
		if(tmp != "")
			*m_ErrorString = tmp;
		#endif
		return false;
	}
	bool Error(const string& msg) { if(msg != "") *m_ErrorString = msg; return false; }
	bool Error(const PgResult& res) { string tmp = res.ErrorStr(); if (tmp != "") *m_ErrorString = tmp; return false; }

	string CleanErrorStr();

	/// Execute prepared statement (prepared with Prepare())
 	bool SendPrepared(const string& name, const vector<const char*>& values, const vector<int>& lengths, const vector<int>& isBinary);
	/// Execute prepared statement (prepared with Prepare()), simplified version allowing only string parameters
 	bool SendPrepared(const string& name, const vector<const char*>& values);

 	PGconn* m_Conn;

	set<string>* m_Prepared;
	map<string,string>* m_PreparedStatements;
	string* m_ErrorString;
	string m_ConnectionString;

	short m_CopyCols;
	
	// No copy-construction allowed
 	PgConn(PgConn& orig);

	// to be able to cleanup in case we need to terminate
	File m_LastOpenFile;
	
	// this reflects a server constant indicating the block size in the pg_largeobjects system table
	int m_LOPageSize;
};


#endif
